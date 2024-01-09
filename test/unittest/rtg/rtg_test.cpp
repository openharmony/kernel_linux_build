/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * SPDX-License-Identifier: GPL-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "rtg_test.h"
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <ctime>
#include <climits>
#include <pthread.h>
#include <sys/syscall.h>
#include <securec.h>
#include <string>
#include <vector>

using namespace testing::ext;
using namespace std;

constexpr size_t MAX_LENGTH = 100;
const char RTG_SCHED_IPC_MAGIC = 0xAB;
const int RTG_ERR = -1;
const int RTG_SUCC = 0;
const int MAX_TID_NUM = 5;
const int MULTI_FRAME_NUM = 5;

#define CMD_ID_SET_ENABLE \
    _IOWR(RTG_SCHED_IPC_MAGIC, SET_ENABLE, struct RtgEnableData)
#define CMD_ID_SET_RTG \
    _IOWR(RTG_SCHED_IPC_MAGIC, SET_RTG, struct RtgStrData)
#define CMD_ID_BEGIN_FRAME_FREQ \
    _IOWR(RTG_SCHED_IPC_MAGIC, BEGIN_FRAME_FREQ, struct ProcStateData)
#define CMD_ID_END_FRAME_FREQ \
    _IOWR(RTG_SCHED_IPC_MAGIC, END_FRAME_FREQ, struct ProcStateData)
#define CMD_ID_END_SCENE \
    _IOWR(RTG_SCHED_IPC_MAGIC, END_SCENE, struct ProcStateData)
#define CMD_ID_SET_MIN_UTIL \
    _IOWR(RTG_SCHED_IPC_MAGIC, SET_MIN_UTIL, struct ProcStateData)
#define CMD_ID_SET_MARGIN \
    _IOWR(RTG_SCHED_IPC_MAGIC, SET_MARGIN, struct ProcStateData)
#define CMD_ID_SET_RTG_ATTR \
    _IOWR(RTG_SCHED_IPC_MAGIC, SET_RTG_ATTR, struct RtgStrData)
#define CMD_ID_SET_CONFIG \
    _IOWR(RTG_SCHED_IPC_MAGIC, SET_CONFIG, struct RtgStrData)

struct RtgEnableData {
    int enable;
    int len;
    char *data;
};

struct RtgStrData {
    int type;
    int len;
    char *data;
};

struct ProcStateData {
    int grpId;
    int stateParam;
};

enum GrpCtrlCmd {
    CMD_CREATE_RTG_GRP,
    CMD_ADD_RTG_THREAD,
    CMD_REMOVE_RTG_THREAD,
    CMD_CLEAR_RTG_GRP,
    CMD_DESTROY_RTG_GRP
};

struct RtgGrpData {
    int rtgCmd;
    int grpId;
    int prioType;
    int rtCnt;
    int tidNum;
    int tids[MAX_TID_NUM];
};

struct RtgInfo {
    int rtgNum;
    int rtgs[MULTI_FRAME_NUM];
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

enum RtgType : int {
    VIP = 0,
    TOP_TASK_KEY,
    NORMAL_TASK,
    RTG_TYPE_MAX,
};

static int BasicOpenRtgNode()
{
    char fileName[] = "/proc/self/sched_rtg_ctrl";
    int fd = open(fileName, O_RDWR);

    if (fd < 0) {
        cout << "open node err." << endl;
    }

    return fd;
}

static int EnableRtg(bool flag)
{
    struct RtgEnableData enableData;
    char configStr[] = "load_freq_switch:1;sched_cycle:1";

    enableData.enable = flag;
    enableData.len = sizeof(configStr);
    enableData.data = configStr;
    int fd = BasicOpenRtgNode();
    if (fd < 0) {
        return RTG_ERR;
    }
    if (ioctl(fd, CMD_ID_SET_ENABLE, &enableData)) {
        close(fd);
        return RTG_ERR;
    }

    close(fd);
    return 0;
}

static int CreateNewRtgGrp(int prioType, int rtNum)
{
    struct RtgGrpData grpData;
    int ret;
    int fd = BasicOpenRtgNode();
    if (fd < 0) {
        return RTG_ERR;
    }
    (void)memset_s(&grpData, sizeof(struct RtgGrpData), 0, sizeof(struct RtgGrpData));
    if ((prioType > 0) && (prioType < RTG_TYPE_MAX)) {
        grpData.prioType = prioType;
    }
    if (rtNum > 0) {
        grpData.rtCnt = rtNum;
    }
    grpData.rtgCmd = CMD_CREATE_RTG_GRP;
    ret = ioctl(fd, CMD_ID_SET_RTG, &grpData);

    close(fd);
    return ret;
}

static int DestroyRtgGrp(int grpId)
{
    struct RtgGrpData grpData;
    int ret;
    int fd = BasicOpenRtgNode();
    if (fd < 0) {
        return fd;
    }
    (void)memset_s(&grpData, sizeof(struct RtgGrpData), 0, sizeof(struct RtgGrpData));
    grpData.rtgCmd = CMD_DESTROY_RTG_GRP;
    grpData.grpId = grpId;
    ret = ioctl(fd, CMD_ID_SET_RTG, &grpData);

    close(fd);
    return ret;
}

static int AddThreadToRtg(int tid, int grpId, int prioType)
{
    struct RtgGrpData grpData;
    int ret;
    int fd = BasicOpenRtgNode();
    if (fd < 0) {
        return fd;
    }
    (void)memset_s(&grpData, sizeof(struct RtgGrpData), 0, sizeof(struct RtgGrpData));
    grpData.tidNum = 1;
    grpData.tids[0] = tid;
    grpData.grpId = grpId;
    grpData.rtgCmd = CMD_ADD_RTG_THREAD;
    grpData.prioType = prioType;
    ret = ioctl(fd, CMD_ID_SET_RTG, &grpData);

    close(fd);
    return ret;
}

static int BeginFrameFreq(int stateParam)
{
    int ret = 0;
    struct ProcStateData stateData;
    stateData.stateParam = stateParam;
    int fd = BasicOpenRtgNode();
    if (fd < 0) {
        return fd;
    }
    ret = ioctl(fd, CMD_ID_BEGIN_FRAME_FREQ, &stateData);

    close(fd);
    return ret;
}

static int EndFrameFreq(int stateParam)
{
    int ret = 0;
    struct ProcStateData stateData;
    stateData.stateParam = stateParam;
    int fd = BasicOpenRtgNode();
    if (fd < 0) {
            return fd;
        }
    ret = ioctl(fd, CMD_ID_END_FRAME_FREQ, &stateData);

    close(fd);
    return ret;
}

static int EndScene(int grpId)
{
    int ret = 0;
    struct ProcStateData stateData;
    stateData.grpId = grpId;

    int fd = BasicOpenRtgNode();
    if (fd < 0) {
       return fd;
    }
    ret = ioctl(fd, CMD_ID_END_SCENE, &stateData);

    close(fd);
    return ret;
}

static int SetStateParam(unsigned int cmd, int stateParam)
{
    int ret = 0;
    struct ProcStateData stateData;
    stateData.stateParam = stateParam;

    int fd = BasicOpenRtgNode();
    if (fd < 0) {
        return fd;
    }
    ret = ioctl(fd, cmd, &stateData);

    close(fd);
    return ret;
}

static int SetFrameRateAndPrioType(int rtgId, int rate, int rtgType)
{
    int ret = 0;
    char formatData[MAX_LENGTH] = {};
    (void)sprintf_s(formatData, sizeof(formatData), "rtgId:%d;rate:%d;type:%d", rtgId, rate, rtgType);
    struct RtgStrData strData;
    strData.len = strlen(formatData);
    strData.data = formatData;

    int fd = BasicOpenRtgNode();
    if (fd < 0) {
        return fd;
    }
    ret = ioctl(fd, CMD_ID_SET_RTG_ATTR, &strData);

    close(fd);
    return ret;
}

static int AddThreadsToRtg(vector<int> tids, int grpId, int prioType)
{
    struct RtgGrpData grpData;
    int ret;
    int fd = BasicOpenRtgNode();

    if (fd < 0) {
        return fd;
    }
    (void)memset_s(&grpData, sizeof(struct RtgGrpData), 0, sizeof(struct RtgGrpData));
    int num = static_cast<int>(tids.size());
    if (num > MAX_TID_NUM) {
        return -1;
    }
    grpData.tidNum = num;
    grpData.grpId = grpId;
    grpData.rtgCmd = CMD_ADD_RTG_THREAD;
    grpData.prioType = prioType;
    for (int i = 0; i < num; i++) {
        if (tids[i] < 0) {
            return -1;
        }
        grpData.tids[i] = tids[i];
    }
    ret = ioctl(fd, CMD_ID_SET_RTG, &grpData);

    close(fd);
    return ret;
}

void RtgTest::SetUp() {
    // must enable rtg before use the interface
    int ret = EnableRtg(true);
    ASSERT_EQ(RTG_SUCC, ret);
}

void RtgTest::TearDown() {
    // disable rtg after use the interface
    int ret = EnableRtg(false);
    ASSERT_EQ(RTG_SUCC, ret);
}

void RtgTest::SetUpTestCase() {}

void RtgTest::TearDownTestCase() {}

/**
 * @tc.name: setEnableSucc
 * @tc.desc: Verify the enable rtg function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, setEnableSucc, Function | MediumTest | Level1)
{
    int ret;

    // test set enable again
    ret = EnableRtg(true);
    ASSERT_EQ(RTG_SUCC, ret);

    // test set disable again
    ret = EnableRtg(false);
    ASSERT_EQ(RTG_SUCC, ret);
}

/**
 * @tc.name: createAndDestroyRtgSucc
 * @tc.desc: Verify the create and destroy rtggrp function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, createAndDestroyRtgSucc, Function | MediumTest | Level1)
{
    int ret;
    int grpId;

    grpId = CreateNewRtgGrp(NORMAL_TASK, 0);
    ASSERT_GT(grpId, 0);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, 0);
}

/**
 * @tc.name: destoryErrorRtgGrp
 * @tc.desc: Verify Destroy function with error param.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, destoryErrorRtgGrp, Function | MediumTest | Level1)
{
    int ret;
    ret = DestroyRtgGrp(-1);
    ASSERT_NE(RTG_SUCC, ret);
}

/**
 * @tc.name: addRtgGrpSucc
 * @tc.desc: Verify add rtg function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, addRtgGrpSucc, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    int pid = getpid();

    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = AddThreadToRtg(pid, grpId, VIP);
    ASSERT_EQ(ret, 0);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, 0);
}

/**
 * @tc.name: addRtgGrpFail
 * @tc.desc: Verify add rtg function with error param.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, addRtgGrpFail, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    int pid = getpid();

    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);

    // error tid
    ret = AddThreadToRtg(-1, grpId, VIP);
    ASSERT_NE(ret, 0);

    // error grpid
    ret = AddThreadToRtg(pid, -1, VIP);
    ASSERT_NE(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, 0);
}

/**
 * @tc.name: begainFrameFreqSucc
 * @tc.desc: Verify rtg frame start function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, begainFrameFreqSucc, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    int pid = getpid();
    vector<int> pids = {};
    pids.push_back(pid);
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = AddThreadsToRtg(pids, grpId, VIP);
    ASSERT_EQ(ret, 0);
    ret = BeginFrameFreq(0);
    ASSERT_EQ(ret, 0);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, 0);
}

/**
 * @tc.name: RtgInterfaceBeginFrameFreqWithNoAddThreadtoGrp
 * @tc.desc: Verify rtg frame start function with NoAddThreadtoGrp.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, RtgInterfaceBeginFrameFreqWithNoAddThreadtoGrp, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = BeginFrameFreq(0);
    ASSERT_NE(ret, 0);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, 0);
}

/**
 * @tc.name: endFrameFreqSucc
 * @tc.desc: Verify rtg frame end function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, endFrameFreqSucc, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    int pid = getpid();
    vector<int> pids = {};
    pids.push_back(pid);
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = AddThreadsToRtg(pids, grpId, VIP);
    ASSERT_EQ(ret, 0);
    ret = EndFrameFreq(0);
    ASSERT_EQ(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}

/**
 * @tc.name: endFrameFreqWithNoAddThreadtoGrp
 * @tc.desc: Verify rtg frame end function with NoAddThreadtoGrp.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, endFrameFreqWithNoAddThreadtoGrp, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = EndFrameFreq(0);
    ASSERT_NE(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}


/**
 * @tc.name: endSceneSucc
 * @tc.desc: Verify scene end function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, endSceneSucc, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    int pid = getpid();
    vector<int> pids = {};
    pids.push_back(pid);
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = AddThreadsToRtg(pids, grpId, VIP);
    ASSERT_EQ(ret, 0);
    ret = EndScene(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}

/**
 * @tc.name: endSceneFail
 * @tc.desc: Verify scene end function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, endSceneFail, Function | MediumTest | Level1)
{
    int ret;

    ret = EndScene(-1);
    ASSERT_NE(ret, RTG_SUCC);
}

/**
 * @tc.name: setMinUtilSucc
 * @tc.desc: Verify set min util function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, setMinUtilSucc, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    int pid = getpid();
    vector<int> pids = {};
    pids.push_back(pid);
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = AddThreadsToRtg(pids, grpId, VIP);
    ASSERT_EQ(ret, 0);
    ret = SetStateParam(CMD_ID_SET_MIN_UTIL, 0);
    ASSERT_EQ(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}

/**
 * @tc.name: setMinUtilWithNoAddThreadtoGrp
 * @tc.desc: Verify set min util function with NoAddThreadtoGrp.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, setMinUtilWithNoAddThreadtoGrp, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = SetStateParam(CMD_ID_SET_MIN_UTIL, 0);
    ASSERT_NE(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}

/**
 * @tc.name: setMarginSucc
 * @tc.desc: Verify set min margin function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, setMarginSucc, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    int pid = getpid();
    vector<int> pids = {};
    pids.push_back(pid);
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = AddThreadsToRtg(pids, grpId, VIP);
    ASSERT_EQ(ret, 0);
    ret = SetStateParam(CMD_ID_SET_MARGIN, 0);
    ASSERT_EQ(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}

/**
 * @tc.name: setMarginWithNoAddThreadtoGrp
 * @tc.desc: Verify set min margin function with NoAddThreadtoGrp.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, setMarginWithNoAddThreadtoGrp, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = SetStateParam(CMD_ID_SET_MARGIN, 0);
    ASSERT_NE(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}

/**
 * @tc.name: SetRtgAttrSucc
 * @tc.desc: Verify rtg attr set function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, SetRtgAttrSucc, Function | MediumTest | Level1)
{
    int ret;
    int grpId;

    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = SetFrameRateAndPrioType(grpId, 60, VIP);
    ASSERT_EQ(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}

/**
 * @tc.name: SetRtgAttrFail
 * @tc.desc: Verify rtg attr set function with error param.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, SetRtgAttrFail, Function | MediumTest | Level1)
{
    int ret;
    int grpId;
    grpId = CreateNewRtgGrp(VIP, 0);
    ASSERT_GT(grpId, 0);
    ret = SetFrameRateAndPrioType(grpId, 90, -1);
    ASSERT_NE(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}

/**
 * @tc.name: RtgAddMutipleThreadsSucc
 * @tc.desc: Verify rtg multiple add function.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, RtgAddMutipleThreadsSucc, Function | MediumTest | Level1)
{
    int ret;
    int pid[3];
    vector<int> threads;
    int grpId;

    for (int i = 0; i < 3; i++) {
        pid[i] = fork();
        ASSERT_TRUE(pid[i] >= 0) << "> parent: fork errno = " << errno;
        if (pid[i] == 0) {
            usleep(50000);
             _Exit(0);
        }
        threads.push_back(pid[i]);
    }
    grpId = CreateNewRtgGrp(NORMAL_TASK, 0);
    ASSERT_GT(grpId, 0);
    ret = AddThreadsToRtg(threads, grpId, NORMAL_TASK);
    ASSERT_EQ(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}

/**
 * @tc.name: RtgAddMutipleThreadsOutOfLimit
 * @tc.desc: Verify rtg multiple add function with out of limit threads.
 * @tc.type: FUNC
 */
HWTEST_F(RtgTest, RtgAddMutipleThreadsOutOfLimit, Function | MediumTest | Level1)
{
    int ret;
    int pid[8];
    vector<int> threads;
    int grpId;

    for (int i = 0; i < 8; i++) {
        pid[i] = fork();
        ASSERT_TRUE(pid[i] >= 0) << "> parent: fork errno = " << errno;
        if (pid[i] == 0) {
            usleep(50000);
            _Exit(0);
        }
    threads.push_back(pid[i]);
    }
    grpId = CreateNewRtgGrp(NORMAL_TASK, 0);
    ASSERT_GT(grpId, 0);
    ret = AddThreadsToRtg(threads, grpId, NORMAL_TASK);
    ASSERT_NE(ret, RTG_SUCC);
    ret = DestroyRtgGrp(grpId);
    ASSERT_EQ(ret, RTG_SUCC);
}
