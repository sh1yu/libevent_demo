#include <stdio.h>
#include <event2/event.h>
#include <time.h>

struct event *ev;

//回调函数
void timer_cb(int fd, short event, void *arg)
{
        printf("timer_cb\n");
        event_add(ev, arg); //重新注册
}

int main()
{
        //初始化libevent库
        struct event_base *base = event_base_new();

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        //初始化event结构中成员
        ev = event_new(base, -1, 0, timer_cb, &tv);

        // event_base_set用于libevent2.0版本之前，那时没有event_new函数
        // 当前已经废弃
        // event_base_set(base, ev);

        //将event添加到events事件链表，注册事件
        event_add(ev, &tv);
        //循环、分发事件
        event_base_dispatch(base);

        return 0;
}