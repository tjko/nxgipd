/* nxstat.c
 *
 * Tool to display alarm status by querying nxgipd daemon. 
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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#if HAVE_GETOPT_H && HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "getopt.h"
#endif

#define DEBUG 0

#include "nxgipd.h"



#define PRINT_SUP_FLAG(name,val,flag)  printf((csv_mode?"%s,%s\n":" %40s: %s\n"), \
					      name,(val & flag ? "Enabled" : "Disabled"))
#define PRINT_FLAG(name,val) printf((csv_mode?"%s,%s\n":" %40s: %s\n"), \
				    name,(val ? "Active" : "Inactive"))

int verbose_mode = 0;

nx_shm_t *shm = NULL;
int shmid = -1;
nx_interface_status_t *istatus;
nx_system_status_t *astat;
nx_configuration_t configuration;
nx_configuration_t *config = &configuration;

int reverse_sort_order = 0;

static int sort_time_func(const void *p1, const void *p2)
{
  const nx_zone_status_t *z1 = * (const nx_zone_status_t**)p1;
  const nx_zone_status_t *z2 = * (const nx_zone_status_t**)p2;

  if (z1->last_tripped < z2->last_tripped) return (reverse_sort_order ? -1 :  1);
  if (z1->last_tripped > z2->last_tripped) return (reverse_sort_order ?  1 : -1);
  if (z1->num < z2->num) return (reverse_sort_order ?  1 : -1);
  if (z1->num > z2->num) return (reverse_sort_order ? -1 :  1);
  return 0;
}


int main(int argc, char **argv)
{
  int opt_index = 0;
  int opt;
  int log_mode = 0;
  int zones_mode = 0;
  char *config_file = CONFIG_FILE;
  int i;
  int azones = 0, apart = 0;
  int lastzone = 0;
  time_t lastzonetime = 0;
  time_t lastparttime = 0;
  time_t now;
  int interface_status = 0;
  int zone_info = -1;
  int partition_info = -1;
  int system_status = 0;
  int csv_mode = 0;
  int display_all = 0;
  int sort_time = 0;
  nx_zone_status_t* zonemap[NX_ZONES_MAX];

  struct option long_options[] = {
    {"all",0,0,'a'},
    {"config",1,0,'c'},
    {"csv",0,0,'C'},
    {"help",0,0,'h'},
    {"interface",0,0,'i'},
    {"log",2,0,'l'},
    {"partition",1,0,'p'},
    {"reverse",0,0,'r'},
    {"system",0,0,'s'},
    {"time",0,0,'t'},
    {"verbose",0,0,'v'},
    {"version",0,0,'V'},
    {"zones",0,0,'z'},
    {"zones-long",0,0,'Z'},
    {"zoneinfo",1,0,'x'},
    {NULL,0,0,0}
  };
  program_name="nxstat";

  umask(022);

  while ((opt=getopt_long(argc,argv,"aip:rstvVhCc:l::zZ",long_options,&opt_index)) != -1) {
    switch (opt) {

    case 'a':
      display_all=1;
      break;
      
    case 'c':
      config_file=strdup(optarg);
      break;

    case 'C':
      csv_mode=1;
      break;

    case 'i':
      interface_status=1;
      break;

    case 'r':
      reverse_sort_order=1;
      break;
      
    case 's':
      system_status=1;
      break;

    case 't':
      sort_time=1;
      break;

    case 'v':
      verbose_mode=1;
      break;

    case 'V':
      fprintf(stderr,"%s v%s (%s)  %s\nCopyright (C) 2009-2015 Timo Kokkonen. All Rights Reserved.\n",
	      program_name,VERSION,BUILDDATE,HOST_TYPE);
      exit(0);
      
    case 'l':
      if (optarg && sscanf(optarg,"%d",&log_mode)==1) {
	if (log_mode < 0) log_mode=0;
      } else
	log_mode=NX_MAX_LOG_ENTRIES;;
      break;

    case 'p':
      if (optarg && sscanf(optarg,"%d",&partition_info)==1) {
	if (partition_info < 1 || partition_info > NX_PARTITIONS_MAX)
	  die("invalid partition specified (valid range: 1..8)");
      }
      break;

    case 'z':
      zones_mode=1;
      break;

    case 'Z':
      zones_mode=2;
      break;

    case 'x':
      if (optarg && sscanf(optarg,"%d",&zone_info)==1) {
	if (zone_info < 1 || zone_info > NX_ZONES_MAX)
	  die("invalid zone specified for option --zoneinfo");
      }
      break;

    case 'h':
    default:
      fprintf(stderr,"Usage: %s [OPTIONS]\n\n",program_name);
      fprintf(stderr,
	      "  --all, -a               display all zones\n"
	      "  --config=<configfile>   use specified config file\n"
	      "  -c <configfile>\n"
	      "  --csv, -C               output in CSV format\n"
	      "  --help, -h              display this help and exit\n"
	      "  --interface, -i         display interface status\n"
	      "  --log, -l               display full panel log\n"
	      "  --log=<n>, -l <n>       display last n entries of panel log\n"
	      "  --partition=<b>, -p <b> display full partition status\n"
	      "  --system, -s            display full system status\n"
	      "  --time, -t              sort zones by last trigger/trouble date\n"
	      "  --verbose, -v           enable verbose output to stdout\n"
	      "  --version, -V           print program version\n"
	      "  --zones, -z             display short zone status info\n"
	      "  --zones-long, -Z        display long zone status info\n"
	      "\n");
      exit(1);
    }
  }


  if (verbose_mode) 
    printf("Loading configuration...\n");

  if (load_config(config_file,config,0))
    die("failed to open configuration file: %s",config_file);


  /* initialize shared memory segment */

  shmid = shmget(config->shmkey,sizeof(nx_shm_t),0);
  if (shmid < 0) {
    if (errno==EINVAL) 
      die("version mismatch with daemon version (shared memory segment wrong size)");
    if (errno==ENOENT)
      die("cannot find share memory segment (server not running?)");
    die("shmget() failed: %s (%d)\n",strerror(errno),errno);
  }
  shm = shmat(shmid,NULL,SHM_RDONLY);
  if (shm == (void*)-1)  
    die("shmat() failed: %d (%s)\n",errno,strerror(errno));
  istatus=&shm->intstatus;
  astat=&shm->alarmstatus;

  if (strcmp(shm->shmversion, SHMVERSION))
    die("version mismatch with daemon (shared memory) version: %s vs %s", 
	SHMVERSION, shm->shmversion);



  /* check if server process is alive... */

  if (verbose_mode) {
    printf("    Server PID: %d\n",shm->pid);
    printf("Server Version: %s\n",shm->daemon_version);
    printf("    shmversion: %s\n",shm->shmversion);
    printf("  Last Updated: %s\n",timestampstr(shm->last_updated));
    printf("\n");
  }
  if (kill(shm->pid,0) < 0 && errno != EPERM)
    die("server process not running anymore (pid=%d): %s",shm->pid,strerror(errno));
  if (shm->last_updated < 1)
    die("server process running but not fully initialized yet");
  if (shm->last_updated + (5*60) < time(NULL)) {
    printf("WARNING: server process appears have hung!\n");
  }
    


  /* check active partitions and zone... */

  for (i=0;i<astat->last_partition;i++) { 
    if (astat->partitions[i].valid) {
      apart++; 
      if (astat->partitions[i].last_updated > lastparttime)
	lastparttime=astat->partitions[i].last_updated;
    }
  }
  for (i=0;i<astat->last_zone;i++) {
    if (astat->zones[i].valid) {
      azones++;
      if (astat->zones[i].last_updated > lastzonetime) {
	lastzonetime=astat->zones[i].last_updated;
	lastzone=i;
      }
    }
  }



  /* display alarm interface status */
  if (interface_status) {
    char *c = istatus->sup_cmd_msgs;
    char *t = istatus->sup_trans_msgs;

    if (csv_mode) {
      printf("interface_feature,status\n");
    } else {
      printf("NX-584 Interface status\n\n");
      printf(" %40s: %s\n","Firmware Version",istatus->version);
      printf("Commands:\n");
    }

    PRINT_SUP_FLAG("Interface Configuration Request",c[0],0x02);
    PRINT_SUP_FLAG("Zone Name Request",c[0],0x08);
    PRINT_SUP_FLAG("Zone Status Request",c[0],0x10);
    PRINT_SUP_FLAG("Zones Snapshot Request",c[0],0x20);
    PRINT_SUP_FLAG("Partition Status Request",c[0],0x40);
    PRINT_SUP_FLAG("Partitions Snapshot Request",c[0],0x80);
    PRINT_SUP_FLAG("System Status Request",c[1],0x01);
    PRINT_SUP_FLAG("Send X-10 Message",c[1],0x02);
    PRINT_SUP_FLAG("Log Event Request",c[1],0x04);
    PRINT_SUP_FLAG("Send Keypad Text Message",c[1],0x08);
    PRINT_SUP_FLAG("Keypad Terminal Mode Request",c[1],0x10);
    PRINT_SUP_FLAG("Program Data Request",c[2],0x01);
    PRINT_SUP_FLAG("Program Data Command",c[2],0x02);
    PRINT_SUP_FLAG("User Information Request (w/PIN)",c[2],0x04);
    PRINT_SUP_FLAG("User Information Request (wo/PIN)",c[2],0x08);
    PRINT_SUP_FLAG("Set User Code Command (w/PIN)",c[2],0x10);
    PRINT_SUP_FLAG("Set User Code Command (wo/PIN)",c[2],0x20);
    PRINT_SUP_FLAG("Set User Authorization Command (w/PIN)",c[2],0x40);
    PRINT_SUP_FLAG("Set User Authorization Command (wo/PIN)",c[2],0x80);
    PRINT_SUP_FLAG("Store Communication Even Command",c[3],0x04);
    PRINT_SUP_FLAG("Set Clock / Calendar Command",c[3],0x08);
    PRINT_SUP_FLAG("Primary Keypad Function (w/PIN)",c[3],0x10);
    PRINT_SUP_FLAG("Primary Keypad Function (wo/PIN)",c[3],0x20);
    PRINT_SUP_FLAG("Secondary Keypad Function",c[3],0x40);
    PRINT_SUP_FLAG("Zone Bypass Toggle",c[3],0x80);
    
    if (!csv_mode)
      printf("Transition Messages:\n");

    PRINT_SUP_FLAG("Interface Configuration Message",t[0],0x02);
    PRINT_SUP_FLAG("Zone Status Message",t[0],0x10);
    PRINT_SUP_FLAG("Zones Snapshot Message",t[0],0x20);
    PRINT_SUP_FLAG("Partition Status Message",t[0],0x40);
    PRINT_SUP_FLAG("Partitions Snapshot Message",t[0],0x80);
    PRINT_SUP_FLAG("System Status Message",t[1],0x01);
    PRINT_SUP_FLAG("X-10 Message Received",t[1],0x02);
    PRINT_SUP_FLAG("Log Event Message",t[1],0x04);
    PRINT_SUP_FLAG("Keypad Message Received",t[1],0x04);

    if (!csv_mode)
      printf("\n");

    return 0;
  } 

  /* display (detailed) system status */
  if (system_status) {
    if (csv_mode) {
      printf("panel_feature,status\n");
    } else {
      printf("Alarm Panel Status:\n\n");
    }

    PRINT_FLAG("Line seizure",astat->line_seizure);
    PRINT_FLAG("Off Hook",astat->off_hook);
    PRINT_FLAG("Initial handshake received",astat->handshake_rcvd);
    PRINT_FLAG("Download in progress",astat->download_in_progress);
    PRINT_FLAG("Dialer delay in progress",astat->dialerdelay_in_progress);
    PRINT_FLAG("Using backup phone",astat->backup_phone);
    PRINT_FLAG("Listen-in active",astat->listen_in);
    PRINT_FLAG("Two-way lockout",astat->twoway_lockout);
    PRINT_FLAG("Ground fault",astat->ground_fault);
    PRINT_FLAG("Phone fault",astat->phone_fault);
    PRINT_FLAG("Fail to communicate",astat->fail_to_comm);
    PRINT_FLAG("Fuse fault",astat->fuse_fault);
    PRINT_FLAG("Box tamer",astat->box_tamper);
    PRINT_FLAG("Siren tamper / trouble",astat->siren_tamper);
    PRINT_FLAG("Low Battery",astat->low_battery);
    PRINT_FLAG("AC fail",astat->ac_fail);
    PRINT_FLAG("Expander box tamper",astat->exp_tamper);
    PRINT_FLAG("Expander AC failure",astat->exp_ac_fail);
    PRINT_FLAG("Expander low battery",astat->exp_low_battery);
    PRINT_FLAG("Expander loss of supervision",astat->exp_loss_supervision);
    PRINT_FLAG("Auxiliary communication channel failure",astat->aux_com_channel_fail);
    PRINT_FLAG("Expander bell fault",astat->exp_bell_fault);
    PRINT_FLAG("6-digit PINs enabled",astat->sixdigitpin);
    PRINT_FLAG("Programming token in use",astat->prog_token_inuse);
    PRINT_FLAG("PIN required for local download",astat->pin_local_dl);
    PRINT_FLAG("Global pulsing buzzer",astat->global_pulsing_buzzer);
    PRINT_FLAG("Global Siren on",astat->global_siren);
    PRINT_FLAG("Global steady siren",astat->global_steady_siren);
    PRINT_FLAG("Bus device has line seized",astat->bus_seize_line);
    PRINT_FLAG("Bus device has requested sniff mode",astat->bus_sniff_mode);
    PRINT_FLAG("Dynamic battery test",astat->battery_test);
    PRINT_FLAG("AC power on",astat->ac_power);
    PRINT_FLAG("Low battery memory",astat->low_battery_memory);
    PRINT_FLAG("Ground fault memory",astat->ground_fault_memory);
    PRINT_FLAG("Fire alarm verification being timed",astat->fire_alarm_verification);
    PRINT_FLAG("Smoke power reset",astat->smoke_power_reset);
    PRINT_FLAG("50Hz line power detected",astat->line_power_50hz);
    PRINT_FLAG("Timing a high voltage battery charge",astat->high_voltage_charge);
    PRINT_FLAG("Communication since last autotest",astat->comm_since_autotest);
    PRINT_FLAG("Power-up delay in progress",astat->powerup_delay);
    PRINT_FLAG("Walk test mode",astat->walktest_mode);
    PRINT_FLAG("Loss of system time",astat->system_time_loss);
    PRINT_FLAG("Enroll requested",astat->enroll_request);
    PRINT_FLAG("Test fixture mode",astat->testfixture_mode);
    PRINT_FLAG("Control shutdown mode",astat->controlshutdown_mode);
    PRINT_FLAG("Timing a cancel window",astat->cancel_window);
    PRINT_FLAG("Call back in progress",astat->callback_in_progress);
    PRINT_FLAG("Phone line faulted",astat->phone_line_fault);
    PRINT_FLAG("Voltage present interrupt active",astat->voltage_present_int);
    PRINT_FLAG("House phone off hook",astat->house_phone_offhook);
    PRINT_FLAG("Phone line monitor enabled",astat->phone_monitor);
    PRINT_FLAG("Sniffing",astat->phone_sniffing);
    PRINT_FLAG("Last read was off hook",astat->offhook_memory);
    PRINT_FLAG("Listen-in requested",astat->listenin_request);
    PRINT_FLAG("Listen-in trigger",astat->listenin_trigger);

    if (!csv_mode)
      printf("\n");

    return 0;
  }


  /* display detailed partition status */
  if (partition_info > 0) {
    nx_partition_status_t *p = &astat->partitions[partition_info-1];

    if (!p->valid) {
      warn("Partition %d not active",partition_info);
      return 1;
    }

    if (csv_mode) {
      printf("partition_feature,status\n");
    } else {
      printf("Partition %d Status:\n\n",partition_info);
    }

    PRINT_FLAG("Ready to arm",p->ready);
    PRINT_FLAG("Armed",p->armed);
    PRINT_FLAG("Stay Mode",p->stay_mode);
    PRINT_FLAG("Entry Delay",p->entry_delay);
    PRINT_FLAG("Exit Delay",p->exit_delay);
    PRINT_FLAG("Previous Alarm",p->prev_alarm);
    PRINT_FLAG("Alarm memory",p->alarm_mem);

    PRINT_FLAG("Fire",p->fire);
    PRINT_FLAG("Fire Trouble",p->fire_trouble);
    PRINT_FLAG("Instat Mode",p->instant);
    PRINT_FLAG("Tamper",p->tamper);
    PRINT_FLAG("Valid PIN entered",p->valid_pin);
    PRINT_FLAG("Cancel command entered",p->cancel_entered);
    PRINT_FLAG("Code netered",p->code_entered);
    PRINT_FLAG("Silent exit enabled",p->silent_exit);
    PRINT_FLAG("Pulsing Buzzer",p->buzzer_on);
    PRINT_FLAG("Siren on",p->siren_on);
    PRINT_FLAG("Steady siren on",p->steadysiren_on);
    PRINT_FLAG("Chime on",p->chime_on);
    PRINT_FLAG("Error beep (triple beep)",p->errorbeep_on);
    PRINT_FLAG("Tone on (activation tone)",p->tone_on);
    PRINT_FLAG("Sensor low battery",p->low_battery);
    PRINT_FLAG("Sensor lost supervision",p->lost_supervision);
    PRINT_FLAG("Alarm Sent",p->alarm_sent);
    PRINT_FLAG("Keyswitch armed",p->keyswitch_armed);
    PRINT_FLAG("Zone bypassed",p->zones_bypassed);

    if (csv_mode) {
      printf("Last User,%d\n",p->last_user);
    } else {
      printf("\n");
      printf(" %40s: %d\n","Last User",p->last_user);
    } 

    return 0;
  }


  /* display zone status flags */
  if (zone_info > 0) {
    nx_zone_status_t *z = &astat->zones[zone_info-1];
    uchar *f;
    if (z->valid != 1)
      die("not a valid/active zone: %d",zone_info);

    if (csv_mode) {
      printf("zone_status_flag,status\n");
      printf("Zone Name,%s\n",z->name);
      printf("Last updated,%s\n",(z->last_updated > 0 ?timestampstr(z->last_updated):"n/a"));
    } else {
      printf("Zone Status Flags:\n        Zone: %d\n",zone_info);
      printf("   Zone Name: %s\n",z->name);
      printf("Last updated: %s\n",(z->last_updated > 0 ?timestampstr(z->last_updated):"n/a"));
    }

    //printf("flags: %02x %02x %02x\n",z->type_flags[0],z->type_flags[1],z->type_flags[2]);
    f=z->type_flags;

    // NOTE! this information seems bogus...either NX-584 (v1.06) is returning bad data or bug somewhere...

    PRINT_SUP_FLAG("Fire",f[0],0x01);
    PRINT_SUP_FLAG("24 Hour",f[0],0x02);
    PRINT_SUP_FLAG("Key-switch",f[0],0x04);
    PRINT_SUP_FLAG("Follower",f[0],0x08);
    PRINT_SUP_FLAG("Entry / exit delay 1",f[0],0x10);
    PRINT_SUP_FLAG("Entry / exit delay 2",f[0],0x20);
    PRINT_SUP_FLAG("Interior",f[0],0x40);
    PRINT_SUP_FLAG("Local only",f[0],0x80);

    PRINT_SUP_FLAG("Keypad sounder",f[1],0x01);
    PRINT_SUP_FLAG("Yelping siren",f[1],0x02);
    PRINT_SUP_FLAG("Steady siren",f[1],0x04);
    PRINT_SUP_FLAG("Chime",f[1],0x08);
    PRINT_SUP_FLAG("Bypassable",f[1],0x10);
    PRINT_SUP_FLAG("Group Bypassable",f[1],0x20);
    PRINT_SUP_FLAG("Force armable",f[1],0x40);
    PRINT_SUP_FLAG("Entry guard",f[1],0x80);

    PRINT_SUP_FLAG("Fast loop response",f[2],0x01);
    PRINT_SUP_FLAG("Double EOL tamper",f[2],0x02);
    PRINT_SUP_FLAG("Trouble",f[2],0x04);
    PRINT_SUP_FLAG("Cross zone",f[2],0x08);
    PRINT_SUP_FLAG("Dialer delay",f[2],0x10);
    PRINT_SUP_FLAG("Swinger shutdown",f[2],0x20);
    PRINT_SUP_FLAG("Restorable",f[2],0x40);
    PRINT_SUP_FLAG("Listen in",f[2],0x80);

    return 0;
  }


  /* print panel event log ... */

  if (log_mode) {
    int p = astat->comm_stack_ptr;
    int size = astat->last_log;
    const char *e,*s;

    //printf("Panel Event Log: %d (%d)\n",log_mode,size);
    if (log_mode > size) log_mode=size;

    if (csv_mode)
      printf("num,logsize,type,reporting,month,day,hour,min,zone,user,description\n");

    for (i=p-log_mode+1;i<=p;i++) {
      int pos = i;

      if (pos < 0) 
	pos=size+pos; 
      if ((astat->log[pos].msgno & NX_MSG_MASK ) == NX_LOG_EVENT_MSG) {
	  nx_log_event_t *l = &astat->log[pos];
	s=nx_log_event_str(l);
	if ((e=strstr(s,": ")) != NULL) e=e+2;
	else  e=s;

	if (csv_mode) {
	  char valtype = nx_log_event_valtype(l->type);
	  s=nx_log_event_text(l->type);
	  printf("%u,%u,%u,%c,%d,%d,%d,%d,%d,%d,%s\n",l->no+1,l->logsize,
		 (l->type & NX_EVENT_TYPE_MASK),
		 (NX_IS_REPORTING_EVENT(l->type)?'Y':'N'),
		 l->month,l->day,l->hour,l->min,
		 (valtype=='Z'?l->num:-1),
		 (valtype=='U'?l->num:-1),
		 s);
	} else {
	  printf("%s\n",e);
	}
      }
    }
    return 0;
  }



  /* main status display */

  if (!csv_mode) {
    printf("NetworX Alarm Panel status\n\n");

    printf(" Active Partitions: %-12d NX-584 Firmware version: %s\n",apart,istatus->version);
    printf("      Active Zones: %-12d             Panel Model: %s\n",azones,
	   astat->panel_model);
    printf("      Phone in-use: %-12s                AC Power: %s\n",
	   (astat->off_hook?"Panel":(astat->house_phone_offhook?"House-Phone":"NO")),
	   (astat->ac_fail ? "FAIL" : (astat->ac_power?"OK":"OFF")));
    printf("            Tamper: %-12s                 Battery: %s\n",
	   (astat->box_tamper?"Panel":(astat->siren_tamper?"Siren":(astat->exp_tamper?"Expansion":"OK"))),
	   (astat->low_battery?"LOW":"OK"));
    printf("             Phone: %-12s                    Fuse: %s\n",
	   (astat->phone_fault?"Fault (Phone)":(astat->phone_line_fault?"Fault (Line)":"OK")),
	   (astat->fuse_fault?"Fail":"OK"));
    printf("    Communications: %-12s                  Ground: %s\n",
	   (astat->fail_to_comm?"FAIL":"OK"),
	   (astat->ground_fault?"FAULT":"OK"));
    printf("\n");

    printf("Part  Ready Armed  Stay Instant Chime  Fire Delay Alarm Sound\n");
    printf("----  ----- -----  ---- ------- -----  ---- ----- ----- ---------\n");
  } else {
    printf("system,active_partitions,fw_version,active_zones,panel_model,phone_in_use,ac_power," 
	   "tamper,battery,phone,fuse,communications,ground\n");
    printf("system,%d,%s,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
	   apart,istatus->version,
	   azones,astat->panel_model,
	   (astat->off_hook?"Panel":(astat->house_phone_offhook?"House-Phone":"NO")),
	   (astat->ac_fail ? "FAIL" : (astat->ac_power?"OK":"OFF")),
	   (astat->box_tamper?"Panel":(astat->siren_tamper?"Siren":(astat->exp_tamper?"Expansion":"OK"))),
	   (astat->low_battery?"LOW":"OK"),
	   (astat->phone_fault?"Fault (Phone)":(astat->phone_line_fault?"Fault (Line)":"OK")),
	   (astat->fuse_fault?"Fail":"OK"),
	   (astat->fail_to_comm?"FAIL":"OK"),
	   (astat->ground_fault?"FAULT":"OK")
	   );
    printf("\n");
    printf("partition,num,ready,armed,stay,instant,chime,fire,delay,alarm,sound\n");
  }
  
  for (i=0;i<astat->last_partition;i++) {
    nx_partition_status_t *p = &astat->partitions[i];

    if (!p->valid) continue;
    
    printf((csv_mode ? "partition,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s\n":
	               "%-4d  %-5s %-5s  %-4s %-7s %-5s  %-4s %-5s %-5s %-5s\n"),
	   i+1,
	   (p->ready?"Yes":"No"),
	   (p->armed?"Yes":"No"),
	   (p->stay_mode?"Yes":"No"),
	   (p->instant?"Yes":"No"),
	   (p->chime_mode?"Yes":"No"),
	   (p->fire?"Yes":(p->fire_trouble?"Trbl":"No")),
	   (p->entry_delay?"Entry":(p->exit_delay?"Exit":"No")),
	   (p->prev_alarm?"Yes":"No"),
	   (p->siren_on?"Siren":(p->buzzer_on?"Buzzer":(p->errorbeep_on?"ErrorBeep":(p->tone_on?"Tone":(p->chime_on?"Chime":"No")))))
	   );

  }

  if (!csv_mode)
    printf("\n");
  now=time(NULL);


  if (csv_mode && zones_mode > 0) 
    zones_mode=2;
  

  for(i=0;i<azones;i++) {
    if (reverse_sort_order) 
      zonemap[(azones-1)-i]=&astat->zones[i];
    else
      zonemap[i]=&astat->zones[i];
  }
  if (sort_time) {
    qsort(zonemap,azones,sizeof(nx_zone_status_t*),sort_time_func);
  }


  if (zones_mode==1) {
    nx_zone_status_t *zn;
    int z;
    int cols = 3;
    int rows = (azones/cols)+(azones%cols>0?1:0);

    printf("Zones:\n");
    
    for (i=0;i<azones;i++) {
      z=(i%3)*rows+(i/3);
      if (z < azones) {
	zn=zonemap[z];

	printf("%c%16s%c=%-6s ",
	       (zn->bypass?'{':'['),
	       zn->name,
	       (zn->bypass?'}':']'),
	       (zn->fault?"Fault":(zn->trouble?"Trouble":"OK"))
	       );
      } else {
	printf("zone=%18d ",z);
      }
      if (i%3==2) printf("\n") ;
    }

  }
  else if (zones_mode==2) {
    nx_zone_status_t *zn;
    char tmp[16];

    if (csv_mode) {
      printf("\nzone,num,name,mode,status,last_fault\n");
    } else {
      printf("Zone  Name              Mode      Status  Last Fault/Trouble\n"
	     "----  ----------------  --------  ------  --------------------------------\n"
	     );
    }

    for(i=0;i<azones;i++) {
      zn=zonemap[i];
      if (zn->last_tripped > 0) {
	snprintf(tmp,sizeof(tmp)-1,"(%s ago)",timedeltastr(now - zn->last_tripped));
	tmp[sizeof(tmp)-1]=0;
      } else {
	tmp[0]=0;
      }

      if (display_all || zn->last_tripped > 0)
	printf((csv_mode ? "zone,%d,%s,%s,%s,%s,%s\n" : "%02d    %-16s  %-8s  %-6s  %s %s\n"),
	       zn->num,
	       zn->name,
	       (zn->bypass?"Bypassed":"Active"),
	       (zn->fault?"Fault":(zn->trouble?"Trouble":"OK")),
	       (zn->last_tripped > 0 ? timestampstr(zn->last_tripped):"n/a"),
	       tmp
	       );
    }
  }


  if (csv_mode)
    return 0;

  if (astat->last_updated > lastparttime)
    lastparttime=astat->last_updated;

  if (astat->zones[lastzone].last_updated > shm->daemon_started) {
    printf("\n Last zone tripped: %s (%s ago) [%s]\n",
	   timestampstr(astat->zones[lastzone].last_updated),
	   timedeltastr(now - astat->zones[lastzone].last_updated),
	   astat->zones[lastzone].name);
  } else{
    printf("\n Last zone tripped: n/a\n");
  }
  printf("Last status change: %s (%s ago)\n",
	 timestampstr(lastparttime),
	 timedeltastr(now - lastparttime)
	 );


  if (verbose_mode) {
    printf("    Daemon started: %s (%s ago)\n",
	   timestampstr(shm->daemon_started),
	   timedeltastr(now - shm->daemon_started));
    printf(" Last status check: %s\n",timestampstr(astat->last_statuscheck));
    printf("   Last clock sync: %s\n",timestampstr(astat->last_timesync));
  }


  return 0;
}

