/*
 * socket_server_example.c
 *
 * Minimal TCP server in C.
 * Accepts one connection, reads a message, and echoes it back.
 *
 * Compile:  gcc -o server socket_server_example.c
 * Run:      ./server 9000
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>   /* socket, bind, listen, accept, send, recv */
#include <netinet/in.h>   /* struct sockaddr_in, htons, INADDR_ANY    */


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
    if (argc < 2) { fprintf(stderr, "usage: server <port>\n"); return 1; }

    /* 1. Create a TCP socket.
          AF_INET    = IPv4
          SOCK_STREAM = TCP (reliable, ordered byte stream)

          socket() returns a file descriptor (fd) — a small non-negative integer.
          In Unix, every open resource (files, pipes, sockets) is represented as
          an fd, which is just an index into the kernel's per-process table of
          open resources. The first three are always reserved:
            0 = stdin, 1 = stdout, 2 = stderr
          so socket() will return 3 or higher.

          Because sockets are fds, you can use the same close() call on them
          as on regular files. send() and recv() are socket-specific, but you
          could also use read() and write() on a socket fd. */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    /* 2. Allow reuse of the port immediately after the server exits.
          Without this, you often get "Address already in use" for ~60 s. */
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 3. Bind — attach the socket to a local address and port. */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;            /* accept on all interfaces   */
    addr.sin_port        = htons(atoi(argv[1]));  /* host-to-network byte order */

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }

    /* 4. Listen — mark the socket as passive (willing to accept connections).
          The second argument is the backlog: how many pending connections to
          queue before refusing new ones. */
    listen(srv, 10);

    printf("Listening on port %s\n", argv[1]);

    /* 5. Accept — block until a client connects; returns a new fd for that
          connection. The original srv fd keeps listening for more clients.

          At this point the process has two socket fds:
            srv    — the listening socket, used only to accept new connections
            client — the connected socket for this specific client

          Each call to accept() produces a fresh fd for a new client, which is
          why multi-client servers can handle many connections simultaneously:
          each gets its own fd to read from and write to independently. */
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    int client = accept(srv, (struct sockaddr *)&caddr, &clen);
    if (client < 0) { perror("accept"); return 1; }

    /* 6. Receive data from the client and echo it back. */
    char buf[256];
    int n = recvall(client, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Received: %s\n", buf);
        send(client, buf, n, 0);  /* echo */
    }

    close(client);
    close(srv);
    return 0;
}
