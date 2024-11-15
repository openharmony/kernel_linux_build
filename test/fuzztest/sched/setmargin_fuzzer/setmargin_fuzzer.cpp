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

#include <vector>
#include <cstddef>
#include <cstdint>
#include "__config"
#include "rtg_interface.h"

using namespace std;
using namespace OHOS::RME;
namespace OHOS {
bool SetMarginFuzzTest(const uint8_t *data, size_t size)
{
    bool ret = false;
    constexpr int marginUpperLimitTime = 32000;
    constexpr int marginLowerLimitTime = -32000;
    if (data == nullptr) {
        return ret;
    } else {
        uint8_t *countData = const_cast<uint8_t *>(data);
        int margin = *(reinterpret_cast<int *>(countData));
        if (margin < marginLowerLimitTime || margin > marginUpperLimitTime) {
            return ret;
        }
        ret = SetMargin(margin);
    }
    return ret;
}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    OHOS::SetMarginFuzzTest(data, size);
    return 0;
}
