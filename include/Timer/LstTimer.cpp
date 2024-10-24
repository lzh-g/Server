#include "LstTimer.h"

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
    if (timer->m_expire < m_head->m_expire)
    {
        timer->m_next = m_head;
        m_head->m_prev = timer;
        m_head = timer;
        return;
    }
    AddTimer(timer, m_head);
}

void SortTimerLst::AdjustTimer(UtilTimer *timer)
{
    if (!timer)
    {
        return;
    }
    UtilTimer *tmp = timer->m_next;
    if (!tmp || (timer->m_expire < tmp->m_expire))
    {
        return;
    }
    if (timer == m_head)
    {
        m_head = m_head->m_next;
        m_head->m_prev = NULL;
        timer->m_next = NULL;
        AddTimer(timer, m_head);
    }
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
    if ((timer == m_head) && (timer == m_tail))
    {
        delete timer;
        m_head = NULL;
        m_tail = NULL;
        return;
    }
    if (timer == m_head)
    {
        m_head = m_head->m_next;
        m_head->m_prev = NULL;
        delete timer;
        return;
    }
    if (timer == m_tail)
    {
        m_tail = m_tail->m_prev;
        m_tail->m_next = NULL;
        delete timer;
        return;
    }
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
    if (!tmp)
    {
        prev->m_next = timer;
        timer->m_prev = prev;
        timer->m_next = NULL;
        m_tail = timer;
    }
}
