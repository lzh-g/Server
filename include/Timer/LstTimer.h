#ifndef LST_TIMER
#define LST_TIMER

#include "stdafx.h"

#include <time.h>
#include <netinet/in.h>

#include <Log/Log.h>

class UtilTimer;
struct client_data
{
    sockaddr_in address;
    int sockfd;
    UtilTimer *timer;
};

class UtilTimer
{
public:
    UtilTimer() : m_prev(NULL), m_next(NULL) {}

public:
    time_t m_expire;
    void (*cb_func)(client_data *);
    client_data *m_userData;
    UtilTimer *m_prev;
    UtilTimer *m_next;
};

class SortTimerLst
{
public:
    SortTimerLst() : m_head(NULL), m_tail(NULL) {}
    ~SortTimerLst();

    void AddTimer(UtilTimer *timer);

    void AdjustTimer(UtilTimer *timer);
    void DelTimer(UtilTimer *timer);
    void Tick();

private:
    void AddTimer(UtilTimer *timer, UtilTimer *lst_head);

private:
    UtilTimer *m_head;
    UtilTimer *m_tail;
};

#endif //! LST_TIMER