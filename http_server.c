#include <stdlib.h>
#include <stdlib.h>
#include <unistd.h>    //getopt, fork
#include <string.h>    //strcat
#include <event2/event.h>
#include <event2/http.h> //http
#include <event2/buffer.h>
#include <event2/http_struct.h>
#include <event2/keyvalq_struct.h>
#include <signal.h>

#define MYMETHOD_SIGNATURE "my httpd v 0.0.1"

void http_handler(struct evhttp_request *req, void *arg);

void show_help();

void signal_handler(int sig);

struct event_base *base;

int main(int argc, char *argv[]) {
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);

    char *httpd_option_listen = "0.0.0.0";
    int httpd_option_port = 8080;
    int httpd_option_daemon = 0;
    int httpd_option_timeout = 120; //in seconds

    int c;
    while ((c = getopt(argc, argv, "l:p:dt:h")) != -1) {
        switch (c) {
            case 'l':
                httpd_option_listen = optarg;
                break;
            case 'p':
                httpd_option_port = atoi(optarg);
                break;
            case 'd':
                httpd_option_daemon = 1;
                break;
            case 't':
                httpd_option_timeout = atoi(optarg);
                break;
            case 'h':
            default:
                show_help();
                exit(EXIT_SUCCESS);
        }
    }

    if (httpd_option_daemon) {
        pid_t pid;
        pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            //退出父进程
            exit(EXIT_SUCCESS);
        }
    }

    // deprecated
    // event_init();
    // struct evhttp *httpd = evhttp_start(httpd_option_listen, httpd_option_port);
    base = event_base_new();
    struct evhttp *httpd = evhttp_new(base);
    evhttp_bind_socket(httpd, httpd_option_listen, httpd_option_port);

    evhttp_set_timeout(httpd, httpd_option_timeout);
    evhttp_set_gencb(httpd, http_handler, NULL);
    // evhttp_set_cb(httpd, "/", sepecific_handler, NULL);

    // deprecated
    // event_dispatch();
    // evhttp_free(httpd);

    event_base_dispatch(base);

    return 0;
}

void http_handler(struct evhttp_request *request, void *arg) {
    const struct evhttp_uri *evhttp_uri = evhttp_request_get_evhttp_uri(request);
    char uri[8192];
    evhttp_uri_join((struct evhttp_uri *) evhttp_uri, uri, 8192);
    printf("accept request url:%s\n", uri);

    char *decoded_uri = evhttp_decode_uri(uri);
    printf("decoded_uri=%s\n", decoded_uri);
    free(decoded_uri);

    //解析uri参数
    struct evkeyvalq params;
    evhttp_parse_query_str(uri, &params);
    printf("q=%s\n", evhttp_find_header(&params, "q"));
    printf("s=%s\n", evhttp_find_header(&params, "s"));

    //打印请求header
    struct evkeyvalq *headers = evhttp_request_get_input_headers(request);
    for (struct evkeyval *header = headers->tqh_first; header; header = header->next.tqe_next) {
        printf("    %s: %s\n", header->key, header->value);
    }

    //获取POST数据
    // char *post_data = (char *) request->input_buffer;
    // printf("post_data=%s\n", post_data);
    size_t post_size = evbuffer_get_length(request->input_buffer);
    unsigned char *post_data = evbuffer_pullup(request->input_buffer, -1);
    printf("post_data size: %ld, post_data=%s\n", post_size, post_data);

    evhttp_add_header(request->output_headers, "Server", MYMETHOD_SIGNATURE);
    evhttp_add_header(request->output_headers, "Content-Type", "text/plain; charset=UTF-8");
    evhttp_add_header(request->output_headers, "Connection", "close");

    struct evbuffer *evbuf = evbuffer_new();
    evbuffer_add_printf(evbuf, "Server response. Your request url is %s", uri);
    evhttp_send_reply(request, HTTP_OK, "OK", evbuf);
    evbuffer_free(evbuf);
}

void show_help() {
    char *help = "writen by Min\n\n"
                 "-l <ip_addr> interface to listen on, default is 0.0.0.0\n"
                 "-p <num>     port number to listen on, default is 8080\n"
                 "-d           run as a daemon\n"
                 "-t <second>  timeout for a http request, default is 120 seconds\n"
                 "-h           print this help and exit\n"
                 "\n";
    fprintf(stderr, "%s", help);
}

void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGHUP:
        case SIGQUIT:
        case SIGINT:
            event_base_loopbreak(base);
            break;
    }
}