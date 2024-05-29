#include <stdio.h>
#include <netinet/in.h>
#include "checksum.h"
#include "string.h"

#define PDU_HEADER_LEN 7

int createPDU(uint8_t * pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t * payload, int payloadLen);
void printPDU(uint8_t * aPDU, int pduLength);
int processPDU(uint8_t * aPDU, int pduLength, uint32_t *sequenceNumber, uint8_t *recv_flag, uint8_t **payload_pointer, uint8_t *payloadLen);
void printBytes(const uint8_t *data, size_t size);
void printChars(const uint8_t *data, size_t size);