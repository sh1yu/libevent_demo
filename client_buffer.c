#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

int tcp_connect_server(const char *server_ip, int port);

void cmd_msg_cb(int fd, short events, void *arg);

// void socket_read_cb(int fd, short events, void *arg);
void server_msg_cb(struct bufferevent *bev, void *arg);

void event_cb(struct bufferevent *bev, short event, void *arg);

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("please input 2 parameter\n");
        return -1;
    }

    //两个参数依次是服务器端的IP地址、端口号
    int sockfd = tcp_connect_server(argv[1], atoi(argv[2]));
    if (sockfd == -1) {
        perror("tcp_connect error ");
        return -1;
    }

    printf("connect to server successful\n");

    struct event_base *base = event_base_new();

    // struct event *ev_sockfd = event_new(base, sockfd, EV_READ|EV_PERSIST, socket_read_cb, NULL);
    // event_add(ev_sockfd, NULL);

    // struct event* ev_cmd = event_new(base, STDIN_FILENO, EV_READ|EV_PERSIST, cmd_msg_cb, (void*)&sockfd);
    // event_add(ev_cmd, NULL);

    struct bufferevent *bev = bufferevent_socket_new(base, sockfd,
                                                     BEV_OPT_CLOSE_ON_FREE);

    //监听终端输入事件
    struct event *ev_cmd = event_new(base, STDIN_FILENO,
                                     EV_READ | EV_PERSIST, cmd_msg_cb,
                                     (void *) bev);
    event_add(ev_cmd, NULL);

    //当socket关闭时会用到ev_cmd，所以使用参数传入
    //server_msg_cb为读取回调， 由于不需要监听写入事件，所以写入回调为空
    //event_cb 其他事件回调
    bufferevent_setcb(bev, server_msg_cb, NULL, event_cb, (void *) ev_cmd);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);

    event_base_dispatch(base);

    printf("finished \n");
    return 0;
}

//键盘输入事件的回调函数。注意和非buffer版本相比，此处传入的arg不是原始的sockfd，而是bev
void cmd_msg_cb(int fd, short events, void *arg) {
    char msg[1024];

    int ret = read(fd, msg, sizeof(msg));
    if (ret < 0) {
        perror("read fail ");
        exit(1);
    }

    // int sockfd = *((int *)arg);
    // write(sockfd, msg, ret);
    struct bufferevent *bev = (struct bufferevent *) arg;

    //把终端的消息发送给服务器端
    bufferevent_write(bev, msg, ret);
}

// void socket_read_cb(int fd, short events, void *arg)
// {
//     char msg[1024];
//     int len = read(fd, msg, sizeof(msg)-1);
//     if (len <= 0)
//     {
//         perror("read fail");
//         exit(1);
//     }

//     msg[len] = '\0';
//     printf("recv %s from server\n", msg);
// }

////////////////////////
///以下是bufferevent相关回调

//bev 读取事件回调， 此处的arg为ev_cmd（当然这里没有用到）
void server_msg_cb(struct bufferevent *bev, void *arg) {
    char msg[1024];

    size_t len = bufferevent_read(bev, msg, sizeof(msg));
    msg[len] = '\0';

    printf("recv %s from server\n", msg);
}

//其他事件回调，此处的arg为ev_cmd
void event_cb(struct bufferevent *bev, short event, void *arg) {
    if (event & BEV_EVENT_EOF)
        printf("connection closed\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error\n");

    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);

    struct event *ev = (struct event *) arg;
    //因为socket已经没有，所以这个event也没有存在的必要了
    event_free(ev);
}

int tcp_connect_server(const char *server_ip, int port) {
    int sockfd, ret, save_errno;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    ret = inet_aton(server_ip, &server_addr.sin_addr);

    if (ret == 0) //the server_ip is not valid value
    {
        errno = EINVAL;
        return -1;
    }

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        return sockfd;

    ret = connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));

    if (ret == -1) {
        save_errno = errno;
        close(sockfd);
        errno = save_errno; //the close may be error
        return -1;
    }

    evutil_make_socket_nonblocking(sockfd);

    return sockfd;
}