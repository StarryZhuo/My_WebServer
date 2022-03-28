#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <wait.h>

#include "wrap.h"

using namespace std;

#define SER_PORT  9999

//信号调用函数
void do_sigchild(int num)
{
    //非阻塞轮询回收进程
    while (waitpid(0, NULL, WNOHANG) > 0)
    
    return;
}

int main(int argc, char *argv[]) {
    int lfd = 0, bfd = 0, cfd = 0;
    char buf[BUFSIZ];
    char client_IP[1024];
    pid_t pid;

    //
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    //将结构体内字节清除
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    lfd = Socket(AF_INET, SOCK_STREAM, 0);

    bfd = Bind(lfd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    Listen(lfd, 128);

    std::cout << "Accepting connections ..." << endl;

    while(1) {
        client_addr_len = sizeof(client_addr);

        cfd = Accept(lfd, (struct sockaddr *) &client_addr, &client_addr_len);

        pid = fork();
        if(pid < 0) { 
            perr_exit("fork error");
        } else if(pid == 0) { //子进程
            Close(lfd);
            int ret = 0;
            while(1) {
                ret = Read(cfd, buf, sizeof(buf));
                //没收到信息
                if(ret == 0) {
                    std::cout << "client close" << endl;
                    Close(cfd);
                    exit(1);
                }
                //网络传本地
                std::cout << "client ip" << inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_IP, sizeof(client_IP))
                            << " port:"  << ntohs(client_addr.sin_port) << endl;

                for(int i = 0; i < ret; i++) {
                    buf[i] = toupper(buf[i]);
                }
                Write(cfd, buf, ret);
                Write(STDOUT_FILENO, buf, ret); 
            }  
        } else {
            //信号结构体
            struct sigaction newact;
            //定义信号结构体，将信号
            newact.sa_handler = do_sigchild;
            sigemptyset(&newact.sa_mask);
            newact.sa_flags = 0;

            int sig = sigaction(SIGCHLD, &newact, NULL);
            if(sig != 0) {
                perr_exit("sigaction error");
            }
            Close(cfd);
        }

    }
    
    return 0;

}
