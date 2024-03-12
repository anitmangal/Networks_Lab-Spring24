/*
    Anit Mangal
    21CS10005
    Computer Networks Lab Assignment 4
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>


// struct for peer table
typedef struct {
    char name[7];
    char ip[16];
    int port;
} peerTable;


// Functions to do operations on timeval structs
struct timeval add(struct timeval a, struct timeval b) {
    struct timeval result;
    result.tv_sec = a.tv_sec + b.tv_sec;
    result.tv_usec = a.tv_usec + b.tv_usec;
    if (result.tv_usec >= 1000000) {
        result.tv_sec++;
        result.tv_usec -= 1000000;
    }
    return result;
}

struct timeval sub(struct timeval a, struct timeval b) {
    struct timeval result;
    result.tv_sec = a.tv_sec - b.tv_sec;
    result.tv_usec = a.tv_usec - b.tv_usec;
    if (result.tv_usec < 0) {
        result.tv_sec--;
        result.tv_usec += 1000000;
    }
    return result;
}

struct timeval min(struct timeval a, struct timeval b) {
    if (a.tv_sec < b.tv_sec) return a;
    if (a.tv_sec > b.tv_sec) return b;
    if (a.tv_usec < b.tv_usec) return a;
    return b;
}

int equal(struct timeval a, struct timeval b) {
    return a.tv_sec == b.tv_sec && a.tv_usec == b.tv_usec;
}



int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    // Build peer table
    peerTable table[3];
    strcpy(table[0].name, "user_1");
    strcpy(table[0].ip, "127.0.0.1");
    table[0].port = 50000;
    strcpy(table[1].name, "user_2");
    strcpy(table[1].ip, "127.0.0.1");
    table[1].port = 50001;
    strcpy(table[2].name, "user_3");
    strcpy(table[2].ip, "127.0.0.1");
    table[2].port = 50002;

    int serversock, clientsock1, clientsock2;
    struct sockaddr_in serveraddr, clientaddr1, clientaddr2;

    // Find out which clients are to be served
    int client1, client2;
    if (table[0].port == atoi(argv[1])) {
        client1 = 1;
        client2 = 2;
    }
    else if (table[1].port == atoi(argv[1])) {
        client1 = 2;
        client2 = 0;
    }
    else if (table[2].port == atoi(argv[1])) {
        client1 = 0;
        client2 = 1;
    }
    
    // Create server socket and client sockets and bind server
    serversock = socket(AF_INET, SOCK_STREAM, 0);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(atoi(argv[1]));
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serversock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("bind");
        exit(1);
    }
    listen(serversock, 5);      // Listen for incoming connections
    clientsock1 = socket(AF_INET, SOCK_STREAM, 0);
    clientsock2 = socket(AF_INET, SOCK_STREAM, 0);

    // Build fd_set
    fd_set original_fds, check_fds;
    FD_ZERO(&original_fds);
    FD_SET(serversock, &original_fds);
    FD_SET(STDIN_FILENO, &original_fds);
    int max_fd = serversock+1;
    int conn1 = 0, conn2 = 0;   // To keep track of whether client sockets are connected or not

    /* Timeout mechanism:
        timeout: keeps default timeout value of 5 minutes
        currtimeout: keeps the current timeout value
        now: keeps the current time (system time)
        last: keeps the time of last activity of client1 and client2
    */
    struct timeval timeout, currtimeout, now, last[2];
    timeout.tv_sec = 300;
    timeout.tv_usec = 0;
    currtimeout = timeout;
    gettimeofday(&now, NULL);
    last[0] = last[1] = now;
    int timeout1 = 0, timeout2 = 0;

    while(1) {
        check_fds = original_fds;       // Reset check_fds
        gettimeofday(&now, NULL);       // Get system time
        currtimeout = timeout;          // By default, currtimeout is timeout
        // if client1 or client2 is connected, currtimeout is the minimum of timeout and time left for timeout for each
        if (conn1) {
            currtimeout = min(currtimeout, sub(add(last[0], timeout), now));
            // If updated by client1, timeout1 is set to 1
            if (equal(currtimeout, sub(add(last[0], timeout), now))) {
                timeout1 = 1;
                timeout2 = 0;
            }
            else timeout1 = 0;
        }
        if (conn2) {
            currtimeout = min(currtimeout, sub(add(last[1], timeout), now));
            if (equal(currtimeout, sub(add(last[1], timeout), now))) {
                timeout2 = 1;
                timeout1 = 0;
            }
            else timeout2 = 0;
        }

        // Select. If timeout1 or timeout2 is 1, select with currtimeout, else select with NULL
        int result;
        if (conn1 || conn2) result = select(max_fd, &check_fds, NULL, NULL, &currtimeout);
        else result = select(max_fd, &check_fds, NULL, NULL, NULL);

        // If select returned 0
        if (result == 0 && (timeout1 || timeout2)) {
            // If because of client 1, close its connection
            if(timeout1) {
                if (conn1 == 1) {
                    close(clientsock1);
                    clientsock1 = socket(AF_INET, SOCK_STREAM, 0);
                    conn1 = 0;
                    timeout1 = 0;
                }
            }
            // If because of client 2, close its connection
            if (timeout2) {
                if (conn2 == 1) {
                    close(clientsock2);
                    clientsock2 = socket(AF_INET, SOCK_STREAM, 0);
                    conn2 = 0;
                    timeout2 = 0;
                }
            }
        }
        // Result non zero, a socket is updated
        else {
            // STDIN updated
            if (FD_ISSET(STDIN_FILENO, &check_fds)) {
                // Find user
                char buffer[300];
                fgets(buffer, 300, stdin);
                int delimindex = 0;
                while (delimindex < 7 && buffer[delimindex] != '/') delimindex++;
                if (delimindex == 7) {
                    printf("Invalid user: too long\n");
                    continue;
                }
                char user[7];
                strncpy(user, buffer, delimindex);
                user[delimindex] = '\0';

                // Get port of server
                char port[6];
                strcpy(port, argv[1]);
                // Connect to client1 or client2
                if (strcmp(user, table[client1].name) == 0) {
                    if (conn1 == 0) {
                        clientaddr1.sin_family = AF_INET;
                        clientaddr1.sin_port = htons(table[client1].port);
                        clientaddr1.sin_addr.s_addr = inet_addr(table[client1].ip);
                        connect(clientsock1, (struct sockaddr *)&clientaddr1, sizeof(clientaddr1));
                        gettimeofday(&last[0], NULL);
                        conn1 = 1;
                    }
                    strcpy(buffer+1, port);
                    buffer[delimindex] = '/';
                    send(clientsock1, buffer, strlen(buffer), 0);   // Send server port and message to client1
                }
                else if (strcmp(user, table[client2].name) == 0) {
                    if (conn2 == 0) {
                        clientaddr2.sin_family = AF_INET;
                        clientaddr2.sin_port = htons(table[client2].port);
                        clientaddr2.sin_addr.s_addr = inet_addr(table[client2].ip);
                        connect(clientsock2, (struct sockaddr *)&clientaddr2, sizeof(clientaddr2));
                        gettimeofday(&last[1], NULL);
                        conn2 = 1;
                    }
                    strcpy(buffer+1, port);
                    buffer[delimindex] = '/';
                    send(clientsock2, buffer, strlen(buffer), 0);       // Send server port and message to client2
                }
                else {
                    printf("Invalid user: %s\n", user);
                }
            }
            // Server socket updated
            if (FD_ISSET(serversock, &check_fds)) {

                struct sockaddr_in clientaddr;
                int clientsock;
                socklen_t clientlen = sizeof(clientaddr);
                clientsock = accept(serversock, (struct sockaddr *)&clientaddr, &clientlen);        // Accept incoming connection

                // Get data from client till newline
                char buffer[300], recvbuffer[300];
                int recvbytes = recv(clientsock, recvbuffer, 300, 0);
                int recv_ind = 0, buff_ind = 0;
                while(1) {
                    while (recv_ind < recvbytes && recvbuffer[recv_ind] != '\n') {
                        buffer[buff_ind++] = recvbuffer[recv_ind++];
                    }
                    if (recv_ind == recvbytes) {
                        recvbytes = recv(clientsock, recvbuffer, 300, 0);
                        recv_ind = 0;
                    }
                    else {
                        buffer[buff_ind++] = '\n';
                        buffer[buff_ind] = '\0';
                        break;
                    }
                }

                // Find user from port and print message
                char user[7], port[6];
                strncpy(port, buffer+1, 5);
                port[5] = '\0';
                if (atoi(port) == table[client1].port) strcpy(user, table[client1].name);
                else if (atoi(port) == table[client2].port) strcpy(user, table[client2].name);
                else strcpy(user, "undef");
                printf("Mesage from <%s>: %s", user, buffer+7);
                fflush(stdout);
            }
        }
    }
}