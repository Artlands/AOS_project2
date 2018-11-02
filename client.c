#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define MAXBUFSIZE 2048

// Called when a system call fails
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int registerClient(char *name, char *ip, char *port, int socket);

int main(int argc, char *argv[]) {
  pid_t pid;
  char input[MAXBUFSIZE]; //user input stored
  char output[MAXBUFSIZE]; //send file stored
  char buffer[MAXBUFSIZE]; //recevie message stored
  int command; // user command stored
  char *temp; // variable to store temporary values
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

  int enable1 = 1; //set SO_REUSEADDR so that socket immediate unbinds from port after closed
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable1, sizeof(int)) < 0)
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

  int enable2 = 1; //set SO_REUSEADDR so that socket immediate unbinds from port after closed
  if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &enable2, sizeof(int)) < 0)
    error("setsockopt(SO_REUSEADDR) failed.");

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;  //INADDR_ANY means server will bind to all netwrok interfaces on machine for given port no
  server.sin_port = htons(atoi(argv[3]));
  bzero(&server.sin_zero, 8); //padding zeros

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

    printf("--------Welcome to P2P file system--------\n");
    printf("1: ------------------ Register with server\n");
    printf("2: --------------------------- Search file\n");
    printf("3: --------------------------- Obtain file\n");
    printf("4: --------Deregister with Server and Quit\n");
    printf("Enter choice: \t");
    if (scanf("%d", &command) <= 0) {
      printf("Enter only an integer from 1 to 4\n");
      kill(pid, SIGTERM);
      exit(0);
    }
    else {
      switch (command) {
        case 1:
          temp = "r";
          send(sockfd, temp, sizeof(temp), 0);
          printf("Enter the file name with extension: ");
          // fgets(input, MAXBUFSIZE, stdin);
          scanf(" %[^\t\n]s", input);
          send(sockfd, input, strlen(input), 0); // send input to server
          len = recv(sockfd, output, sizeof(output), 0);
          output[len] = '\0';
          printf("%s\n", output);
          bzero(output, sizeof(output));
          break;
        case 2:
          temp = "s";
          send(sockfd, temp, sizeof(temp), 0);
          printf("Enter the file name to search: ");
          scanf(" %[^\t\n]s", input); //recieve user input, --scanf to accept multi-word string
          send(sockfd, input, strlen(input), 0); // send input to server
          printf("waiting for response...\n");
          printf("Filename\tPeer name\t\tPeer IP\t\tPeer Port\n");

          recv(sockfd, output, MAXBUFSIZE, 0);
          printf("%s\n", output);
          bzero(output, sizeof(output));
          printf("Search complete!\n");
          break;
        case 3:
          temp = "o";
          send(sockfd, temp, sizeof(temp), 0);
          /*obtain file from other peer*/
          printf("Enter file name to be obtained with extension: ");
          scanf(" %[^\t\n]s", file_name);
          printf("Enter peer IP address:  ");
          scanf(" %[^\t\n]s", peer_ip);
          printf("Enter peer listening port:  ");
          scanf(" %[^\t\n]s", peer_port);

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

          /*send file keyword to peer*/
          send(peer_sock, file_name,strlen(file_name), 0);
          printf("Receiving file from peer...\n");

          char *recv_name = file_name;
          FILE *obtain_file = fopen(recv_name, "w");
          if(obtain_file == NULL) {
            printf("File %s cannot be created.\n", recv_name);
          }
          else {
            bzero(buffer, sizeof(buffer));
            int file_obtain_size = 0;
            int len_recv = 0;
            while((file_obtain_size = recv(peer_sock, buffer, MAXBUFSIZE, 0)) > 0) {
              len_recv = fwrite(buffer, sizeof(char), file_obtain_size, obtain_file);
              if(len_recv < file_obtain_size) {
                printf("ERROR on writing file. Try agian\n");
              }
              bzero(buffer, sizeof(buffer));
              if(file_obtain_size == 0 || file_obtain_size != MAXBUFSIZE) {
                break;
              }
            }
            if(file_obtain_size < 0){
              printf("ERROR on receiving. Try agian\n");
              break;
            }
            fclose(obtain_file);
            printf("Obtain file %s successful!\n", file_name);
            close(peer_sock);
          }
        case 4:
          temp = "q";
          send(sockfd, temp, sizeof(temp), 0);
          printf("Exit peer service, deregister on the server\n");
          close(listen_sock);
          kill(pid, SIGTERM);
          exit(0);
        default:
          printf("Invalid choice\n");
      }//close switch
    }//close else
  }//close while loop
  close(listen_sock);
  return (0);
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
