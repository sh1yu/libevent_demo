#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <event2/event.h>

//处理客户端发送过来的数据，其中参数arg为事件本身。
//之所以要把事件对象本身传递过来，是为了在读取出错时可以关闭该事件
void socket_read_cb(int fd, short events, void *arg)
{
    char msg[4096];

    //将参数还原回event事件对象
    struct event *ev = (struct event *)arg;
    int len = read(fd, msg, sizeof(msg) - 1);
    if (len <= 0)
    {
        //读取出错，关闭事件。注意此处是在事件回调函数内关闭事件
        if (len == 0)
            printf("closed by peer\n");
        else
            printf("some error happen when read: %d\n", errno);
        event_free(ev); //关闭事件循环
        close(fd);      //关闭当前套接字
        return;
    }

    msg[len] = '\0';
    printf("recv the client msg: %s", msg);

    char reply_msg[4096] = "I have recvieced the msg: ";
    // strcat(reply_msg + strlen(reply_msg), msg);
    strcat(reply_msg, msg);

    //将数据加上一个头之后再写回去
    write(fd, reply_msg, strlen(reply_msg));
}

//accept回调函数，触发该事件后，新建一个socket句柄，然后使用该socket进行数据交互
//socket句柄创建完成后会新建相关事件，然后绑定到base上
void accept_cb(int fd, short events, void *arg)
{
    //准备新建的sockfd句柄，实际上就是一个int
    evutil_socket_t sockfd;

    //新建sockaddr_in
    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    // 不同于客户端主动创建sockaddr_in和空白socket来打开网络连接，
    // 等待句柄fd主动发起连接。被连接之后，使用新的socket建立连接，
    // 并设置client指向的sockaddr资源和sockaddr长度.
    // 最后返回新建的socket的句柄
    sockfd = accept(fd, (struct sockaddr *)&client, &len);

    // 将被连接而建立的socket句柄设置为非阻塞
    evutil_make_socket_nonblocking(sockfd);

    printf("accept a client %d\n", sockfd);

    // arg中传递过来的参数为event_base，因为新创建的socket还没有注册事件，需要主动注册事件
    struct event_base *base = (struct event_base *)arg;

    //仅仅是为了动态创建一个空的event结构体
    struct event *ev = event_new(NULL, -1, 0, NULL, NULL);
    //event_assign和event_new的不同在于需要事先提供一个结构体
    //使用event_assign而不是event_new的原因是需要将事件本身作为该事件触发后回调时的回调参数
    //事件ev触发的条件是在新创建的sockfd句柄上有可读的内容产生，也就是客户端有发送数据过来
    event_assign(ev, base, sockfd, EV_READ | EV_PERSIST,
                 socket_read_cb, (void *)ev);
    //添加事件到事件循环.应为不需要设置超时触发，所以timeval参数保持为NULL
    event_add(ev, NULL);
}

//初始化tcp server，创建并返回一个处于监听状态的socket
int tcp_server_init(int port, int listen_num)
{
    int errno_save;
    evutil_socket_t listener_sock;

    //创建一个空白的 以太网tcp socket, 返回其句柄
    listener_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_sock == -1)
        return -1;

    //允许多次绑定同一个地址。要用在socket和bind之间
    evutil_make_listen_socket_reuseable(listener_sock);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);

    //将创建好的listener_sock绑定一个地址，然后监听连接
    if (bind(listener_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        goto error;
    if (listen(listener_sock, listen_num) < 0)
        goto error;

    //跨平台统一接口，将套接字设置为非阻塞状态
    evutil_make_socket_nonblocking(listener_sock);

    return listener_sock;

error:
    errno_save = errno;
    evutil_closesocket(listener_sock);
    errno = errno_save;

    return -1;
}

int main(int argc, char **argv)
{
    int listener = tcp_server_init(9999, 10);
    if (listener == -1)
    {
        perror(" tcp_server_init error ");
        return -1;
    }

    struct event_base *base = event_base_new();

    //添加监听客户端请求连接事件，将base自己作为参数传入，因为回调函数内可能还需要绑定事件
    struct event *ev_listen = event_new(base, listener, EV_READ | EV_PERSIST,
                                        accept_cb, base);
    event_add(ev_listen, NULL);

    event_base_dispatch(base);

    return 0;
}