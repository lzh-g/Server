#ifndef _HTTPCONN_H_
#define _HTTPCONN_H_

#include "HttpStatusCode.h"
#include "stdafx.h"

#include <CGImysql/SqlConnPool.h>
#include <Locker/Locker.h>
#include <Log/Log.h>
#include <ThreadPool/Task.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

map<string, string> users;

static const int FILENAME_LEN = 200;       // 读取文件名称大小
static const int READ_BUFFER_SIZE = 2048;  // 读缓存
static const int WRITE_BUFFER_SIZE = 1024; // 写缓存

class HTTP_LIB HttpConn : public CTask
{
public:
public:
    HttpConn();
    virtual ~HttpConn();

public:
    // 初始化连接，外部调用初始化套接字地址
    void Init(int sockfd, const sockaddr_in &addr);
    // 关闭连接，连接数减一
    void CloseConn(bool read_close = true);
    // 任务执行
    void run();
    // 循环读取客户数据，直到无数据可读或对方关闭连接，非阻塞ET工作模式下，需要一次性将数据读完
    bool Read();
    // 写数据
    bool Write();
    // 获得地址
    sockaddr_in *GetAddress();

    void InitMysqlResult(ConnectionPool *connPool);

private:
    // 初始化新的连接，check_state默认为分析请求行的状态
    void Init();
    // 从m_readBuf读取，并处理请求报文
    HTTP_CODE ProcessRead();
    // 向m_writeBuf写入响应报文数据
    bool ProcessWrite(HTTP_CODE ret);
    // 主状态机解析报文中的请求行数据
    HTTP_CODE ParseRequestLine(char *text);
    // 解析http请求行，获得请求方法，目标url以及http版本号
    HTTP_CODE ParseHeaders(char *text);
    // 主状态机解析报文中的请求内容
    HTTP_CODE ParseContent(char *text);
    // 生成响应报文
    HTTP_CODE DoRequest();
    // 用于将指针向后偏移，指向未处理的字符
    char *GetLine();
    // 从状态机，用于分析该行内容，返回值为行的读取状态
    LINE_STATUS ParseLine();
    void Unmap();
    // 根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    bool AddResponse(const char *format, ...);
    // 添加文本content
    bool AddContent(const char *content);
    // 添加状态行
    bool AddStatusLine(int status, const char *title);
    // 添加消息报头，如文本长度、连接状态和空行
    bool AddHeaders(int content_length);
    // 添加文本类型
    bool AddContentType();
    // 添加报文长度
    bool AddContentLength(int content_length);
    // 添加连接装填，保持连接还是关闭
    bool AddLinger();
    // 添加空行
    bool AddBlankLine();

public:
    static int m_epollfd;
    static int m_userCount;
    MYSQL *m_mysql;

private:
    int m_sockfd;
    CLocker *m_locker;
    sockaddr_in m_address;
    // 存储读取的请求报文数据
    char m_readBuf[READ_BUFFER_SIZE];
    // 缓冲区中m_readBuf中数据的最后一个字节的下一个位置
    int m_readIdx;
    // m_readBuf读取的位置
    int m_checkedIdx;
    // m_readBuf中已解析的字符个数
    int m_startLine;
    // 存储发出的响应报文数据
    char m_writeBuf[WRITE_BUFFER_SIZE];
    // 指示buffer中的长度
    int m_writeIdx;
    // 主状态机的状态
    CHECK_STATE m_checkState;
    // 请求方法
    METHOD m_method;
    // 解析请求报文中对应的6个变量
    // 存储读取文件的名称
    char m_realFile[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_contentLength;
    bool m_linger; // 长连接标志

    char *m_fileAddress; // 读取服务器上的文件地址
    struct stat m_fileStat;
    struct iovec m_iv[2]; // io向量机制iovec
    int m_ivCount;
    int m_cgi;           // 是否启用的POST
    char *m_string;      // 存储请求头数据
    int m_bytes2Send;    // 剩余发送字节数
    int m_bytesHaveSend; // 已发送字节数
};

#endif //!_HTTPCONN_H_
