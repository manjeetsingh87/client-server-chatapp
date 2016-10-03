#ifndef CLIENT_H
#define CLIENT_H

#define BUFFER_SIZE 2048
#define BACKLOG 10     // how many pending connections queue will hold
#define CLIENTS_MAX_SIZE 10
#define TAM 4 //elements

//LIST struct to display LIST data to client
struct ClientList {
	char *machine_name;
	char *user_adderss;
	int port;
};

//start client application method
int start_client(char *port);

#endif
