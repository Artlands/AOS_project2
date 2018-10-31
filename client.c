// linkedlist.h refer to https://github.com/skorks/c-linked-list
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
#include <sys/select.h>
#include <signal.h>
#include "linkedlist.h"

#define MAXBUFSIZE 2048

void error(const char *msg)
{
    perror(msg);
    exit(0);
}
int getFiles(List **fileList, char *ip, char *port);
int listToBuffer(List *fileList, char *buffer);
int registerClient(char *name, char *ip, char *port, int socket);
int registerFiles(int socket, List *fileList);
int obtainFile(int socket, struct sockaddr_in peer, char file_name,
               char *ip, char *port, FILE *file);

int main(int argc, char *argv[]) {
  List *fileList = emptylist();
  pid_t pid;
  char input[MAXBUFSIZE]; //user input stored
  char output[MAXBUFSIZE]; //send file stored
  char buffer[MAXBUFSIZE]; //recevie message stored
  char command[MAXBUFSIZE]; // user command stored
  char hostname[256];  // host name stored
  char *myIP; //local ip address
  char myPort[MAXBUFSIZE];    //port used for other peers
  struct hostent *host_entry;
  int len; //length of recived input stream

  /*variables for acting as a client*/
  int sockfd; //file descriptor for connecting to the central server
  struct sockaddr_in central_server;

  /*variables for obtaining file from other peers*/
  int peer_sock;
  char file_name[MAXBUFSIZE];
  char peer_ip[MAXBUFSIZE];
  char peer_port[MAXBUFSIZE];
  struct sockaddr_in peer_connect;

  /*variables for acting as a server*/
  int listen_sock;
  struct sockaddr_in server, client;
  socklen_t clilen;

  /*variables for select system call*/
  fd_set master;  //master file descriptor
  fd_set read_fd; //for select


  if (argc < 3){
    error("ERROR, Enter <server ip> <server port> <listening port>");
  }

  /*get hostname, ip addr, port no. */
  gethostname(hostname, sizeof(hostname));  //get hostname, store in hostname
  host_entry = gethostbyname(hostname);
  myIP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));  //get IP address, inet_ntop - convert IPv4 and IPv6 addresses from binary to text form
  strcpy(myPort, argv[3]);

  /*Connect with central server*/
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0)
    error("ERROR on opening socket.");

  int enable = 1; //set SO_REUSEADDR so that socket immediate unbinds from port after closed
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    error("setsockopt(SO_REUSEADDR) failed");

  central_server.sin_family = AF_INET;
  central_server.sin_addr.s_addr = inet_addr(argv[1]);
  central_server.sin_port = htons(atoi(argv[2]));
  bzero(&central_server.sin_zero, 8); //padding zeros

  if(connect(sockfd, (struct sockaddr *) &central_server, sizeof(central_server)) < 0)
    error("ERROR on connecting");
  printf("Connect with the central indexing server successfull!\n");

  /*Register client name*/
  if(registerClient(hostname, myIP, myPort, sockfd) == -1) {
    error("ERROR on sending client info");
  }

  /*Set up port for listening incoming connections for download files*/
  listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  if(listen_sock < 0)
    error("ERROR on opening socket.");

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;  //INADDR_ANY means server will bind to all netwrok interfaces on machine for given port no
  server.sin_port = htons(atoi(argv[3]));

  if(bind(listen_sock, (struct sockaddr *) &server, sizeof(server)) < 0)
    error("ERROR on binding.");

  /*Use select system call to handle multiple connections*/
  int i;
  FD_ZERO(&master);
  FD_SET(listen_sock, &master);

  /*Create a listening process for incoming connections*/
  pid = fork();
  if(!pid) {
    while(1) {
      /*waiting for incoming request*/
      read_fd = master;
      if(select(FD_SETSIZE, &read_fd, NULL, NULL, NULL) == -1) {
        printf("ERROR on select\n");
        return -1;
      }

      /*handle multiple connections*/
      for (i = 0; i < FD_SETSIZE; ++i) {
        if(FD_ISSET(i, &read_fd)) { //return true if in in read_fd
          if( i == listen_sock) {
            int new_peer_sock;
            new_peer_sock = accept(listen_sock, (struct sockaddr *) &client, &clilen);
            if(new_peer_sock < 0) {
              error("ERROR on accepting new connection.");
            }
            else {
              FD_SET(new_peer_sock, &master); // add to master set
              printf("New peer connected from IP %s and prot no. %d\n",
                    inet_ntoa(client.sin_addr), ntohs(client.sin_port));
            }
          }
          else {  //handle data from a client
            bzero(input, sizeof(input));
            if((len = recv(i, input, sizeof(input), 0) <= 0)){  //connection closed by client or error
              if(len == 0) {
                printf("Peer %d with IP %s hung up\n", i, inet_ntoa(client.sin_addr));
              }
              else {
                printf("ERROR on receiving\n");
              }
              close(i); //close this connection
              FD_CLR(i, &master);
            }
            else {
              printf("Request file: %s\n", input);  //file name requested by other peer

              /*read and transfer  file*/
              char *requested_file = input;
              FILE *file_request = fopen(requested_file, "r");
              if(file_request == NULL) {
                printf("Requested file not found\n");
                close(i);
                FD_CLR(i, &master);
              }
              else {
                bzero(output, sizeof(output));
                int file_request_send;
                while((file_request_send = fread(output, sizeof(char),
                                            MAXBUFSIZE, file_request)) > 0) {
                  if((send(i, output, file_request_send, 0)) < 0) {
                    printf("ERROR on sending file\n");
                  }
                  bzero(output, sizeof(output));
                }
              }
              close(i);
              FD_CLR(i, &master);
            } //close else of none error
          } //close handle data from client
        } //close if statement of fd_isset
      } //close for loop
    } //close while loop
    close(listen_sock);
    exit(0);
  } //close if of fork system call

  /*Display menu for user input*/
  while(1) {
    printf("----------------------------------\n");
    printf("r: ---------- Register with server\n");
    printf("l: ------------- Get list of files\n");
    printf("s: ------------------- Search file\n");
    printf("o: ------------------- Obtain file\n");
    printf("q: Deregister with Server and Quit\n");
    printf("Enter choice: \t");
    scanf("%s", command);
    send(sockfd, command, sizeof(command), 0);
    printf("Choice:%s\n", command);

    if(strcmp(command, "r") == 0) {   //register request
      /*Create file list*/
      if(getFiles(&fileList, myIP, myPort) != 0) {
        printf("ERROR on getting files\n");
        continue;
      }
      /*Register files*/
      if(registerFiles(sockfd, fileList) != 0) {
        printf("Register files failed\n");
        continue;
      }
      printf("Register with server successfull\n");
    }
    else if(strcmp(command, "l") == 0) {  //list files request
      printf("List request sent to the server.\n");
      /*Recevie list from server*/
      bzero(buffer, sizeof(buffer));
      if(recv(sockfd, buffer, sizeof(buffer), 0) <= 0) {
        printf("Receive file list failed\n");
        continue;
      }
      else {
        printf("File list:\n");
        printf("%s\n", buffer);
      }
    }
    // else if(strcmp(command, "s") == 0) {
    //   printf("Search request sent to the server.\n");
    // }
    else if(strcmp(command, "o") == 0) {
      /*obtain file from other peer*/
      printf("Enter file name to be obtained with extension: \t");
      scanf("%s\n", file_name);
      printf("Enter peer IP address: \t");
      scanf("%s\n", peer_ip);
      printf("Enter peer listening port: \t");
      scanf("%s\n", peer_port);

      /*create local file to save the obtained file*/
      FILE *obtain_file = fopen(file_name, "w");
      if(obtain_file == NULL) {
        printf("File %s cannot be created\n", file_name);
        continue;
      }
      printf("Obtain request sent to the peer: %s\n", peer_ip);

      /*create socket to desired peer*/
      peer_sock = socket(AF_INET, SOCK_STREAM, 0);
      if(peer_sock < 0) {
        printf("ERROR oon opening socket.\n");
        continue;
      }

      peer_connect.sin_family = AF_INET;
      peer_connect.sin_addr.s_addr = inet_addr(peer_ip);
      peer_connect.sin_port = htons(atoi(peer_port));
      bzero(&peer_connect.sin_zero, 8);

      /*try to connect desired peer*/
      if(connect(peer_sock, (struct sockaddr *) &peer_connect, sizeof(peer_connect)) < 0){
        printf("ERROR on connecting to other peers\n");
        continue;
      }

      /*down load file*/
      bzero(buffer, sizeof(buffer));
      int file_obtain_size = 0;
      int len_recv = 0;
      while((file_obtain_size = recv(peer_sock, buffer, MAXBUFSIZE, 0)) > 0) {
        len_recv = fwrite(buffer, sizeof(char), file_obtain_size, obtain_file);
        if(len_recv < file_obtain_size) {
          printf("ERROR on writing file. Try agian\n");
          continue;
        }
        bzero(buffer, sizeof(buffer));
        if(file_obtain_size == 0 || file_obtain_size != MAXBUFSIZE) {
          break;
        }
      }
      if(file_obtain_size < 0){
        printf("ERROR on receiving. Try agian\n");
        continue;
      }
      printf("Obtain file %s successfull1\n", file_name);
    }
    else if(strcmp(command, "q") == 0) {
      printf("Exit peer service, deregister on the server\n");
      kill(pid, SIGTERM);
      exit(0);
    }
    else {
      printf("Invalid choice, please enter your choice agian.\n");
      continue;
    }
  }
}

/* Get files information in the directory, save them into a linked list*/
int getFiles(List **fileList, char *ip, char *port) {
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

/*Convert linkedlist to buffer*/
int listToBuffer(List *fileList, char *buffer) {
  if(fileList->head != NULL) {
    Node *currNode = fileList->head;
    strcat(buffer, "<");
    strcat(buffer, currNode->data.name);
    strcat(buffer, ",");
    sprintf(buffer, "%s%d", buffer, currNode->data.size);
    strcat(buffer, ",");
    strcat(buffer, currNode->data.owner);
    strcat(buffer, ",");
    strcat(buffer, currNode->data.ip);
    strcat(buffer, ",");
    strcat(buffer, currNode->data.port);
    strcat(buffer, ">");

    while(currNode->next != NULL) {
      currNode = currNode->next;
      strcat(buffer, "<");
      strcat(buffer, currNode->data.name);
      strcat(buffer, ",");
      sprintf(buffer, "%s%d", buffer, currNode->data.size);
      strcat(buffer, ",");
      strcat(buffer, currNode->data.owner);
      strcat(buffer, ",");
      strcat(buffer, currNode->data.ip);
      strcat(buffer, ",");
      strcat(buffer, currNode->data.port);
      strcat(buffer, ">");
    }

    if(strlen(buffer) > 0) return 0;
    return 1;
  }
  return 1;
}

/*Register client name with server*/
int registerClient(char *name, char *ip, char *port, int socket){
  char buffer[MAXBUFSIZE];
  bzero(buffer, sizeof(buffer));

  strcat(buffer, name);
  strcat(buffer, "|");
  strcat(buffer, ip);
  strcat(buffer, "|");
  strcat(buffer, port);

  if(send(socket, buffer, sizeof(buffer), 0) > 0) {
    return 0;
  }
  else {
    printf("Name send failed\n");
    return -1;
  }
}

/*Register client files with server*/
int registerFiles(int socket, List *fileList) {
  char buffer[MAXBUFSIZE];
  bzero(buffer, sizeof(buffer));
  if(listToBuffer(fileList, buffer) != 0){
    printf("Failed to read file list.\n");
    return 1;
  }
  /*send buffer to server*/
  if(send(socket, buffer, sizeof(buffer), 0) != 0) {
    printf("Failed to send file list.\n");
    return 1;
  }
  return 0;
}
