#include "both.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

typedef enum State STATE;

enum State /* listing all the states server has */
{
    START, FILENAME, FILE_OKAY, IN_ORDER, BUFFERING, FLUSH, DONE
};

void processFile(char * argv[]);
STATE start(char ** argv, int * clientSocket, struct sockaddr_in6 * serverAddress);
STATE filename(char **argv, int *clientSocket, int32_t * sequenceNumber, struct sockaddr_in6 * serverAddress, int32_t buffer_size);
STATE file_okay(int clientSocket, int *outputFileFd, char *outputFileName);
STATE in_order(WindowArray **window, int32_t window_size, int32_t buffer_size, int clientSocket, struct sockaddr_in6 * serverAddress, int outputFileFd, int32_t * sequenceNumber);
STATE buffering(WindowArray *window, int32_t buffer_size, int clientSocket, struct sockaddr_in6 * serverAddress, int outputFileFd, int32_t * sequenceNumber);
STATE flush(int clientSocket, int outputFileFd, int32_t buffer_size, WindowArray *window, struct sockaddr_in6 * serverAddress, int32_t * sequenceNumber);
void clean_up(WindowArray **window, int outputFileFd, int serverSocket);
void checkArgs(int argc, char * argv[]);
void sendRR(int clientSocket, int32_t rr_Number, int32_t * sending_seq, struct sockaddr_in6 * serverAddress);
void sendSREJ(int clientSocket, int32_t srej_number, int32_t * sending_seq, struct sockaddr_in6 * serverAddress);

