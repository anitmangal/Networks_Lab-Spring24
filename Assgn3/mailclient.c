#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <assert.h>

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
            case 1: {
                // Make a TCP connection with POP3 server
                if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    perror("Unable to create socket\n");
                    exit(0);
                }

                serv_addr.sin_family = AF_INET;
                inet_aton(argv[1], &serv_addr.sin_addr);
                serv_addr.sin_port = htons(atoi(argv[3]));
                if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
                    perror("Unable to connect to server.\n");
                    exit(0);
                }
                myrecv(sockfd, buf, MAXBUFFLEN);            // Receive connection response
                printf("%s\n", buf); // debug
                if (strncmp(buf, "+OK", 3) == 0) {
                    // AUTHORIZATION state
                    // Send USER command
                    strcpy(buf, "USER ");
                    strcat(buf, username);
                    strcat(buf, "\r\n");
                    send(sockfd, buf, strlen(buf), 0);
                    
                    myrecv(sockfd, buf, MAXBUFFLEN);      // Receive response to USER
                    printf("%s\n", buf); // debug
                    fflush(stdout);

                    if (strncmp(buf, "+OK", 3) == 0) {
                        // Send PASS command
                        strcpy(buf, "PASS ");
                        strcat(buf, password);
                        strcat(buf, "\r\n");
                        send(sockfd, buf, strlen(buf), 0);
                        printf("%s\n", buf); // debug
                        // printf("in PASS\n");    // debug
                        // fflush(stdout);

                        myrecv(sockfd, buf, MAXBUFFLEN);        // Receive response to PASS
                        printf("%s\n", buf); // debug
                        fflush(stdout);

                        if (strncmp(buf, "+OK", 3) == 0) {
                            // TRANSACTION state
                            printf("transaction state\n");   // debug
                            while (1) {
                                // Send STAT command
                                strcpy(buf, "STAT\r\n");
                                send(sockfd, buf, strlen(buf), 0);

                                myrecv(sockfd, buf, MAXBUFFLEN);        // Receive response to STAT
                                printf("STAT 1: %s\n", buf); // debug

                                if (strncmp(buf, "+OK", 3) == 0) {
                                    // Find number of messages and size of maildrop through response
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


                                    char * mailbox[nummsg];             // To store the mailbox mail-wise
                                    int msgmap[nummsg];                 // To map index of local mailbox to actual message number sent by server
                                    // Send LIST command
                                    strcpy(buf, "LIST\r\n");
                                    send(sockfd, buf, strlen(buf), 0);

                                    // Get response from server
                                    char msgbuf[MAXBUFFLEN];
                                    int msg_ind = 0, recv_ind = 0;
                                    int recvbytes = recv(sockfd, buf, MAXBUFFLEN, 0);
                                    while(1) {
                                        while(recv_ind < recvbytes && buf[recv_ind] != '\r') {
                                            msgbuf[msg_ind] = buf[recv_ind];
                                            msg_ind++;
                                            recv_ind++;
                                        }
                                        if(recv_ind == recvbytes) {
                                            recvbytes = recv(sockfd, buf, MAXBUFFLEN, 0);
                                            recv_ind = 0;
                                        }
                                        else {
                                            msgbuf[msg_ind] = '\0';
                                            recv_ind += 2;
                                            msg_ind = 0;
                                            break;
                                        }
                                    }
                                    printf("LIST 1: %s\n", msgbuf); // debug
                                    if (strncmp(msgbuf, "+OK", 3) == 0) {
                                        // Response OK, get each line giving scan listing of each message
                                        int cnt = 0;
                                        while(1) {
                                            while (recv_ind < recvbytes && buf[recv_ind] != '\r') {
                                                msgbuf[msg_ind] = buf[recv_ind];
                                                msg_ind++;
                                                recv_ind++;
                                            }
                                            if (recv_ind >= recvbytes) {
                                                recvbytes = recv(sockfd, buf, MAXBUFFLEN, 0);
                                                recv_ind = 0;
                                            }
                                            else {
                                                msgbuf[msg_ind] = '\0';
                                                printf("LIST 2: %s\n", msgbuf); // debug
                                                recv_ind += 2;
                                                msg_ind = 0;
                                                if (strcmp(msgbuf, ".") == 0 || cnt >= nummsg) {
                                                    // .<CR><LF> means end of multi-line response
                                                    assert(cnt == nummsg);
                                                    break;
                                                }
                                                else {
                                                    // One line received, get message number and message size from that line.
                                                    int msgNumber = 0;
                                                    while (msgbuf[msg_ind] >= '0' && msgbuf[msg_ind] <= '9') {
                                                        msgNumber = 10*msgNumber + (int)(msgbuf[msg_ind]-'0');
                                                        msg_ind++;
                                                    }
                                                    int msgSize = 0;
                                                    msg_ind++;
                                                    while (msgbuf[msg_ind] >= '0' && msgbuf[msg_ind] <= '9') {
                                                        msgSize = 10*msgSize + (int)(msgbuf[msg_ind]-'0');
                                                        msg_ind++;
                                                    }

                                                    msgmap[cnt] = msgNumber;                // Map message number
                                                    mailbox[cnt] = (char*)malloc(msgSize*sizeof(char));     // Allocate space for message
                                                    cnt++;
                                                    msg_ind = 0;
                                                }
                                            }
                                        }

                                        // Print list of messages in tabular form
                                        printf("%-7s\t%-40s\t%-14s\t%-100s\n", "Sl. No.", "Sender's email id", "Time received", "Subject");
                                        int flag = 1;       // To record if any error occurs while retrieving messages.

                                        for (cnt = 0; cnt < nummsg; cnt++) {
                                            // Send RETR <msgnumber> command
                                            strcpy(buf, "RETR ");
                                            sprintf(buf+strlen(buf), "%d", msgmap[cnt]);
                                            strcat(buf, "\r\n");
                                            send(sockfd, buf, strlen(buf), 0);

                                            // Get response from server
                                            msg_ind = 0, recv_ind = 0;
                                            recvbytes = recv(sockfd, buf, MAXBUFFLEN, 0);
                                            while(1) {
                                                while (recv_ind < recvbytes && buf[recv_ind] != '\r') {
                                                    msgbuf[msg_ind] = buf[recv_ind];
                                                    msg_ind++;
                                                    recv_ind++;
                                                }
                                                if (recv_ind == recvbytes) {
                                                    recvbytes = recv(sockfd, buf, MAXBUFFLEN, 0);
                                                    recv_ind = 0;
                                                }
                                                else {
                                                    msgbuf[msg_ind] = '\0';
                                                    msg_ind = 0;
                                                    recv_ind += 2;
                                                    break;
                                                }
                                            }
                                            printf("RETR 1: %s\n", msgbuf); // debug
                                            if (strncmp(msgbuf, "+OK", 3) == 0) {
                                                // Response OK, get message until <CR><LF>.<CR><LF>
                                                while(1) {
                                                    while(recv_ind < recvbytes) {
                                                        mailbox[cnt][msg_ind] = buf[recv_ind];
                                                        msg_ind++;
                                                        recv_ind++;
                                                    }
                                                    if (msg_ind >= 5 && strncmp(mailbox[cnt]+msg_ind-5, "\r\n.\r\n", 5)==0) break;
                                                    recvbytes = recv(sockfd, buf, MAXBUFFLEN, 0);
                                                    recv_ind = 0;
                                                }

                                                // Split received message and extract relevant fields for list
                                                char sno[10], from[100], rec[100], subject[101];
                                                sprintf(sno, "%d", msgmap[cnt]);
                                                
                                                char * token = strtok(mailbox[cnt], "\r\n");
                                                msg_ind = 0;
                                                while (token[msg_ind] != ' ') msg_ind++;
                                                strcpy(from, token+msg_ind+1);

                                                token = strtok(NULL, "\r\n");
                                                token = strtok(NULL, "\r\n");
                                                msg_ind = 0;
                                                while (token[msg_ind] != ' ') msg_ind++;
                                                strcpy(subject, token+msg_ind+1);

                                                token = strtok(NULL, "\r\n");
                                                msg_ind = 0;
                                                while (token[msg_ind] != ' ') msg_ind++;
                                                strcpy(rec, token+msg_ind+1);

                                                printf("%-7s\t%-40s\t%-14s\t%-100s\n", sno, from, rec, subject);        // Print 1 item of list
                                            }
                                            else {
                                                perror("Error in RETR");
                                                flag = 0;
                                                break;
                                            }
                                        }

                                        if (flag == 0) break;       // Some message caused an error in RETR, go to main menu

                                        // Get choice of mail to see
                                        printf("Enter mail no. to see: ");
                                        int mail_choice;
                                        while(1) {
                                            scanf("%d", &mail_choice);
                                            if (mail_choice == -1) {
                                                break;
                                            }
                                            else {
                                                for (cnt = 0; cnt < nummsg; cnt++) if (msgmap[cnt] == mail_choice) break;
                                                if (cnt < nummsg) {
                                                    break;
                                                }
                                                else {
                                                    perror("Mail no. out of range, give again.");
                                                }
                                            }
                                        }
                                        if (mail_choice >= 0) {
                                            // Valid mail chosen, print the message until . is seen
                                            char * token = strtok(mailbox[mail_choice], "\r\n");
                                            while (strcmp(token, ".") != 0) printf("%s\n", token);
                                            char c = getchar();     // Wait for response
                                            if (c == 'd') {
                                                // Send DELE <msgnumber> command
                                                strcpy(buf, "DELE ");
                                                sprintf(buf+strlen(buf), "%d", mail_choice);
                                                strcat(buf, "\r\n");
                                                send(sockfd, buf, strlen(buf), 0);

                                                myrecv(sockfd, buf, MAXBUFFLEN);        // Receive response from server

                                                if (strncmp(buf, "+OK", 3) == 0) printf("Deleted succesfully.\n");
                                                else perror("Error in deleting email.\n");
                                            }
                                        }
                                        else {
                                            // Mail choice = -1, send QUIT command
                                            strcpy(buf, "QUIT\r\n");
                                            send(sockfd, buf, strlen(buf), 0);

                                            myrecv(sockfd, buf, MAXBUFFLEN);            // Receive response from server

                                            // UPDATE state
                                            if (strncmp(buf, "+OK", 3) == 0) {
                                                printf("Connection closed.\n");
                                            }
                                            else {
                                                perror("Unable to delete some messages.\n");
                                            }
                                            break;                                      // UPDATE state ended
                                        }
                                    }
                                    else {
                                        perror("Error in LIST.\n");
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
                        perror("Username authentication error.\n");     // should we send quit command????
                    }
                }
                else {
                    perror("Error occured after connection.\n");
                }
                break;
            }
            case 2:
            {

                char *from, *to, *subject, *body[50];
                from = (char*)malloc(100*sizeof(char));
                to = (char*)malloc(100*sizeof(char));
                subject = (char*)malloc(100*sizeof(char));

                scanf("%c", &tmp);          // to consume the \n after the choice
                printf("Enter the mail:\n");

                getline(&from, &maxLen, stdin);
                getline(&to, &maxLen, stdin);
                getline(&subject, &maxLen, stdin);
                int lines = 0;
                while (lines<50)
                {
                    body[lines] = (char*)malloc(100*sizeof(char));
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
                myrecv(sockfd, buf, MAXBUFFLEN);
                // bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                // printf("s: %s\n", buf);
                // if(bytesRead<0)
                // {
                //     perror("Unable to read from socket\n");
                //     exit(0);
                // }
                if(!strncmp(buf, "220", 3)){
                    strcpy(buf,"HELO ");
                    strcat(buf, local_ip); 
                    strcat(buf, "\r\n");                 

                    send(sockfd, buf, strlen(buf), 0);

                    // shud get 250
                    myrecv(sockfd, buf, MAXBUFFLEN);
                    // bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                    // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                    // printf("s: %s\n", buf);
                    // if(bytesRead<0)
                    // {
                    //     perror("Unable to read from socket\n");
                    //     exit(0);
                    // }
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
                        myrecv(sockfd, buf, MAXBUFFLEN);
                        // bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                        // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                        // printf("s: %s\n", buf);
                        // if(bytesRead<0)
                        // {
                        //     perror("Unable to read from socket\n");
                        //     exit(0);
                        // }
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
                            myrecv(sockfd, buf, MAXBUFFLEN);
                            // bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                            // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                            // printf("s: %s\n", buf);
                            // if(bytesRead<0)
                            // {
                            //     perror("Unable to read from socket\n");
                            //     exit(0);
                            // }
                            if(!strncmp(buf,"250",3)){
                                send(sockfd, "DATA\r\n", 6, 0);

                                // shud get 354
                                myrecv(sockfd, buf, MAXBUFFLEN);
                                // bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                                // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                                // printf("s: %s\n", buf);
                                // if(bytesRead<0)
                                // {
                                //     perror("Unable to read from socket\n");
                                //     exit(0);
                                // }
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
                                    myrecv(sockfd, buf, MAXBUFFLEN);
                                    // bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                                    // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                                    // printf("s: %s\n", buf);
                                    // if(bytesRead<0)
                                    // {
                                    //     perror("Unable to read from socket\n");
                                    //     exit(0);
                                    // }
                                    if(!strncmp(buf,"250",3)){     
                                        send(sockfd, "QUIT\r\n", 6, 0);

                                        // shud get 221
                                        myrecv(sockfd, buf, MAXBUFFLEN);
                                        // bytesRead=recv(sockfd, buf, MAXBUFFLEN, 0);
                                        // buf[bytesRead]='\0';        //to remove the garbage value at the end of buf
                                        // printf("s: %s\n", buf);
                                        // if(bytesRead<0)
                                        // {
                                        //     perror("Unable to read from socket\n");
                                        //     exit(0);
                                        // }
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