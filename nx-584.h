/* nx-584.h
 *
 * Library of functions to implement NX-584 Protocol.
 * 
 * Copyright (C) 2009-2016 Timo Kokkonen <tjko@iki.fi>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301, USA. 
 *
 */

#ifndef NX_584_H
#define NX_584_H 1

#define NX_PROTOCOL_BINARY    0x00
#define NX_PROTOCOL_ASCII     0x01

#define NX_MSG_MASK           0x3f
#define NX_MSG_ACK_FLAG       0x80
#define NX_IS_ACKMSG(n)  ((n & NX_MSG_ACK_FLAG) == NX_MSG_ACK_FLAG)
#define NX_IS_NONREPORTING_EVENT(type)  ( (type & 0x80) == 0x80 ? 1 : 0 ) 
#define NX_IS_REPORTING_EVENT(type)  ( (type & 0x80) == 0x80 ? 0 : 1 ) 
#define NX_EVENT_TYPE_MASK    0x7f

#define NX_PARTITIONS_MAX        8
#define NX_ZONES_MAX             256
#define NX_ZONE_NAME_MAXLEN      16
#define NX_MAX_LOG_ENTRIES       256
#define NX_LOGICAL_LOCATION_MAX  4095
#define NX_BUS_ADDRESS_MAX       255
#define NX_NO_USER               98 

/* module numbers */
#define NX_MODULE_CONTROL_PANEL         8
#define NX_MODULE_NX540			40
#define NX_MODULE_NX2192E		44
#define NX_MODULE_NX534			64
#define NX_MODULE_NX584E                72
#define NX_MODULE_NX582			77
#define NX_MODULE_NX592E                78
#define NX_MODULE_NX590E		79
#define NX_MODULE_NX320			84
#define NX_MODULE_KEYPAD1		192
#define NX_MODULE_KEYPAD2		200
#define NX_MODULE_KEYPAD3		208
#define NX_MODULE_KEYPAD4		216
#define NX_MODULE_KEYPAD5		224
#define NX_MODULE_KEYPAD6		232
#define NX_MODULE_KEYPAD7		240
#define NX_MODULE_KEYPAD8		248
#define NX_MODULE_VIRTUAL_KEYPAD	248

/* formula to calculate keypad address: num = 1..8, partition = 1..8 */
#define NX_MODULE_KEYPAD(num,partition)  (NX_MODULE_KEYPAD1 + (num-1)*8 + (partition-1))

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



#define NX_KEYPAD_FUNC_NEED_PIN(mask)  ( (mask >> 8) == 0x3c ? 1 : 0 ) 

/* masks for "keypad" functions (command / function) */
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


/* keypad terminal mode key mapping */
#define NX_KEYPAD_KEY_0                0x00
#define NX_KEYPAD_KEY_1                0x01
#define NX_KEYPAD_KEY_2                0x02
#define NX_KEYPAD_KEY_3                0x03
#define NX_KEYPAD_KEY_4                0x04
#define NX_KEYPAD_KEY_5                0x05
#define NX_KEYPAD_KEY_6                0x06
#define NX_KEYPAD_KEY_7                0x07
#define NX_KEYPAD_KEY_8                0x08
#define NX_KEYPAD_KEY_9                0x09
#define NX_KEYPAD_KEY_STAY             0x0a
#define NX_KEYPAD_KEY_CHIME            0x0b
#define NX_KEYPAD_KEY_EXIT             0x0c
#define NX_KEYPAD_KEY_BYPASS           0x0d
#define NX_KEYPAD_KEY_CANCEL           0x0e
#define NX_KEYPAD_KEY_FIRE             0x0f
#define NX_KEYPAD_KEY_MEDIC            0x10
#define NX_KEYPAD_KEY_POLICE           0x11
#define NX_KEYPAD_KEY_ASTERISK         0x12
#define NX_KEYPAD_KEY_HASH             0x13
#define NX_KEYPAD_KEY_UP               0x14
#define NX_KEYPAD_KEY_DOWN             0x15


/* X-10 Message Functions */
#define NX_X10_ALL_UNITS_OFF           0x08
#define NX_X10_ALL_LIGHTS_ON           0x18
#define NX_X10_ON                      0x28
#define NX_X10_OFF                     0x38
#define NX_X10_DIM                     0x48
#define NX_X10_BRIGHT                  0x58
#define NX_X10_ALL_LIGHTS_OFF          0x68


/* Program Data data types */

#define NX_PROG_DATA_BIN               0x00
#define NX_PROG_DATA_DEC               0x01
#define NX_PROG_DATA_HEX               0x02
#define NX_PROG_DATA_ASCII             0x03

 

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


typedef struct nx_panel_model {
  int id;
  const char* name;
  uchar max_zones;
  uchar max_partitions;
} nx_panel_model_t;



extern const nx_panel_model_t nx_panel_models[];
extern const nx_log_event_type_t nx_log_event_types[];


int nx_read_packet(int fd, nxmsg_t *msg, int protocol);
int nx_write_packet(int fd, nxmsg_t *msg, int protocol);
void nx_print_msg(FILE *fp, nxmsg_t *msg);
int nx_receive_message(int fd, int protocol, nxmsg_t *msg, int timeout);
int nx_send_message(int fd, int protocol, nxmsg_t *msg, int timeout, int retry, unsigned char replycmd, nxmsg_t *replymsg);
const char* nx_timestampstr(time_t t);
const char* nx_log_event_str(const nx_log_event_t *event);
const char* nx_log_event_text(uchar eventnum);
int nx_log_event_partinfo(uchar eventnum);
char nx_log_event_valtype(uchar eventnum);
const char* nx_prog_datatype_str(uchar datatype);


void logmsg(int priority, char *format, ...);


#endif /* NX_584_H */

/* eof :-) */
