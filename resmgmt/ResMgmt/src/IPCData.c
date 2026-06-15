#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "IPCData.h"
#include "internal.h"
#include "log_mgmt.h"

#define DEF_SERVER_PORT  8888
#define DEF_SERVER_IP    "127.0.0.1"

#define BUFF_QUEUE_SIZE  2048 
#define MAX_QUEUE_SIZE   50


typedef struct {
    const char *server_ip;
    int    server_port;
    int    sock_fd;
    pthread_t  send_thread;
    pthread_t  recv_thread;
}_IPCInfo;
_IPCInfo sIPCInfo;


// ��Ϣ�ṹ��
typedef struct 
IPCMsgHeader{
    char data[BUFF_QUEUE_SIZE];
    int  is_sent;        // 0:������, 1:�ѷ���
    int  msg_id;         // ��ϢID
    time_t timestamp;
} _MsgData;

// ��Ϣ���нṹ��
typedef struct 
{
    _MsgData messages[MAX_QUEUE_SIZE];
    int front;          // ����
    int rear;           // ��β
    int count;          // �����е���Ϣ����
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} _MsgQueue;
_MsgQueue send_queue;
_MsgQueue recv_queue;

unsigned int g_msg_id = 1;


int init_ipc_msg(void)
{
    int ret;
    
    sIPCInfo.sock_fd = -1;
    sIPCInfo.server_ip =   DEF_SERVER_IP;
    sIPCInfo.server_port = DEF_SERVER_PORT;
    
    // ��ʼ��TCP�ͻ���
    sIPCInfo.sock_fd = tcp_client_init(sIPCInfo.server_ip, sIPCInfo.server_port);
    if (sIPCInfo.sock_fd < 0) {
        DBG_ERR("tcp_client_init: failed sock_fd=%d ", sIPCInfo.sock_fd);
        return RET_TCP_ERR;
    }
    
    // ���������߳�
    ret = pthread_create(&sIPCInfo.send_thread, NULL, send_thread_func, &sIPCInfo.sock_fd);
    if (ret != 0) {
        DBG_ERR("pthread_create-send fail, ret=%d ", ret);
        return RET_PTHREAD_ERR;
    }
    
    // ���������߳�
    ret = pthread_create(&sIPCInfo.recv_thread, NULL, recv_thread_func, &sIPCInfo.sock_fd);
    if (ret != 0) {
        DBG_ERR("pthread_create-recv fail, ret=%d ", ret);        
        return RET_PTHREAD_ERR;
    }

    return RET_OK;
}


int tcp_client_init(const char* server_ip, int server_port) {
    int ret;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        DBG_ERR("socket creat failed.");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // ת��IP��ַ
    ret = inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    if (ret <= 0) {
        DBG_ERR("invalid ip addr (inet_pton:ret=%d).", ret);
        close(sock);
        return -1;
    }
    
    // ���ӵ�������
    ret = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret < 0) {
        DBG_ERR("connect failed (connect:ret=%d).", ret);
        close(sock);
        return -1;
    }
    
    DBG_DBG("connect sucess(%s:%d).", server_ip, server_port);
    return sock;
}



// ��ʼ����Ϣ����
void init_message_queue(_MsgQueue* queue) 
{
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    memset(queue->messages, 0, sizeof(queue->messages));
    DBG_DBG("init msg queue.");
}

// ������Ϣ����
void destroy_message_queue(_MsgQueue* queue) 
{
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    DBG_DBG("destroy msg queue.");
}


// ��ʼ��IPCͨ�ŵ���Ϣ����
int init_ipc_queue(void)
{
    // ��ʼ����Ϣ����
    init_message_queue(&send_queue);
    init_message_queue(&recv_queue);
    DBG_DBG("init ipc msg queue.");
    return RET_OK;
}

// �����������Ϣ
int enqueue_message(_MsgQueue* queue, const char* data) 
{
    pthread_mutex_lock(&queue->mutex);
    
    // �ȴ����в����� ����Ҫ�Ӹ�����ʱ�䣬����һֱ���� 1111111111
    while (queue->count >= MAX_QUEUE_SIZE) 
    {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    
    // ������Ϣ
    _MsgData* msg = &queue->messages[queue->rear];
    strncpy(msg->data, data, BUFF_QUEUE_SIZE - 1);
    msg->data[BUFF_QUEUE_SIZE - 1] = '\0';
    msg->is_sent = 0;
    msg->msg_id = g_msg_id++;
    msg->timestamp = time(NULL);
    
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    queue->count++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    DBG_DBG("enqueue_message:msg_id=%d", msg->msg_id);
    return msg->msg_id;
}

// �Ӷ���ȡ����Ϣ
int dequeue_message(_MsgQueue* queue, _MsgData* msg) {
    pthread_mutex_lock(&queue->mutex);

    DBG_DBG("dequeue_message.enter");
    
    // �ȴ����зǿ�
    while (queue->count == 0) 
    {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    DBG_DBG("dequeue_message.count=%d");
    
    if (queue->count == 0) 
    {
        DBG_ERR("dequeue_message:count=0");
        pthread_mutex_unlock(&queue->mutex);
        return RET_ERR;
    }
    
    // ȡ����Ϣ
    *msg = queue->messages[queue->front];
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    queue->count--;
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    DBG_DBG("dequeue_message.sucess");
    return RET_OK;
}

// �����ϢΪ�ѷ���
void mark_message_sent(_MsgQueue* queue, int msg_id) {
    pthread_mutex_lock(&queue->mutex);
    
    for (int i = 0; i < queue->count; i++) 
    {
        int idx = (queue->front + i) % MAX_QUEUE_SIZE;
        if (queue->messages[idx].msg_id == msg_id) {
            queue->messages[idx].is_sent = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&queue->mutex);
}


// �����߳� - �ӷ��Ͷ���ȡ��Ϣ������
void* send_thread_func(void* arg) {
    _MsgData msg;
    int sock = *(int*)arg;

    DBG_ERR("send_thread_func.enter");
    
    while (1) 
    {
        // �ӷ��Ͷ���ȡ��Ϣ
        if (dequeue_message(&send_queue, &msg) == 0) 
        {
            
            // �������ݵ�������
            int send_len = send(sock, msg.data, strlen(msg.data), 0);
            if (send_len <= 0) 
            {
                if (errno != EINTR) {
                    DBG_ERR("send_thread_func:send error");
                }
                break;
            }
            
            // �����ϢΪ�ѷ���
            mark_message_sent(&send_queue, msg.msg_id);
            
            char time_str[64];
            struct tm* tm_info = localtime(&msg.timestamp);
            strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
            DBG_DBG("[send sucess] ID:%d (%s) data:[%s]\n", 
                   msg.msg_id, time_str, msg.data);
        }
    }

    DBG_ERR("send_thread_func.exit");
    return NULL;
}

// �����߳� - ������Ϣ��������ն���
void* recv_thread_func(void* arg) 
{
    char buffer[BUFF_QUEUE_SIZE];
    int sock = *(int*)arg;

    DBG_ERR("recv_thread_func.enter");
    
    while (1) 
    {
        memset(buffer, 0, BUFF_QUEUE_SIZE);
        
        int recv_len = recv(sock, buffer, BUFF_QUEUE_SIZE - 1, 0);
        if (recv_len <= 0) {
            if (recv_len == 0) 
            {
                DBG_ERR("recv error:len=0");
            } 
            else if (errno != EINTR) 
            {
                DBG_ERR("recv error:ret.%d", recv_len);
            }
            
            break;
        }
        
        buffer[recv_len] = '\0';
        
        // �����յ�����Ϣ������ն���
        enqueue_message(&recv_queue, buffer);
    }

    DBG_ERR("recv_thread_func.exit");
    return NULL;
}


// -------------------------------------

#if 1

RES_API void tst_send(char *pData)
{
    // ����Ϣ���뷢�Ͷ���
    int msg_id = enqueue_message(&send_queue, pData);
    if (msg_id >= 0) 
    {
        printf("[input send queue] ID:%d data:(%s) \n", msg_id, pData);
    }
}

RES_API void tst_recv()
{
    _MsgData msg;

    printf("\nwait recv............\n");
    while (1) {
        // �ӽ��ն���ȡ��Ϣ����ʾ
        if (dequeue_message(&recv_queue, &msg) == 0) 
        {
            char time_str[64];
            struct tm* tm_info = localtime(&msg.timestamp);
            strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
            printf("[recv] ID:%d time:%s data:(%s)\n", 
                   msg.msg_id, time_str, msg.data);
            break;
        }
    }
}



#endif



