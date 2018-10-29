CC=gcc -DDEBUG
CLIENT=client
SERVER=server

all: linkedlist.o
	gcc -Wall linkedlist.o client.c -o client
	gcc -Wall server.c -o server

# all: $(CLIENT).out $(SERVER).out
#
# %.out : %.c
# 	$(CC) -o $@ $^ -lpthread

linkedlist.o: linkedlist.h

run_server: $(SERVER).out
	./$< 8080
run_client: $(CLIENT).out
	./$< localhost 8080

# view_result:
# 	echo "Result of server:"
# 	cat server.log
# 	echo "Result of client:"
# 	cat client.log

clean:
	rm -rf $(CLIENT).out $(SERVER).out *.out *.log
