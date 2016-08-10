/* nxgipd.h
 *
 * Copyright (C) 2011-2016 Timo Kokkonen. All Rights Reserved.
 */

#ifndef NXGIPD_H
#define NXGIPD_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "nx-584.h"
#if !HAVE_STRLCAT || !HAVE_STRLCPY
#include "strl-funcs.h"
#endif


#define PRGNAME "nxgipd"

/* shared memory version, update if shared memory locations change... */
#define SHMVERSION "42.7"

#ifndef CONFIG_FILE
#define CONFIG_FILE "/etc/nxgipd.conf"
#endif

typedef struct nx_interface_status {
  char version[5];
  char sup_trans_msgs[2];
  char sup_cmd_msgs[4];
} nx_interface_status_t;


typedef struct nx_partition_status {
  char valid;
  char ready;
  char armed;
  char stay_mode;
  char chime_mode;
  char entry_delay;
  char exit_delay;
  char prev_alarm;

  char fire;
  char fire_trouble;
  char instant;
  char tamper;
  uchar last_user;
  char valid_pin;
  char cancel_entered;
  char code_entered;
  char alarm_mem;
  char buzzer_on;
  char siren_on;
  char steadysiren_on;
  char chime_on;
  char errorbeep_on;
  char tone_on;
  char low_battery;
  char lost_supervision;
  char silent_exit;
  char alarm_sent;
  char keyswitch_armed;

  char zones_bypassed;

  time_t last_updated;
} nx_partition_status_t;

typedef struct nx_zone_status {
  char valid;
  uchar num;
  char name[NX_ZONE_NAME_MAXLEN+1];
  char fault;
  char tamper;
  char trouble;
  char bypass;
  char inhibited;
  char low_battery;
  char loss_supervision;
  char alarm_mem;
  char bypass_mem;

  uchar partition_mask;
  uchar type_flags[3];

  time_t last_tripped;
  time_t last_updated;
  time_t update_interval;
} nx_zone_status_t;

typedef struct nx_system_status {
  uchar panel_id;
  char  panel_model[16];
  uchar comm_stack_ptr;

  char line_seizure;
  char off_hook;
  char handshake_rcvd;
  char download_in_progress;
  char dialerdelay_in_progress;
  char backup_phone;
  char listen_in;
  char twoway_lockout;
  char ac_fail;
  char low_battery;
  char phone_fault;
  char ground_fault;
  char fuse_fault;
  char fail_to_comm;
  char box_tamper;
  char siren_tamper;
  char exp_tamper;
  char exp_ac_fail;
  char exp_low_battery;
  char exp_loss_supervision;
  char exp_aux_overcurrent;
  char aux_com_channel_fail;
  char exp_bell_fault;
  char sixdigitpin;
  char prog_token_inuse;
  char pin_local_dl;
  char global_pulsing_buzzer;
  char global_siren;
  char global_steady_siren;
  char bus_seize_line;
  char bus_sniff_mode;
  char battery_test;
  char ac_power;
  char low_battery_memory;
  char ground_fault_memory;
  char fire_alarm_verification;
  char smoke_power_reset;
  char line_power_50hz; 
  char high_voltage_charge;
  char comm_since_autotest;
  char powerup_delay;
  char walktest_mode;
  char system_time_loss;
  char enroll_request;
  char testfixture_mode;
  char controlshutdown_mode;
  char cancel_window;
  char callback_in_progress;
  char phone_line_fault;
  char voltage_present_int;
  char house_phone_offhook;
  char phone_monitor;
  char phone_sniffing;
  char offhook_memory;
  char listenin_request;
  char listenin_trigger;

  nx_partition_status_t partitions[NX_PARTITIONS_MAX];
  int last_partition;
  nx_zone_status_t zones[NX_ZONES_MAX]; 
  int last_zone;
  nx_log_event_t log[NX_MAX_LOG_ENTRIES];
  int last_log;

  char armed;

  time_t last_updated;

  time_t last_statuscheck;
  time_t statuscheck_interval;

  time_t last_timesync;
  time_t timesync_interval;

  time_t last_savestatus;
  time_t savestatus_interval;
} nx_system_status_t;


typedef struct nx_configuration {
  char *serial_device;
  char *serial_speed;
  uchar serial_protocol;
  int   zones;
  uchar partitions;
  int   timesync;
  int   statuscheck;

  int   syslog_mode;
  int   debug_mode;
  char *log_file;
  char *status_file;
  int   status_save_interval;

  char *alarm_program;
  uchar trigger_enable;
  uchar trigger_log;
  uchar trigger_partition;
  uchar trigger_zone;
  int   max_triggers;
  
  uint  shmkey;
  int   shmmode;
  int   shm_uid;
  int   shm_gid;
  uint  msgkey;
  int   msgmode;
  int   msg_uid;
  int   msg_gid;
} nx_configuration_t;




#define NX_IPC_MSG_DATA_LEN   256

typedef struct nx_ipc_msg {
  long msgtype;
  uint msgid[2];
  uchar data[NX_IPC_MSG_DATA_LEN];  
} nx_ipc_msg_t;

typedef struct nx_ipc_msg_reply {
  uint   msgid[2];
  time_t timestamp;
  int    result;
  char   data[256];
} nx_ipc_msg_reply_t;

/* msgtype values for nx_ipc_msg_t */
#define NX_IPC_MSG_CMD       1
#define NX_IPC_MSG_GET_PROG  2
#define NX_IPC_MSG_BYPASS    3
#define NX_IPC_MSG_MESSAGE   4
#define NX_IPC_X10_CMD       5
#define NX_IPC_MSG_SET_PROG  6
#define NX_IPC_SET_CLOCK     7

#define IPC_MSG_REPLY_TABLE_SIZE 64

typedef struct nx_shm {
  char                   shmversion[8];
  pid_t                  pid;
  time_t                 last_updated;
  int                    comm_fail;
  nx_interface_status_t  intstatus;
  nx_system_status_t     alarmstatus;  
  time_t                 daemon_started;
  char                   daemon_version[32];
  int                    reply_index;
  nx_ipc_msg_reply_t     replies[IPC_MSG_REPLY_TABLE_SIZE];
} nx_shm_t;




extern nx_configuration_t *config;
extern char *program_name;
extern int trigger_processes;

/* misc.c */
void die(char *format, ...);
void warn(char *format, ...);
void logmsg(int priority, char *format, ...);
void set_message_reply(nx_ipc_msg_reply_t *reply, const nx_ipc_msg_t *msg, int result, const char *format, ...);
int openserialdevice(const char *device, const char *speed);
const char* timestampstr(time_t t);
const char *timedeltastr(time_t delta);

/* configuration.c */
int load_config(const char *configxml, nx_configuration_t *config, int logtest);
int save_status_xml(const char *filename, nx_system_status_t *astat);
int load_status_xml(const char *filename, nx_system_status_t *astat);

/* ipc.c */
int init_shared_memory(int shmkey, int shmmode, size_t size, int *shmidptr, nx_shm_t **shmptr);
int init_message_queue(int msgkey, int msgmode);
void release_shared_memory(int shmid, void *shmseg);
void release_message_queue(int msgid);
int read_message_queue(int msgid, nx_ipc_msg_t *msg);


/* probe.c */
int read_config(int fd, int protocol, uchar node, int location);
int probe_bus(int fd, int protocol);
int detect_panel(int fd, int protocol, nx_system_status_t *astat, nx_interface_status_t *istatus, int verbose);


/* process.c */
void process_message(nxmsg_t *msg, int init_mode, int verbose_mode, nx_system_status_t *astat, nx_interface_status_t *istatus);

void process_command(int fd, int protocol, const nx_ipc_msg_t *msg,
		     nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply);
void process_keypadmsg_command(int fd, int protocol, const nx_ipc_msg_t *msg, 
			       nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply);
void process_get_program_command(int fd, int protocol, const nx_ipc_msg_t *msg, 
				 nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply);
void process_set_program_command(int fd, int protocol, const nx_ipc_msg_t *msg, 
				 nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply);
void process_zone_bypass_command(int fd, int protocol, const nx_ipc_msg_t *msg, 
				 nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply);
void process_x10_command(int fd, int protocol, const nx_ipc_msg_t *msg, 
			 nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply);

int process_set_clock(int fd, int protocol, nx_system_status_t *astat, nx_interface_status_t *istatus);
int dump_log(int fd, int protocol, nx_system_status_t *astat, nx_interface_status_t *istatus);
int get_system_status(int fd, int protocol, nx_system_status_t *astat, nx_interface_status_t *istatus);


/* trigger.c */
void  run_zone_trigger(int zonenum,const char* zonename, int fault, int bypass, int trouble,
		       int tamper, int armed, const char* zonestatus);
void run_partition_trigger(int partnum, const char* partitionstatus,int armed, int ready,
			   int stay, int chime, int entryd, int exitd, int palarm);
void run_log_trigger(nx_log_event_t *e);



#endif /* NXGIPD_H */

/* eof :-) */
