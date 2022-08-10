# My_WebServer
多并发服务器
LINUX C++，Cmake，epoll，线程池，HTTP，多线程
1.使用Epoll边沿触发的IO多路复用技术，非阻塞IO
2.使用多线程充分利用多核CPU，并使用线程池避免线程频繁创建销毁的开销
3.解析HTTP报文

新建一个build文件夹和bin文件夹，在build文件夹下../cmake, make,在bin下执行
