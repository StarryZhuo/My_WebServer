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

    int threadpool_free();

    //创造线程的执行函数
    static void *threadpool_thread(void *args);

    static void *adjust_thread(void *args);

    // ~ThreadPool();


private:
    static pthread_mutex_t lock;        /* 用于锁住本结构体 */    
    static pthread_mutex_t thread_counter;     /* 记录忙状态线程个数的琐 -- busy_thr_num */

    static pthread_cond_t queue_not_full;      /* 当任务队列满时，添加任务的线程阻塞，等待此条件变量 */
    static pthread_cond_t queue_not_empty;     /* 任务队列里不为空时，通知等待任务的线程 */

    static std::vector<pthread_t> threads;     /* 存放线程池中每个线程的tid。数组 */
    static pthread_t adjust_tid;               /* 存管理线程tid */

    //不用链表方便查找
    static std::vector<ThreadPoolTask> task_queue;      /* 任务队列(数组首地址) */

    static int min_thr_num;                    /* 线程池最小线程数 */
    static int max_thr_num;                    /* 线程池最大线程数 */
    static int live_thr_num;                   /* 当前存活线程个数 */
    static int busy_thr_num;                   /* 忙状态线程个数 */
    static int wait_exit_thr_num;              /* 要销毁的线程个数 */

    static int queue_front;                    /* task_queue队头下标 */
    static int queue_rear;                     /* task_queue队尾下标 */
    static int queue_size;                     /* task_queue队中实际任务数 */
    static int queue_max_size;                 /* task_queue队列可容纳任务数上限 */

    //不关闭线程
    static bool shutdown;                       /* 标志位，线程池使用状态，true或false */
};


#endif