#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "linkedlist.h"

#define MAXBUFSIZE 2048
#define MAXINTSIZE 8

void error(const char *msg)
{
    perror(msg);
    exit(0);
}
int getFiles(List **fileList, char *name, char *ip, char *port);
int registerClient(char *name, char *ip, char *port, int sock);

int main(int argc, char *argv[])
{
    List *fileList = emptylist();
    int getSocket, getPort;
    pid_t pid;

    char hostbuffer[256];
    char *myIP;
    char myPort[12];
    struct hostent *host_entry;
    int hostname;
    //get hostname, store in hostbuffer
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    host_entry = gethostbyname(hostbuffer);
    //get IP address, inet_ntop - convert IPv4 and IPv6 addresses from binary to text form
    myIP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));

    //init server addr with address and port provided in argv
    // int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = 0;

    //create getSocket and set SO_REUSEADDR
    getSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (getSocket < 0)
      error("ERROR opening socket");
    //set SO_REUSEADDR so that socket immediate unbinds from port after closed
    int enable = 1;
    if (setsockopt(getSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
      error("setsockopt(SO_REUSEADDR) failed");

    if (bind(getSocket, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
      error("ERROR on binding");
    //get myPort no., other peers will connect to this port.
    getPort = ntohs(serv_addr.sin_port);
    sprintf(myPort, "%d", getPort);
    listen(getSocket, 5);
    printf("Start listening on port %d...\n", getPort);

    //Create file list
    if(getFiles(&fileList, argv[1], myIP, myPort))
      printf("ERROR on getting files\n");

    //create a child process, one process handles file download requests
    //the other process connects and talks to the central indexing server
    pid = fork();
    if(pid >= 0) {
      if(pid == 0) {
        // init server addr with addr and port provided in argv
        int socketfd;
        struct sockaddr_in serv_addr;
        bzero((char *) &serv_addr, sizeof(serv_addr));   //sets all values in a buffer to 0
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
        serv_addr.sin_port = htons(atoi(argv[2]));

        //create socketfd and set SO_REUSEADDR
        socketfd = socket(AF_INET, SOCK_STREAM, 0);
        int enable = 1;
        if (setsockopt(getSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
          error("setsockopt(SO_REUSEADDR) failed");

        //attemp to connect to the central indexing server
        if(connect(socketfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
          error("ERROR connecting");
        printf("Connect with the central indexing server successfull!\n");

        char command[MAXBUFSIZE];
        // char buffer[MAXBUFSIZE];

        //wait for commands
        while(1) {
          printf("---------------------------------\n");
          printf("r: --------- Register with server\n");
          printf("l: ------------ Get list of files\n");
          printf("s: ------------------ Search file\n");
          printf("o: ------------------ Obtain file\n");
          printf("q: ------------------------- Quit\n");
          printf("Enter choice: ");
          scanf("%s", command);

          if(strcmp(command, "r") == 0) {
            //Register client name
            if(registerClient(argv[1], myIP, myPort, socketfd)) {
              printf("Client Name already registered.\n");
              continue;
            }
            // if(registerFiles(socketfd, fileList)) {
            //   printf("Register files failed\n");
            //   continue;
            // }
          }
          else if(strcmp(command, "l") == 0) {
            printf("List request sent to the server.\n");
          }
          else if(strcmp(command, "s") == 0) {
            printf("Search request sent to the server.\n");
          }
          else if(strcmp(command, "o") == 0) {
            printf("Obtain request sent to the server.\n");
          }
          else if(strcmp(command, "q") == 0) {
            error("Exit peer service, deregister on the server\n");
          }
          else {
            printf("Invalid choice, please enter your choice agian.\n");
            continue;
          }
        }
        close(socketfd);
      }
      else {
        printf("Handle file downloads\n");
      }
      exit(0);
    }
    return 0;
}

//Register client name with server
int registerClient(char *name, char *ip, char *port, int sock){
  char buffer[MAXBUFSIZE];
  bzero(buffer, sizeof(buffer));

  strcat(buffer, name);
  strcat(buffer, "-");
  strcat(buffer, ip);
  strcat(buffer, "-");
  strcat(buffer, port);

  if(send(sock, buffer, sizeof(buffer), 0) > 0) {
    bzero(buffer, sizeof(buffer));
    if(recv(sock, buffer, sizeof(buffer), 0) > 0) {
      if(strcmp(buffer, "success") == 0) {
        return 0;
      }
      return 1;
    }
    else {
      printf("Receive after name send failed.\n");
      return 1;
    }
  }
  else {
    printf("Name send failed\n");
    return 1;
  }
  return 1;
}

// Get files information in the directory, save them into a linked list
int getFiles(List **fileList, char *name, char *ip, char *port) {
  DIR *dirp;
  struct dirent *dp;
  struct stat fileStats;
  char *fileName;
  *fileList = emptylist();

  if ((dirp = opendir(".")) == NULL) {
    perror("Cannot open '.'\n");
    return 1;
  }
  do {
    if ((dp = readdir(dirp)) != NULL) {
      fileName = dp -> d_name;
      if((strcmp(fileName, ".") != 0) && (strcmp(fileName, "..") != 0)) {
        stat(fileName, &fileStats);
        FileInfo fileInfo = {fileName, fileStats.st_size, ip, port};
        add(fileInfo, *fileList);
      }
    }
  } while (dp != NULL);
  return 0;
}
