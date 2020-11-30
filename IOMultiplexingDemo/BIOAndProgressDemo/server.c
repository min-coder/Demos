#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <error.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include  <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINE 4096
#define SERV_PORT 5555

char convert_char(char c) {
    if ( 'A' <= c && c <= 'Z')
        return c + 32; // 转换小写
    else if ( 'a' <= c && c <= 'z')
        return c - 32; // 转换大写
    else
        return c; // 其他不变
}

void child_run(int fd) {

    printf("child_run int fd = %d\n",fd);

    char outbuf[MAX_LINE + 1];
    size_t outbuf_used = 0;
    ssize_t result;
    char ch[128];
    while (1) {
        bzero(outbuf,MAX_LINE + 1);
        bzero(ch,128);

        result = recv(fd, &ch, 128, 0);
        if (result == 0) {
            // 这里表示对端的socket已正常关闭.
            break;
        } else if (result == -1) {
            perror("read");
            break;
        }

        u_long len = strlen(ch);
        outbuf_used = 0;
        for (int i = 0; i < len; ++i) {
            outbuf[outbuf_used++] = convert_char(ch[i]);
        }
        send(fd, outbuf, outbuf_used, 0);

    }
    printf("child_run out\n");
}

/**
 * 信号处理函数
 * @param sig
 */
void sigchld_handler(int sig) {
    ///  pid =  -1 等待任何一个子进程退出，没有任何限制，此时waitpid和wait的作用一模一样
    /// WNOHANG 若由pid指定的子进程未发生状态改变(没有结束)，则waitpid()不阻塞，立即返回0
    while (waitpid(-1, 0, WNOHANG) > 0);
    printf("sigchld_handler out\n");
}

/**
 * 创建服务端套 并 返回 监听套接字
 * @param port  监听端口
 * @return 监听套接字
 */
int tcp_server_listen(int port) {

    int listenfd;
    /// 监听套接字
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    /// 填写 sockaddr_in
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    int on = 1;
    /// 设置属性
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    /// 绑定ip
    int rt1 = bind(listenfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (rt1 < 0) {
        error(1, errno, "bind failed ");
    }

    /// 监听 套接字
    int rt2 = listen(listenfd, 1024);
    if (rt2 < 0) {
        error(1, errno, "listen failed ");
    }
    /// 捕获SIGPIPE信号  参见 https://blog.csdn.net/xinguan1267/article/details/17357093
    signal(SIGPIPE, SIG_IGN);

    return listenfd;
}
int main(int c, char **v) {

    /// 创建服务端
    int listener_fd = tcp_server_listen(SERV_PORT);

    /// 捕获 SIGCHLD 信号， 设置信号处理函数  sigchld_handler
    signal(SIGCHLD, sigchld_handler);
    /// 循环 监听 有连接到来 fork 进程处理
    while (1) {
        struct sockaddr_storage ss;
        socklen_t slen = sizeof(ss);
        /// accept
        int fd = accept(listener_fd, (struct sockaddr *) &ss, &slen);
        if (fd < 0) { /// accept 失败
            error(1, errno, "accept failed");
            exit(1);
        }

        if (fork() == 0) { /// fork 子进程 并通过返回值 区分 子父进程
            /// 子进程
            close(listener_fd); /// 关闭从父进程复制来的 listener_fd
            child_run(fd); /// 运行子程序
            exit(0);
        } else {
            /// 父进程
            close(fd);
        }
    }

    return 0;
}