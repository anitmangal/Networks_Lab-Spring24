#include <stdio.h>
#include<pthread.h> //????
#include<string.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<netinet/in.h>
#include<sys/sem.h>
#include"msocket.h"

// pthread_mutex_t sem1, sem2;
// pthread_cond_t cond1, cond2;
// int wait1, wait2;

#define P(s) semop(s, &pop, 1)  /* pop is the structure we pass for doing
				   the P(s) operation */
#define V(s) semop(s, &vop, 1)  /* vop is the structure we pass for doing
				   the V(s) operation */


int shmid_sock_info;
int sem1, sem2;
struct sembuf pop, vop;
/*

    Each message will have a 5 bit header. 1 bit for type (0 = ACK, 1 = DATA), 4 bit for sequence number.
    Next bits are either rwnd size (for ACK) or 1 KB data (for DATA).

*/

int main(){
    key_t key_sock_info=ftok("msocket.h", 'A');
    key_t key_sem1=ftok("msocket.h", 'B');
    key_t key_sem2=ftok("msocket.h", 'C');


    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1;

    SOCK_INFO *sock_info;

    // create shared memory
    shmid_sock_info=shmget(key_sock_info, sizeof(SOCK_INFO), 0666|IPC_CREAT);
    sem1=semget(key_sem1, 1, 0666|IPC_CREAT);
    sem2=semget(key_sem2, 1, 0666|IPC_CREAT);
    sock_info=(SOCK_INFO *)shmat(shmid_sock_info, 0, 0);

    //initialising shared memory
    sock_info->sock_id=0;
    sock_info->err_no=0;
    sock_info->ip_address[0]='\0';
    sock_info->port=0;

    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);

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