#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ctype.h>

#define MAX_CLIENTS   3
#define MAX_WORDS     15
#define MAX_WORD_LEN  9
#define MAX_INCORRECT 6

char words[MAX_WORDS][MAX_WORD_LEN];
int numsWords = 0;
int activeClients = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void lWords() {
    FILE *fp = fopen("hangman_words.txt", "r");
    if (fp == NULL) { perror("fopen"); exit(1); }
    while (numsWords < MAX_WORDS && fscanf(fp, "%s", words[numsWords]) == 1)
        numsWords++;
    fclose(fp);
}

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

void sendM(int fd, const char *msg) {
    unsigned char len = strlen(msg);
    char buf[256];
    buf[0] = len;
    memcpy(buf + 1, msg, len);
    send(fd, buf, len + 1, 0);
}

void sendG(int fd, char *word, char *guessed, int num_incorrect) {
    unsigned char word_length = strlen(word);
    char buf[256];
    buf[0] = 0x00;
    buf[1] = word_length;
    buf[2] = num_incorrect;
    memcpy(buf + 3, word, word_length);
    memcpy(buf + 3 + word_length, guessed, num_incorrect);
    send(fd, buf, 3 + word_length + num_incorrect, 0);
}

void PlayG(int fd) {
    char *word = words[rand() % numsWords];
    int wordL = strlen(word);
    char wordS[MAX_WORD_LEN];
    memset(wordS, '_', wordL);
    wordS[wordL] = '\0';
    char guessed[MAX_INCORRECT];
    int numI = 0;

    while (1) {
        sendG(fd, wordS, guessed, numI);

        char rcvBuf[1];
        if (recvall(fd, rcvBuf, 1) < 0) { close(fd); return; }
        char guess = tolower(rcvBuf[0]);

        int correct = 0;
        for (int i = 0; i < wordL; i++) {
            if (word[i] == guess) { wordS[i] = guess; correct = 1; }
        }

        if (!correct) {
            int dup = 0;
            for (int i = 0; i < numI; i++)
                if (guessed[i] == guess) { dup = 1; break; }
            if (!dup) guessed[numI++] = guess;
        }

        if (strcmp(wordS, word) == 0) {
            char msg[64];
            snprintf(msg, sizeof(msg), "The word was %s", word);
            sendM(fd, msg);
            sendM(fd, "You Win!");
            sendM(fd, "Game Over!");
            return;
        } else if (numI >= MAX_INCORRECT) {
            char msg[64];
            snprintf(msg, sizeof(msg), "The word was %s", word);
            sendM(fd, msg);
            sendM(fd, "You Lose!");
            sendM(fd, "Game Over!");
            return;
        }
    }
}

void *gameT(void *arg) {
    int fd = *((int *)arg);
    free(arg);

    pthread_mutex_lock(&lock);
    if (activeClients >= MAX_CLIENTS) {
        pthread_mutex_unlock(&lock);
        sendM(fd, "server-overloaded");
        close(fd);
        return NULL;
    }
    activeClients++;
    pthread_mutex_unlock(&lock);

    char rcvBuf[1];
    if (recvall(fd, rcvBuf, 1) < 0 || rcvBuf[0] != 0x00) {
        pthread_mutex_lock(&lock);
        activeClients--;
        pthread_mutex_unlock(&lock);
        close(fd);
        return NULL;
    }

    PlayG(fd);

    pthread_mutex_lock(&lock);
    activeClients--;
    pthread_mutex_unlock(&lock);

    close(fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "usage: hangman_server <port>\n"); return 1; }
    srand(time(NULL));
    lWords();

    int serverSock;
    if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) { perror("socket"); exit(1); }
    int opt = 1;
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) { perror("setsockopt"); exit(1); }

    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(atoi(argv[1]));

    if (bind(serverSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) { perror("bind"); exit(1); }
    if (listen(serverSock, 20) < 0) { perror("listen"); exit(1); }

    printf("Listening on port %s\n", argv[1]);
    fflush(stdout);

    while (1) {
        struct sockaddr_in clntAddr;
        socklen_t clntLen = sizeof(clntAddr);
        int *fd = malloc(sizeof(int));
        *fd = accept(serverSock, (struct sockaddr *)&clntAddr, &clntLen);
        if (*fd < 0) { perror("accept"); free(fd); continue; }
        pthread_t tid;
        if (pthread_create(&tid, NULL, gameT, fd) != 0) { perror("pthread_create"); close(*fd); free(fd); continue; }
        pthread_detach(tid);
    }
    return 0;
}