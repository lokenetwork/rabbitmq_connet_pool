/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2012-2013
 * Alan Antonuk. All Rights Reserved.
 *
 * Portions created by VMware are Copyright (c) 2007-2012 VMware, Inc.
 * All Rights Reserved.
 *
 * Portions created by Tony Garnock-Jones are Copyright (c) 2009-2010
 * VMware, Inc. and Tony Garnock-Jones. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ***** END LICENSE BLOCK *****
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>

#include <curl/curl.h>
#include <assert.h>
#include <time.h>
#include <zconf.h>
#include <syslog.h>
#include "utils.h"
#include "confread.h"
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
//推送接口
char const *http_api;
//是否开启守护进场模式
int deamon = 0;



int load_config(void){
    char exe_name[] = "/amqp_consumer";

    char config_dir[1024] = {0};
    int n = readlink("/proc/self/exe", config_dir, 1024);
    char * exe_p = strstr(config_dir,exe_name);
    str_replace(exe_p,strlen(exe_name),"/../config.ini");
    printf("dir: %s -- n is %d\n", config_dir,n);

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

    http_api =  confread_find_value(rootSect,"http_api");

    printf("http_api is %s\n",http_api);


    return 0;

}


int get_rabbitmq_connect( amqp_connection_state_t * return_conn){
    int status,fail=-1,success=1;
    amqp_socket_t *socket = NULL;
    amqp_connection_state_t  conn;
    conn = amqp_new_connection();
    socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        return fail;
    }

    struct timeval time_out;
    time_out.tv_sec = 5;
    time_out.tv_usec = 0;
    status = amqp_socket_open_noblock(socket, hostname, port,&time_out);

    if (status) {
        //die_on_error(amqp_destroy_connection(conn), "OPEN Ending connection");
        return fail;
    }

    amqp_rpc_reply_t amqp_login_status = amqp_login(conn, "/", 0, 131072, 5, AMQP_SASL_METHOD_PLAIN, rabbitmq_user, rabbitmq_pwd);
    if( AMQP_RESPONSE_NORMAL != amqp_login_status.reply_type ){
        return fail;
    }
    amqp_rpc_reply_t amqp_status;
    amqp_channel_open(conn, 1);
    amqp_status = amqp_get_rpc_reply(conn);
    if( AMQP_RESPONSE_NORMAL != amqp_status.reply_type ){
        return fail;
    }

    amqp_confirm_select(conn,1);
    amqp_status  = amqp_get_rpc_reply(conn);
    if( AMQP_RESPONSE_NORMAL != amqp_status.reply_type ){
        return fail;
    }
    amqp_queue_bind(conn, 1, amqp_cstring_bytes(queue),
                    amqp_cstring_bytes("amq.direct"), amqp_cstring_bytes("testqueue"),
                    amqp_empty_table);
    amqp_status  = amqp_get_rpc_reply(conn);
    if( AMQP_RESPONSE_NORMAL != amqp_status.reply_type ){
        return fail;
    }

    amqp_basic_consume(conn, 1, amqp_cstring_bytes(queue), amqp_empty_bytes, 0, 0, 0,
                       amqp_empty_table);
    amqp_status  = amqp_get_rpc_reply(conn);
    if( AMQP_RESPONSE_NORMAL != amqp_status.reply_type ){
        return fail;
    }
    *return_conn = conn;
    return success;
}


size_t save_http_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    strcat(stream, (char *)ptr);
    return size*nmemb;
}

char *  send_post(char const *url, char *data) {
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    char * curl_res = calloc(1,100);
    if (curl) {
        //www.baidu.com/#wd=java
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, save_http_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, curl_res);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        printf("curl_res is %s --  res is %d \n",curl_res,res);
    }
    return curl_res;
}


static void run(amqp_connection_state_t conn) {
  amqp_frame_t frame;

  for (;;) {
    amqp_rpc_reply_t ret;
    amqp_envelope_t envelope;

    amqp_maybe_release_buffers(conn);
    ret = amqp_consume_message(conn, &envelope, NULL, 0);

    if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
      if (AMQP_RESPONSE_LIBRARY_EXCEPTION == ret.reply_type &&
          AMQP_STATUS_UNEXPECTED_STATE == ret.library_error) {
        if (AMQP_STATUS_OK != amqp_simple_wait_frame(conn, &frame)) {
          return;
        }

        if (AMQP_FRAME_METHOD == frame.frame_type) {
          switch (frame.payload.method.id) {
            case AMQP_BASIC_ACK_METHOD:
              /* if we've turned publisher confirms on, and we've published a
               * message here is a message being confirmed.
               */
              break;
            case AMQP_BASIC_RETURN_METHOD:
              /* if a published message couldn't be routed and the mandatory
               * flag was set this is what would be returned. The message then
               * needs to be read.
               */
              {
                amqp_message_t message;
                ret = amqp_read_message(conn, frame.channel, &message, 0);
                if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
                  return;
                }

                amqp_destroy_message(&message);
              }

              break;

            case AMQP_CHANNEL_CLOSE_METHOD:
              /* a channel.close method happens when a channel exception occurs,
               * this can happen by publishing to an exchange that doesn't exist
               * for example.
               *
               * In this case you would need to open another channel redeclare
               * any queues that were declared auto-delete, and restart any
               * consumers that were attached to the previous channel.
               */
              return;

            case AMQP_CONNECTION_CLOSE_METHOD:
              /* a connection.close method happens when a connection exception
               * occurs, this can happen by trying to use a channel that isn't
               * open for example.
               *
               * In this case the whole connection must be restarted.
               */
              return;

            default:
              fprintf(stderr, "An unexpected method was received %u\n",
                      frame.payload.method.id);
              return;
          }
        }
      }else if(AMQP_RESPONSE_LIBRARY_EXCEPTION == ret.reply_type &&
                  (AMQP_STATUS_HEARTBEAT_TIMEOUT == ret.library_error || AMQP_STATUS_SOCKET_CLOSED == ret.library_error)
              ){
          printf(" 断线了准备重连 \n");

          //断线了准备重连
          amqp_destroy_connection(conn);

          //死循环不断链接 rabbitmq
          for (;;) {
              printf(" 重连 start \n");
              if(get_rabbitmq_connect(&conn) > 0){
                  printf(" 重新链接rabbitmq成功 \n");
                  syslog(LOG_INFO, "重新链接rabbitmq成功");
                  break;
              }else{
                  printf(" 重新链接rabbitmq失败 \n");
                  syslog(LOG_ERR, "重新链接rabbitmq失败");
              };
              sleep(5);
          }

      }

    } else {

        char * body = calloc(1,envelope.message.body.len+1+5);
        strcpy(body,"data=");
        memcpy(body+5,envelope.message.body.bytes,envelope.message.body.len);

        //处理业务逻辑,把请求转发给 http api 处理,收到回复之后确认ack
        printf("message is %s\n",body);

        char * http_res = send_post(http_api,body);
        if( NULL != strstr(http_res,"success") ){
            printf("here 1\n");
            amqp_basic_ack(conn,envelope.channel,envelope.delivery_tag,0);
        }else{
            printf("here 2\n");
            amqp_basic_nack(conn,envelope.channel,envelope.delivery_tag,0,1);
        }

        amqp_destroy_envelope(&envelope);
        free(body);
        free(http_res);

    }

  }
}


int main(int argc, char *argv[]) {

    //开启10个进程
    int thread_num = 10;
    int t_i;
    for ( t_i = 0; t_i < thread_num; ++t_i) {
        pid_t p1 = fork();
        if( p1 == 0 )
        {
            printf("in child 1, pid = %d\n", getpid());
            break;
        }
    }

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

    }else{
        umask(S_IXUSR|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
    }

    load_config();

    amqp_connection_state_t  conn;
    if( get_rabbitmq_connect(&conn) < 0 ){
        die("connect rabbitmq fail");
    };

    run(conn);

    die_on_amqp_error(amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS),
                      "Closing channel");
    die_on_amqp_error(amqp_connection_close(conn, AMQP_REPLY_SUCCESS),
                      "Closing connection");
    die_on_error(amqp_destroy_connection(conn), "Ending connection");

    return 0;
}
