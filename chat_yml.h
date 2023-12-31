#ifndef SYSTEMPROGRAM_MESSAGE_H
#define SYSTEMPROGRAM_MESSAGE_H
#define BuffSize 128
#define BuffMiniSize 16
#define Config  "server_config.ini"
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
#endif //SYSTEMPROGRAM_MESSAGE_H
