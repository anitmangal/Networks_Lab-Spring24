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
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <net/if.h>
#include <asm-generic/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/select.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <pthread.h>



#define MAXLEN 32
#define interface_name "lo"
int ipID = 0;

// Struct for query table entry
typedef struct node{
    int id;
    int n;
    char query[sizeof(struct ethhdr) + sizeof(struct iphdr) + 4*sizeof(char)+8*(MAXLEN)*sizeof(char)];
    char domains[8][MAXLEN];
    int len;
    int tries;
    struct node *next;
}node;

void insertNode(node *head, node* newNode){
    node *temp = head;
    while(temp->next != NULL){
        temp = temp->next;
    }
    temp->next = newNode;
}

void deleteNode(node *head, int id){
    node *temp = head;
    while(temp->next != NULL){
        if(temp->next->id == id){
            node *temp2 = temp->next;
            temp->next = temp->next->next;
            free(temp2);
            return;
        }
        temp = temp->next;
    }
}

node *findNode(node *head, int id){
    node *temp = head;
    while(temp->next != NULL){
        if(temp->next->id == id){
            return temp->next;
        }
        temp = temp->next;
    }
    return NULL;
}

// Function to check if domain is valid
int isDomainValid(char *domain){
    int len = strlen(domain);
    if(len < 3 || len > 31){
        return 0;
    }
    int i = 0;
    while(i < len){
        if(domain[i] == '.'){
        }
        else if(domain[i] == '-'){
            if(i == 0 || i == len-1 || domain[i-1] == '-' || domain[i+1] == '-'){
                return 0;
            }
        }
        else if(!isalnum(domain[i])){
            return 0;
        }
        i++;
    }
    return 1;
}

// Mutex lock
pthread_mutex_t lock;

// Struct for arguments to pass to thread
typedef struct argument{
    int sockfd;
    struct sockaddr_ll destaddr;
    node *head;
}argument;

// Function to receive and retransmit queries (Thread function)
void *R(void *arg){
    // Set timeout for select, get list head and socket file descriptor
    struct timeval timeout;
    fd_set readfds;
    argument *argu = (argument *)arg;
    int sockfd = argu->sockfd;
    struct sockaddr_ll destaddr = argu->destaddr;
    pthread_mutex_lock(&lock);
    node *head = argu->head;
    pthread_mutex_unlock(&lock);
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    while(1){
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        int retval = select(sockfd+1, &readfds, NULL, NULL, &timeout);
        if(retval == -1){
            perror("select");
            exit(1);
        }
        else if(retval == 0){
            // Timeout
            pthread_mutex_lock(&lock);
            node *temp = head;
            while(temp->next != NULL){
                if(temp->next->tries == 4){
                    // Drop query since it has been retransmitted 3 times
                    node *temp2 = temp->next;
                    temp->next = temp->next->next;
                    free(temp2);
                }
                else{
                    // Retransmit query, increment tries
                    sendto(sockfd, temp->next->query, temp->next->len, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
                    temp->next->tries++;
                    temp=temp->next;
                }
            }
            pthread_mutex_unlock(&lock);
            timeout.tv_sec = 10;        // Reset timeout
            timeout.tv_usec = 0;
        }
        else{
            if(FD_ISSET(sockfd,&readfds)){
                // Some packet received
                char response[82];
                int recvBytes = recvfrom(sockfd, response, 82, 0, NULL, NULL);
                if (recvBytes < 0) {
                    perror("recvfrom");
                    exit(1);
                }
                
                // Parse Ethernet header, if not IP packet, continue
                struct ethhdr *ethResponse = (struct ethhdr *)response;
                if (ntohs(ethResponse->h_proto) != ETH_P_IP) {
                    continue;
                }

                // Parse IP header
                struct iphdr *ipResponse = (struct iphdr *)(response + sizeof(struct ethhdr));
                
                // check if protocol is 254
                if(ipResponse->protocol != 254){
                    continue;
                }

                // Parse simDNS response
                char *simDNSresponse = response + sizeof(struct ethhdr) + sizeof(struct iphdr);

                if(simDNSresponse[2] != 1){     // not a response
                    continue;
                }

                // checking if query ID is valid
                int queryID = (simDNSresponse[0] << 8) + simDNSresponse[1];
                pthread_mutex_lock(&lock);
                node *query=findNode(head, queryID);
                pthread_mutex_unlock(&lock);
                if(query == NULL){
                    continue;
                }

                // Output response as table
                printf("\nQuery ID: %d\n", queryID);

                int responsePointer = 0;
                int numResponses = simDNSresponse[3];
                
                printf("Total Query Strings: %d\n", numResponses);
                responsePointer += 4;
                for(int i = 0; i < numResponses; i++){
                    printf("%s\t\t", query->domains[i]);
                    if(simDNSresponse[responsePointer] == 0){
                        printf("NO IP ADDRESS FOUND\n");
                    }
                    else{
                        int ip= (simDNSresponse[responsePointer+1] << 24) | (simDNSresponse[responsePointer+2] << 16) | (simDNSresponse[responsePointer+3] << 8) | simDNSresponse[responsePointer+4];
                        struct in_addr ipAddr;
                        ipAddr.s_addr = ip;
                        printf("%s\n", inet_ntoa(ipAddr));
                    }
                    responsePointer += 5;
                }
                printf("\nWhat do you want to do?: ");
                fflush(stdout);
                pthread_mutex_lock(&lock);
                deleteNode(head, queryID);
                pthread_mutex_unlock(&lock);
                timeout.tv_sec = 10;        // Reset timeout
                timeout.tv_usec = 0;
            }
        }
    }
}

int main(int argc, char *argv[]){
    // Initialize mutex lock
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_trylock(&lock);
    pthread_mutex_unlock(&lock);
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <MAC address>\n", argv[0]);
        exit(1);
    }

    // Raw socket creation
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Bind raw socket to local IP Address
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex(interface_name);
    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("bind");
        exit(1);
    }

    unsigned char srcmac[6];
    for (int i = 0; i < ETH_ALEN; i++) {
        srcmac[i] = (unsigned char)strtol("00:00:00:00:00:00"+3*i, NULL, 16);
    }

    // get destination mac from argv[1]
    unsigned char mac[6];
    for (int i = 0; i < ETH_ALEN; i++) mac[i] = (unsigned char)strtol(argv[1] + 3*i, NULL, 16);

    uint16_t qid=1;
    node *head = (node *)malloc(sizeof(node));
    head->id = 0;
    head->next = NULL;

    // Destination address
    struct sockaddr_ll destaddr;
    destaddr.sll_family = AF_PACKET;
    destaddr.sll_protocol = htons(ETH_P_ALL);
    destaddr.sll_ifindex = if_nametoindex(interface_name);
    destaddr.sll_halen = 6;
    for(int i = 0; i < 6; i++){
        destaddr.sll_addr[i] = mac[i];          // Destination MAC address from argv[1]
    }

    // Create Receiver thread
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    argument *arg = (argument *)malloc(sizeof(argument));
    arg->sockfd = sockfd;
    arg->destaddr = destaddr;
    arg->head = head;

    if(pthread_create(&tid, &attr, R, (void *)arg) != 0){
        perror("pthread_create");
        exit(1);
    }

    while(1){
        printf("What do you want to do?: ");
        char cmd[MAXLEN];
        scanf("%s", cmd);
        if(strcmp(cmd, "EXIT") == 0){
            break;
        }
        else if(strcmp(cmd, "getIP")){
            printf("Enter valid query\n");
            continue;
        }

            int n;
            scanf("%d", &n);
            char domain[n][MAXLEN];
            for(int i = 0; i < n; i++){
                scanf("%s", domain[i]);
            }
            if(n>8){
                printf("Too many queries\n");
                continue;
            }
            int isQueryValid = 1;
            int whereWrong = -1;
            for(int i = 0; i < n; i++){
                if(!isDomainValid(domain[i])){
                    isQueryValid = 0;
                    whereWrong = i;
                    break;
                }
            }
            if(!isQueryValid){
                printf("Query invalid, format for domain %d is incorrect\n", whereWrong+1);
                continue;
            }
            int sumOfLengths = 0;
            for(int i = 0; i < n; i++){
                sumOfLengths += strlen(domain[i]);
            }

            // Construct query
            char query[4*sizeof(char) + sizeof(int)*n + sizeof(char)*sumOfLengths];    // 4 bytes for header, 4 bytes for each query string
            query[0] = (qid >> 8) & 0xFF;
            query[1] = qid & 0xFF;
            query[2] = 0x00;
            query[3] = n;
            int i=4,j=0;
            while(j < n){
                // Length of domain
                int len= strlen(domain[j]);
                query[i++] = (len >> 24) & 0xFF;
                query[i++] = (len >> 16) & 0xFF;
                query[i++] = (len >> 8) & 0xFF;
                query[i++] = len & 0xFF;
                // Domain
                for(int k = 0; k < len; k++){
                    query[i++] = domain[j][k];
                }
                j++;
            }

            // create packet to send as query
            int packlen = sizeof(struct ethhdr) + sizeof(struct iphdr) + 4*sizeof(char)+sizeof(int)*n+sizeof(char)*sumOfLengths;
            char packet[packlen];

            // Pointers to Ethernet and IP headers
            struct ethhdr *eth = (struct ethhdr *)packet;
            struct iphdr *ipheader = (struct iphdr *)(packet + sizeof(struct ethhdr));

            // copy simDNS query to packet
            memcpy(packet + sizeof(struct ethhdr) + sizeof(struct iphdr), query, 4+4*n+sumOfLengths);

            // Fill in IP header
            ipheader->ihl = 5;
            ipheader->version = 4;
            ipheader->tos = 0;
            ipheader->tot_len = htons(sizeof(struct iphdr)+4*sizeof(char)+sizeof(int)*n+sizeof(char)*sumOfLengths);
            ipheader->id = htons(ipID++);
            ipheader->ttl = 64;
            ipheader->protocol = 254;
            ipheader->saddr = inet_addr("127.0.0.1");       // Loopback address
            ipheader->daddr = inet_addr("127.0.0.1");       // Loopback address
            ipheader->frag_off = 0;

            // Fill in Ethernet header
            eth->h_proto = htons(ETH_P_IP);
            for(int i = 0; i < 6; i++){
                eth->h_dest[i] = mac[i];
                eth->h_source[i] = srcmac[i];
            }

            // Query bookkeeping
            node *queryNode = (node *)malloc(sizeof(node));
            queryNode->id = qid;
            queryNode->len = packlen;
            queryNode->n = n;
            queryNode->tries = 1;
            memcpy(queryNode->query, packet, packlen);
            for(int i = 0; i < n; i++){
                strcpy(queryNode->domains[i], domain[i]);
            }
            queryNode->next = NULL;
            pthread_mutex_lock(&lock);
            insertNode(head, queryNode);
            pthread_mutex_unlock(&lock);
            
            // Send query
            int sentBytes = sendto(sockfd, packet, packlen, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
            if (sentBytes < 0) {
                perror("sendto");
                exit(1);
            }
            qid++;
    }

    // Clean up
    pthread_mutex_lock(&lock);
    node *temp = head;
    while(temp->next != NULL){
        node *temp2 = temp->next;
        temp->next = temp->next->next;
        free(temp2);
    }
    free(head);
    pthread_mutex_unlock(&lock);
    close(sockfd);
    pthread_mutex_destroy(&lock);
}