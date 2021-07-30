#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])
{
    int sockfd = 0, n = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr;

    if (argc != 2)
    {
        printf("\n Usage: %s <ip of server> \n", argv[0]);
        return 1;
    }

    memset(recvBuff, '0', sizeof(recvBuff));
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000);

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    }

    if ( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\n Error : Connect Failed \n");
        return 1;
    }

    char buffer[1000000];
    memset(buffer, 'x', 1000000);
    write(sockfd, buffer, 1000000); // returns 1000000
    /*=========================================================*/
    // 正确的关闭的socket 连接的方式
    shutdown(sock, SHUT_WR);
    for (;;) {
        res = read(sock, buffer, 4000);
        if (res < 0) {
            perror("reading");
            exit(1);
        }
        if (!res)
            break;
    }
    // 如果存在没有正确处理的read的数据，如何合理的关闭的呢？使用shut_down。
    // 最佳的方式在设计协议的头部中加入length
    /*============================= */
    close(sockfd);
    
    return 0;
}