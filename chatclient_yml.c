#include<unistd.h>
#include<stdio.h>
#include <stdlib.h>
#include<string.h>
#include<sys/stat.h>
#include<fcntl.h>
#include <signal.h>
#include"chat_yml.h"
#include"minIni.h"

char client_fifo[BuffSize];

void showPage(User *user);

void handler(int sig) {
    unlink(client_fifo);
    exit(1);
}

void init_server() {
    if (ini_gets("FIFO", "REG_FIFO", "", REG_FIFO, BuffSize, Config) == 0 ||
        ini_gets("FIFO", "LOGIN_FIFO", "", LOGIN_FIFO, BuffSize, Config) == 0 ||
        ini_gets("FIFO", "MSG_FIFO", "", MSG_FIFO, BuffSize, Config) == 0 ||
        ini_gets("FIFO", "LOGOUT_FIFO", "", LOGOUT_FIFO, BuffSize, Config) == 0) {
        fprintf(stderr, "Error: failed to read configuration file %s\n", Config);
        exit(1);
    }
}

void logout(User *user) {
    int logout_fd = open(LOGOUT_FIFO, O_RDWR | O_NONBLOCK);
    write(logout_fd, user, sizeof(User));
    int fd = open(user->fifo, O_RDWR | O_NONBLOCK);
    char message[BuffSize];
    while (1) {
        int result = read(fd, message, BuffSize);
        if (result > 0) {
            printf("%s\n", message);
            break;
        }
    }
}

void registerClient(User *user) {
    int register_fd = open(REG_FIFO, O_RDWR | O_NONBLOCK);
    write(register_fd, user, sizeof(User));
    int fd = open(user->fifo, O_RDWR | O_NONBLOCK);
    char message[BuffSize];
    while (1) {
        int result = read(fd, message, BuffSize);
        if (result > 0) {
            printf("%s\n", message);
            break;
        }
    }
    showPage(user);
}

void chatClient(User *user) {
    char what, message[BuffSize],receivers[BuffSize];
    printf("Chat Page: press r for receive | s for send | q for logout\n");
    while ((what = getchar()) == '\n') {}
    if (what == 'q') {
        logout(user);
        showPage(user);
        return;
    } else if (what == 's') { // 发送信息
        Chat chat;
        strcpy(chat.sender, user->username);
        getchar(); // 吃回车
        printf("receivers username: ");
        scanf("%[^\n]", receivers);
        getchar(); // 吃回车
        printf("send data: ");
        scanf("%[^\n]", message);
        sprintf(chat.message, "[%s]: %s", user->username, message);
        int chat_fd = open(MSG_FIFO, O_RDWR | O_NONBLOCK);
        char*receiver=strtok(receivers, " ");
        while(receiver){
            strcpy(chat.receiver,receiver);
            write(chat_fd, &chat, sizeof(Chat));
            // 等待服务器响应
            int fd = open(user->fifo, O_RDWR | O_NONBLOCK);
            while (1) {
                int result = read(fd, message, BuffSize);
                if (result > 0) {
                    printf("%s\n", message);
                    break;
                }
            }
            receiver=strtok(NULL," ");
        }
    } else if (what == 'r') { //接收消息
        int fd = open(user->fifo, O_RDWR | O_NONBLOCK);
        int result = read(fd, message, BuffSize);
        if (result > 0) {
            printf("%s\n", message);
        } else {
            printf("There is no message.\n");
        }
    } else {
        printf("Wrong press key, please try again!\n");
    }
    chatClient(user);
}

void loginClient(User *user) {
    int login_fd = open(LOGIN_FIFO, O_RDWR | O_NONBLOCK);
    write(login_fd, user, sizeof(User));
    int fd = open(user->fifo, O_RDWR | O_NONBLOCK);
    Response response;
    while (1) {
        int result = read(fd, &response, sizeof(Response));
        if (result > 0) {
            printf("%s\n", response.message);
            break;
        }
    }
    if (response.ok != 0) { // 登录失败
        showPage(user);
    } else {
        chatClient(user);
    }
}

void showPage(User *user) {
    char what;
    printf("Chat Client: press r for register | l for login | q for quit\n");
    while ((what = getchar()) == '\n') {}
    if (what == 'q') {
        exit(0);
    } else if (what == 'r' || what == 'l') {
        printf("username: ");
        scanf("%s", user->username);
        printf("password: ");
        scanf("%s", user->password);
    } else {
        printf("Wrong press key, please try again!\n");
        showPage(user);
    }
    if (what == 'r') {
        registerClient(user);
    } else if (what == 'l') {
        loginClient(user);
    }
}

int main() {
    init_server();
    User user;
    sprintf(user.fifo, "/home/yemaolin2021155015/client_fifo/client_fifo%d", getpid());
    sprintf(client_fifo, "%s", user.fifo);
    signal(SIGKILL, handler);//收到的kill进程
    signal(SIGINT, handler);//终端释放的终止信号Ctrl+C
    signal(SIGTERM, handler);// 默认终止信号，进程正常结束
    mkfifo(user.fifo, 0777);
    showPage(&user);
    exit(0);
}
