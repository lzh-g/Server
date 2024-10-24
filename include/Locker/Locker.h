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

    void wait() { sem_wait(&m_sem); }

    void post() { sem_post(&m_sem); }

private:
    sem_t m_sem; // 信号量
};

class LOCKER_LIB CLocker
{

public:
    CLocker();
    ~CLocker();

    void Lock() { pthread_mutex_lock(&m_mutex); }

    void UnLock() { pthread_mutex_unlock(&m_mutex); }

    void Wait() { pthread_cond_wait(&m_cond, &m_mutex); }

    void TimeWait(timespec time) { pthread_cond_timedwait(&m_cond, &m_mutex, &time); }

    void Signal() { pthread_cond_signal(&m_cond); }

    void Broadcast() { pthread_cond_broadcast(&m_cond); }

private:
    pthread_mutex_t m_mutex; // 线程互斥量
    // pthread_mutexattr_t m_mutexAttr; // 线程属性
    pthread_cond_t m_cond; // 线程条件变量
};

#endif //!_LOCKER_H_