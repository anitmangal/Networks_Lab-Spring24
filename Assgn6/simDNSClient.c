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


#define MAXLEN 32
#define interface_name "eno1"
int ipID = 0;

unsigned short checksum(unsigned short* buff, int _16bitword) {
    unsigned long sum;
    for(sum=0;_16bitword>0;_16bitword--)
    sum+=htons(*(buff)++);
    sum = ((sum >> 16) + (sum & 0xFFFF));
    sum += (sum>>16);
    return (unsigned short)(~sum);
}

typedef struct node{
    int id;
    int n;
    char query[4+4*(8+MAXLEN)];
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

int isDomainValid(char *domain){
    int len = strlen(domain);
    if(len < 3 || len > 31){
        return 0;
    }
    int i = 0;
    while(i < len){
        if(domain[i] == '.'){
            // if(i == 0 || i == len-1 || domain[i-1] == '.'){
            //     return 0;
            // }
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

int main(int argc, char *argv[]){
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
    printf("Socket created\n");

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy((char*)ifr.ifr_name, interface_name);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl1");
        exit(1);
    }
    printf("Interface index: %d\n", ifr.ifr_ifindex);

    // Bind raw socket to local IP Address
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = ifr.ifr_ifindex;
    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("bind");
        exit(1);
    }
    printf("Socket bound\n");

    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl2");
        exit(1);
    }

    unsigned char srcmac[6];
    memcpy(srcmac, (unsigned char*)ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    for (int i = 0; i < ETH_ALEN; i++) {
        printf("%02x:", srcmac[i]);
    }
    printf("\n");
    // get mac from argv[1]
    unsigned char mac[6];
    sscanf(argv[1], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    printf("mac: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    uint16_t qid=1;
    node *head = (node *)malloc(sizeof(node));
    head->id = 0;
    head->next = NULL;
    
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    fd_set readfds;

    // mac address of server
    struct sockaddr_ll destaddr;
    destaddr.sll_family = AF_PACKET;
    destaddr.sll_protocol = htons(ETH_P_ALL);
    destaddr.sll_ifindex = ifr.ifr_ifindex;
    destaddr.sll_halen = 6;
    for(int i = 0; i < 6; i++){
        destaddr.sll_addr[i] = mac[i];
    }


    while(1){
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        printf("What do you wanna do?: ");
        char cmd[MAXLEN];
        scanf("%s", cmd);
        if(strcmp(cmd, "exit") == 0){
            break;
        }
        else if(strcmp(cmd, "getIP")){
            printf("Enter valid query\n");
            continue;
        }

            int n;
            scanf("%d", &n);
            printf("Enter %d queries\n", n);
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
            char query[4*sizeof(char) + sizeof(int)*n + sizeof(char)*sumOfLengths];    // 4 bytes for header, 4 bytes for each query string
            query[0] = (qid >> 8) & 0xFF;
            query[1] = qid & 0xFF;
            query[2] = 0;
            query[3] = n;
            int i=4,j=0;
            while(j < n){
                int len= strlen(domain[j]);
                query[i++] = (len >> 24) & 0xFF;
                query[i++] = (len >> 16) & 0xFF;
                query[i++] = (len >> 8) & 0xFF;
                query[i++] = len & 0xFF;
                for(int k = 0; k < len; k++){
                    query[i++] = domain[j][k];
                }
                j++;
            }
            for (int i = 0; i < 4+4*n+sumOfLengths; i++) printf("%c ", query[i]);
            printf("\n");

            // create packet to send as query
            int packlen = sizeof(struct ethhdr) + sizeof(struct iphdr) + 4*sizeof(char)+sizeof(int)*n+sizeof(char)*sumOfLengths;
            char packet[packlen];     // 38 bytes for Ethernet and IP headers, 4+4*n+sumOfLengths bytes for query

            // Pointers to Ethernet and IP headers
            struct ethhdr *eth = (struct ethhdr *)packet;
            struct iphdr *ipheader = (struct iphdr *)(packet + sizeof(struct ethhdr));

            // copy simDNS query to packet
            memcpy(packet + sizeof(struct ethhdr) + sizeof(struct iphdr), query, 4+4*n+sumOfLengths);

            // Fill in IP header
            ipheader->ihl = 5;
            ipheader->version = 4;
            ipheader->tos = 0;
            ipheader->tot_len = htons(20+4+4*n+sumOfLengths);    // Header: 20 bytes, Data: 4+4*n+sumOfLengths bytes
            ipheader->id = htons(ipID++);
            ipheader->ttl = 64;
            ipheader->protocol = 254;
            ipheader->check = checksum((unsigned short *)ipheader, sizeof(struct iphdr)/2);
            ipheader->saddr = inet_addr("127.0.0.1");
            ipheader->daddr = inet_addr("127.0.0.1");       //????

            // Fill in Ethernet header
            eth->h_proto = htons(ETH_P_IP);
            for(int i = 0; i < 6; i++){
                eth->h_dest[i] = mac[i];
                eth->h_source[i] = srcmac[i];
            }

            node *queryNode = (node *)malloc(sizeof(node));
            queryNode->id = qid;
            queryNode->len = 4+4*n+sumOfLengths;
            queryNode->n = n;
            queryNode->tries = 1;
            memcpy(queryNode->query, query, 4+4*n+sumOfLengths);
            for(int i = 0; i < n; i++){
                strcpy(queryNode->domains[i], domain[i]);
            }
            queryNode->next = NULL;
            insertNode(head, queryNode);
            
            for (int i = 0; i < packlen; i++) printf("%02x ", packet[i]);
            printf("\n");
            int sentBytes = sendto(sockfd, packet, packlen, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
            if (sentBytes < 0) {
                perror("sendto");
                exit(1);
            }
            qid++;



        // int retval = select(sockfd+1, &readfds, NULL, NULL, &timeout);
        // if(retval == -1){
        //     perror("select");
        //     exit(1);
        // }
        // else if(retval == 0){
        //     node *temp = head;
        //     while(temp->next != NULL){
        //         if(temp->next->tries == 4){
        //             printf("Query ID: %d, no response\n", temp->next->id);
        //         }
        //         else{
        //             printf("Query ID: %d, retransmitting query\n", temp->next->id);
        //             sendto(sockfd, temp->next->query, temp->next->len, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
        //             temp->next->tries++;
        //         }
        //     }
        //     continue;
        // }
        // if(FD_ISSET(STDIN_FILENO, &readfds)){
        //     int n;
        //     scanf("%d", &n);
        //     printf("Enter %d queries\n", n);
        //     char domain[n][MAXLEN];
        //     for(int i = 0; i < n; i++){
        //         scanf("%s", domain[i]);
        //     }
        //     if(n>8){
        //         printf("Too many queries\n");
        //         continue;
        //     }
        //     int isQueryValid = 1;
        //     int whereWrong = -1;
        //     for(int i = 0; i < n; i++){
        //         if(!isDomainValid(domain[i])){
        //             isQueryValid = 0;
        //             whereWrong = i;
        //             break;
        //         }
        //     }
        //     if(!isQueryValid){
        //         printf("Query invalid, format for domain %d is incorrect\n", whereWrong+1);
        //         continue;
        //     }
        //     int sumOfLengths = 0;
        //     for(int i = 0; i < n; i++){
        //         sumOfLengths += strlen(domain[i]);
        //     }
        //     char query[4*sizeof(char) + sizeof(int)*n + sizeof(char)*sumOfLengths];    // 4 bytes for header, 4 bytes for each query string
        //     query[0] = (qid >> 8) & 0xFF;
        //     query[1] = qid & 0xFF;
        //     query[2] = 0;
        //     query[3] = n;
        //     int i=4,j=0;
        //     while(j < n){
        //         int len= strlen(domain[j]);
        //         query[i++] = (len >> 24) & 0xFF;
        //         query[i++] = (len >> 16) & 0xFF;
        //         query[i++] = (len >> 8) & 0xFF;
        //         query[i++] = len & 0xFF;
        //         for(int k = 0; k < len; k++){
        //             query[i++] = domain[j][k];
        //         }
        //         j++;
        //     }
        //     for (int i = 0; i < 4+4*n+sumOfLengths; i++) printf("%c ", query[i]);
        //     printf("\n");

        //     // create packet to send as query
        //     int packlen = sizeof(struct ethhdr) + sizeof(struct iphdr) + 4*sizeof(char)+sizeof(int)*n+sizeof(char)*sumOfLengths;
        //     char packet[packlen];     // 38 bytes for Ethernet and IP headers, 4+4*n+sumOfLengths bytes for query

        //     // Pointers to Ethernet and IP headers
        //     struct ethhdr *eth = (struct ethhdr *)packet;
        //     struct iphdr *ipheader = (struct iphdr *)(packet + sizeof(struct ethhdr));

        //     // copy simDNS query to packet
        //     memcpy(packet + sizeof(struct ethhdr) + sizeof(struct iphdr), query, 4+4*n+sumOfLengths);

        //     // Fill in IP header
        //     ipheader->ihl = 5;
        //     ipheader->version = 4;
        //     ipheader->tos = 0;
        //     ipheader->tot_len = htons(20+4+4*n+sumOfLengths);    // Header: 20 bytes, Data: 4+4*n+sumOfLengths bytes
        //     ipheader->id = htons(ipID++);
        //     ipheader->ttl = 64;
        //     ipheader->protocol = 254;
        //     ipheader->check = checksum((unsigned short *)ipheader, sizeof(struct iphdr)/2);
        //     ipheader->saddr = inet_addr("127.0.0.1");
        //     ipheader->daddr = inet_addr("127.0.0.1");       //????

        //     // Fill in Ethernet header
        //     eth->h_proto = htons(ETH_P_IP);
        //     for(int i = 0; i < 6; i++){
        //         eth->h_dest[i] = mac[i];
        //         eth->h_source[i] = srcmac[i];
        //     }

        //     node *queryNode = (node *)malloc(sizeof(node));
        //     queryNode->id = qid;
        //     queryNode->len = 4+4*n+sumOfLengths;
        //     queryNode->n = n;
        //     queryNode->tries = 1;
        //     memcpy(queryNode->query, query, 4+4*n+sumOfLengths);
        //     for(int i = 0; i < n; i++){
        //         strcpy(queryNode->domains[i], domain[i]);
        //     }
        //     queryNode->next = NULL;
        //     insertNode(head, queryNode);
            
        //     for (int i = 0; i < packlen; i++) printf("%02x ", packet[i]);
        //     printf("\n");
        //     int sentBytes = sendto(sockfd, packet, packlen, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
        //     if (sentBytes < 0) {
        //         perror("sendto");
        //         exit(1);
        //     }
        //     qid++;
        // }
        // if(FD_ISSET(sockfd,&readfds)){
        //     char response[82];
        //     int recvBytes = recvfrom(sockfd, response, 82, 0, NULL, NULL);
        //     if (recvBytes < 0) {
        //         perror("recvfrom");
        //         exit(1);
        //     }
            
        //     // Parse Ethernet header
        //     struct ethhdr *ethResponse = (struct ethhdr *)response;
        //     if (ntohs(ethResponse->h_proto) != ETH_P_IP) {
        //         printf("Not an IP packet\n");
        //         continue;
        //     }

        //     // Parse IP header
        //     struct iphdr *ipResponse = (struct iphdr *)(response + sizeof(struct ethhdr));
            
        //     // check if protocol is 254
        //     if(ipResponse->protocol != 254){
        //         printf("Not a simDNS packet\n");
        //         continue;
        //     }

        //     // Parse simDNS response
        //     char *simDNSresponse = response + sizeof(struct ethhdr) + sizeof(struct iphdr);

        //     if(simDNSresponse[2] != 1){     // not a response
        //         continue;
        //     }

        //     // checking if query ID is valid
        //     int queryID = (simDNSresponse[0] << 8) + simDNSresponse[1];
        //     node *query=findNode(head, queryID);
        //     if(query == NULL){
        //         printf("Query ID: %d, no such query\n", queryID);
        //         continue;
        //     }

        //     printf("Query ID: %d\n", queryID);

        //     int responsePointer = 0;
        //     int numResponses = simDNSresponse[3];
            
        //     printf("Total Query Strings: %d\n", numResponses);
        //     responsePointer += 4;
        //     for(int i = 0; i < numResponses; i++){
        //         printf("%s ", query->domains[i]);
        //         if(simDNSresponse[responsePointer] == 0){
        //             printf("NO IP ADDRESS FOUND\n");
        //         }
        //         else{
        //             printf("%d.%d.%d.%d\n", simDNSresponse[responsePointer+1], simDNSresponse[responsePointer+2], simDNSresponse[responsePointer+3], simDNSresponse[responsePointer+4]);
        //         }
        //         responsePointer += 5;
        //     }
        //     deleteNode(head, queryID);
        // }
    }
}