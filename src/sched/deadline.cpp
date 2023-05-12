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

#include "deadline.h"
#include "c/deadline.h"
#include "internal_inc/osal.h"
#include "sched/interval.h"
#include "core/dependence_manager.h"
#include "sched/frame_interval.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
class qos_interval_private_t {
public:
    template <typename... Args>
    qos_interval_private_t(uint64_t deadlineUs, const QoS& qos)
    {
        if (qos == qos_user_interactive) {
            it = std::unique_ptr<Interval>(new (std::nothrow) FrameInterval(deadlineUs, qos));
        } else {
            it = std::unique_ptr<Interval>(new (std::nothrow) DefaultInterval(deadlineUs, qos));
        }
    }

    Interval* operator->()
    {
        return it.get();
    }

private:
    std::unique_ptr<Interval> it;
};

}; // namespace ffrt

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
ffrt_qos_interval_t ffrt_qos_interval_create(uint64_t deadline_us, ffrt_qos_t qos)
{
    if (qos < ffrt_qos_user_interactive) {
        FFRT_LOGE("Invalid QoS Interval!");
        return nullptr;
    }

    return new ffrt::qos_interval_private_t(deadline_us, qos);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_qos_interval_update(ffrt_qos_interval_t it, uint64_t new_deadline_us)
{
    if (!it) {
        FFRT_LOGE("QoS Interval Not Created Or Has Been Canceled!");
        return;
    }

    auto _it = static_cast<ffrt::qos_interval_private_t *>(it);

    (*_it)->Update(new_deadline_us);
}

API_ATTRIBUTE((visibility("default")))
int ffrt_qos_interval_begin(ffrt_qos_interval_t it)
{
    if (!it) {
        FFRT_LOGE("QoS Interval Not Created Or Has Been Canceled!");
        return -1;
    }

    auto _it = static_cast<ffrt::qos_interval_private_t *>(it);

    return (*_it)->Begin();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_qos_interval_end(ffrt_qos_interval_t it)
{
    if (!it) {
        FFRT_LOGE("QoS Interval Not Created Or Has Been Canceled!");
        return;
    }

    auto _it = static_cast<ffrt::qos_interval_private_t *>(it);

    (*_it)->End();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_qos_interval_destroy(ffrt_qos_interval_t it)
{
    if (!it) {
        FFRT_LOGE("QoS Interval Not Created Or Has Been Canceled!");
        return;
    }

    delete static_cast<ffrt::qos_interval_private_t *>(it);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_qos_interval_join(ffrt_qos_interval_t it)
{
    if (!it) {
        FFRT_LOGE("QoS Interval Not Created Or Has Been Canceled!");
        return;
    }

    auto _it = static_cast<ffrt::qos_interval_private_t *>(it);

    (*_it)->Join();
}

void ffrt_qos_interval_leave(ffrt_qos_interval_t it)
{
    if (!it) {
        FFRT_LOGE("QoS Interval Not Created Or Has Been Canceled!");
        return;
    }

    auto _it = static_cast<ffrt::qos_interval_private_t *>(it);

    (*_it)->Leave();
}

#ifdef __cplusplus
}
#endif