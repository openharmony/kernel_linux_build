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

#include <cstddef>
#include <cstdint>
#include "hccommon.h"

const char *g_hcWarmDataWaterline = "/sys/fs/f2fs/loop1/hc_warm_data_waterline";

namespace OHOS {
bool HcWarmDataWaterLineFuzzTest(const uint8_t *data, size_t size)
{
    bool ret = HcFuzzTest(data, g_hcWarmDataWaterline, size);
    return ret;
}
} // namespace OHOS

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::HcWarmDataWaterLineFuzzTest(data, size);
    return 0;
}
