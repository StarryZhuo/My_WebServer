#ifndef __HTTPDEAL_H__
#define __HTTPDEAL_H__

#include <stdio.h>
#include <string>

//十六进制转十进制
int hexit(char letter);

//解码
void decode_str(char *to, char *from);

//加密
void encode_str(char* to, int tosize, const char* from);

//发送错误
void send_error(int cfd, int status, char *title, char *text);

//发送目录
void send_dir(int cfd, const char* dirname);

// 发送响应头
void send_respond_head(int cfd, int no, const char* desp, const char* type, long len);

// 发送文件
void send_file(int cfd, const char* filename);

// 通过文件名获取文件的类型
const char *get_file_type(const char *name);

// 解析http请求消息的每一行内容
int get_line(int sock, char *buf, int size);

// 通过文件名获取文件的类型
void http_request(const char* request, int cfd);


#endif