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

// void addFile(char *name, char *source, char *ip, char *port, int *fileList);
// Called when a system call fails
void error(const char *msg) {
  perror(msg);
  exit(1);
}

struct client getClientInfo(int socket);
int addClient(char name[MAXBUFSIZE], char ip[MAXBUFSIZE], int port, int *sizeClients);
int registerFiles(int socket, int *sizeClients, int *sizeFileList);
void addFile(char *name, char *size, char *source, char *ip, char *port, int *sizeFileList);
void sendFileList(int socket, int *sizeFileList);
void removeClient(char name[MAXBUFSIZE], int *sizeClients, int *sizeFileList);
void removeFile(int index, int *sizeFileList);

int main(int argc, char *argv[]) {
  int sizeFileList = 0;
  int sizeClients = 0;
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
    error("ERROR, no port provided.\n");
  }
  sockfd = socket(AF_INET, SOCK_STREAM, 0); //create a new socket, returns an entry into file descriptor table
  if (sockfd < 0)
    error("ERROR opening socket.");

  int enable = 1; //set SO_REUSEADDR so that socket immediate unbinds from port after closed
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    error("setsockopt(SO_REUSEADDR) failed.");

  bzero((char *) &serv_addr, sizeof(serv_addr));  //sets all values in a buffer to 0
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY means server will bind to all netwrok interfaces on machine for given port no
  serv_addr.sin_port = htons(portno); //htons converts a port number in host byte order to a port number in network byte order

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)  //binds a socket to an address
    error("ERROR on binding.");

  listen(sockfd,5);
  printf("Starting Central Indexing Server...\n");
  printf("Hostname: %s\n", hostbuffer);
  printf("IP address: %s\n", IPbuffer);
  printf("Start listening on port %u...\n", portno);

  char buffer[MAXBUFSIZE];  //recevier buffer
  while(1) {
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
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

      struct client tmpClientInfo;
      tmpClientInfo = getClientInfo(newsockfd);

      /*keep receiving message from client*/
      while(1) {
        bzero(buffer, sizeof(buffer));
        if(recv(newsockfd, buffer, sizeof(buffer), 0) > 0) {
          /*Register peer with server*/
          if(strcmp(buffer, "r") == 0) {

            //add client
            bzero(buffer, sizeof(buffer));
            if(addClient(tmpClientInfo.name, tmpClientInfo.ip, tmpClientInfo.port, &sizeClients)) {
              strcat(buffer, "Add client successfull.");
              if(send(newsockfd, buffer, sizeof(buffer), 0) <= 0) {
                printf("Send add client response failed\n");
              }
            }
            else {
              strcat(buffer, "The client has already registered.");
              if(send(newsockfd, buffer, sizeof(buffer), 0) <= 0) {
                printf("Send add client response failed\n");
              }
            }
            //register files
            bzero(buffer, sizeof(buffer));
            if(registerFiles(newsockfd, &sizeClients, &sizeFileList)) {
              strcat(buffer, "Register files successfull.");
              if(send(newsockfd, buffer, sizeof(buffer), 0) <= 0) {
                printf("Send register file response failed\n");
              }
            }
            else {
              strcat(buffer, "Register files failed.");
              if(send(newsockfd, buffer, sizeof(buffer), 0) <= 0) {
                printf("Send register file response failed\n");
              }
            }
          }
          else if(strcmp(buffer, "l") == 0) {
            /*List files registered with server*/
            sendFileList(newsockfd, &sizeFileList);
          }
          else if (strcmp(buffer, "q")) {
            printf("Client disconnected from port no. %u and IP %s\n", cli_port, cli_addr_str );
            /*deregister with server*/
            removeClient(tmpClientInfo.name, &sizeClients, &sizeFileList);
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

/*return client information*/
struct client getClientInfo(int socket) {
  struct client clientInfo;
  char name[MAXBUFSIZE], ip[MAXBUFSIZE], port[MAXBUFSIZE];
  char buffer[MAXBUFSIZE];

  bzero(buffer, sizeof(buffer));
  bzero(name, sizeof(name));
  bzero(ip, sizeof(ip));
  bzero(port, sizeof(port));

  recv(socket, buffer, sizeof(buffer), 0);
  char *start;
  char *end;
  start = buffer;
  end = strchr(buffer, '|');
  //get name from string
  int i = 0;
  while(start != end) {
    name[i] = *start;
    i++;
    start++;
  }
  strcpy(clientInfo.name, name);
  //get ip from string
  start++;
  end = strchr(end+1, '|');
  i = 0;
  while(start != end) {
    ip[i] = *start;
    i++;
    start++;
  }
  strcpy(clientInfo.ip, ip);
  //get port from string
  end++;
  strcpy(port, end);
  clientInfo.port = atoi(port);

  return clientInfo;
}

/*add client to server, return 0 if client exists, return 1 if add successfull*/
int addClient(char name[MAXBUFSIZE], char ip[MAXBUFSIZE], int port, int *sizeClients) {
  int i;
  for(i = 0; i < *sizeClients; i++) {
    //client name already exists
    if(strcmp(clientList[i].name, name) == 0) {
      return 0;
    }
  }
  //copy client info and add them to client list.
  strcpy(clientList[*sizeClients].name, name);
  strcpy(clientList[*sizeClients].ip, ip);
  clientList[*sizeClients].port = port;
  (*sizeClients)++;
  return 1;
}

/*register files with server, return 0 if no file registered, return 1 if file registered*/
int registerFiles(int socket, int *sizeClients, int *sizeFileList) {
  char name[MAXBUFSIZE], size[MAXBUFSIZE], source[MAXBUFSIZE];
  char ip[MAXBUFSIZE], port[MAXBUFSIZE];
  char buffer[MAXBUFSIZE];
  bzero(buffer, sizeof(buffer));
  bzero(name, sizeof(name));
  bzero(size, sizeof(size));
  bzero(source, sizeof(source));
  bzero(ip, sizeof(ip));
  bzero(port, sizeof(port));
  /*wait for client files*/
  if(recv(socket, buffer, sizeof(buffer), 0) > 0) {
    if(strlen(buffer) == 0) { //no file registered
      bzero(buffer, sizeof(buffer));
      strcat(buffer, "No file registered with server");
      if(send(socket, buffer, sizeof(buffer), 0) <= 0) {
        printf("File registration response send failed\n");
        return 0;
      }
      return 0;
    }

    char *start = buffer;
    char *end = buffer;
    char *comma = buffer;
    /*part the string, extract name, size, source, ip, port info*/
    while((start = strchr(start, '<')) != NULL) {
      end = strchr(end + 1, '>');
      start ++;
      //get name from string
      comma = strchr(start, ',');
      strncpy(name, start, comma-start);
      comma++;
      start = comma;
      //get size from string
      comma = strchr(start, ',');
      strncpy(size, start, comma-start);
      comma++;
      start = comma;
      //get source from string
      comma = strchr(start, ',');
      strncpy(source, start, comma-start);
      comma++;
      start = comma;
      //get ip from string
      comma = strchr(start, ',');
      strncpy(ip, start, comma-start);
      comma++;
      start = comma;
      //get port from string
      strncpy(port, start, end-start);
      addFile(name, size, source, ip, port, sizeFileList);
    }
    return 1;
  }
  return 0;
}

void addFile(char *name, char *size, char *source, char *ip, char *port, int *sizeFileList) {
  strcpy(fileList[*sizeFileList].name, name);
  fileList[*sizeFileList].size = atoi(size);
  strcpy(fileList[*sizeFileList].location.name, source);
  strcpy(fileList[*sizeFileList].location.ip, ip);
  fileList[*sizeFileList].location.port = atoi(port);
  (*sizeFileList)++;
}

void sendFileList(int socket, int *sizeFileList) {
  char buffer[MAXBUFSIZE];
  bzero(buffer, sizeof(buffer));
  char currSize[MAXBUFSIZE];
  char currPort[MAXBUFSIZE];

  int i;
  for(i = 0; i < *sizeFileList; i++) {
    bzero(currSize, sizeof(currSize));
    bzero(currPort, sizeof(currPort));

    strcat(buffer, fileList[i].name);
    strcat(buffer, "||");
    sprintf(currSize, "%d", fileList[i].size);
    strcat(buffer, currSize);
    strcat(buffer, " || ");
    strcat(buffer, fileList[i].location.name);
    strcat(buffer, " || ");
    strcat(buffer, fileList[i].location.ip);
    strcat(buffer, " || ");
    sprintf(currPort, "%d", fileList[i].location.port);
    strcat(buffer, currPort);
    strcat(buffer, "\n");
  }
  if(send(socket, buffer, sizeof(buffer), 0) <= 0) {
    printf("Send file list failed\n");
    return;
  }
  return;
}

void removeClient(char name[MAXBUFSIZE], int *sizeClients, int *sizeFileList) {
  int i, j;
  int found = 0;

  for(i = 0; i < *sizeClients; i++) {
    if(!found) {
      if(strcmp(clientList[i].name, name) == 0) {
        found = 1;
        for(j = 0; j < *sizeFileList;) {
          if(strcmp(clientList[i].name, fileList[j].location.name) == 0)
            removeFile(j, sizeFileList);
          else
            j++;
        }
      }
    }
    else {
      strcpy(clientList[i-1].name, clientList[i].name);
      strcpy(clientList[i-1].name, clientList[i].ip);
      clientList[i-1].port = clientList[i].port;
    }
  }
  if(found) {
    (*sizeClients)--;
    (*sizeFileList)--;
  }
}

void removeFile(int index, int *sizeFileList) {
  int i;
  for(i = index + 1; i < *sizeFileList; i++) {
    strcpy(fileList[i-1].name, fileList[i].name);
    fileList[i-1].size = fileList[i].size;
    strcpy(fileList[i - 1].location.name, fileList[i].location.name);
    strcpy(fileList[i - 1].location.ip, fileList[i].location.ip);
    fileList[i - 1].location.port = fileList[i].location.port;
  }
  (*sizeFileList)--;
}
