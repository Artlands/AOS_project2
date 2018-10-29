#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXBUFSIZE 2048
#define MAXFILES 1024
#define MAXCLIENTS 32

struct client {
  char name[MAXBUFSIZE];
  char ip[MAXBUFSIZE];
  int port;
};

struct file {
  char name[MAXBUFSIZE];
  int size;
  struct client location;
};

struct file fileList[MAXFILES];
struct client clientList[MAXCLIENTS];
int cli_socketfds[MAXCLIENTS];

// void addFile(char *name, char *source, char *ip, char *port, int *fileList);
// Called when a system call fails
void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    pid_t pid;
    int sockfd, newsockfd, portno;  //sockfd, newsockfd are file descriptors, store values returned by the socket system call and the accept system call.
    socklen_t clilen; //clilen stores the size of the address of the client.
    struct sockaddr_in serv_addr, cli_addr; //structure containing internet addresses
    clilen = sizeof(cli_addr);

    /*get hostname and ip address*/
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    gethostname(hostbuffer, sizeof(hostbuffer));
    host_entry = gethostbyname(hostbuffer);
    IPbuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));

    if (argc < 2) {
      error("ERROR, no port provided\n");
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0); //create a new socket, returns an entry into file descriptor table
    if (sockfd < 0)
      error("ERROR opening socket");

    int enable = 1; //set SO_REUSEADDR so that socket immediate unbinds from port after closed
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
      error("setsockopt(SO_REUSEADDR) failed");

    bzero((char *) &serv_addr, sizeof(serv_addr));  //sets all values in a buffer to 0
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; //the IP address of the machine on which the server is running, INADDR_ANY gets this address
    serv_addr.sin_port = htons(portno); //htons converts a port number in host byte order to a port number in network byte order

    if (bind(sockfd, (struct sockaddr *) &serv_addr,  //binds a socket to an address
            sizeof(serv_addr)) < 0)
            error("ERROR on binding");

    listen(sockfd,5);
    printf("Starting Central Indexing Server...\n");
    printf("Hostname: %s\n", hostbuffer);
    printf("IP address: %s\n", IPbuffer);
    printf("Start listening on port %u...\n", portno);

    char buffer[MAXBUFSIZE];  //recevier buffer
    while(1) {
      newsockfd = accept(sockfd,
                   (struct sockaddr *) &cli_addr,
                   &clilen);
      if(newsockfd < 0) continue;
      /*create seperate process for each client*/
      pid = fork();
      if(pid != 0){ //child process
        close(sockfd); // close the socket for other connections

        /*get client ip address and port*/
        char cli_addr_str[clilen];
        u_short cli_port;
        inet_ntop(AF_INET, &(cli_addr.sin_addr.s_addr), cli_addr_str, clilen);
        cli_port = cli_addr.sin_port;
        printf("Client connected from port no. %u and IP %s\n", cli_port, cli_addr_str);
        //add client ip address to IP list

        /*keep receiving message from client*/
        while(1) {
          bzero(buffer, sizeof(buffer));
          if(recv(newsockfd, buffer, sizeof(buffer), 0) > 0) {
            if(strcmp(buffer, "r") == 0) {
              /*Register peer with server*/
            }
            else if(strcmp(buffer, "l") == 0) {
              /*List files registered with server*/
            }
            else if (strcmp(buffer, "s") == 0) {
              /*Search files sent from client*/
            }
            else if (strcmp(buffer, "q")) {
              printf("Client disconnected from port no. %u and IP %s\n", cli_port, cli_addr_str );
              /*deregister with server*/
              close(newsockfd);
              exit(0);
            }
          }
        } //close inner while loop
      }
      close(newsockfd);
    } //close main while loop
    printf("Server shutting down...\n");
    return 0;
  }

  // void *req_handler(void *sock)
  // {
  //     int newsockfd = *(int*) sock;
  //     int n;
  //     // reads characters from the socket connection into this buffer
  //     char buffer[1];
  //
  //     char name[MAXBUFSIZE], size[MAXBUFSIZE], source[MAXBUFSIZE];
  //     char ip[MAXBUFSIZE], port[MAXBUFSIZE];
  //     char buffer[MAXBUFSIZE];
  //
  //     bzero(buffer, sizeof(buffer));
  //     bzero(name, sizeof(name));
  //     bzero(ip, sizeof(ip));
  //     bzero(prot, sizeof(port));
  //
  //     n = read(newsockfd, buffer, sizeof(buffer));
  //     if (n < 0)
  //         error("ERROR reading from socket");
  // }

  // void addFile(char *name, char *source, char *ip, char *port, int *fileList)
  // {
  //   strcpy(fileList[*fileList].name, name);
  //   fileList[*fileList].size = atoi(size);
  //   strcpy(fileList[*fileList].location.name, source);
  //   strcpy(fileList[*fileList].location.ip, ip);
  //   fileList[*fileList].location.port = atoi(port);
  //   (*fileList)++;
  // }
