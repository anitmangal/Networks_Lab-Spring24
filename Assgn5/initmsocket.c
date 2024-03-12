#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/select.h>
#include "msocket.h"


struct sembuf pop, vop;
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

void R() {
    fd_set readfds;
    FD_ZERO(&readfds);
    int nfds = 0;
    while (1) {
        fd_set readyfds = readfds;
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        int retval = select(nfds + 1, &readyfds, NULL, NULL, &tv);

        if (retval == -1) {
            perror("select()");
        }
        else if (retval == 0) {
            //timeout
            FD_ZERO(&readfds);
            P(sem_SM);
            for (int i = 0; i < N; i++) {
                if (SM[i].is_free == 0) {
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
                    int len = sizeof(cliaddr);
                    int n = recvfrom(SM[i].udp_socket_id, buffer, 1029, 0, (struct sockaddr *)&cliaddr, &len);
                    if (n < 0) {
                        perror("recvfrom()");
                    }
                    else {
                        if (buffer[0] == '0') {
                            // ACK
                            int seq = (buffer[1]-'0')*8 + (buffer[2]-'0')*4 + (buffer[3]-'0')*2 + (buffer[4]-'0');
                            int rwnd = (buffer[5]-'0')*4 + (buffer[6]-'0')*2 + (buffer[7]-'0');
                            if (SM[i].swnd.wndw[seq]) {
                                int j = SM[i].swnd.start_seq;
                                while (j != seq) {
                                    SM[i].swnd.wndw[j] = 0;
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
                                int nextseq = (SM[i].rwnd.start_seq+1)%16;
                                SM[i].rwnd.start_seq = nextseq;
                                SM[i].rwnd.size = (SM[i].rwnd.size+1)%16;
                                strcpy(SM[i].recv_buffer[seq], buffer+5);
                                SM[i].recv_buffer_valid[seq] = 1;
                            }
                            else {
                                // Send ACK for the last received sequence number
                                int lastseq = ((SM[i].rwnd.start_seq+16)-1)%16;     // Last acknowledged sequence number
                                struct sockaddr_in cliaddr;
                                cliaddr.sin_family = AF_INET;
                                inet_aton(SM[i].ip_address, &cliaddr.sin_addr);
                                cliaddr.sin_port = htons(SM[i].port);

                                char ack[8];
                                ack[0] = '0';
                                ack[1] = (lastseq>>3)%2 + '0';
                                ack[2] = (lastseq
            }
            V(sem_SM);
        }
    }
}

void S(){
    while(1){
        sleep(T/2);
        P(sem_SM);
        for(int i=0;i<N;i++){
            if(SM[i].is_free==0){
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(SM[i].port);
                inet_aton(SM[i].ip_address, &serv_addr.sin_addr);
                if(SM[i].lastSendTime+T<time(NULL)){
                    // check if any message in send window is not acknowledged, find that message in send buffer and send it
                    for(int j=0;j<16;j++){
                        if(SM[i].swnd.wndw[j]!=-1){
                            if(sendto(SM[i].udp_socket_id, SM[i].send_buffer[SM[i].swnd.wndw[j]], 1024, 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr))==-1){
                                perror("sendto()");
                                exit(1);
                            }
                            SM[i].lastSendTime=time(NULL);
                        }
                    }
                }
                else{
                    // gotta ask AG
                }
            }
        }
        V(sem_SM);
    }
}

int main(){


    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1;

    // create shared memory
    shmid_sock_info=shmget(IPC_PRIVATE, sizeof(SOCK_INFO), 0666|IPC_CREAT);
    // shmid_SM=shmget(IPC_PRIVATE, sizeof(struct SM_entry)*N, 0666|IPC_CREAT);
    sem1=semget(IPC_PRIVATE, 1, 0666|IPC_CREAT);
    sem2=semget(IPC_PRIVATE, 1, 0666|IPC_CREAT);
    sem_SM=semget(IPC_PRIVATE, 1, 0666|IPC_CREAT);
    sem_sock_info=semget(IPC_PRIVATE, 1, 0666|IPC_CREAT);

    sock_info=(SOCK_INFO *)shmat(shmid_sock_info, 0, 0);
    // SM=(struct SM_entry *)shmat(shmid_SM, 0, 0);

    //initialising shared memory
    sock_info->sock_id=0;
    sock_info->err_no=0;
    sock_info->ip_address[0]='\0';
    sock_info->port=0;

    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);
    semctl(sem_SM, 0, SETVAL, 1);
    semctl(sem_sock_info, SETVAL, 1);

    // create threads S, R, etc.



    // while loop

    while(1){
        P(sem1);
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
        V(sem2);
    }

    // Signal handler for SIGINT to release resources

    return 0;
}