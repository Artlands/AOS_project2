#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>                              //includes a number of definitions of structures needed for sockets
#include <netinet/in.h>                              //contains constants and structures needed for internet domain addresses
#include <arpa/inet.h>                               //inet_ntop - convert IPv4 and IPv6 addresses from binary to text form

#define MAXBUFSIZE 2048
#define MAXFILES 1024
#define MAXCLIENTS 32

struct client {
  char name[MAXBUFSIZE];
  char ip[MAXBUFSIZE];
  int port;
}

struct file {
  char name[MAXBUFSIZE];
  int size;
  struct client location;
}

struct file fileList[MAXFILES]
struct client clientList[MAXCLIENTS]
int cli_socketfds[MAXCLIENTS]

void *req_handler(void *sock);                       //handle requests
void addFile(char *name, char *source, char *ip, char *port, int *fileList);
void error(const char *msg)                          // Called when a system call fails
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{

    int sockfd, newsockfd, portno;                   //sockfd, newsockfd are file descriptors, store values returned by the socket system call and the accept system call.
    socklen_t clilen;                                //clilen stores the size of the address of the client.
    struct sockaddr_in serv_addr, cli_addr;          //structure containing internet addresses

    char hostbuffer[256];                            //get hostname and ip address
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    host_entry = gethostbyname(hostbuffer);
    IPbuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));

    if (argc < 2) {
       fprintf(stderr,"ERROR, no port provided\n");
       exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);        //create a new socket, returns an entry into file descriptor table
    if (sockfd < 0)
      error("ERROR opening socket");
    int enable = 1;                                  //set SO_REUSEADDR so that socket immediate unbinds from port after closed
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
      error("setsockopt(SO_REUSEADDR) failed");

    bzero((char *) &serv_addr, sizeof(serv_addr));   //sets all values in a buffer to 0
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;          //the IP address of the machine on which the server is running, INADDR_ANY gets this address
    serv_addr.sin_port = htons(portno);              //htons converts a port number in host byte order to a port number in network byte order
    if (bind(sockfd, (struct sockaddr *) &serv_addr, //binds a socket to an address
            sizeof(serv_addr)) < 0)
            error("ERROR on binding");

    listen(sockfd,5);
    printf("Starting Central Indexing Server...\n");
    printf("Hostname: %s\n", hostbuffer);
    printf("IP address: %s\n", IPbuffer);
    printf("Start listening on port %u...\n", portno);

    while(1)
    {
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd,                     //accept() system call causes the process to block until a client to the server
                   (struct sockaddr *) &cli_addr,
                   &clilen);
      if(newsockfd < 0) continue;

      char cli_addr_str[clilen];
      u_short cli_port;
      inet_ntop(AF_INET, &(cli_addr.sin_addr.s_addr), cli_addr_str, clilen);
      cli_port = cli_addr.sin_port;                  //get client ip address and port
      printf("Connected with %s, the client port is %u\n", cli_addr_str, cli_port);

      pthread_t threadid;
      int status;
      int * sock = calloc(1, sizeof(int));
      *sock = newsockfd;
      status = pthread_create(&threadid, NULL, req_handler, (void *) sock);
      if(status !=0) {
        printf("ERROR on thread creation\n");
        exit(-1);
      }
      pthread_join(threadid, NULL);
    }
    close(sockfd);
    return 0;
  }

  void *req_handler(void *sock)
  {
      int newsockfd = *(int*) sock;
      int n;
      char buffer[1];                                // reads characters from the socket connection into this buffer

      char name[MAXBUFSIZE], size[MAXBUFSIZE], source[MAXBUFSIZE];
      char ip[MAXBUFSIZE], port[MAXBUFSIZE];
      char buffer[MAXBUFSIZE];

      bzero(buffer, sizeof(buffer));
      bzero(name, sizeof(name));
      bzero(ip, sizeof(ip));
      bzero(prot, sizeof(port));

      n = read(newsockfd, buffer, sizeof(buffer));
      if (n < 0)
          error("ERROR reading from socket");

      if(strcmp(buffer, 'r') == 0){
        addFile
      }

      // printf("Here is the message: %s\n", buffer);
      // n = write(newsockfd, "I got your message", 18);
      // if (n < 0)
      //     error("ERROR writing to socket");
      pthread_exit(NULL);
  }

  void addFile(char *name, char *source, char *ip, char *port, int *fileList)
  {
    strcpy(fileList[*fileList].name, name);
    fileList[*fileList].size = atoi(size);
    strcpy(fileList[*fileList].location.name, source);
    strcpy(fileList[*fileList].location.ip, ip);
    fileList[*fileList].location.port = atoi(port);
    (*fileList)++;
  }
