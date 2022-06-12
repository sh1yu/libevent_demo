#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <event2/event.h>
#include <event2/bufferevent.h>

//处理客户端发送过来的数据
void socket_read_cb(struct bufferevent *bev, void *arg) {
    char msg[4096];

    size_t len = bufferevent_read(bev, msg, sizeof(msg));

    msg[len] = '\0';
    printf("recv the client msg: %s", msg);

    char reply_msg[4096] = "I have received the msg: ";

    strcat(reply_msg, msg);
    // strcat(reply_msg + strlen(reply_msg), msg);
    bufferevent_write(bev, reply_msg, strlen(reply_msg));
}

//处理客户端关闭事件
void event_cb(struct bufferevent *bev, short event, void *arg) {

    if (event & BEV_EVENT_EOF)
        printf("connection closed\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error\n");

    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);

    //没有什么多余的event需要关闭的
}

//accept回调函数，触发该事件后，新建一个socket句柄，然后使用该socket进行数据交互
//socket句柄创建完成后会新建相关事件，然后绑定到base上
void accept_cb(int fd, short events, void *arg) {
    //准备新建的sockfd句柄，实际上就是一个int
    evutil_socket_t sockfd;

    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    // 不同于客户端主动创建sockaddr_in和空白socket来打开网络连接，
    // 等待句柄fd主动发起连接。被连接之后，使用新的socket建立连接，
    // 并设置client指向的sockaddr资源和sockaddr长度.
    // 最后返回新建的socket的句柄
    sockfd = accept(fd, (struct sockaddr *) &client, &len);

    // 将被连接而建立的socket句柄设置为非阻塞
    evutil_make_socket_nonblocking(sockfd);

    printf("accept a client %d\n", sockfd);

    // arg中传递过来的参数为event_base，因为新创建的socket还没有注册事件，需要主动注册事件
    struct event_base *base = (struct event_base *) arg;

    // 和server.c中不同的是，这里创建了一个带buffer的bev
    struct bufferevent *bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    // bev将读回调和事件回调本身分开了，所以使用event_cb处理关闭事件
    bufferevent_setcb(bev, socket_read_cb, NULL, event_cb, NULL);
    // bufferevent_setcb(bev, socket_read_cb, NULL, event_cb, arg);

    bufferevent_enable(bev, EV_READ | EV_PERSIST);
}

//初始化tcp server，创建并返回一个处于监听状态的socket
int tcp_server_init(int port, int listen_num) {
    int errno_save;
    evutil_socket_t listener;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1)
        return -1;

    //允许多次绑定同一个地址。要用在socket和bind之间
    evutil_make_listen_socket_reuseable(listener);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);

    if (bind(listener, (struct sockaddr *) &sin, sizeof(sin)) < 0)
        goto error;

    if (listen(listener, listen_num) < 0)
        goto error;

    //跨平台统一接口，将套接字设置为非阻塞状态
    evutil_make_socket_nonblocking(listener);

    return listener;

    error:
    errno_save = errno;
    evutil_closesocket(listener);
    errno = errno_save;

    return -1;
}

int main(int argc, char **argv) {

    // 通过socket接口手动监听一个socket，返回其句柄
    int listener = tcp_server_init(9999, 10);
    if (listener == -1) {
        perror(" tcp_server_init error ");
        return -1;
    }

    // 初始化event_base
    struct event_base *base = event_base_new();

    //添加监听客户端请求连接事件
    struct event *ev_listen = event_new(base, listener, EV_READ | EV_PERSIST,
                                        accept_cb, base);
    event_add(ev_listen, NULL);

    event_base_dispatch(base);
    event_base_free(base);

    return 0;
}