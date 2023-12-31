#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "chat_yml.h"
#include <sys/select.h>
#include <time.h>
#include "minIni.h"

int MAX_LOG_PERUSER;
int MAX_ONLINE_USERS;
User users[BuffMiniSize]; // 最大注册用户量
OfflineMSG *offlineMsgs = NULL; // 离线消息链表头节点
void init() {
    offlineMsgs = malloc(sizeof(OfflineMSG));
    if (ini_gets("FIFO", "REG_FIFO", "", REG_FIFO, BuffSize, Config) == 0 ||
        ini_gets("FIFO", "LOGIN_FIFO", "", LOGIN_FIFO, BuffSize, Config) == 0 ||
        ini_gets("FIFO", "MSG_FIFO", "", MSG_FIFO, BuffSize, Config) == 0 ||
        ini_gets("FIFO", "LOGOUT_FIFO", "", LOGOUT_FIFO, BuffSize, Config) == 0 ||
        (MAX_ONLINE_USERS = ini_getl("SERVER", "MAX_ONLINE_USERS", 0, Config)) == 0 ||
        (MAX_LOG_PERUSER = ini_getl("SERVER", "MAX_LOG_PERUSER", 0, Config)) == 0 ||
        ini_gets("SERVER", "LOGFILES", "", LOGFILES, BuffSize, Config) == 0) {
        fprintf(stderr, "Error: failed to read configuration file %s\n", Config);
        exit(1);
    }
}

char *getUserFIFO(char *name) {
    for (int i = 0; i < userNumber; i++) {
        if (strcmp(users[i].username, name) == 0)
            return users[i].fifo;
    }
    return NULL;
}

void writeToUser(const char *fifo, const void *buffer, size_t n) {
    int fd = open(fifo, O_RDWR | O_NONBLOCK);
    write(fd, buffer, n);
}


void logger(const char *file, char *buffer) {
    char log[BuffSize];
    time_t currentTime;
    time(&currentTime);
    sprintf(log, "%s %s", buffer, ctime(&currentTime));
    printf("%s\n%s", file, log);
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
    OfflineMSG *p = offlineMsgs;
    while (p) {
        if (strcmp(p->receiver, receiver) == 0) {
            writeToUser(getUserFIFO(receiver), p->message, BuffSize);
            char logFile[BuffSize], log[BuffSize]; // 准备写日志
            sprintf(log, "[Chat] Sender: %s Receiver: %s True", p->sender, receiver);
            sprintf(logFile, "%s%s", LOGFILES, p->sender);
            logger(logFile, log);
            deleteOfflineMSG(p);
            return;
        }
        p = p->next;
    }
}

void logoutHandler(User *user) {
    user->loginCount--;
    userOnline--;
    char message[BuffSize];
    sprintf(message, "Logout succeed!");
    writeToUser(user->fifo, message, BuffSize);
    char logFile[BuffSize], log[BuffSize]; // 准备写日志
    sprintf(log, "[Logout] %s", user->username);
    sprintf(logFile, "%s%s", LOGFILES, user->username);
    logger(logFile, log);
}

void registerHandler(User *user) {
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
        sprintf(log, "[Register] %s", user->username);
        sprintf(logFile, "%s%s", LOGFILES, user->username);
        logger(logFile, log);
    }
    writeToUser(user->fifo, message, BuffSize);
}

void loginHandler(User *user) {
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
                        break;
                    }
                    strcpy(users[i].fifo, user->fifo);
                    sprintf(response.message, "Login succeed!");
                    response.ok = 0;
                    userOnline++; // 在线用户数++
                    sendOfflineMSG(user->username); // 发送离线消息
                    char logFile[BuffSize], log[BuffSize]; // 准备写日志
                    sprintf(log, "[Login] %s", user->username);
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
}

void chatHandler(Chat *chat) {
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
                sprintf(log, "[Chat] Sender: %s Receiver: %s False", chat->sender, chat->receiver);
            } else {
                writeToUser(users[i].fifo, chat->message, BuffSize);
                sprintf(log, "[Chat] Sender: %s Receiver: %s True", chat->sender, chat->receiver);
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
}

int main() {
    init();
    fd_set fds, read_fds; // 文件描述符集合
    int max_fd;
    int register_fd, login_fd, chat_fd, logout_fd;
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
    printf("[Server by YeMaolin]: Listening...\n");
    while (1) {
        read_fds = fds;
        User registerData;
        User loginData;
        Chat chatData;
        User logoutData;
        // 使用select监听文件描述符
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }
        // 检查哪些文件描述符已经准备好
        if (FD_ISSET(register_fd, &read_fds)) {
            // 处理注册
            read(register_fd, &registerData, sizeof(User));
            registerHandler(&registerData);
        }
        if (FD_ISSET(login_fd, &read_fds)) {
            // 处理登录
            read(login_fd, &loginData, sizeof(User));
            loginHandler(&loginData);
        }
        if (FD_ISSET(chat_fd, &read_fds)) {
            // 处理聊天
            read(chat_fd, &chatData, sizeof(Chat));
            chatHandler(&chatData);
        }
        if (FD_ISSET(logout_fd, &read_fds)) {
            // 处理注销
            read(logout_fd, &logoutData, sizeof(User));
            logoutHandler(&logoutData);
        }
    }
}
