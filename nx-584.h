/* nx-584.h
 *
 * Copyright (C) 2009 Timo Kokkonen. All Rights Reserved.
 */

#ifndef _NX_584_H
#define _NX_584_H 1

#define NX_PROTOCOL_BINARY    0x00
#define NX_PROTOCOL_ASCII     0x01

#define NX_MSG_MASK           0x3f
#define NX_MSG_ACK_FLAG       0x80
#define NX_IS_ACKMSG(n)  ((n & NX_MSG_ACK_FLAG) == NX_MSG_ACK_FLAG)
#define NX_IS_NONREPORTING_EVENT(type)  ( (type & 0x80) == 0x80 ? 1 : 0 ) 

#define NX_PARTITIONS_MAX    8
#define NX_ZONES_MAX         256
#define NX_ZONE_NAME_MAXLEN  16
#define NX_MAX_LOG_ENTRIES   256


/* module numbers */
#define NX_MODULE_CONTROL_PANEL		8
#define NX_MODULE_NX584	                72
#define NX_MODULE_NX592                 78
#define NX_MODULE_KEYPAD1		192
#define NX_MODULE_KEYPAD2		200
#define NX_MODULE_KEYPAD3		208
#define NX_MODULE_KEYPAD4		216
#define NX_MODULE_KEYPAD5		224
#define NX_MODULE_KEYPAD6		232
#define NX_MODULE_KEYPAD7		240
#define NX_MODULE_KEYPAD8		248
#define NX_MODULE_VIRTUAL_KEYPAD	248


/* message types used in gateway interface protocol */

#define NX_INT_CONFIG_MSG     0x01
#define NX_ZONE_NAME_MSG      0x03
#define NX_ZONE_STATUS_MSG    0x04
#define NX_ZONE_SNAPSHOT_MSG  0x05
#define NX_PART_STATUS_MSG    0x06
#define NX_PART_SNAPSHOT_MSG  0x07
#define NX_SYS_STATUS_MSG     0x08
#define NX_X10_RCV_MSG        0x09
#define NX_LOG_EVENT_MSG      0x0a
#define NX_KEYPAD_MSG_RCVD    0x0b
#define NX_PROG_DATA_REPLY    0x10 
#define NX_USER_INFO_REPLY    0x12
#define NX_CMD_FAILED         0x1c
#define NX_POSITIVE_ACK       0x1d
#define NX_NEGATIVE_ACK       0x1e
#define NX_MSG_REJECTED       0x1f
#define NX_INT_CONFIG_REQ     0x21
#define NX_ZONE_NAME_REQ      0x23
#define NX_ZONE_STATUS_REQ    0x24
#define NX_ZONE_SNAPSHOT_REQ  0x25
#define NX_PART_STATUS_REQ    0x26
#define NX_PART_SNAPSHOT_REQ  0x27
#define NX_SYS_STATUS_REQ     0x28
#define NX_X10_SEND_MSG       0x29 
#define NX_LOG_EVENT_REQ      0x2a
#define NX_KEYPAD_MSG_SEND    0x2b
#define NX_KEYPAD_TM_REQ      0x2c
#define NX_PROG_DATA_REQ      0x30
#define NX_PROG_DATA_CMD      0x31
#define NX_USER_INFO_REQ_PIN  0x32
#define NX_USER_INFO_REQ      0x33
#define NX_SET_USER_CODE_PIN  0x34
#define NX_SET_USER_CODE      0x35
#define NX_SET_USER_AUTH_PIN  0x36
#define NX_SET_USER_AUTH      0x37
#define NX_STORE_COMM_EVENT   0x3a
#define NX_SET_CLOCK_CMD      0x3b
#define NX_PRI_KEYPAD_FUNC_PIN 0x3c
#define NX_PRI_KEYPAD_FUNC    0x3d
#define NX_SEC_KEYPAD_FUNC    0x3e
#define NX_ZONE_BYPASS_TOGGLE 0x3f

/* supported commands for "keypad" functions */
#define NX_KEYPAD_FUNC_SILENCE	       0x3c00
#define NX_KEYPAD_FUNC_DISARM	       0x3c01
#define NX_KEYPAD_FUNC_ARM_AWAY	       0x3c02
#define NX_KEYPAD_FUNC_ARM_STAY	       0x3c03
#define NX_KEYPAD_FUNC_CANCEL	       0x3c04
#define NX_KEYPAD_FUNC_AUTO_ARM        0x3c05
#define NX_KEYPAD_FUNC_WALKTEST_START  0x3c06
#define NX_KEYPAD_FUNC_WALKTEST_STOP   0x3c07
#define NX_KEYPAD_FUNC_STAY            0x3e00
#define NX_KEYPAD_FUNC_CHIME           0x3e01
#define NX_KEYPAD_FUNC_EXIT            0x3e02
#define NX_KEYPAD_FUNC_BYPASS          0x3e03
#define NX_KEYPAD_FUNC_FIRE_PANIC      0x3e04
#define NX_KEYPAD_FUNC_MEDICAL_PANIC   0x3e05
#define NX_KEYPAD_FUNC_POLICE_PANIC    0x3e06
#define NX_KEYPAD_FUNC_SMOKE_RESET     0x3e07
#define NX_KEYPAD_FUNC_SILENT_EXIT     0x3e0a
#define NX_KEYPAD_FUNC_TEST            0x3e0b
#define NX_KEYPAD_FUNC_GROUP_BYPASS    0x3e0c
#define NX_KEYPAD_FUNC_AUX1            0x3e0d
#define NX_KEYPAD_FUNC_AUX2            0x3e0e
#define NX_KEYPAD_FUNC_START_SOUNDER   0x3e0f
  

typedef unsigned char uchar;
typedef unsigned short int ushort;
typedef unsigned int uint;


typedef struct nxmsg {
  uchar len;
  uchar msgnum;
  uchar msg[255];
  uchar sum1;
  uchar sum2;

  time_t r_time;
  time_t s_time;
} nxmsg_t;


typedef struct nx_log_event_type {
  uchar type;
  char valtype;  /* Z = Zone, U = User, D = Device, N = None */
  char partition; /* 0 = No, 1 = Yes */
  char *description;
} nx_log_event_type_t;


typedef struct nx_log_event {
  uchar msgno;  /* message number (0x0a) */
  uchar no;     /* event number */
  uchar logsize;/* max (last) event number */
  uchar type;   /* event type */
  uchar num;    /* zone / user / device number */
  uchar part;   /* partition number (0=partition 1, ...) */
  uchar month;  /* month (1-12) */
  uchar day;    /* day (1-31) */
  uchar hour;   /* hour (0-23) */
  uchar min;    /* minute (0-59) */

  time_t last_updated; /* timestamp when received from panel */
} nx_log_event_t;



extern const nx_log_event_type_t nx_log_event_types[];


int nx_read_packet(int fd, nxmsg_t *msg, int protocol);
int nx_write_packet(int fd, nxmsg_t *msg, int protocol);
void nx_print_msg(FILE *fp, nxmsg_t *msg);
int nx_receive_message(int fd, int protocol, nxmsg_t *msg, int timeout);
int nx_send_message(int fd, int protocol, nxmsg_t *msg, int timeout, int retry, unsigned char replycmd, nxmsg_t *replymsg);
const char* nx_timestampstr(time_t t);
const char* nx_log_event_str(const nx_log_event_t *event);

void logmsg(int priority, char *format, ...);


#endif

/* eof :-) */
