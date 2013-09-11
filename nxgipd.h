/* nxgipd.h
 *
 * Copyright (C) 2011 Timo Kokkonen. All Rights Reserved.
 */

#ifndef _NXGIPD_H
#define _NXGIPD_H 1

#include "nx-584.h"

#define PRGNAME "nxgipd"

#define SHMVERSION "42.1"

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
  
  char buzzer_on;
  char siren_on;
  char steadysiren_on;
  char chime_on;
  char errorbeep_on;
  char tone_on;

  char zones_bypassed;

  time_t last_updated;
} nx_partition_status_t;

typedef struct nx_zone_status {
  char valid;
  char name[NX_ZONE_NAME_MAXLEN+1];
  char fault;
  char bypass;
  char trouble;
  char alarm_mem;

  time_t last_updated;
  time_t update_interval;
} nx_zone_status_t;

typedef struct nx_system_status {
  uchar panel_id;
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
} nx_system_status_t;


typedef struct nx_configuration {
  char *serial_device;
  char *serial_speed;
  uchar serial_protocol;
  uchar zones;
  uchar partitions;
  int   timesync;
  int   statuscheck;

  int   syslog_mode;
  int   debug_mode;
  char *log_file;

  uint  shmkey;
  int   shmmode;
} nx_configuration_t;



typedef struct nx_shm {
  char                   shmversion[8];
  pid_t                  pid;
  time_t                 last_updated;
  int                    comm_fail;
  nx_interface_status_t  intstatus;
  nx_system_status_t     alarmstatus;  
  time_t                 daemon_started;
} nx_shm_t;

extern nx_configuration_t *config;
extern char *program_name;

/* misc.c */
void die(char *format, ...);
void warn(char *format, ...);
void logmsg(int priority, char *format, ...);
int openserialdevice(const char *device, const char *speed);
const char* timestampstr(time_t t);

/* configuration.c */
int load_config(const char *configxml, nx_configuration_t *config, int logtest);

/* probe.c */
int read_config(int fd, int protocol, uchar node, int location);
int probe_bus(int fd, int protocol);

/* process.c */
void process_message(nxmsg_t *msg, int init_mode, int verbose_mode, nx_system_status_t *astat, nx_interface_status_t *istatus);


#endif

/* eof :-) */
