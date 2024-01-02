#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "chat_yml.h"
#include <sys/select.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include "minIni.h"
#include <sys/param.h>
#include <sys/types.h>
#define ThreadPoolLog "/var/log/chat-logs/threads-log"
int MAX_LOG_PERUSER;
int MAX_ONLINE_USERS;
int POOLSIZE;
User users[BuffMiniSize]; // 最大注册用户量
OfflineMSG *offlineMsgs = NULL; // 离线消息链表头节点
int register_fd, login_fd, chat_fd, logout_fd;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 初始化互斥锁
void init_server() { // 读取配置文件初始化服务器
    offlineMsgs = malloc(sizeof(OfflineMSG));
    if (ini_gets("FIFO", "REG_FIFO", "", REG_FIFO, BuffSize, Config) == 0 ||
        ini_gets("FIFO", "LOGIN_FIFO", "", LOGIN_FIFO, BuffSize, Config) == 0 ||
        ini_gets("FIFO", "MSG_FIFO", "", MSG_FIFO, BuffSize, Config) == 0 ||
        ini_gets("FIFO", "LOGOUT_FIFO", "", LOGOUT_FIFO, BuffSize, Config) == 0 ||
        (MAX_ONLINE_USERS = ini_getl("SERVER", "MAX_ONLINE_USERS", 0, Config)) == 0 ||
        (MAX_LOG_PERUSER = ini_getl("SERVER", "MAX_LOG_PERUSER", 0, Config)) == 0 ||
        (POOLSIZE = ini_getl("SERVER", "POOLSIZE", 0, Config)) == 0 ||
        ini_gets("SERVER", "LOGFILES", "", LOGFILES, BuffSize, Config) == 0) {
        fprintf(stderr, "Error: failed to read configuration file %s\n", Config);
        exit(1);
    }
    // 创建或打开FIFO文件
    mkfifo(REG_FIFO, 0777);
    mkfifo(LOGIN_FIFO, 0777);
    mkfifo(MSG_FIFO, 0777);
    mkfifo(LOGOUT_FIFO, 0777);
    // 打开FIFO文件 O_RDWR 可读写 O_NONBLOCK 非阻塞
    register_fd = open(REG_FIFO, O_RDWR | O_NONBLOCK);
    login_fd = open(LOGIN_FIFO, O_RDWR | O_NONBLOCK);
    chat_fd = open(MSG_FIFO, O_RDWR | O_NONBLOCK);
    logout_fd = open(LOGOUT_FIFO, O_RDWR | O_NONBLOCK);
}

char *getUserFIFO(char *name) {
    for (int i = 0; i < userNumber; i++) {
        if (strcmp(users[i].username, name) == 0)
            return users[i].fifo;
    }
    return NULL;
}

void writeToUser(const char *fifo, const void *buffer, size_t n) {
    pthread_mutex_lock(&mutex); // 加锁
    int fd = open(fifo, O_RDWR | O_NONBLOCK);
    write(fd, buffer, n);
    pthread_mutex_unlock(&mutex); // 解锁
}

void broadcast(char *news) { // 将在线的总人数及用户名显示给所有的用户
    char buffer[BuffSize];
    sprintf(buffer, "%s  OnlineUser: %d  ", news, userOnline);
    for (int i = 0; i < userNumber; i++) {
        if (users[i].loginCount > 0) {
            strcat(buffer, users[i].username);
            strcat(buffer, " ");
        }
    }
    for (int i = 0; i < userNumber; i++) {
        if (users[i].loginCount > 0) {
            writeToUser(users[i].fifo, buffer, BuffSize);
        }
    }
}

void logger(const char *file, char *buffer) {
    pthread_mutex_lock(&mutex); // 加锁
    char log[BuffSize];
    time_t currentTime;
    time(&currentTime);
    sprintf(log, "%s  %s", buffer, ctime(&currentTime));
    int fd = open(file, O_WRONLY | O_APPEND);
    write(fd, log, strlen(log));
    close(fd);
    pthread_mutex_unlock(&mutex); // 解锁
}

void insertOfflineMSG(OfflineMSG *new) {
    new->next = offlineMsgs->next; // 直接插在头节点的后面
    offlineMsgs->next = new;
}

void deleteOfflineMSG(OfflineMSG *p) {
    OfflineMSG *prior = offlineMsgs, *current;
    while (prior->next) {
        current = prior->next;
        if (p == current)
            break;
        prior = prior->next;
    }
    prior->next = current->next;
    free(p);
}

void sendOfflineMSG(char *receiver) {
    OfflineMSG *p = offlineMsgs, *temp;
    while (p) {
        if (strcmp(p->receiver, receiver) == 0) {
            temp = p->next;
            writeToUser(getUserFIFO(receiver), p->message, BuffSize);
            char logFile[BuffSize], log[BuffSize]; // 准备写日志
            sprintf(log, "[Chat]  Sender: %s  Receiver: %s  True", p->sender, receiver);
            sprintf(logFile, "%s%s", LOGFILES, p->sender);
            logger(logFile, log);
            deleteOfflineMSG(p);
            p = temp;
        } else
            p = p->next;
    }
}

void *logoutHandler() {
    User *user = malloc(sizeof(User));
    read(logout_fd, user, sizeof(User));
    for (int i = 0; i < userNumber; i++) {
        if (strcmp(user->username, users[i].username) == 0) {
            users[i].loginCount--;
            break;
        }
    }
    userOnline--;
    char message[BuffSize];
    sprintf(message, "[Logout] %s", user->username);
    writeToUser(user->fifo, message, BuffSize);
    broadcast(message);
    char logFile[BuffSize], log[BuffSize]; // 准备写日志
    sprintf(log, "[Logout]  User: %s", user->username);
    sprintf(logFile, "%s%s", LOGFILES, user->username);
    logger(logFile, log);
    free(user);
    return NULL;
}

void *registerHandler() {
    User *user = malloc(sizeof(User));
    read(register_fd, user, sizeof(User));
    char message[BuffSize];
    int ok = 0;
    for (int i = 0; i < userNumber; i++) {
        if (strcmp(user->username, users[i].username) == 0) {
            sprintf(message, "The username has already exited!");
            ok = -1;
            break;
        }
    }
    if (ok == 0) {
        strcpy(users[userNumber].username, user->username);
        strcpy(users[userNumber].password, user->password);
        users[userNumber].loginCount = 0;
        sprintf(message, "Register succeed!");
        userNumber++;
        char logFile[BuffSize], log[BuffSize]; // 准备写日志
        sprintf(log, "[Register]  User: %s", user->username);
        sprintf(logFile, "%s%s", LOGFILES, user->username);
        int fd = open(logFile, O_CREAT | O_TRUNC); //创建日志文件
        if (fd < 0) {
            perror("Log file creation failed");
            exit(EXIT_FAILURE);
        }
        close(fd);
        if (chmod(logFile, S_IRUSR | S_IWUSR) != 0) { // 日志文件只有超级用户可以读、写
            perror("Failed to set file permissions");
            exit(EXIT_FAILURE);
        }
        logger(logFile, log);
    }
    writeToUser(user->fifo, message, BuffSize);
    free(user);
    return NULL;
}

void *loginHandler() {
    User *user = malloc(sizeof(User));
    read(login_fd, user, sizeof(User));
    Response response;
    response.ok = -1;
    if (userOnline == MAX_ONLINE_USERS) { // 在线用户数达到上限
        sprintf(response.message, "Online users number has reached the upper limit.");
    } else {
        for (int i = 0; i < userNumber; i++) {
            if (strcmp(user->username, users[i].username) == 0) {
                if (strcmp(user->password, users[i].password) == 0) {
                    if (users[i].loginCount == MAX_LOG_PERUSER) { // 用户同一时刻在终端登录数达到上限
                        sprintf(response.message, "The user has already logged in to another terminal.");
                        response.ok = -3;
                        break;
                    }
                    strcpy(users[i].fifo, user->fifo);
                    users[i].loginCount++;
                    sprintf(response.message, "[Login] %s", user->username);
                    response.ok = 0;
                    userOnline++; // 在线用户数++
                    char logFile[BuffSize], log[BuffSize]; // 准备写日志
                    sprintf(log, "[Login]  User: %s", user->username);
                    sprintf(logFile, "%s%s", LOGFILES, user->username);
                    logger(logFile, log);
                    break;
                } else {
                    sprintf(response.message, "Wrong password!");
                    response.ok = -2;
                    break;
                }
            }
        }
        if (response.ok == -1) {
            sprintf(response.message, "Wrong username!");
        }
    }
    writeToUser(user->fifo, &response, sizeof(Response));
    if (response.ok == 0) {
        broadcast(response.message);
        sendOfflineMSG(user->username); // 发送离线消息
    }
    free(user);
    return NULL;
}

void *chatHandler() {
    Chat *chat = malloc(sizeof(Chat));
    read(chat_fd, chat, sizeof(Chat));
    char message[BuffSize];
    int ok = -1;
    for (int i = 0; i < userNumber; i++) {
        if (strcmp(chat->receiver, users[i].username) == 0) {
            char logFile[BuffSize], log[BuffSize]; // 准备写日志
            if (users[i].loginCount < 1) { // 用户不在线
                OfflineMSG *p = malloc(sizeof(OfflineMSG));
                strcpy(p->sender, chat->sender);
                strcpy(p->receiver, chat->receiver);
                strcpy(p->message, chat->message);
                insertOfflineMSG(p);
                sprintf(log, "[Chat]  Sender: %s  Receiver: %s  False", chat->sender, chat->receiver);
            } else {
                writeToUser(users[i].fifo, chat->message, BuffSize);
                sprintf(log, "[Chat]  Sender: %s  Receiver: %s  True", chat->sender, chat->receiver);
            }
            sprintf(logFile, "%s%s", LOGFILES, chat->sender);
            logger(logFile, log);
            sprintf(message, "Send succeed!");
            ok = 0;
            break;
        }
    }
    if (ok == -1) {
        sprintf(message, "User %s does not exit!", chat->receiver);
    }
    writeToUser(getUserFIFO(chat->sender), message, BuffSize);
    free(chat);
    return NULL;
}

int init_daemon(void) { // 突变守护进程
    int pid;
    int i;
    /*忽略终端I/O信号，STOP信号*/
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    pid = fork();
    if (pid > 0) { exit(0); /*结束父进程，使得子进程成为后台进程*/ }
    else if (pid < 0) { return -1; }
    /*建立一个新的进程组,在这个新的进程组中,子进程成为这个进程组的首进程,以使该进程脱离所有终端*/
    setsid();
    /*再次新建一个子进程，退出父进程，保证该进程不是进程组长，同时让该进程无法再打开一个新的终端*/
    pid = fork();
    if (pid > 0) { exit(0); }
    else if (pid < 0) { return -1; }
    /*关闭所有从父进程继承的不再需要的文件描述符*/
    for (i = 0; i < NOFILE; close(i++));
    chdir("/");    /*改变工作目录，使得进程不与任何文件系统联系*/
    umask(0);    /*将文件当时创建屏蔽字设置为0*/
    signal(SIGCHLD, SIG_IGN);    /*忽略SIGCHLD信号*/
    signal(SIGTERM, SIG_IGN);    /*忽略SIGTERM信号*/
    return 0;
}

void *execute(void *arg) {
    ThreadPool *threadPool = (ThreadPool *) arg;
    while (threadPool->shutdown) {
        pthread_mutex_lock(&threadPool->mutex);
        while (threadPool->size == 0) {    // 等待直到有任务
            pthread_cond_wait(&threadPool->condition, &threadPool->mutex);
        }
        // 取出任务
        Task task = threadPool->tasks[threadPool->front];
        threadPool->front = (threadPool->front + 1) % POOLSIZE;
        threadPool->size--;
        // 执行任务
        pthread_cond_signal(&threadPool->condition);
        pthread_mutex_unlock(&threadPool->mutex);
        task.function(task.argument);
        free(task.argument);
        logger(ThreadPoolLog,"[Thread Recycle]");
    }
    return NULL;
}

void init_thread_pool(ThreadPool *threadPool) {
    int fd = open(ThreadPoolLog, O_CREAT | O_TRUNC); //创建日志文件
    if (fd < 0) {
        perror("Log file creation failed");
        exit(EXIT_FAILURE);
    }
    threadPool->tasks = (Task *) malloc(sizeof(Task) * POOLSIZE);
    threadPool->size = 0;
    threadPool->front = 0;
    threadPool->rear = 0;
    threadPool->shutdown = -1;
    pthread_mutex_init(&threadPool->mutex, NULL);
    pthread_cond_init(&threadPool->condition, NULL);
    threadPool->threads = (pthread_t *) malloc(sizeof(pthread_t) * POOLSIZE);
    for (int i = 0; i < POOLSIZE; ++i) {
        pthread_create(&threadPool->threads[i], NULL, execute, threadPool);
    }
}

void shutdown_thread_pool(ThreadPool *threadPool) { // 线程池的销毁
    threadPool->shutdown = 0;
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
    logger(ThreadPoolLog,"[Thread Dispatch]");
    pthread_cond_signal(&threadPool->condition);
    pthread_mutex_unlock(&threadPool->mutex);
}


int main() {
    init_daemon();
    init_server();
    ThreadPool threadPool;
    init_thread_pool(&threadPool);
    fd_set fds, read_fds; // 文件描述符集合
    int max_fd;
    // 指定要检查的文件描述符
    FD_ZERO(&fds);
    FD_SET(register_fd, &fds);
    FD_SET(login_fd, &fds);
    FD_SET(chat_fd, &fds);
    FD_SET(logout_fd, &fds);
    // 获取最大文件描述符
    max_fd = register_fd > login_fd ? register_fd : login_fd;
    max_fd = max_fd > chat_fd ? max_fd : chat_fd;
    max_fd = max_fd > logout_fd ? max_fd : logout_fd;
    while (1) {
        read_fds = fds;
        // 使用select监听文件描述符
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        // 检查哪些文件描述符已经准备好
        if (FD_ISSET(register_fd, &read_fds)) {
            pthread_t pthread;// 处理注册
            submit_task(&threadPool, (void (*)(void *)) &registerHandler, NULL);
        }
        if (FD_ISSET(login_fd, &read_fds)) {
            pthread_t pthread; // 处理登录
            submit_task(&threadPool, (void (*)(void *)) &loginHandler,NULL);
        }
        if (FD_ISSET(chat_fd, &read_fds)) {
            pthread_t pthread;// 处理聊天
            submit_task(&threadPool, (void (*)(void *)) &chatHandler,NULL);
        }
        if (FD_ISSET(logout_fd, &read_fds)) {
            pthread_t pthread;// 处理注销
            submit_task(&threadPool, (void (*)(void *)) &logoutHandler,NULL);
        }
        sleep(1);
    }
}
