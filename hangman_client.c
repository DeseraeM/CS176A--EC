#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* TODO: Implement recvall (see socket_client_example.c). */
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

/* TODO: Prompt the user for a single letter guess.
   Print >>>Error! Please guess one letter. and re-prompt on bad input.
   Return 0 on EOF, 1 on success. */
int getGuess(char *buf){
   char guess;
   int guessV;
   printf("\n>>>Letter to guess: ");
   guess = tolower(getchar());
   getchar();

   while(guess < 'a' || guess > 'z'){
         printf(">>>Error! Please guess one letter.\n");
         printf(">>>Letter to guess: ");
         guess = tolower(getchar());
         getchar();
      }
   buf[0] = guess;
   return 1;
}

int clientSock;
struct sockaddr_in servAddr;
char sndBuf[1];
char rcvBuf[256];
char servIP[16];
int servPort;

int main(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "usage: hangman_client <ip> <port>\n"); return 1; }

    setvbuf(stdout, NULL, _IONBF, 0);

    /* TODO: Create socket and connect to server (see socket_client_example.c). */
      strncpy(servIP, argv[1], sizeof(servIP));
      servPort = atoi(argv[2]);
      if ((clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        perror("socket");
        exit(1);
      }


      int reuse = 1;
      setsockopt(clientSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
      memset(&servAddr, 0, sizeof(servAddr));
      servAddr.sin_family = AF_INET;
      servAddr.sin_port = htons(servPort);
      inet_pton(AF_INET, servIP, &servAddr.sin_addr);

      sleep(1);

      if(connect(clientSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) <0){
         printf("connect() failed\n");
         exit(1);
      }

    /* TODO: Prompt >>>Ready to start game? (y/n):
       If 'n', close and exit.
       If 'y', send the start packet (1 byte, value 0). */
       char ans;
       do{
         printf(">>>Ready to start game? (y/n): ");
         ans = getchar();
         ans = tolower(ans);
         getchar();
       }while (ans != 'y' && ans != 'n');
       printf("\n");
       if (ans == 'n'){
         close(clientSock);
         exit(0);
       }
       sndBuf[0] = 0x00;
       if (send(clientSock, sndBuf, 1, 0) < 0){
         perror("send");
         close(clientSock);
         exit(1);
       }

    /* TODO: Loop receiving packets from the server.
       - msg_flag > 0: read msg_flag bytes, print as >>>...\n, break on "Game Over!".
       - msg_flag == 0: read word_length and num_incorrect, then the word state and
         incorrect letters, print the game state, prompt for a guess, send it. */
      while(1){
         if (recvall(clientSock, rcvBuf, 1) < 0) {
            perror("recv");
            close(clientSock);
            exit(1);
         }
         unsigned char msg_flag = rcvBuf[0];
         if (msg_flag >0 ){
            if (recvall(clientSock, rcvBuf, msg_flag) < 0) {
               perror("recv");
               close(clientSock);
               exit(1);
         }
         rcvBuf[msg_flag] = '\0';
         printf(">>>%s\n", rcvBuf);
         if (strcmp(rcvBuf, "Game Over!") == 0) break;
         }else{
            if (recvall(clientSock, rcvBuf, 2) < 0) {
               perror("recv");
               close(clientSock);
               exit(1);
         }
         unsigned char word_length = rcvBuf[0];
         unsigned char num_incorrect = rcvBuf[1];
         int total = word_length + num_incorrect;
         if(recvall(clientSock, rcvBuf, total) <0 ){
            perror("recv");
            close(clientSock);
            exit(1);
         }
         for (int i =0; i < word_length; i++)
            printf("%c ", rcvBuf[i]);
            printf("\n");
         if (num_incorrect > 0){
            printf(">>>Incorrect Guesses: ");
            for (int i = 0; i < num_incorrect; i++){
               if(i < num_incorrect - 1){
                  printf("%c, ", rcvBuf[word_length + i]);
                  } else {
                     printf("%c", rcvBuf[word_length + i]);
                  }
               }
               printf("\n");
            }
         if (getGuess(sndBuf) == 0) break;
         if(send(clientSock, sndBuf, 1, 0) <0 ){
            perror("send");
            close(clientSock);
            exit(1);
         }
        }
      }
   close(clientSock);
   return 0;
}