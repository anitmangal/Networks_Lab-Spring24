#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define MAXBUFFLEN 512

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

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Usage: ./mailclient <server_IP> <smtp_port> <pop3_port>\n");
        exit(0);
    }
    int sockfd;
    struct sockaddr_in serv_addr, local_addr;
    socklen_t local_addr_len = sizeof(local_addr);
    char buf[MAXBUFFLEN];
    int bytesRead;
    size_t maxLen = MAXBUFFLEN;
    char username[100], password[100];

    // ask for username and password
    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);
    char tmp;
    scanf("%c", &tmp);          // to consume the \n after the password

    while(1)
    {
        // ask for choice
        printf("1. Manage Mail\n2. Send Mail\n3. Quit\nWhat would you like to do?: ");
        int choice;
        scanf("%d", &choice);
        switch (choice)
        {
            case 1:
            {
                serv_addr.sin_family = AF_INET;
                inet_aton(argv[1], &serv_addr.sin_addr);
                serv_addr.sin_port = htons(atoi(argv[3]));
                if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
                    perror("Unable to connect to server.\n");
                    exit(0);
                }
                myrecv(sockfd, buf, MAXBUFFLEN);
                if (strncmp(buf, "+OK", 3) == 0) {

                    strcpy(buf, "USER ");
                    strcat(buf, username);
                    strcat(buf, "\r\n");
                    send(sockfd, buf, MAXBUFFLEN, 0);
                    
                    myrecv(sockfd, buf, MAXBUFFLEN);

                    if (strncmp(buf, "+OK", 3) == 0) {

                        strcpy(buf, "PASS ");
                        strcat(buf, password);
                        strcat(buf, "\r\n");
                        send(sockfd, buf, MAXBUFFLEN, 0);

                        myrecv(sockfd, buf, MAXBUFFLEN);

                        if (strncmp(buf, "+OK", 3) == 0) {

                            while (1) {
                                strcpy(buf, "STAT\r\n");
                                send(sockfd, buf, MAXBUFFLEN, 0);

                                myrecv(sockfd, buf, MAXBUFFLEN);

                                if (strncmp(buf, "+OK", 3) == 0) {
                                    int nummsg = 0, bufInd = 4;
                                    while (buf[bufInd] >= '0' && buf[bufInd] <= '9') {
                                        nummsg = 10*(nummsg) + (int)(buf[bufInd]-'0');
                                        bufInd++;
                                    }

                                    int sizemaildrop = 0;
                                    bufInd++;
                                    while (buf[bufInd] >= '0' && buf[bufInd] <= '9') {
                                        sizemaildrop = 10*(sizemaildrop) + (int)(buf[bufInd]-'0');
                                        bufInd++;
                                    }

                                    char * mailbox[nummsg+1];
                                    int flag = 1;
                                    

                                    // RECONSIDER MESSAGE NUMBERS. CHECK RFCCCCC !!!!!!!!   !!! !! !! !! !!!!!!         !!!!!!!!!!!!!!


                                    for (int i = 1; i <= nummsg && flag; i++) {

                                        strcpy(buf, "LIST ");
                                        sprintf(buf+strlen(buf), "%d", i);
                                        strcat(buf, "\r\n");
                                        send(sockfd, buf, MAXBUFFLEN, 0);

                                        myrecv(sockfd, buf, MAXBUFFLEN);

                                        if (strncmp(buf, "+OK", 3) == 0) {
                                            int msgNumber = 0;
                                            bufInd = 4;
                                            while (buf[bufInd] >= '0' && buf[bufInd] <= '9') {
                                                msgNumber = 10*(msgNumber) + (int)(buf[bufInd]-'0');
                                                bufInd++;
                                            }

                                            int msgSize = 0;
                                            bufInd++;
                                            while (buf[bufInd] >= '0' && buf[bufInd] <= '9') {
                                                msgSize = 10*(msgSize) + (int)(buf[bufInd]-'0');
                                                bufInd++;
                                            }

                                            mailbox[msgNumber] = (char*)malloc((msgSize)*sizeof(char));

                                            strcpy(buf, "RETR ");
                                            sprintf(buf+strlen(buf), "%d", i);
                                            strcat(buf, "\r\n");
                                            send(sockfd, buf, MAXBUFFLEN, 0);

                                            int msgind = 0, recvind = 0;
                                            int recvbytes = recv(sockfd, buf, MAXBUFFLEN, 0);
                                            while(1) {
                                                while (recvind < recvbytes) {
                                                    mailbox[msgNumber][msgind] = buf[recvind];
                                                    if (msgind >= 5 && mailbox[msgNumber][msgind-4] == '\r' && mailbox[msgNumber][msgind-3] == '\n' && mailbox[msgNumber][msgind-2] == '.' && mailbox[msgNumber][msgind-1] == '\r' && mailbox[msgNumber][msgind] == '\n') break;
                                                    msgind++;
                                                    recvind++;
                                                }
                                                if (recvind == recvbytes) {
                                                    recvbytes = recv(sockfd, buf, MAXBUFFLEN, 0);
                                                    recvind = 0;
                                                }
                                                else break;
                                            }


                                        }
                                        else {
                                            perror("Error in retrieving scan listing for a message.\n");
                                            flag = 0;
                                        }
                                    }
                                }
                                else {
                                    perror("Error in STAT.\n");
                                }
                            }
                        }
                        else {
                            perror("Password authentication error.\n");
                        }
                    }
                    else {
                        perror("Username authentication error.\n");
                    }
                }
                else {
                    perror("Error occured after connection.\n");
                }
            }
            case 2:
            {

                char from[100], to[100], subject[100], body[50][100];

                scanf("%c", &tmp);          // to consume the \n after the choice
                printf("Enter the mail:\n");

                getline(&from, &maxLen, stdin);
                getline(&to, &maxLen, stdin);
                getline(&subject, &maxLen, stdin);
                int lines = 0;
                while (lines<50)
                {
                    getline(&body[lines], &maxLen, stdin);
                    for(int j=0; j<100; j++)
                    {
                        if(body[lines][j]=='\n')
                        {
                            body[lines][j]='\r';
                            body[lines][j+1]='\n';
                            body[lines][j+2]='\0';
                            break;
                        }
                    }
                    if (strcmp(body[lines], ".\r\n") == 0)
                        break;
                    lines++;
                }
                if(strncmp(from, "From: ", 6))
                {
                    printf("Enter valid mail!\n");
                    // close(sockfd);
                    break;
                }
                int f=1;
                for(int i=6;i<100;i++)
                {
                    if(from[i]=='@'&&from[i-1]==' '){
                        printf("Enter valid mail!\n");
                        // close(sockfd);
                        f=0;
                        break;
                    }
                    if(i==99){
                        printf("Enter valid mail!\n");
                        // close(sockfd);
                        f=0;
                        break;
                    }
                    if(from[i]=='@' && from[i+1]!='\n'){
                        break;
                    }
                }
                if(f==0){
                    break;
                }
                for(int i=0; i<100; i++)
                {
                    if(from[i]=='\n')
                    {
                        from[i]='\0';
                        break;
                    }
                }

                if(strncmp(to, "To: ", 4))
                {
                    printf("Enter valid mail!\n");
                    // close(sockfd);
                    break;
                }
                f=1;
                for(int i=4;i<100;i++)
                {
                    if(to[i]=='@'&&to[i-1]==' '){
                        printf("Enter valid mail!\n");
                        // close(sockfd);
                        f=0;
                        break;
                    }
                    if(i==99){
                        printf("Enter valid mail!\n");
                        // close(sockfd);
                        f=0;
                        break;
                    }
                    if(to[i]=='@' && to[i+1]!='\n'){
                        break;
                    }
                }
                if(f==0){
                    break;
                }
                for(int i=0; i<100; i++)
                {
                    if(to[i]=='\n')
                    {
                        to[i]='\0';
                        break;
                    }
                }

                if(strncmp(subject, "Subject: ", 9))
                {
                    printf("Enter valid mail!\n");
                    // close(sockfd);
                    break;
                }
                for(int i=0; i<100; i++)
                {
                    if(subject[i]=='\n')
                    {
                        subject[i]='\0';
                        break;
                    }
                }
                if(lines==50){
                    printf("Mail too long!\n");
                    // close(sockfd);
                    break;
                }
                if(strcmp(body[lines], ".\r\n")){
                    printf("Enter valid mail!\n");
                    // close(sockfd);
                    break;
                }
                lines++;

                //establishing connection as mail is in right format
                if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    perror("Unable to create socket\n");
                    exit(0);
                }

                serv_addr.sin_family = AF_INET;
                inet_aton(argv[1], &serv_addr.sin_addr);
                serv_addr.sin_port = htons(atoi(argv[2]));

                if ((connect(sockfd, (struct sockaddr *)&serv_addr,
                            sizeof(serv_addr))) < 0)
                {
                    perror("Unable to connect to server\n");
                    exit(0);
                }

                // Get local address information
                if (getsockname(sockfd, (struct sockaddr *)&local_addr, &local_addr_len) < 0) {
                    perror("Unable to get local address\n");
                    exit(EXIT_FAILURE);
                }

                char local_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));

                // shud get 220
                bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                // printf("s: %s\n", buf);
                if(bytesRead<0)
                {
                    perror("Unable to read from socket\n");
                    exit(0);
                }
                if(!strncmp(buf, "220", 3)){
                    strcpy(buf,"HELO ");
                    strcat(buf, local_ip); 
                    strcat(buf, "\r\n");                 

                    send(sockfd, buf, strlen(buf), 0);

                    // shud get 250
                    bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                    // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                    // printf("s: %s\n", buf);
                    if(bytesRead<0)
                    {
                        perror("Unable to read from socket\n");
                        exit(0);
                    }
                    if(!strncmp(buf,"250",3)){

                        //from
                        strcpy(buf, "MAIL FROM:<");
                        int i;
                        for(i=6;i<100;i++){
                            if(from[i]!=' '){
                                break;
                            }
                        }
                        strcat(buf, from+i);
                        strcat(buf, ">\r\n\0");
                        send(sockfd, buf, strlen(buf), 0);

                        // shud get 250
                        bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                        // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                        // printf("s: %s\n", buf);
                        if(bytesRead<0)
                        {
                            perror("Unable to read from socket\n");
                            exit(0);
                        }
                        if(!strncmp(buf,"250",3)){
                            //to
                            strcpy(buf, "RCPT TO:<");
                            for(i=4;i<100;i++){
                                if(to[i]!=' '){
                                    break;
                                }
                            }
                            strcat(buf, to+i);
                            strcat(buf, ">\r\n\0");

                            send(sockfd, buf, strlen(buf), 0);

                            // shud get 250
                            bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                            // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                            // printf("s: %s\n", buf);
                            if(bytesRead<0)
                            {
                                perror("Unable to read from socket\n");
                                exit(0);
                            }
                            if(!strncmp(buf,"250",3)){
                                send(sockfd, "DATA\r\n", 6, 0);

                                // shud get 354
                                bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                                // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                                // printf("s: %s\n", buf);
                                if(bytesRead<0)
                                {
                                    perror("Unable to read from socket\n");
                                    exit(0);
                                }
                                if(!strncmp(buf,"354",3)){
                                    // sending mail data
                                    for(int i=0; i<100; i++)        //the 3 loops are for adding <CR><LF> at the end of each line
                                    {
                                        if(from[i]=='\0')
                                        {
                                            from[i]='\r';
                                            from[i+1]='\n';
                                            from[i+2]='\0';
                                            break;
                                        }
                                    }

                                    for(int i=0; i<100; i++)
                                    {
                                        if(to[i]=='\0')
                                        {
                                            to[i]='\r';
                                            to[i+1]='\n';
                                            to[i+2]='\0';
                                            break;
                                        }
                                    }

                                    for(int i=0; i<100; i++)
                                    {
                                        if(subject[i]=='\0')
                                        {
                                            subject[i]='\r';
                                            subject[i+1]='\n';
                                            subject[i+2]='\0';
                                            break;
                                        }
                                    }

                                    send(sockfd, from, strlen(from), 0);
                                    send(sockfd, to, strlen(to), 0);
                                    send(sockfd, subject, strlen(subject), 0);

                                    for(int i=0; i<lines; i++)
                                    {
                                        send(sockfd, body[i], strlen(body[i]), 0);
                                    }

                                    // shud get 250
                                    bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                                    // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                                    // printf("s: %s\n", buf);
                                    if(bytesRead<0)
                                    {
                                        perror("Unable to read from socket\n");
                                        exit(0);
                                    }
                                    if(!strncmp(buf,"250",3)){     
                                        send(sockfd, "QUIT\r\n", 6, 0);

                                        // shud get 221
                                        bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                                        // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                                        // printf("s: %s\n", buf);
                                        if(bytesRead<0)
                                        {
                                            perror("Unable to read from socket\n");
                                            exit(0);
                                        }
                                        if(!strncmp(buf,"221",3)){
                                            printf("Mail sent successfully!\n");
                                        }
                                    }
                                }
                            }
                            else{
                                printf("Error occured!\n");
                            }
                        }
                    }
                }
                else{
                    printf("Error occured!\n");
                }

                close(sockfd);
                break;
            }
            case 3:
            {
                exit(0);
            }
            default:
            {
                printf("Enter valid choice!\n");
            }
        }
    }

    exit(0);
}