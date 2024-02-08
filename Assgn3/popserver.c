#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>

#define AUTHORIZATION 0
#define TRANSACTION 1
#define UPDATE 2


// Function to receive exactly 1 message from socket
void myrecv(int sockid, char * buf, int len) {
    char msgbuf[len];
    int msgind = 0, recvbytes = 0, recv_ind = 0;
    recvbytes = recv(sockid, buf, len, 0);
    while(1) {
        while (recv_ind < recvbytes && buf[recv_ind] != '\r') {
            msgbuf[msgind] = buf[recv_ind];
            msgind++;
            recv_ind++;
        }
        if (recv_ind == recvbytes) {
            recvbytes = recv(sockid, buf, len, 0);
            recv_ind = 0;
        }
        else {
            msgbuf[msgind] = '\0';
            break;
        }
    }
    strcpy(buf, msgbuf);
}

int main(int argc, char * argv[]) {
    if (argc < 2) {
        printf("Usage: ./%s <port>\n", argv[0]);
        exit(0);
    }
    // Create socket and bind
    int sockid = socket(AF_INET, SOCK_STREAM, 0);
    if (sockid < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockid, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Unable to bind\n");
        exit(0);
    }

    // Listen for incoming connections
    listen(sockid, 5);
    int newsockid;
    struct sockaddr_in cli_addr;
    int clilen = sizeof(cli_addr);

    // Iterative server
    while(1) {
        // Accept connection and fork
        newsockid = accept(sockid, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockid < 0) {
            perror("Unable to accept\n");
            exit(0);
        }
        int pid = fork();
        if (pid < 0) {
            perror("Unable to fork\n");
            exit(0);
        }
        if (pid == 0) {
            close(sockid);
            char buf[100];
            int state;  // initialise with what????

            // When the connection is established, the POP3 server sends a greeting.
            char greeting[] = "+OK POP3 server ready\r\n";
            // printf("Sending greeting\n");   // debug
            send(newsockid, greeting, strlen(greeting), 0);

            state = AUTHORIZATION;

            // implement USER and PASS auth
            while(1){
                if (state == AUTHORIZATION) {
                    myrecv(newsockid, buf, 100);
                    printf("%s\n", buf);    // debug
                    fflush(stdout);
                    if (strncmp(buf, "USER", 4) == 0) {
                        // printf("USER command\n");   // debug
                        fflush(stdout);
                        char user[100];
                        int i;
                        for(i=5; buf[i] != '\0'; i++) {
                            user[i-5] = buf[i];
                            // printf("%d\n", i);   // debug
                        }
                        user[i-5] = '\0';
                        // printf("%s\n", user);   // debug
                        fflush(stdout);
                        char filepath[100];
                        strcpy(filepath, user);
                        strcat(filepath, "/mymailbox");
                        FILE * mailbox = fopen(filepath, "r");
                        // printf("%s\n", filepath);   // debug
                        fflush(stdout);
                        if (mailbox == NULL) {
                            printf("Mailbox not found\n");   // debug
                            fflush(stdout);
                            char usererr[] = "-ERR no such user\r\n";
                            send(newsockid, usererr, strlen(usererr), 0);
                            close(newsockid);
                            exit(0);
                            // what to do here????
                        }
                        else {
                            printf("Mailbox found\n");   // debug
                            fflush(stdout);
                            char userok[] = "+OK user exists\r\n";
                            send(newsockid, userok, strlen(userok), 0);
                            printf("expecting PASS\n");   // debug
                            fflush(stdout);
                            myrecv(newsockid, buf, 100);
                            printf("%s\n", buf);    // debug
                            fflush(stdout);
                            if(strncmp(buf, "PASS", 4) == 0) {
                                char pass[100];
                                for(i=5; buf[i] != '\0'; i++) {
                                    pass[i-5] = buf[i];
                                }
                                pass[i-5] = '\0';
                                FILE * checkpass = fopen("user.txt", "r");
                                char stored_user[100], stored_pass[100];
                                while(fscanf(checkpass, "%s %s", stored_user, stored_pass) != EOF) {
                                    if(strcmp(stored_user, user) == 0) {
                                        if(strcmp(stored_pass, pass) == 0) {
                                            // parse mailbox????
                                            int n=0,sz;
                                            printf("Reading mailbox\n");   // debug
                                            fflush(stdout);
                                            while(fgets(buf, 100, mailbox)){
                                                printf("%s", buf);    // debug
                                                fflush(stdout);
                                                if(strcmp(buf, ".\r\n") == 0){
                                                    n++;
                                                }
                                            }
                                            fseek(mailbox, 0L, SEEK_END);
                                            sz = ftell(mailbox);
                                            char passok[100];
                                            sprintf(passok, "+OK mailbox has %d messages (%d octets)\r\n", n, sz);
                                            printf("%s\n", passok); // debug
                                            send(newsockid, passok, strlen(passok), 0);
                                            state = TRANSACTION;
                                            myrecv(newsockid, buf, 100);
                                            printf("%s", buf);    // debug
                                            break;
                                        }
                                    }
                                }
                                if(state != TRANSACTION) {
                                    char passerr[] = "-ERR wrong password\r\n";
                                    send(newsockid, passerr, strlen(passerr), 0);
                                    close(newsockid);
                                    exit(0);
                                }
                            }
                        }
                    }
                    else if(strncmp(buf, "QUIT", 4) == 0) {
                        char quit[] = "+OK bye\r\n";
                        send(newsockid, quit, strlen(quit), 0);
                        close(newsockid);
                        exit(0);
                    }
                }
                else if(state == TRANSACTION){
                    myrecv(newsockid, buf, 100);
                    if(strncmp(buf, "STAT", 4) == 0) {
                        
                    }
                    else if(strncmp(buf, "LIST", 4) == 0) {
                        // implement LIST
                    }
                    else if(strncmp(buf, "RETR", 4) == 0) {
                        // implement RETR
                    }
                    else if(strncmp(buf, "DELE", 4) == 0) {
                        // implement DELE
                    }
                    else if(strncmp(buf, "RSET", 4) == 0) {
                        // implement RSET
                    }
                    else if(strncmp(buf, "QUIT", 4) == 0) {
                        // implement QUIT
                        state = UPDATE;
                    }
                    else {
                        // wrong command????
                    }
                }
                else if(state == UPDATE){
                    // implement UPDATE
                }
                else {
                    // what to do here????
                }

            }
        }
        else {
            close(newsockid);           // Parent closes newsockid
        }
    }

}