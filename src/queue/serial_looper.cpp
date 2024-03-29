/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "serial_looper.h"
#include <sstream>
#include "cpp/task.h"
#include "dfx/log/ffrt_log_api.h"
#include "ihandler.h"
#include "sync/sync.h"
#include "util/slab.h"

namespace {
constexpr uint32_t STRING_SIZE_MAX = 128;
}

namespace ffrt {
SerialLooper::SerialLooper(const char* name, enum qos qos, uint64_t timeout, ffrt_function_header_t* timeoutCb)
    : timeout_(timeout), timeoutCb_(timeoutCb)
{
    if (name != nullptr && (std::string(name).size() <= STRING_SIZE_MAX)) {
        name_ += name;
    }

    if (timeout > 0 && timeoutCb_ != nullptr) {
        GetSerialTaskByFuncStorageOffset(timeoutCb)->IncDeleteRef();
    }

    queue_ = std::make_shared<SerialQueue>(name_);
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "failed to construct serial queue");
    // using nested submission is to submit looper task on worker.
    // when ffrt::wait() is used in the current thread, the looper task is not in the waiting list.
    submit([this, qos] { handle = submit_h([this] { Run(); }, {}, {}, task_attr().name(name_.c_str()).qos(qos)); },
        {}, { &handle });
    ffrt::wait({&handle});
    FFRT_COND_DO_ERR((handle == nullptr), return, "failed to construct serial looper");
    FFRT_LOGI("create serial looper [%s] succ", name_.c_str());
}

SerialLooper::~SerialLooper()
{
    Quit();
}

void SerialLooper::Quit()
{
    FFRT_LOGI("quit serial looper [%s] enter", name_.c_str());
    isExit_.store(true);
    queue_->Quit();
    // wait for the task being executed to complete.
    wait({handle});

    if (timeout_ > 0) {
        // wait for all delayedWorker to complete.
        while (delayedCbCnt_.load() > 0) {
            this_task::sleep_for(std::chrono::microseconds(timeout_));
        }

        if (timeoutCb_ != nullptr) {
            GetSerialTaskByFuncStorageOffset(timeoutCb_)->DecDeleteRef();
        }
    }
    FFRT_LOGI("quit serial looper [%s] leave", name_.c_str());
}

void SerialLooper::Run()
{
    FFRT_LOGI("run serial looper [%s] enter", name_.c_str());
    while (!isExit_.load()) {
        ITask* task = queue_->Next();
        if (task) {
            FFRT_LOGD("get next serial task [0x%x]", task);
            SetTimeoutMonitor(task);
            FFRT_COND_DO_ERR((task->handler_ == nullptr), break, "failed to run task, handler is nullptr");
            task->handler_->DispatchTask(task);
        }
    }
    FFRT_LOGI("run serial looper [%s] leave", name_.c_str());
}

void SerialLooper::SetTimeoutMonitor(ITask* task)
{
    if (timeout_ <= 0) {
        return;
    }

    task->IncDeleteRef();
    WaitUntilEntry* we = new (SimpleAllocator<WaitUntilEntry>::allocMem()) WaitUntilEntry();
    // set dealyedworker callback
    we->cb = ([this, task](WaitEntry* we) {
        if (!task->isFinished_) {
            RunTimeOutCallback(task);
        }
        delayedCbCnt_.fetch_sub(1);
        task->DecDeleteRef();
        SimpleAllocator<WaitUntilEntry>::freeMem(static_cast<WaitUntilEntry*>(we));
    });

    // set dealyedworker wakeup time
    std::chrono::microseconds timeout(timeout_);
    auto now = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
    we->tp = std::chrono::time_point_cast<std::chrono::steady_clock::duration>(now + timeout);

    if (!DelayedWakeup(we->tp, we, we->cb)) {
        task->DecDeleteRef();
        SimpleAllocator<WaitUntilEntry>::freeMem(we);
        FFRT_LOGW("timeout [%llu us] is too short to set watchdog of task [0x%x]", task);
        return;
    }

    delayedCbCnt_.fetch_add(1);
    FFRT_LOGD("set watchdog of task [0x%x] succ", task);
}

void SerialLooper::RunTimeOutCallback(ITask* task)
{
    std::stringstream ss;
    ss << "serial queue [" << name_ << "], serial task [" << std::hex << task << "], execution time exceeds "
       << std::dec << timeout_ << " us";
    std::string msg = ss.str();
    std::string eventName = "SERIAL_TASK_TIMEOUT";

#ifdef FFRT_SEND_EVENT
    time_t cur_time = time(nullptr);
    std::string sendMsg = std::string((ctime(&cur_time) == nullptr) ? "" : ctime(&cur_time)) + "\n" + msg + "\n";

    HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::OTHERS, eventName,
        OHOS::HiviewDFX::HiSysEvent::EventType::FAULT, "PID", getpid(), "TGID", getgid(), "UID", getuid(),
        "MODULE_NAME", "ffrt", "PROCESS_NAME", "ffrt", "MSG", sendMsg);
#endif

    FFRT_LOGE("[%s], %s", eventName.c_str(), msg.c_str());
    if (timeoutCb_ != nullptr) {
        timeoutCb_->exec(timeoutCb_);
    }
}
} // namespace ffrt
