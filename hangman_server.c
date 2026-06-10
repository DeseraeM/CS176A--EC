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

/* TODO: Load words from hangman_words.txt intgito a global array. */
char words[MAX_WORDS][MAX_WORD_LEN];
int numsWords =0;

void lWords(){
    FILE *fp = fopen("hangman_words.txt", "r");
    if (fp == NULL){
        perror("fopen");
        exit(1);
    }
    while (numsWords < MAX_WORDS && fscanf(fp, "%s", words[numsWords]) == 1){
        numsWords++;
    }
    fclose(fp);
}

/* TODO: Implement recvall (see socket_server_example.c). */
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

/* TODO: Send a message packet to fd. */
void sendM(int fd, const char *msg){
    unsigned char len = strlen(msg);
    char buf[256];
    buf[0] = len;
    memcpy(buf+1, msg, len);
    if (send(fd, buf, len + 1, 0) < 0) {
        perror("send");
        close(fd);
        exit(1);
    }
}

/* TODO: Send a game control packet to fd. */
void sendG(int fd, char *word, char *guessed, int num_incorrect) {
    unsigned char word_length = strlen(word);
    char buf[256];
    buf[0] = 0x00;
    buf[1] = word_length;
    buf[2] = num_incorrect;
    memcpy(buf + 3, word, word_length);
    memcpy(buf + 3 + word_length, guessed, num_incorrect);
    if (send(fd, buf, 3 + word_length + num_incorrect, 0) < 0) {
        perror("send");
        close(fd);
        exit(1);
    }
}
/* TODO: Play one game with a connected client on fd. */
void PlayG(int fd){
    char *word = words[rand() % numsWords];
    int wordL = strlen(word);

    char wordS[MAX_WORD_LEN];
    memset(wordS, '_', wordL);
    wordS[wordL] = '\0';

    char guessed[MAX_INCORRECT];
    int numI = 0;
    int gameO = 0;

    sendM(fd, ">>>Game Starting!");
    sendG(fd, wordS,guessed, numI);
    while(!gameO){
        sendG(fd, wordS, guessed, numI);
        char rcvBuf[1];
        if (recvall(fd, rcvBuf, 1) < 0) {
            perror("recv");
            close(fd);
            exit(1);
        }
        char guess = tolower(rcvBuf[0]);

        int c = 0;
        for (int i =0; i < wordL; i ++){
            if (word[i]==guess){
                wordS[i] = guess;
                c =1;
            }
        }
        if (!c){
            int duplicate = 0;
            for(int i =0; i < numI; i++){
                if(guessed[i] == guess){
                    duplicate =1;
                    break;
                }
            }
            if(!duplicate){
            guessed[numI++] = guess;
            }
        }
        if (strcmp(wordS, word) == 0){
            sendG(fd, wordS, guessed, numI);
            sendM(fd, ">>>You Win!");
            sendM(fd, ">>>Game Over!");
            gameO = 1;
        }
        else if (numI >= MAX_INCORRECT){
            sendG(fd, wordS, guessed, numI);
            sendM(fd, ">>>The word was ");
            sendM(fd, word);
            sendM(fd, ">>>You Lose!");
            sendM(fd, ">>>Game Over!");
            gameO = 1;
        }
    }
}


/* TODO: Thread entry point — receive fd, play game, close fd. */
void *gameT(void *arg) {
    int fd = *((int *)arg);
    free(arg);
    char rcvBuf[1];
    if (recvall(fd, rcvBuf, 1) < 0) {
        perror("recv");
        close(fd);
        return NULL;
    }
    if (rcvBuf[0] != 0x00) {
        close(fd);
        return NULL;
    }
    PlayG(fd);
    close(fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "usage: hangman_server <port>\n"); return 1; }

    srand(time(NULL));

    /* TODO: Load words. */
    lWords();

    /* TODO: Create socket, bind, listen (see socket_server_example.c). */
    int serverSock;
    if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket");
        exit(1);
        }
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &((int){1}), sizeof(int)) < 0) {
        perror("setsockopt");
        exit(1);
    }
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(atoi(argv[1]));

    if (bind(serverSock, (struct sockaddr *)&servAddr, sizeof(servAddr))<0){
        perror("bind");
        exit(1);
        
    }
    if (listen(serverSock, 20) < 0){
        perror("listen");
        exit(1);
    }

    printf("Listening on port %s\n", argv[1]);
    while (1) {
        /* TODO: Accept a connection and spawn a thread to handle it. */
        struct sockaddr_in clntAddr;
        socklen_t clntLen = sizeof(clntAddr);
        int *fd = malloc(sizeof(int));
        *fd = accept(serverSock, (struct sockaddr *)&clntAddr, &clntLen);
        if(*fd <0){
            perror("accept");
            free(fd);
            continue;
        }
        pthread_t tid;
        if(pthread_create(&tid, NULL, gameT, fd)!=0){
            perror("pthread_create");
            close(*fd);
            free(fd);
            continue;
        }
        pthread_detach(tid);
    }
    return 0;
}
