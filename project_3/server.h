#include "both.h"

typedef enum State STATE;

enum State /* listing all the states server has */
{
    START, DONE, FILENAME, WINDOW_OPEN, WINDOW_CLOSED, END_OF_FILE
};

void process_client(int childSocket, struct sockaddr_in6 client, int clientAddrLen, uint8_t *buf, int recv_len);
int checkArgs(int argc, char * argv[]);
void process_server(float error_rate, int clientSocket);
STATE filename(int childSocket, uint32_t * sequenceNumber, struct sockaddr_in6 * client, uint8_t *buf, int recv_len, int32_t *data_file, int32_t *buf_size, int32_t *window_size);
STATE window_open(int socketNumber, struct sockaddr_in6 * client, WindowArray **window, int32_t data_file, int32_t window_size, int32_t buffer_size);
STATE window_closed(int socketNumber, struct sockaddr_in6 * client, WindowArray *window, int32_t buffer_size);
void cleaning_up_with_RR(WindowArray *window, int32_t rr_seq_num);
void resend_data_packet(int clientSocket, WindowArray *window, uint32_t sequence_number, uint8_t flag, int32_t buffer_size, struct sockaddr_in6 * client);
void send_data_packet(int clientSocket, WindowArray *window, uint8_t *payload, int payloadLength, struct sockaddr_in6 * client);
STATE end_of_file(int childSocket, WindowArray *window, int32_t buffer_size, struct sockaddr_in6 * client);
void handleZombies(int sig);
void clean_up(WindowArray **window, int data_file, int childSocket);
