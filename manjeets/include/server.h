#ifndef SERVER_H
#define SERVER_H

#define BUFFER_SIZE 2048
#define BACKLOG 10     // how many pending connections queue will hold
#define CLIENTS_MAX_SIZE 10
#define TAM 4 //elements

void *get_in_addr(struct sockaddr *sa);
void findAddress(struct addrinfo *hints, struct addrinfo *ai);
int create_server_connection (char *input_port);

struct Statistics {
	char *machine_name;
	const char *user_adderss;
	int port;
	int messages_sent ;
	int messages_received ;
	int login_status;
	char *status_str;
	int sockfd;
};

struct Message {
	char *sender_adderss;
	char *receiver_adderss;
	char *message;
};

struct BraodcastMsg {
	char *sender_adderss;
	char *receiver_adderss;
	char *message;
};

struct BlockSender {
	char *blocked_by_address;
	char *user_to_be_blocked;
};

#endif
