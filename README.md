Introduction:
In this project, we design a simple peer-to-peer(P2P) system in which contains two components: a central indexing server and a peer. The server indexes all the files registered by peers and provides search facility to peers. The peer acts as a client and a server. It searches file on the server and obtains the file from other peers.

How to execute and run the program:
Compile:
Run “make”, or run “gcc -Wall server.c -o server.out”, “gcc -Wall client.c -o peer_1/client.out”, “gcc -Wall client.c -o peer_2/client.out”, “gcc -Wall client.c -o peer_3/client.out”. It will compile the server.c into server.out, compile the client.c into peer_1/client.out, peer_2/client.out and peer_3/client.out.

For central indexing server:
	Run “make run_server”, it will start the server listening on port 8080; or Run “./server <port no>”.
	The terminal will show:
	Starting Central Indexing Server…
	Hostname: <host name>
	IP address: <IP address>
	Start listening on port <port no.>...
	When connected with client:
	Client connected from port no. <port no.> and IP <IP address>
When register request received:
	Request from <client name>, register file <filename.extension>
When search request received:
	Request from <client name>, search file <filename.extension>
When obtain request received:
	Client obtaining file from other peers
When quit request received:
	Client disconnected from port no. <port no.> and IP <IP address>

For client:
Go to each peer directory, run “./client <server ip> <server port> <listening port>”.
The terminal will show:
--------Welcome to P2P file system--------
1: ----------------------- Register with server
2: ----------------------------------- Search file
3: ------------------------------------ Obtain file
4: --------Deregister with Server and Quit
Enter choice:
Follow commands on screen for register, search, obtain, deregister and quit.
Option 1: Register with server.
Terminal: Enter the file name with extension:
Type: “<filename.extension>”
When register successful:
	File register successful.
Option 2: Search file.
	Terminal: Enter the file name to search: :
	Type: “<filename.extension>”
	Terminal: Waiting for response...
	When get search result:
		File name	Peer name	Peer IP	Peer Port
		<file.ext>	<name>	<IP addr>	<Port no.>
Option 3: Obtain file.
	Terminal: Enter file name to be obtained with extension:
	Type: “<filename.extension>”
Terminal: Enter peer IP address:
	Type: “<IP address>”
Terminal: Enter peer listening port:
	Type: “<Port no.>”
	When connect successful with other peer:
		Receiving file from peer…
	When download successful:
Obtain file <filename.extension>successful!
	Option 4: Deregister with Server and Quit
		Terminal: Exit peer service, deregister on the server
		Quit the program

List of Programs developed:
Server.c
This source file is implementation of server, which is bound to a known port and accepts client connection requests using socket. It serves client request such as registering files, recording the peers’ status and so on. When it receives a register request, it stores the information (including file name shared by client, client name, ip address, sharing port etc.) sent by the client to a file. When it receives a search request, it runs a search script on a file in which contains all the information registered by clients and then stores the result into a file, the program reads the result file and sends back the result to the client. When a deregister request is read, it updates the clients’ status and kills the process serving that client.

Client.c
This source file is implementation of client, which is used to connect to the central indexing server and is connected  to other peers to provide downloading files. To start this program, the central indexing server’s ip address, server port and listening port need to be specified by user. It provides option “register” to register files path and name with the server, option “search” to send search request to the server and receive the results sent back from server, option “obtain” to obtain files from other peers, option “deregister” to deregister from the server and quit the client program.

Searchscript.sh
This shell script is used by server.c to search text requested by clients, it uses “grep” command line to search plain-text data sets for lines that match a regular expression and saves the lines into a searchresult file.

Project summary and possible improvement:
We use concepts like socket programming and parent-child processes to implement this project. The socket is used extensively throughout the whole project. Fork system call is used to handle multiple clients connections on the server side and handle transferring files , keep communicating with server on the client side. To keep records of clients and files shared, we use a text file to store these data. A shell script is used to provide search function.

To make this project more efficient and easier to use, several possible improvements could be made:
Automatically generate the data of files to be shared instead of typing manually.
Design a data structure to store the registration data instead of using a file.
