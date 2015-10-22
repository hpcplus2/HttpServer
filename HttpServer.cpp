#include "HttpServer.h"

int HttpServer::m_user_count = 0;
int HttpServer::m_epollfd = -1;

const char* doc_root = "/var/www/html";

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void removefd(int epollfd, int fd) 
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void HttpServer::init(int sockfd, const sockaddr_in& addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    addfd(m_epollfd, sockfd);
    m_user_count++;
    
    init();
}

void HttpServer::init()
{ 
    m_read_index = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE); 
    memset(m_response, '\0', RESPONSE_SIZE); 
    memset(m_url, '\0', URL_SIZE); 
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE); 
    memset(m_read_file, '\0', FILENAME_LEN); 
}

void HttpServer::close_conn()
{
    if(m_sockfd != -1)
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        --m_user_count;
    }
}

bool HttpServer::read()
{
    if(m_read_index >= READ_BUFFER_SIZE)
    {
        return false;
    }

    int bytes_read = 0;
    while(1)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_index,READ_BUFFER_SIZE - m_read_index, 0);
        if(bytes_read == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if((bytes_read) == 0 && (m_read_index == 0))
        {
            return false;
        }
        else if((bytes_read) == 0 && (m_read_index != 0))
        {
            break;
        }
        m_read_index += bytes_read;
    }
    return true;
}

bool HttpServer::write()
{
    m_lock.lock();
    int fd = open(m_read_file,O_RDONLY,0);
    if(fd == -1)
    {
        printf("open file failed!\n");
	m_lock.unlock();
        return false;
    }
    int ret = 0;
    snprintf(m_response, RESPONSE_SIZE ,"HTTP/1.1 200 OK\r\nData:2015\r\nConnection:keep-alive\r\nContent-Length:%d\r\nContent-Type:%s\r\n\r\n",(int)m_file_stat.st_size, m_type);
    //printf("%s\n", m_response);
	/*
    ret = ::send(m_sockfd, m_response, strlen(m_response), 0);
    if(ret == -1)
    {
        printf("send response failed!\n");
        close(fd);
		m_lock.unlock();
        return false;
    }
    while((ret = ::read(fd, m_write_buf,WRITE_BUFFER_SIZE )) > 0)
    {
        ret = send(m_sockfd, m_write_buf, strlen(m_write_buf), 0);
        if(ret == -1)
        {
            printf("send failed!\n");
            close(fd);
			m_lock.unlock();
            return false;
        }
    }
	*/
    ret = ::read(fd, m_write_buf, WRITE_BUFFER_SIZE);
    //printf("%s\nfile_size = %dbytes\n", m_write_buf, (int)strlen(m_write_buf));
    if(ret == -1)
    {
	printf("read failed!\n");
        close(fd);
	init();
	m_lock.unlock();
        return false;
    }
    strcat(m_response, m_write_buf);
    //printf("%s", m_response);
    //ret = ::send(m_sockfd, m_write_buf, strlen(m_write_buf), 0);
    ret = ::send(m_sockfd, m_response, strlen(m_response), 0);
	if(ret == -1)
	{
	printf("send failed!\n");
     	close(fd);
	init();
	m_lock.unlock();
        return false;
	}
    printf("response发送成功!\n");
    close(fd);
    init();
    modfd(m_epollfd, m_sockfd, EPOLLIN);
    m_lock.unlock();
    return true;
}

void HttpServer::process()
{
    /*分析是否是HTTP协议*/
    if(!is_http_protocol(m_read_buf))
    {
        printf("This is not HTTP protocol!\n");
        return;
    }
    /*获取URI*/
    if(!geturl(m_read_buf, m_url))
    {
        printf("get URL failed!\n");
        return;
    }

    /*URI的文件类型*/
    if((m_type = get_file_type(m_url)) == NULL)
    {
        printf("URL文件类型unknow!\n");
        return;
    }
	/*
    else
    {
        printf("URL type:%s\n" , m_type);
    }
	*/
    if(!get_file())
    {
        printf("请求文件错误!\n");
        return;
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

char* HttpServer::geturl(char* msg_from_client, char* url)
{
    int index = 0;
    while((msg_from_client[index] != '/') && (msg_from_client[index] != '\0'))
    {
        ++index;
    }
    int base = index;
    while((msg_from_client[index] != ' ' ) && (msg_from_client[index] != '\0'))
    {
        ++index;
    }
    if((msg_from_client[index - 1] == '/') && (msg_from_client[index] == ' '))
    {
        strcpy(url, "index.html");
        return url;
    }

    strncpy(url, msg_from_client + base, index - base);
    return url;
}

const char* HttpServer::get_file_type(char* url)
{
    int len = strlen(url);
    int index = len - 1;

    while(index > 0 && url[index] != '.')
    {
        --index;
    }
    if(index <= 0)
    {
        return NULL;
    }
    index++;
    int type_len = len - index;
    char* file_type = url + index;
    switch(type_len)
    {
        case 2:
            if((!strcmp(file_type , "js")) || (!strcmp(file_type , "JS")))
            {
                return "text/javascript";
            }
            break;
        case 3:
            if((!strcmp(file_type , "htm")) || (!strcmp(file_type , "HTM")))
            {
                return "text/html";
            }
            if((!strcmp(file_type , "css")) || (!strcmp(file_type , "CSS")))
            {
                return "text/css";
            }
            if((!strcmp(file_type , "txt")) || (!strcmp(file_type , "TXT")))
            {
                return "text/plain";
            }
            if((!strcmp(file_type , "png")) || (!strcmp(file_type , "PNG")))
            {
                return "text/png";
            }
            if((!strcmp(file_type , "gif")) || (!strcmp(file_type , "GIF")))
            {
                return "text/gif";
            }
            if((!strcmp(file_type , "jpg")) || (!strcmp(file_type , "JPG")))
            {
                return "text/jpeg";
            }
            break;
        case 4:
            if((!strcmp(file_type , "html")) || (!strcmp(file_type , "HTML")))
            {
                return "text/html";
            }
            if((!strcmp(file_type , "jpeg")) || (!strcmp(file_type , "JPEG")))
            {
                return "text/jpeg";
            }
            break;
        default:
            return NULL;
    }
    return NULL;
}
bool HttpServer::is_http_protocol(char* msg_from_client)
{
    //printf("%s\n", msg_from_client);
    int index = 0;
    while(msg_from_client[index] != '\0' && msg_from_client[index] != '\n')
    {
        ++index;
    }
    //printf("%s\n", msg_from_client + index - 9);
    if(strncmp(msg_from_client + index - 9,"HTTP/", 5) == 0)
    {
        return true;
    }
    return false;
}

bool HttpServer::get_file()
{
    strcpy(m_read_file, doc_root);
    int len = strlen(doc_root);
    //printf("%s\n", m_url);
    strncpy(m_read_file + len, m_url, FILENAME_LEN - len - 1);
    //printf("请求的文件:%s\n", m_read_file);
    if(stat(m_read_file, &m_file_stat) < 0)
    {
        printf("404 Not Found\n");
        return false;
    }

    if(!(S_ISREG(m_file_stat.st_mode)))
    {
        printf("404 Not Found\n");
        return false;
    } 
    return true;
}
