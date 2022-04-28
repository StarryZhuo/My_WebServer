#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <pthread.h>
#include <vector>
#include <functional>
#include <memory>
#include <queue>

#define DEFAULT_THREADS_OPR 10
#define DEFUAL_TIME 10
#define MAX_THREADS 1024
#define MAX_QUEUE 65535

struct ThreadPoolTask
{
    std::function<void(void *)> fun;
    void *args;
};

class ThreadPool {

public:
    typedef std::shared_ptr<ThreadPool> ptr;

    // ThreadPool();

    int threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);
    
    //任务队列中添加一个任务

    int threadpool_add(std::function<void(void *)> fun, void *args);

    int threadpool_destroy();

    //int threadpool_free();

    //创造线程的执行函数
    void* threadpool_thread();

    void* adjust_thread();

    // ~ThreadPool();

private:
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;        /* 用于锁住本结构体 */    
    pthread_mutex_t thread_counter = PTHREAD_MUTEX_INITIALIZER;     /* 记录忙状态线程个数的琐 -- busy_thr_num */

    pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;      /* 当任务队列满时，添加任务的线程阻塞，等待此条件变量 */
    pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;     /* 任务队列里不为空时，通知等待任务的线程 */

    std::vector<pthread_t> threads;     /* 存放线程池中每个线程的tid。数组 */
    pthread_t adjust_tid;               /* 存管理线程tid */

    //不用链表方便查找
    std::vector<ThreadPoolTask> task_queue;      /* 任务队列(数组首地址) */

    int min_thr_num = 0;                    /* 线程池最小线程数 */
    int max_thr_num = 1024;                    /* 线程池最大线程数 */
    int live_thr_num = 0;                   /* 当前存活线程个数 */
    int busy_thr_num = 0;                   /* 忙状态线程个数 */
    int wait_exit_thr_num = 0;              /* 要销毁的线程个数 */

    int queue_front = 0;                    /* task_queue队头下标 */
    int queue_rear = 0;                     /* task_queue队尾下标 */
    int queue_size = 0;                     /* task_queue队中实际任务数 */
    int queue_max_size = 65535;                 /* task_queue队列可容纳任务数上限 */

    //不关闭线程
    bool shutdown = false;                       /* 标志位，线程池使用状态，true或false */
};
// pthread_mutex_t ThreadPool::lock = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t ThreadPool::thread_counter = PTHREAD_MUTEX_INITIALIZER;
// pthread_cond_t ThreadPool::queue_not_full = PTHREAD_COND_INITIALIZER;
// pthread_cond_t ThreadPool::queue_not_empty = PTHREAD_COND_INITIALIZER;

// std::vector<pthread_t> ThreadPool::threads;
// pthread_t ThreadPool::adjust_tid;
// std::vector<ThreadPoolTask> ThreadPool::task_queue;

// int ThreadPool::queue_front = 0;
// int ThreadPool::queue_rear = 0;
// int ThreadPool::queue_size = 0;
// int ThreadPool::queue_max_size = 65535;

// bool ThreadPool::shutdown = false;
// int ThreadPool::min_thr_num = 0;
// int ThreadPool::max_thr_num = 1024; 
// int ThreadPool::wait_exit_thr_num = 0;
// int ThreadPool::busy_thr_num = 0;
// int ThreadPool::live_thr_num =0; 


#endif