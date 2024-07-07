#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "cpe464.h"
#include "pdu.h"
#include "pollLib.h"
#include "window.h"

#define MAX_FILENAME_LENGTH 100     /* not including null - since strlen does not return null */
#define SIZE_OF_BUF_SIZE_VAR  4     /* 4 bytes to store "buffer size" variable = how many bytes sent in a packet */
#define SIZE_OF_WINDOW_SIZE_VAR 4   /* 4 bytes to store "window size" variable = how many packets can be stored in window */
#define ISSUE -1               /* poll timeout or issue*/
#define FILE_OK 1                   /* from-file is okay */
#define FILE_NOT_OK 0               /* from-file not okay*/
#define TEN_SEC 10000   /* use this for pollCall for 10 sec */
#define ONE_SEC 1000   /* use this for pollCall for 1 sec */
#define RECV_FLAG_LOCATION 6                /* where in the pdu the recv flag is stored */

/* packet lengths */
#define FILENAME_RESPONSE_PACKET_LENGTH 1   /* for OK or NOT_OK */
#define EOF_ACK_PACKET_LENGTH 1             /* just to fill up with space cause have to */
#define RCOPY_RESPONSES_PACKET_LENGTH 4     /* four bytes for seq # RR and SREJ */
#define EOF_PACKET_LENGTH 1                 /* ust to fill up with space cause have to  */

/* flags defined */
#define RR_FLAG 5                       /* RR packet */
#define SREJ_FLAG 6                     /* SREJ packet */
#define FILENAME_FLAG 8                 /* Packet contains the file name/buffer-size/window-size (rcopy to server) */
#define FILENAME_RESPONSE_FLAG 9        /*  Packet contains the response to the filename packet (server to rcopy) */
#define EOF_FLAG 10                     /* Packet is your EOF indication (server to rcopy) */
#define EOF_ACK_FLAG 32                 /* Packet is your EOF ACK indicator (rcopy to server) */
#define DATA_PACKET_FLAG 16                  /* Regular data packet */
#define RESENT_SREJ_DATA_PACKET_FLAG 17      /* Resent data packet (after sender receiving a SREJ, not a timeout) */
#define RESENT_TIMEOUT_DATA_PACKET_FLAG 18   /* Resent data packet after a timeout (so lowest in window resent data packet.) */
