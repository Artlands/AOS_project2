CC=gcc -DDEBUG
CLIENT=client
SERVER=server

all:
	gcc -Wall client.c -o peer_1/client.out
	gcc -Wall client.c -o peer_2/client.out
	gcc -Wall client.c -o peer_3/client.out
	gcc -Wall server.c -o server.out

run_server: $(SERVER).out
	./$< 8080

clean:
	rm -rf $(CLIENT).out $(SERVER).out *.out *.log
