#ifndef SOCKET_H_

#define SOCKET_H_

/* code referred from beej programming guide */
struct addrinfo {
 int ai_flags; // AI_PASSIVE, AI_CANONNAME, etc.
 int ai_family; // AF_INET, AF_INET6, AF_UNSPEC
 int ai_socktype; // SOCK_STREAM, SOCK_DGRAM
 int ai_protocol; // use 0 for "any"
 size_t ai_addrlen; // size of ai_addr in bytes
 struct sockaddr *ai_addr; // struct sockaddr_in or _in6
 char *ai_canonname; // full canonical hostname
 struct addrinfo *ai_next; // linked list, next node
}; 

/* code referred from beej programming guide */
struct sockaddr_in {
 short int sin_family; // Address family, AF_INET
 unsigned short int sin_port; // Port number
 struct in_addr sin_addr; // Internet address
 unsigned char sin_zero[8]; // Same size as struct sockaddr
};

/* code referred from beej programming guide */
struct in_addr {
 uint32_t s_addr; // that's a 32-bit int (4 bytes)
};

/* code referred from beej programming guide */
struct sockaddr_storage {
 sa_family_t ss_family; // address family
 // all this is padding, implementation specific, ignore it:
 char __ss_pad1[_SS_PAD1SIZE];
 int64_t __ss_align;
 char __ss_pad2[_SS_PAD2SIZE];
};

#endif
