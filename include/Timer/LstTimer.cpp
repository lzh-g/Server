#include "LstTimer.h"

SortTimerLst::SortTimerLst() : m_head(NULL), m_tail(NULL)
{
}

SortTimerLst::~SortTimerLst()
{
    UtilTimer *tmp = m_head;
    while (tmp)
    {
        m_head = tmp->m_next;
        delete tmp;
        tmp = m_head;
    }
}

void SortTimerLst::AddTimer(UtilTimer *timer)
{
    if (!timer)
    {
        return;
    }
    if (!m_head)
    {
        m_head = m_tail = timer;
        return;
    }

    // 如果新的定时器超时时间小于当前头部节点，直接将当前定时器节点作为头部节点
    if (timer->m_expire < m_head->m_expire)
    {
        timer->m_next = m_head;
        m_head->m_prev = timer;
        m_head = timer;
        return;
    }
    // 否则调用私有成员，调整内部节点
    AddTimer(timer, m_head);
}

void SortTimerLst::AdjustTimer(UtilTimer *timer)
{
    if (!timer)
    {
        return;
    }
    UtilTimer *tmp = timer->m_next;
    // 被调整的定时器在链表尾部
    // 定时器超时值仍然小于下一个定时器超时值，不调整
    if (!tmp || (timer->m_expire < tmp->m_expire))
    {
        return;
    }

    // 被调整定时器是链表头节点，将定时器取出，重新插入
    if (timer == m_head)
    {
        m_head = m_head->m_next;
        m_head->m_prev = NULL;
        timer->m_next = NULL;
        AddTimer(timer, m_head);
    }
    // 被调整定时器在内部，将定时器取出，重新插入
    else
    {
        timer->m_prev->m_next = timer->m_next;
        timer->m_next->m_prev = timer->m_prev;
        AddTimer(timer, timer->m_next);
    }
}

void SortTimerLst::DelTimer(UtilTimer *timer)
{
    if (!timer)
    {
        return;
    }
    // 链表中仅一个定时器
    if ((timer == m_head) && (timer == m_tail))
    {
        delete timer;
        m_head = NULL;
        m_tail = NULL;
        return;
    }
    // 删除定时器为头节点
    if (timer == m_head)
    {
        m_head = m_head->m_next;
        m_head->m_prev = NULL;
        delete timer;
        return;
    }
    // 删除定时器为尾节点
    if (timer == m_tail)
    {
        m_tail = m_tail->m_prev;
        m_tail->m_next = NULL;
        delete timer;
        return;
    }
    // 删除定时器在链表内部，常规链表节点删除
    timer->m_prev->m_next = timer->m_next;
    timer->m_next->m_prev = timer->m_prev;
    delete timer;
}

void SortTimerLst::Tick()
{
    if (!m_head)
    {
        return;
    }
    LOG_INFO("%s", "timer tick");
    Log::GetInstance()->flush();
    time_t cur = time(NULL);
    UtilTimer *tmp = m_head;
    while (tmp)
    {
        if (cur < tmp->m_expire)
        {
            break;
        }
        tmp->cb_func(tmp->m_userData);
        m_head = tmp->m_next;
        if (m_head)
        {
            m_head->m_prev = NULL;
        }
        delete tmp;
        tmp = m_head;
    }
}

void SortTimerLst::AddTimer(UtilTimer *timer, UtilTimer *lst_head)
{
    UtilTimer *prev = lst_head;
    UtilTimer *tmp = prev->m_next;
    // 遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入操作
    while (tmp)
    {
        if (timer->m_expire < tmp->m_expire)
        {
            prev->m_next = timer;
            timer->m_next = tmp;
            tmp->m_prev = timer;
            timer->m_prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->m_next;
    }
    // 遍历完发现，目标定时器需要放到尾结点处
    if (!tmp)
    {
        prev->m_next = timer;
        timer->m_prev = prev;
        timer->m_next = NULL;
        m_tail = timer;
    }
}
