#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>
#include "msocket.h"


/*

    Each message will have a 5 bit header. 1 bit for type (0 = ACK, 1 = DATA), 4 bit for sequence number.
    Next bits are either 3-bit rwnd size (for ACK) or 1 KB data (for DATA).

*/

void sigHandler(int sig){
    // release resources
    shmdt(sock_info);
    shmdt(SM);
    shmctl(shmid_sock_info, IPC_RMID, NULL);
    shmctl(shmid_SM, IPC_RMID, NULL);
    semctl(sem1, 0, IPC_RMID);
    semctl(sem2, 0, IPC_RMID);
    semctl(sem_SM, 0, IPC_RMID);
    semctl(sem_sock_info, 0, IPC_RMID);
    exit(0);
}

void * R() {
    fd_set readfds;
    FD_ZERO(&readfds);
    int nfds = 0;
    while (1) {
        fd_set readyfds = readfds;
        struct timeval tv;
        tv.tv_sec = T;
        tv.tv_usec = 0;
        int retval = select(nfds + 1, &readyfds, NULL, NULL, &tv);

        if (retval == -1) {
            perror("select()");
        }
        if (retval <= 0) {
            //timeout
            printf("R: Timeout\n");
            FD_ZERO(&readfds);
            P(sem_SM);
            printf("R: Timeout1\n");
            for (int i = 0; i < N; i++) {
                if (SM[i].is_free == 0) {
                    printf("R: Setting %d\n", SM[i].udp_socket_id);
                    FD_SET(SM[i].udp_socket_id, &readfds);
                    if (SM[i].udp_socket_id > nfds) nfds = SM[i].udp_socket_id;

                    // Check if the receive window has space, might be a PROBLEM here
                    if (SM[i].nospace && SM[i].rwnd.size > 0) {
                        SM[i].nospace = 0;
                        int lastseq = ((SM[i].rwnd.start_seq+16)-1)%16;     // Last acknowledged sequence number
                        struct sockaddr_in cliaddr;
                        cliaddr.sin_family = AF_INET;
                        inet_aton(SM[i].ip_address, &cliaddr.sin_addr);
                        cliaddr.sin_port = htons(SM[i].port);

                        char ack[8];
                        ack[0] = '0';
                        ack[1] = (lastseq>>3)%2 + '0';
                        ack[2] = (lastseq>>2)%2 + '0';
                        ack[3] = (lastseq>>1)%2 + '0';
                        ack[4] = (lastseq)%2 + '0';
                        ack[5] = (SM[i].rwnd.size>>2)%2 + '0';
                        ack[6] = (SM[i].rwnd.size>>1)%2 + '0';
                        ack[7] = (SM[i].rwnd.size)%2 + '0';
                        printf("R: Sending ACK %d\n", lastseq);
                        sendto(SM[i].udp_socket_id, ack, 8, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                    }
                }
            }
            V(sem_SM);
        }
        else {
            P(sem_SM);
            for (int i = 0; i < N; i++) {
                if (FD_ISSET(SM[i].udp_socket_id, &readyfds)) {
                    char buffer[1029];
                    struct sockaddr_in cliaddr;
                    unsigned int len = sizeof(cliaddr);
                    printf("R: Receiving from %d\n", SM[i].udp_socket_id); // Change this, change works???
                    int n = recvfrom(SM[i].udp_socket_id, buffer, 1029, 0, (struct sockaddr *)&cliaddr, &len);
                    if (n < 0) {
                        perror("recvfrom()");
                    }
                    else {
                        if (buffer[0] == '0') {
                            // ACK
                            int seq = (buffer[1]-'0')*8 + (buffer[2]-'0')*4 + (buffer[3]-'0')*2 + (buffer[4]-'0');
                            int rwnd = (buffer[5]-'0')*4 + (buffer[6]-'0')*2 + (buffer[7]-'0');
                            if (SM[i].swnd.wndw[seq] >= 0) {
                                int j = SM[i].swnd.start_seq;
                                while (j != seq) {
                                    SM[i].swnd.wndw[j] = -1;
                                    SM[i].send_buffer_sz++;
                                    j = (j+1)%16;
                                }
                                SM[i].swnd.start_seq = (seq+1)%16;
                            }
                            SM[i].swnd.size = rwnd;
                        }
                        else {
                            // DATA
                            int seq = (buffer[1]-'0')*8 + (buffer[2]-'0')*4 + (buffer[3]-'0')*2 + (buffer[4]-'0');
                            if (seq == SM[i].rwnd.start_seq) {
                                // In order message
                                int buff_ind = SM[i].rwnd.wndw[seq];
                                strncpy(SM[i].recv_buffer[buff_ind], buffer+5, 1024);
                                SM[i].recv_buffer_valid[buff_ind] = 1;
                                SM[i].rwnd.size--;
                                // Find the next in order message
                                while (SM[i].rwnd.wndw[SM[i].rwnd.start_seq] >= 0 && SM[i].recv_buffer_valid[SM[i].rwnd.wndw[SM[i].rwnd.start_seq]] == 1) {
                                    SM[i].rwnd.start_seq = (SM[i].rwnd.start_seq+1)%16;
                                }
                            }
                            else {
                                // Keep out of order message if in rwnd, else discard. If duplicate, discard.
                                if (SM[i].rwnd.wndw[seq] >= 0 && !SM[i].recv_buffer_valid[SM[i].rwnd.wndw[seq]]) {
                                    int buff_ind = SM[i].rwnd.wndw[seq];
                                    strncpy(SM[i].recv_buffer[buff_ind], buffer+5, 1024);
                                    SM[i].recv_buffer_valid[buff_ind] = 1;
                                    SM[i].rwnd.size--;
                                }
                            }
                            // Nospace
                            if (SM[i].rwnd.size == 0) {
                                SM[i].nospace = 1;
                            }
                            // Send ACK
                            seq = (SM[i].rwnd.start_seq+16-1)%16;
                            char ack[8];
                            ack[0] = '0';
                            ack[1] = (seq>>3)%2 + '0';
                            ack[2] = (seq>>2)%2 + '0';
                            ack[3] = (seq>>1)%2 + '0';
                            ack[4] = (seq)%2 + '0';
                            ack[5] = (SM[i].rwnd.size>>2)%2 + '0';
                            ack[6] = (SM[i].rwnd.size>>1)%2 + '0';
                            ack[7] = (SM[i].rwnd.size)%2 + '0';
                            sendto(SM[i].udp_socket_id, ack, 8, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                        }
                    }
                }
            }
            V(sem_SM);
        }
    }
}

void * S(){
    while(1){
        sleep(T/2);
        P(sem_SM);
        for(int i=0;i<N;i++){
            if(SM[i].is_free==0){
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(SM[i].port);
                inet_aton(SM[i].ip_address, &serv_addr.sin_addr);
                int timeout=0;
                for(int j=0;j<16;j++){
                    if(SM[i].lastSendTime[j]!=-1 && time(NULL)-SM[i].lastSendTime[j]>T){
                        timeout=1;
                        break;
                    }
                }
                if(timeout){
                    int j=SM[i].swnd.start_seq;
                    while(j<SM[i].swnd.start_seq+5){
                        if(SM[i].swnd.wndw[j]!=-1){
                            char buffer[1029];
                            buffer[0]='1';
                            buffer[1]=(j>>3)%2+'0';
                            buffer[2]=(j>>2)%2+'0';
                            buffer[3]=(j>>1)%2+'0';
                            buffer[4]=(j)%2+'0';
                            strncpy(buffer+5, SM[i].send_buffer[SM[i].swnd.wndw[j]], 1024);
                            printf("S: Resending %d\n", j);
                            int sendb = sendto(SM[i].udp_socket_id, buffer, 1029, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                            SM[i].lastSendTime[j]=time(NULL);
                            printf("S: Resent %d at %d\n", sendb, j);
                        }
                        j++;
                    }
                }
                else{
                    int j=SM[i].swnd.start_seq;
                    while(j<SM[i].swnd.start_seq+5){
                        if(SM[i].swnd.wndw[j]!=-1 && SM[i].lastSendTime[j]==-1){
                            char buffer[1029];
                            buffer[0]='1';
                            buffer[1]=(j>>3)%2+'0';
                            buffer[2]=(j>>2)%2+'0';
                            buffer[3]=(j>>1)%2+'0';
                            buffer[4]=(j)%2+'0';
                            strncpy(buffer+5, SM[i].send_buffer[SM[i].swnd.wndw[j]], 1024);
                            printf("S: Sending %d\n", j);
                            int sendb = sendto(SM[i].udp_socket_id, buffer, 1029, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                            SM[i].lastSendTime[j]=time(NULL);
                            printf("S: Sent %d at %d\n", sendb, j);
                        }
                        j++;
                    }
                }
            }
        }
        V(sem_SM);
    }
}

void * G() {
    // Garbage collector thread to identify killed messages and close their sockets if not closed already in the SM table
    while (1) {
        sleep(T);
        P(sem_SM);
        for (int i = 0; i < N; i++) {
            if (SM[i].is_free) continue;                    // Free entry
            if (kill(SM[i].process_id, 0) == 0) continue;   // Process still running
            printf("G: Process %d killed\n", SM[i].process_id);
            int socket_id = SM[i].udp_socket_id;
            int j;
            for (j = 0; j < N; j++) if (i != j && SM[j].udp_socket_id == socket_id) break;
            if (j == N) m_close(i);                   // No other entry with same socket_id, close the socket and mark the entry as free
            else SM[i].is_free = 1;                    // The socket file descriptor was replaced by another socket, just mark the entry as free
        }
        V(sem_SM);
    }

}

int main(){
    signal(SIGINT, sigHandler); // Ctrl+C will release resources and exit

    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1;

    // Get keys
    int key_shmid_sock_info, key_shmid_SM, key_sem1, key_sem2, key_sem_SM, key_sem_sock_info;
    key_shmid_sock_info = ftok("makefile", 'A');
    key_shmid_SM = ftok("makefile", 'B');
    key_sem1 = ftok("makefile", 'C');
    key_sem2 = ftok("makefile", 'D');
    key_sem_SM = ftok("makefile", 'E');
    key_sem_sock_info = ftok("makefile", 'F');


    // create shared memory and semaphores
    shmid_sock_info=shmget(key_shmid_sock_info, sizeof(SOCK_INFO), 0666|IPC_CREAT);
    shmid_SM=shmget(key_shmid_SM, sizeof(struct SM_entry)*N, 0666|IPC_CREAT);
    sem1=semget(key_sem1, 1, 0666|IPC_CREAT);
    sem2=semget(key_sem2, 1, 0666|IPC_CREAT);
    sem_SM=semget(key_sem_SM, 1, 0666|IPC_CREAT);
    sem_sock_info=semget(key_sem_sock_info, 1, 0666|IPC_CREAT);

    printf("%d %d %d %d %d %d\n", shmid_sock_info, shmid_SM, sem1, sem2, sem_SM, sem_sock_info);

    // attach shared memory
    sock_info=(SOCK_INFO *)shmat(shmid_sock_info, 0, 0);
    SM=(struct SM_entry *)shmat(shmid_SM, 0, 0);

    // initialising sock_info
    sock_info->sock_id=0;
    sock_info->err_no=0;
    sock_info->ip_address[0]='\0';
    sock_info->port=0;

    // initialising SM
    for(int i=0;i<N;i++){
        SM[i].is_free=1;
        SM[i].udp_socket_id=0;
        SM[i].ip_address[0]='\0';
        SM[i].port=0;

        // swnd. Need to initialise swnd.wndw
        for(int j=0;j<16;j++) SM[i].swnd.wndw[j]=-1;          // Change this, change works???
        SM[i].swnd.size=5;
        SM[i].swnd.start_seq=1;
        SM[i].send_buffer_sz=10;

        // rwnd
        for (int j = 0; j < 16; j++) {
            if (j > 0 && j < 6) SM[i].rwnd.wndw[j] = j-1;
            else SM[i].rwnd.wndw[j] = -1;
        }
        SM[i].rwnd.size=5;
        SM[i].rwnd.start_seq=1;

        for (int j = 0; j < 5; j++) SM[i].recv_buffer_valid[j] = 0;
        SM[i].recv_buffer_pointer=0;

        SM[i].nospace=0;
        for(int j=0;j<16;j++) SM[i].lastSendTime[j]=-1;           // Change this, change works????
    }

    // initialising semaphores
    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);
    semctl(sem_SM, 0, SETVAL, 1);
    semctl(sem_sock_info, 0, SETVAL, 1);

    // create threads S, R, etc.
    pthread_t S_thread, R_thread, G_thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&S_thread, &attr, S, NULL);
    pthread_create(&R_thread, &attr, R, NULL);
    pthread_create(&G_thread, &attr, G, NULL);


    // while loop

    while(1){
        P(sem1);
        P(sem_sock_info);
        if(sock_info->sock_id==0 && sock_info->ip_address[0]=='\0' && sock_info->port==0){
            int sock_id=socket(AF_INET, SOCK_DGRAM, 0);
            if(sock_id==-1){
                sock_info->sock_id=-1;
                sock_info->err_no=errno;
            }
            else{
                sock_info->sock_id=sock_id;
            }
        }
        else{
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(sock_info->port);
            inet_aton(sock_info->ip_address, &serv_addr.sin_addr);
            if(bind(sock_info->sock_id, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0){
                sock_info->sock_id=-1;
                sock_info->err_no=errno;
            }
        }
        V(sem_sock_info);
        V(sem2);
    }

    return 0;
}