#ifndef _BLOCK_QUEUE_H_
#define _BLOCK_QUEUE_H_

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <Locker/Locker.h>

using namespace std;

template <class T>
class CBlockQueue
{
public:
    CBlockQueue(int max_size = 1000);
    ~CBlockQueue();

    void clear();
    // 判断队列是否满
    bool full();
    // 判断队列是否为空
    bool empty();
    // 返回队首元素
    bool front(T &value);
    // 返回队尾元素
    bool back();
    int size();
    int max_size();
    /**
     * 往队列添加元素，需要将所有使用队列的线程先唤醒
     * 当有元素push进队列，相当于生产者生产了一个元素
     * 若无线程等待条件变量，则唤醒无意义
     */
    bool push(const T &item);
    // pop时，若队列无元素，则等待条件变量
    bool pop(T &item);
    // 增加超时处理，项目中没有使用到
    bool pop(T &item, int ms_timeout);

private:
    CLocker m_mutex;
    T *m_array;
    int m_size;
    int m_maxSize;
    int m_front;
    int m_back;
};

#endif