#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_SOCKETS 10000

#define check(expr) if (!(expr)) { perror(#expr); exit(1); }

int main(int argc, char** argv) {
    if (argc != 5) {
        printf("usage: %s nsockets tcp|udp listen|nolisten reuseaddr|noreuseaddr\n", argv[0]);
        return 1;
    }

    int num_sockets = atoi(argv[1]);
    int enable_tcp = strcmp(argv[2], "tcp") == 0;
    int enable_listen = strcmp(argv[3], "listen") == 0;
    int enable_reuse = strcmp(argv[4], "reuseaddr") == 0;

    if (num_sockets > MAX_SOCKETS) {
        num_sockets = MAX_SOCKETS;
    }

    int conflicts = 0;

    int port_array[MAX_SOCKETS] = {};
    int i, j;

    printf("protocol = %s\n", enable_tcp ? "tcp" : "udp");
    printf("listen = %s\n", enable_listen ? "yes" : "no");
    printf("reuseaddr = %s\n", enable_reuse ? "yes" : "no");

    for (i = 0; i < num_sockets; i++) {
        int sock;
        if (enable_tcp) {
            sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        } else {
            sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        }
        check(sock != -1);

        if (enable_reuse) {
            int yes = 1;
            check(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != -1);
        }

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        check(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != -1);

        if (enable_listen) {
            check(listen(sock, 1) != -1);
        }

        socklen_t addrlen = sizeof(addr);
        check(getsockname(sock, (struct sockaddr*)&addr, &addrlen) != -1);

        port_array[i] = ntohs(addr.sin_port);

        for (j = 0; j < i; j++) {
            if (port_array[j] == port_array[i]) {
                printf("conflict: port[%04d] = port[%04d] = %d\n", j, i, port_array[i]);
                conflicts++;
            }
        }
    }

    printf("conflicts = %d\n", conflicts);

    return 0;
}
