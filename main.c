#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define POOLSIZE 4

typedef struct {
    void (*function)(void *); // ����ָ�룬��ʾ����ĺ���
    void *argument;          // ��������
} Task;

typedef struct {
    Task *tasks;            // ��������
    int size;                 // ��ǰ��������
    int front;                // ��ͷ����
    int rear;                 // ��β����
    pthread_mutex_t mutex;    // ������
    pthread_cond_t condition; // ��������
    pthread_t *threads;       // �߳�����
    int shutdown;          // �Ƿ������̳߳�
} ThreadPool;

// ��ʼ���̳߳�
void init_thread_pool(ThreadPool *threadPool);

// �����̳߳�
void shutdown_thread_pool(ThreadPool *threadPool);

// ���̳߳����������
void submit_task(ThreadPool *threadPool, void (*function)(void *), void *argument);

// ִ��������̺߳���
void *execute(void *arg);

void *f() {
    printf("1\n");
    return NULL;
}

int main() {
    ThreadPool threadPool;
    init_thread_pool(&threadPool);
    // ���һЩ�����̳߳�
    for (int i = 0; i < 10; ++i) {
        submit_task(&threadPool, (void (*)(void *)) &f, NULL);
    }
    // �ȴ�����ִ�����
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
    while (threadPool->size == POOLSIZE) {    // �ȴ�ֱ���п���λ��
        pthread_cond_wait(&threadPool->condition, &threadPool->mutex);
    }
    // ������񵽶���
    threadPool->tasks[threadPool->rear].function = function;
    threadPool->tasks[threadPool->rear].argument = argument;
    threadPool->rear = (threadPool->rear + 1) % POOLSIZE;
    threadPool->size++;
    // ֪ͨ�߳���������
    pthread_cond_signal(&threadPool->condition);
    pthread_mutex_unlock(&threadPool->mutex);
}

void *execute(void *arg) {
    ThreadPool *threadPool = (ThreadPool *) arg;
    while(threadPool->shutdown){
        pthread_mutex_lock(&threadPool->mutex);
        while (threadPool->size == 0) {    // �ȴ�ֱ��������
            pthread_cond_wait(&threadPool->condition, &threadPool->mutex);
        }
        // ȡ������
        Task task = threadPool->tasks[threadPool->front];
        threadPool->front = (threadPool->front + 1) % POOLSIZE;
        --threadPool->size;
        // ִ������
        pthread_cond_signal(&threadPool->condition);
        pthread_mutex_unlock(&threadPool->mutex);
        task.function(task.argument);
        free(task.argument);
    }
    return NULL;
}
