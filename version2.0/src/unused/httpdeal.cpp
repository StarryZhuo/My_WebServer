#include "httpdeal.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <iostream>
#include <string>
using namespace std;

//十六进制转换成10进制
int hexit(char letter) {
    if(letter >= '0' && letter <= '9') {
        return letter - '0';
    }
    if(letter >= 'a' && letter <= 'f') {
        return letter - 'a' + 10;
    }
    if(letter >= 'A' && letter <= 'F') {
        return letter - 'A' + 10;
    }
    return 0;
} 

void decode_str(char *to, char *from) {
    for(; *from != '\0'; ++to, ++from) {
        if(from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
            *to = hexit(from[1])*16 + hexit(from[2]);
            from += 2;
        } else {
            *to = *from;
        }
    }
    *to = '\0';
}

void decode_str1(string to, string from) {
    for(int i = 0, j = 0; i < from.size(); i++,j++) {
        if(from[i] == '%' && isxdigit(from[i + 1]) && isxdigit(from[i + 2])) {
            to[j] = hexit(from[1])*16 + hexit(from[2]);
            i += 2;
        } else {
            to[j] = from[i];
        }
    }
}

/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */
void encode_str(char* to, int tosize, const char* from)
{
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {    
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {      
            *to = *from;
            ++to;
            ++tolen;
        } else {
            sprintf(to, "%%%02x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}

// void encode_str1(string to, int tosize, const string from){
//     for(int i = 0, j = 0; i < from.size(); i++,j++) {
//         if(isalnum(from[i]) || strchr("/_.-~", from[i]) != (char*)0) {
//             to[j] = from[i];
//         } else {
//             char a[4];
//             sprintf(a, "%%%02x", (int) from[i] & 0xff);
//             string b(a,a+3);
//             to += b;
//             j += 3;
//         }
//     }
// }

//发送错误
void send_error(int cfd, int status, char *title, char *text) {
    char buf[4096] = {0};

    sprintf(buf, "%s %d %s\r\n", "HTTP/1.1", status, title);
    sprintf(buf + strlen(buf), "Content-Type:%s\r\n", "text/html");
    sprintf(buf + strlen(buf), "Content-Length:%d\r\n", -1);
    sprintf(buf + strlen(buf), "Connection: close\r\n");
    send(cfd, buf, strlen(buf), 0);
    send(cfd, "\r\n", 2, 0);

    memset(buf, 0, sizeof(buf));

    sprintf(buf, "<html><head><title>%d %s</title></head>\n", status, title);
    sprintf(buf + strlen(buf), "<body bgcolor = \"#cc99cc\"><h2 align=\"center\">%d %s</h4>\n", status, title);
    sprintf(buf+strlen(buf), "%s\n", text);
    sprintf(buf + strlen(buf), "<hr>\n</body>\n</html>\n");
    send(cfd, buf, strlen(buf), 0);

    return;
}

//发送目录
void send_dir(int cfd, const char* dirname)
{
    int i, ret;

    // 拼一个html页面<table></table>
    char buf[4094] = {0};

    sprintf(buf, "<html><head><title>目录: %s</title></head>", dirname);
    sprintf(buf+strlen(buf), "<body><h1>当前目录: %s</h1><table>", dirname);

    char enstr[1024] = {0};
    char path[1024] = {0};
    
    // 目录项二级指针
    struct dirent** ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);
    
    // 遍历
    for(i = 0; i < num; ++i) {
    
        char* name = ptr[i]->d_name;

        // 拼接文件的完整路径
        sprintf(path, "%s/%s", dirname, name);
        printf("path = %s ===================\n", path);
        struct stat st;
        stat(path, &st);

		// 编码生成 %E5 %A7 之类的东西
        encode_str(enstr, sizeof(enstr), name);
        
        // 如果是文件
        if(S_ISREG(st.st_mode)) {       
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        } else if(S_ISDIR(st.st_mode)) {		// 如果是目录       
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }
        ret = send(cfd, buf, strlen(buf), 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error:");
                continue;
            } else if (errno == EINTR) {
                perror("send error:");
                continue;
            } else {
                perror("send error:");
                exit(1);
            }
        }
        memset(buf, 0, sizeof(buf));
        // 字符串拼接
    }

    sprintf(buf+strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);

    printf("dir message send OK!!!!\n");
#if 0
    // 打开目录
    DIR* dir = opendir(dirname);
    if(dir == NULL)
    {
        perror("opendir error");
        exit(1);
    }

    // 读目录
    struct dirent* ptr = NULL;
    while( (ptr = readdir(dir)) != NULL )
    {
        char* name = ptr->d_name;
    }
    closedir(dir);
#endif
}

// 发送响应头
void send_respond_head(int cfd, int no, const char* desp, const char* type, long len)
{
    char buf[1024] = {0};
    // 状态行
    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, desp);
    // printf("http/1.1 %d %s\r\n", no, desp);
    send(cfd, buf, strlen(buf), 0);
    // 消息报头
    // printf("Content-Type: %s\r\n", type);
    sprintf(buf, "Content-Type: %s\r\n", type);

    // printf("Content-Length:%ld\r\n", len);
    sprintf(buf+strlen(buf), "Content-Length:%ld\r\n", len);
    send(cfd, buf, strlen(buf), 0);
    // 空行
    send(cfd, "\r\n", 2, 0);
}

// 发送文件
void send_file(int cfd, const char* filename)
{
    // 打开文件
    int fd = open(filename, O_RDONLY);
    if(fd == -1) {
        char a[] = {'N','o', 't', ' ', 'F', 'o', 'u', 'n', 'd'};
        "NO such file or direntry";
        char b[] = {'N', 'O', ' ', 's', 'u', 'c', 'h', ' ', 'f', 'i', 'l', 'e',
                    ' ', 'o', 'r', ' ', 'd', 'i', 'r', 'e', 'n', 't', 'r', 'y'};
        // std::string a = "Not Found";
        // std::string b = "NO such file or direntry";
        send_error(cfd, 404, a, b);
        exit(1);
    }

    // 循环读文件
    char buf[4096] = {0};
    int len = 0, ret = 0;
    while( (len = read(fd, buf, sizeof(buf))) > 0 ) {   
        // 发送读出的数据
        ret = send(cfd, buf, len, 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error:");
                continue;
            } else if (errno == EINTR) {
                perror("send error:");
                continue;
            } else {
                perror("send error:");
                exit(1);
            }
        }
    }
    if(len == -1)  {  
        perror("read file error");
        exit(1);
    }

    close(fd);
}

// 解析http请求消息的每一行内容
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n')) {    
        n = recv(sock, &c, 1, 0);
        if (n > 0) {        
            if (c == '\r') {            
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {               
                    recv(sock, &c, 1, 0);
                } else {                            
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } else {       
            c = '\n';
        }
    }
    buf[i] = '\0';

    return i;
}

// 通过文件名获取文件的类型
const char *get_file_type(const char *name)
{
    

    // 自右向左查找‘.’字符, 如不存在返回NULL
    auto dot = strrchr(name, '.');   
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}


void http_request(const char* request, int cfd) {

    //拆分http请求行
    char method[12];
    char path[1024];
    char protocol[12];
    std::cout << "http_request" << std::endl;
    sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol);
    std::cout << "http_request" << std::endl;
    printf("method = %s, path = %s, protocol = %s\n", method, path, protocol);

    //转码 不能识别中文乱码
    //解码 %23 %34 %5f
    decode_str(path, path);

    //去掉path中的/获取访问文件名
    char *file = path + 1;
    if(strcmp(path, "/") == 0) {
        char a[] = {'.', '/'};
        file = a;
    }

    // 获取文件属性
    struct stat st;
    int ret = stat(file, &st);
    if(ret == -1) { 
        char a[] = {'N','o', 't', ' ', 'F', 'o', 'u', 'n', 'd'};
        //"NO such file or direntry";
        char b[] = {'N', 'O', ' ', 's', 'u', 'c', 'h', ' ', 'f', 'i', 'l', 'e',
                    ' ', 'o', 'r', ' ', 'd', 'i', 'r', 'e', 'n', 't', 'r', 'y'};
        send_error(cfd, 404, a, b);     
        return;
    }

    // 判断是目录还是文件
    if(S_ISDIR(st.st_mode)) {  		// 目录 
        // 发送头信息
        send_respond_head(cfd, 200, "OK", get_file_type(".html"), -1);
        // 发送目录信息
        send_dir(cfd, file);
    } else if(S_ISREG(st.st_mode)) { // 文件        
        // 发送消息报头
        
        // printf("file: %s", file);
        send_respond_head(cfd, 200, "OK", get_file_type(file), st.st_size);

        // 发送文件内容
        send_file(cfd, file);
    }
}

