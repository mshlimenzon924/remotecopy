#include <stdio.h>
#include <netinet/in.h>
#include "checksum.h"
#include "string.h"

#define PDU_HEADER_LEN 7

int32_t createPDU(uint8_t * pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t * payload, int32_t payloadLen);
void printPDU(uint8_t * aPDU, int32_t pduLength);
int processPDU(uint8_t * aPDU, int32_t pduLength, uint32_t *sequenceNumber, uint8_t *recv_flag, uint8_t **payload_pointer, int32_t *payloadLen);
void printBytes(const uint8_t *data, size_t size);
void printChars(const uint8_t *data, size_t size);