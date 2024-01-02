# Linux系统编程-多人即时聊天系统
基于openEuler系统调用，支持多用户注册、登录和注销、一对一发信息、一对多发信息、离线消息发送，支持用户独立日志记录。
## master分支-基础版
IO多路复用，支持多用户注册、登录和注销、一对一发信息、离线消息发送。
## multithread分支-多线程版
在IO多路复用基础上，分别启动多个线程处理用户的注册请求、登录请求、聊天请求和退出请求
## daemon分支-守护进程版
服务器升级为多线程的守护进程
## threadPool分支-线程池版
服务器升级为线程池的守护进程版本，C语言标准库中并没有提供线程池的实现，这里手搓了一个线程池
