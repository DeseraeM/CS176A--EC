#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int recvall(int fd, void *buf, int n) {
    char *p = (char *)buf;
    int got = 0;
    while (got < n) {
        int r = recv(fd, p + got, n - got, 0);
        if (r <= 0) return -1;
        got += r;
    }
    return got;
}

int getGuess(char *buf) {
    printf("\n>>>Letter to guess: ");
    fflush(stdout);

    int ch = getchar();
    if (ch == EOF) { printf("\n"); return 0; }

    int next = getchar();
    // flush rest of line if more chars
    if (next != '\n' && next != EOF) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        // force invalid
        ch = 0;
    }

    while (!(( ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))) {
        printf(">>>Error! Please guess one letter.\n");
        printf(">>>Letter to guess: ");
        fflush(stdout);
        ch = getchar();
        if (ch == EOF) { printf("\n"); return 0; }
        next = getchar();
        if (next != '\n' && next != EOF) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            ch = 0;
        }
    }

    buf[0] = tolower(ch);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "usage: hangman_client <ip> <port>\n"); return 1; }

    setvbuf(stdout, NULL, _IONBF, 0);

    int clientSock;
    struct sockaddr_in servAddr;
    if ((clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) { perror("socket"); exit(1); }
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &servAddr.sin_addr);
    if (connect(clientSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        perror("connect() failed");
        exit(1);
    }

    char ans;
    do {
        printf(">>>Ready to start game? (y/n): ");
        fflush(stdout);
        ans = tolower(getchar());
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    } while (ans != 'y' && ans != 'n');

    if (ans == 'n') { close(clientSock); return 0; }

    char sndBuf[1] = {0x00};
    if (send(clientSock, sndBuf, 1, 0) < 0) { perror("send"); close(clientSock); exit(1); }

    char rcvBuf[256];
    while (1) {
        if (recvall(clientSock, rcvBuf, 1) < 0) break;
        unsigned char msg_flag = (unsigned char)rcvBuf[0];

        if (msg_flag > 0) {
            if (recvall(clientSock, rcvBuf, msg_flag) < 0) break;
            rcvBuf[msg_flag] = '\0';
            printf(">>>%s\n", rcvBuf);
            if (strcmp(rcvBuf, "Game Over!") == 0) break;
        } else {
            if (recvall(clientSock, rcvBuf, 2) < 0) break;
            unsigned char word_length = (unsigned char)rcvBuf[0];
            unsigned char num_incorrect = (unsigned char)rcvBuf[1];
            if (recvall(clientSock, rcvBuf, word_length + num_incorrect) < 0) break;

            printf(">>>");
            for (int i = 0; i < word_length; i++) {
                printf("%c", rcvBuf[i]);
                if (i < word_length - 1) printf(" ");
            }
            printf("\n");

            printf(">>>Incorrect Guesses: ");
            for (int i = 0; i < num_incorrect; i++) {
                printf("%c", rcvBuf[word_length + i]);
                if (i < num_incorrect - 1) printf(" ");
            }
            printf("\n");

            if (getGuess(sndBuf) == 0) break;
            if (send(clientSock, sndBuf, 1, 0) < 0) { perror("send"); break; }
        }
    }

    close(clientSock);
    return 0;
}