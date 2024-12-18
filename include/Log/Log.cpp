#include "Log.h"
#include <string.h>

Log *Log::GetInstance()
{
    static Log instance;
    return &instance;
}

Log::Log()
{
    m_count = 0;
    m_isAsync = false;
}

Log::~Log()
{
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
}

void *Log::AsyncWriteLog()
{
    string single_log;
    // 从阻塞队列中去除一个日志string，写入文件
    while (m_logQueue->pop(single_log))
    {
        m_mutex.Lock();
        fputs(single_log.c_str(), m_fp);
        m_mutex.UnLock();
    }
}

void *Log::FlushLogThread(void *args)
{
    Log::GetInstance()->AsyncWriteLog();
}

bool Log::Init(const char *file_name, int log_buf_size, int split_lines, int max_queue_size)
{
    // 若设置了max_queue_size,则设置为异步
    if (max_queue_size >= 1)
    {
        // 设置写入方式flag
        m_isAsync = true;
        m_logQueue = new CBlockQueue<string>(max_queue_size);

        pthread_t tid;
        // FlushLogThread为回调函数，这里表示创建线程异步写日志
        pthread_create(&tid, NULL, FlushLogThread, NULL);
    }

    // 输出内容的长度
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    // 日志最大行数
    m_split_lines = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 从后往前找第一个/的位置
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    // 相当于自定义日志名
    // 若输入的文件名没有/，则直接将时间+文件名作为日志名
    if (p == NULL)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        // 将/的位置向后移动要给位置，然后复制到logname中
        strcpy(m_logName, p + 1);
        strncpy(m_dirName, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%S%d_%02d_%02d_%s", m_dirName, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m_logName);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");
    if (m_fp == NULL)
    {
        return false;
    }

    return true;
}

void Log::WriteLog(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    // 日志分级
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;

    case 1:
        strcpy(s, "[info]:");
        break;

    case 2:
        strcpy(s, "[warn]:");
        break;

    case 3:
        strcpy(s, "[error]:");
        break;

    default:
        strcpy(s, "[info]:");
        break;
    }

    // 写入一次日志
    m_mutex.Lock();
    m_count++;

    // 日志不是今天或写入的日志行数是最大行的倍数
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
    {
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        // 格式化日志名中的时间部分
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        // 如果时间不是今天，则创建今天的日志，更新m_today和m_count
        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", m_dirName, tail, m_logName);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            // 超过了最大行，在之前的日志名基础上加后缀，m_count/m_split_lines
            snprintf(new_log, 255, "%s%s%s.%lld", m_dirName, tail, m_logName, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.UnLock();

    va_list valst;
    // 将传入的format参数赋值给valst，便于格式化输出
    va_start(valst, format);

    string log_str;
    m_mutex.Lock();

    // 写入具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // vsnprintf将可变参数格式化输出到一个字符数组
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.UnLock();

    // 异步且队列未满
    if (m_isAsync && !m_logQueue->full())
    {
        m_logQueue->push(log_str);
    }
    else
    {
        m_mutex.Lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.UnLock();
    }

    va_end(valst);
}

void Log::flush(void)
{
    m_mutex.Lock();
    // 强制刷新写入缓冲区
    fflush(m_fp);
    m_mutex.UnLock();
}
