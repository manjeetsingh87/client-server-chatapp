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
#include "../include/server.h"
#include "../include/address_info.h"

using namespace std;

static char ipstr[INET_ADDRSTRLEN];
static int port;
static char hostname[256];
static list<struct Statistics> users_stats_list;
static list<struct Message> messagesList;
static list<struct BraodcastMsg> broadcastLst;
static list<struct BlockSender> blockedList;
char *client_msg;

//code to convert any char to int using atio function.
int convert_char_int(char* arg) {
	return atoi(arg);
}

// get sockaddr, IPv4: code from beej guide
void *get_in_addr(struct sockaddr *sa) {
	return &(((struct sockaddr_in*) sa)->sin_addr);
}

/*
 * Gets the IP address of the machine.
 * Referred from beej guide
 */
void findAddress(struct addrinfo *hints, struct addrinfo *ai) {
	void *addr;
	// get the pointer to the address itself,
	struct sockaddr_in *ipv4 = (struct sockaddr_in *) ai->ai_addr;
	addr = &(ipv4->sin_addr);
	//convert the IP:
	inet_ntop(ai->ai_family, addr, ipstr, INET_ADDRSTRLEN);
}

/*
 * check if the user to whom the message has to be sent is blocked or not before sending the message
 */
int findBlockedUser(const char *blocked_byaddr, char *user_ip_name) {
	int isblocked = -1;
	if (!blockedList.empty()) {
		for (list<struct BlockSender>::iterator it = blockedList.begin();
				it != blockedList.end(); ++it) {
			if (strncmp(it->blocked_by_address, blocked_byaddr,
					strlen(blocked_byaddr)) == 0
					&& strncmp(it->user_to_be_blocked, user_ip_name,
							strlen(user_ip_name)) == 0) {
				isblocked = 0;
				break;
			}
		}
	}
	return isblocked;
}

/*
 * check if the client is logged out and no more connected to the server on the socket they were listening on.
 */
int isClientLoggedOut(int fd_num) {
	int isLoggedOut = 0;
	if (!users_stats_list.empty()) {
		for (list<struct Statistics>::iterator itr = users_stats_list.begin();
				itr != users_stats_list.end(); ++itr) {
			if (itr->sockfd == fd_num) {
				isLoggedOut = itr->login_status;
				break;
			}
		}
	}
	return isLoggedOut;
}

/*
 * find the number of unread messages for a client who logged into the server
 * remove this later. unnecessary function.
 */
int isUnreadMessage(char *receiver_addr) {
	int has_messages = 0;
	if (!messagesList.empty()) {
		for (list<Message>::iterator msgitr = messagesList.begin();
				msgitr != messagesList.end(); ++msgitr) {
			if (strncmp(msgitr->receiver_adderss, receiver_addr,
					strlen(receiver_addr)) == 0)
				has_messages++;
		}
	}
	return has_messages;
}

/*
 * sends the entire message to the client ensuring no partial sends happen. In case of a partial send, throws an error message
 * Referred from Beej guide. Section - how to handle partial send using sendall.
 */
int sendmessage(int s, char *buf, int *len) {
	int total = 0;        // how many bytes we've sent
	int bytesleft = *len; // how many we have left to send
	int n;
	while (total < *len) {
		n = send(s, buf + total, bytesleft, 0);
		if (n == -1) {
			break;
		}
		total += n;
		bytesleft -= n;
	}
	*len = total; // return number actually sent here
	return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}

/*
 * gets the IP address of the requesting machine from the StatisticsList based on the client socket fd value
 */
char * getIPbySockFd(int fd) {
	char *address;
	for (list<Statistics>::iterator it = users_stats_list.begin();
			it != users_stats_list.end(); ++it) {
		if (fd == it->sockfd) {
			address = (char *) it->user_adderss;
			break;
		}
	}
	return address;
}

/*
 * gets the socket fd of the requesting machine from the StatisticsList based on the client IP address
 */
int getFDbyIP(const char* ip_addrs) {
	for (list<Statistics>::iterator it = users_stats_list.begin(); it != users_stats_list.end(); ++it) {
		if (strncmp(it->user_adderss, ip_addrs, strlen(ip_addrs))
				== 0) {
			return it->sockfd;
			break;
		}
	}
	return -1;
}

/* sends the list of all the clients who are/were connected to the server and have not exited the application
 * to client when the client login to the server is success or when the client does a REFRESH
*/
void sendListToClient(const char* clientname) {
	int listen_fd = getFDbyIP(clientname);
	if(listen_fd > 2) {
		list<struct Statistics> loginDataList;
		if(!loginDataList.empty())
			loginDataList.clear();

		for (list<Statistics>::iterator itr = users_stats_list.begin(); itr != users_stats_list.end(); ++itr) {
			if (itr->login_status == 0) {
				Statistics obj;
				obj.machine_name = itr->machine_name;
				obj.user_adderss = itr->user_adderss;
				obj.port = itr->port;
				loginDataList.push_back(obj);
			}
		}

		char clientLst[256];
		memset(clientLst, '\0', 255);
		int first = -1;
		for (list<Statistics>::iterator itr = loginDataList.begin(); itr != loginDataList.end(); ++itr) {
			if (first == -1) {
				strncpy(clientLst, itr->machine_name, strlen(itr->machine_name));
				strcat(clientLst, "-");
				strncat(clientLst, itr->user_adderss, strlen(itr->user_adderss));
				strcat(clientLst, "-");
				stringstream sst;
				sst << itr->port;
				strncat(clientLst, sst.str().c_str(), strlen(sst.str().c_str()));
				strcat(clientLst, "-");
				first = 0;
			} else {
				strcat(clientLst, "::");
				strncat(clientLst, itr->machine_name, strlen(itr->machine_name));
				strcat(clientLst, "-");
				strncat(clientLst, itr->user_adderss, strlen(itr->user_adderss));
				strcat(clientLst, "-");
				stringstream sst;
				sst << itr->port;
				strncat(clientLst, sst.str().c_str(), strlen(sst.str().c_str()));
				strcat(clientLst, "-");
			}
		}
		if(messagesList.empty() && broadcastLst.empty()) {
			int buffsize = strlen(clientLst);
			int sent_bytes_status = sendmessage(listen_fd, clientLst, &buffsize);
		} else {
			/*
			 * send the message from the message list to the client when the client logs onto the server.
			 * iterates over the message list finding all message pending for delivery to the client and sends them as a char array
			 * to the client
			*/
			strncat(clientLst, "+++", 2);
			if(!messagesList.empty()) {
				int i = 0;
				for(list<Message>::iterator msgItr = messagesList.begin(); msgItr != messagesList.end(); ++msgItr) {
					if(strncmp(clientname, msgItr->receiver_adderss, strlen(clientname)) == 0) {
						if(i==0) {
							strncat(clientLst, msgItr->sender_adderss, strlen(msgItr->sender_adderss));
							strcat(clientLst, "-");
							strncat(clientLst, msgItr->receiver_adderss, strlen(clientname));
							strcat(clientLst, "-");
							strncat(clientLst, msgItr->message, strlen(msgItr->message));
							strcat(clientLst, "-");
						} else {
							strcat(clientLst, "::");
							strncat(clientLst, msgItr->sender_adderss, strlen(msgItr->sender_adderss));
							strcat(clientLst, "-");
							strncat(clientLst, msgItr->receiver_adderss, strlen(clientname));
							strcat(clientLst, "-");
							strncat(clientLst, msgItr->message, strlen(msgItr->message));
							strcat(clientLst, "-");
						}
					}
					i++;
				}
			}
			if(!broadcastLst.empty()) {
				for(list<BraodcastMsg>::iterator brdItr = broadcastLst.begin(); brdItr != broadcastLst.end(); ++brdItr) {
					if(strncmp(clientname, brdItr->receiver_adderss, strlen(clientname)) == 0) {
						strcat(clientLst, "::");
						strncat(clientLst, brdItr->sender_adderss, strlen(brdItr->sender_adderss));
						strcat(clientLst, "-");
						strncat(clientLst,"255.255.255.255", strlen("255.255.255.255"));
						strcat(clientLst, "-");
						strncat(clientLst, brdItr->message, strlen(brdItr->message));
						strcat(clientLst, "-");
					}
				}
			}
			int buffsize = strlen(clientLst);
			clientLst[buffsize] = '\0';
			int sent_bytes_status = sendmessage(listen_fd, clientLst, &buffsize);
		}
	}
}

/* sends the list of all the clients who are/were connected to the server and have not exited the application
 * to client when the client login to the server is success or when the client does a REFRESH
 */
void sendRefreshListToClient(char *sendr_addr) {
	list<struct Statistics> loginDataList;
	int fd = getFDbyIP(sendr_addr);
	if(!loginDataList.empty())
		loginDataList.clear();

	for (list<Statistics>::iterator itr = users_stats_list.begin(); itr != users_stats_list.end(); ++itr) {
		if (itr->login_status == 0) {
			Statistics obj;
			obj.machine_name = itr->machine_name;
			obj.user_adderss = itr->user_adderss;
			obj.port = itr->port;
			loginDataList.push_back(obj);
		}
	}
	char clientRefreshLst[256];
	memset(clientRefreshLst, '\0', 255);
	int first = -1;
	for (list<Statistics>::iterator itr = loginDataList.begin(); itr != loginDataList.end(); ++itr) {
		if (first == -1) {
			strncpy(clientRefreshLst, itr->machine_name, strlen(itr->machine_name));
			strcat(clientRefreshLst, "-");
			strncat(clientRefreshLst, itr->user_adderss, strlen(itr->user_adderss));
			strcat(clientRefreshLst, "-");
			stringstream sst;
			sst << itr->port;
			strncat(clientRefreshLst, sst.str().c_str(), strlen(sst.str().c_str()));
			strcat(clientRefreshLst, "-");
			first = 0;
		} else {
			strcat(clientRefreshLst, "::");
			strncat(clientRefreshLst, itr->machine_name, strlen(itr->machine_name));
			strcat(clientRefreshLst, "-");
			strncat(clientRefreshLst, itr->user_adderss, strlen(itr->user_adderss));
			strcat(clientRefreshLst, "-");
			stringstream sst;
			sst << itr->port;
			strncat(clientRefreshLst, sst.str().c_str(), strlen(sst.str().c_str()));
			strcat(clientRefreshLst, "-");
		}
	}
	int buffsize = strlen(clientRefreshLst);
	clientRefreshLst[buffsize] = '\0';
	int sent_bytes_status = sendmessage(fd, clientRefreshLst, &buffsize);
}

/*
 * this method adds the ip of the machine the client as requested to block, to the blockedList
 * maintains the list of all blocked ip address in a list of BlockSender struct which maps the blocked_by_address and user_to_be_blocked
 */
void block_sender(char *sender_ipadr, char *user_toblock_ip) {
	struct BlockSender blockLstObj;
	char *sender_addr;
	int len = strlen(sender_ipadr);
	sender_addr = (char *)malloc(sizeof (char)*(len)+1);
	sender_addr[len] = '\n';
	memcpy(sender_addr, sender_ipadr, len);
	blockLstObj.blocked_by_address = sender_addr;

	char *rcvr_blockaddr;
	len = strlen(user_toblock_ip);
	rcvr_blockaddr = (char *)malloc(sizeof (char)*(len)+1);
	rcvr_blockaddr[len] = '\n';
	memcpy(rcvr_blockaddr, user_toblock_ip, len);
	blockLstObj.user_to_be_blocked = rcvr_blockaddr;

	if (blockedList.empty()) {
		blockedList.push_back(blockLstObj);
	} else if (!blockedList.empty()
			&& findBlockedUser(sender_addr, user_toblock_ip) == -1) {
		blockedList.push_back(blockLstObj);
	}
}

/*
 * this method removes the ip of the machine the client as requested to unblock.
 * removes the object which matches the IP address of the client requesting i.e blocked_by_address and client to be unblocked i.e
 * user_to_be_blocked from the blockedList
 */
void unblock_sender(char *unblock_requester, char *user_to_unblock) {
	int isblock = findBlockedUser(unblock_requester, user_to_unblock);
	for (list<BlockSender>::iterator item_itr = blockedList.begin(); item_itr != blockedList.end();) {
		if (isblock == 0
				&& strncmp(item_itr->blocked_by_address, unblock_requester, strlen(unblock_requester)) == 0
				&& strncmp(item_itr->user_to_be_blocked, user_to_unblock, strlen(user_to_unblock)) == 0) {

			blockedList.erase(item_itr);
			break;
		} else {
			++item_itr;
		}
	}
}

/*
 * used to display list of clients to the server when the server types the command LIST
 * output data fields: row-number, machine_name, user_address and port
 */
void displayClientList() {
	int srNo = 1;
	for (list<struct Statistics>::iterator it = users_stats_list.begin(); it != users_stats_list.end(); ++it) {
		cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", srNo, it->machine_name,
				it->user_adderss, it->port);
		srNo++;
	}
}

/*
 * used to display statistics of clients to the server when the server types the command STATISTICS
 * output data fields: row-number, machine_name, no of messgaes sent by the machine,
 * no of messgaes received by the machine and online/offline status.
 */
void displayStatistics() {
	int srNo = 1;
	for (list<struct Statistics>::iterator it = users_stats_list.begin(); it != users_stats_list.end(); ++it) {
		cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", srNo, it->machine_name, it->messages_sent, it->messages_received, it->status_str);
		srNo++;
	}
}

/*
 * custom comparator to sort users_stats_list based on port number
 */
bool sort_list_by_port(Statistics  obj1, Statistics  obj2) {
	return obj1.port < obj2.port;
}

/*
 * when a client logout is encountered, this function will update the status of the client as offline until the client
 * login occurs again
 */
void logout_client(int i) {
	if (!users_stats_list.empty()) {
		list<Statistics>::iterator statObj;
		for (statObj = users_stats_list.begin(); statObj != users_stats_list.end(); ++statObj) {
			Statistics &temp = *statObj;
			if (i == temp.sockfd) {
				temp.login_status = -1;
				temp.status_str = "offline";
			}
		}
	}
}

/*
 * this function is called when a server exits the application.
 * the server will remove all data before exiting the application.
 */
void exit_server() {
	users_stats_list.clear();
	blockedList.clear();
	messagesList.clear();
	broadcastLst.clear();
}

/*
 * find the FD of the logged in client in the statistics list using address, port and socket number client and server are listening on.
 */
int searchStatsListforFD(int fd, char* addrss, int port) {
	list<Statistics>::iterator statObj;
	for (statObj = users_stats_list.begin(); statObj != users_stats_list.end(); ++statObj) {
		Statistics &temp = *statObj;
		if (fd == temp.sockfd) {
			return 0;
		}
	}
	return -1;
}

/*
 * when a user login occurs, the server updates the user_stats_list, adding the client info to the list
 * and updating the client status as online
 */
void addtoStatisticslist(char *machineName, const char* clientName, int fd_num, int portNum) {
	int ret = searchStatsListforFD(fd_num, (char *) clientName, portNum);
	if(users_stats_list.empty() || ret == -1) {
		Statistics current;
		int n = strlen(machineName);
		char *name = (char *)malloc(sizeof(char)*n);
		name[n] = '\0';
		memcpy(name, machineName, n);
		current.machine_name = name;
		n = strlen(clientName);
		char *ipname = (char *)malloc(sizeof(char)*n);
		ipname[n] = '\0';
		memcpy(ipname, clientName, n);
		current.user_adderss = ipname;
		current.port = portNum;
		current.sockfd = fd_num;
		current.login_status = 0;
		current.status_str = "online";
		current.messages_sent = 0;
		current.messages_received = 0;
		users_stats_list.push_back(current);
	} else if (ret == 0 && isClientLoggedOut(fd_num) == -1) {
		for (list<Statistics>::iterator statObj = users_stats_list.begin(); statObj != users_stats_list.end(); ++statObj) {
			Statistics &temp = *statObj;
			if (fd_num == temp.sockfd) {
				if (temp.login_status == -1) {
					temp.login_status = 0;
					temp.status_str = "online";
					break;
				}
			}
		}
	}
	users_stats_list.sort(sort_list_by_port);
}

/*
 * adds a received message for a client to the messagelist if the client who this message should be delivered to is offline.
 */
void addMessagetoMsgList(const char *ip_sender, char *ip_receiver, char *rcvd_message) {
	struct Message new_message_tolist;
	int n = strlen(ip_sender);
	char *sender= (char *)malloc(sizeof(char)*n);
	memcpy(sender, ip_sender, n);
	new_message_tolist.sender_adderss = (char *)sender;

	n = strlen(ip_receiver);
	char *iprecvr = (char *)malloc(sizeof(char)*n);
	memcpy(iprecvr, ip_receiver, n);
	new_message_tolist.receiver_adderss = iprecvr;

	n = strlen(rcvd_message);
	char *msg_content = (char *)malloc(sizeof(char)*n);
	memcpy(msg_content, rcvd_message, n);
	new_message_tolist.message = msg_content;

	messagesList.push_back(new_message_tolist);
}

/*
 * adds a received message for a client to the messagelist if the client who this message should be delivered to is offline.
 */
void addMessagetoBroadcastList(const char *ip_sender, char *recvrip, char *rcvd_message) {
	struct BraodcastMsg broadcastObj;
	int n = strlen(ip_sender);
	char *sender= (char *)malloc(sizeof(char)*n);
	memcpy(sender, ip_sender, n);
	broadcastObj.sender_adderss = (char *)sender;

	n = strlen(recvrip);
	char *iprecvr = (char *)malloc(sizeof(char)*n);
	memcpy(iprecvr, recvrip, n);
	broadcastObj.receiver_adderss = iprecvr;

	n = strlen(rcvd_message);
	char *msg_data = (char *)malloc(sizeof(char)*n);
	memcpy(msg_data, rcvd_message, n);
	broadcastObj.message = msg_data;

	broadcastLst.push_back(broadcastObj);
}

/*
 * this function updates the count of the sender in the statistics list.
 */
void updateSenderCount(const char *sender) {
	list<Statistics>::iterator itr;
	for (itr = users_stats_list.begin(); itr != users_stats_list.end(); ++itr) {
		Statistics &temp = *itr;
		if (strncmp(temp.user_adderss, sender, strlen(temp.user_adderss)) == 0) {
			int count = temp.messages_sent ;
			temp.messages_sent = count+1;
		}
	}
}

/*
 * this function updates the count of the receiver in the statistics list.
 */
void updateReceiverCount(const char *receiver) {
	list<Statistics>::iterator itr;
	for (itr = users_stats_list.begin(); itr != users_stats_list.end(); ++itr) {
		Statistics &temp = *itr;
		if (strncmp(temp.user_adderss, receiver, strlen(temp.user_adderss)) == 0) {
			int count = temp.messages_received;
			temp.messages_received = count+1;
		}
	}
}

/*
 * will send message to all users who are online.
 * adds the received broadcast message for a client to the message list if the client to whom
 * this broad cast should be delivered to is offline.
 */
void broadcastMessage(char *ip_sender, char *rcvd_message) {
	for (list<Statistics>::iterator itr = users_stats_list.begin(); itr != users_stats_list.end(); ++itr) {
		if (strncmp(ip_sender, itr->user_adderss, strlen(ip_sender)) != 0) {
			char *addrs_rcvr = (char *) itr->user_adderss;
			if (findBlockedUser(ip_sender, addrs_rcvr) < 0) {
				int recvrfd = getFDbyIP(addrs_rcvr);
				if (isClientLoggedOut(recvrfd) == 0) {
					char *msg_for_recvr;
					int len = strlen(rcvd_message) + strlen(addrs_rcvr) + strlen(ip_sender)+1;
					msg_for_recvr = (char *) malloc(sizeof(char) * (len));
					msg_for_recvr[len] = '\0';
					strncpy(msg_for_recvr, ip_sender, strlen(ip_sender));
					strcat(msg_for_recvr, "-");
					strncat(msg_for_recvr, addrs_rcvr, strlen(addrs_rcvr));
					strcat(msg_for_recvr, "-");
					strncat(msg_for_recvr, rcvd_message, strlen(rcvd_message));
					strcat(msg_for_recvr, "-");
					int buffsize = strlen(msg_for_recvr);
					int sent_bytes_to_rcvr = sendmessage(recvrfd, msg_for_recvr, &buffsize);
				} else {
					addMessagetoBroadcastList(ip_sender, addrs_rcvr, rcvd_message);
				}

				updateReceiverCount(addrs_rcvr);
			}
		}
	}
	updateSenderCount(ip_sender);
	cse4589_print_and_log("[RELAYED:SUCCESS]\n");
	cse4589_print_and_log("msg from:%s, to:255.255.255.255\n[msg]:%s", ip_sender, rcvd_message);
	cse4589_print_and_log("[RELAYED:END]\n");
}

void sendMessageToRecvr(char *sender_ip, char *recevr_ip, char *message) {
	if (findBlockedUser(sender_ip, recevr_ip) < 0) {
		int recvrfd = getFDbyIP(recevr_ip);
		if (isClientLoggedOut(recvrfd) == 0) {
			char *msg_for_recvr;
			int len = strlen(message) + strlen(recevr_ip) + strlen((char *) sender_ip);
			msg_for_recvr = (char *) malloc(sizeof(char) * (len));
			strcpy(msg_for_recvr, (char *) sender_ip);
			strcat(msg_for_recvr, "-");
			strncat(msg_for_recvr, recevr_ip, strlen(recevr_ip));
			strcat(msg_for_recvr, "-");
			strncat(msg_for_recvr, message, strlen(message));
			strcat(msg_for_recvr, "-\0");
			int buffsize = strlen(msg_for_recvr);
			int sent_bytes_to_rcvr = sendmessage(recvrfd, msg_for_recvr, &buffsize);

			cse4589_print_and_log("[RELAYED:SUCCESS]\n");
			cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s", sender_ip, recevr_ip, message);
			cse4589_print_and_log("[RELAYED:END]\n");
		} else {
			addMessagetoMsgList(sender_ip, recevr_ip, message);
			cse4589_print_and_log("[RELAYED:SUCCESS]\n");
			cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s", sender_ip, recevr_ip, message);
			cse4589_print_and_log("[RELAYED:END]\n");
		}
		updateSenderCount(sender_ip);
		updateReceiverCount(recevr_ip);
	}
}

/*
 * executes server commands like AUTHOR, IP, PORT, LIST and STATISTICS which the server requested for via the stdin
 */
void execute_commands(char *command, int sock_listener) {
	if (strncmp(command, "AUTHOR", 6) == 0) {
		const char* name = "manjeets";
		cse4589_print_and_log("[AUTHOR:SUCCESS]\n");
		cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n", name);
		cse4589_print_and_log("[AUTHOR:END]\n");
		fflush (stdout);
	} else if (strncmp(command, "IP", 2) == 0) {
		if (ipstr == NULL || strlen(ipstr) <= 0) {
			cse4589_print_and_log("[IP:ERROR]\n");
			cse4589_print_and_log("[IP:END]\n");
		} else {
			cse4589_print_and_log("[IP:SUCCESS]\n");
			cse4589_print_and_log("IP:%s\n", ipstr);
			cse4589_print_and_log("[IP:END]\n");
		}
		fflush (stdout);
	} else if (strncmp(command, "PORT", 4) == 0) {
		cse4589_print_and_log("[PORT:SUCCESS]\n");
		cse4589_print_and_log("PORT:%d\n", port);
		cse4589_print_and_log("[PORT:END]\n");
		fflush (stdout);
	} else if (strncmp(command, "LIST", 4) == 0) {
		cse4589_print_and_log("[LIST:SUCCESS]\n");
		displayClientList();
		cse4589_print_and_log("[LIST:END]\n");
		fflush (stdout);
	} else if (strncmp(command, "STATISTICS", 10) == 0) {
		cse4589_print_and_log("[STATISTICS:SUCCESS]\n");
		displayStatistics();
		cse4589_print_and_log("[STATISTICS:END]\n");
		fflush (stdout);
	} else if (strncmp(command, "EXIT", 4) == 0) {
		cse4589_print_and_log("[EXIT:SUCCESS]\n");
		exit_server();
		close(sock_listener);
		exit(0);
		cse4589_print_and_log("[EXIT:END]\n");
		fflush (stdout);
	} else {
		cse4589_print_and_log("[%s:ERROR]\n", command);
		cse4589_print_and_log("[%s:END]\n", command);
		fflush (stdout);
	}
}

/*
 * get ip address of the machine by name and sets it to the char *ipstr variable
 * Referred from Beej guide
 */
int get_ip_by_hostname(char* host_address) {
	struct hostent *he;
	struct in_addr **addr_list;
	int i;
	if ((he = gethostbyname(host_address)) == NULL) {
		// get the host info
		cse4589_print_and_log("error during resolving host IP address. \n");
		return 1;
	}
	addr_list = (struct in_addr **) he->h_addr_list;
	for (i = 0; addr_list[i] != NULL; i++) {
		//Return the first one;
		strcpy(ipstr, inet_ntoa(*addr_list[i]));
		return 0;
	}
	return 1;
}

/*
 * helper function to create tokens of the incoming buffer from the client
 */
void create_command_token(char *buff) {
	char *tokens;
	char *temp_msg;
	char *task_name;
	char *sndr_addr;
	char *recvr_addr;
	char *msg_arr;
	int incr = 0;
	int issendmsg = -1;
	int isbroadcast = -1;
	int isblockunblock = -1;
	int isrefresh = -1;
	tokens = strtok(buff, " ");
	while (tokens != NULL) {
		temp_msg = (char *) malloc(sizeof(char) * (strlen(tokens)+1));
		temp_msg[strlen(tokens)] = '\0';
		memcpy(temp_msg, tokens, strlen(tokens));
		tokens = strtok(NULL, " ");
		if (incr == 0) {
			task_name = (char *) malloc(sizeof(char) * (strlen(temp_msg)));
			memcpy(task_name, temp_msg, strlen(temp_msg));
		}
		if (strncmp(task_name, "BR", 2) == 0 && isblockunblock < 0) {
			isbroadcast = 0;
			issendmsg = -1;
			isblockunblock = -1;
			isrefresh = -1;
		} else if (strncmp(task_name, "UB", 2) == 0 && isblockunblock < 0) {
			isblockunblock = 0;
			issendmsg = -1;
			isbroadcast = -1;
			isrefresh = -1;
		} else if (strncmp(task_name, "S", 1) == 0 && issendmsg < 0) {
			issendmsg = 0;
			isblockunblock = -1;
			isbroadcast = -1;
			isrefresh = -1;
		} else if (strncmp(task_name, "BL", 2) == 0 && issendmsg < 0) {
			isblockunblock = 0;
			issendmsg = -1;
			isbroadcast = -1;
			isrefresh = -1;
		} else if (strncmp(task_name, "REFRESH", 7) == 0 && issendmsg < 0) {
			isrefresh = 0;
			isblockunblock = -1;
			issendmsg = -1;
			isbroadcast = -1;
		}

		if (isblockunblock == 0 && incr == 1) {
			sndr_addr = (char *) malloc(sizeof(char) * (strlen(temp_msg)+1));
			sndr_addr[strlen(temp_msg)] = '\0';
			memcpy(sndr_addr, temp_msg, strlen(temp_msg));
		} else if (isblockunblock == 0 && incr == 2) {
			recvr_addr = (char *) malloc(sizeof(char) * (strlen(temp_msg)+1));
			recvr_addr[strlen(temp_msg)] = '\0';
			memcpy(recvr_addr, temp_msg, strlen(temp_msg));
		}
		if (issendmsg == 0 && incr == 1) {
			sndr_addr = (char *) malloc(sizeof(char) * (strlen(temp_msg)));
			memcpy(sndr_addr, temp_msg, strlen(temp_msg));
		} else if (issendmsg == 0 && incr == 2) {
			recvr_addr = (char *) malloc(sizeof(char) * (strlen(temp_msg)));
			memcpy(recvr_addr, temp_msg, strlen(temp_msg));
		} else if (issendmsg == 0 && incr == 3) {
			msg_arr = (char *) malloc(sizeof(char) * (strlen(temp_msg)+1));
			msg_arr[strlen(temp_msg)] = '\0';
			memcpy(msg_arr, temp_msg, strlen(temp_msg));
		} else if (issendmsg == 0 && incr > 3) {
			strcat(msg_arr, " ");
			strncat(msg_arr, temp_msg, strlen(temp_msg));
		}

		if (isbroadcast == 0 && incr == 1) {
			sndr_addr = (char *) malloc(sizeof(char) * (strlen(temp_msg)));
			memcpy(sndr_addr, temp_msg, strlen(temp_msg));
		} else if (isbroadcast == 0 && incr == 2) {
			msg_arr = (char *) malloc(sizeof(char) * (strlen(temp_msg)+1));
			msg_arr[strlen(temp_msg)] = '\0';
			memcpy(msg_arr, temp_msg, strlen(temp_msg));
		} else if (isbroadcast == 0 && incr > 1) {
			strcat(msg_arr, " ");
			strncat(msg_arr, temp_msg, strlen(temp_msg));
		}

		if(isrefresh == 0 && incr == 1) {
			sndr_addr = (char *) malloc(sizeof(char) * (strlen(temp_msg)+1));
			sndr_addr[strlen(temp_msg)] = '\0';
			memcpy(sndr_addr, temp_msg, strlen(temp_msg));
		}
		incr++;
	}
	if (strncmp(task_name, "BL", 2) == 0)
		block_sender(sndr_addr, recvr_addr);
	else if (strncmp(task_name, "UB", 2) == 0)
		unblock_sender(sndr_addr, recvr_addr);
	else if (strncmp(task_name, "S", 1) == 0) {
		sendMessageToRecvr(sndr_addr, recvr_addr, msg_arr);
	} else if (strncmp(task_name, "BR", 2) == 0)
		broadcastMessage(sndr_addr, msg_arr);
	else if (strncmp(task_name, "REFRESH", 7) == 0) {
		sendRefreshListToClient(sndr_addr);
	}
}

/*
 * this is the first function called when the application is started as server.
 * The server then opens a socket and starts listening on it while accepting any incoming connections on the PORT it is
 * listening to for incoming connections.
 * Referred from Beej socket programming guide, server.c code in select() function section of the guide.
 */
int create_server_connection(char *input_port) {
	struct address_info addr;
	addr.ai_family = AF_INET;
	addr.ai_socktype = SOCK_STREAM;
	addr.ai_protocol = IPPROTO_TCP;
	addr.server_port = input_port;
	fd_set read_master;    // master file descriptor list
	fd_set read_fds;  // temp file descriptor list for select()
	fd_set write_master;
	fd_set write_fds;
	int fdmax;        // maximum file descriptor number
	int listener;     // listening socket descriptor
	int newfd;        // newly accept()ed socket descriptor
	struct sockaddr_in remoteaddr; // client address
	socklen_t addrlen;
	char read_buffer[BUFFER_SIZE];    // buffer for client data
	memset(&read_buffer, 0, sizeof(read_buffer));
	int nbytes;
	char remoteIP[INET_ADDRSTRLEN];
	int yes = 1;        // for setsockopt() SO_REUSEADDR, below
	int i, j, rv;
	int CLIENTS = 4;
	struct addrinfo hints, *ai;

	FD_ZERO(&read_master);    // clear the read master and temp sets
	FD_ZERO(&read_fds);
	FD_SET(0, &read_master);
	FD_ZERO(&write_master);   // clear the write master and temp sets		
	FD_ZERO(&write_fds);
	FD_SET(0, &write_master);

	gethostname(hostname, sizeof hostname);
	addr.host_name = hostname;
	get_ip_by_hostname(hostname);
	port = convert_char_int(addr.server_port);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, addr.server_port, &hints, &ai)) != 0)
		perror("selectserver: getaddrinfo error\n");

	listener = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (listener < 0) {
		return 1;
	}

	// lose the pesky "address already in use" error message.
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	if (bind(listener, ai->ai_addr, ai->ai_addrlen) < 0) {
		close(listener);
		return 2;
	}

	// if we got here, it means we didn't get bound
	if (ai == NULL) {
		perror("selectserver: failed to bind\n");
		return 2;
	}

	freeaddrinfo(ai); // all done with this

	// listen error
	if (listen(listener, CLIENTS_MAX_SIZE) == -1) {
		perror("listen error on server\n");
	}

	// add the listener to the master set
	FD_SET(listener, &read_master);

	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one	
	for (;;) {
		read_fds = read_master; // copy it
		if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
			return 4;
		}
		// run through the existing connections looking for data to read
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &read_fds)) { // we got one!!
				if (i == listener) {
					// handle new connections
					addrlen = sizeof remoteaddr;
					newfd = accept(listener, (struct sockaddr *) &remoteaddr,
							&addrlen);
					if (newfd == -1) {
						perror("accept");
					} else {
						FD_SET(newfd, &read_master); // add to master set
						if (newfd > fdmax)    // keep track of the max
							fdmax = newfd;

						const char* remoteipaddr = inet_ntop(remoteaddr.sin_family, get_in_addr((struct sockaddr*) &remoteaddr),
																remoteIP, INET6_ADDRSTRLEN);
						struct hostent *host;
						struct in_addr ipv4addr;
						char *remotemachinename;
						//the next 4 lines of code get the client machine name using the client ip address
						inet_pton(AF_INET, remoteipaddr, &ipv4addr);
						host = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
						remotemachinename = (char *) malloc(sizeof(char) * (strlen(host->h_name)));
						remotemachinename = host->h_name;
						//get the client machine port number
						int clientPort = htons(remoteaddr.sin_port);
						addtoStatisticslist(remotemachinename, remoteipaddr, newfd, clientPort);
						sendListToClient(remoteipaddr);
					}
				} else if (FD_ISSET(STDIN_FILENO, &read_fds)) {
					//user input command
					char *token;
					int counter = 0;
					fgets(read_buffer, sizeof read_buffer, stdin);
					char *command_input;
					token = strtok(read_buffer, " ");
					while (token != NULL) {
						command_input = (char *) malloc(sizeof(char) * (counter));
						strcpy(command_input, token);
						token = strtok(NULL, " ");
						counter++;
					}
					if (counter == 1)
						execute_commands(command_input, listener);
				} else {
					// handle data from a client
					nbytes = recv(i, read_buffer, sizeof read_buffer, 0);
					if (nbytes <= 0) {
						if (isClientLoggedOut(i) == 0) {
							// connection closed - logging out client
							logout_client(i);
							close(i); // bye!
							FD_CLR(i, &read_master); // remove from master set
							FD_CLR(i, &write_master);
						}
					} else {
						read_buffer[nbytes] = '\0';
						create_command_token(read_buffer);
					}
				} // END handle data from client
			} // END got new incoming connection
		} // END looping through file descriptors
	}
	return 0;
}
