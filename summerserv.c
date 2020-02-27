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
#include <signal.h>

#define SIZE 256

void *client_thread(void*);
void signal_handler(int);

int sockfd, shared_id = 258, clisockfds[SIZE], climax;
char shared_buffer[SIZE*4], shared_users[SIZE][SIZE], *shared_user;
pthread_mutex_t lock;

struct thread_args { /*contains the information about the thread*/
    int sock;
    int id;
};

int main(int argc, char *argv[])
{
    int clisockfd, n, i = 0, j = 0;
    socklen_t clilen; /*for the accept*/
    pthread_t thread_id;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0); /*TCP/IP conection*/
    if (sockfd < 0){
        perror("Error on opening socket");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(6000);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        perror("Error on binding");
        exit(1);
    }

    listen(sockfd, 1); /*from one machine so the number of the backlog doesn't matter*/

    clilen = sizeof(cli_addr);

    if (pthread_mutex_init(&lock, NULL) < 0){
        perror("Error on initializing mutex");
        exit(1);
    }

    /* zero out shared_users */
    for (n = 0; n < SIZE; n++) {
        memset(shared_users[n], 0, SIZE);
    }

    signal(SIGINT,signal_handler);

    printf("Running Messenger Server...\n");

    while (1) {

        clisockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (clisockfd < 0){
            perror("Error on accept");
            exit(1);
        }

        /* create new thread for client */
        struct thread_args args;
        args.sock = clisockfd;
        j++;
        if(strlen(shared_users[j-1]) == 0){
                args.id = j-1;
        }

        /* set default username to shared_users*/
        bzero(shared_users[args.id], SIZE);
        sprintf(shared_users[args.id], "Client-%d", args.id);

        if (pthread_create(&thread_id, NULL, client_thread, (void*) &args) < 0) { /*client_thread= pointer to function*/ 
            perror("Error on creating thread");
            exit(1);
        }

    }

    pthread_join(thread_id, NULL);
    pthread_mutex_destroy(&lock);
    close(sockfd);
    return 0;
}

void *client_thread(void *args_ptr) {

    struct thread_args *args = (struct thread_args *) args_ptr;
    int sock, id, n, i;
    char buffer[SIZE*2], message[SIZE], command[SIZE], msg_token, *amessage;

    sock = args->sock;
    id = args->id;
	
    /* send welcome message to user */
    bzero(buffer, SIZE);
    sprintf(buffer, "Welcome to the Messenger Server: type /help for available commands\n");
    n = write(sock, buffer, SIZE);
    if (n < 0) 
        return NULL;

    /* write new connections to shared buffer*/
    printf("%s has joined\n", shared_users[id]);
	
    while (1) {

        /* read new message into buffer */
        bzero(buffer, SIZE);
        recv(sock, buffer, SIZE-1, MSG_DONTWAIT); /* non-blocking read */
        n = 0;
        msg_token = buffer[0];
        if (msg_token == '/') {

            /* read buffer into command and message strings */
            bzero(command, SIZE);
            bzero(message, SIZE);
            sscanf(buffer, "%s %[^\n]", command, message);

            /* process chat commands */
            if (strcmp(command, "/help") == 0) {
                bzero(buffer, SIZE);

                /* send list of commands */
                sprintf(buffer, "Commands: /msg, /list, /help, /exit\n");
                n = write(sock, buffer, SIZE);
                if (n < 0 ) break;
            } 
	else if (strcmp(command, "/list") == 0) {
                bzero(buffer, SIZE);
                strncat(buffer, "Connected users: \n", SIZE);
                for (n = 0; n < SIZE; n++) {
                    if (strlen(shared_users[n]) > 0) {
                        strncat(buffer, shared_users[n], SIZE);
                        strncat(buffer, " ", SIZE);
                    }
                }
                printf("%s\n", buffer);
                n = write(sock, buffer, strlen(buffer));
                if (n < 0)
                    break;
                strcpy(buffer,shared_buffer);
            } else if (strcmp(command, "/exit") == 0) {
                printf("%s has disconnected\n", shared_users[id]);

                /* let other users know of disconnection */
                pthread_mutex_lock(&lock);
                bzero(shared_buffer, SIZE);
                sprintf(shared_buffer, "%s has disconnected\n", shared_users[id]);
                shared_id = 257;
                pthread_mutex_unlock(&lock);

                /* send final confirmation to client */
                sprintf(buffer, "You have been disconnected");
                n = write(sock, buffer, strlen(buffer));
                break;
            } else if (strcmp(command, "/msg") == 0) {

                /* write new message to specific clients */
                printf("%s: %s", shared_users[id], buffer);
                amessage = strtok(message, "|");
                shared_user = strtok(NULL, "| ");
                if(shared_user != NULL){
                    if(amessage != NULL){
                        strcpy(message,amessage);        
                    }
                    for(i=0;i<SIZE;i++){
                        if(strcmp(shared_user,"all") == 0 ){
                            pthread_mutex_lock(&lock);
                            shared_id = 257;
                            pthread_mutex_unlock(&lock);
                            break;
                        }   
                        else if(strcmp(shared_user,shared_users[i]) == 0){
                            pthread_mutex_lock(&lock);
                            shared_id = i;
                            pthread_mutex_unlock(&lock);
                            break;
                        }
                    } 
                    pthread_mutex_lock(&lock);
                    bzero(shared_buffer, SIZE);
                    sprintf(shared_buffer, "%s: %s", shared_users[id], message);
                    pthread_mutex_unlock(&lock);
                }else{
                    pthread_mutex_lock(&lock);
                    bzero(shared_buffer, SIZE);
                    sprintf(shared_buffer, "Please choose a user to send the message.\n");
                    shared_id = id;
                    pthread_mutex_unlock(&lock);
                }
            } else {
                bzero(buffer, SIZE);
                sprintf(buffer, "%s: command not found, try /help\n", command);
                n = write(sock, buffer, strlen(buffer));
                if (n < 0)
                    break;
            }
        }

        /* write current message back to client */            
        if( shared_id == id || shared_id == 257){   
                n = write(sock, shared_buffer, strlen(shared_buffer));
                sleep(1);
                pthread_mutex_lock(&lock);
                shared_id = -1;
                bzero(shared_buffer,sizeof(shared_buffer));
                pthread_mutex_unlock(&lock);
        }
        if (n < 0) {
            /* if we can't write to the client that means they
                * have disconnected from the server */
            printf("%s has disconnected\n", shared_users[id]);

            /* let other users know of disconnection */
            pthread_mutex_lock(&lock);
            bzero(shared_buffer, SIZE);
            sprintf(shared_buffer, "%s has disconnected", shared_users[id]);
		
            pthread_mutex_unlock(&lock);
            close(sock);
            break;
        }
        /* don't bombard the client */
        sleep(1);
    }
    printf("%s has exited\n", shared_users[id]);
    pthread_mutex_lock(&lock);
    memset(shared_users[id], 0, SIZE);
	
    pthread_mutex_unlock(&lock);
    close(sock);
    return NULL;
}

void signal_handler(int signnum){

        int i;

        printf("\nServer shutting down.. \n");                                    //the handler for the signal SIGINT ( ctrl-c )
		
        pthread_mutex_destroy(&lock);                                              
        close(sockfd);
        sleep(1);
        exit(0);
}
