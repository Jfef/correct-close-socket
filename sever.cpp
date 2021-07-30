#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;

    char sendBuff[1025];
    time_t ticks;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(5000);

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    listen(listenfd, 10);

    connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
    /*======================================*/
    // 如果没有这一行，不会报告错误的发生的，由于socket，处理的异常的读取关闭，存在没有处理消息
    // 对面直接关闭 了，会发生RST关闭包。
    // 即：read: Connection reset by peer
    write(connfd, "220 Welcome\r\n", 13);
    //  如果不存在这一句，对面属于正常的socket 关闭。不会发生错误。
    /*=========================================*/
    char buffer[1000000];
    int bytesRead = 0, res;

    for (;;) {
        res = read(connfd, buffer, 4096);
        if (res < 0)  {
            perror("read");
            exit(1);
        }
        if (!res)
            break;
        bytesRead += res;
    }
    printf("%d\n", bytesRead);

    // close(connfd);

    // ticks = time(NULL);  // 获取时间
    // snprintf(sendBuff, sizeof(sendBuff), "%.24s\r\n", ctime(&ticks));  // 格式化时间
    // write(connfd, sendBuff, strlen(sendBuff));  // 发送

    // close(connfd);
    // sleep(1);

}