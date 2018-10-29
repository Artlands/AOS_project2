CC=gcc -DDEBUG
CLIENT=client
SERVER=server


all: $(CLIENT).out $(SERVER).out

%.out : %.c
	$(CC) -o $@ $^ -lpthread

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
