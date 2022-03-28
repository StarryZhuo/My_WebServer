#include "epoll_threadpool.h"
#include "wrap.h"
#include "ThreadPool.h"
#include "httpdeal.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <memory.h>
#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <arpa/inet.h>
#include <fcntl.h>


int Epoll_threadpool::lfd = 0;      //监听描述符
ThreadPool Epoll_threadpool::pool;
int Epoll_threadpool::epfd = 0;     //epoll句柄
int Epoll_threadpool::work_socket = 0;
// epoll_event Epoll_threadpool::allset[MAXSIZE];


//　socket创建，绑定，监听，epoll根节点的创建
int Epoll_threadpool::init_Epoll() {

    //创造线程池
    if(pool.threadpool_create(10, 200, 50) == -1) {
        perror("create ThreadPool Failed");
        exit(1);
    }

    //socket创建
    lfd = Socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    //端口复用
    int flag = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    int ret = Bind(lfd, (sockaddr* )&serv, sizeof(serv));

    ret = Listen(lfd, 64);

    std::cout << "lfd create: " << lfd << std::endl;

    //创建epoll的根
    epfd = epoll_create(MAXSIZE);
    if(epfd == -1) {
        perr_exit("epoll_create error");
    }

    //将监听lfd挂载
    if(Addfd(lfd, true) == -1) {
        perr_exit("epoll_ctl add lfd error");
    }
    
    int a = pool.threadpool_add(listen_work, (void *)(0));

    return lfd;
}

int Epoll_threadpool::Addfd(int sockfd, bool enable_et) {
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    if(enable_et) {
        ev.events |= EPOLLET;
    }
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        return -1;
    }
    return 0;
}

void Epoll_threadpool::listen_work(void *args) {
    std::cout << "start work" << std::endl;
    while(1) {
        // sleep(10);
        epoll_event allset[MAXSIZE];
        work_socket = epoll_wait(epfd, allset, MAXSIZE, 0);
        if(work_socket == -1) {
            perr_exit("epoll_wait error");
        }
        // std::cout << "work_socket: " << work_socket << std::endl;
        // work_socket = 1;
        for(int i = 0; i < work_socket; i++) {
            // std::cout << "i: " << i << std::endl;
            epoll_event *pev = &allset[i];
            // 不是读事件
            if(!(pev->events & EPOLLIN)) {
                continue;
            }
            std::cout << "pev->data.fd: " << pev->data.fd << std::endl;
            if(pev->data.fd == lfd) {
                std::cout << " create new " << std::endl;
                pool.threadpool_add(do_accept, (void *)(0));
            } else {
                std::cout << "begin to read"  << std::endl;
                int ccfd = pev->data.fd;
                pool.threadpool_add(do_read, &ccfd);
                std::cout << "read end" << std::endl;
            }
        }
        // bzero(allset,sizeof(allset));
    }
    return;
}



// 接受新连接处理
void Epoll_threadpool::do_accept(void *args) {
    sockaddr_in client;
    socklen_t len = sizeof(client);
    int cfd = Accept(lfd, (struct sockaddr*)&client, &len);

    // 打印客户端信息
    char ip[64] = {0};
    printf("New Client IP: %s, Port: %d, cfd = %d\n",
           inet_ntop(AF_INET, &client.sin_addr.s_addr, ip, sizeof(ip)),
           ntohs(client.sin_port), cfd);

    //设置cfd为非阻塞
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    Addfd(cfd, true);
    return;

}

void Epoll_threadpool::disconnect(int cfd) {
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if(ret == -1) {
        perror("epoll_ctl_del cfd error");
        exit(1);
    }
    Close(cfd);
    return;
}

//http协议的解析
void Epoll_threadpool::do_read(void *args) {
    // epoll_event newEv = *(epoll_event *)(args);
    int cfd = *(int *)args;
    int efdtmp = epfd;
    // std::cout << "newEv->data.fd: " << cfd << std::endl;
    // char line[1024] = {0};

    // int a = Read(cfd, line, sizeof(line));
    // Write(STDOUT_FILENO, line, a);

    // 将浏览器发过来的数据, 读到buf中 
    char line[1024] = {0};
    // 读请求行
    int len = get_line(cfd, line, sizeof(line));
    if(len == 0) {   
        printf("客户端断开了连接...\n");
        // 关闭套接字, cfd从epoll上del
        disconnect(cfd);         
    } else { 
    	printf("============= 请求头 ============\n");   
        printf("请求行数据: %s", line);
        // 还有数据没读完,继续读走
		while (1) {
			char buf[1024] = {0};
			len = get_line(cfd, buf, sizeof(buf));	
			if (buf[0] == '\n') {
				break;	
			} else if (len == -1)
				break;
		}
        printf("============= The End ============\n");
    }
    
    // 判断get请求
    // char a[] = {'g', 'e', 't'};
    std::string a = "get";
    if(strncasecmp(a.c_str(), line, 3) == 0) { // 请求行: get /hello.c http/1.1 
        std::cout << "deal begin" << std::endl;  
        // 处理http请求
        http_request(line, cfd);
        
        // 关闭套接字, cfd从epoll上del
        disconnect(cfd);         
    }

    return;
}