#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include "locker.h"

class HttpServer
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 4096;
    static const int WRITE_BUFFER_SIZE = 4096;
    static const int RESPONSE_SIZE = 4096;
    static const int URL_SIZE = 168;
public:
    HttpServer(){}
    ~HttpServer(){}
    void init(int sockfd, const sockaddr_in& addr);
    void close_conn();
    /*处理客户请求*/
    void process();
    bool read();
    bool write();
public:
    /*所有socket上的事件都注册到同一个epoll内核事件表中，
        所以epoll文件描述符设置为静态的*/
    static int m_epollfd;
    /*用户数量*/
    static int m_user_count;
private:
    void init();
    /*获取URL*/
    char* geturl(char* msg_from_client, char* url);
    /*判断是否为HTTP请求*/
    bool is_http_protocol(char* msg_from_client);
    const char* get_file_type(char* url);
    /*获取目标文件*/
    bool get_file();
private:
    int m_sockfd;
    sockaddr_in m_address;
    
    /*读缓存区*/
    char m_read_buf[ READ_BUFFER_SIZE ];
    char m_write_buf[ WRITE_BUFFER_SIZE ];
    char m_response[ RESPONSE_SIZE ];
    /*客户端请求到目标文件的文件名*/
    char m_url[ URL_SIZE ];
    /*当前正在分析已读入缓存的最后一个字节的下一个位置*/
    int m_read_index;
    /*客户端请求的目标文件的完整路径*/
    char m_read_file[ FILENAME_LEN ]; 
    /*目标文件的状态*/
    struct stat m_file_stat;
    /*Content-Type*/
    const char* m_type;
	/*locker for file*/
	locker m_lock;
};
#endif
