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
    struct sockaddr_in serv_addr, local_addr;
    socklen_t local_addr_len = sizeof(local_addr);
    char buf[100];
    int bytesRead;
    size_t maxLen=100;
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

        // Get local address information
        if (getsockname(sockfd, (struct sockaddr *)&local_addr, &local_addr_len) < 0) {
            perror("Unable to get local address\n");
            exit(EXIT_FAILURE);
        }

        char local_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));

        // shud get 220
        bytesRead=recv(sockfd, buf, 100, 0);
        if(bytesRead<0)
        {
            perror("Unable to read from socket\n");
            exit(0);
        }

        // buf[bytesRead]='\0'; //debug
        // printf("%s", buf);  //debug
        strcpy(buf,"HELO ");
        strcat(buf, local_ip);                                       // CLIENT IP here, Section 4.1.1.1

        send(sockfd, buf, strlen(buf), 0);

        // shud get 250
        bytesRead=recv(sockfd, buf, 100, 0);
        // buf[bytesRead]='\0'; //debug
        // printf("%s", buf);  //debug

        char *from, *to, *subject, *body[50];
        from=(char *)malloc(100*sizeof(char));
        to=(char *)malloc(100*sizeof(char));
        subject=(char *)malloc(100*sizeof(char));

        char tmp;
        scanf("%c", &tmp); // to consume the \n after the choice, can we do without it????
        printf("Enter the mail:\n");

        // check format, unsure about the <CR><LF> part         ---> <CR><LF> is \r\n, check if fixed now!
        getline(&from, &maxLen, stdin);
        for(int i=0; i<100; i++)
        {
            if(from[i]=='\n')
            {
                // from[i]='\r';
                // from[i+1]='\n';
                from[i]='\0';
                break;
            }
        }

        getline(&to, &maxLen, stdin);
        for(int i=0; i<100; i++)
        {
            if(to[i]=='\n')
            {
                // to[i]='\r';
                // to[i+1]='\n';
                to[i]='\0';
                break;
            }
        }

        getline(&subject, &maxLen, stdin);
        for(int i=0; i<100; i++)
        {
            if(subject[i]=='\n')
            {
                // subject[i]='\r';
                // subject[i+1]='\n';
                subject[i]='\0';
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

        //from                                                      // Add <> for email address in both MAIL FROM and RCPT TO and no space after :, check if fixed now!
        strcpy(buf, "MAIL FROM:<");
        strcat(buf, from+6);
        strcat(buf, ">\r\n\0");
        // printf("%s", buf);  //debug
        send(sockfd, buf, strlen(buf), 0);

        // shud get 250
        bytesRead=recv(sockfd, buf, 100, 0);
        // buf[bytesRead]='\0'; //debug
        // printf("%s", buf);  //debug

        //to
        strcpy(buf, "RCPT TO:<");
        strcat(buf, to+4);
        strcat(buf, ">\r\n\0");

        // printf("%s", buf);  //debug
        send(sockfd, buf, strlen(buf), 0);

        // shud get 250
        bytesRead=recv(sockfd, buf, 100, 0);
        // buf[bytesRead]='\0'; //debug
        // printf("%s", buf);  //debug

        send(sockfd, "DATA\r\n", 6, 0);

        // shud get 354
        bytesRead=recv(sockfd, buf, 100, 0);
        // buf[bytesRead]='\0'; //debug
        // printf("%s", buf);  //debug

        // sending mail data
        for(int i=0; i<100; i++)        //added these 3 for senidng right data
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
        // printf("%s", from);  //debug
        send(sockfd, from, strlen(from), 0);

        // printf("%s", to);  //debug
        send(sockfd, to, strlen(to), 0);

        // printf("%s", subject);  //debug
        send(sockfd, subject, strlen(subject), 0);
        for(int i=0; i<lines; i++)
        {
            // printf("%s", body[i]);  //debug
            send(sockfd, body[i], strlen(body[i]), 0);
        }

        // shud get 250
        bytesRead=recv(sockfd, buf, 100, 0);
        // buf[bytesRead]='\0'; //debug
        // printf("%s", buf);  //debug

        send(sockfd, "QUIT\r\n", 6, 0);

        // shud get 221
        bytesRead=recv(sockfd, buf, 100, 0);
        // buf[bytesRead]='\0'; //debug
        // printf("%s", buf);  //debug

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