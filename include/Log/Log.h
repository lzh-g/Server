#ifndef _LOG_H_
#define _LOG_H_

#include "stdafx.h"

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>

#include "BlockQueue.h"
#include "../Locker/Locker.h"

using namespace std;

class Log
{
public:
    static Log *GetInstance();

    Log();
    virtual ~Log();

    void *AsyncWriteLog();

    static void *FlushLogThread(void *args);

    // 可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool Init(const char *file_name, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    void WriteLog(int level, const char *format, ...);

    void flush(void);

private:
    char m_dirName[128]; // 路径名
    char m_logName[128]; // log文件名
    int m_split_lines;   // 日志最大行数
    int m_log_buf_size;  // 日志缓冲区大小
    long long m_count;   // 日志行数记录
    int m_today;         // 因为按天分类，记录当前时间
    FILE *m_fp;          // 打开log的文件指针
    char *m_buf;
    CBlockQueue<string> *m_logQueue; // 阻塞队列
    bool m_isAsync;                  // 同步标志
    CLocker m_mutex;
};

// ...表示可变参数 __VA_ARGS__就是将...的值复制到此,##用于当可变参数个数为0时，把前面的逗号去掉，否则编译出错
// ##__VA_ARGS__宏使函数可以传递任意数量的参数
#define LOG_DEBUG(format, ...) Log::GetInstance()->WriteLog(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::GetInstance()->WriteLog(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::GetInstance()->WriteLog(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::GetInstance()->WriteLog(3, format, ##__VA_ARGS__)

#endif //!_LOG_H_