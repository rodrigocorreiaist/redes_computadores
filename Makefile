CC = gcc
CFLAGS = -Iinclude -Wall -g
SRC = src/event_server.c src/user.c src/utils.c
OBJ = $(SRC:.c=.o)
EXEC = event_server user

all: $(EXEC)

event_server: src/event_server.o src/utils.o
	$(CC) -o event_server src/event_server.o src/utils.o

user: src/user.o src/utils.o
	$(CC) -o user src/user.o src/utils.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(EXEC)