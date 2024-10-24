#include "HttpConn.h"

// 将表中用户名与密码放入map
map<string, string> users;
CLocker m_lock;

HttpConn::HttpConn()
{
}

HttpConn::~HttpConn()
{
}

void HttpConn::Init()
{
    m_mysql = NULL;
    m_bytes2Send = 0;
    m_bytesHaveSend = 0;
    m_checkState = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_contentLength = 0;
    m_host = 0;
    m_startLine = 0;
    m_checkedIdx = 0;
    m_readIdx = 0;
    m_writeIdx = 0;
    m_cgi = 0;
    memset(m_readBuf, '\0', READ_BUFFER_SIZE);
    memset(m_writeBuf, '\0', WRITE_BUFFER_SIZE);
    memset(m_readBuf, '\0', FILENAME_LEN);
}

void HttpConn::Init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true);
    m_userCount++;
    Init();
}

void HttpConn::CloseConn(bool real_close)
{
    if (real_close && (m_sockfd != 01))
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_userCount--;
    }
}

void HttpConn::run()
{
    HTTP_CODE read_ret = ProcessRead();

    // NO_REQUEST，表示请求不完整，需要继续接收请求数据
    if (read_ret == NO_REQUEST)
    {
        // 注册并监听读事件
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    // 调用processWrite完成报文响应
    bool write_ret = ProcessWrite(read_ret);
    if (!write_ret)
    {
        CloseConn();
    }
    // 注册并监听写事件
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

bool HttpConn::Read()
{
    if (m_readIdx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

#ifdef CONNFD_FT

    bytes_read = recv(m_sockfd, m_readBuf + m_readIdx, READ_BUFFER_SIZE - m_readIdx, 0);

    if (bytes_read <= 0)
    {
        return false;
    }

    m_readIdx += bytes_read;

    return true;
#endif

#ifdef CONNFD_ET
    while (true)
    {
        // 从套接字接受数据，存储在m_readBuf缓冲区
        bytes_read = recv(m_sockfd, m_readBuf + m_readIdx, READ_BUFFER_SIZE - m_readIdx, 0);
        if (bytes_read == -1)
        {
            // 非阻塞ET模式下，需要一次性将数据读完
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        // 修改m_readIdx的读取字节数
        m_readIdx += bytes_read;
    }
    return true;

#endif
}

bool HttpConn::Write()
{
    int temp = 0;

    int newadd = 0;

    // 若要发送的数据长度为0，表示响应报文为空，一般不会出现这种情况
    if (m_bytes2Send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        Init();
        return true;
    }

    while (1)
    {
        // 将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
        temp = writev(m_sockfd, m_iv, m_ivCount);

        // 正常发送，temp为发送字节
        if (temp > 0)
        {
            // 更新已发送字节
            m_bytesHaveSend += temp;
            // 偏移文件iovec指针
            newadd = m_bytesHaveSend - m_writeIdx;
        }
        if (temp <= -1)
        {
            // 判断缓冲区是否满
            if (errno == EAGAIN)
            {
                // 第一个iovec头部信息数据已发送完，发送第二个iovec数据
                if (m_bytesHaveSend >= m_iv[0].iov_len)
                {
                    // 不在继续发送头部信息
                    m_iv[0].iov_len = 0;
                    m_iv[1].iov_base = m_fileAddress + newadd;
                    m_iv[1].iov_len = m_bytes2Send;
                }
                // 继续发送第一个iovec头部信息数据
                else
                {
                    m_iv[0].iov_base = m_writeBuf + m_bytes2Send;
                    m_iv[0].iov_len = m_iv[0].iov_len - m_bytesHaveSend;
                }
                // 重新注册写事件
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            // 发送失败，但不是缓冲区问题，则取消映射
            Unmap();
            return false;
        }
        // 更新已发送字节数
        m_bytes2Send -= temp;

        // 判断数据是否发送完
        if (m_bytes2Send <= 0)
        {
            Unmap();

            // 在epoll树上充值EPOLLONESHOT事件
            modfd(m_epollfd, m_sockfd, EPOLLIN);

            // 浏览器请求为长连接
            if (m_linger)
            {
                Init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

sockaddr_in *HttpConn::GetAddress()
{
    return &m_address;
}

void HttpConn::InitMysqlResult(ConnectionPool *connPool)
{
    // 从连接池中去一个连接
    MYSQL *mysql = NULL;
    ConnectionPoolRAII mysqlCon(&mysql, connPool);

    // 在user表中检索username，passwd
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    // 从表中检索完成的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    // 返回结果集的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_field(result);

    // 将结果存入map
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

HTTP_CODE HttpConn::ProcessRead()
{
    // 初始化从状态机状态，HTTP请求解析结果
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_checkState == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = ParseLine()) == LINE_OK))
    {
        text = GetLine();

        // m_start_line是每一个数据行在m_read_buf中的起始位置
        // m_check_idx表示从状态机在m_read_buf中读取的位置
        m_startLine = m_checkedIdx;

        switch (m_checkState)
        {
        case CHECK_STATE_REQUESTLINE:
            // 请求解析行
            ret = ParseRequestLine(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        case CHECK_STATE_HEADER:
            // 解析请求头
            ret = ParseHeaders(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            // 完整解析GET请求后，跳转到报文响应函数
            else if (ret = GET_REQUEST)
            {
                return DoRequest();
            }

            break;
        case CHECK_STATE_CONTENT:

            // 解析消息体
            ret = ParseContent(text);
            // 完成解析POST请求后，跳转到报文响应函数
            if (ret == GET_REQUEST)
            {
                return DoRequest();
            }
            // 解析完消息体即完成报文解析，避免再次进入循环，更新line_status
            line_status = LINE_OPEN;
            break;

        default:
            return INTERNAL_ERROR;
            break;
        }
    }
    return NO_REQUEST;
}

bool HttpConn::ProcessWrite(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
        AddStatusLine(500, error_500_title);
        AddHeaders(strlen(error_500_form));
        if (!AddContent(error_500_form))
        {
            return false;
        }
        break;
    case BAD_REQUEST:
        AddStatusLine(404, error_404_title);
        AddHeaders(strlen(error_404_form));
        if (!AddContent(error_404_form))
        {
            return false;
        }
        break;
    case FORBIDDEN_REQUEST:
        AddStatusLine(403, error_403_title);
        AddHeaders(strlen(error_403_form));
        if (!AddContent(error_403_form))
        {
            return false;
        }
        break;

    case FILE_REQUEST:
        AddStatusLine(200, ok_200_title);
        // 若请求资源存在
        if (m_fileStat.st_size != 0)
        {
            AddHeaders(m_fileStat.st_size);
            // 第一个iovec指针指向响应报文缓冲区，长度指向m_writeIdx
            m_iv[0].iov_base = m_writeBuf;
            m_iv[0].iov_len = m_writeIdx;
            // 第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
            m_iv[1].iov_base = m_fileAddress;
            m_iv[1].iov_len = m_fileStat.st_size;
            // 发送的全部数据为响应报文头部信息和文件大小
            m_bytes2Send = m_writeIdx + m_fileStat.st_size;
            return true;
        }
        else
        {
            // 请求资源为0，返回空白html文件
            const char *ok_string = "<html><body></body></html>";
            AddHeaders(strlen(ok_string));
            if (!AddContent(ok_string))
            {
                return false;
            }
        }

    default:
        return false;
        break;
    }
    // 除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
    m_iv[0].iov_base = m_writeBuf;
    m_iv[0].iov_len = m_writeIdx;
    m_ivCount = 1;
    return true;
}

HTTP_CODE HttpConn::ParseRequestLine(char *text)
{
    // 找到请求行中含有空格和\t的位置并返回
    m_url = strpbrk(text, " \t");
    // 没找到，则报文格式错误
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    // 将该位置改为\0，便于将前面数据取出
    *m_url++ = '\0';
    // 去除数据，比较GET和POST，确定请求方式
    char *method = text;
    // strcasecmp 忽略字符大小写进行比较
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        m_cgi = 1;
    }
    else
    {
        return BAD_REQUEST;
    }
    // 查找空格和\t字符，跳过，指向请求资源的的一个字符
    m_url += strspn(m_url, " \t");

    // 判断HTTP版本号
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    // 仅支持HTTP/1.1
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }
    // 对请求资源的前7个字符进行判断
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }
    // 当url为/时，显示判断界面
    if (strlen(m_url) == 1)
    {
        strcat(m_url, "judge.html");
    }
    m_checkState = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTP_CODE HttpConn::ParseHeaders(char *text)
{
    // 判断是空行还是请求头
    if (text[0] == '\0')
    {
        // 判断是GET还是POST请求
        if (m_contentLength != 0)
        {
            // POST需要跳转到消息体处理状态
            m_checkState = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    // 解析请求头部连接字段
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;

        // 跳过空格和\t字符
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            // 如果是长连接，则将linger标志设置为true
            m_linger = true;
        }
    }
    // 解析请求头部内容长度字段
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_contentLength = atol(text);
    }
    // 解析请求头部HOST字段
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        printf("Unknow header:%s\n", text);
    }
    return NO_REQUEST;
}

// 判断http请求是否被完整读入
HTTP_CODE HttpConn::ParseContent(char *text)
{
    // 判断buffer中是否读取了消息体
    if (m_readIdx >= (m_contentLength + m_checkedIdx))
    {
        text[m_contentLength] = '\0';

        // POST请求中最后为输入的用户名和密码
        m_string = text;

        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HTTP_CODE HttpConn::DoRequest()
{
    // 将初始化的m_read_file赋值为网站根目录
    strcpy(m_realFile, doc_root);
    int len = strlen(doc_root);

    // 找到m_url中的/位置
    const char *p = strrchr(m_url, '/');

    // 实现登录和注册校验
    if (m_cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {
        // 根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_realFile + len, m_url_real, FILENAME_LEN - len + 1);
        free(m_url_real);

        // 将用户名和密码提取出来
        // user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
        {
            name[i - 5] = m_string[i];
        }
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        // 同步线程登录校验
        if (*(p + 1) == '3')
        {
            // 如果是注册，先检测数据库中是否有重名的
            // 没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {

                m_lock.Lock();
                int res = mysql_query(m_mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.UnLock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        // 如果是登录，直接判断
        // 若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
        // CGI多进程登录校验
    }

    // 如果请求资源为/0，表示跳转注册界面
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");

        // 将网站目录和/log.html进行拼接，更新到m_realFile中
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // 请求资源为/1，表示跳转登录界面
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");

        // 将网站目录和/log.html进行拼接，更新到m_realFile中
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");

        // 将网站目录和/log.html进行拼接，更新到m_realFile中
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");

        // 将网站目录和/log.html进行拼接，更新到m_realFile中
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");

        // 将网站目录和/log.html进行拼接，更新到m_realFile中
        strncpy(m_realFile + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
    {
        // 以上均不符合，直接将url与网站目录拼接
        strncpy(m_realFile + len, m_url, FILENAME_LEN - len - 1);
    }

    // 通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
    // 失败返回NO_RESOURCE状态，表示资源不存在
    if (stat(m_realFile, &m_fileStat) < 0)
        return NO_RESOURCE;

    // 判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if (!(m_fileStat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    // 判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
    if (S_ISDIR(m_fileStat.st_mode))
        return BAD_REQUEST;

    // 以只读方式获取文件描述符，通过mmap将该文件映射到内存中
    int fd = open(m_realFile, O_RDONLY);
    m_fileAddress = (char *)mmap(0, m_fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    // 避免文件描述符的浪费和占用
    close(fd);

    // 表示请求文件存在，且可以访问
    return FILE_REQUEST;
}

// m_startLine是行在buffer中的起始位置，将该位置后面的数据赋给text
// 此时从状态机已提前将一行的末尾字符\r\n变为\0\0，所以text可以直接读取完成的行进行解析
char *HttpConn::GetLine()
{
    return m_readBuf + m_startLine;
}

// 从状态机，用于分析一行的内容
// 返回值为行的读取状态，有LINE_OK，LINE_BAD，LINE_OPEN

// m_readIdx指向缓冲区m_read_buf的数据末尾的下一个字符
// m_checkedIdx指向从状态机当前分析的字符
LINE_STATUS HttpConn::ParseLine()
{
    char temp;
    for (; m_checkedIdx < m_readIdx; ++m_checkedIdx)
    {
        // temp为当前分析的字符
        temp = m_readBuf[m_checkedIdx];
        // 读到\r，表示可能读到完整行
        if (temp == '\r')
        {
            // 下一个字符为buffer结尾，则接收不完成，继续接收
            if ((m_checkedIdx + 1) == m_readIdx)
            {
                return LINE_OPEN;
            }
            // 下一个字符为\n，表示读取到了完整行，将\r\n改为\0\0
            else if (m_readBuf[m_checkedIdx + 1] == '\n')
            {
                m_readBuf[m_checkedIdx++] = '\0';
                m_readBuf[m_checkedIdx++] = '\0';
                return LINE_OK;
            }
            // 读取不合规，则返回错误
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checkedIdx > 1 && m_readBuf[m_checkedIdx - 1] == '\r')
            {
                m_readBuf[m_checkedIdx - 1] = '\0';
                m_readBuf[m_checkedIdx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    // 没找到\r\n，继续接收
    return LINE_OPEN;
}

void HttpConn::Unmap()
{
    if (m_fileAddress)
    {
        munmap(m_fileAddress, m_fileStat.st_size);
        m_fileAddress = 0;
    }
}

bool HttpConn::AddResponse(const char *format, ...)
{
    // 如果写入内容超出m_writeBuf大小则报错
    if (m_writeIdx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }

    // 定义可变参数列表
    va_list arg_list;

    // 将变量arg_list初始化为传入参数
    va_start(arg_list, format);

    // 将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
    int len = vsnprintf(m_writeBuf + m_writeIdx, WRITE_BUFFER_SIZE - 1 - m_writeIdx, format, arg_list);

    // 如果写入的数据长度超过缓冲区剩余空间，则报错
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_writeIdx))
    {
        va_end(arg_list);
        return false;
    }

    // 更新m_writeIdx位置
    m_writeIdx += len;
    // 清空可变参数列表
    va_end(arg_list);

    return true;
}

bool HttpConn::AddContent(const char *content)
{
    return AddResponse("%s", content);
}

bool HttpConn::AddStatusLine(int status, const char *title)
{
    return AddResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConn::AddHeaders(int content_length)
{
    AddContentLength(content_length);
    AddLinger();
    AddBlankLine();
}

bool HttpConn::AddContentType()
{
    return AddResponse("Content-Type:%s\r\n", "text/html");
}

bool HttpConn::AddContentLength(int content_length)
{
    return AddResponse("Content-Length:%d\r\n", content_length);
}

bool HttpConn::AddLinger()
{
    return AddResponse("Connection:%s\r\n", (m_linger == true) ? "keep-alive:" : "close");
}

bool HttpConn::AddBlankLine()
{
    return AddResponse("%s", "\r\n");
}

// -------------------------------------------
// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;

// EPOLL 数据可读
// EPOLLET 边缘触发
// EPOLLRDHUP TCP连接被对方关闭，或对方关闭写操作
#ifdef CONNFD_ET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef CONNFD_LT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef LISTENFD_ET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

    if (one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 从内核事件表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int eopllfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(eopllfd, EPOLL_CTL_MOD, fd, &event);
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}