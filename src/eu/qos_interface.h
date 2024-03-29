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

#ifndef QOS_INTERFACE_H
#define QOS_INTERFACE_H
#include "internal_inc/config.h"
#include "eu/worker_thread.h"
#ifdef __cplusplus
extern "C" {
#endif

/*
 * generic
 */
#define SYSTEM_UID 1000
#define ROOT_UID 0

/*
 * auth_ctrl
 */
struct AuthCtrlData {
    unsigned int uid;
    unsigned int type;
    unsigned int rtgUaFlag;
    unsigned int qosUaFlag;
    unsigned int status;
};

enum AuthManipulateType {
    AUTH_ENABLE = 1,
    AUTH_DELETE,
    AUTH_GET,
    AUTH_SWITCH,
    AUTH_MAX_NR,
};

enum AuthStatus {
    AUTH_STATUS_DISABLED = 1,
    AUTH_STATUS_SYSTEM_SERVER = 2,
    AUTH_STATUS_FOREGROUND = 3,
    AUTH_STATUS_BACKGROUND = 4,
    AUTH_STATUS_DEAD,
};

enum AuthCtrlCmdid {
    BASIC_AUTH_CTRL = 1,
    AUTH_CTRL_MAX_NR
};

#define AUTH_CTRL_IPC_MAGIG 0xCD

#define BASIC_AUTH_CTRL_OPERATION \
    _IOWR(AUTH_CTRL_IPC_MAGIG, BASIC_AUTH_CTRL, struct AuthCtrlData)

/*
 * qos ctrl
 */

constexpr unsigned char NR_QOS = 6;
constexpr unsigned char QOS_NUM_MAX = 10;

constexpr unsigned char AF_QOS_ALL = 0x0003;
constexpr unsigned char AF_QOS_DELEGATED = 0x0001;

enum QosManipulateType {
    QOS_APPLY = 1,
    QOS_LEAVE,
    QOS_MAX_NR,
};

struct QosCtrlData {
    int pid;
    unsigned int type;
    unsigned int level;
};

struct QosPolicyData {
    int latency_nice;
    int uclamp_min;
    int uclamp_max;
    unsigned long affinity;
    unsigned char priority;
    unsigned char init_load;
    unsigned char prefer_idle;
};

enum QosPolicyType {
    QOS_POLICY_DEFAULT = 1,
    QOS_POLICY_SYSTEM_SERVER = 2,
    QOS_POLICY_FRONT = 3,
    QOS_POLICY_BACK = 4,
    QOS_POLICY_MAX_NR,
};

constexpr unsigned char QOS_FLAG_NICE = 0X01;
constexpr unsigned char QOS_FLAG_LATENCY_NICE = 0X02;
constexpr unsigned char QOS_FLAG_UCLAMP = 0x04;
constexpr unsigned char QOS_FLAG_RT = 0x08;

#define QOS_FLAG_ALL    (QOS_FLAG_NICE          | \
            QOS_FLAG_LATENCY_NICE       | \
            QOS_FLAG_UCLAMP     | \
            QOS_FLAG_RT)

struct QosPolicyDatas {
    int policyType;
    unsigned int policyFlag;
    struct QosPolicyData policys[NR_QOS + 1];
};

enum QosCtrlCmdid {
    QOS_CTRL = 1,
    QOS_POLICY,
    QOS_CTRL_MAX_NR
};

#define QOS_CTRL_IPC_MAGIG 0xCC

#define QOS_CTRL_BASIC_OPERATION \
    _IOWR(QOS_CTRL_IPC_MAGIG, QOS_CTRL, struct QosCtrlData)
#define QOS_CTRL_POLICY_OPERATION \
    _IOWR(QOS_CTRL_IPC_MAGIG, QOS_POLICY, struct QosPolicyDatas)

/*
 * RTG
 */
#define AF_RTG_ALL          0x1fff
#define AF_RTG_DELEGATED    0x1fff

struct RtgEnableData {
    int enable;
    int len;
    char *data;
};

enum RtgSchedCmdid {
    SET_ENABLE = 1,
    SET_RTG,
    SET_CONFIG,
    SET_RTG_ATTR,
    BEGIN_FRAME_FREQ = 5,
    END_FRAME_FREQ,
    END_SCENE,
    SET_MIN_UTIL,
    SET_MARGIN,
    LIST_RTG = 10,
    LIST_RTG_THREAD,
    SEARCH_RTG,
    GET_ENABLE,
    RTG_CTRL_MAX_NR,
};

#define RTG_SCHED_IPC_MAGIC 0xAB

#define CMD_ID_SET_ENABLE \
    _IOWR(RTG_SCHED_IPC_MAGIC, SET_ENABLE, struct RtgEnableData)

/*
 * interface
 */
int EnableRtg(bool flag);
int AuthEnable(unsigned int uid, unsigned int uaFlag, unsigned int status);
int AuthPause(unsigned int uid);
int AuthDelete(unsigned int uid);
int AuthGet(unsigned int uid, unsigned int *uaFlag, unsigned int *status);
int AuthSwitch(unsigned int uid, unsigned int rtgFlag, unsigned int qosFlag, unsigned int status);
int QosApply(unsigned int level);
int QosApplyForOther(unsigned int level, int tid);
int QosLeave(void);
int QosLeaveForOther(int tid);
int QosPolicy(struct QosPolicyDatas *policyDatas);
typedef int (*Func_affinity)(unsigned long affinity, int tid);
void setFuncAffinity(Func_affinity func);
Func_affinity getFuncAffinity();
typedef int (*Func_priority)(unsigned char priority, ffrt::WorkerThread* thread);
void setFuncPriority(Func_priority func);
Func_priority getFuncPriority();

#ifdef __cplusplus
}
#endif

#endif /* OQS_INTERFACE_H */
