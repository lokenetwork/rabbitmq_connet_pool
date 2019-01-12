# rabbitmq 本地链接池 #

用 C 语言实现的rabbitmq 本地链接池,多个线程维持多个rabbitmq链接，

通过domain socket可以给PHP等程序调用，发布消息通过 domain socket 发布。

rabbitmq本地连接池 支持断线自动重连。

消息消费者端也是用C语言实现的，支持推送消息给http api接口。支持rabbitmq心跳检查，断线自动重连。

安装非常简单，cd 到 build 目录，然后执行 "cmake .." 命令，然后再执行 make install，就完成编译了。

amqp_con_pool 是 连接池程序 ，启动要进入到这个程序的目录进行启动，不要 用绝对路径启动，不然可能找不到配置文件。

amqp_amqp_consumer 是 消费者端程序 。

两个程序都是 加上 -d 选项，就可以进入守护进程模式。

注意事项：

程序依赖 curl.h ，要先到 http://curl.haxx.se/download/ 安装curl。

程序在ubuntu 14.4 经过测试。其他环境编译有可能不太兼容。
