#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include "stdafx.h"
#include <list>
#include <pthread.h>
#include <stdint.h>

#include "../Locker/Locker.h"
#include "Task.h"

class THREADPOOL_LIB CThreadPool
{
public:
    CThreadPool();
    ~CThreadPool();

    bool Init(uint32_t threadNum);
    void AddTask(CTask *Task);
    void Destroy();

private:
    static void *worker(void *arg);
    void run();

private:
    pthread_t *m_threads;          // 线程池中的线程
    uint32_t m_taskCnt;            // 任务计数
    uint32_t m_threadNum;          // 线程数
    CLocker m_locker;              // 互斥锁
    std::list<CTask *> m_taskList; // 任务列表
};

#endif //!_THREADPOOL_H_
