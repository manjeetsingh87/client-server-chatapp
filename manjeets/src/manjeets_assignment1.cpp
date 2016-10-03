/**
 * @manjeets_assignment1
 * @author  Manjeet Singh <manjeets@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <iostream>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "../include/global.h"
#include "../include/logger.h"
#include "../include/server.h"
#include "../include/client.h"

#define BACKLOG 10     // how many pending connections queue will hold
using namespace std;
/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int main(int argc, char **argv) {
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/* Clear LOGFILE*/
	fclose(fopen(LOGFILE, "w"));

	/*Start Here*/
	if(argc-1 < 1) {
		cse4589_print_and_log("Invalid command\n");
		exit(1);
	}
	string input_command = argv[1];
	if(input_command.compare("s") == 0) {
		//application was started as server i.e command was ./assignment1 s <PORT>
		create_server_connection(argv[2]);
	} else if(input_command.compare("c") == 0){
		//application was started as client i.e command was ./assignment1 c <PORT>
		start_client(argv[2]);
	}
	return 0;
}
