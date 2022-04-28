#include "../src/epoll.h"
#include "../src/http_conn.h"
#include "../src/ThreadPool.h"
#include "../src/wrap.h"

#include <sys/epoll.h>
#include <queue>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <signal.h>

using namespace std;

#define TIMER_TIME_OUT 500

extern pthread_mutex_t timelock;
extern struct epoll_event* events;

extern priority_queue<mytimer*, deque<mytimer*>, timerCmp> myTimerQueue;

const string PATH = "/";

int socket_bind_listen(int port)
{
    // 检查port值，取正确区间范围
    if (port < 1024 || port > 65535)
        return -1;

    int lfd = Socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    //端口复用
    int flag = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    int ret = Bind(lfd, (sockaddr* )&serv, sizeof(serv));

    ret = Listen(lfd, 1024);

    std::cout << "lfd create: " << lfd << std::endl;

    return lfd;
}

void myHandler(void *args)
{
    //cout << "do myHandler" << endl;
    http_conn *req_data = (http_conn*)args; //因为在mian函数一开始epoll事件结构体的event.data.ptr 项就是用requestData转换过去的 所以这里可以转换回来
    req_data->handleRequest();
}

void acceptConnection(int listen_fd, int epoll_fd, const string &path)
{
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_addr_len = 0;
    int accept_fd = 0;
    while((accept_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len)) > 0)
    {
        /*
        // TCP的保活机制默认是关闭的
        int optval = 0;
        socklen_t len_optval = 4;
        getsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
        cout << "optval ==" << optval << endl;
        */
        
        // 设为非阻塞模式
        int ret = setSocketNonBlocking(accept_fd);
        if (ret < 0)
        {
            perror("Set non block failed!");
            return;
        }

        //cout << "connection:" << accept_fd <<endl; 浏览器会连接十次

        http_conn *req_info = new http_conn(epoll_fd, accept_fd, path);

        // 文件描述符可以读，边缘触发(Edge Triggered)模式，保证一个socket连接在任一时刻只被一个线程处理
        __uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
        epoll_add(epoll_fd, accept_fd, static_cast<void*>(req_info), _epo_event);
       
        // 新增时间信息
        mytimer *mtimer = new mytimer(req_info, TIMER_TIME_OUT);
        req_info->addTimer(mtimer);

        pthread_mutex_lock(&timelock);
        myTimerQueue.push(mtimer);
        pthread_mutex_unlock(&timelock);
    }
    //if(accept_fd == -1)
     //   perror("accept");
}

// 分发处理函数
void handle_events(int epoll_fd, int listen_fd, struct epoll_event* events, int events_num, const string &path, ThreadPool* tp)
{
    for(int i = 0; i < events_num; i++)
    {
        // 获取有事件产生的描述符
        http_conn* request = (http_conn*)(events[i].data.ptr);
        int fd = request->getFd();

        // 有事件发生的描述符为监听描述符
        if(fd == listen_fd)
        {
            //cout << "This is listen_fd" << endl;
            acceptConnection(listen_fd, epoll_fd, path);
        }
        else
        {
            // 排除错误事件
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)
                || (!(events[i].events & EPOLLIN)))
            {
                printf("error event\n");
                delete request;
                continue;
            }

            // 将请求任务加入到线程池中
            // 加入线程池之前将mytimer和request分离(由于数据进入，之前的事件的定时器更新，更新方式就是先分离定时器堆所包含的http_con（之后会清理),再绑定新的定时器重新加入（handleRequest()中执行）)
            //timer里面有request指针成员  requsetData类里面有mytimer类成员)
            request->seperateTimer();//避免执行的时候检测到超时，从而删掉
            int rc = tp->threadpool_add(myHandler, events[i].data.ptr);//myHandler是对任务的处理函数   events[i].data.ptr是用户传过来的数据(报文) 作为任务处理函数的参数
        }
    }
}

void handle_expired_event()
{
    pthread_mutex_lock(&timelock);
    while (!myTimerQueue.empty())
    {
        //cout << "handle_expired_event" << endl;
        mytimer *ptimer_now = myTimerQueue.top();
        if (ptimer_now->isDeleted())
        {
            cout << "a" << endl;
            myTimerQueue.pop();
            delete ptimer_now;
        }
        else if (ptimer_now->isvalid() == false)
        {
            //cout << "b" << endl;
            myTimerQueue.pop();
            delete ptimer_now;
        }
        else
        {
            break;
        }
    }
    pthread_mutex_unlock(&timelock);
}

int main(int argc, const char* argv[]) {
    if(argc < 3)
    {
        printf("eg: ./a.out port path\n");
        exit(1);
    }

    // 采用指定的端口
    int port = atoi(argv[1]);

    // 修改进程工作目录, 方便后续操作
    int ret = chdir(argv[2]);
    if(ret == -1)
    {
        perror("chdir error");
        exit(1);
    }

    // 忽略SIGPIPE信号
    addsig(SIGPIPE,SIG_IGN);

    //初始化epoll事件表
    int epoll_fd = epoll_init();
    if (epoll_fd < 0)
    {
        perror("epoll init failed");
        return 1;
    }

    //初始化线程池
    ThreadPool* pool = new ThreadPool();
    pool->threadpool_create(4, MAX_THREADS , MAX_QUEUE);

    int lfd = socket_bind_listen(port);

    //设置成非阻塞
    if (setSocketNonBlocking(lfd) < 0)
    {
        perror("set socket non block failed");
        return 1;
    }

    /******将监听套接字纳入epoll的监管*******/
    __uint32_t event = EPOLLIN | EPOLLET;
    http_conn *req = new http_conn(); //req会存放用户传过来的数据同时里面也放了监听套接字描述符
    req->setFd(lfd);
    epoll_add(epoll_fd, lfd, static_cast<void*>(req), event);

    while(1) {
        /******就绪的事件放入events事件结构体数组*******/
        int events_num = my_epoll_wait(epoll_fd, events, MAXEVENTS, -1);

        if (events_num == 0)
            continue;
        printf("%d\n", events_num);

        handle_events(epoll_fd, lfd, events, events_num, PATH, pool);//epoll_fd表示epoll事件表的套接字  listenfd表示监听套接字的描述符  events_num表示就绪io套接字的数组 
        /******处理超时事件*******/
        handle_expired_event();
    }

    return 0;

}
