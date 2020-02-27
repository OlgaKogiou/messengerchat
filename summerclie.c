#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>

#define SIZE 256

void *get_messages(void*);
void server_dced(int);
void signal_handler(int);

int sockfd;

int main(int argc, char *argv[]) {

    int n;
    struct sockaddr_in serv_addr;
    pthread_t thread_id;
    char buffer[SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        perror("Error on opening socket");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(6000);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Connecting to Messenger Server...\n");

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        perror("Error on connecting");
        exit(1);
    }

    static struct sigaction act, act2; 
    act.sa_handler = signal_handler;
    //act2.sa_handler = SIG_IGN; 
    sigfillset(&(act.sa_mask));
    //sigfillset(&(act2.sa_mask));  
    sigaction(SIGTSTP, &act, 0);
    //sigaction(SIGTSTP, &act2, 0); 
    //signal(SIGINT,server_dced);
    signal(SIGINT,signal_handler);

    printf("Connected !\n");

    /* create thread for receiving chat messages from the server */
    if (pthread_create(&thread_id, NULL, get_messages, (void*) &sockfd) < 0){
        perror("Error on creating thread");
        exit(1);
    }

    while (1) {
        /* set message prompt */
        printf("> ");

        /* send a message to server */
        bzero(buffer, SIZE);
        fgets(buffer, SIZE-1, stdin);
        if(strcmp(buffer,"clear\n")==0){
            system("clear");
            bzero(buffer, SIZE);
        }
        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0)
            break;
    }
    printf("Lost connection to server\n");
    pthread_exit(NULL);
    close(sockfd);
    return 0;
}

void *get_messages(void *sock_ptr) {
    int sock, len, n;
    char buffer[SIZE], cache[SIZE];

    sock = *(int *) sock_ptr;

    bzero(cache, SIZE);
    while(1) {
        /* read message into buffer*/
        bzero(buffer, SIZE);
        n = recv(sock, buffer, SIZE-1, MSG_DONTWAIT); /* non-blocking read */

        len = strlen(buffer);

        /* only display new messages */
        if (len > 0 && strcmp(buffer, cache) != 0) {
            printf("\n-%s \n", buffer);
            if(strcmp(buffer,"You have been disconnected") == 0){
                exit(0);
            }
            bzero(cache, SIZE);
        }

        /* give the user time to reply */
        sleep(1);
    }
    close(sock);
    return NULL;
}

void signal_handler(int signum){
    printf("\nType /exit in order to disconnect. \n");
    return;
}
