#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define POOLSIZE 4

typedef struct {
    void (*function)(void *); // 函数指针，表示任务的函数
    void *argument;          // 函数参数
} Task;

typedef struct {
    Task *tasks;            // 任务数组
    int size;                 // 当前任务数量
    int front;                // 队头索引
    int rear;                 // 队尾索引
    pthread_mutex_t mutex;    // 互斥锁
    pthread_cond_t condition; // 条件变量
    pthread_t *threads;       // 线程数组
    int shutdown;          // 是否销毁线程池
} ThreadPool;

// 初始化线程池
void init_thread_pool(ThreadPool *threadPool);

// 销毁线程池
void shutdown_thread_pool(ThreadPool *threadPool);

// 向线程池中添加任务
void submit_task(ThreadPool *threadPool, void (*function)(void *), void *argument);

// 执行任务的线程函数
void *execute(void *arg);

void *f() {
    printf("1\n");
    return NULL;
}

int main() {
    ThreadPool threadPool;
    init_thread_pool(&threadPool);
    // 添加一些任务到线程池
    for (int i = 0; i < 10; ++i) {
        submit_task(&threadPool, (void (*)(void *)) &f, NULL);
    }
    // 等待任务执行完毕
    shutdown_thread_pool(&threadPool);
    return 0;
}

void init_thread_pool(ThreadPool *threadPool) {
    threadPool->tasks = (Task *) malloc(sizeof(Task) * POOLSIZE);
    threadPool->size = 0;
    threadPool->front = 0;
    threadPool->rear = 0;
    threadPool->shutdown=-1;
    pthread_mutex_init(&threadPool->mutex, NULL);
    pthread_cond_init(&threadPool->condition, NULL);
    threadPool->threads = (pthread_t *) malloc(sizeof(pthread_t) * POOLSIZE);
    for (int i = 0; i < POOLSIZE; ++i) {
        pthread_create(&threadPool->threads[i], NULL, execute, threadPool);
    }
}

void shutdown_thread_pool(ThreadPool *threadPool) {
    threadPool->shutdown=0;
    for (int i = 0; i < POOLSIZE; ++i) {
        pthread_join(threadPool->threads[i], NULL);
    }
    free(threadPool->threads);
    free(threadPool->tasks);
    pthread_mutex_destroy(&threadPool->mutex);
    pthread_cond_destroy(&threadPool->condition);
}

void submit_task(ThreadPool *threadPool, void (*function)(void *), void *argument) {
    pthread_mutex_lock(&threadPool->mutex);
    while (threadPool->size == POOLSIZE) {    // 等待直到有空闲位置
        pthread_cond_wait(&threadPool->condition, &threadPool->mutex);
    }
    // 添加任务到队列
    threadPool->tasks[threadPool->rear].function = function;
    threadPool->tasks[threadPool->rear].argument = argument;
    threadPool->rear = (threadPool->rear + 1) % POOLSIZE;
    threadPool->size++;
    // 通知线程有新任务
    pthread_cond_signal(&threadPool->condition);
    pthread_mutex_unlock(&threadPool->mutex);
}

void *execute(void *arg) {
    ThreadPool *threadPool = (ThreadPool *) arg;
    while(threadPool->shutdown){
        pthread_mutex_lock(&threadPool->mutex);
        while (threadPool->size == 0) {    // 等待直到有任务
            pthread_cond_wait(&threadPool->condition, &threadPool->mutex);
        }
        // 取出任务
        Task task = threadPool->tasks[threadPool->front];
        threadPool->front = (threadPool->front + 1) % POOLSIZE;
        --threadPool->size;
        // 执行任务
        pthread_cond_signal(&threadPool->condition);
        pthread_mutex_unlock(&threadPool->mutex);
        task.function(task.argument);
        free(task.argument);
    }
    return NULL;
}
