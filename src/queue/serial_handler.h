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
#ifndef FFRT_SERIAL_HANDLER_H
#define FFRT_SERIAL_HANDLER_H

#include "queue/ihandler.h"
#include "queue/serial_looper.h"

namespace ffrt {
class ITask;
class SerialHandler : public IHandler {
public:
    explicit SerialHandler(const std::shared_ptr<SerialLooper>& looper) : looper_(looper) {}

    int Cancel(ITask* task) override;
    void DispatchTask(ITask* task) override;
    int SubmitDelayed(ITask* task, uint64_t delayUs = 0) override;

private:
    void DestroyTask(ITask* task);
    const std::shared_ptr<SerialLooper> looper_;
};
} // namespace ffrt

#endif // FFRT_SERIAL_HANDLER_H