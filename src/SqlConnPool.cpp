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

void ConnectionPool::Init(string Url, string User, string Password, string DataBaseName, int Port, uint32_t MaxConn)
{
    // 初始化数据库信息
    this->m_url = Url;
    this->m_port = Port;
    this->m_user = User;
    this->m_password = Password;
    this->m_databaseName = DataBaseName;

    m_locker.Lock();

    // 创建Maxconn条数据库连接
    for (size_t i = 0; i < MaxConn; i++)
    {
        MYSQL *con = NULL;
        con = mysql_init(con);

        if (con == NULL)
        {
            cout << "Error" << mysql_error(con);
            exit(1);
        }
        // 更新连接池和空闲连接数量
        m_connList.push_back(con);
        ++m_freeConn;
    }

    m_reserve = CSem(m_freeConn);

    this->m_maxConn = MaxConn;

    m_locker.UnLock();
}

MYSQL *ConnectionPool::GetConnection()
{
    MYSQL *con = NULL;

    if (0 == m_connList.size())
    {
        return NULL;
    }
    // 取出连接，信号量原子减一，为0则等待
    m_reserve.wait();

    m_locker.Lock();

    con = m_connList.front();
    m_connList.pop_front();

    // 无用变量
    --m_freeConn;
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

    ++m_freeConn;
    --m_curConn;

    m_locker.UnLock();

    m_reserve.post();
    return true;
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
