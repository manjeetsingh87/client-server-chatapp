#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVERPORT 4950    // the port users will be connecting to

int main(int argc, char *argv[])
{
	char hostname[128];
	gethostname(hostname, sizeof hostname);		
	printf("%s\n", hostname);
	struct hostent *he;
	struct in_addr **addr_list;
	int i;
        char ipstr[256];
	if ((he = gethostbyname(hostname)) == NULL) {
		// get the host info
		return 1;
	}
	addr_list = (struct in_addr **) he->h_addr_list;
	for (i = 0; addr_list[i] != NULL; i++) {
		//Return the first one;
		strcpy(ipstr, inet_ntoa(*addr_list[i]));
		printf("host ip address is: %s\n", ipstr);
        }
    return 0;
}
