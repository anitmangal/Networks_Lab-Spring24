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

#define MAXBUFFLEN 512
#define MAXMAILNO 1000

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
            char buf[MAXBUFFLEN];
            int state;
            int n=0;
            int del[MAXMAILNO+1];
            char user[100], pass[100];
            char filepath[100];
            FILE * mailbox;
            memset(del, 0, sizeof(del));

            // When the connection is established, the POP3 server sends a greeting.
            char greeting[] = "+OK POP3 server ready\r\n";
            // printf("Sending greeting\n");   // debug
            send(newsockid, greeting, strlen(greeting), 0);

            state = AUTHORIZATION;

            // implement USER and PASS auth
            while(1){
                if (state == AUTHORIZATION) {
                    myrecv(newsockid, buf, MAXBUFFLEN);
                    printf("%s\n", buf);    // debug
                    fflush(stdout);
                    if (strncmp(buf, "USER", 4) == 0) {
                        // printf("USER command\n");   // debug
                        fflush(stdout);
                        int i;
                        for(i=5; buf[i] != '\0'; i++) {
                            user[i-5] = buf[i];
                            // printf("%d\n", i);   // debug
                        }
                        user[i-5] = '\0';
                        // printf("%s\n", user);   // debug
                        fflush(stdout);
                        strcpy(filepath, user);
                        strcat(filepath, "/mymailbox");
                        mailbox = fopen(filepath, "r");
                        // printf("%s\n", filepath);   // debug
                        fflush(stdout);
                        if (mailbox == NULL) {
                            printf("Mailbox not found\n");   // debug
                            fflush(stdout);
                            char usererr[] = "-ERR no such user\r\n";
                            send(newsockid, usererr, strlen(usererr), 0);
                            close(newsockid);
                            exit(0);
                        }
                        else {
                            printf("Mailbox found\n");   // debug
                            fflush(stdout);
                            char userok[] = "+OK user exists\r\n";
                            send(newsockid, userok, strlen(userok), 0);
                            printf("expecting PASS\n");   // debug
                            fflush(stdout);
                            myrecv(newsockid, buf, MAXBUFFLEN);
                            printf("%s\n", buf);    // debug
                            fflush(stdout);
                            if(strncmp(buf, "PASS", 4) == 0) {
                                for(i=5; buf[i] != '\0'; i++) {
                                    pass[i-5] = buf[i];
                                }
                                pass[i-5] = '\0';
                                FILE * checkpass = fopen("user.txt", "r");
                                char stored_user[100], stored_pass[100];
                                while(fscanf(checkpass, "%s %s", stored_user, stored_pass) != EOF) {
                                    if(strcmp(stored_user, user) == 0) {
                                        if(strcmp(stored_pass, pass) == 0) {
                                            int sz;
                                            printf("Reading mailbox\n");   // debug
                                            fflush(stdout);
                                            int cnt=0;
                                            while(fgets(buf, MAXBUFFLEN, mailbox)){
                                                cnt++;
                                                // printf("%d: ", cnt);    // debug
                                                // fflush(stdout);
                                                // for(int j=0; j<strlen(buf); j++){
                                                //     printf("(%d,%c)", buf[j], buf[j]);    // debug
                                                //     fflush(stdout);
                                                // }
                                                if(strcmp(buf, ".\n") == 0){
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
                                            printf("Entering TRANSACTION state\n");   // debug
                                            // myrecv(newsockid, buf, MAXBUFFLEN);
                                            // printf("%s", buf);    // debug
                                            break;
                                        }
                                        else{
                                            char passerr[] = "-ERR wrong password\r\n";
                                            send(newsockid, passerr, strlen(passerr), 0);
                                            close(newsockid);
                                            exit(0);
                                        
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
                    myrecv(newsockid, buf, MAXBUFFLEN);
                    // printf("%s\n", buf);    // debug
                    if(strncmp(buf, "STAT", 4) == 0) {
                        int ntosend = n, sztosend = 0;
                        for(int i=0; i<MAXMAILNO; i++){
                            if(del[i] == 1){
                                ntosend--;
                            }
                        }
                        fseek(mailbox, 0L, SEEK_SET);
                        char szbuf[MAXBUFFLEN];
                        int num=1;
                        while(fgets(szbuf, MAXBUFFLEN, mailbox)){
                            if(del[num] == 0)
                                sztosend += strlen(szbuf);
                            if(strcmp(szbuf, ".\n") == 0){
                                num++;
                            }
                        }
                        char stat[100];
                        sprintf(stat, "+OK %d %d\r\n", ntosend, sztosend);
                        send(newsockid, stat, strlen(stat), 0);
                    }
                    else if(strncmp(buf, "LIST", 4) == 0) {
                        if(strlen(buf) <= 5){
                            int num=1,last=0,totsz=0;
                            int szofmail[n+1];
                            memset(szofmail, 0, sizeof(szofmail));
                            int ntosend=0;
                            for(int i=1; i<=n; i++){
                                if(del[i] == 0){
                                    ntosend++;
                                }
                            }
                            // printf("ntosend: %d\n", ntosend);   // debug
                            fseek(mailbox, 0L, SEEK_SET);
                            while(fgets(buf, MAXBUFFLEN, mailbox)){
                                if(del[num] == 0){
                                }
                                if(strcmp(buf, ".\n") == 0){
                                    szofmail[num]=ftell(mailbox)-last;
                                    totsz+=ftell(mailbox)-last;
                                    last = ftell(mailbox);
                                    num++;
                                }
                            }
                            char list[100];
                            sprintf(list, "+OK %d messages (%d octets)\r\n", ntosend, totsz);
                            send(newsockid, list, strlen(list), 0);
                            for(int i=1; i<=n; i++){
                                if(del[i] == 0){
                                    char listmsg[100];
                                    sprintf(listmsg, "%d %d\r\n", i, szofmail[i]);
                                    send(newsockid, listmsg, strlen(listmsg), 0);
                                    // printf("serv: %s", listmsg);   // debug
                                }
                            }
                            char listend[] = ".\r\n";
                            send(newsockid, listend, strlen(listend), 0);
                        }
                        else{
                            int num,last,szofmail;
                            sscanf(buf, "LIST %d", &num);
                            if(num<=n && del[num] == 0){
                                fseek(mailbox, 0L, SEEK_SET);
                                int cnt=1;
                                while(fgets(buf, MAXBUFFLEN, mailbox)){
                                    if(cnt>num){
                                        break;
                                    }
                                    if(strcmp(buf, ".\n") == 0){
                                        if(cnt==num){
                                            szofmail=ftell(mailbox)-last;
                                        }
                                        if(cnt==num-1){
                                            last = ftell(mailbox);
                                        }
                                        num++;
                                    }
                                }
                                char listmsg[100];
                                sprintf(listmsg, "+OK %d %d\r\n", num, szofmail);
                                send(newsockid, listmsg, strlen(listmsg), 0);
                            }
                            else{
                                char listerr[] = "-ERR no such message\r\n";
                                send(newsockid, listerr, strlen(listerr), 0);
                            }
                        }
                    }
                    else if(strncmp(buf, "RETR", 4) == 0) {
                        int num;
                        sscanf(buf, "RETR %d", &num);
                        // printf("num: %d\n", num);   // debug
                        if(num<=n && del[num] == 0){
                            char msg[51][MAXBUFFLEN];
                            int szofmail=0, last=0, lineno=0;
                            fseek(mailbox, 0L, SEEK_SET);
                            int cnt=1;
                            while(fgets(buf, MAXBUFFLEN, mailbox)){
                                if(cnt>num){
                                    break;
                                }
                                if(cnt==num){
                                    strcpy(msg[lineno], buf);
                                    for(int i=0;i<strlen(msg[lineno]);i++){
                                        if(msg[lineno][i] == '\n'){
                                            msg[lineno][i] = '\r';
                                            msg[lineno][i+1] = '\n';
                                            msg[lineno][i+2] = '\0';
                                            break;
                                        }
                                    }
                                    lineno++;
                                }
                                if(strcmp(buf, ".\n") == 0){
                                    if(cnt==num){
                                        szofmail=ftell(mailbox)-last;
                                    }
                                    if(cnt==num-1){
                                        last = ftell(mailbox);
                                    }
                                    cnt++;
                                }
                            }
                            char retr[100];
                            sprintf(retr, "+OK %d octets\r\n", szofmail);
                            send(newsockid, retr, strlen(retr), 0);
                            for(int i=0; i<lineno; i++){
                                // printf("serv: %s", msg[i]);   // debug
                                send(newsockid, msg[i], strlen(msg[i]), 0);
                            }
                        }
                        else{
                            char retrerr[] = "-ERR no such message\r\n";
                            send(newsockid, retrerr, strlen(retrerr), 0);
                        }
                    }
                    else if(strncmp(buf, "DELE", 4) == 0) {
                        int num;
                        sscanf(buf, "DELE %d", &num);
                        del[num] = 1;
                        char delok[100];
                        sprintf(delok, "+OK message %d deleted\r\n", num);
                        send(newsockid, delok, strlen(delok), 0);
                    }
                    else if(strncmp(buf, "RSET", 4) == 0) {
                        memset(del, 0, sizeof(del));
                        char rsetok[100];
                        sprintf(rsetok, "+OK maildrop has %d messages\r\n", n);
                        send(newsockid, rsetok, strlen(rsetok), 0);
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
                    int num=0;
                    for(int i=1; i<=n; i++){
                        if(del[i] == 0){
                            num++;
                        }
                    }
                    char update[100];
                    sprintf(update, "+OK %d messages left\r\n", num);
                    int cnt=1;
                    FILE * tmp=fopen("tmp", "w");
                    fseek(mailbox, 0L, SEEK_SET);
                    while(fgets(buf, MAXBUFFLEN, mailbox)){
                        if(del[cnt] == 0){
                            fprintf(tmp, "%s", buf);
                        }
                        if(strcmp(buf, ".\n") == 0){
                            cnt++;
                        }
                    }
                    // make sure the code below works as intended (removes prev mailbox and replaces with new one)
                    fclose(mailbox);
                    fclose(tmp);
                    remove(filepath);
                    rename("tmp", filepath);
                    send(newsockid, update, strlen(update), 0);
                    close(newsockid);
                    exit(0);
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