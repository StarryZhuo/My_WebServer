#include "ThreadPool.h"
#include <iostream>
#include <unistd.h>
#include <signal.h>

pthread_mutex_t ThreadPool::lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ThreadPool::thread_counter = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ThreadPool::queue_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t ThreadPool::queue_not_empty = PTHREAD_COND_INITIALIZER;

std::vector<pthread_t> ThreadPool::threads;
pthread_t ThreadPool::adjust_tid;
std::vector<ThreadPoolTask> ThreadPool::task_queue;

int ThreadPool::queue_front = 0;
int ThreadPool::queue_rear = 0;
int ThreadPool::queue_size = 0;
int ThreadPool::queue_max_size = 65535;

bool ThreadPool::shutdown = false;
int ThreadPool::min_thr_num = 0;
int ThreadPool::max_thr_num = 1024; 
int ThreadPool::wait_exit_thr_num = 0;
int ThreadPool::busy_thr_num = 0;
int ThreadPool::live_thr_num =0; 

int ThreadPool::threadpool_create(int _Min_thr_num, int _Max_thr_num, int _Queue_max_size) {
    
    do {
        if(_Min_thr_num > _Max_thr_num || _Min_thr_num < 0 || _Max_thr_num > MAX_THREADS || _Queue_max_size <= 0 
            || _Queue_max_size > MAX_QUEUE) {
            // std::cout << "线程池指定参数出错" << std::endl;
            break;
        }
        min_thr_num = _Min_thr_num;
        max_thr_num = _Max_thr_num;
        queue_max_size = _Queue_max_size;

        threads.resize(max_thr_num);
        task_queue.resize(queue_max_size);

        live_thr_num = min_thr_num;
        
        /* 启动 min_thr_num 个 work thread */
        for(int i = 0; i < _Min_thr_num; i++) {
            pthread_create(&threads[i], NULL, threadpool_thread, (void *)(0));
            // std::cout << "start thread" << (unsigned int)threads[i] << std::endl;
        }

        /* 创建管理者线程 */
        // std::cout << "create adjust_thread" << std::endl;
        if(int ret = pthread_create(&adjust_tid, NULL, adjust_thread, (void *)(0)) != 0) {
            std::cout << "adjust_thread create error :" << ret << std::endl;
           break;
        }
        return 0;       
    } while(0);

    return -1;
}
    //static  *threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);

int ThreadPool::threadpool_add(std::function<void(void *)> function, void *args) {
    pthread_mutex_lock(&lock);

    //线程池没有关闭且队列满
    while((queue_size == queue_max_size) && !shutdown) {
        pthread_cond_wait(&queue_not_full, &lock);
    }

    if(shutdown) {
        pthread_cond_broadcast(&queue_not_empty);
        pthread_mutex_unlock(&lock);
        return -1;
    }

    //任务队列之前的清理干净
    if(task_queue[queue_rear].args != NULL) {
        task_queue[queue_rear].args = NULL;
    }

    //添加任务到任务队列
    task_queue[queue_rear].fun = function;
    task_queue[queue_rear].args = args;
    
    queue_rear = (queue_rear + 1) % queue_max_size; /* 队尾指针移动, 模拟环形 */
    queue_size++;

    /*添加完任务后，队列不为空，唤醒线程池中 等待处理任务的线程*/
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&lock);
    
    // std::cout << "add_finish" << std::endl;
    return 0;
}

void * ThreadPool::threadpool_thread(void *args) {
    ThreadPoolTask Task;
    while(true) {
        pthread_mutex_lock(&lock);

        //判断任务列表有无任务
        while((queue_size == 0) && (!shutdown)) {
            pthread_cond_wait(&queue_not_empty, &lock);
            
            //处理管理线程的销毁线程
            if(wait_exit_thr_num  > 0) {
                wait_exit_thr_num--;
                // std::cout << "thread:" << pthread_self() << "exit";
                live_thr_num --;

                pthread_mutex_unlock(&lock);
                pthread_detach(pthread_self());
                pthread_exit(NULL);
            }
        }

        if(shutdown) {
            pthread_mutex_unlock(&lock);
            // pthread_detach(pthread_self());
            pthread_exit(NULL);
        }
        //任务出队操作
        Task = task_queue[queue_front];
        queue_front = (queue_front + 1) % queue_max_size;
        queue_size --;

        //通知任务队列可以加任务
        pthread_cond_broadcast(&queue_not_full);

        //任务取出后将任务队列释放
        pthread_mutex_unlock(&lock);

        pthread_mutex_lock(&lock);
        // std::cout << "pthread: 0x%x" << (unsigned int)pthread_self() << " start work" << std::endl;
        pthread_mutex_unlock(&lock);

        pthread_mutex_lock(& thread_counter);   /*忙状态线程数变量琐*/
        busy_thr_num ++;                        /*忙状态线程数+1*/
        pthread_mutex_unlock(& thread_counter);

        //执行任务
        Task.fun(Task.args);

        //任务处理结束后，忙线程减一
        pthread_mutex_lock(& thread_counter);   /*忙状态线程数变量琐*/
        busy_thr_num --;                        /*忙状态线程数-1*/
        pthread_mutex_unlock(& thread_counter);
        // std::cout << "pthread: 0x%x" << (unsigned int)pthread_self() << " finish work" << std::endl;
    }

    pthread_exit(NULL);
    return NULL;
}

bool is_thread_alive(pthread_t tid) {
    int kill_rt = pthread_kill(tid, 0);     //发0号信号，测试线程是否存活
    if (kill_rt == ESRCH) {
        return false;
    }
    return true;
}

void * ThreadPool::adjust_thread(void *args) {
    while(!shutdown) {
        sleep(DEFUAL_TIME);

        pthread_mutex_lock(&lock);
        /* 创建新线程 任务数大于最小线程池个数, 且存活的线程数少于最大线程个数时*/
        if(queue_size > min_thr_num && live_thr_num < max_thr_num) {
            
            for(int i = 0, j = 0; i < max_thr_num && j < DEFAULT_THREADS_OPR; i++) {
                if(threads[i] == 0 || is_thread_alive(threads[i])) {
                    pthread_create(&threads[i], NULL, threadpool_thread, (void *)(0));
                    j++;
                    live_thr_num++;
                }
            }
        }
        pthread_mutex_unlock(&lock);

        /* 销毁多余的空闲线程 忙线程X2 小于 存活的线程数 且 存活的线程数 大于 最小线程数时*/
        pthread_mutex_lock(&lock);
        if((busy_thr_num * 2) < live_thr_num && live_thr_num > min_thr_num){
            wait_exit_thr_num = DEFAULT_THREADS_OPR;
        }
        pthread_mutex_unlock(&lock);

        /* 通知处在空闲状态的线程, 他们会自行终止*/
        for(int i = 0; i < DEFAULT_THREADS_OPR; i++) {
            pthread_cond_signal(& queue_not_empty);
        }
    }
    pthread_exit(NULL);
}

int ThreadPool::threadpool_destroy() {
    std::cout << "destroy ThreadPool" << std::endl;

    pthread_mutex_lock(&lock);
    shutdown = true;
    pthread_mutex_unlock(&lock);

    /*先销毁管理线程*/
    pthread_join(adjust_tid, NULL);

    pthread_cond_broadcast(&queue_not_empty);

    for (int i = 0; i < live_thr_num; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_mutex_lock(&lock);
    pthread_mutex_destroy(&lock);
    pthread_mutex_lock(&thread_counter);
    pthread_mutex_destroy(&thread_counter);
    pthread_cond_destroy(&queue_not_empty);
    pthread_cond_destroy(&queue_not_full);

    // std::cout << "delete this" << std::endl;
    return 0;
}

// int ThreadPool::threadpool_free() {

// }

// ThreadPool::~ThreadPool() {

// }