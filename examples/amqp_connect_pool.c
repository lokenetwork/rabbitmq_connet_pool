#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/epoll.h>
#include <zconf.h>
#include <pwd.h>
#include <limits.h>

#include "list.h"
#include "confread.h"
#include "utils.h"
#include "apue.h"

//主机名称
char const *hostname;
//rabbitmq 队列名称
char const *queue;
//父子线程domain socket
char const * fuzi_socket_path;
char  const * rabbitmq_connect_pool_server_sock;
//主机端口
int port;
//启动用户
char const *username;
char const *rabbitmq_user;
char const *rabbitmq_pwd;


//线程信息
struct threadinfo {
    pthread_t tid;
    int fd;
};

//是否开启守护进场模式
int deamon = 0;


int load_config(void){
    char exe_name[] = "/amqp_con_pool";

    char config_dir[1024] = {0};
    int n = readlink("/proc/self/exe", config_dir, 1024);
    char * exe_p = strstr(config_dir,exe_name);
    str_replace(exe_p,strlen(exe_name),"/../config.ini");
    printf("dir: %s, n is %d\n", config_dir,n);

    struct confread_file *configFile;
    struct confread_section *rootSect = 0;

    if (!(configFile = confread_open(config_dir))) {
        die("Config open failed\n");
        return -1;
    }

    rootSect = confread_find_section(configFile,"root");

    fuzi_socket_path = confread_find_value(rootSect,"fuzi_socket_path");
    hostname = confread_find_value(rootSect,"rabbitmq_server");
    port = atoi(confread_find_value(rootSect,"rabbitmq_port"));
    queue =  confread_find_value(rootSect,"rabbitmq_queue") ;
    username =  confread_find_value(rootSect,"run_user");
    rabbitmq_connect_pool_server_sock =  confread_find_value(rootSect,"rabbitmq_connect_pool_server_sock");

    rabbitmq_user =  confread_find_value(rootSect,"rabbitmq_user");
    rabbitmq_pwd =  confread_find_value(rootSect,"rabbitmq_pwd");

    printf("rabbitmq_connect_pool_server_sock is %s\n",rabbitmq_connect_pool_server_sock);

    return 0;

}

amqp_connection_state_t get_rabbitmq_connect(int * connect_broken){
    int status;
    amqp_connection_state_t conn;
    amqp_socket_t *socket = NULL;
    conn = amqp_new_connection();
    socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        *connect_broken = 0;
        syslog(LOG_ERR, "amqp_tcp_socket_new faild ");
        goto resutl;
    }

    struct timeval time_out;
    time_out.tv_sec = 5;
    time_out.tv_usec = 0;
    status = amqp_socket_open_noblock(socket, hostname, port,&time_out);

    if (status) {
        *connect_broken = 1;
        //die_on_error(amqp_destroy_connection(conn), "OPEN Ending connection");
        syslog(LOG_ERR, "amqp_socket_open_noblock faild ");
        goto resutl;
    }else{
        *connect_broken = 0;
    }

    amqp_rpc_reply_t amqp_login_status = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, rabbitmq_user, rabbitmq_pwd);
    if( AMQP_RESPONSE_NORMAL != amqp_login_status.reply_type ){
        *connect_broken = 1;
        syslog(LOG_ERR, "amqp_login faild ");
        goto resutl;
    }
    amqp_rpc_reply_t amqp_status;
    amqp_channel_open(conn, 1);
    amqp_status = amqp_get_rpc_reply(conn);
    if( AMQP_RESPONSE_NORMAL != amqp_status.reply_type ){
        *connect_broken = 1;
        syslog(LOG_ERR, "amqp_channel_open faild ");
        goto resutl;
    }

    amqp_confirm_select(conn,1);
    amqp_status  = amqp_get_rpc_reply(conn);
    if( AMQP_RESPONSE_NORMAL != amqp_status.reply_type ){
        *connect_broken = 1;
        syslog(LOG_ERR, "amqp_confirm_select faild ");
        goto resutl;
    }
    amqp_queue_bind(conn, 1, amqp_cstring_bytes(queue),
                    amqp_cstring_bytes("amq.direct"), amqp_cstring_bytes("testqueue"),
                    amqp_empty_table);
    amqp_status  = amqp_get_rpc_reply(conn);
    if( AMQP_RESPONSE_NORMAL != amqp_status.reply_type ){
        *connect_broken = 1;
        syslog(LOG_ERR, "amqp_queue_bind faild ");
        goto resutl;
    }
    resutl:
    return conn;
}

int worker(void *arg) {

    int * connect_broken = calloc(1,sizeof(int));

    amqp_connection_state_t conn = get_rabbitmq_connect(connect_broken);

    //跟父进程进行domain socket 通信。
    int fu_fd = cli_conn(fuzi_socket_path);
    printf(" fu_fd is %d --- %s\n",fu_fd,(char *)arg);

    int NEVENTS = 10240;
    struct epoll_event ev, ev_ret[NEVENTS];
    //死循环监听各种套接字
    int epfd = epoll_create(NEVENTS);
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fu_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fu_fd, &ev) != 0) {
        die("epoll_ctl fail");
        return 1;
    }

    int * cgi_fd = calloc(1,sizeof(int));


    for (;;) {
        int nfds = epoll_wait(epfd, ev_ret, NEVENTS, -1);
        if (nfds < 0) {
            die(" epoll_wait fail %d",nfds);
        }

        int i;
        for (i = 0; i < nfds; i++) {

            int ret = readn(fu_fd, cgi_fd, sizeof(int));
            if (ret != sizeof(int)) {
                //读取socket错误，退出循环
                syslog(LOG_ERR, "read fu_fd faild %d", ret);
                break;
            }

            int true_cgi_fd = *cgi_fd;

            printf("thread get fd %d\n", true_cgi_fd);

            int body_len;
            ret = readn(true_cgi_fd, &body_len, sizeof(int));
            printf("header_len is %d\n",ret);
            if (ret != sizeof(int)) {
                //读取socket错误，退出循环
                syslog(LOG_ERR, "read cgif data faild %d", ret);
                //关闭套接字
                close(true_cgi_fd);
                break;
            }

            char *body = calloc(1, body_len);
            ret = readn(true_cgi_fd, body, body_len);
            printf("body_len is %d\n",ret);
            if (ret != body_len ) {
                //读取socket错误，退出循环
                syslog(LOG_ERR, "read cgif data faild %d", ret);
                //关闭套接字
                close(true_cgi_fd);
                break;
            }

            printf("body finish is %s\n", body);

            int fail = 0;
            int success = 1;

            //如果链接是好的，可以发信息，如果不是 重新获取一下接
            if( 1 == *connect_broken ){
                amqp_destroy_connection(conn);
                conn = get_rabbitmq_connect(connect_broken);
            }
            //链接还是没好
            if( 1 == *connect_broken ){
                syslog(LOG_ERR, "connect_broken Publishing faild  %s ", body);
                free(body);
                int w_num = write(true_cgi_fd, &fail, sizeof(int));
                if( w_num != sizeof(int) ){
                    syslog(LOG_ERR, " write true_cgi_fd faild faild  " );
                }
                close(true_cgi_fd);
                break;
            }

            amqp_basic_properties_t props;
            props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
            props.content_type = amqp_cstring_bytes("text/plain");
            props.delivery_mode = 2;
            int publish_ret = amqp_basic_publish(conn, 1, amqp_cstring_bytes("amq.direct"),
                                                 amqp_cstring_bytes("order"), 0, 0,
                                                 &props, amqp_cstring_bytes(body));

            printf("publish_ret is %d\n",publish_ret);


            //第一次push失败就重新建立链接
            if (publish_ret < 0) {
                syslog(LOG_ERR, "First Publishing faild  %s %s", body,
                       amqp_error_string2(publish_ret));
                amqp_destroy_connection(conn);

                //再次获取一个新的rabbitmq链接
                conn = get_rabbitmq_connect(connect_broken);
                //如果获取不到链接
                if( 1 == *connect_broken ){
                    syslog(LOG_ERR, "connect_broken Publishing faild  %s ", body);
                    free(body);
                    int w_num = write(true_cgi_fd, &fail, sizeof(int));
                    if( w_num != sizeof(int) ){
                        syslog(LOG_ERR, " write true_cgi_fd faild faild  " );
                    }
                    close(true_cgi_fd);
                    break;
                }

                publish_ret = amqp_basic_publish(conn, 1, amqp_cstring_bytes("amq.direct"),
                                                 amqp_cstring_bytes("order"), 0, 0,
                                                 &props, amqp_cstring_bytes(body));
            };

            if (publish_ret < 0) {
                syslog(LOG_ERR, "Second Publishing faild  %s %s", body,
                       amqp_error_string2(publish_ret));
                //返回给PHP端失败
                int w_num = write(true_cgi_fd, &fail, sizeof(int));
                if( w_num != sizeof(int) ){
                    syslog(LOG_ERR, " write true_cgi_fd faild faild  " );
                }
            } else {
                amqp_frame_t frame;
                struct timeval time_out;
                time_out.tv_sec = 5;
                time_out.tv_usec = 0;
                int ack_res = amqp_simple_wait_frame_noblock(conn, &frame,&time_out);
                printf("ack_res is %d %s\n",ack_res, amqp_error_string2(ack_res));

                if( AMQP_STATUS_OK == ack_res && frame.payload.method.id == AMQP_BASIC_ACK_METHOD ){

                    // Message successfully delivered
                    //返回给PHP端成功
                    int w_num = write(true_cgi_fd, &success, sizeof(int));
                    if( w_num != sizeof(int) ){
                        syslog(LOG_ERR, " write true_cgi_fd success faild  " );
                    }

                    //超时了，链接可能断了，重新链接，再次发信息
                }else{
                    amqp_destroy_connection(conn);
                    //再次获取一个新的rabbitmq链接
                    conn = get_rabbitmq_connect(connect_broken);
                    //如果获取不到链接
                    if( 1 == *connect_broken  ){
                        syslog(LOG_ERR, "connect_broken Publishing faild  %s ", body);
                        free(body);
                        int w_num = write(true_cgi_fd, &fail, sizeof(int));
                        if( w_num != sizeof(int) ){
                            syslog(LOG_ERR, " write true_cgi_fd faild faild  " );
                        }
                        close(true_cgi_fd);
                        break;
                    }
                    publish_ret = amqp_basic_publish(conn, 1, amqp_cstring_bytes("amq.direct"),
                                                     amqp_cstring_bytes("order"), 0, 0,
                                                     &props, amqp_cstring_bytes(body));

                    if (publish_ret < 0) {
                        syslog(LOG_ERR, "NEXT Publishing faild  %s %s", body,
                               amqp_error_string2(publish_ret));
                        //返回给PHP端失败
                        int w_num = write(true_cgi_fd, &fail, sizeof(int));
                        if( w_num != sizeof(int) ){
                            syslog(LOG_ERR, " write true_cgi_fd faild faild  " );
                        }
                    }else{
                        ack_res = amqp_simple_wait_frame_noblock(conn, &frame,&time_out);
                        printf("ack_res is %d %s\n",ack_res, amqp_error_string2(ack_res));

                        if( AMQP_STATUS_OK == ack_res && frame.payload.method.id == AMQP_BASIC_ACK_METHOD ){
                            //返回给PHP端成功
                            int w_num = write(true_cgi_fd, &success, sizeof(int));
                            if( w_num != sizeof(int) ){
                                syslog(LOG_ERR, " write true_cgi_fd success faild  " );
                            }

                        }else{
                            syslog(LOG_ERR, "NEXT ACK CONFIRM faild  %s %s", body, amqp_error_string2(ack_res));
                            //返回给PHP端失败
                            int w_num = write(true_cgi_fd, &fail, sizeof(int));
                            if( w_num != sizeof(int) ){
                                syslog(LOG_ERR, " write true_cgi_fd faild faild  " );
                            }
                        }
                    }

                }
            }
            free(body);
            close(true_cgi_fd);
        }
    }

    free(cgi_fd);
    free(connect_broken);
}

int change_user(void){
    struct passwd * pw;
    pw = getpwnam(username);
    int set_uid_res = setuid(pw->pw_uid);
    if( set_uid_res < 0 ){
        die("setuid fail %d ",set_uid_res);
    }
    return set_uid_res;
}

int main(int argc, char *argv[]) {


    printf("cdcdcdcdc\n");
    //读取命令行参数
    int opt;
    char *string = "d";
    while ((opt = getopt(argc, argv, string))!= -1)
    {
        if( opt == 'd' ){
            deamon = 1;
        }
    }

    if (deamon){
        char				*cmd;
        if ((cmd = strrchr(argv[0], '/')) == NULL)
            cmd = argv[0];
        else
            cmd++;
        /*
         * Become a daemon.
         */
        daemonize(cmd);
        extern int already_running(void);
        /*
         * Make sure only one copy of the daemon is running.
         */
        if (already_running()) {
            syslog(LOG_ERR, "daemon already running");
            exit(1);
        }
    }else{
        umask(S_IXUSR|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
    }


    load_config();

    change_user();

    //忽略管道信号
    signal(SIGPIPE, SIG_IGN);

    //要开启的进程数量
    int THREAD_NUM = 1;
    int i, err;
    struct threadinfo	ti[THREAD_NUM];

    //创建 第一个 domain socket，用于父子进程通信。
    int fuzi_listenfd = serv_listen(fuzi_socket_path);
    if (fuzi_listenfd < 0) {
        die("serv_listen faile %d",fuzi_listenfd);
    };

    //先循环 创建线程建立链接，跟各个子进场都建立好通信。
    for (i = 0; i < THREAD_NUM; i++) {
        if ((err = pthread_create(&(ti[i].tid), NULL, (void *)worker,NULL)) != 0) {
            die("pthread_create");
        }
        int clifd;
        uid_t			uid;
        if ((clifd = serv_accept(fuzi_listenfd, &uid)) < 0){
            die("serv_accept error: %d", clifd);
        }
        ti[i].fd = clifd;
    }

    //创建 第一个 domain socket，用于php cgi 进程通信。
    int cgi_listenfd = serv_listen(rabbitmq_connect_pool_server_sock);
    if (cgi_listenfd < 0) {
        die("rabbitmq_connect_pool_server_socket_create");
    };

    //当前轮训到第几个线程。
    int current_thread = 0;

    for (;;){
        int cgi_clifd;
        uid_t uid;
        if ((cgi_clifd = serv_accept(cgi_listenfd, &uid)) < 0){
            syslog(LOG_ERR, "serv_cgi_accept error: %d", cgi_clifd);
        }
        printf("php_cgi_fd is %d\n",cgi_clifd);

        int ret = write(ti[current_thread].fd,&cgi_clifd, sizeof(int));
        if( ret < 0 ){
            syslog(LOG_ERR, "trans fd error: %d", ret);
        }
        current_thread++;
        if( current_thread >= THREAD_NUM ){
            current_thread = 0;
        }
    }


    return 0;
}


