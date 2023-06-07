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

#include "sched/interval.h"

#include "core/dependence_manager.h"
#include "eu/execute_unit.h"
#include "dfx/trace/ffrt_trace.h"

namespace ffrt {
void Deadline::Update(uint64_t deadlineUs)
{
    if (deadlineUs != ToUs()) {
        deadlineNs = deadlineUs < 1 ? 1 : deadlineUs * 1000;
    }

    absDeadlineNs = deadlineNs + AbsNowNs();

    FFRT_LOGI("Deadline %lu Update %lu Abs %lu", deadlineUs, deadlineNs, absDeadlineNs);
}

uint64_t Deadline::AbsNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

PerfCtrl::PerfCtrl(const QoS& qos) : qos(qos)
{
    if (qos == qos_inherit) {
        FFRT_LOGW("Invalid Thread Group");
        return;
    }

    tg = ExecuteUnit::Instance().BindTG(DevType::CPU, this->qos);
}

PerfCtrl::~PerfCtrl()
{
    if (tg) {
        tg = nullptr;
        ExecuteUnit::Instance().UnbindTG(DevType::CPU, qos);
    }
}

void PerfCtrl::Update(bool force)
{
    if (!force && predUtil == curUtil) {
        FFRT_LOGW("Predict Util Same as Current Util %lu", predUtil);
        return;
    }

    curUtil = predUtil;

    if (tg) {
        tg->UpdateUitl(curUtil);
    }
}

void PerfCtrl::Update(uint64_t deadlineNs, uint64_t load, bool force)
{
    if (deadlineNs == 0) {
        deadlineNs = 1;
    }
    predUtil = (load << SCHED_CAPACITY_SHIFT) / deadlineNs;
    if (predUtil > SCHED_MAX_CAPACITY) {
        FFRT_LOGW("Predict Util %lu Exceeds Max Capacity", predUtil);
        predUtil = SCHED_MAX_CAPACITY;
    }

    FFRT_LOGI("Update Load %lu, Deadline %lu, Util %lu\n", load, deadlineNs, predUtil);

    Update(force);
}

void IntervalLoadPredictor::UpdateTotalLoad(uint64_t load)
{
    totalLoad.UpdateLoad(load);
}

void IntervalLoadPredictor::UpdateCPLoad(uint64_t load)
{
    if (cpLoadIndex + 1 > cpLoad.size()) {
        cpLoad.resize(cpLoadIndex + 1);
    }

    cpLoad[cpLoadIndex++].UpdateLoad(load);
}

uint64_t IntervalLoadPredictor::GetTotalLoad()
{
    return totalLoad.GetPredictLoad();
}

uint64_t IntervalLoadPredictor::GetCPLoad()
{
    uint64_t load = cpLoad[cpLoadIndex].GetPredictLoad();
    if (load == 0) {
        return 0UL;
    }

    uint64_t predictLoad = totalLoad.GetPredictLoad();
    return (predictLoad < load) ? 0 : (predictLoad - load);
}

DefaultInterval::DefaultInterval(uint64_t deadlineUs, const QoS& qos) : Interval(deadlineUs, qos), lt(*this), ctrl(qos)
{
    ctrl.SetWindowSize(Ddl().ToNs());
}

DefaultInterval::~DefaultInterval()
{
    std::unique_lock lock(mutex);
    ctrl.Update(1, 0, true);
}

int DefaultInterval::Begin()
{
    FFRT_TRACE_SCOPE(TRACE_LEVEL1, IntervalBegin);
    std::unique_lock lock(mutex);

    if (Enabled()) {
        FFRT_LOGE("interval already begin\n");
        return -1;
    }

    if (ctrl.isBusy()) {
        FFRT_LOGE("qos interval is busy, please retry later\n");
        return -1;
    }

    enabled = true;

    lt.Begin();

    ctrl.Update(Ddl().ToNs(), lp.GetTotalLoad(), true);
    lp.ResetCPIndex();

    return 0;
}

void DefaultInterval::Update(uint64_t deadlineUs)
{
    FFRT_TRACE_SCOPE(TRACE_LEVEL1, IntervalUpdate);
    std::unique_lock lock(mutex);

    if (!Enabled()) {
        return;
    }

    Ddl().Update(deadlineUs);
    ctrl.SetWindowSize(Ddl().ToNs());
}

void DefaultInterval::End()
{
    FFRT_TRACE_SCOPE(TRACE_LEVEL1, IntervalEnd);
    std::unique_lock lock(mutex);

    if (!Enabled()) {
        return;
    }

    enabled = false;

    lp.UpdateTotalLoad(lt.GetLoad());

    lt.End();
}

void DefaultInterval::CheckPoint()
{
    FFRT_TRACE_SCOPE(TRACE_LEVEL1, IntervalCheckPoint);
    std::unique_lock lock(mutex);

    if (!Enabled()) {
        return;
    }

    ctrl.Update(Ddl().LeftNs(), lp.GetCPLoad());
    lp.UpdateCPLoad(lt.GetLoad());
}

void DefaultInterval::Join()
{
    FFRT_TRACE_SCOPE(TRACE_LEVEL1, IntervalJoin);
    std::unique_lock lock(mutex);
    if (!ctrl.Join()) {
        FFRT_LOGE("Failed to Join Thread %d", ThreadGroup::GetTID());
    }
}

void DefaultInterval::Leave()
{
    FFRT_TRACE_SCOPE(TRACE_LEVEL1, IntervalLeave);
    std::unique_lock lock(mutex);
    if (!ctrl.Leave()) {
        FFRT_LOGE("Failed to Leave Thread %d", ThreadGroup::GetTID());
    }
}

void DefaultInterval::UpdateTaskSwitch(TaskSwitchState state)
{
    FFRT_TRACE_SCOPE(TRACE_LEVEL1, IntervalUpdateTaskSwitch);
    std::unique_lock lock(mutex);

    switch (state) {
        case TaskSwitchState::BEGIN:
            ctrl.Update(true);
            break;
        case TaskSwitchState::UPDATE:
            ctrl.Update();
            break;
        case TaskSwitchState::END:
            ctrl.clear();
            ctrl.Update(true);
            break;
        default:
            break;
    }

    lt.Record(state);
}
} // namespace ffrt
