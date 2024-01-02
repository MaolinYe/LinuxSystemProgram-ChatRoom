#ifndef SYSTEMPROGRAM_MESSAGE_H
#define SYSTEMPROGRAM_MESSAGE_H
#define BuffSize 256
#define BuffMiniSize 16
#define Config  "/home/yemaolin2021155015/server_config.ini"
char REG_FIFO[BuffSize];
char LOGIN_FIFO[BuffSize];
char MSG_FIFO[BuffSize];
char LOGOUT_FIFO[BuffSize];
char LOGFILES[BuffSize];
int userNumber = 0; // 总注册用户量
int userOnline = 0; // 当前在线用户
typedef struct {
    char fifo[BuffSize]; //client's FIFO name
    char username[BuffMiniSize];
    char password[BuffMiniSize];
    short loginCount; // 用户在线终端登录数
} User;
typedef struct {
    char sender[BuffSize]; // name
    char receiver[BuffSize]; // name
    char message[BuffSize];
} Chat;
typedef struct { // 服务器响应消息
    int ok;
    char message[BuffSize];
} Response;
struct OfflineMSG {
    char sender[BuffMiniSize]; // name
    char receiver[BuffMiniSize]; // name
    char message[BuffSize];
    struct OfflineMSG *next;
};
typedef struct OfflineMSG OfflineMSG;
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
#endif //SYSTEMPROGRAM_MESSAGE_H
