/*--- 模块头文件（无） ---*/

/*--- 项目头文件 ---*/
#include "ResAPI.h"
#include "dut_group_types.h"
#include "soc_context.h"
#include "ipc_api.h"
#include "session_types.h"
#include "session.h"

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件 ---*/
#include <stdlib.h>
#include <string.h>


const char *pSoftVer = "version_module: V1.0.1("__DATE__" "__TIME__")""(ResMgmt)";



// resMgmt模块初始化，提供给MCI模块调用的
RES_API int32_t res_Init(int argc, char **argv)
{
    int ret;
    const char *socketName = NULL;

    // 先根据配置文件的或 gLogLevel参数

    // 日志文件的相关初始化


    // 0. 解析全部命令行参数 + 会话信息初始化（最先执行，后续模块可能需要查询）
    {
        SessionInitParam sessParam;
        uint32_t         funcKeys[RES_MAX_FUNC_KEY_COUNT];

        memset(&sessParam, 0, sizeof(sessParam));
        memset(funcKeys, 0, sizeof(funcKeys));

        for (int i = 0; i < argc; i++) {
            /* --socketName=<path> */
            if (strncmp(argv[i], "--socketName=", 13) == 0) {
                socketName = argv[i] + 13;
            }
            /* --funcKey=1,2,3,4 */
            else if (strncmp(argv[i], "--funcKey=", 10) == 0) {
                char *val = argv[i] + 10;
                char *saveptr;
                char *token = strtok_r(val, ",", &saveptr);
                while (token != NULL
                       && sessParam.func_key_count < RES_MAX_FUNC_KEY_COUNT) {
                    funcKeys[sessParam.func_key_count++] = (uint32_t)atoi(token);
                    token = strtok_r(NULL, ",", &saveptr);
                }
                sessParam.func_keys = funcKeys;
            }
            /* --programName=<name> */
            else if (strncmp(argv[i], "--programName=", 14) == 0) {
                sessParam.program_name = argv[i] + 14;
            }
            /* --testName=<name> */
            else if (strncmp(argv[i], "--testName=", 11) == 0) {
                sessParam.test_name = argv[i] + 11;
            }
            /* --siteNo=<n> */
            else if (strncmp(argv[i], "--siteNo=", 9) == 0) {
                sessParam.site_no = (uint32_t)atoi(argv[i] + 9);
            }
        }

        ret = session_init(&sessParam);
        if (ret != SESSION_OK) {
            return ret;
        }
    }

    // 1. DUT 分组管理子模块初始化
    ret = Res_DutGroupInit();
    if (ret != DUT_GROUP_OK ) {
        return ret;
    }

    // 2. 解析线程模块初始化 (socket 编译解析工作线程)
    ret = socket_json_parse_init(socketName);
    if (ret != SOC_OK) {
        return ret;
    }

    // 3. stdio的初始化，等模块连接通知之后再加载   1111111111111111

    return 0;
}
