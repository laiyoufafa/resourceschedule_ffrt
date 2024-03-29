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

#include <climits>
#include <cstring>
#include <sys/stat.h>
#include "eu/cpu_monitor.h"
#include "eu/cpu_manager_interface.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "eu/qos_interface.h"
#include "eu/cpuworker_manager.h"

namespace ffrt {

bool CPUWorkerManager::IncWorker(const QoS& qos)
{
    std::unique_lock lock(groupCtl[qos()].tgMutex);
    if (tearDown) {
        return false;
    }

    auto worker = std::unique_ptr<WorkerThread>(new (std::nothrow) CPUWorker(qos, {
        std::bind(&CPUWorkerManager::PickUpTask, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::NotifyTaskPicked, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerIdleAction, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerRetired, this, std::placeholders::_1),
    }));
    if (worker == nullptr) {
        FFRT_LOGE("Inc CPUWorker: create worker\n");
        return false;
    }
    worker->WorkerSetup(worker.get(), qos);
    WorkerJoinTg(qos, worker->Id());
    groupCtl[qos()].threads[worker.get()] = std::move(worker);
    return true;
}

void CPUWorkerManager::WakeupWorkers(const QoS& qos)
{
    if (tearDown) {
        return;
    }

    auto& ctl = sleepCtl[qos()];
    ctl.cv.notify_one();
}

int CPUWorkerManager::GetTaskCount(const QoS& qos)
{
    auto& sched = FFRTScheduler::Instance()->GetScheduler(qos);
    return sched.RQSize();
}

TaskCtx* CPUWorkerManager::PickUpTask(WorkerThread* thread)
{
    if (tearDown) {
        return nullptr;
    }

    auto& sched = FFRTScheduler::Instance()->GetScheduler(thread->GetQos());
    auto lock = GetSleepCtl(static_cast<int>(thread->GetQos()));
    std::lock_guard lg(*lock);
    return sched.PickNextTask();
}

void CPUWorkerManager::NotifyTaskPicked(const WorkerThread* thread)
{
    monitor.Notify(thread->GetQos(), TaskNotifyType::TASK_PICKED);
}

void CPUWorkerManager::WorkerRetired(WorkerThread* thread)
{
    pid_t pid = thread->Id();
    int qos = static_cast<int>(thread->GetQos());
    thread->SetExited(true);
    thread->Detach();

    {
        std::unique_lock lock(groupCtl[qos].tgMutex);
        auto worker = std::move(groupCtl[qos].threads[thread]);
        size_t ret = groupCtl[qos].threads.erase(thread);
        if (ret != 1) {
            FFRT_LOGE("erase qos[%d] thread failed, %d elements removed", qos, ret);
        }
        WorkerLeaveTg(qos, pid);
        worker = nullptr;
    }
}

WorkerAction CPUWorkerManager::WorkerIdleAction(const WorkerThread* thread)
{
    if (tearDown) {
        return WorkerAction::RETIRE;
    }

    auto& ctl = sleepCtl[thread->GetQos()];
    std::unique_lock lk(ctl.mutex);
    monitor.IntoSleep(thread->GetQos());
    FFRT_LOGI("worker sleep");
#if defined(IDLE_WORKER_DESTRUCT)
    if (ctl.cv.wait_for(lk, std::chrono::seconds(5),
        [this, thread] {return tearDown || GetTaskCount(thread->GetQos());})) {
        monitor.WakeupCount(thread->GetQos());
        FFRT_LOGI("worker awake");
        return WorkerAction::RETRY;
    } else {
        monitor.TimeoutCount(thread->GetQos());
        FFRT_LOGI("worker exit");
        return WorkerAction::RETIRE;
    }
#else /* !IDLE_WORKER_DESTRUCT */
    ctl.cv.wait(lk, [this, thread] {return tearDown || GetTaskCount(thread->GetQos());});
    monitor.WakeupCount(thread->GetQos());
    FFRT_LOGI("worker awake");
    return WorkerAction::RETRY;
#endif /* IDLE_WORKER_DESTRUCT */
}

void CPUWorkerManager::NotifyTaskAdded(enum qos qos)
{
    QoS taskQos(qos);
    monitor.Notify(taskQos, TaskNotifyType::TASK_ADDED);
}

CPUWorkerManager::CPUWorkerManager() : monitor({
    std::bind(&CPUWorkerManager::IncWorker, this, std::placeholders::_1),
    std::bind(&CPUWorkerManager::WakeupWorkers, this, std::placeholders::_1),
    std::bind(&CPUWorkerManager::GetTaskCount, this, std::placeholders::_1)})
{
    groupCtl[qos_deadline_request].tg = std::unique_ptr<ThreadGroup>(new ThreadGroup());
}

void CPUWorkerManager::WorkerJoinTg(const QoS& qos, pid_t pid)
{
    if (qos == qos_user_interactive) {
        (void)JoinWG(pid);
        return;
    }
    auto& tgwrap = groupCtl[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Join(pid);
}

void CPUWorkerManager::WorkerLeaveTg(const QoS& qos, pid_t pid)
{
    if (qos == qos_user_interactive) {
        return;
    }
    auto& tgwrap = groupCtl[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Leave(pid);
}
} // namespace ffrt
