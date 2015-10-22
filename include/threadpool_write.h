#ifndef THREADPOOL_WRITE_H
#define THREADPOOL_WRITE_H

#include <list>
#include <stdio.h>
#include <pthread.h>
#include <exception>
#include "locker.h"

template <typename T>
class threadwrite
{
public:
    threadwrite();
    ~threadwrite();
    bool append(T* request);
private:
    static void* worker(void* arg);
    void run();
private:
    int m_thread_number;     //线程池中的线程数量
    size_t m_max_requests;      //请求队列中允许的最大请求数量
    pthread_t* m_threads;       //线程池数组
    std::list<T* > m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //信号量，表示是否有任务需要处理
    bool m_stop;                //是否结束进程
};

template <typename T>
threadwrite< T >::threadwrite():
    m_thread_number(10),m_max_requests(100000),
    m_threads(NULL),m_stop(false)
{
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads)
    {
        throw std::exception();
    }

    //创建thread_number个线程，并将他们设置成脱离线程
    for(int i = 0; i < m_thread_number; ++i)
    {
        if(pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadwrite< T >::~threadwrite()
{
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadwrite<T >::append(T* request)
{
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void* threadwrite<T >::worker(void* arg)
{
    threadwrite* pool = (threadwrite*)arg;
    pool->run();
    return pool;
}

template < typename T>
void threadwrite< T >::run()
{
    while(!m_stop)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
        {
            continue;
        }
        request->write();
    }
}

#endif
