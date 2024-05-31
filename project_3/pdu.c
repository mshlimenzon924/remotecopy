#include "pdu.h"

/* Creates pdu and calculates checksum */
int32_t createPDU(uint8_t * pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t * payload, int32_t payloadLen) {
    uint32_t seqNumber_Network = htonl(sequenceNumber);
    uint16_t zeroed_checksum = 0;
    
    memcpy(pduBuffer, &seqNumber_Network, 4);  /* Network sequence number */
    memcpy(pduBuffer + 4, &zeroed_checksum, 2);  /* Checksum - zeroing out for now */
    memcpy(pduBuffer + 6, &flag, 1);  /* Flag */
    memcpy(pduBuffer + 7, payload, payloadLen);  /* Payload */

    int32_t pduLength = PDU_HEADER_LEN + payloadLen;

    uint16_t checksum = (uint16_t)in_cksum((unsigned short *)pduBuffer, pduLength);
    memcpy(pduBuffer + 4, &checksum, 2);

    return pduLength;  /* return total PDU length */
}

void printBytes(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

void printChars(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%c ", (char)data[i]);
    }
    printf("\n");
}

/* Verifies checksum, prints out PDU information, and error messages if PDU corrupted */
void printPDU(uint8_t * aPDU, int32_t pduLength) {
    int payloadLen = pduLength - PDU_HEADER_LEN;  /* payload Len */
    if(payloadLen < 0) {
        printf("Error: PDU was truncated.\n");
        return;
    }

    /* getting PDU information */
    uint32_t seqNumber_Network = 0;
    memcpy(&seqNumber_Network, aPDU, 4);  /* getting sequence number */
    uint32_t sequenceNumber = ntohl(seqNumber_Network);   /* converting into host order */
    /* skipping checksum didn't ask for this value */
    uint8_t flag = 0;
    memcpy(&flag, aPDU + 6, 1);  /* getting flag */
    uint8_t *payload = aPDU + PDU_HEADER_LEN;  /* getting payload */

  /* Check Sum Processing */
    unsigned short result = in_cksum((unsigned short *)aPDU, pduLength);
    if(result != 0) {
        printf("Error: Checksum does not match. PDU has been corrupted.\n");
        return;
    }

    /* Print PDU Contents */
    printf("Sequence Number: %u\n", sequenceNumber);
    printf("Flag: %u\n", flag);
    printf("Payload of payload length %u\n", payloadLen);
    printBytes(payload, payloadLen);
    printChars(payload, payloadLen);
}

/* Processes PDU returns -1 if there is any issues and 0 if there is none */
int processPDU(uint8_t * aPDU, int32_t pduLength, uint32_t *sequenceNumber, uint8_t *recv_flag, uint8_t **payload_pointer, int32_t *payloadLen) {
    *payloadLen = pduLength - PDU_HEADER_LEN;  /* payload Len */
    printf("PAYLOAD LENGTH %d %d\n", pduLength, *payloadLen);
    if(*payloadLen < 0) {
        printf("Error: PDU was truncated.\n");
        return -1;
    }

    /* getting PDU information */
    uint32_t seqNumber_Network = 0;
    memcpy(&seqNumber_Network, aPDU, 4);  /* getting sequence number */
    *sequenceNumber = ntohl(seqNumber_Network);   /* converting into host order */
    /* skipping checksum didn't ask for this value */
    memcpy(recv_flag, aPDU + 6, 1);  /* getting flag */
    *payload_pointer = aPDU + PDU_HEADER_LEN;  /* getting payload */

  /* Check Sum Processing */
    unsigned short result = in_cksum((unsigned short *)aPDU, pduLength);
    if(result != 0) {
        printf("Error: Checksum does not match. PDU has been corrupted.\n");
        return -1;
    }

    /* Print PDU Contents */
    printf("Sequence Number: %u\n", *sequenceNumber);
    printf("Flag: %u\n", *recv_flag);
    printf("Payload of payload length %u\n", *payloadLen);
    printBytes(*payload_pointer, *payloadLen);
    printChars(*payload_pointer, *payloadLen);

    return 0;
}