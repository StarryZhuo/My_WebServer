#include "../src/ThreadPool.h"
#include <unistd.h>

/* 线程池中的线程，模拟处理业务 */
void process(void *arg)
{
    printf("thread 0x%x working on task %d\n ",(unsigned int)pthread_self(),*(int *)arg);
    sleep(1);                           //模拟 小---大写
    printf("task %d is end\n",*(int *)arg);

    return;
}

int main()
{
    ThreadPool pool;
    int a = pool.threadpool_create(3, 100, 100);  /*创建线程池，池里最小3个线程，最大100，队列最大100*/
    printf("pool inited");

    //int *num = (int *)malloc(sizeof(int)*20);
    int num[20];
    num[0] = 1;
    //process((void *)&num[0]);
    for (int i = 0; i < 20; i++) {
        num[i] = i;
        printf("add task %d\n",i);
        int b = pool.threadpool_add(process, (void *)&num[i]);   /* 向线程池中添加任务 */
    }

    // while(1) {;
    // }                                          /* 等子线程完成任务 */
    sleep(10);
    int c = pool.threadpool_destroy();

    return 0;
}

