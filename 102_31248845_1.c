#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>

typedef struct {
    int returncode;
    char *filename;
} httpRequest;


char *header200 = "HTTP/1.0 200 OK\nServer: CS288Serv v0.1\nContent-Type: text/html\n\n";
char *header400 = "HTTP/1.0 400 Bad Request\nServer: CS288Serv v0.1\nContent-Type: text/html\n\n";
char *header404 = "HTTP/1.0 404 Not Found\nServer: CS288Serv v0.1\nContent-Type: text/html\n\n";
char *header501 = "HTTP/1.0 501 Not Implemented\nServer: CS288Serv v0.1\nContent-Type: text/html\n\n";

ssize_t sendMessage(int fd, char *message) {
    return write(fd, message, strlen(message));
}

void printFile(int fd, char *fname) {
    
    size_t size = 1;
    char *aloc;
    FILE *read;
    
    printf("Filename: %s\n", fname);
    
    if( (read = fopen(fname, "r")) == NULL)
    {
        fprintf(stderr, "Error opening file in printFile()\n");
        exit(1);
    }
    
    aloc = malloc(sizeof(char) * size);
    if(  aloc == NULL )
    {
        fprintf(stderr, "Memory allocation error (aloc) in printFile()\n");
        exit(1);
    }
    
    ssize_t line;
    while( (line = getline( &aloc, &size, read)) > 0)
    {
        sendMessage(fd, aloc);
    }
    
    sendMessage(fd, "\n");
    free(aloc);

}

char *get_data_msg(int fd) {
    size_t size = 1;
    ssize_t line;
    FILE *f_stream;
    char *aloc, *s_block;
    int prev_size = 1;
    
    if( (f_stream = fdopen(fd, "r")) == NULL)
    {
        fprintf(stderr, "Error opening file descriptor in getMessage()\n");
        exit(EXIT_FAILURE);
    }
    
    if( (s_block = malloc(sizeof(char) * size)) == NULL )
    {
        fprintf(stderr, "Memory allocation error () to block in getMessage\n");
        exit(1);
    }
    *s_block = '\0';
    
    if( (aloc = malloc(sizeof(char) * size)) == NULL )
    {
        fprintf(stderr, "Error allocating memory to tmp in getMessage\n");
        exit(1);
    }
    *aloc = '\0';
    
    while( (line = getline( &aloc, &size, f_stream)) > 0)
    {
        if( strcmp(aloc, "\r\n") == 0) break;
        
        s_block = realloc(s_block, size+prev_size);
        prev_size += size;
        strcat(s_block, aloc);
    }
    
    printf("%s\n", s_block);
    
    free(aloc);
    
    return s_block;
    
}

char * getFileName(char* msg)
{

    char *file_requested, *web_dir, *base;
    
    if( (file_requested = malloc(sizeof(char) * strlen(msg))) == NULL)
    {
        fprintf(stderr, "Error allocating memory to file in getFileName()\n");
        exit(EXIT_FAILURE);
    }
    
    sscanf(msg, "GET %s HTTP/1.1", file_requested);
    
    if( (base = malloc(sizeof(char) * (strlen(file_requested) + 18))) == NULL)
    {
        fprintf(stderr, "Error allocating memory to base in getFileName()\n");
        exit(EXIT_FAILURE);
    }
    
    web_dir = "web";
    strcpy(base, web_dir);
    strcat(base, file_requested);
    
    free(file_requested);
    
    return base;
}

httpRequest parseRequest(char *msg){
    char* fname;
    int try_file;
    httpRequest request;
    FILE *file_exist;
    
    if( (fname = malloc(sizeof(char) * strlen(msg))) == NULL)
    {
        fprintf(stderr, "Error allocating memory to filename in parseRequest()\n");
        exit(EXIT_FAILURE);
    }
    
    fname = getFileName(msg);
    try_file = strcmp(fname, "web/");
    file_exist = fopen(fname, "r" );
    
    if(try_file == 0)
    {
        request.returncode = 200;
        request.filename = "web/index.html";
    }
    else if( file_exist != NULL )
    {
        request.returncode = 200;
        request.filename = fname;
        fclose(file_exist);
    }
    else
    {
        request.returncode = 404;
        request.filename = "web/404.html";
    }
    
    return request;
}

void* connection_handler(void *socket_desc)
{
    int sock = *(int*)socket_desc;
    
    char * http_header = get_data_msg(sock);
    httpRequest req_info = parseRequest(http_header);
    
    switch (req_info.returncode)
    {
        case 200:
            sendMessage(sock, header200);
            printf("Everything is good: 200\n");
            break;
            
        case 400:
            sendMessage(sock, header400);
            break;
            
        case 404:
            sendMessage(sock, header404);
            printf("Page does not exist: 404\n");
            break;
            
        case 501:
            sendMessage(sock, header501);
            break;
    }
    
    printFile(sock, req_info.filename);
    free(http_header);
    close(sock);
    
    return 0;
}

int main(int argc, char *argv[]) {
    
    if(argc == 2)
    {
        /* Variable */
        int port_num = atoi(argv[1]);
        
        struct sockaddr_in server_addr, client_addr;
        socklen_t addr_len;
        int fd_server, fd_client;
        int option = 1;
        
        /* Creating Socket */
        fd_server = socket(AF_INET, SOCK_STREAM, 0);
        
        if(fd_server < 0)
        {
            perror("Socket Failed");
            exit(1);
        }
        
        memset(&server_addr, 0, sizeof(server_addr));
        setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(port_num);
        
        /* Bind */
        if(bind(fd_server, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        {
            perror("Bind failed");
            close(fd_server);
            exit(1);
        }
        
        /* Listen */
        if(listen(fd_server, 10) == -1)
        {
            perror("Listen");
            close(fd_server);
            exit(1);
        }
        
        /* Accept */
        addr_len = sizeof(client_addr);
        
        puts("Waiting for incoming connections...");
        pthread_t thread_id;
        
        while( (fd_client = accept(fd_server, (struct sockaddr *) &client_addr, &addr_len)) )
        {
            
            if( pthread_create( &thread_id , NULL ,  connection_handler , (void*) &fd_client) < 0)
            {
                perror("could not create thread");
                return 1;
            }
            pthread_join( thread_id , NULL);
        }
        
        if (fd_client < 0)
        {
            perror("accept failed");
            return 1;
        }

    }
    else
    {
        puts("There must be only one argument and a port number must be specified.");
    }
    
    
    return EXIT_SUCCESS;
}
