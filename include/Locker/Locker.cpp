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

CLocker::CLocker()
{
}

CLocker::~CLocker()
{
}