#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <string>
#include <cstring>
#include <unordered_map>
#include "wrap.h"
#include "epoll.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/time.h>
#include <unordered_map>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <iostream>

// class getFileType {
// public:
//     std::string get_file_type(const std::string& name);

// private:
//     static std::unordered_map<std::string, std::string> FileType;
// };



#define BUFF_SIZE 4096
//重连次数，对于有请求出现但是读不到数据，可能是数据还没到达，对这样请求尝试超过一定的次数放弃
#define AGAIN_MAX_TIMES 200
#define EPOLL_WAIT_TIME 500

//主状态机状态,当前分析状态，请求行、请求头部、内容、正在分析、分析完成
enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0, 
    CHECK_STATE_HEADER, 
    CHECK_STATE_CONTENT,
    CHECK_STATE_ANALYSIS,
    CHECK_STATE_FINISH
};

//服务器处理HTTP请求的结果
enum HTTP_CODE {
    PARSE_SUCCESS = 0, 
    PARSE_ERROR, 
    PARSE_AGAIN 
};

//请求头部读取状态
enum HeaderState
{
    h_start = 0,
    h_key,
    h_colon,
    h_spaces_after_colon,
    h_value,
    h_CR,
    h_LF,
    h_end_CR,
    h_end_LF
};

//得到文件类型
std::string getFileType(const std::string& name);

class mytimer;

class http_conn {
private:
    int againTimes;//重复链接次数
    std::string path;//路径
    int fd;//文件描述符
    int epollfd;//epoll句柄
    std::string content;//读取的内容
    std::string method;//请求方法
    std::string HTTPversion;
    std::string file_name;//文件名
    int now_read_pos;//当前读取位置
    CHECK_STATE state;//主状态机状态
    HeaderState h_state;//请求头部的解析状态
    bool isfinish;
    bool keep_alive;//长连接
    std::unordered_map<std::string, std::string> headers;
    mytimer *timer;//定时器

private:
    HTTP_CODE parse_URI();
    HTTP_CODE parse_Headers();
    HTTP_CODE analysisRequest();

public:
    http_conn();
    http_conn(int _epollfd, int _fd, std::string _path);
    ~http_conn();

    //添加定时属性
    void addTimer(mytimer *mtimer);

    //重置http_conn
    void reset();

    //去除http_conn的定时属性
    void seperateTimer();

    int getFd();
    void setFd(int _fd);
    void handleRequest();
    void handleError(int fd, int err_num, std::string short_msg);

};

class mytimer {
public:
    mytimer(http_conn *_request_data, int timeout);
    ~mytimer();
    void update(int timeout);//更新时间
    bool isvalid(); //判断有没有超时
    void clearReq(); //清除定时器内的事件，seperateTimer时候使用
    void setDeleted();//超时或分离定时器后，都会使用
    bool isDeleted();
    size_t getExpTime() const;

private:
    size_t expire;//定时器生效的绝对时间,time_t是秒为单位
    http_conn* request_data;
    bool deleted;
};

struct timerCmp
{
    bool operator()(const mytimer *a, const mytimer *b) const;
};



#endif