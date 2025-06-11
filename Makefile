# Makefile for multi-client chat system
CC = gcc
CFLAGS = -Wall -pthread

all: server client client_gui

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c -lpthread

# Add target for client GUI
client_gui: client_gui.c
	$(CC) $(CFLAGS) -o client_gui client_gui.c `pkg-config --cflags --libs gtk+-3.0` -lpthread

runServer:
	./server 50000

runClient:
	./client localhost 50000

clean:
	rm -f  *~

clean-all:
	rm -f server client client_gui
