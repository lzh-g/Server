#include "Locker.h"

CSem::CSem()
{
    sem_init(&m_sem, 0, 0);
}

CSem::CSem(int num)
{
    sem_init(&m_sem, 0, num);
}

CSem::~CSem()
{
    sem_destroy(&m_sem);
}

void CSem::wait()
{
    sem_wait(&m_sem);
}

void CSem::post()
{
    sem_post(&m_sem);
}

CLocker::CLocker()
{
    pthread_mutex_init(&m_mutex, NULL);
}

CLocker::~CLocker()
{
    pthread_mutex_destroy(&m_mutex);
}

void CLocker::Lock()
{
    pthread_mutex_lock(&m_mutex);
}

void CLocker::UnLock()
{
    pthread_mutex_unlock(&m_mutex);
}

void CLocker::Wait()
{
    pthread_cond_wait(&m_cond, &m_mutex);
}

void CLocker::TimeWait(timespec time)
{
    pthread_cond_timedwait(&m_cond, &m_mutex, &time);
}

void CLocker::Signal()
{
    pthread_cond_signal(&m_cond);
}

void CLocker::Broadcast()
{
    pthread_cond_broadcast(&m_cond);
}

pthread_mutex_t *CLocker::Get()
{
    return &m_mutex;
}
