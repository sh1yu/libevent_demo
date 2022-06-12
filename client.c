#include <sys/types.h>
#include <sys/socket.h> //socket sockaddr AF_INET
#include <netinet/in.h> //sockaddr_in htons etc.
#include <arpa/inet.h>  //inet_aton
#include <errno.h>
#include <unistd.h> //read write getpid etc.
#include <stdio.h>
#include <string.h>
#include <stdlib.h> //malloc STDIN_FILENO
#include <event2/event.h>

// 从网络句柄fd中读取数据，写入句柄 arg
void send_msg_cb(int fd, short events, void *arg) {
    char msg[1024] = {};

    int ret = read(fd, msg, sizeof(msg));
    if (ret <= 0) {
        perror("read fail ");
        exit(1);
    }
    // arg为void *类型，转换为 int *类型后进行解引用
    int sockfd = *((int *) arg);

    write(sockfd, msg, ret);
    printf("write: %s\n", msg);
}

// 从fd中读取数据然后打印
void read_msg_cb(int fd, short events, void *arg) {
    char msg1[1024];

    int len = read(fd, msg1, sizeof(msg1) - 1);
    if (len <= 0) {
        perror("read error!!!");
        exit(1);
    }
    msg1[len] = '\0';
    printf("read is ok,msg: %s\n", msg1);
}

//根据给定的server ip和port，创建并返回网络句柄fd
int connect_server(const char *server_ip, const int port) {
    int sockfd, retcode;

    //创建一个空白的 以太网tcp socket, 返回其句柄
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd) {
        perror("socket is error");
        exit(-1);
    }

    struct sockaddr_in sock;
    // memset(&sock, 0, sizeof(sock));
    sock.sin_family = AF_INET;
    sock.sin_port = htons(port);
    retcode = inet_aton(server_ip, &sock.sin_addr);
    if (retcode == 0) {
        errno = EINVAL;
        return -1;
    }

    //使用空白socket和指定的 sockaddr 网络端口来打开连接，此时sockfd句柄指向的网络连接被打开
    retcode = connect(sockfd, (struct sockaddr *) &sock, sizeof(sock));
    if (retcode == -1) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    //使用event2/util中的工具，将socket设置为非阻塞
    evutil_make_socket_nonblocking(sockfd);

    //返回socket句柄
    return sockfd;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("please input two param.\n");
        return -1;
    }

    int sockfd = connect_server(argv[1], atoi(argv[2]));
    if (sockfd == -1) {
        printf("connect is error.\n");
        return -1;
    }

    //初始化base
    struct event_base *base = event_base_new();
    //ev1事件，监控套接字句柄，使用read_msg_cb函数处理数据
    //EV_READ表示在sockfd可读取时触发事件
    //EV_PERSIST表示事件触发回调函数执行后保持挂起状态
    //arg参数没有使用，保持为NULL即可
    struct event *ev1 = event_new(base, sockfd,
                                  EV_READ | EV_PERSIST,
                                  read_msg_cb, NULL);
    //添加事件到事件循环.应为不需要设置超时触发，所以timeval参数保持为NULL
    event_add(ev1, NULL);

    //ev2事件，监控标准输入句柄，使用send_msg_cb处理回调
    //EV_READ表示在标准输入可读取时触发事件
    //EV_PERSIST表示事件触发回调函数执行后保持挂起状态
    //send_msg_cb回调函数需要传入arg参数作为网络句柄用于数据转发，因此将sockfd转为void*然后传入
    struct event *ev2 = event_new(base, STDIN_FILENO,
                                  EV_READ | EV_PERSIST, send_msg_cb,
                                  (void *) &sockfd);
    //添加事件到事件循环.应为不需要设置超时触发，所以timeval参数保持为NULL
    event_add(ev2, NULL);

    //开始事件循环监听与分发
    event_base_dispatch(base);
    printf("ending.\n");
    return 0;
}