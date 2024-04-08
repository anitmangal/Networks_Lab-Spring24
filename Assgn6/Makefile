CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS =

CLIENT_SRC = simDNSClient.c
SERVER_SRC = simDNSServer.c

CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)

CLIENT_EXEC = simDNSClient
SERVER_EXEC = simDNSServer

all: $(CLIENT_EXEC) $(SERVER_EXEC)

$(CLIENT_EXEC): $(CLIENT_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(SERVER_EXEC): $(SERVER_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean run_client run_server

clean:
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ) $(CLIENT_EXEC) $(SERVER_EXEC)

run_client: $(CLIENT_EXEC)
	@echo "Enter MAC address of the server: "; \
	read mac; \
	sudo ./$(CLIENT_EXEC) mac;

run_server: $(SERVER_EXEC)
	sudo ./$(SERVER_EXEC)

