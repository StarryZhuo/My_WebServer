#ifndef __EPOLL_THREADPOOL_H__
#define __EPOLL_THREADPOOL_H__

#include "ThreadPool.h"
#include "sys/epoll.h"

#define MAXSIZE 2000

class Epoll_threadpool {
public:
    Epoll_threadpool(int _port):port(_port){

    }
    //　socket创建，绑定，监听，epoll根节点的创建
    int init_Epoll(); 

    //新的socket, 并挂载到红黑树上
    static int Addfd(int sockfd, bool enable_et);

    //处理就绪事件
    static int Epoll_Deal();

    //循环监听
    static void listen_work(void *args);

    //监听事件调用
    static void do_accept(void *args);

    //读事件调用
    static void do_read(void *args);

    //断开连接的函数
    static void disconnect(int cfd);

private:
    int port = 0;     //端口号
    static int lfd;      //监听描述符
    static ThreadPool pool;
    static int epfd;     //epoll句柄
    static int work_socket;

};




#endif