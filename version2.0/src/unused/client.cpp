#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "wrap.h"

#define SER_PORT 9000


int main(int argc, char *argv[]) {
    int cfd = 0;
    int count = 10;
    char buf[BUFSIZ];

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SER_PORT);

    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr.s_addr);

    cfd = Socket(AF_INET, SOCK_STREAM, 0);


    //将新创建的socket与服务器的socket相连
    int ret = Connect(cfd, (sockaddr *) &server_addr, sizeof(server_addr));

    while(count--) {
        Write(cfd, "hello\n", 6);
        ret = Read(cfd, buf, sizeof(buf));
        Write(STDOUT_FILENO, buf, ret);
        sleep(1);
    }
    Close(cfd);

    return 0;

}