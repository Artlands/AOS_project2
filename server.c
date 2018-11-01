#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#define MAXBUFSIZE 2048

int add_Client(char *peer_name, char*peer_ip); //add client and IP to the list
int update_Client(char *peer_name, char*peer_ip); //update client and IP to the list
time_t current_time; // variable to store current time

/*save client info*/
struct client {
  char name[MAXBUFSIZE];
  char ip[MAXBUFSIZE];
  int port;
};

// Called when a system call fails
void error(const char *msg) {
  perror(msg);
  exit(1);
}

struct client getClientInfo(char *clientString);

int main(int argc, char *argv[]) {
  pid_t pid;
  char clientInfo[MAXBUFSIZE]; // client info string stored
  char buffer[MAXBUFSIZE];  //recevier buffer
  struct client tmpClientInfo;
  char client_name[MAXBUFSIZE];
  char client_ip[MAXBUFSIZE];
  char client_port[MAXBUFSIZE];

  /*variables for creating socket*/
  int sockfd, newsockfd, portno;  //sockfd, newsockfd are file descriptors, store values returned by the socket system call and the accept system call.
  socklen_t clilen; //clilen stores the size of the address of the client.
  struct sockaddr_in serv_addr, cli_addr; //structure containing internet addresses
  clilen = sizeof(cli_addr);
  int len;// variables to measure incoming stream from user

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

  /*set up port for listening incoming peer requests*/
  sockfd = socket(AF_INET, SOCK_STREAM, 0); //create a new socket, returns an entry into file descriptor table
  if (sockfd < 0)
    error("ERROR opening socket.");

  int enable = 1; //set SO_REUSEADDR so that socket immediate unbinds from port after closed
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    error("setsockopt(SO_REUSEADDR) failed.");

  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY means server will bind to all netwrok interfaces on machine for given port no
  serv_addr.sin_port = htons(portno); //htons converts a port number in host byte order to a port number in network byte order

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)  //binds a socket to an address
    error("ERROR on binding.");

  if(listen(sockfd,5) < 0) {
    error("ERROR on listening.");
  }

  printf("Starting Central Indexing Server...\n");
  printf("Hostname: %s\n", hostbuffer);
  printf("IP address: %s\n", IPbuffer);
  printf("Start listening on port %u...\n", portno);

  while(1) {
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if(newsockfd < 0)
      error("ERROR on accepting new connection");

    /*create seperate process for each client*/
    pid = fork();
    if(!pid){ //for multiple connections, child process
      close(sockfd); // close the socket for other connections

      /*get client ip address and port*/
      printf("Client connected from port no. %u and IP %s\n",
            ntohs(cli_addr.sin_port), inet_ntoa(cli_addr.sin_addr));

      /*recevie client information*/
      bzero(clientInfo, sizeof(clientInfo));
      len = recv(newsockfd, clientInfo, sizeof(clientInfo), 0);
      clientInfo[len] = '\0';
      tmpClientInfo = getClientInfo(clientInfo);
      strcpy(client_name, tmpClientInfo.name);
      strcpy(client_ip, tmpClientInfo.ip);
      sprintf(client_port, "%d", tmpClientInfo.port);
      add_Client(client_name, client_ip);

      /*keep receiving message from client*/
      while(1) {
        bzero(buffer, sizeof(buffer));
        len = recv(newsockfd, buffer, sizeof(buffer), 0);
        buffer[len] = '\0';
        printf("Request from %s, %s\n",client_name, buffer); // check client request

        /*connection check*/
        if(len <= 0) {
          if(len == 0)
            printf("Peer %s hung up\n",inet_ntoa(cli_addr.sin_addr));
          else
            printf("ERROR on receving\n");
          close(newsockfd);
          exit(0);
        }

        /*Register peer with server*/
        if(strcmp(buffer, "r") == 0) {
          /*add client*/
          bzero(buffer, sizeof(buffer));
          char *fileinfo = "filelist.txt";
          FILE *filedet = fopen(fileinfo, "a+");

          if(filedet == NULL) {
            error("ERROR on opening File List file.");
          }
          else {
            fwrite("\n", sizeof(char), 1, filedet);
          }
          len = recv(newsockfd, buffer, sizeof(buffer), 0);
          printf("Request from %s, register file %s\n",client_name, buffer);
          fwrite(&buffer, len, 1, filedet);
          char report[] = "File register successfull";
          send(newsockfd, report, sizeof(report), 0);

          fwrite("\t", sizeof(char), 1, filedet);
          fwrite(client_name, strlen(client_name), 1, filedet);
          fwrite("\t", sizeof(char), 1, filedet);
          fwrite(client_ip, strlen(client_ip), 1, filedet);
          fwrite("\t", sizeof(char), 1, filedet);
          fwrite(client_port, strlen(client_port), 1, filedet);

          fclose(filedet);
          printf("%s\n", report);
        }
        else if(strcmp(buffer, "s") == 0) {
          bzero(buffer, sizeof(buffer));
          len = recv(newsockfd, buffer, sizeof(buffer), 0);
          buffer[len] = '\0';
          printf("Request from %s ,search file: %s\n",client_name, buffer);

          char command[50];
          system("chmod 755 ./searchscript.sh");// command executed to give execute permissions to script
          strcpy(command, "./searchscript.sh ");
          strcat(command, buffer);
          system(command);

          char *search_result = "searchresult.txt";
          bzero(buffer, sizeof(buffer));
          FILE *file_search = fopen(search_result, "r");
          if(file_search == NULL) {
            error("Unable to open file.");
          }

          fread(buffer, sizeof(char), MAXBUFSIZE, file_search);
          send(newsockfd, buffer, sizeof(buffer), 0);
          fclose(file_search);
        }
        else if (strcmp(buffer, "o") == 0) {
          bzero(buffer, sizeof(buffer));
          printf("Client obtaning file from other peers\n");
        }
        else if (strcmp(buffer, "q") == 0) {
          bzero(buffer, sizeof(buffer));
          printf("Client disconnected from port no. %u and IP %s\n",
                ntohs(cli_addr.sin_port), inet_ntoa(cli_addr.sin_addr));
          update_Client(client_name,client_ip);
          close(newsockfd);
          // kill(pid, SIGTERM);
          exit(0);
        }
      } //close inner while loop
    }
    close(newsockfd);
  } //close main while loop
  printf("Server shutting down...\n");
  return 0;
}

/*return client information*/
struct client getClientInfo(char *clientString) {
  struct client clientInfo;
  char name[MAXBUFSIZE], ip[MAXBUFSIZE], port[MAXBUFSIZE];

  bzero(name, sizeof(name));
  bzero(ip, sizeof(ip));
  bzero(port, sizeof(port));

  char *start;
  char *end;
  start = clientString;
  end = strchr(clientString, '|');
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

int add_Client(char *peer_name, char*peer_ip) {
  char *peerinfo = "Peer_IPList.txt";
  FILE *peerdet = fopen(peerinfo, "a+");
  if(peerdet == NULL) {
    printf("ERROR on opening IP List file.");
    return -1;
  }
  else {
    fwrite("\n", sizeof(char), 1, peerdet);
    fwrite(peer_name, 1, strlen(peer_name), peerdet);
    fwrite(": ", sizeof(char), 1, peerdet);
    fwrite(peer_ip, 1, strlen(peer_ip), peerdet);
    char update[] = " connected to server at ";
    fwrite(update, 1, strlen(update), peerdet);
    current_time = time(NULL);
    fwrite(ctime(&current_time), 1, strlen(ctime(&current_time)), peerdet);
    fclose(peerdet);
    return 0;
  }
}

int update_Client(char *peer_name, char*peer_ip) {
  char *peerinfo = "Peer_IPList.txt";
  FILE *peerdet = fopen(peerinfo, "a+");
  if(peerdet == NULL) {
    printf("ERROR on opening IP List file.");
    return -1;
  }
  else {
    fwrite("\n", sizeof(char), 1, peerdet);
    fwrite(peer_name, 1, strlen(peer_name), peerdet);
    fwrite(": ", sizeof(char), 1, peerdet);
    fwrite(peer_ip, 1, strlen(peer_ip), peerdet);
    char update[] = " disconnected to server at ";
    fwrite(update, 1, strlen(update), peerdet);
    current_time = time(NULL);
    fwrite(ctime(&current_time), 1, strlen(ctime(&current_time)), peerdet);
    fclose(peerdet);
    return 0;
  }
}
