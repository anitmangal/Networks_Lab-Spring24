#ifndef MSOCKET_H
#define MSOCKET_H

#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h> 
// Custom socket type for MTP
#define SOCK_MTP 3
#define ENOTBOUND 1000  //????

// defining T
#define T 5
#define p 0.05
#define N 25    
                   
#define P(s) semop(s, &pop, 1)  /* pop is the structure we pass for doing
				   the P(s) operation */
#define V(s) semop(s, &vop, 1)  /* vop is the structure we pass for doing
				   the V(s) operation */

struct window {
    int wndw[16];
    /*
    SEND WINDOW - 
        -1: NotSent, Sent-Acknowledged
        i: Sent-NotAcknowledged, ith message in send buffer for send window.
    RECEIVE WINDOW -
        -1: Not Expected
        i: Expected, ith message in receive buffer for receive window.
    */
    int size;
    int start_seq;              // start sequence number of the window
};

// SM structure
struct SM_entry {
    int is_free;               // (i) whether the MTP socket with ID i is free or allotted
    pid_t process_id;          // (ii) process ID for the process that created the MTP socket
    int udp_socket_id;         // (iii) mapping from the MTP socket i to the corresponding UDP socket ID
    char ip_address[16];       // (iv) IP address of the other end of the MTP socket (assuming IPv4)
    uint16_t port;             // (iv) Port address of the other end of the MTP socket
    char send_buffer[10][1024];    // (v) send buffer
    char recv_buffer[5][1024];    // (vi) receive buffer
    int recv_buffer_pointer;   // pointer to the next location in recv_buffer for application to read
    int recv_buffer_valid[5];  // If the message at index i in recv_buffer is valid or not
    struct window swnd;        // (vii) send window
    struct window rwnd;        // (viii) receive window
    int nospace;               // whether receive buffer has space or not
    time_t lastSendTime;      // last message send time
};



typedef struct SOCK_INFO{
    int sock_id;
    char ip_address[16];
    uint16_t port;
    int err_no;
} SOCK_INFO;

// Global variables
extern struct SM_entry* SM;
extern SOCK_INFO* sock_info;
extern int sem1, sem2;
extern int sem_sock_info, sem_SM;
extern int shmid_sock_info, shmid_SM;
extern struct sembuf pop, vop;

// Function prototypes
int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);
int dropMessage();

#endif  // MSOCKET_H
