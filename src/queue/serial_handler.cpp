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
#include "serial_handler.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
int SerialHandler::Cancel(ITask* task)
{
    FFRT_COND_TRUE_DO_ERR((task == nullptr), "submit task is nullptr", return -1);
    FFRT_COND_TRUE_DO_ERR((looper_ == nullptr || looper_->m_queue == nullptr), "queue is nullptr", return -1);

    int ret = looper_->m_queue->RemoveTask(task);
    if (ret == 0) {
        DestroyTask(task);
    }
    FFRT_LOGD("Cancel Task [0x%x] succ", task);
    return ret;
}

void SerialHandler::DispatchTask(ITask* task)
{
    FFRT_LOGD("DispatchTask Task [0x%x] begin", task);
    FFRT_COND_TRUE_DO_ERR((task == nullptr), "failed to dispatch, task is nullptr", return);
    auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
    FFRT_LOGD("task->func_storage [0x%x], f->exec [0x%x]", task->func_storage, f->exec);
    f->exec(f);
    f->destroy(f);
    DestroyTask(task);
    FFRT_LOGD("DispatchTask Task [0x%x] succ", task);
}

int SerialHandler::SubmitDelayed(ITask* task, uint64_t delayUs)
{
    FFRT_COND_TRUE_DO_ERR((task == nullptr), "submit task is nullptr", return -1);
    FFRT_COND_TRUE_DO_ERR((looper_ == nullptr || looper_->m_queue == nullptr), "queue is nullptr", return -1);

    auto nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
    uint64_t upTime = static_cast<uint64_t>(nowUs.time_since_epoch().count());
    if (delayUs > 0) {
        upTime = upTime + delayUs;
    }

    FFRT_LOGD("SubmitDelayed Task [0x%x] succ", task);
    return looper_->m_queue->PushTask(task, upTime);
}

void SerialHandler::DestroyTask(ITask* task)
{
    task->Notify();
    task->DecDeleteRef();
}
} // namespace ffrt