// rcopy (based on udpClient.c) must take in an error rate, read in user input, create an
// application level PDU, print that PDU and then send (using sendtoErr()) the PDU to
// the server

#include "rcopy.h"

#define MAXBUF 80

void talkToServer(int socketNum, struct sockaddr_in6 * server);
int readFromStdin(char * buffer);
int checkArgs(int argc, char * argv[]);
uint32_t sequenceNumber = 0;


int main (int argc, char *argv[])
{
	int socketNum = 0;
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);

	sendErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
	
	socketNum = setupUdpClientToServer(&server, argv[2], portNumber);
	
	talkToServer(socketNum, &server);
	
	close(socketNum);

	return 0;
}

void talkToServer(int socketNum, struct sockaddr_in6 * server){
	int serverAddrLen = sizeof(struct sockaddr_in6);
	// char * ipString = NULL;
	int payloadLen = 0;
	int bufferLen = 0;
	char payload[MAXBUF - 6]; /* MAXBUF + 1 - 7 */
	char buffer[MAXBUF + 1];
	
	payload[0] = '\0';
	while (payload[0] != '.')
	{
		payloadLen = readFromStdin(payload);

		printf("Sending: %s with len: %d\n", payload ,payloadLen);

		bufferLen = createPDU((uint8_t *)buffer, sequenceNumber, 0, (uint8_t *)payload, payloadLen);

		safeSendto(socketNum, buffer, bufferLen, 0, (struct sockaddr *) server, serverAddrLen);
		
	// 	int recvLen = safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) server, &serverAddrLen);
		
	// 	printPDU((uint8_t *)buffer, recvLen);

	// 	// print out bytes received
	// 	ipString = ipAddressToString(server);
	// 	printf("Server with ip: %s and port %d said it received %s\n", ipString, ntohs(server->sin6_port), buffer);
	}
}

int readFromStdin(char * buffer)
{
	char aChar = 0;
	int inputLen = 0;
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: ");
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

int checkArgs(int argc, char * argv[])
{

    int portNumber = 0;
	
    /* check command line arguments  */
	if (argc != 4) {
		printf("usage: %s error_rate host-name port-number \n", argv[0]);
		exit(1);
	}
	
	portNumber = atoi(argv[3]);
	return portNumber;
}





