#ifndef _LOCKER_H_
#define _LOCKER_H_

#include <pthread.h>
#include <semaphore.h>

#include "stdafx.h"

class LOCKER_LIB CSem
{
public:
    CSem();

    CSem(int num);

    ~CSem();

    void wait();

    void post();

private:
    sem_t m_sem; // 信号量
};

class LOCKER_LIB CLocker
{

public:
    CLocker();
    ~CLocker();

    void Lock();

    void UnLock();

    void Wait();

    void TimeWait(timespec time);

    void Signal();

    void Broadcast();

    pthread_mutex_t *Get();

private:
    pthread_mutex_t m_mutex; // 线程互斥量
    // pthread_mutexattr_t m_mutexAttr; // 线程属性
    pthread_cond_t m_cond; // 线程条件变量
};

#endif //!_LOCKER_H_