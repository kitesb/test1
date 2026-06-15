#include <stdio.h>
#include <stdbool.h>

#include "internal.h"
#include "dealsoc.h"
#include "log_mgmt.h"


const char *pSoftVer = "version_module: V1.0.1("__DATE__" "__TIME__")""(ResMgmt)";



// resMgmt模块初始化，提供给MCI模块调用的
RES_API int32_t res_Init(uint32_t argc, char *argv)
{
    // 先根据配置文件的或 gLogLevel参数

    // 日志文件的相关初始化


    // 1. 完成全局变量的初始化

    // 2. 进程间通信的初始化，启动收发消息的线程
    init_ipc_msg(); // 会起失败的，后面再重试 1111111111111111
    init_ipc_queue();

    // 3. 解析线程模块初始化 (socket 编译解析工作线程)
    socket_json_parse_init(argc, argv);

    // 4. stdio的初始化，等模块连接通知之后再加载   1111111111111111

    return 0;
}
