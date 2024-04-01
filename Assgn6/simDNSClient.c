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
#include <ctype.h>

#define MAXLEN 32

typedef struct node{
    int id;
    struct node *next;
}node;

void insert(node *head, int id){
    node *temp = head;
    while(temp->next != NULL){
        temp = temp->next;
    }
    temp->next = (node *)malloc(sizeof(node));
    temp->next->id = id;
    temp->next->next = NULL;
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

int main(){
    int sockfd;
    uint16_t qid=1;
    node *head = (node *)malloc(sizeof(node));
    head->id = 0;
    head->next = NULL;
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // connect call????

    while(1){
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
        char query[3+4*n+sumOfLengths];
        query[0] = (qid >> 8) & 0xFF;
        query[1] = qid & 0xFF;
        query[2] = 0;
        query[2] |= (n << 4);       // last 4 bits are unused
        int i=3,j=0;
        while(j < n){
            query[i++] = strlen(domain[j]);
            for(int k = 0; k < strlen(domain[j]); k++){
                query[i++] = domain[j][k];
            }
            j++;
        }
        sendto(sockfd, query, 3+4*n+sumOfLengths, 0, NULL, 0);      //what call to use???? 
        insert(head, qid++);

    }
}