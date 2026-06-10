/*
 * socket_client_example.c
 *
 * Minimal TCP client in C.
 * Connects to a server, sends a message, and prints the reply.
 *
 * Compile:  gcc -o client socket_client_example.c
 * Run:      ./client 127.0.0.1 9000
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>   /* socket, connect, send, recv */
#include <netinet/in.h>   /* struct sockaddr_in, htons   */
#include <arpa/inet.h>    /* inet_pton                   */


/*
 * recvall — receive exactly n bytes from fd into buf.
 *
 * TCP is a stream protocol: a single recv() call may return fewer bytes than
 * requested. This wrapper loops until all n bytes have arrived or the
 * connection closes. Returns n on success, -1 on error or disconnect.
 */
int recvall(int fd, void *buf, int n) {
    char *p = (char *)buf;
    int got = 0;
    while (got < n) {
        int r = recv(fd, p + got, n - got, 0);
        if (r <= 0) return -1;  /* 0 = peer closed, <0 = error */
        got += r;
    }
    return got;
}


int main(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "usage: client <ip> <port>\n"); return 1; }

    /* 1. Create a TCP socket.
          AF_INET    = IPv4
          SOCK_STREAM = TCP (reliable, ordered byte stream) */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    /* 2. Fill in the server's address.
          inet_pton converts a dotted-decimal string ("127.0.0.1") to the
          binary representation stored in sin_addr. */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &addr.sin_addr);

    /* 3. Connect — initiate the TCP handshake with the server. */
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }

    /* 4. Send data and receive the echo. */
    const char *msg = "hello";
    send(sock, msg, strlen(msg), 0);

    char buf[256];
    int n = recvall(sock, buf, strlen(msg));
    if (n > 0) {
        buf[n] = '\0';
        printf("Echo: %s\n", buf);
    }

    close(sock);
    return 0;
}
