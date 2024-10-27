#ifndef LST_TIMER
#define LST_TIMER

#include "stdafx.h"

#include <time.h>
#include <netinet/in.h>

#include <Log/Log.h>

// 连接资源结构体成员需要用到定时器类
// 需要前向声明
class UtilTimer;

// 连接资源
struct client_data
{
    sockaddr_in address; // 客户端socket地址
    int sockfd;          // socket文件描述符
    UtilTimer *timer;    // 定时器
};

// 定时器类
class UtilTimer
{
public:
    UtilTimer() : m_prev(NULL), m_next(NULL) {}

public:
    time_t m_expire;                // 超时时间
    void (*cb_func)(client_data *); // 回调函数
    client_data *m_userData;        // 连接资源
    UtilTimer *m_prev;              // 前向定时器
    UtilTimer *m_next;              // 后继定时器
};

// 定时器容器类，为带头尾结点的升序双向链表，按超时时间升序
class SortTimerLst
{
public:
    SortTimerLst();

    // 常规销毁链表
    ~SortTimerLst();

    // 添加定时器，内部调用私有成员AddTimer
    void AddTimer(UtilTimer *timer);

    // 调整定时器，任务发生变化时，调整定时器在链表中的位置
    void AdjustTimer(UtilTimer *timer);

    // 删除定时器
    void DelTimer(UtilTimer *timer);

    // 定时任务处理函数
    void Tick();

private:
    // 私有成员函数，被公有成员AddTimer和AdjustTimer调用
    void AddTimer(UtilTimer *timer, UtilTimer *lst_head);

private:
    // 头尾节点
    UtilTimer *m_head;
    UtilTimer *m_tail;
};

#endif //! LST_TIMER