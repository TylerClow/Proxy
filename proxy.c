#include <stdio.h>
#include <string.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 16777216 
#define MAX_OBJECT_SIZE 8388608 
#define MAXLINE 8192
char CACHE[MAX_OBJECT_SIZE] = "";

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct cacheNode{
        char* hostname;
        char* path;
        char *port;
        char *obj;
        int size;
        struct cacheNode *next;
        struct cacheNode *prev;
    }cacheNode;

    typedef struct cacheList{
        cacheNode *head;
        cacheNode *tail;
        int size;
    }cacheList;

void parse_uri(char *uri, char *filename, char *cgiargs, int *portnum);
void request(char *http_header,char *filename,char *cgiargs,int port,rio_t *rio);
void handler(int connfd, struct sockaddr_storage *clientaddr, cacheList *cacheList);
void removeTail(cacheList *listPtr);
void insertHead(cacheList *listPtr, char* hostname, char* port, char*path, int size);
cacheNode* search(cacheList* listPtr, char* hostname, char* path, char* port);


int main(int argc,char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    //CHANGED THIS:
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    cacheList *newCache = (struct cacheList*)malloc(MAX_CACHE_SIZE);

    if(argc != 2){
        fprintf(stderr, "usage:%s <port> \n", argv[0]);
        exit(1);
    }
    Signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(argv[1]);
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s %s)\n", client_hostname, client_port);
        handler(connfd, &clientaddr, newCache);
        Close(connfd);
    }
    return 0;
}

void handler(int connfd, struct sockaddr_storage *clientaddr, cacheList *cacheList)
{   
    char portnumber[100];
    int is_static;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiagrs[MAXLINE];
    rio_t rio, rio_2;
    char requestit [MAXLINE];
    int filedesc;
    char *cachebuf = malloc(MAX_OBJECT_SIZE);
    memset(cachebuf, '\0', sizeof(cachebuf) - 1);
    int filesize=0;

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request header:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(strcasecmp(method, "GET")){
        printf("%d %s 501 Method Not Impemented!\n", connfd, method);
        return;
    }
    parse_uri(uri, filename, cgiagrs, &is_static);
    sprintf(portnumber, "%d", is_static);
    printf("%s\n", portnumber);
    cacheNode *checker = search(cacheList, filename, cgiagrs, portnumber);
    printf("%s\n", portnumber);
    printf("%d %s", is_static, "FORPORT1\n");
    if(checker == NULL){
        printf("%s", "========CACHE MISS===========\n");
        //printf("%d %s", is_static, "FORPORT1\n");
        printf("%s %s", filename, "FILENAME\n");
        request(requestit, filename, cgiagrs, is_static, &rio);
        //printf("%d %s", is_static, "FORPORT2\n");
        sprintf(portnumber, "%d", is_static);
        printf("%s %s: %s %s: %s %s %s %s", "REQUEST TO OPEN CLIENT: ", "FILENAME", filename, "PORT", portnumber, "PATH:", cgiagrs, "\n");
        filedesc = Open_clientfd(filename,portnumber);

        Rio_readinitb(&rio_2,filedesc);
        Rio_writen(filedesc,requestit,strlen(requestit));
        size_t new;
        while((new=Rio_readnb(&rio_2,buf,1))> 0)
        {
            filesize = filesize + new;
            if(filesize<MAX_OBJECT_SIZE){
                memcpy(cachebuf, buf, 1);
                cachebuf += 1;
            }
            //Rio_writen(connfd,buf,size);
        }
        cachebuf = cachebuf - filesize;
        
        memcpy(CACHE, cachebuf, filesize);
        free(cachebuf);

        if(filesize < MAX_OBJECT_SIZE){
            while(((cacheList->size) + filesize) > MAX_CACHE_SIZE )
                removeTail(cacheList);
            insertHead(cacheList, filename, portnumber, cgiagrs, filesize);
            memcpy((cacheList->head)->obj, CACHE, filesize);
        }
        Rio_writen(connfd, cacheList->head->obj, filesize);
        Close(filedesc);
    }
       
    else{
        printf("%s", "========CACHE HIT===========\n");
        insertHead(cacheList, filename, portnumber, cgiagrs, checker->size);
        memcpy((cacheList->head)->obj, checker->obj, checker->size);
        Rio_writen(connfd, cacheList->head->obj, cacheList->head->size);
}
}
void request(char *http_header,char *filename,char *cgiargs,int port,rio_t *rio)
{
    char initial[MAXLINE];
    char request_header[MAXLINE];
    char final_header[MAXLINE];
    char host_header[MAXLINE];
    printf("%s %s", filename, "FILENAME AT REQUEST LEVEL\n");
    memset(request_header, 0, strlen(request_header));
    sprintf(request_header,"GET %s HTTP/1.0\r\n",cgiargs);
    
    while(Rio_readlineb(rio,initial,MAXLINE)>0)
    {
        if(!strcmp(initial,"\r\n")) 
            break;
        printf("%s %s", initial, "INITIAL AT REQUEST LEVEL\n");
        if(!strncasecmp(initial,"Host",strlen("Host")))
        {
            strcpy(host_header,initial);
            continue;
        }

        if(!strncasecmp(initial,"Connection",strlen("Connection"))){
            if(!strncasecmp(initial,"Proxy-Connection",strlen("Proxy-Connection"))){
                if(!strncasecmp(initial,"User-Agent",strlen("User-Agent"))){
                    strcat(final_header,initial);
                }
            }
        }
    }
    if(!strlen(host_header))
    {
        strcat(host_header,"Host: ");
        strcat(host_header, filename);
        strcat(host_header, "\r\n");
    }

    memset(http_header, 0, strlen(http_header));
    
    strcat(http_header,request_header);
    strcat(http_header,host_header);
    strcat(http_header,"Connection: close\r\n");
    strcat(http_header,"Proxy-Connection: close\r\n");
    strcat(http_header,user_agent_hdr);
    //strcat(http_header,final_header);
    strcat(http_header,"\r\n");

    // printf("%s\n %s %s","START_TEST_HEADER", http_header, "TEST_HEADER\n");
    printf("%s", http_header);

    return ;
}


void parse_uri(char *uri, char *filename, char *cgiargs, int *portnum)
{
 
    *portnum = 80;
    char *filestart;
    char *cgiargs_start;
    char *port;

    char* copy = malloc(strlen(uri) + 1); 
    strcpy(copy, uri);
    
    filestart = strtok(&uri[7], "/");
    strcpy(filename, filestart);

    if(!(cgiargs_start = strchr(&copy[7], '/'))){
        strcpy(cgiargs, "/");
    }
    else
        strcpy(cgiargs, cgiargs_start);

    
    if(strstr(filestart, ":") == NULL){
        strcpy(filename, filestart);
        free(copy);
    }
    else{
        char* copy2 = malloc(strlen(filestart) + 1); 
        strcpy(copy2, filestart);
        port = strchr(copy2, ':');
        port++;
        sscanf(port, "%d", portnum);
        filestart = strtok(filestart, ":");
        strcpy(filename, filestart);
        free(copy);
        free(copy2);
    }
    //printf("%s %s\n", "FILENAME--->", filename);
    //printf("%s %s\n", "CGIARGS--->", cgiargs);
    //printf("%s %d\n", "PORTNUM--->", portnum[0]);
    return;
}  
    void removeTail(cacheList *listPtr) {

        if(listPtr -> head == NULL)
            return;
        int clear;
        clear = listPtr->tail->size;
        struct cacheNode* lastNode = listPtr->tail;//temp node is assigned to the node to be deleted
        if(listPtr->head->next ==NULL){//if only one node in list, list becomes empty list
            listPtr->head = NULL;
            listPtr->tail = NULL;
            free(lastNode-> hostname);
            free(lastNode -> port);
            free(lastNode -> path);
            free(lastNode -> obj);
            free(lastNode);
            listPtr-> size = 0;
        }
        listPtr->tail = listPtr->tail->prev;//else, new tail becomes the node before the current tail
        listPtr->tail->next = NULL;//next node from new tail is null
        listPtr->head = NULL;
        listPtr->tail = NULL;
        free(lastNode-> hostname);
        free(lastNode -> port);
        free(lastNode -> path);
        free(lastNode -> obj);
        free(lastNode);
        listPtr-> size = (listPtr->size) - clear;//free memory
        //return value of the removed node
        return;
    } 
    void insertHead(cacheList *listPtr, char* hostname, char* port, char*path, int size) {
        cacheNode* myNode =(struct cacheNode *)malloc(sizeof( cacheNode)); //creating new node
        myNode->prev = NULL;
        myNode->next = NULL; //Initialized new node with values, next and prev nodes
        myNode -> hostname = malloc(strlen(hostname) + 1);
        strcpy(myNode->hostname, hostname);
        myNode -> port = malloc(strlen(port) + 1);
        strcpy(myNode->port, port);
        myNode -> path = malloc(strlen(path) + 1);
        strcpy(myNode->path, path);
        myNode->obj = malloc(size + 1);
        myNode -> size = size;
        listPtr->size += size;
        if(listPtr->head == NULL){//If empty list
            listPtr->head = myNode;
            listPtr->tail = myNode;
            return;
        }
        else{//If not empty list
            listPtr->head->prev = myNode;
            myNode->next = listPtr->head;
            listPtr->head = myNode;
            return;
        }
    }
    cacheNode* search(cacheList* listPtr, char* hostname, char* path, char* port){
        if(listPtr->head == NULL)
            return NULL;
        cacheNode* guy = listPtr -> head;
        while(guy!= NULL){
            if(!(strcasecmp(guy->hostname, hostname)) && !(strcasecmp(guy->path, path)) 
            && !(strcasecmp(guy->port, port))){
                if(guy == listPtr->head)
                    return guy;
                else if(guy == listPtr->tail){
                    insertHead(listPtr, hostname, path, port, (guy->size));
                    return guy;
                    removeTail(listPtr);
                }
                else{
                    insertHead(listPtr, hostname, path, port, (guy->size));
                    return guy;
                    (guy->prev)->next = guy ->next;
                    (guy->next)->prev = guy->prev;
                    listPtr->size = (listPtr->size) - (listPtr->head->size);
                    free(guy->hostname);
                    free(guy->port);
                    free(guy->path);
                    free(guy->obj);
                    free(guy);
                }
            }
            guy = guy->next;
        }
        return NULL;
    }