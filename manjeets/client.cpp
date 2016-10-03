#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <ctype.h>
#include <list>
#include <limits.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "../include/logger.h"
#include "../include/global.h"
#include "../include/client.h"
#include "../include/server.h"
#include "../include/address_info.h"

using namespace std;
static char *client_ip;
static int client_port;
static list<struct ClientList> clientUserslist;
static list<struct Message> clientMsgList;
static int loggedin = -1;
int isloginrefreshmessage = -1;
int convert_char_to_integer(char* char_arg) {
	return atoi(char_arg);
}

// method for getting IP address of the machine using localhost. Reffered code snippet from Beej guide section 9.7 
int get_ip_by_name(char *client_name) {
    struct hostent *he_client;
    struct in_addr **addr_list_client;
    int a;
    if ((he_client = gethostbyname(client_name)) == NULL) {
        // get the host info
    	perror("error during resolving host IP address. \n");
        return 1;
    }
    addr_list_client = (struct in_addr **) he_client->h_addr_list;
    for(a = 0; addr_list_client[a] != NULL; a++) {
        //Return the first one;
    	int ip_length = strlen(inet_ntoa(*addr_list_client[a]));
    	client_ip = (char *)malloc(sizeof(char)*(ip_length));
        strncpy(client_ip , inet_ntoa(*addr_list_client[a]), ip_length);
        return 0;
    }
    return 1;
}

/*
 * code from beej guide to ensure no packets/bytes remain unsent when a client is sending message to the server
 */
int sendall(int s, char *buf, int *len) {
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;
    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    *len = total; // return number actually sent here
    return n==-1?-1:0; // return -1 on failure, 0 on success
}
/*
 * sending message from client to server based on the listening socket
 * forwards the message buffer to tge sendall() function which sends the message to server and thereafter the the destined ip address.
 */
int send_message(int listener_client, char *message_buffer, int msgType) {
	int msglen = strlen(message_buffer);
	//message_buffer[msglen] = '/0';
	int sent_bytes_status = sendall(listener_client,message_buffer, &msglen);
	if (sent_bytes_status == -1) {
		if(msgType == 0)
			cse4589_print_and_log("[SEND:ERROR]\n");
		else
			cse4589_print_and_log("[BROADCAST:ERROR]\n");
	} else {
		if(msgType == 0)
			cse4589_print_and_log("[SEND:SUCCESS]\n");
		else
			cse4589_print_and_log("[BROADCAST:SUCCESS]\n");
	}
	return 0;
}

/*
 * displays the list of clients to the user when the user enters a LIST command
 */
void showClientList() {
	int counter = 1;
	for(list<struct ClientList>::iterator it = clientUserslist.begin(); it != clientUserslist.end(); ++it ) {
		cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", counter, it->machine_name, it->user_adderss, it->port);
		counter++;
	}
}

/*
 * sorting comparator to sort the client data list based on the port number.
 * arguments: two clientlist struct type objects to be sorted.
 */
bool sort_client_list(ClientList obj1, ClientList obj2) {
	return obj1.port < obj2.port;
}

/*
This method is used to check the client input command and perform the operations based on the client.
Operations include - AUTHOR, IP, PORT, LIST, LOGOUT and EXIT
*/
void execute_client_command(char *client_command) {
	if(strncmp(client_command, "AUTHOR", 6) == 0) {
		const char* author_name = "manjeets";
		cse4589_print_and_log("[AUTHOR:SUCCESS]\n");
	cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n", author_name);
		cse4589_print_and_log("[AUTHOR:END]\n");
		fflush(stdout);
	} else if(strncmp(client_command, "IP", 2) == 0 && loggedin == 0) {
		cse4589_print_and_log("[IP:SUCCESS]\n");
		cse4589_print_and_log("IP:%s\n", client_ip);
		cse4589_print_and_log("[IP:END]\n");
		fflush(stdout);
	} else if(strncmp(client_command, "PORT", 4) == 0 && loggedin == 0) {
		cse4589_print_and_log("[PORT:SUCCESS]\n");
		cse4589_print_and_log("PORT:%d\n", client_port);
		cse4589_print_and_log("[PORT:END]\n");
		fflush(stdout);
	} else if(strncmp(client_command, "LIST", 4) == 0 && loggedin == 0) {
		cse4589_print_and_log("[LIST:SUCCESS]\n");
		showClientList();
		cse4589_print_and_log("[LIST:END]\n");
		fflush(stdout);
	} else {
		cse4589_print_and_log("[%s:ERROR]\n", client_command);
		cse4589_print_and_log("invalid command:: %s\n", client_command);
		cse4589_print_and_log("[%s:END]\n", client_command);
		fflush(stdout);
	}
}
/*
 * REFRESH command input by client on console will fetch the updated list of the users who were/are connected to the server.
 * The client can then view this updated user's list when it types the LIST command(refer showClientList function for this part)
 */
void refresh_client_list(char *senderip, int listener_client) {
	char *refresh_msg;
	int len = strlen(client_ip)+8;
	refresh_msg = (char *)malloc(sizeof (char)*(len));
	strncpy(refresh_msg, "REFRESH", 7);
	strcat(refresh_msg, " ");
	strncat(refresh_msg, senderip, strlen(senderip));
	int sent_bytes_status = sendall(listener_client,refresh_msg, &len);
	cse4589_print_and_log("[REFRESH:SUCCESS]\n");
	cse4589_print_and_log("[REFRESH:END]\n");
	fflush(stdout);
}

/*
 * this method blocks a user on request of the client sending the ip of the machine to be blocked.
 * blocked user can't send any messages to the client who blocked it.
 * arguments : client listener id and ip of the client to be blocked
 */
void block_user(int listener_client, char *to_block_ip) {
	int bytes_sent = send(listener_client,to_block_ip,strlen(to_block_ip), 0);
	cse4589_print_and_log("[BLOCK:SUCCESS]\n");
	cse4589_print_and_log("[BLOCK:END]\n");
}

/*
 * this method unblocks the previously blocked user by this client
 * arguments : client listener id and ip of the client to be blocked
 */
void unblock_user(int listener_client, char *to_block_ip) {
	int bytes_sent = send(listener_client,to_block_ip,strlen(to_block_ip), 0);
	cse4589_print_and_log("[UNBLOCK:SUCCESS]\n");
	cse4589_print_and_log("[UNBLOCK:END]\n");
}

//code to tokenize the list data and save them in the statistics list as objects with parameters machinename, ipaddress and port
//code snippet reffered from stackover link : http://stackoverflow.com/questions/10349270/c-split-a-char-array-into-different-variables
void createClientList(char *list_arr) {
	char *str1, *str2, *token, *subtoken;
	char *saveptr1, *saveptr2;
	int j;
	if(!clientUserslist.empty())
		clientUserslist.clear();
	for (j = 1, str1 = list_arr; ; j++, str1 = NULL) {
	   ClientList tempObj;
	   token = strtok_r(str1, "::", &saveptr1);
	   if (token == NULL)
		   break;

	   int i = 0;
	   for (str2 = token; ; str2 = NULL) {
		   subtoken = strtok_r(str2, "-", &saveptr2);
		   if (subtoken == NULL)
			   break;
		   if(i == 0) {
			   int n = strlen(subtoken);
			   char *nme = (char *)malloc(sizeof (char)*n);
			   //nme[n] = '\0';
			   memcpy(nme, subtoken, n);
			   tempObj.machine_name = nme;
		   } else if(i == 1) {
			   int n = strlen(subtoken);
			   char *addrs = (char *)malloc(sizeof (char)*n);
			   //addrs[n] = '\0';
			   memcpy(addrs, subtoken, n);
			   tempObj.user_adderss = addrs;
	       } else if(i == 2) {
			   int n = strlen(subtoken);
			   char *portchar = (char *)malloc(sizeof (char)*n);
			   //portchar[n] = '\0';
			   memcpy(portchar, subtoken, n);
			   int temp_portval = atoi(portchar);
			   tempObj.port = temp_portval;
		   }
		   i++;
	   }
	   clientUserslist.push_back(tempObj);
	}
	clientUserslist.sort(sort_client_list);
}

void displayLoginClientMessages(char *msgchar) {
	char *char1, *char2, *tok, *subtok;
	char *ptr1, *ptr2;
	int j;
	for (j = 1, char1 = msgchar; ; j++, char1 = NULL) {
		tok = strtok_r(char1, "::", &ptr1);
	   if (tok == NULL) {
		   break;
	   }
	   int i = 0;
	   char *sender, *recvr, *messg_dsply;
	   for (char2 = tok; ; char2 = NULL) {
		   subtok = strtok_r(char2, "-", &ptr2);
		   if (subtok == NULL)
			   break;
		   if(i == 0) {
			   sender = (char *)malloc(sizeof(char)*(strlen(subtok)));
			   memcpy(sender, subtok, strlen(subtok));
		   } else if(i == 1) {
			   recvr = (char *)malloc(sizeof(char)*(strlen(subtok)));
			   memcpy(recvr, subtok, strlen(subtok));
		   } else if(i == 2) {
			   char *portchar;
			   messg_dsply = (char *)malloc(sizeof(char)*(strlen(subtok)));
			   memcpy(messg_dsply, subtok, strlen(subtok));
		   }
		   i++;
	   }
	   cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender, recvr, messg_dsply);
	}
}

/*
 * create tokens for list data and message data when the server sends the messages and list of users to the client
 * after client LOGIN is success
 */
void tokenizeLoginDatafromServer(char* clients) {
	//get only list data and not message data
	char *str1, *token, *saveptr1;
	char *list;
	char *msgchar;
	int j;
	int count = 0;
	int len = strlen(clients);
	for (j = 1, str1 = clients; ; j++, str1 = NULL) {
		token = strtok_r(str1, "+++", &saveptr1);
		if (token == NULL)
		   break;
		if(j==1) {
			int size = strlen(token);
			list = (char *)malloc(sizeof(char)*(size));
			memcpy(list, token, size);
		} else if(j==2) {
			int msgsize = strlen(token);
			msgchar = (char *)malloc(sizeof(char)*(len));
			memcpy(msgchar, token, msgsize);
		}
		count++;
	}

	createClientList(list);
	if(count > 1)
		displayLoginClientMessages(msgchar);
}

/*
 * this function checks if the incoming data from the server is a message to be displayed to the client
 * or is it the LIST amd message data send by server on client login
 */
void displayUserMessagesfromRecv(char *data_buffer) {
	char *str1, *token, *saveptr1;
	char *origin, *msg, *destination;
	int j;
	int len = strlen(data_buffer-1);
	for (j = 1, str1 = data_buffer; ; j++, str1 = NULL) {
		token = strtok_r(str1, "-", &saveptr1);
		if (token == NULL)
		   break;

		int toklen = strlen(token);
		if(j==1) {
			origin = (char *)malloc(sizeof(char)*(toklen));
			memcpy(origin, token, toklen);
		} else if(j==2) {
			destination = (char *)malloc(sizeof(char)*toklen);
			memcpy(destination, token, toklen);
		} else if(j==3) {
			msg = (char *)malloc(sizeof(char)*(len));
			memcpy(msg, token, toklen);
		}
	}
	cse4589_print_and_log("[RECEIVE:SUCCESS]\n");
	cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", origin, destination, msg);
	cse4589_print_and_log("[RECEIVE:END]\n");
}

/*Method to start client. On starting client application, it gets the IP address of the client machine, port it is running on and redies all the structs and variables for client connection and other chat featurs.
Reffered this code from Beej guide, server.c for select() statement, with the change that it will not have the statements to accept connection, so this method will create the socket, bind it to the port we pass for listening to 
and execute stdin commands like LOGIN, SEND, LIST, AUTHOR, IP, PORT, BLOCK, UNBLOCK, BROADCAST, LOGOUT and EXIT 
Section 7.2 Beej socket programming guide
*/
int start_client(char *port_number) {
	struct address_info addr;
	addr.ai_family = AF_INET; //set address family fr IPV4
	addr.ai_socktype = SOCK_STREAM; // set socket type for IPV4
	addr.ai_protocol = IPPROTO_TCP; // set protocol to TCP
	addr.server_port = port_number; // set the port client loaded the applciation on
	fd_set read_masterclient;    // master file descriptor list
	fd_set read_fdsclient;  // temp file descriptor list for select()
	fd_set write_masterclient;
	fd_set write_fdsclient;
	int fdmax_client;        // maximum file descriptor number
	int listener_client ;     // client listening socket descriptor
	//struct sockaddr_storage remoteaddr_client; // client address
	socklen_t addrlen_client;
	char read_buffer_client[BUFFER_SIZE];    // buffer for client data
	int nbytes_client;
	int k, l, rv_client;
	int yes = 1; // for setsockopt() SO_REUSEADDR, below
	struct addrinfo hints_client, *ai_client;

	FD_ZERO(&read_masterclient);    // clear the read master and temp sets
	FD_ZERO(&read_fdsclient);
	FD_SET(0, &read_masterclient);
	FD_ZERO(&write_masterclient);   // clear the write master and temp sets
	FD_ZERO(&write_fdsclient);
	FD_SET(0, &write_masterclient);
	
	for(;;) {
		read_fdsclient = read_masterclient; // copy it  - referred from Beej guide
		// referred from Beej guide
		if (select(fdmax_client+1, &read_fdsclient, NULL, NULL, NULL) == -1) {
				return 4;
		}
		// run through the existing connections looking for data to read - referred from Beej guide
		for(k = 0; k <= fdmax_client; k++) {
			if (FD_ISSET(k, &read_fdsclient)) { // we got one!  - referred from Beej guide
				if(FD_ISSET(STDIN_FILENO, &read_fdsclient)){
					//user input command
					char *token_client;
					int count = 0;
					fgets(read_buffer_client, sizeof read_buffer_client, stdin);
					char *temp_char;
					char *client_cmd;
					char *server_ipaddress;
					char *server_port_number;
					char *msg_buffer;
					int islogincmd = -1;
					int issendcmd = -1;
					int isblockcmd = -1;
					int isunblockcmd = -1;
					int isbroadcast = -1;
					token_client = strtok(read_buffer_client, " ");
					while(token_client != NULL) {
						temp_char = (char *)malloc(sizeof(char)*(count));
						memcpy(temp_char, token_client, strlen(token_client));
						token_client = strtok(NULL, " ");
						if(count == 0) {
							client_cmd = (char *)malloc(sizeof(char)*(strlen(temp_char)));
							memcpy(client_cmd, temp_char, strlen(temp_char));
						}
						if(strncmp(client_cmd, "LOGIN", 5) == 0 &&  islogincmd < 0) {
							islogincmd = 0;
							issendcmd = -1;
							isblockcmd = -1;
							isunblockcmd = -1;
							isbroadcast = -1;
						} else if(strncmp(client_cmd, "SEND", 4) == 0 && issendcmd < 0) {
							issendcmd = 0;
							islogincmd = -1;
							isblockcmd = -1;
							isunblockcmd = -1;
							isbroadcast = -1;
						} else if(strncmp(client_cmd, "BROADCAST", 9) == 0 && issendcmd < 0) {
							isbroadcast = 0;
							issendcmd = 1;
							islogincmd = -1;
							isblockcmd = -1;
							isunblockcmd = -1;
						} else if(strncmp(client_cmd, "BLOCK", 5) == 0 && isblockcmd < 0) {
							isblockcmd = 0;
							issendcmd = -1;
							islogincmd = -1;
							isunblockcmd = -1;
							isbroadcast = -1;
						} else if(strncmp(client_cmd, "UNBLOCK", 7) == 0 && isblockcmd < 0) {
							isunblockcmd = 0;
							isblockcmd = -1;
							issendcmd = -1;
							islogincmd = -1;
							isbroadcast = -1;
						}
						//checks if the first argument of the stdin is LOGIN and then gets the IP address of the client to login to server
						if(islogincmd == 0 && count == 1) {
							server_ipaddress = (char *)malloc(sizeof(char)*(strlen(temp_char)));
							memcpy(server_ipaddress, temp_char, strlen(temp_char));
						} else if(islogincmd == 0 && count == 2) {
							server_port_number = (char *)malloc(sizeof(char)*(strlen(temp_char)));
							memcpy(server_port_number, temp_char, 4);
						}
						/*checks if the first argument of the stdin is SEND and
						* then gets the IP address of the client and the message before sending it to the server
						*/
						if(issendcmd == 0 && count == 1) {
							msg_buffer = (char *)malloc(sizeof(char)*256);
							strncpy(msg_buffer, "S", 1);
							strcat(msg_buffer, " ");
							strncat(msg_buffer, client_ip, strlen(client_ip));
							strcat(msg_buffer, " ");
							strncat(msg_buffer, temp_char, strlen(temp_char));
						} else if(issendcmd == 0 && count > 1) {
							strcat(msg_buffer, " ");
							strncat(msg_buffer, temp_char, strlen(temp_char));
						} else if(isbroadcast == 0 && count == 1) {
							msg_buffer = (char *)malloc(sizeof(char)*256);
							strncpy(msg_buffer, "BR", 2);
							strcat(msg_buffer, " ");
							strncat(msg_buffer, temp_char, strlen(temp_char));
						} else if(isbroadcast == 0 && count > 1) {
							strcat(msg_buffer, " ");
							strncat(msg_buffer, temp_char, strlen(temp_char));
						} else if(isblockcmd == 0 && count == 1) {
							server_ipaddress = (char *)malloc(sizeof(char)*(strlen(temp_char)+2));
							strncpy(server_ipaddress, "BL", 2);
							strcat(server_ipaddress, " ");
							strncat(server_ipaddress, temp_char, strlen(temp_char));
						} else if(isunblockcmd == 0 && count == 1) {
							server_ipaddress = (char *)malloc(sizeof(char)*(strlen(temp_char)+2));
							strncpy(server_ipaddress, "UB", 2);
							strcat(server_ipaddress, " ");
							strncat(server_ipaddress, temp_char, strlen(temp_char));
						}
						count++;
					}
					free(temp_char);
					free(token_client);
					//LOGOUT Command started
					if(count == 1 && strncmp(client_cmd, "LOGOUT", 6) == 0) {
						cse4589_print_and_log("[LOGOUT:SUCCESS]\n");
						close(listener_client); // closing client listening socket while logging out of server! - referred from Beej guide*/
						FD_CLR(listener_client, &read_masterclient); // remove from master set
						FD_CLR(listener_client, &write_masterclient);
						fflush(stdout);
						cse4589_print_and_log("[LOGOUT:END]\n");
					} else if(count == 1 && strncmp(client_cmd, "EXIT", 4) == 0) { //exit command started here
						cse4589_print_and_log("[EXIT:SUCCESS]\n");
						close(listener_client); // bye! - referred from Beej guide
						FD_CLR(listener_client, &read_masterclient); // remove from master set
						FD_CLR(listener_client, &write_masterclient);
						fflush(stdout); //fluhsing out std data before exit
						cse4589_print_and_log("[EXIT:END]\n");
						exit(0) ;
					} else if(count == 1 && strncmp(client_cmd, "REFRESH", 7) == 0 && loggedin == 0) {
						refresh_client_list(client_ip, listener_client);
						isloginrefreshmessage = 0;
					} else if(count == 1) {
						//call for function which displays AUTHOR, IP, PORT command output for the user
						execute_client_command(client_cmd);
					}
					//sending client message to intended recipient
					if(strncmp(client_cmd, "SEND", 4) == 0 && loggedin == 0) {
						int msgln = strlen(msg_buffer)+1;
						msg_buffer[msgln] = '\0';
						send_message(listener_client, msg_buffer, 0);
						fflush(stdout);
					}
					//boradcasting message to all users
					if(strncmp(client_cmd, "BROADCAST", 9) == 0 && loggedin == 0) {
						int msgln = strlen(msg_buffer)+1;
						msg_buffer[msgln] = '\0';
						send_message(listener_client, msg_buffer, 1);
						fflush(stdout);
					}
					//blocking machine IP provided by client
					if(strncmp(client_cmd, "BLOCK", 5) == 0 && loggedin == 0) {
						block_user(listener_client, server_ipaddress);
						fflush(stdout);
					}
					//unblock the machine IP client had blocked previously
					if(strncmp(client_cmd, "UNBLOCK", 7) == 0 && loggedin == 0) {
						unblock_user(listener_client, server_ipaddress);
						fflush(stdout);
					}
					//client logging onto the server port server is listening for incoming connections
					if(strncmp(client_cmd, "LOGIN", 5) == 0 && count == 3) {

						//get the machine hostname - referred from Beej guide
						char client_name[BUFFER_SIZE];
						memset(&client_name, 0, sizeof(client_name));
						gethostname(client_name, sizeof(client_name));
						addr.host_name = client_name;
						//get the machine IP address using hostname - referred from Beej guide
						get_ip_by_name(client_name);
						//get the machine port and convert from char to integer using atoi()
						client_port = convert_char_to_integer(port_number);

						memset(&hints_client, 0, sizeof hints_client);
						hints_client.ai_family = AF_INET;
						hints_client.ai_protocol = IPPROTO_TCP;
						hints_client.ai_socktype = SOCK_STREAM;
						hints_client.ai_flags = AI_PASSIVE;
						char aliasname[128];

						//Get the IP address of the host for binding to socket - referred from Beej guide
						rv_client=getaddrinfo(server_ipaddress, server_port_number, &hints_client, &ai_client);
						if(rv_client != 0)
							perror("selectserver: getaddrinfo error\n");

						//Creating a socket - referred from Beej guide
						listener_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
						if (listener_client < 0) {
							perror("listen");
							fflush(stdout);
							return 1;
						}

						/* lose the pesky "address already in use" error message - picks up the socket if it is stiil
						 *  hanging around in the kernel after client was logged out, since a socket ca typically be in the kernel
						 * upto a minute after the socket was closed.
						 * Referred from Beej guide
						*/
						setsockopt(listener_client, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

						/*referred to the stack over flow link
						 * http://stackoverflow.com/questions/6468113/binding-a-port-number-to-a-tcp-socket-outgoin-to-send-packets; for binding client to socket before establishing socket question
						 * need to reset memset before binding client to a socket desired by the client
						*/
						struct sockaddr_in client;
						memset(&client, 0, sizeof(client));
						client.sin_family = AF_INET;
						client.sin_port = htons(client_port);

						if (bind(listener_client, (const struct sockaddr *)&client, sizeof(client)) < 0) {
							perror("selectserver: failed to bind\n");
							fflush(stdout);
							return 2;
						}

						//Trying to connect to server socket - referred from Beej guide
						if(connect(listener_client, ai_client->ai_addr, ai_client->ai_addrlen) == -1) {
							perror("selectserver: failed to connect\n");
							close(listener_client);
							return -1;
						} else {
							if (listener_client >= fdmax_client)
								fdmax_client += listener_client;
							int temp = listener_client;
							FD_SET(listener_client, &read_masterclient); // add to master set
							cse4589_print_and_log("[LOGIN:SUCCESS]\n");
							cse4589_print_and_log("[LOGIN:END]\n");
							loggedin = 0;
							isloginrefreshmessage = 0;
							fflush(stdout);
						}
					}
				} else if(k == listener_client) {
					// handle data from a client - referred from Beej guide
					memset(read_buffer_client, 0, sizeof(read_buffer_client));
					nbytes_client = recv(k, read_buffer_client, sizeof read_buffer_client, 0);
					if (nbytes_client <= 0) {
						close(listener_client); // bye! - referred from Beej guide
						FD_CLR(listener_client, &read_masterclient); // remove from master set
						FD_CLR(listener_client, &write_masterclient);
						fflush(stdout); //fluhsing out std data before exit
					} else {
						if(isloginrefreshmessage == 0) {
							tokenizeLoginDatafromServer(read_buffer_client);
							isloginrefreshmessage = -1;
						} else {
							displayUserMessagesfromRecv(read_buffer_client);
						}
					}
				}
			}
		}
	}
	return 0;
}
