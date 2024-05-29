// server (based on udpServer) must also take an error rate, recvfrom() the PDU from
// the rcopy and print out the PDU that it received
#include "server.h"
#define MAXBUF 80

void processClient(int socketNum);
int checkArgs(int argc, char *argv[]);
uint32_t sequenceNumber = 0;

int main ( int argc, char *argv[]  )
{
	int socketNum = 0;
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);
	sendErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
	socketNum = udpServerSetup(portNumber);

	processClient(socketNum);

	close(socketNum);
	
	return 0;
}

void processClient(int socketNum)
{
	int dataLen = 0;
	char buffer[MAXBUF + 1];
	struct sockaddr_in6 client;
	int clientAddrLen = sizeof(client);
	
	buffer[0] = '\0';
	while (buffer[0] != '.')
	{
		dataLen = safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
	
		printf("Received message from client with ");
		printIPInfo(&client);
		/* add printPDU to check checksum */
		printPDU((uint8_t *)buffer, dataLen);
	}
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 3) {
		fprintf(stderr, "Usage %s error_rate [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 3) {
		portNumber = atoi(argv[2]);
	}
	
	return portNumber;
}


