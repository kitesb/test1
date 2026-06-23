/*--- 模块头文件 ---*/
#include "log_mgmt.h"

/*--- 项目头文件（无） ---*/

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件 ---*/
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

uint32_t gLogLevel = LOG_DBG;
#define  LOG_FILE_MAX  (20971520)  // (20*1024*1024)

int8_t   LOG_FILE[LOG_FILE_LEN + 1] = {0};

#define ADD_LOG_HEADER() \
    pLevel = "RES_ERR: ";\
    if (LOG_WARN == level)\
    {\
        pLevel = "RES_WRN: ";\
    }\
    else if (LOG_ERR == level)\
    {\
        pLevel = "RES_ERR: ";\
    }\
    else if (LOG_INFO == level)\
    {\
        pLevel = "RES_INF: ";\
    }\
    else if (LOG_DBG == level)\
    {\
        pLevel = "RES_DBG: ";\
    }\
    else if (LOG_ENTER == level)\
    {\
        pLevel = "RES_ENT: ";\
    }\
    else if (LOG_EXIT == level)\
    {\
        pLevel = "RES_EXI: ";\
    }

void printf_log2(uint32_t level, int8_t *format, ...)
{
    char  *pLevel  = PTR_NULL;
    FILE  *pFile   = PTR_NULL;
    int len = 0;
    va_list args;

    char datax[100] = {0};

    // ������־����
    if (level > gLogLevel)
    {
        return;
    }

    deal_log_file(LOG_FILE);

    ADD_LOG_HEADER();

    len = strlen(pLevel);
    strcpy(datax, pLevel);
    snprintf(datax+len, 15, "PID=%d. ", getpid());
    pLevel = datax;

    pFile = fopen(LOG_FILE, "a+");
    if (PTR_NULL == pFile)
    {
        //fprintf(stderr, "\n======  ERROR! open file (%s) failed ======\n", LOG_FILE);
        ERR_PRINT("\n======  ERROR! open file (%s) failed. ======\n", LOG_FILE);
        return;
    }
    fseek(pFile, 0, SEEK_END); //�ļ�ָ������ļ�ĩβ

    len = strlen(pLevel);
    fwrite(pLevel, 1, len, pFile);

    va_start(args, format);
    vfprintf(pFile, format, args);
    va_end(args);

    char data[100] = {0};
    struct timeval time;
    struct tm * pTempTm;

    gettimeofday(&time, PTR_NULL);

    memset(data, 0, 100);

    pTempTm = localtime(&time.tv_sec);
    if (PTR_NULL != pTempTm )
    {
        snprintf(data, 100, " (%02d:%02d:%02d.%03ld)",
            pTempTm->tm_hour,
            pTempTm->tm_min,
            pTempTm->tm_sec,
            time.tv_usec/1000);

        len = strlen(data);
        fwrite(data, 1, len, pFile);
    }

    len = strlen("\r\n");
    fwrite("\r\n", 1, len, pFile);

    fclose(pFile);

    return;
}

void get_only_date(int8_t *pDate)
{
    struct timeval tv;
    struct tm tm_info;
    char time_buffer[30]={0};
    int tmp = 0;

    gettimeofday(&tv, PTR_NULL);

    localtime_r(&tv.tv_sec, &tm_info);

    strftime(time_buffer, sizeof(time_buffer), "%Y-%m%d %H:%M:%S", &tm_info);
    //strncpy(pDate, time_buffer + 5, 4); // �����·ݺ�����
    tmp = 0;
    pDate[tmp] = time_buffer[5+tmp];
    tmp = 1;
    pDate[tmp] = time_buffer[5+tmp];
    tmp = 2;
    pDate[tmp] = time_buffer[5+tmp];
    tmp = 3;
    pDate[tmp] = time_buffer[5+tmp];
    pDate[4] = 0;

    return;
}

// ��ȡָ��Ŀ¼�������ļ����
int find_max_file_number(const char *directory, const char *prefix)
{
    DIR *dir;
    struct dirent *entry;
    int max_number = 0;
    char dat[10] = {0};

    dir = opendir(directory);
    if (dir == PTR_NULL) {
        DBG_printf("can't open(%s)", directory);
        return 0;
    }

    while ((entry = readdir(dir)) != PTR_NULL)
    {
        // ���� . �� .. Ŀ¼
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // ����ļ�����ʽ
        DBG_printf("mci d_name file(%s), prefix(%s)\n", entry->d_name, prefix);
        if (0 != strncmp(entry->d_name, prefix, LOG_FILE_NAME_NUM))
        {
            continue;
        }

        // ��ȡ��� (dat�̶�����4���ֽڣ����ﲻ���ǳ�ʼ��)
        strncpy(dat, entry->d_name+LOG_FILE_NAME_NUM, 4);
        int number = atoi(dat);
        if (number > max_number)
        {
            max_number = number;
        }
    }

    closedir(dir);
    return max_number;
}


// ������־��ӡ�ļ�������ʼ��ֵ���ļ�����һ������ʱ���ļ���+1
void deal_log_file(int8_t *pFileName)
{
    int tmp = 0;

    if (0 == strlen(pFileName))
    {
        strcpy(pFileName, LOG_FILE_PATH);
        strcpy(pFileName + strlen(LOG_FILE_PATH), LOG_FILE_NAME);

        DBG_printf("0.mci log file(%s)\n", pFileName);

        // �޸��ļ����е����ڣ�
        int8_t aDate[20] = {0};
        get_only_date(aDate);
        //strncpy(pFileName + LOG_FILE_DATE, aDate, 4);
        tmp = 0;
        pFileName[LOG_FILE_DATE+tmp] = aDate[tmp];
        tmp = 1;
        pFileName[LOG_FILE_DATE+tmp] = aDate[tmp];
        tmp = 2;
        pFileName[LOG_FILE_DATE+tmp] = aDate[tmp];
        tmp = 3;
        pFileName[LOG_FILE_DATE+tmp] = aDate[tmp];

        DBG_printf("1.mci log file(%s)\n", pFileName);

        // ���ҵ�ǰ�����Ƿ����ļ����еĻ������������
        strncpy(aDate, pFileName+strlen(LOG_FILE_PATH), LOG_FILE_NAME_NUM);
        int num = find_max_file_number(LOG_FILE_PATH, aDate);
        DBG_printf("file max num (%d)\n", num);
        num ++;

        memset(aDate, 0, 20);
        sprintf(aDate, "%04d", num);
        //strncpy(pFileName+LOG_FILE_NUM, aDate, 4);
        tmp = 0;
        pFileName[LOG_FILE_NUM+tmp] = aDate[tmp];
        tmp = 1;
        pFileName[LOG_FILE_NUM+tmp] = aDate[tmp];
        tmp = 2;
        pFileName[LOG_FILE_NUM+tmp] = aDate[tmp];
        tmp = 3;
        pFileName[LOG_FILE_NUM+tmp] = aDate[tmp];
        DBG_printf("\n2.mci log file(%s)\n", pFileName);

        return;
    }

    // �ж��Ƿ��ļ��Ƿ񳬴󣬳���Ļ������ļ���+1
    struct stat file_stat;
    if (stat(pFileName, &file_stat) == 0)
    {
	    if (file_stat.st_size > LOG_FILE_MAX)
	    {
            int8_t data[20] = {0};
            int num = 0;
            strncpy(data, pFileName+LOG_FILE_NUM, 4);
            num = atoi(data);
            num++;
            if (num > 9999)
            {
                // �쳣������߲�����������ֻ��ӡ������������ļ���
                //fprintf(stderr, "\n\n\n---ERROR ERROR ERROR :mci log file over 999---\n\n\n");
                ERR_PRINT("\n\n\n---ERROR ERROR ERROR :mci log file over 999---\n\n\n");
                return;
            }

            // ��Ϊdata����С��4�ֽڣ����ﲻ����0����
            snprintf(data, 20, "%04d", num);
            data[4] = 0;
            //strncpy(pFileName+LOG_FILE_NUM, data, 4);
            tmp  = 0;
            pFileName[LOG_FILE_NUM+tmp] = data[tmp];
            tmp  = 1;
            pFileName[LOG_FILE_NUM+tmp] = data[tmp];
            tmp  = 2;
            pFileName[LOG_FILE_NUM+tmp] = data[tmp];
            tmp  = 3;
            pFileName[LOG_FILE_NUM+tmp] = data[tmp];
            DBG_printf("3.mci log file(%s)\n", pFileName);
	    }
    }

    // �賿12�㣬�����ļ��������ڵĴ������Ȳ�����  111111111111111111111

}
