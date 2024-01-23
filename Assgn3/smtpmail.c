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

int main(int argc, char * argv[]) {
    if (argc < 2) {
        printf("Usage: ./smtpmail <port>\n");
        exit(0);
    }
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


    listen(sockid, 5);
    int newsockid;
    struct sockaddr_in cli_addr;

    // Iterative server
    while(1) {
        int clilen = sizeof(cli_addr);
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
            strcpy(buf, "220 [");
            strcat(buf, inet_ntoa(cli_addr.sin_addr));
            strcat(buf, "] Service ready\r\n");
            send(newsockid, buf, strlen(buf), 0);
            recv(newsockid, buf, 100, 0);
            if (strncmp(buf, "HELO", 4) == 0) {
                strcpy(buf, "250 OK Hello ");
                strcat(buf, inet_ntoa(cli_addr.sin_addr));
                strcat(buf, " Pleased to meet you\r\n");
                send(newsockid, buf, strlen(buf), 0);
                recv(newsockid, buf, 100, 0);
                if (strncmp(buf, "MAIL FROM:", 10) == 0) {
                    int i = 11;
                    char from_user[100], from_domain[100];
                    while (buf[i] != '@') {
                        from_user[i-11] = buf[i];
                        i++;
                    }
                    from_user[i-11] = '\0';
                    i++;
                    int j = 0;
                    while (buf[i+j] != '>') {
                        from_domain[j] = buf[i+j];
                        j++;
                    }
                    from_domain[j] = '\0';
                    strcpy(buf, "250 OK\r\n");
                    send(newsockid, buf, strlen(buf), 0);
                    recv(newsockid, buf, 100, 0);
                    if (strncmp(buf, "RCPT TO:", 8) == 0) {
                        i = 9;
                        char to_user[100], to_domain[100];
                        while (buf[i] != '@') {
                            to_user[i-9] = buf[i];
                            i++;
                        }
                        to_user[i-9] = '\0';
                        i++;
                        j = 0;
                        while (buf[i+j] != '>') {
                            to_domain[j] = buf[i+j];
                            j++;
                        }
                        to_domain[j] = '\0';
                        char filepath[100];
                        strcpy(filepath, to_user);
                        strcat(to_user, "/mymailbox");
                        FILE * f = fopen(filepath, "r");
                        if (f == NULL) {
                            strcpy(buf, "550 No such user here\r\n");
                            send(newsockid, buf, strlen(buf), 0);
                            close(newsockid);
                        }
                        else {
                            fclose(f);
                            f = fopen(to_user, "a");
                            recv(newsockid, buf, 100, 0);
                            if (strncmp(buf, "DATA", 4) == 0) {
                                strcpy(buf, "354 End data with <CR><LF>.<CR><LF>\r\n");
                                send(newsockid, buf, strlen(buf), 0);
                                char writebuf[100];
                                int write_ind = 0, recv_ind = 0, cnt = 0;
                                int recvbytes = recv(newsockid, buf, 100, 0);
                                while (cnt < 3) {
                                    while (recv_ind < recvbytes && buf[recv_ind] != '\r') {
                                        writebuf[write_ind] = buf[recv_ind];
                                        write_ind++;
                                        recv_ind++;
                                    }
                                    if (recv_ind == recvbytes) {
                                        recvbytes = recv(newsockid, buf, 100, 0);
                                        recv_ind = 0;
                                    }
                                    else {
                                        writebuf[write_ind] = '\0';
                                        fprintf(f, "%s\n", writebuf);
                                        write_ind = 0;
                                        recv_ind += 2;
                                        cnt++;
                                    }
                                }

                                strcpy(writebuf, "Recieved: ");
                                time_t t = time(NULL);
                                struct tm * tm = localtime(&t);
                                char timebuf[100];
                                strftime(timebuf, 100, "%d/%m/%y:%H:%M", tm);
                                strcat(writebuf, timebuf);
                                fprintf(f, "%s\n", writebuf);

                                while(1) {
                                    while (recv_ind < recvbytes && buf[recv_ind] != '\r') {
                                        writebuf[write_ind] = buf[recv_ind];
                                        write_ind++;
                                        recv_ind++;
                                    }
                                    if (recv_ind == recvbytes) {
                                        recvbytes = recv(newsockid, buf, 100, 0);
                                        recv_ind = 0;
                                    }
                                    else {
                                        writebuf[write_ind] = '\0';
                                        fprintf(f, "%s\n", writebuf);
                                        if (write_ind == 1 && writebuf[0] == '.') {
                                            break;
                                        }
                                        write_ind = 0;
                                        recv_ind += 2;
                                    }
                                }

                                fclose(f);
                                strcpy(buf, "250 OK\r\n");
                                send(newsockid, buf, strlen(buf), 0);
                                recv(newsockid, buf, 100, 0);
                                if (strncmp(buf, "QUIT", 4) == 0) {
                                    strcpy(buf, "221 [");
                                    strcat(buf, inet_ntoa(cli_addr.sin_addr));
                                    strcat(buf, "] Service closing transmission channel\r\n");
                                    send(newsockid, buf, strlen(buf), 0);
                                    close(newsockid);
                                    exit(0);
                                }
                            }
                        }
                    }
                }
            }
        }
        else {
            close(newsockid);
        }
    }

}