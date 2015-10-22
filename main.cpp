#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/epoll.h>
#include "HttpServer.h"
#include "threadpool.h"
#include "threadpool_write.h"
#include "locker.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern int addfd(int epollfd, int fd);
extern int removefd(int epollfd, int fd);

void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char* info)
{
    printf("%s" , info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", argv[0]);
        return -1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    /*忽略SIGPIPE信号*/
    addsig(SIGPIPE, SIG_IGN);

    /*创建线程池*/
    threadpool<HttpServer> *pool = new threadpool<HttpServer>;
//  threadwrite<HttpServer> *writepool = new threadwrite<HttpServer>;

    HttpServer* users = new HttpServer[MAX_FD];
    assert(users);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    ret = listen(listenfd, 5);
    assert(ret >= 0);

    epoll_event events[ MAX_EVENT_NUMBER ];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);
    HttpServer::m_epollfd = epollfd;

    while(true)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR))
        {
            printf("epollfd failure\n");
            break;
        }

        for(int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address,&client_addrlength);
                if(connfd < 0)
                {
                    printf("accept failed,errno is : %d\n",errno);
                    continue;
                }
                if(HttpServer::m_user_count > MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    continue;
                }
                /*初始化客户连接*/
                users[connfd].init(connfd, client_address);
            }
            else if(events[i].events & EPOLLRDHUP)
            {
                /*直接关闭客户连接*/
                users[sockfd].close_conn();
            }
            else if(events[i].events & EPOLLIN)
            {
                if(users[sockfd].read())
                {
                    if(!pool->append(users + sockfd))
                    {
                        printf("error!\n");
                        continue;
                    }
                }
                else 
                {
                    printf("read failed!\n");
                    users[sockfd].close_conn();
                    continue;
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
					/*
                if(!writepool->append(users + sockfd))
                {
                    printf("write failed~\n");
                    users[sockfd].close_conn();
                    continue;
                }
                */
                if(!users[sockfd].write())
                {
                    printf("write failed!\n");
                    continue;
                }
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}
