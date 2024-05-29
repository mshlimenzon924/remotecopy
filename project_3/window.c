#include "window.h"

WindowArray * make_window(int32_t window_size){
    WindowArray *window = (WindowArray *)malloc(sizeof(WindowArray));
    if (!window) {
        perror("Memory allocation for WindowArray");
        return NULL;
    }

    window->array = (packet **)malloc(window_size * sizeof(packet *));
    if (!window->array) {
        perror("Memory allocation for PDU array");
        free(window);
        return NULL;
    }

    for (int32_t i = 0; i < window_size; i++) {
        window->array[i] = NULL;
    }

    window->lower = 0;
    window->current = 0;
    window->window_size = window_size;
    window->upper = window->lower + window->window_size;

    return window;
}

int getCurrent(WindowArray *window){
    return window->current;
}

int getLower(WindowArray *window){
    return window->lower;
}

int getHighest(WindowArray *window){
    return window->highest;
}

int getUpper(WindowArray *window){
    return window->upper;
}

void updateHighest(WindowArray *window, int32_t value) {
    window->highest = value;
}

void incrementLower(WindowArray *window) {
    window->lower = window->lower + 1;
}

void incrementUpper(WindowArray *window) {
    window->upper = window->upper + 1;
}
void incrementCurrent(WindowArray *window) {
    window->current = window->current + 1;
}

int checkWindowOpen(WindowArray *window) {
    return window->current < window->upper;
}

/* check if given index has PDU */
int checkPDUValid(WindowArray *window, int32_t sequence_number) {
    int32_t index = sequence_number % window->window_size;
    return (window->array[index] != NULL);
}

/* get PDU from array and populate PDU buffer given */
int get_PDU(WindowArray *window, uint8_t *PDU, int32_t sequence_number) {
    int32_t index = sequence_number % window->window_size;

    if (window->array[index] == NULL) {
        printf("PDU does not exist.\n");
        return -1;
    } else {
        memcpy(PDU, window->array[index]->pdu, window->array[index]->pduLength); /* move into PDU our pdu */
        return window->array[index]->pduLength;
    }
}

// Function to add a PDU to the window
void add_PDU(WindowArray *window, uint8_t *PDU, int pdulength, int32_t sequence_number) {
    int32_t index = sequence_number % window->window_size;

    /* if PDU already populated */
    if (window->array[index] != NULL) {
        printf("PDU already populated.\n");
        return;
    }

    packet *new_pdu = (packet *)malloc(sizeof(packet));
    if (new_pdu == NULL) {
        printf("Error: malloc PDU\n");
        return;
    }
    /* setting up PDU */
    new_pdu->sequenceNumber = sequence_number;
    new_pdu->pduLength = pdulength;
    new_pdu->flag = PDU[6];
    new_pdu->pduLength = pdulength;
    memcpy(new_pdu->pdu, PDU, new_pdu->pduLength);

    /* adding the PDU */
    window->array[index] = new_pdu;
}

/* Function to remove a PDU from the window */
void remove_PDU(WindowArray *window, int32_t sequence_number) {
    int32_t index = sequence_number % window->window_size;
    if (window->array[index] != NULL) {
        free(window->array[index]);
        window->array[index] = NULL;
    } else {
        printf("Index is not populated.\n");
    }
}

void free_window(WindowArray * window) {
    if (window == NULL) {
        return;
    }

    for (int32_t i = 0; i < window->window_size; i++) { /* for window size go one by one and free array */
        if (window->array[i] != NULL) {
            free(window->array[i]);
        }
    }

    free(window->array); /* free array itself */
    free(window); /* free window */
}



