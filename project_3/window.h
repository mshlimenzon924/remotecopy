#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

// up to 5 global variables but not accessed anywhere else 
typedef struct {
    int32_t sequenceNumber;
    int pduLength;
    int8_t flag;
    uint8_t pdu[1407];
} packet;


typedef struct {
  packet ** array; // head of the array that will store all the PDUs
  int32_t upper; // upper edge of window - bottom of window + window size
  int32_t lower; // lowest frame that has not been ACKed
  int32_t current; //frame you are sending - lower <= current <= upper
  int32_t window_size; //  will correlate to window size
  int32_t highest; // highest sequence number we have seen so far
} WindowArray;

WindowArray * make_window(int32_t window_size);
int checkWindowOpen(WindowArray *window);
void free_window(WindowArray * window);

int getLower(WindowArray *window); // return lower
int getCurrent(WindowArray *window); // return current
int getUpper(WindowArray *window); // return upper
int getHighest(WindowArray *window); // return highest

void updateHighest(WindowArray *window, int32_t value);

void incrementLower(WindowArray *window); // lower + 1
void incrementUpper(WindowArray *window); // upper + 1
void incrementCurrent(WindowArray *window); // current + 1

int checkPDUValid(WindowArray * window, int32_t sequence_number);
int get_PDU(WindowArray * window, uint8_t *PDU, int32_t sequence_number); // memcpy into the PDU we send it the PDU; returns pduLength

void add_PDU(WindowArray *window, uint8_t *PDU, int pdulength, int32_t sequence_number); // malloc a PDU struct and add the pdu there
void remove_PDU(WindowArray * window, int32_t sequence_number); // given a sequence number remove that pdu just make it NULL