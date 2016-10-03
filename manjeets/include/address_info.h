#ifndef ADDRESS_H_
#define ADDRESS_H_

#define SERVER_PORT 5432
#define MAX_PENDING 5
#define MAX_LINE 256

struct address_info {
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	char *server_port;
	char *host_name;	
	char *ip_address;
	std::string user_command;
	std::string message;	
	size_t ai_addrlen;
};

#endif
