#include "msocket.h"
#include <assert.h>

// Shared memory structure
struct SM_entry * SM;
SOCK_INFO * sock_info;

// semaphore and shared memory variables
int sem1, sem2;
int sem_sock_info, sem_SM;
int shmid_sock_info, shmid_SM;
struct sembuf pop, vop;

// Function to get the shared memory and semaphore
void get_shared_resources() {
    // Get keys
    int key_shmid_sock_info, key_shmid_SM, key_sem1, key_sem2, key_sem_SM, key_sem_sock_info;
    key_shmid_sock_info = ftok("makefile", 'A');
    key_shmid_SM = ftok("makefile", 'B');
    key_sem1 = ftok("makefile", 'C');
    key_sem2 = ftok("makefile", 'D');
    key_sem_SM = ftok("makefile", 'E');
    key_sem_sock_info = ftok("makefile", 'F');

    // Get the shared memory and semaphor
    shmid_sock_info = shmget(key_shmid_sock_info, sizeof(SOCK_INFO), 0666);
    shmid_SM = shmget(key_shmid_SM, sizeof(struct SM_entry)*N, 0666);
    sem1 = semget(key_sem1, 1, 0666);
    sem2 = semget(key_sem2, 1, 0666);
    sem_sock_info = semget(key_sem_sock_info, 1, 0666);
    sem_SM = semget(key_sem_SM, 1, 0666);

    if (shmid_sock_info == -1 || shmid_SM == -1 || sem1 == -1 || sem2 == -1 || sem_sock_info == -1 || sem_SM == -1) {
        perror("Error in getting shared memory or semaphore. Maybe initprocess is not running.");
        exit(1);
    }

    // Attach the shared memory
    sock_info = (SOCK_INFO *)shmat(shmid_sock_info, NULL, 0);
    SM = (struct SM_entry *)shmat(shmid_SM, NULL, 0);

    // Initialize the sembuf structures
    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1; vop.sem_op = 1;
}

// Function to check if a free entry is available in SM
int is_free_entry_available() {
    P(sem_SM);
    for(int i=0;i<N;i++){
        if(SM[i].is_free==1){
            V(sem_SM);
            return i;
        }
    }
    V(sem_SM);
    return -1;
}

int m_socket(int domain, int type, int protocol) {
    get_shared_resources();
    if (type != SOCK_MTP) {
        errno = EINVAL;
        return -1;
    }
    
    int i;
    P(sem_sock_info);

    if ((i=is_free_entry_available())==-1) {
        errno = ENOBUFS;
        sock_info->err_no=errno;

        // resetting sock_info
        sock_info->sock_id=0;
        sock_info->err_no=0;
        sock_info->ip_address[0]='\0';
        sock_info->port=0;
        return -1;
    }

    V(sem_sock_info);
    V(sem1);

    P(sem2);
    P(sem_sock_info);
    if(sock_info->sock_id==-1){
        errno = sock_info->err_no;
        sock_info->sock_id=0;
        sock_info->err_no=0;
        sock_info->ip_address[0]='\0';
        sock_info->port=0;
        V(sem_sock_info);
        return -1;
    }
    V(sem_sock_info);

    P(sem_SM);
    SM[i].is_free=0;
    SM[i].process_id=getpid();
    SM[i].udp_socket_id=sock_info->sock_id;
    V(sem_SM);
    
    // resetting sock_info
    P(sem_sock_info);
    sock_info->sock_id=0;
    sock_info->err_no=0;
    sock_info->ip_address[0]='\0';
    sock_info->port=0;
    V(sem_sock_info);

    return i;
}

int m_bind(char src_ip[], uint16_t src_port, char dest_ip[], uint16_t dest_port) {
    get_shared_resources();
    int sockfd=-1;
    P(sem_SM);
    for(int i=0;i<N;i++){
        if(SM[i].is_free==0 && SM[i].process_id==getpid()){
            sockfd=i;
            break;
        }
    }
    V(sem_SM);

    // should never execute... just for safety
    P(sem_sock_info);
    if(sockfd==-1 || sockfd ==N){
        errno = ENOBUFS;
        sock_info->sock_id=0;
        sock_info->err_no=0;
        sock_info->ip_address[0]='\0';
        sock_info->port=0;
        return -1;
    }

    sock_info->sock_id=SM[sockfd].udp_socket_id;
    strcpy(sock_info->ip_address, src_ip);
    sock_info->port=src_port;

    V(sem_sock_info);
    V(sem1);

    P(sem2);
    P(sem_sock_info);
    if(sock_info->sock_id==-1){
        errno = sock_info->err_no;
        sock_info->sock_id=0;
        sock_info->err_no=0;
        sock_info->ip_address[0]='\0';
        sock_info->port=0;
        return -1;
    }
    V(sem_sock_info);

    P(sem_SM);
    strcpy(SM[sockfd].ip_address, dest_ip);
    SM[sockfd].port=dest_port;

    printf("Binding successful: %d %d %d %d %16s %d %d\n", SM[sockfd].is_free, SM[sockfd].process_id, getpid(), SM[sockfd].udp_socket_id, SM[sockfd].ip_address, SM[sockfd].port, SM[sockfd].nospace);
    V(sem_SM);

    // resetting sock_info
    P(sem_sock_info);
    sock_info->sock_id=0;
    sock_info->err_no=0;
    sock_info->ip_address[0]='\0';
    sock_info->port=0;
    V(sem_sock_info);

    return 0;
}

ssize_t m_sendto(int m_sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen) {
    get_shared_resources();
    P(sem_SM);
    char * dest_ip;
    uint16_t dest_port;
    dest_ip = inet_ntoa(((struct sockaddr_in *)dest_addr)->sin_addr);
    dest_port=ntohs(((struct sockaddr_in *)dest_addr)->sin_port);

    printf("m_sendto: %16s %d %16s %d\n", SM[m_sockfd].ip_address, SM[m_sockfd].port, dest_ip, dest_port);
    if(strcmp(SM[m_sockfd].ip_address, dest_ip)!=0 || SM[m_sockfd].port!=dest_port){
        errno = ENOTCONN;
        V(sem_SM);
        return -1;
    }

    if(SM[m_sockfd].send_buffer_sz==0){      // send buffer is full
        errno=ENOBUFS;
        V(sem_SM);
        return -1;
    }

    int seq_no=SM[m_sockfd].swnd.start_seq;
    while(SM[m_sockfd].swnd.wndw[seq_no]!=-1){
        seq_no=(seq_no+1)%16;
    }

    int buff_index=0;
    int f=1;
    for(buff_index=0;buff_index<10;buff_index++){
        f=1;
        for(int i=0;i<16;i++){
            if(SM[m_sockfd].swnd.wndw[i]==buff_index){
                printf("%d %d %d\n", i, SM[m_sockfd].swnd.wndw[i], buff_index);
                f=0;
                break;
            }
        }
        if(f==1){
            break;
        }
    }

    // should never execute... just for safety
    if(f==0){
        errno=ENOBUFS;
        V(sem_SM);
        return -1;
    }

    SM[m_sockfd].swnd.wndw[seq_no]=buff_index;
    // SM[m_sockfd].swnd.size--;
    strncpy(SM[m_sockfd].send_buffer[buff_index], buf, len);
    SM[m_sockfd].lastSendTime[seq_no]=-1;
    SM[m_sockfd].send_buffer_sz--;
    SM[m_sockfd].lengthOfMessageSendBuffer[buff_index]=len;
    printf("m_sendto: %d %d %1024s %ld %d %d %d", SM[m_sockfd].swnd.wndw[seq_no], SM[m_sockfd].swnd.size, SM[m_sockfd].send_buffer[buff_index], SM[m_sockfd].lastSendTime[seq_no], SM[m_sockfd].send_buffer_sz, seq_no, buff_index);

    V(sem_SM);
    return len;
}

ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen) {
    get_shared_resources();
    P(sem_SM);
    printf("m_recvfrom: starting\n");
    if (sockfd < 0 || sockfd >= N || SM[sockfd].is_free) {
        errno = EBADF;
        V(sem_SM);
        return -1;
    }
    printf("m_recvfrom: 1\n");
    struct SM_entry * sm = SM + sockfd;
    if (sm->recv_buffer_valid[sm->recv_buffer_pointer]) {
        printf("m_recvfrom: 2\n");
        printf("m_recvfrom: %1024s\n", sm->recv_buffer[sm->recv_buffer_pointer]);
        sm->recv_buffer_valid[sm->recv_buffer_pointer] = 0;
        sm->rwnd.size++;
        int seq = -1;
        for (int i = 0; i < 16; i++) if (sm->rwnd.wndw[i] == sm->recv_buffer_pointer) seq = i;
        printf("m_recvfrom: seq %d\n", seq);
        // assert(seq != -1);
        sm->rwnd.wndw[seq] = -1;
        sm->rwnd.wndw[(seq+5)%16] = sm->recv_buffer_pointer;
        printf("m_recvfrom: 4\n");
        strncpy(buf, sm->recv_buffer[sm->recv_buffer_pointer], (len < 1024) ? len : 1024);
        printf("m_recvfrom: 5\n");
        sm->recv_buffer_pointer = (sm->recv_buffer_pointer + 1) % 5;
        V(sem_SM);
        printf("m_recvfrom: 6\n");
        return (len < 1024) ? len : 1024;
    }
    printf("m_recvfrom: 3\n");
    errno = ENOMSG;
    V(sem_SM);
    return -1;
}

int m_close(int sockfd) {
    get_shared_resources();
    close(SM[sockfd].udp_socket_id);
    SM[sockfd].is_free=1;
    return 0;
}

int dropMessage() {
    float r = (float)rand() / (float)RAND_MAX;
    if (r < p) return 1;
    return 0;
}