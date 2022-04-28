#include "../src/epoll_threadpool.h"
#include <iostream>
#include <unistd.h>

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
    
    Epoll_threadpool epollthp(port);
    epollthp.init_Epoll();
    while(1) {
        ;
    }
    std::cout << "end" << std::endl;
    return 1;

}