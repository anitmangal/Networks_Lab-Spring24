#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Usage: ./mailclient <server_IP> <smtp_port> <pop3_port>\n");
        exit(0);
    }
    int sockfd;
    struct sockaddr_in serv_addr;
    char buf[100];
    int bytesRead, maxLen=100;
    char *username, *password;
    username=(char *)malloc(100*sizeof(char));
    password=(char *)malloc(100*sizeof(char));

    // ask for username and password
    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);

    // ask for choice
    printf("1. Manage Mail\n2. Send Mail\n3. Quit\nWhatchu wanna do?: ");
    int choice;
    scanf("%d", &choice);
    switch (choice)
    {
    case 1:
    {
        break;
    }
    case 2:
    {
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

        // shud get 220
        bytesRead=recv(sockfd, buf, 100, 0);
        if(bytesRead<0)
        {
            perror("Unable to read from socket\n");
            exit(0);
        }

        // buf[bytesRead]='\0';
        strcpy(buf,"HELO ");
        strcat(buf, argv[1]);                                       // CLIENT IP here, Section 4.1.1.1

        send(sockfd, buf, strlen(buf), 0);

        // shud get 250
        recv(sockfd, buf, 100, 0);

        char *from, *to, *subject, *body[50];
        from=(char *)malloc(100*sizeof(char));
        to=(char *)malloc(100*sizeof(char));
        subject=(char *)malloc(100*sizeof(char));

        // check format, unsure about the <CR><LF> part         ---> <CR><LF> is \r\n
        getline(&from, &maxLen, stdin);
        for(int i=0; i<100; i++)
        {
            if(from[i]=='\n')
            {
                from[i]='\r';
                from[i+1]='\n';
                from[i+2]='\0';
                break;
            }
        }

        getline(&to, &maxLen, stdin);
        for(int i=0; i<100; i++)
        {
            if(to[i]=='\n')
            {
                to[i]='\r';
                to[i+1]='\n';
                to[i+2]='\0';
                break;
            }
        }

        getline(&subject, &maxLen, stdin);
        for(int i=0; i<100; i++)
        {
            if(subject[i]=='\n')
            {
                subject[i]='\r';
                subject[i+1]='\n';
                subject[i+2]='\0';
                break;
            }
        }
        int lines = 0;
        while (1)
        {
            body[lines] = (char *)malloc(100 * sizeof(char));
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
        lines++;

        //from                                                      // Add <> for email address in both MAIL FROM and RCPT TO and no space after :
        strcpy(buf, "MAIL FROM: ");
        strcat(buf, from+6);

        send(sockfd, buf, strlen(buf), 0);

        // shud get 250
        recv(sockfd, buf, 100, 0);

        //to
        strcpy(buf, "RCPT TO: ");
        strcat(buf, to+4);

        send(sockfd, buf, strlen(buf), 0);

        // shud get 250
        recv(sockfd, buf, 100, 0);

        send(sockfd, "DATA\r\n", 6, 0);

        // shud get 354
        recv(sockfd, buf, 100, 0);

        // sending mail data
        
        send(sockfd, from, strlen(from), 0);
        send(sockfd, to, strlen(to), 0);
        send(sockfd, subject, strlen(subject), 0);
        for(int i=0; i<lines; i++)
        {
            send(sockfd, body[i], strlen(body[i]), 0);
        }

        // shud get 250
        recv(sockfd, buf, 100, 0);

        send(sockfd, "QUIT\r\n", 6, 0);

        // shud get 221
        recv(sockfd, buf, 100, 0);

        close(sockfd);
        break;
    }
    case 3:
    {
        exit(0);
    }
    }

    exit(0);
}