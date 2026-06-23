/*--- 模块头文件 ---*/
#include "redirect.h"

/*--- 项目头文件 ---*/
#include "log_mgmt.h"

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件 ---*/
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct
{
    bool is_stdout;
    bool is_stderr;
    int stdout_fd;
    int stderr_fd;
    int saved_stdout;
    int saved_stderr;
}_redirect_info;
_redirect_info  sRedirect;


void redirect_stdio_to_socket()
{
    sRedirect.is_stdout = false;
    sRedirect.is_stderr = false;
    sRedirect.stdout_fd = -1;
    sRedirect.stderr_fd = -1;

    start_stdout_to_socket();
    start_stderr_to_socket();
}

void redirect_stdio_to_file()
{

}

static int connect_to_server(const char *ip, int port) {
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
    {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }

    return fd;
}
bool start_stdout_to_socket()
{
    const char *ip = "127.0.0.1";
    int port = 8192;

    if (false != sRedirect.is_stdout)
    {
        DBG_printf("start_stdout_to_socket has redirect \n");
        return false;
    }

    sRedirect.saved_stdout = dup(STDOUT_FILENO);

    if (sRedirect.saved_stdout < 0)
    {
        DBG_printf("STDOUT_FILENO dup failed\n");
        return false;
    }

    sRedirect.stdout_fd = connect_to_server(ip, port);
    if (sRedirect.stdout_fd < 0) {
        DBG_printf("STDOUT connect failed\n");
        close(sRedirect.saved_stdout);
        return false;
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    sRedirect.is_stdout = true;

    return true;
}

bool start_stderr_to_socket()
{
    const char *ip = "127.0.0.1";
    int port = 8193;

    if (false != sRedirect.is_stderr)
    {
        DBG_printf("start_stderr_to_socket has redirect \n");
        return false;
    }

    sRedirect.saved_stderr = dup(STDERR_FILENO);

    if (sRedirect.saved_stderr < 0)
    {
        DBG_printf("STDOUT_FILENO dup failed\n");
        return false;
    }

    sRedirect.stderr_fd = connect_to_server(ip, port);
    if (sRedirect.stderr_fd < 0) {
        DBG_printf("STDERR connect failed\n");
        close(sRedirect.saved_stderr);
        return false;
    }

    setvbuf(stderr, NULL, _IONBF, 0);
    sRedirect.is_stderr = true;

    return true;
}

bool stop_stdout_to_socket()
{
    if (true != sRedirect.is_stdout)
    {
        DBG_printf("stdout Not redirected\n");
        return false;
    }

    fflush(stdout);

    if (dup2(sRedirect.saved_stdout, STDOUT_FILENO) < 0)
    {
        DBG_printf("stdout restore dup2 failed\n");
        return false;
    }

    close(sRedirect.saved_stdout);
    close(sRedirect.stdout_fd);

    sRedirect.is_stdout = false;
    sRedirect.stdout_fd = -1;
    sRedirect.saved_stdout = -1;

    return true;
}

bool stop_stderr_to_socket()
{
    if (true != sRedirect.is_stderr)
    {
        DBG_printf("stderr Not redirected\n");
        return false;
    }

    fflush(stderr);

    if (dup2(sRedirect.saved_stderr, STDERR_FILENO) < 0)
    {
        DBG_printf("stderr restore dup2 failed\n");
        return false;
    }

    close(sRedirect.saved_stderr);
    close(sRedirect.stderr_fd);

    sRedirect.is_stderr    = false;
    sRedirect.stderr_fd    = -1;
    sRedirect.saved_stderr = -1;

    return true;
}


bool in_redirect = false;
int in_pipefd[2];
#define MAX_STDIN_BUF_LEN   1024
char in_content[MAX_STDIN_BUF_LEN+1];

void redirect_stdin_to_socket()
{
    if (true == in_redirect)
    {
        return;
    }

    pipe(in_pipefd);

    dup2(in_pipefd[0], STDIN_FILENO);

    in_redirect = true;
}

#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#define MIN(a, b)   ((a) < (b) ? (a) : (b))


int write_data_to_stdin(char *pData, int mLen)
{
    int len = 0;
    int ret;

    if (NULL == pData)
    {
        return RET_ERR;
    }

    len = MIN(mLen, MAX_STDIN_BUF_LEN);
    in_content[len] = 0;

    memcpy(in_content, pData, len);

    ret = write(in_pipefd[1], in_content, len);
    if (ret != len)
    {
        DBG_ERR("write %d, success %d", len, ret);
    }

    return RET_OK;
}

void close_redirect_stdin()
{
    close(in_pipefd[0]);
    close(in_pipefd[1]);
}
