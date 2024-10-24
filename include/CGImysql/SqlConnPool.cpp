#include "SqlConnPool.h"

ConnectionPool::ConnectionPool()
{
    this->m_curConn = 0;
}

ConnectionPool::~ConnectionPool()
{
    Destroy();
}

ConnectionPool *ConnectionPool::GetInstance()
{
    static ConnectionPool connPool;
    return &connPool;
}

void ConnectionPool::Init(string Url, string User, string Password, string DataBaseName, int Port, uint32_t Maxconn)
{
    this->m_url = Url;
    this->m_port = Port;
    this->m_user = User;
    this->m_password = Password;
    this->m_databaseName = DataBaseName;

    m_locker.Lock();

    for (size_t i = 0; i < Maxconn; i++)
    {
        MYSQL *con = NULL;
        con = mysql_init(con);

        if (con == NULL)
        {
            cout << "Error" << mysql_error(con);
            exit(1);
        }
        m_connList.push_back(con);
    }

    this->m_maxConn = Maxconn;

    m_locker.UnLock();
}

MYSQL *ConnectionPool::GetConnection()
{
    MYSQL *con = NULL;

    if (0 == m_connList.size())
    {
        return NULL;
    }

    m_locker.Lock();

    con = m_connList.front();
    m_connList.pop_front();

    ++m_curConn;

    m_locker.UnLock();
    return con;
}

bool ConnectionPool::ReleaseConnection(MYSQL *conn)
{
    if (NULL == conn)
    {
        return false;
    }
    m_locker.Lock();

    m_connList.push_back(conn);
}

int ConnectionPool::GetFreeConn()
{
    return this->m_maxConn - m_connList.size();
}

void ConnectionPool::Destroy()
{
    m_locker.Lock();
    if (m_connList.size() > 0)
    {
        for (list<MYSQL *>::iterator it = m_connList.begin(); it != m_connList.end(); it++)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_curConn = 0;
        m_connList.clear();

        m_locker.UnLock();
    }

    m_locker.UnLock();
}

ConnectionPoolRAII::ConnectionPoolRAII(MYSQL **conn, ConnectionPool *connPool)
{
    *conn = connPool->GetConnection();
    m_connRAII = *conn;
    m_poolRAII = connPool;
}

ConnectionPoolRAII::~ConnectionPoolRAII()
{
    m_poolRAII->ReleaseConnection(m_connRAII);
}
