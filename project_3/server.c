/* the real rcopy project 3 file */
#include "server.h"

int main (int argc, char *argv[]) {
    int mainServerSocket = 0;
    int portNumber = 0;
    
    portNumber = checkArgs(argc, argv); /* process arguments */
    sendErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON); /* turn on library to drop and corrupt packets */

    /* set up main server socket*/
    mainServerSocket = udpServerSetup(portNumber);

    /* add main server socket to poll set */
    setupPollSet();
    addToPollSet(mainServerSocket);

	process_server(atof(argv[1]), mainServerSocket); /* start program */

	return 0;
}

void process_server(float error_rate, int clientSocket) {
    pid_t pid = 0; /* pid for client */
    int childSocket = 0; /* socket in child */

    /* variables created to process set-up packet */
    int buffer_length = PDU_HEADER_LEN + MAX_FILENAME_LENGTH + SIZE_OF_BUF_SIZE_VAR + SIZE_OF_WINDOW_SIZE_VAR;
    uint8_t buf[buffer_length]; /* pdu header + filename + and null */
    int flag = 0;
    struct sockaddr_in6 client;
    int clientAddrLen = sizeof(client);
    int recv_len = 0;

    // comment it out until he updates pollLib code
    // signal(SIGCHLD, handleZombies); /* setting up signal handler to clean up after we fork */

    /* while loop waiting for new client and forking a child when we get it */
    while(1) {
        /* server should never turn off even if no clients are connected and block waiting for a new client */
        if(pollCall(-1) != ISSUE) { /* waiting for a client to connect */
            /* receive client information and filename so we can pass onto our child */
            recv_len = safeRecvfrom(clientSocket, buf, buffer_length, flag, (struct sockaddr *) &client, &clientAddrLen);
            if((pid = fork()) < 0){ /* creating child to deal with client */
                perror("Forking issue.\n");
                exit(-1);
            }
            /* child process succesfully made */
            if(pid == 0) {
                /* set up socket for child */
                close(clientSocket); /* no need to communicate with client with this socket so we shall close it */
                childSocket = udpServerSetup(0); /* get new socket on child to talk with client - must use different port number x4*/
                sendErr_init(error_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON); /* turn on library to drop and corrupt packets */

                /* and now I'm gonna let the child talk to the client and process filename */
                process_client(childSocket, client, clientAddrLen, buf, recv_len);
                exit(0);
            }
            /* if server parent does nothing continue loop and continue pollCalling waiting for children */
        }
    }
}

void process_client(int childSocket, struct sockaddr_in6 client, int clientAddrLen, uint8_t *buf, int recv_len) {
    /* given a child socket to talk to, client information, and buffer with the filename let's get into it */
    STATE state = START;
    uint32_t sequenceNumber = 0;

    /* given information from client */
    int32_t buf_size = 0;
    int32_t window_size = 0;
    int32_t data_file = 0;

    /* created for window communication */
    WindowArray *window = NULL;

    while(state != DONE) {
        switch(state) {
            case START: /* create child */
                state = FILENAME;
                break;
            case FILENAME: /* processing file name given */
                state = filename(childSocket, &sequenceNumber, &client, buf, recv_len, &data_file, &buf_size, &window_size);
                break;
            case WINDOW_OPEN: /* window is open and can send packets */
                state = window_open(childSocket, &client, &window, data_file, window_size, buf_size);
                break;
            case WINDOW_CLOSED: /* window is closed cannot send packets */
                state = window_closed(childSocket, &client, window, buf_size);
                break;
            case END_OF_FILE: /* have read an end of file/ similar to closed window - no more to send */
                state = end_of_file(childSocket, window, buf_size, &client);
                break;
            case DONE: /* we finished! */
                clean_up(&window, data_file, childSocket);
                break;
            default:
                /* should never have reached this state */
                break;
        }
    }
}

/* child server processing filename packet and setting up */
STATE filename(int childSocket, uint32_t * sequenceNumber, struct sockaddr_in6 * client, uint8_t *buf, int recv_len, int32_t *data_file, int32_t *buf_size, int32_t *window_size) {
    STATE returnValue = DONE;

    // recevied buffer size, window size, filename in buffer
    uint8_t recv_flag = 0;
    uint8_t *payload = NULL;
    int32_t payloadLen = 0;

    /* processing PDU = if issue with packet!! */
    if(processPDU(buf, recv_len, sequenceNumber, &recv_flag, &payload, &payloadLen) < 0) {
        returnValue = DONE;
    } else {
        /* process packet given by client */
        char fname[MAX_FILENAME_LENGTH + 1];
        memcpy(window_size, payload, SIZE_OF_WINDOW_SIZE_VAR);
        *window_size = ntohl(*window_size);
        memcpy(buf_size, payload + SIZE_OF_WINDOW_SIZE_VAR, SIZE_OF_BUF_SIZE_VAR);
        *buf_size = ntohl(*buf_size);
        int fname_length = payloadLen - SIZE_OF_WINDOW_SIZE_VAR - SIZE_OF_BUF_SIZE_VAR;
        memcpy(fname, payload + SIZE_OF_WINDOW_SIZE_VAR + SIZE_OF_BUF_SIZE_VAR, fname_length);
        fname[fname_length] = '\0';

        /* preparing to send response back */
        uint8_t flag = FILENAME_RESPONSE_FLAG;
        uint8_t payload[FILENAME_RESPONSE_PACKET_LENGTH]; /* to hold payload */
        int32_t pduLength = PDU_HEADER_LEN + FILENAME_RESPONSE_PACKET_LENGTH;
        uint8_t buffer[pduLength]; /* hold entire buffer */

        if(((*data_file) = open(fname, O_RDONLY)) > 0) {
            *payload = (uint8_t)FILE_OK;
            /* creating PDU with FILE_OK and sending it */
            int32_t pduSendingLength = createPDU(buffer, *sequenceNumber, flag, payload, FILENAME_RESPONSE_PACKET_LENGTH);
            safeSendto(childSocket, buffer, pduSendingLength, 0, (struct sockaddr *)client,  sizeof(struct sockaddr_in6)); /* sent packet */
            returnValue = WINDOW_OPEN;
        } else {
            *payload = (uint8_t)FILE_NOT_OK;
            /* creating PDU with FILE_NOT_OK */
            int32_t pduSendingLength = createPDU(buffer, *sequenceNumber, flag, payload, FILENAME_RESPONSE_PACKET_LENGTH);
            safeSendto(childSocket, buffer, pduSendingLength, 0, (struct sockaddr *)client,  sizeof(struct sockaddr_in6)); /* sent packet */
            /* and now we are killing ourself */
            returnValue = DONE;
        }
    }
    return returnValue;
}


STATE window_open(int socketNumber, struct sockaddr_in6 * client, WindowArray ** window, int32_t data_file, int32_t window_size, int32_t buffer_size){
    STATE returnValue = WINDOW_CLOSED;
    /* if window is open for first time let's make window */
    if(*window == NULL) {
        *window = make_window(window_size);
    }
    while(checkWindowOpen(*window) == TRUE) {
        int32_t payloadLength = buffer_size;
        uint8_t payload[buffer_size]; /* payload with data*/
        /* read data from disk into payload of buffer size */
        int32_t bytesRead = read(data_file, &payload, payloadLength); /* try to read payload of buffer_size */
        if (bytesRead < 0) {
            returnValue = DONE;
            break;
        } else if (bytesRead == 0) {  /* if read an EOF: returnValue = END_OF_FILE and break out of loop */
            returnValue = END_OF_FILE;
            break;
        } else if (bytesRead != buffer_size) { /* didn't have full buffer size to read */
            payloadLength = bytesRead;
        }

        /* create PDU, send PDU, and store PDU in window */
        send_data_packet(socketNumber, *window, payload, payloadLength, client);

        /* update buffer variables - increment current */
        if(pollCall(0) != ISSUE) { /* non-blocking call if receive something then process */
            int32_t recv_buffer_length = PDU_HEADER_LEN + RCOPY_RESPONSES_PACKET_LENGTH;
            uint8_t recv_buf[recv_buffer_length];
            int flag = 0;
            int clientLength = sizeof(struct sockaddr_in6);
            int32_t recv_len = safeRecvfrom(socketNumber, recv_buf, recv_buffer_length, flag, (struct sockaddr *)client, &clientLength);

            /* filling up variablesfrom received buffer */
            uint32_t recvSeqNumber = 0;
            uint8_t recv_flag = 0;
            uint8_t * payload_pointer = NULL;
            int32_t payloadLen = 0;
            int error_check = processPDU(recv_buf, recv_len, &recvSeqNumber, &recv_flag, &payload_pointer, &payloadLen);
            if (error_check == ISSUE){
                returnValue = WINDOW_OPEN;
                break;
            }

            if(recv_flag == RR_FLAG) { /* if recv RR*/
                int32_t rr_seq_num = 0;
                memcpy(&rr_seq_num, payload_pointer, 4);
                int32_t flipped_rr_seq_num = ntohl(rr_seq_num);
                cleaning_up_with_RR(*window, flipped_rr_seq_num);
            } /* lower should be RR now and upper = RR + window_Size - removed all frames we have confirmed we got */
            else if (recv_flag == SREJ_FLAG){ /* if recv SREJ */
                int32_t srej_seq_num = 0;
                memcpy(&srej_seq_num, payload_pointer, 4);
                int32_t flipped_srej_seq_num = ntohl(srej_seq_num);
                int8_t resend_flag = RESENT_SREJ_DATA_PACKET_FLAG;
                resend_data_packet(socketNumber, *window, flipped_srej_seq_num, resend_flag, buffer_size, client);
            } else { /* recevied something else this is wrong */
                returnValue = DONE;
                break;
            }
        }
    }
    return returnValue;
}

void cleaning_up_with_RR(WindowArray *window, int32_t rr_seq_num){
    int32_t lower = 0;
    while( (lower = getLower(window)) < rr_seq_num){ /* until lower = rr_seq_num*/
        remove_PDU(window, lower);
        incrementLower(window);
        incrementUpper(window);
    }
}

/* resending either the lowest or a specific sequence number - flag will tell us if it is from a timeout or SREJ */
void resend_data_packet(int clientSocket, WindowArray *window, uint32_t sequence_number, uint8_t flag, int32_t buffer_size, struct sockaddr_in6 * client) {
    int32_t send_buffer_length = PDU_HEADER_LEN + buffer_size;
    uint8_t resending_buffer[send_buffer_length];
    int32_t pduLength = get_PDU(window, resending_buffer, (int32_t)sequence_number);

    /* remake PDU */
    uint8_t received_flag = 0;
    memcpy(&received_flag, resending_buffer + RECV_FLAG_LOCATION, 1);
    if(received_flag == DATA_PACKET_FLAG) {
        memcpy(&received_flag, &flag, 1); /* getting flag set up */
    }
    uint8_t buf[send_buffer_length]; /* make buffer */
    int32_t pduSendingLength = createPDU(buf, sequence_number, received_flag, resending_buffer + PDU_HEADER_LEN, pduLength - PDU_HEADER_LEN); /* recalculate pdu */
    /* everything same only difference is changing flag if it iss data packet and if EOF keep same */
    safeSendto(clientSocket, buf, pduSendingLength, 0, (struct sockaddr *)client,  sizeof(struct sockaddr_in6));
}

/* sending a normal packet */
void send_data_packet(int clientSocket, WindowArray *window, uint8_t *payload, int payloadLength, struct sockaddr_in6 * client) {
   /* create PDU */
    int32_t buffer_length = PDU_HEADER_LEN + payloadLength;
    uint8_t buf[buffer_length]; /* pdu header + buffer size */
    uint8_t flag = DATA_PACKET_FLAG;
    uint32_t sequenceNum = (uint32_t)getCurrent(window); /* current is the next packet to read */
    int32_t pduSendingLength = createPDU(buf, sequenceNum, flag, payload, payloadLength);

    /* send PDU */
    safeSendto(clientSocket, buf, pduSendingLength, 0, (struct sockaddr *)client,  sizeof(struct sockaddr_in6)); /* send Data Packet */
    /* also store PDU in our window */
    add_PDU(window, buf, pduSendingLength, (int32_t)sequenceNum);
    incrementCurrent(window);
}

/* sending EOF */
void send_eof(int socketNumber, WindowArray *window, int32_t buffer_size, struct sockaddr_in6 * client) {
    int32_t buffer_length = PDU_HEADER_LEN + EOF_PACKET_LENGTH;
    uint8_t buf[buffer_length]; /* pdu header + buffer size */
    uint8_t payload[EOF_PACKET_LENGTH];
    int payloadLength = EOF_PACKET_LENGTH;
    uint8_t sending_flag = EOF_FLAG;
    uint32_t sequenceNum = getCurrent(window); /* current is the next packet to read */
    int32_t pduSendingLength = createPDU(buf, sequenceNum, sending_flag, payload, payloadLength);

    /* send PDU */
    safeSendto(socketNumber, buf, pduSendingLength, 0, (struct sockaddr *)client,  sizeof(struct sockaddr_in6)); /* send Data Packet */
    /* also store PDU in our window buffer */
    add_PDU(window, buf, pduSendingLength, (int32_t)sequenceNum);
}

STATE window_closed(int socketNumber, struct sockaddr_in6 * client, WindowArray *window, int32_t buffer_size) {
    STATE returnValue = WINDOW_OPEN;
    int count = 0;
    while(checkWindowOpen(window) == FALSE && count < 10) {
        if(pollCall(ONE_SEC) != ISSUE) { /* not a timeout */
            int32_t recv_buffer_length = PDU_HEADER_LEN + RCOPY_RESPONSES_PACKET_LENGTH;
            uint8_t recv_buf[recv_buffer_length];
            int flag = 0;
            int serverAddrLen = sizeof(struct sockaddr_in6);
            int32_t recv_len = safeRecvfrom(socketNumber, recv_buf, recv_buffer_length, flag, (struct sockaddr *)client, &serverAddrLen);

            /* filling up variablesfrom received buffer */
            uint32_t recvSeqNumber = 0;
            uint8_t recv_flag = 0;
            uint8_t * payload_pointer = NULL;
            int32_t payloadLen = 0;
            int error_check = processPDU(recv_buf, recv_len, &recvSeqNumber, &recv_flag, &payload_pointer, &payloadLen);
            if (error_check == ISSUE){
                returnValue = WINDOW_CLOSED;
                break;
            }

            if(recv_flag == RR_FLAG) { /* if recv RR*/
                // same as other maybe make function? processing RR_Flag
                int32_t rr_seq_num = 0;
                memcpy(&rr_seq_num, payload_pointer, 4);
                int32_t flipped_rr_seq_num = ntohl(rr_seq_num);
                cleaning_up_with_RR(window, flipped_rr_seq_num);
                /* lower should be RR now and upper = RR + window_Size - removed all frames we have confirmed we got */
                count = 0;
                return WINDOW_OPEN;
            } else if (recv_flag == SREJ_FLAG){ /* if recv SREJ */
                /* resend rejected */
                int32_t srej_seq_num = 0;
                memcpy(&srej_seq_num, payload_pointer, 4);
                int32_t flipped_srej_seq_num = ntohl(srej_seq_num);
                int8_t resend_flag = RESENT_SREJ_DATA_PACKET_FLAG;
                resend_data_packet(socketNumber, window, flipped_srej_seq_num, resend_flag, buffer_size, client);
                count = 0;
            } else { /* recevied something else this is wrong */
                return DONE;
            }
       } else { /* had a timeout */
        int8_t resending_flag = RESENT_TIMEOUT_DATA_PACKET_FLAG;
        resend_data_packet(socketNumber, window, getLower(window), resending_flag, buffer_size, client); /* resending lowest packet */
        count++;
        }
    }
    if (count == 10) { /* tried 10 times to poll and send last packet but it didn't work - no response at all! */
        return DONE;
    }
    return returnValue;
}

/* read an EOF so now in this closed window state */
STATE end_of_file(int socketNumber, WindowArray *window, int32_t buffer_size, struct sockaddr_in6 * client) {
    send_eof(socketNumber, window, buffer_size, client); /* send EOF and added it to our window */
    
    int count = 0;
    /* wait 10 times */
    while(count < 10) {
        /* try polling for 1 second */
        if(pollCall(ONE_SEC) != ISSUE) { /* not a timeout */ // should be 1_sec
            int32_t recv_buffer_length = PDU_HEADER_LEN + RCOPY_RESPONSES_PACKET_LENGTH;
            uint8_t recv_buf[recv_buffer_length];
            int flag = 0;
            int serverAddrLen = sizeof(struct sockaddr_in6);
            int32_t recv_len = safeRecvfrom(socketNumber, recv_buf, recv_buffer_length, flag, (struct sockaddr *)client, &serverAddrLen );

            /* filling up variables from received buffer */
            uint32_t recvSeqNumber = 0;
            uint8_t recv_flag = 0;
            uint8_t * payload_pointer = NULL;
            int32_t payloadLen = 0;
            int error_check = processPDU(recv_buf, recv_len, &recvSeqNumber, &recv_flag, &payload_pointer, &payloadLen);
            if(error_check == ISSUE) {
                count = 0;
                continue; // just ignore this packet but got something so now we can reset count
            }

            if(recv_flag == RR_FLAG) { /* if recv RR*/
                int32_t rr_seq_num = 0;
                memcpy(&rr_seq_num, payload_pointer, 4);
                int32_t flipped_rr_seq_num = ntohl(rr_seq_num);
                cleaning_up_with_RR(window, flipped_rr_seq_num);
                count = 0;
                continue;
            } /* lower should be RR now and upper = RR + window_Size - removed all frames we have confirmed we got */
            else if (recv_flag == SREJ_FLAG){ /* if recv SREJ */
                int32_t srej_seq_num = 0;
                memcpy(&srej_seq_num, payload_pointer, 4);
                int32_t flipped_srej_seq_num = ntohl(srej_seq_num);
                int8_t resend_flag = RESENT_SREJ_DATA_PACKET_FLAG;
                resend_data_packet(socketNumber, window, flipped_srej_seq_num, resend_flag, buffer_size, client);
                count = 0;
            } else if (recv_flag == EOF_ACK_FLAG) {  /* received EOF ACK */
                return DONE;
            } else { /* recevied something else this is wrong */
                return DONE;
            }
       } else { /* had a timeout - can resend either EOF or Data Packet */
        int8_t resending_flag = RESENT_TIMEOUT_DATA_PACKET_FLAG;
        resend_data_packet(socketNumber, window, getLower(window), resending_flag, buffer_size, client); /* resending lowest packet */
        count++;
        }
    }
    /* we have set 10 times time to give up */
    return DONE;
}

void clean_up(WindowArray **window, int data_file, int childSocket){
    free_window(*window); /* free window */
    if (data_file > 0) {  /* close file */
        if (close(data_file) == -1) {
            perror("Error closing file.\n");
        }
    }

    if (childSocket > 0) { /* close socket */
        if (close(childSocket) == -1) {
            perror("Error closing socket.\n");
        }
    }
}

/* Checking arguments */
int checkArgs(int argc, char * argv[]) {
    // 0 1
    // server .1
    // program error-rate [optional-port-number]

    int portNumber = 0;

    /* check command line arguments  */
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: server error-rate [optional-port-number]\n");
        exit(1);
    }

    if (atof(argv[1]) < 0 || atof(argv[1]) >= 1) {
        fprintf(stderr, "Error: error-rate needs to be between 0 and less than 1 and is %f\n", atof(argv[1]));
        exit(1);
    }

    if(argc == 3) {
        portNumber = atoi(argv[2]);
    } else {
        portNumber = 0;
    }

    return portNumber;
}

