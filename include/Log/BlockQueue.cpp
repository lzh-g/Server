#include "BlockQueue.h"

template <class T>
inline CBlockQueue<T>::CBlockQueue(int max_size = 1000)
{
    if (max_size <= 0)
    {
        exit(-1);
    }
    m_maxSize = max_size;
    m_array = new T[max_size];
    m_size = 0;
    m_front = -1;
    m_back = -1;
}

template <class T>
CBlockQueue<T>::~CBlockQueue()
{
    m_mutex.Lock();
    if (m_array != NULL)
    {
        delete[] m_array;
    }
    m_mutex.UnLock();
}

template <class T>
void CBlockQueue<T>::clear()
{
    m_mutex.Lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.UnLock();
}

template <class T>
bool CBlockQueue<T>::full()
{
    m_mutex.Lock();
    if (m_size >= m_maxSize)
    {
        m_mutex.UnLock();
        return true;
    }
    m_mutex.UnLock();
    return false;
}

template <class T>
bool CBlockQueue<T>::empty()
{
    m_mutex.Lock();
    if (0 == m_size)
    {
        m_mutex.UnLock();
        return true;
    }
    m_mutex.UnLock();

    return false;
}

template <class T>
bool CBlockQueue<T>::front(T &value)
{
    m_mutex.Lock();
    if (0 == m_size)
    {
        m_mutex.UnLock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.UnLock();

    return true;
}

template <class T>
bool CBlockQueue<T>::back()
{
    m_mutex.Lock();
    if (0 == m_size)
    {
        m_mutex.UnLock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.UnLock();

    return true;
}

template <class T>
int CBlockQueue<T>::size()
{
    int tmp = 0;

    m_mutex.Lock();
    tmp = m_size;
    m_mutex.UnLock();

    return tmp;
}

template <class T>
int CBlockQueue<T>::max_size()
{
    int tmp = 0;

    m_mutex.Lock();
    tmp = m_maxSize;
    m_mutex.UnLock();

    return tmp;
}

template <class T>
bool CBlockQueue<T>::push(const T &item)
{
    m_mutex.Lock();
    if (m_size >= m_maxSize)
    {
        m_mutex.Broadcast();
        m_mutex.UnLock();

        return false;
    }

    m_back = (m_back + 1) % m_maxSize;
    m_array[m_back] = item;

    m_size++;

    m_mutex.Broadcast();
    m_mutex.UnLock();

    return true;
}

template <class T>
bool CBlockQueue<T>::pop(T &item)
{
    m_mutex.Lock();
    while (m_size <= 0)
    {
        if (!m_mutex.Wait(m_mutex.get()))
        {
            m_mutex.UnLock();
            return false;
        }

        m_front = (m_front + 1) % m_maxSize;
        item = m_array[m_front];
        m_size--;
        m_mutex.UnLock();
        return true;
    }
}

template <class T>
bool CBlockQueue<T>::pop(T &item, int ms_timeout)
{
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    m_mutex.Lock();
    if (m_size <= 0)
    {
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if (!m_mutex.TimeWait(m_mutex.get(), t))
        {
            m_mutex.UnLock();
            return false;
        }
    }

    if (m_size <= 0)
    {
        m_mutex.UnLock();
        return false;
    }

    m_front = (m_front + 1) % m_maxSize;
    item = m_array[m_front];
    m_size--;
    m_mutex.UnLock();
    return true;
}
