#ifndef _SQLCONNPOOL_H_
#define _SQLCONNPOOL_H_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <Locker/Locker.h>
#include "stdafx.h"

using namespace std;

class CGIMYSQL_LIB ConnectionPool
{
public:
    ConnectionPool();
    ~ConnectionPool();

    // 单例模式
    static ConnectionPool *GetInstance();

    void Init(string Url, string User, string Password, string DataBaseName, int Port, uint32_t Maxconn);
    MYSQL *GetConnection();              // 获取数据库连接
    bool ReleaseConnection(MYSQL *conn); // 释放连接
    int GetFreeConn();                   // 获取空闲连接
    void Destroy();                      // 销毁连接池

private:
    uint32_t m_maxConn; // 最大连接数
    uint32_t m_curConn; // 当前连接数

    CLocker m_locker;
    list<MYSQL *> m_connList; // 连接池

    string m_url;          // 主机地址
    string m_port;         // 数据库端口号
    string m_user;         // 登录数据库用户名
    string m_password;     // 登录数据库密码
    string m_databaseName; // 使用数据库名
};

class CGIMYSQL_LIB ConnectionPoolRAII
{
public:
    ConnectionPoolRAII(MYSQL **conn, ConnectionPool *connPool);
    ~ConnectionPoolRAII();

private:
    MYSQL *m_connRAII;
    ConnectionPool *m_poolRAII;
};

#endif //!_SQLCONNPOOL_H_