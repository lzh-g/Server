#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <signal.h>
#include <cassert>
#include <iostream>
#include <string.h>

#include <Timer/LstTimer.h>
#include <Http/HttpConn.h>
#include <CGImysql/SqlConnPool.h>
#include <Locker/Locker.h>
#include <Log/Log.h>
#include <ThreadPool/ThreadPool.h>

#define MAX_FD 65536           // 最大文件描述符
#define MAX_EVENT_NUMBER 10000 // 最大事件数
#define TIMESLOT 5             // 最小超时单位

#define SYNLOG // 同步写日志
// #define ASYNLOG//异步写日志

// #define listenfdET//边缘触发非阻塞
#define listenfdLT // 水平触发阻塞

// 改变链接属性
extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);
extern int setnonblocking(int fd);

// 设置定时器相关参数
static int pipefd[2];
static SortTimerLst timer_lst;
static int epollfd = 0;

// 信号处理函数
void sig_handler(int sig)
{
    // 为保证函数的可重入性，保留原来的errno
    // 可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;

    // 将信号值从管道写端写入，传输字符类型，而非整型
    send(pipefd[1], (char *)&msg, 1, 0);

    // 将原来的errno赋值为当前的errno
    errno = save_errno;
}

// 设置信号函数
void addsig(int sig, void(handler)(int), bool restart = true)
{
    // 创建sigaciton结构体变量
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));

    // 信号处理函数中仅发送信号量，不做对应逻辑处理
    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    // 将所有信号添加到信号集中
    sigfillset(&sa.sa_mask);

    // 执行sigaction函数
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定义处理任务，重新定时以不断触发SIGALRM信号
void timer_handler()
{
    timer_lst.Tick();
    alarm(TIMESLOT);
}

// 定时器回调函数
void cb_func(client_data *user_data)
{
    // 删除非活动连接在socket上的注册事件
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);

    // 关闭文件描述符
    close(user_data->sockfd);
    // 减少连接数
    HttpConn::m_userCount--;
    LOG_INFO("close fd %d", user_data->sockfd);
    Log::GetInstance()->flush();
}

void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char const *argv[])
{
#ifdef ASYNLOG
    // 异步日志模型
    Log::GetInstance()->Init("ServerLog", 2000, 800000, 8);
#endif

#ifdef SYNLOG
    // 同步日志模型
    Log::GetInstance()->Init("ServerLog", 2000, 800000, 0);
#endif

    if (argc <= 1)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    int port = atoi(argv[1]);

    addsig(SIGPIPE, SIG_IGN);

    // 创建数据库连接池
    ConnectionPool *connPool = ConnectionPool::GetInstance();
    connPool->Init("localhost", "root", "root", "123456", 3306, 8);

    // 创建线程池
    CThreadPool *pool = new CThreadPool();

    HttpConn *users = new HttpConn[MAX_FD];

    // 初始化数据库读取表
    users->InitMysqlResult(connPool);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    ret = listen(listenfd, 5);

    // 创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);

    addfd(epollfd, listenfd, false);
    HttpConn::m_epollfd = epollfd;

    // 创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);

    // 设置管道写端为非阻塞
    setnonblocking(pipefd[1]);
    // 设置管道读端为ET非阻塞
    addfd(epollfd, pipefd[0], false);

    // 传递给主循环的信号值，这里只关注SIGALRM和SIGTERM
    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);

    // 循环条件
    bool stop_server = false;

    client_data *users_timer = new client_data[MAX_FD];

    // 超时标志
    bool timeout = false;

    // 每隔TIMESLOT时间触发SIGALRM信号
    alarm(TIMESLOT);

    while (!stop_server)
    {
        // 检测发生事件的文件描述符
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        // 轮询文件描述符
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == listenfd)
            {
                // 初始化客户端连接地址
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);

#ifdef listenfdLT
                // 该连接分配的文件描述符
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (connfd < 0)
                {
                    LOG_ERROR("%s:errno is: %d", "accpet error", errno);
                    continue;
                }
                if (HttpConn::m_userCount >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    LOG_ERROR("%s", "Internal server busy");
                    continue;
                }
                users[connfd].Init(connfd, client_address);

                // 初始化client_data数据
                // 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
                // 初始化该连接对应的连接资源
                users_timer[connfd].address = client_address;
                users_timer[connfd].sockfd = connfd;
                // 创建定时器临时变量
                UtilTimer *timer = new UtilTimer;
                // 设置定时器对应的连接资源
                timer->m_userData = &users_timer[connfd];
                // 设置回调函数
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                // 设置绝对超时时间，15s后到期
                timer->m_expire = cur + 3 * TIMESLOT;
                // 创建该连接对应的定时器，初始化为前述临时变量
                users_timer[connfd].timer = timer;
                // 添加定时器到链表中
                timer_lst.AddTimer(timer);

#endif

#ifdef listenfdET
                while (1)
                {
                    int connfd = accept(listenfd, (struct sockaddr *)&client_address,
                                        &client_addrlength);
                    if (connfd < 0)
                    {
                        LOG_ERROR("%s:errno is:%d", "accept error", errno);
                        break;
                    }
                    if (HttpConn::m_user_count >= MAX_FD)
                    {
                        show_error(connfd, "Internal server busy");
                        LOG_ERROR("%s", "Internal server busy");
                        break;
                    }
                    users[connfd].init(connfd, client_address);

                    // 初始化client_data数据
                    // 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
                    users_timer[connfd].address = client_address;
                    users_timer[connfd].sockfd = connfd;
                    util_timer *timer = new util_timer;
                    timer->user_data = &users_timer[connfd];
                    timer->cb_func = cb_func;
                    time_t cur = time(NULL);
                    timer->expire = cur + 3 * TIMESLOT;
                    users_timer[connfd].timer = timer;
                    timer_lst.add_timer(timer);
                }
                continue;
#endif
            }
            // 处理异常事件
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务器端关闭连接，移除对应的定时器
                UtilTimer *timer = users_timer[sockfd].timer;
                timer->cb_func(&users_timer[sockfd]);

                if (timer)
                {
                    timer_lst.DelTimer(timer);
                }
            }
            // 处理定时器信号
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
            {
                int sig;
                char signals[1024];

                // 从管道读端读出信号值，成功返回字节数，失败返回-1
                // 正常情况下，ret返回值总为1，只有14和15两个ASCII码对应的字符
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0)
                {
                    continue;
                }
                else
                {
                    // 处理信号值对应的逻辑
                    for (int i = 0; i < ret; ++i)
                    {
                        switch (signals[i])
                        {
                        case SIGALRM:
                        {
                            timeout = true;
                            break;
                        }
                        case SIGTERM:
                        {
                            stop_server = true;
                        }
                        }
                    }
                }
            }
            // 处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                // 创建定时器临时变量，将该连接对应的定时器取出来
                UtilTimer *timer = users_timer[sockfd].timer;
                if (users[sockfd].Read())
                {
                    LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].GetAddress()->sin_addr));
                    Log::GetInstance()->flush();
                    // 若检测到读事件，将该事件放入请求队列
                    pool->AddTask(users + sockfd);

                    // 若有数据传输，则将定时器往后延迟3个单位
                    // 并对新的定时器在链表上的位置进行调整
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->m_expire = cur + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        Log::GetInstance()->flush();
                        timer_lst.AdjustTimer(timer);
                    }
                }
                else
                {
                    // 服务器端关闭连接，移除对应的定时器
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.DelTimer(timer);
                    }
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                UtilTimer *timer = users_timer[sockfd].timer;
                if (users[sockfd].Write())
                {
                    LOG_INFO("send data to the client(%s)",
                             inet_ntoa(users[sockfd].GetAddress()->sin_addr));
                    Log::GetInstance()->flush();

                    // 若有数据传输，则将定时器往后延迟3个单位
                    // 并对新的定时器在链表上的位置进行调整
                    if (timer)
                    {
                        time_t cur = time(NULL);
                        timer->m_expire = cur + 3 * TIMESLOT;
                        LOG_INFO("%s", "adjust timer once");
                        Log::GetInstance()->flush();
                        timer_lst.AdjustTimer(timer);
                    }
                }
                else
                {
                    // 服务器端关闭连接，移除对应的定时器
                    timer->cb_func(&users_timer[sockfd]);
                    if (timer)
                    {
                        timer_lst.DelTimer(timer);
                    }
                }
            }
        }
        // 处理定时器为非必须事件，收到信号无需立即处理
        // 完成读写事件后，再进行处理
        if (timeout)
        {
            timer_handler();
            timeout = false;
        }
    }
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete pool;

    return 0;
}
