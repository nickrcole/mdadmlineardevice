#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

uint8_t *packetBuffer;
/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {

	// convert to little endian
	uint8_t header[8] = {};
	read(sd, header, 8);
	uint32_t newOp = ntohl(header[2] + header[3] + header[4] + header[5]);
	memcpy(&header[2], &newOp, 4);
	memcpy(op, &newOp, 2);
	uint16_t length = ntohs(header[0] << 8 | header[0]);
	uint16_t returnCode = ntohs(header[6] + header[7]);
	memcpy(ret, &returnCode, 2);
	
	// read buffer if read operation
	if (length > 8) {
		read(sd, packetBuffer, 264);
		memcpy(block, packetBuffer, 256);
		return true;
	}
	// if not read operation
	else {
		return true;
	}
	
	return false;
}



/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/

static bool send_packet(int sd, uint32_t op, uint8_t *block) {

	uint16_t size = HEADER_LEN; // packet size
	
	// copy buffer if write operation
	uint32_t command = op >> 26;
	if (command == 5) { // case for write
		size = 264;
		uint16_t newSize = htons(264);
		op = htonl(op);
		memcpy(&packetBuffer[0], &newSize, 2);
		memcpy(&packetBuffer[2], &op, 4);
		memcpy(&packetBuffer[8], block, 256);
		
	}
	else { // case for other operations
		uint16_t newSize = htons(size);
		op = htonl(op);
		memcpy(&packetBuffer[0], &newSize, 2);
		memcpy(&packetBuffer[2], &op, 4);
	}
	
	write(sd, packetBuffer, size);
	
	return true;
}



/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {

	// construct struct to connect to jbod server
	struct sockaddr_in address;
	address.sin_port = htons(port);
	address.sin_family = AF_INET;
	inet_aton(ip, &(address.sin_addr));
	
	cli_sd = socket(AF_INET, SOCK_STREAM, 0);
	if (cli_sd == -1) { // case for error
		printf("ERROR: Unable to create socket[%s]\n", strerror(errno));
		return false;
	}
	
	if (connect(cli_sd, (const struct sockaddr *)&address, sizeof(address)) == -1) {
		// if error
		printf("ERROR: Failed to connect [%s]\n", strerror(errno));
		return false;
	}
	
	return true;

}




/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
	close(cli_sd);
	cli_sd = -1;

}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
	// allocate space for the buffer and return operation/status values
	packetBuffer = malloc(264);
	uint32_t *returnOp = malloc(4);
	uint16_t *returnValue = malloc(2);
	
	// request read/write from the server
	send_packet(cli_sd, op, block);
	recv_packet(cli_sd, returnOp, returnValue, block);
	
	free(packetBuffer);
	free(returnOp);
	free(returnValue);
	
	return 0;

}
