#include "http_conn.h"
#include "wrap.h"
#include "epoll.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/time.h>
#include <unordered_map>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string>
#include <queue>
#include <deque>
#include <vector>
#include <iostream>
using namespace std;

//用map形式写的话会方便之后添加新的文件类型。
// std::unordered_map<std::string, std::string> getFileType::FileType = {
//             {".html", "text/html; charset=utf-8"}, 
//             {".jpg", "image/jpeg"},
//             {".gif", "image/gif"},
//             {".png", "image/png"},
//             {".css", "text/css"},
//             {".au", "audio/basic"},
//             {".wav", "audio/wav"},
//             {".avi", "video/x-msvideo"},
//             {".mov", "video/quicktime"},
//             {".mpeg", "video/mpeg"},
//             {".vrml", "model/vrml"},
//             {".midi", "audio/midi"},
//             {".mp3", "audio/mp3"},
//             {".ogg", "application/ogg"},
//             {".pac", "application/x-ns-proxy-autoconfig"},
//             {".txt", "text/plain"},
//             {"default", "text/plain; charset=utf-8"}
//         };

pthread_mutex_t timelock = PTHREAD_MUTEX_INITIALIZER;

priority_queue<mytimer*, deque<mytimer*>, timerCmp> myTimerQueue;

bool timerCmp::operator()(const mytimer *a, const mytimer *b) const {
    return a->getExpTime() > b->getExpTime();
}

std::string getFileType(const std::string& name){
    if(name == "default") return "text/plain; charset=utf-8";
    if(name == ".html") return "text/html; charset=utf-8";
    if(name == ".jpg") return "image/jpeg";
    if(name == ".gif") return "image/gif";
    if(name == ".png") return "image/png";
    if(name == ".css") return "text/css";
    if(name == ".au") return "audio/basic";
    if(name == ".wav") return "audio/wav";
    if(name == ".avi") return "video/x-msvideo";
    if(name == ".mov") return "video/quicktime";
    if(name == ".mpeg") return "video/mpeg";
    if(name == ".vrml") return "model/vrml";
    if(name == ".midi") return "audio/midi";
    if(name == ".mp3") return "audio/mp3";
    if(name == ".ogg") return "application/ogg";
    if(name == ".pac") return "application/x-ns-proxy-autoconfig";
    if(name == ".txt") return "text/plain";
    return "text/plain; charset=utf-8";
} 

http_conn::http_conn():
        now_read_pos(0), state(CHECK_STATE_REQUESTLINE), h_state(h_start),
        keep_alive(false), againTimes(0), timer(NULL) {}

http_conn::http_conn(int _epollfd, int _fd, std::string _path):
        now_read_pos(0), state(CHECK_STATE_REQUESTLINE), h_state(h_start),
        keep_alive(false), againTimes(0), timer(NULL),
        path(_path), fd(_fd), epollfd(_epollfd) {
            cout << "http_conn() construct" << endl;
        }

http_conn::~http_conn() {
    cout << "~http_conn()" << endl;
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = (void*)this;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
    if(timer != NULL) {
        timer->clearReq();
        timer = NULL;
    }
    close(fd);
}

void http_conn::addTimer(mytimer *mtimer)
{
    if (timer == NULL)
        timer = mtimer;
}

int http_conn::getFd()
{
    return fd;
}

void http_conn::setFd(int _fd)
{
    fd = _fd;
}

void http_conn::reset(){
    againTimes = 0;
    content.clear();
    file_name.clear();
    path.clear();
    now_read_pos = 0;
    state = CHECK_STATE_REQUESTLINE;
    h_state = h_start;
    headers.clear();
    keep_alive = false;
}

void http_conn::seperateTimer()
{
    if (timer)
    {
        timer->clearReq();
        timer = NULL;
    }
}

void http_conn::handleRequest() {
    char buff[BUFF_SIZE];
    memset(buff, '\0', BUFF_SIZE);
    bool isError = false;
    while(1) {
        int readNum = Readn(fd, buff, BUFF_SIZE);
        if(readNum < 0) {
            perror("1");
            isError = true;
            break;
        }
        else if(readNum == 0) {
            //有请求但是读不到数据，可能是Request Aborted，或者来自网络的数据没有达到等原因
            perror("read_num == 0");
            if (errno == EAGAIN)
            {
                if (againTimes > AGAIN_MAX_TIMES)
                    isError = true;
                else
                    ++againTimes;
            }
            else if(errno != 0)
                isError = true;
            break;
        }
        std::string now_read(buff, buff + readNum);
        content += now_read;

        //对请求行进行解析,不能用switch，因为后面会实现状态的转移
        if(state == CHECK_STATE_REQUESTLINE) {
            HTTP_CODE flag = this->parse_URI();
            if(flag == PARSE_AGAIN) {
                break;
            } else if(flag == PARSE_ERROR) {
                perror("parse_URI ERROR");
                isError = true;
                break;
            }
        }

        //对请求头部进行解析
        if(state == CHECK_STATE_HEADER) {
            HTTP_CODE flag = this->parse_Headers();
            if (flag == PARSE_AGAIN) {
                break;
            } else if(flag == PARSE_ERROR) {
                perror("parse_Headers ERROR");
                isError = true;
                break;
            }
            if(method == "POST") {
                //如果解析到是post请求
                state = CHECK_STATE_CONTENT;
            } else {
                //如果解析到是get请求
                state = CHECK_STATE_ANALYSIS;
            }
        }

        if(state == CHECK_STATE_CONTENT) {
            int content_length = -1;//post请求报文的首部行里面必然会有Content-length字段而get没有 所以取出这个字段 求出后面实体主体时候要取用的长度
            if(headers.find("Content-length") != headers.end()) {  //在parse_Headers函数里面会把首部行的key value放在一个map里面headers
                content_length = stoi(headers["Content-length"]);
            }
            else {
                isError = true;
                break;
            }
            if(content.size() < content_length)
                continue;
            //将POST的content_length处理后，剩下的内容与get一样进行处理
            state = CHECK_STATE_ANALYSIS;
        }

        if(state == CHECK_STATE_ANALYSIS) {
            HTTP_CODE flag = this->analysisRequest();
            if(flag == PARSE_SUCCESS) {
                state = CHECK_STATE_FINISH;
                break;
            } else {
                isError = true;
                break;
            }
        }
        
    }
    if(isError) {
        delete this;
        return;
    }

    //如果设置长连接，在执行完后，则加入epoll继续响应
    if(state == CHECK_STATE_FINISH) {
        if(keep_alive) {
            printf("ok\n");
            //重置后，继续执行
            this->reset();
        } else {
            delete this;
            return;
        }
    }

    //重新加入定时事件。EPOLL_WAIT_TIME
    pthread_mutex_lock(&timelock);
    mytimer *mtimer = new mytimer(this, EPOLL_WAIT_TIME);
    timer = mtimer;
    myTimerQueue.push(mtimer);
    pthread_mutex_unlock(&timelock);

    //重置该文件描述符的事件
    __uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
    int ret = epoll_mod(epollfd, fd, static_cast<void*>(this), _epo_event);
    if (ret < 0)
    {
        // 返回错误处理
        delete this;
        return;
    }

}

HTTP_CODE http_conn::parse_URI() { //解析报文中的请求行

/*将content中的请求行去掉*/
    int pos = content.find('\r', now_read_pos);
    if(pos < 0) {
        return PARSE_AGAIN;
    }

    std::string request_line = content.substr(0, pos); //取出 请求行
    if (content.size() > pos + 1) 
        content = content.substr(pos + 1);
    else 
        content.clear();
    
/*解析请求行中的请求类型(GET还是POST)*/
    pos = request_line.find("GET");
    if(pos < 0) {
        pos = request_line.find("POST");
        if(pos < 0) {
            return PARSE_ERROR;
        } else {
            method = "POST";
        }
    } else {
        method = "GET";
    }

 /*解析请求行中的URL 即浏览器要请求的文件地址*/
    pos = request_line.find("/", pos);
    if(pos < 0) {
        return PARSE_ERROR;
    } else {
        //URL形式/test/demo_form.php?name1=value1&name2=value2
        int _pos1 = request_line.find(' ', pos);
        if(_pos1 < 0) {
            return PARSE_ERROR;
        }else {
            if(_pos1 - pos > 1) {
                file_name = request_line.substr(pos + 1, _pos1 - pos - 1);
                int _pos2 = file_name.find('?');
                if(_pos2 >= 0) {
                    file_name = file_name.substr(0, _pos2);
                    //std::cout << "filename" << file_name<< std::endl;
                }
            }
            //else file_name = "../include/index.html";
        }
        pos = _pos1;
    }

    /*解析请求行中的HTTP 版本号 */
    pos = request_line.find("/", pos);
    if(pos < 0) {
        return PARSE_ERROR;
    } else {
        if(request_line.size() - pos <= 3) {
            return PARSE_ERROR;
        } else {
            std::string version = request_line.substr(pos + 1, 3);
            std::cout << "version: "<< version << std::endl;
            if(version == "1.0") {
                HTTPversion = "1.0";
            } else if(version == "1.1") {
                HTTPversion = "1.1";
            } else {
                return PARSE_ERROR;
            }
        }
    }

    //读完后，主状态state变更为CHECK_STATE_HEADER
    state = CHECK_STATE_HEADER;
    return PARSE_SUCCESS;
    
}

HTTP_CODE http_conn::parse_Headers() {
    //前闭后开的
    int key_start = -1, key_end = -1, value_start = -1, value_end = -1;
    int now_read_line_begin = 0;
    bool notFinish = true;
    for (int i = 0; i < content.size() && notFinish; ++i) {
        switch (h_state)
        {
            case h_start://开始
            {
                if(content[i] == '\n' || content[i] == '\r') break;
                h_state = h_key;
                key_start = i;
                now_read_line_begin = i;
                break;
            }
            case h_key://读取key
            {
                if (content[i] == ':'){
                    key_end = i;
                    if (key_end - key_start <= 0)
                        return PARSE_ERROR;
                    h_state = h_colon;
                }
                else if(content[i] == '\n' || content[i] == '\r')
                    return PARSE_ERROR;
                break;
            }
            case h_colon://读取key之后的冒号
            {
                if (content[i] == ' ')
                {
                    h_state = h_spaces_after_colon;
                }
                else
                    return PARSE_ERROR;
                break;  
            }
            case h_spaces_after_colon:
            {
                h_state = h_value;
                value_start = i;
                break;  
            }
            case h_value:
            {
                if (content[i] == '\r')
                {
                    h_state = h_CR;
                    value_end = i;
                    if (value_end - value_start <= 0)
                        return PARSE_ERROR;
                }
                else if (i - value_start > 255) //key_value长度应该小于255
                    return PARSE_ERROR;
                break;  
            }
            case h_CR:
            {
                if (content[i] == '\n')
                {
                    h_state = h_LF;
                    string key(content.begin() + key_start, content.begin() + key_end);
                    string value(content.begin() + value_start, content.begin() + value_end);
                    headers[key] = value;
                    now_read_line_begin = i;
                }
                else
                    return PARSE_ERROR;
                break;  
            }
            case h_LF:
            {
                if(content[i] == '\r') {
                    h_state = h_end_CR;
                } else {
                    key_start = i;
                    h_state = h_key;
                }
                break;
            }
            case h_end_CR:
            {
                if (content[i] == '\n')
                {
                    h_state = h_end_LF;
                }
                else
                    return PARSE_ERROR;
                break;
            }
            case h_end_LF:
            {
                notFinish = false;
                key_start = i;
                now_read_line_begin = i;
                break;
            }
        }
    }

    //将content剩下的分离
    if(h_state == h_end_LF) {
        content = content.substr(now_read_line_begin);
        return PARSE_SUCCESS;
    }
    content = content.substr(now_read_line_begin);
    return PARSE_AGAIN;
}

HTTP_CODE http_conn::analysisRequest() {
    if(method == "POST") {

        //get content
        char respondHeader[BUFF_SIZE];
        sprintf(respondHeader, "HTTP/1.1 %d %s\r\n", 200, "OK");
        if(headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive") {
            keep_alive = true;
            sprintf(respondHeader + strlen(respondHeader), "Connection: keep-alive\r\n");
            sprintf(respondHeader + strlen(respondHeader), "Keep-Alive: timeout=%d\r\n", EPOLL_WAIT_TIME);
        }
        string ss = "I have receiced this.";
        const char *send_content = ss.c_str();

        sprintf(respondHeader + strlen(respondHeader), "Content-length: %zu\r\n", strlen(send_content));
        sprintf(respondHeader + strlen(respondHeader), "\r\n");
        
        size_t send_len = (size_t)Writen(fd, respondHeader, strlen(respondHeader));
        if(send_len != strlen(respondHeader))
        {
            perror("Send header failed");
            return PARSE_ERROR;
        }

        send_len = (size_t)Writen(fd, send_content, strlen(send_content));
        if(send_len != strlen(send_content))
        {
            perror("Send content failed");
            return PARSE_ERROR;
        }

        cout << "content size ==" << content.size() << endl;
        vector<char> data(content.begin(), content.end());

        return PARSE_SUCCESS;
    }

    else if (method == "GET") {
        char respondHeader[BUFF_SIZE];
        sprintf(respondHeader, "HTTP/1.1 %d %s\r\n", 200, "OK");
        if(headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
        {
            keep_alive = true;
            sprintf(respondHeader + strlen(respondHeader), "Connection: keep-alive\r\n");
            sprintf(respondHeader + strlen(respondHeader), "Keep-Alive: timeout=%d\r\n", EPOLL_WAIT_TIME);
        }
        //'.'之后代表是文件类型
        int dot_pos = file_name.find('.');
        const char* filetype;
        if(dot_pos < 0)
            filetype = getFileType("default").c_str();
        else
            filetype = getFileType(file_name.substr(dot_pos)).c_str();
        
        //判断是文件
        struct stat sbuf;
        if (stat(file_name.c_str(), &sbuf) < 0)
        { 
            //cout << "handleError" << endl;
            handleError(fd, 404, "Not Found!");
            return PARSE_ERROR;
        }

        sprintf(respondHeader + strlen(respondHeader), "Content-type: %s\r\n", filetype);
        sprintf(respondHeader + strlen(respondHeader), "Content-length: %ld\r\n", sbuf.st_size);

        sprintf(respondHeader + strlen(respondHeader), "\r\n");

        size_t send_len = (size_t)Writen(fd, respondHeader, strlen(respondHeader));
        if(send_len != strlen(respondHeader))
        {
            perror("Send header failed");
            return PARSE_ERROR;
        }

        //使用mmap内存映射文件，传递文件信息
        int src_fd = open(file_name.c_str(), O_RDONLY, 0);
        char *src_addr = static_cast<char*>(mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
        close(src_fd);
        send_len = Writen(fd, src_addr, sbuf.st_size);
        if(send_len != sbuf.st_size)
        {
            perror("Send file failed");
            return PARSE_ERROR;
        }
        munmap(src_addr, sbuf.st_size);
        return PARSE_SUCCESS;

    } else {
        return PARSE_ERROR;
    }
}

void http_conn::handleError(int fd, int err_num, string short_msg)
{
    short_msg = " " + short_msg;
    char send_buff[BUFF_SIZE];
    string body_buff, header_buff;
    body_buff += "<html><title>TKeed Error</title>";
    body_buff += "<body bgcolor=\"ffffff\">";
    body_buff += to_string(err_num) + short_msg;
    body_buff += "<hr><em> Starry's Web Server</em>\n</body></html>";

    header_buff += "HTTP/1.1 " + to_string(err_num) + short_msg + "\r\n";
    header_buff += "Content-type: text/html\r\n";
    header_buff += "Connection: close\r\n";
    header_buff += "Content-length: " + to_string(body_buff.size()) + "\r\n";
    header_buff += "\r\n";

    sprintf(send_buff, "%s", header_buff.c_str());
    Writen(fd, send_buff, strlen(send_buff));
    sprintf(send_buff, "%s", body_buff.c_str());
    Writen(fd, send_buff, strlen(send_buff));
}

mytimer::mytimer(http_conn* _request_date, int timeout):deleted(false), request_data(_request_date){
    struct timeval now;//timeval 中的tv_sec为1970年01月01日0点到创建struct timeval时的秒数，tv_usec为微秒数，
    gettimeofday(&now, NULL);//获取系统当前时间到1970年01月01日0点时struct timeval秒数，tv_usec微秒数，
    // 以毫秒计
    expire = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;//转化为毫秒 即设定当前定时器在距离当前timeout毫秒后超时
}

mytimer::~mytimer()
{
    cout << "~mytimer()" << endl;
    if (request_data != NULL)
    {
        cout << "request_data=" << request_data << endl;
        delete request_data;
        request_data = NULL;
    }
}

void mytimer::update(int timeout)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    expire = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
}

bool mytimer::isvalid()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    size_t temp = ((now.tv_sec * 1000) + (now.tv_usec / 1000));
    if (temp < expire)
    {
        return true;
    }
    else
    {
        this->setDeleted();
        return false;
    }
}

void mytimer::clearReq()
{
    request_data = NULL;
    this->setDeleted();
}

void mytimer::setDeleted()
{
    deleted = true;
}

bool mytimer::isDeleted()
{
    return deleted;
}

size_t mytimer::getExpTime() const
{
    return expire;
}
