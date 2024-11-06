#include "ThreadPool.h"

CThreadPool::CThreadPool()
{
    m_taskCnt = 0;
}

CThreadPool::~CThreadPool()
{
}

bool CThreadPool::Init(uint32_t threadNum)
{
    m_threadNum = threadNum;
    m_threads = new pthread_t[m_threadNum];

    if (threadNum <= 0)
    {
        return false;
    }

    for (size_t i = 0; i < m_threadNum; i++)
    {
        pthread_create(m_threads + i, NULL, worker, this);
    }

    return true;
}

void CThreadPool::AddTask(CTask *Task)
{
    m_locker.Lock();
    m_taskList.push_back(Task);
    m_locker.UnLock();
}

void CThreadPool::Destroy()
{
    if (m_threads)
    {
        delete[] m_threads;
    }
}

void *CThreadPool::worker(void *arg)
{
    CThreadPool *pool = (CThreadPool *)arg;
    pool->run();
    return pool;
}

void CThreadPool::run()
{
    while (true)
    {
        m_locker.Lock();
        while (m_taskList.empty())
        {
            m_locker.Wait();
        }

        CTask *Task = m_taskList.front();
        m_taskList.pop_front();
        m_locker.UnLock();

        Task->run();

        delete Task;
        Task = NULL;
        m_taskCnt++;
    }
}
