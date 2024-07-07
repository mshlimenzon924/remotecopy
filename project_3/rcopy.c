/* the real rcopy project 3 file */
#include "rcopy.h"

int main (int argc, char *argv[]) {
    checkArgs(argc, argv); /* process arguments */
    sendErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON); /* turn on library to drop and corrupt packets */

    processFile(argv); /* start program */
    return 0;
}

void processFile(char * argv[]) {
    STATE state = START;

    /* opens socket and populates information so to be able to send everywhere */
    int clientSocket = 0;
    struct sockaddr_in6 server;

    /* gets variables set up for packet sending */
    int32_t sendSequenceNum = 0; /* sequence number we are sending */
    int outputFileFd = 0;

    /* created for buffer */
    WindowArray *window = NULL;

    while(state != DONE) {
        switch(state) {
            case START: /* start - opens socket */
                state =  start(argv, &clientSocket, &server);
                break;
            case FILENAME: /*  sends prelim information and processes response from server */
                state = filename(argv, &clientSocket, &sendSequenceNum, &server, (int32_t)atoi(argv[4]));
                break;
            case FILE_OKAY: /* tries to open to-from file */
                state = file_okay(clientSocket, &outputFileFd, argv[2]);
                break;
            case IN_ORDER: /* once file open, we can start receving data (seq # = or < expected )*/
                state = in_order(&window, (int32_t)atoi(argv[3]), (int32_t)atoi(argv[4]), clientSocket, &server, outputFileFd, &sendSequenceNum);
                break;
            case BUFFERING: /* Seq # > expected */
                state = buffering(window, (int32_t)atoi(argv[4]), clientSocket, &server, outputFileFd, &sendSequenceNum);
                break;
            case FLUSH: /* can remove pdus stored in our buffer */
                state = flush(clientSocket, outputFileFd, (int32_t)atoi(argv[4]), window, &server, &sendSequenceNum); // 4 is buffer_size and 3 is window_size
                break;
            case DONE: /* we finished! */
                clean_up(&window, outputFileFd, clientSocket);
                break;
            default:
                break;
        }
    }
}

/* getting UDP socket set up and filling up serverAddress struct with information */
STATE start(char ** argv, int * clientSocket, struct sockaddr_in6 * serverAddress) {
    /* returns DONE if error or FILENAME if no error */
    STATE returnvalue = FILENAME;
    *clientSocket = setupUdpClientToServer(serverAddress, argv[6], atoi(argv[7]));
    /* add to poll set */
    setupPollSet();
    addToPollSet(*clientSocket);
    return returnvalue;
}

/* sending filename, window_size, and buffer_size and processes response */
STATE filename(char **argv, int *clientSocket, int32_t * sequenceNumber, struct sockaddr_in6 * serverAddress, int32_t buffer_size) {
    /* returns DONE if timeout and send 10 times OR recevied BAD FILE (NOT OK) else FILE_OKAY if all good */
    STATE returnvalue = DONE;

    /* setting up for sending packet */
    int filenameLength = strlen(argv[1]); /* from-file length */
    int32_t payloadLen = SIZE_OF_WINDOW_SIZE_VAR + SIZE_OF_BUF_SIZE_VAR + filenameLength;
    uint8_t pduBuffer[PDU_HEADER_LEN + buffer_size]; /* first PDU */
    uint8_t sending_flag = FILENAME_FLAG;

    uint8_t payload[payloadLen]; /* payload within PDU */
    uint32_t windowSize = htonl(atoi(argv[3]));
    uint32_t bufferSize = htonl(atoi(argv[4]));
    memcpy(payload, &windowSize, SIZE_OF_WINDOW_SIZE_VAR);
    memcpy(payload + SIZE_OF_WINDOW_SIZE_VAR, &bufferSize, SIZE_OF_WINDOW_SIZE_VAR);
    memcpy(payload + SIZE_OF_WINDOW_SIZE_VAR + SIZE_OF_BUF_SIZE_VAR, argv[1], filenameLength);
    
    /* creating PDU */
    int32_t pduLength = createPDU(pduBuffer, *sequenceNumber, sending_flag, payload, payloadLen);
    int serverAddrLen = sizeof(struct sockaddr_in6);

    /* setting up for waiting for response saying if to-file is OK */
    /* begin polling and incrementing count each time */
    int count = 0;
    while (count < 10) {
        safeSendto(*clientSocket, pduBuffer, pduLength, 0, (struct sockaddr *)serverAddress, serverAddrLen); /* sent packet */
        if(pollCall(ONE_SEC) != ISSUE) { /* if see some action on socket */
            /* received something from server let's check it out */
            uint8_t pduBuffer[PDU_HEADER_LEN + buffer_size]; /* header + buffer_size */
            int32_t recvLength = safeRecvfrom(*clientSocket, pduBuffer, PDU_HEADER_LEN + buffer_size, 0, (struct sockaddr *)serverAddress, &serverAddrLen);

            /* verify checksum */
            unsigned short result = in_cksum((unsigned short *)pduBuffer, recvLength);

            if(result == 0) { /* not corrupted file */
                uint8_t recv_flag = 0;
                memcpy(&recv_flag, pduBuffer + RECV_FLAG_LOCATION, 1);  /* getting flag */
                uint8_t file_response = 0;
                memcpy(&file_response, pduBuffer + PDU_HEADER_LEN, 1);  /* getting flag */
                if(recv_flag != FILENAME_RESPONSE_FLAG) { /* if we receive not the FILE OK/ FILE NOT OK but get a data packet */
                    if(recv_flag == DATA_PACKET_FLAG || recv_flag == RESENT_TIMEOUT_DATA_PACKET_FLAG) {
                        /* if received data packet I'm just not gonna process it yet since we are not in the PROCESS DATA state yet */
                        /* but will acknowledge it as a FILE_OKAY packet */
                        returnvalue = FILE_OKAY;
                        break;
                    }
                } else { /* if we received a file_okay packet */
                    if(file_response != FILE_OK) { /* recv Bad File (NOT OK) */
                        /* could not find from-file */
                        close(*clientSocket);
                        removeFromPollSet(*clientSocket);
                        returnvalue = DONE;
                        break;
                    } else { /* recv FILE OK */
                        returnvalue =  FILE_OKAY;
                        break;
                    }
                }
                returnvalue = DONE;
                break;
            }
        }
        /* so if timer expired or issue with response and count < 10 */
        count++; /* increment count */
        if(count == 10) { /* count == 10 */
            close(*clientSocket); /* close socket */
            returnvalue = DONE; /* could not create socket */
            break;
        }
        /* close socket + remove from poll set, open new socket + add to poll set, send again, poll(1) */
        close(*clientSocket);
        removeFromPollSet(*clientSocket);
        /* open new socket */
        *clientSocket = setupUdpClientToServer((struct sockaddr_in6 *)serverAddress, argv[6], atoi(argv[7]));
        /* add to poll set */
        setupPollSet();
        addToPollSet(*clientSocket);
    }
    (*sequenceNumber)++;
    return returnvalue;
}

/* trying to open "to-file" */
STATE file_okay(int clientSocket, int *outputFileFd, char *outputFileName) {
    STATE returnvalue = DONE;
    if((*outputFileFd = open(outputFileName, O_CREAT | O_TRUNC | O_WRONLY, 0777)) < 0) {
        returnvalue = DONE;
        close(clientSocket);
        removeFromPollSet(clientSocket);
    } else { /* successfully opened to-file */
        returnvalue = IN_ORDER;
    }
    return returnvalue;
}

/* receiving data from server in-order */
STATE in_order(WindowArray **window, int32_t window_size, int32_t buffer_size, int clientSocket, struct sockaddr_in6 * serverAddress, int outputFileFd, int32_t * sequenceNumber) {
    STATE returnValue = DONE;
    if(*window == NULL) { /* if first time receiving */
        *window = make_window(window_size);
    }

    if(pollCall(TEN_SEC) != ISSUE) {
        /* doesn't time out because we received something */
        int32_t receivepduLength = PDU_HEADER_LEN + buffer_size;
        uint8_t receiveBuffer[receivepduLength]; /* header + buffer_size */
        int serverAddrLen = sizeof(struct sockaddr_in6);
        int32_t recvLength = safeRecvfrom(clientSocket, receiveBuffer, receivepduLength, 0, (struct sockaddr *)serverAddress, &serverAddrLen);

        /* filling up variablesfrom received buffer */
        uint32_t recvSeqNumber = 0;
        uint8_t recv_flag = 0;
        uint8_t * payload_pointer = NULL;
        int32_t payloadLen = 0;
        int error_check = processPDU(receiveBuffer, recvLength, &recvSeqNumber, &recv_flag, &payload_pointer, &payloadLen);

        /* processing what we received */
        if(error_check == ISSUE) {
            /* discarding it - aka ignoring it */
            /* return to poll(10) */
            returnValue = IN_ORDER;
            return returnValue;
        } else if(recvSeqNumber >= getCurrent(*window)){ /* if nothing is wrong with packet and seq# >= lower */
            if(recvSeqNumber == getCurrent(*window)) { /* if current = received same */
                if(recv_flag != EOF_FLAG) { /* if we did not receive EOF */
                    /* write to disk the packet */
                    if (payload_pointer != NULL && payloadLen > 0) {
                        ssize_t bytes_written = write(outputFileFd, payload_pointer, payloadLen);
                        if (bytes_written != payloadLen) {
                            return DONE;
                        }
                    } else {
                        return DONE;
                    }

                    updateHighest(*window, recvSeqNumber); /* current data packet highest one seen so far */
                    /* can move up window */
                    incrementLower(*window);
                    incrementUpper(*window);
                    incrementCurrent(*window);
                    /* Current got incremented so can send RR */
                    sendRR(clientSocket, getCurrent(*window), sequenceNumber, serverAddress); /* get the current data packet and send RR for it*/
                    /* return to poll(10) */
                    return IN_ORDER;
                } else { /* if EOF Flag, send EOF ACK */
                    /* no need to adjust window */
                    /* send EOF ACK packet */
                    uint8_t flag = EOF_ACK_FLAG;
                    int32_t pduLength = PDU_HEADER_LEN + EOF_ACK_PACKET_LENGTH; /* one byte to send so has something to send */
                    uint8_t sendBuffer[pduLength];
                    uint8_t payload[EOF_ACK_PACKET_LENGTH]; /* to hold payload */
                    int32_t pduSendingLength = createPDU(sendBuffer, *sequenceNumber, flag, payload, EOF_ACK_PACKET_LENGTH);
                    safeSendto(clientSocket, sendBuffer, pduSendingLength, 0, (struct sockaddr *)serverAddress,  sizeof(struct sockaddr_in6)); /* sent EOF ACK packet */
                        
                    return DONE; /* send EOF ACK and end myself */
                }
            } else { /* received > expected */
                int32_t i = 0;
                for(i = getCurrent(*window); i < recvSeqNumber; i++) {
                    sendSREJ(clientSocket, i, sequenceNumber, serverAddress); /* send SREJ for all packets that we didn't get */
                }
                updateHighest(*window, recvSeqNumber); /* setting highest to received packet */
                /* buffering packet */
                add_PDU(*window, receiveBuffer, recvLength, recvSeqNumber); /* also set up recvSeqNumber to have value in it */
                return BUFFERING;
            }
        } else { /* if already received */
            sendRR(clientSocket, getLower(*window), sequenceNumber, serverAddress);
            return IN_ORDER;
        }
    } else { /* if we time out */
        return DONE;
    }
    return returnValue;
}

void clean_up(WindowArray **window, int outputFileFd, int serverSocket) {
    free_window(*window); /* free window */
    if (outputFileFd > 0) {  /* close file */
        if (close(outputFileFd) == -1) {
            perror("Error closing outputFileFd.\n");
        }
    }

    if (serverSocket > 0) { /* close socket */
        if (close(serverSocket) == -1) {
            perror("Error closing socket.\n");
        }
    }
}

/* Sending a RR to server with this sequenceNumber */
void sendRR(int socketNum, int32_t rr_Number, int32_t * sending_seq, struct sockaddr_in6 * serverAddress){
    int8_t sending_flag = RR_FLAG;
    int buffer_length = PDU_HEADER_LEN + RCOPY_RESPONSES_PACKET_LENGTH;
    uint8_t buf[buffer_length]; /* pdu header + sequence number */
    uint8_t payload[RCOPY_RESPONSES_PACKET_LENGTH];
    int32_t flipped_rrNumber = htonl(rr_Number);
    memcpy(payload, &flipped_rrNumber, 4); /* copying in that sequence number */
    int pduSendingLength = createPDU(buf, *sending_seq, sending_flag, payload, RCOPY_RESPONSES_PACKET_LENGTH);
    safeSendto(socketNum, buf, pduSendingLength, 0, (struct sockaddr *)serverAddress,  sizeof(struct sockaddr_in6)); /* send Data Packet */
    *sending_seq += 1;
}

/* Sending a SREJ to server with this sequenceNumber */
void sendSREJ(int socketNum, int32_t srej_number, int32_t * sending_seq, struct sockaddr_in6 * serverAddress){
    int8_t sending_flag = SREJ_FLAG;
    int buffer_length = PDU_HEADER_LEN + RCOPY_RESPONSES_PACKET_LENGTH;
    uint8_t buf[buffer_length]; /* pdu header + sequence number */
    uint8_t payload[RCOPY_RESPONSES_PACKET_LENGTH];
    int32_t flipped_srejNumber = htonl(srej_number);
    memcpy(payload, &flipped_srejNumber, 4); /* copying in that sequence number */
    int pduSendingLength = createPDU(buf, (uint32_t )*sending_seq, sending_flag, payload, RCOPY_RESPONSES_PACKET_LENGTH);
    safeSendto(socketNum, buf, pduSendingLength, 0, (struct sockaddr *)serverAddress,  sizeof(struct sockaddr_in6)); /* send Data Packet */
    *sending_seq += 1;
}

/* receiving data from server not in-order */
STATE buffering(WindowArray *window, int32_t buffer_size, int clientSocket, struct sockaddr_in6 * serverAddress, int outputFileFd, int32_t * sequenceNumber) {
    STATE returnValue = FLUSH;
    while(1) {
        if(pollCall(TEN_SEC) != ISSUE) {
            /* got something let's process it */
            int32_t receivepduLength = PDU_HEADER_LEN + buffer_size;
            uint8_t receiveBuffer[receivepduLength]; /* header + buffer_size */
            int serverAddrLen = sizeof(struct sockaddr_in6);
            int32_t recvLength = safeRecvfrom(clientSocket, receiveBuffer, receivepduLength, 0, (struct sockaddr *)serverAddress, &serverAddrLen);

            /* filling up variablesfrom received buffer */
            uint32_t recvSeqNumber = 0;
            uint8_t recv_flag = 0;
            uint8_t * payload_pointer = NULL;
            int32_t payloadLen = 0;
            int error_check = processPDU(receiveBuffer, recvLength, &recvSeqNumber, &recv_flag, &payload_pointer, &payloadLen);

            /* processing what we received */
            if(error_check == ISSUE) {
                /* discarding it - aka ignoring it */
                /* return to poll(10) */
            } else if(recvSeqNumber > getCurrent(window)){ /* if nothing is wrong with packet and seq# >= lower */
                add_PDU(window, receiveBuffer, recvLength, recvSeqNumber); /*add PDU to buffer*/
                /* send SREJ for any packet in between new highest */
                if(getHighest(window) < recvSeqNumber) {
                    int32_t i = 0;
                    for(i = getHighest(window) + 1; i < recvSeqNumber; i++) {
			        /* no need to send a SREJ for the one we already have */
                        sendSREJ(clientSocket, i, sequenceNumber, serverAddress); /* send SREJ for all packets that we didn't get */
                    }
                }
                updateHighest(window, max(getHighest(window), recvSeqNumber)); /* highest will either be highest or seq # )*/
                /* poll again */
            } else if(recvSeqNumber == getCurrent(window)) { /* if current = received same */
                if(recv_flag != EOF_FLAG) {
                    /* write it to disk */
                    if (payload_pointer != NULL && payloadLen > 0) {
                        ssize_t bytes_written = write(outputFileFd, payload_pointer, payloadLen);
                        if (bytes_written != payloadLen) {
                            returnValue = DONE;
                            break;
                        }
                    }
                    /* can move up window */
                    incrementLower(window);
                    incrementUpper(window);
                    incrementCurrent(window);
                    /* sending RR since have incremented current */ 
                    // send from lowest + 1 up to current rrs 
                    sendRR(clientSocket, getCurrent(window), sequenceNumber, serverAddress); /* get the current data packet and send RR for it*/
                    return FLUSH;
                } /* never gonna get an EOF_FLAG cause that means that we have one higher than it and we shouldn't have a sequence # higher than EOF
                     since we in buffering mode - so there is no way the we have a packet past EOF */
            } else { /* if already received */
                /* discarding it - aka ignoring it */
                sendRR(clientSocket, getCurrent(window), sequenceNumber, serverAddress); /* resending RR of the last sent data packet */
                /* return to poll(10) */
            }
        } else { /* if we time out */
            return DONE;
        }
    }
    return returnValue;
}

/* while we were buffering and storing past pdus, let's now flush */
STATE flush(int clientSocket, int outputFileFd, int32_t buffer_size, WindowArray *window, struct sockaddr_in6 * serverAddress, int32_t * sequenceNumber) {
    STATE returnvalue = BUFFERING;
    while(checkPDUValid(window, getCurrent(window))) {
        /* write to disk */
        int32_t receivepduLength = PDU_HEADER_LEN + buffer_size;
        uint8_t receiveBuffer[receivepduLength]; /* header + buffer_size */
        int32_t payloadLen = get_PDU(window, receiveBuffer, getCurrent(window));
        uint8_t * payload_pointer = receiveBuffer + PDU_HEADER_LEN;

	    /* if have gotten an EOF packet let's not write but just end it - there is nothing after it so we safe to leave */
	    if(receiveBuffer[RECV_FLAG_LOCATION] == EOF_FLAG) {
            uint8_t flag = EOF_ACK_FLAG;
            int32_t pduLength = PDU_HEADER_LEN + EOF_ACK_PACKET_LENGTH; /* one byte to send so has something to send */
            uint8_t sendBuffer[pduLength];
            uint8_t payload[EOF_ACK_PACKET_LENGTH]; /* to hold payload */
            int32_t pduSendingLength = createPDU(sendBuffer, *sequenceNumber, flag, payload, EOF_ACK_PACKET_LENGTH);
            safeSendto(clientSocket, sendBuffer, pduSendingLength, 0, (struct sockaddr *)serverAddress,  sizeof(struct sockaddr_in6)); /* sent EOF ACK packet */
            return DONE; /* send EOF ACK and end myself */
        }

        if (payload_pointer != NULL && payloadLen > 0) {
            ssize_t bytes_written = write(outputFileFd, payload_pointer, payloadLen - PDU_HEADER_LEN);
            if (bytes_written != (payloadLen - PDU_HEADER_LEN)) {
                return DONE;
            }
        }
        /* clean up cause now wrote pdu */
        remove_PDU(window, getCurrent(window));
        incrementCurrent(window);
        incrementUpper(window);
        incrementLower(window);
	// since incremented current send RR for it
        sendRR(clientSocket, getCurrent(window), sequenceNumber, serverAddress); /* send RR for that sequence number*/
        if(getCurrent(window) > getHighest(window)) {
            return IN_ORDER;
        }
    }
    /* if we reached a current that is not valid in the buffer */
    return returnvalue;
}

/* Checking arguments */
void checkArgs(int argc, char * argv[]) {
    // 0 1 2 3 4 5 6 7
    // rcopy myfile1 myfile2 10 1000 .1 unix1.csc.calpoly.edu 1234
    // program from-filename to-filename window_size buffer_size error-rate remote-machine remote-port
    // from-filename = server file; to-filename = rcopy file 
    /* check command line arguments  */
    if (argc != 8) {
        fprintf(stderr, "Usage: rcopy from-filename to-filename window-size buffer-size error-rate remote-machine remote-port\n");
        exit(1);
    }

    if (strlen(argv[1]) > MAX_FILENAME_LENGTH) {
        fprintf(stderr, "Error: from-filename %s is too long. Maximum length is 100 characters.\n", argv[1]);
        exit(1);
    }

    if (strlen(argv[2]) > MAX_FILENAME_LENGTH) {
        fprintf(stderr, "Error: to-filename %s is too long. Maximum length is 100 characters.\n", argv[2]);
        exit(1);
    }

    if (atoi(argv[5]) < 0 || atoi(argv[5]) >= 1) {
        fprintf(stderr, "Error: error-rate needs to be between 0 and less than 1 and is %d\n", atoi(argv[5]));
        exit(1);
    }

    if (atoi(argv[4]) > 1400 || atoi(argv[4]) < 108) {
		printf("Buffer size %d cannot be greater than 1400 and smaller than 108.\n", atoi(argv[3]));
		exit(-1);
	}
}

