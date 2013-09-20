/* nxstat.c
 * 
 * Copyright (C) 2009,2013 Timo Kokkonen. All Rights Reserved.
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



int verbose_mode = 0;

nx_shm_t *shm = NULL;
int shmid = -1;
nx_interface_status_t *istatus;
nx_system_status_t *astat;
nx_configuration_t configuration;
nx_configuration_t *config = &configuration;



const char *timedeltastr(time_t delta)
{
  static char buf[256];
  uint d = (int)delta;

  if (delta < 60) {
    snprintf(buf,sizeof(buf)-1,"   %02ds",d);
  }
  else if (delta < 60*60){
    snprintf(buf,sizeof(buf)-1,"%02dm%02ds",(d/60),(d%60));
  } 
  else if (delta < 60*60*24) {
    snprintf(buf,sizeof(buf)-1,"%02dh%02dm",(d/(60*60)),(d%(60*60))/60);
  }
  else {
    snprintf(buf,sizeof(buf)-1,"%02dd%02dh",(d/(60*60*24)),(d%(60*60*24))/(60*60));
  }

  buf[sizeof(buf)-1]=0;
  return buf;
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

  struct option long_options[] = {
    {"config",1,0,'c'},
    {"help",0,0,'h'},
    {"log",2,0,'l'},
    {"verbose",0,0,'v'},
    {"version",0,0,'V'},
    {"zones",0,0,'z'},
    {"zones-long",0,0,'Z'},
    {NULL,0,0,0}
  };
  program_name="nxstat";

  umask(022);

  while ((opt=getopt_long(argc,argv,"vVhc:l::zZ",long_options,&opt_index)) != -1) {
    switch (opt) {
      
    case 'c':
      config_file=strdup(optarg);
      break;

    case 'v':
      verbose_mode=1;
      break;

    case 'V':
      fprintf(stderr,"%s v%s  %s\nCopyright (C) 2009,2013 Timo Kokkonen. All Rights Reserved.\n",
	      program_name,VERSION,HOST_TYPE);
      exit(0);
      
    case 'l':
      if (optarg && sscanf(optarg,"%d",&log_mode)==1) {
	if (log_mode < 0) log_mode=0;
      } else
	log_mode=NX_MAX_LOG_ENTRIES;;
      break;

    case 'z':
      zones_mode=1;
      break;

    case 'Z':
      zones_mode=2;
      break;

    case 'h':
    default:
      fprintf(stderr,"Usage: %s [OPTIONS]\n\n",program_name);
      fprintf(stderr,
	      "  --config=<configfile>   use specified config file\n"
	      "  -c <configfile>\n"
	      "  --help, -h              display this help and exit\n"
	      "  --log, -l               display full panel log\n"
	      "  --log=<n>, -l <n>       display last n entries of panel log\n"
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
      fprintf(stderr,"Shared memory segment (key=%x) already exists with wrong size (?)\n",
	      config->shmkey);
    die("shmget() failed: %s (%d)\n",strerror(errno),errno);
  }
  shm = shmat(shmid,NULL,SHM_RDONLY);
  if (shm == (void*)-1)  
    die("shmat() failed: %s (%d)\n",strerror(errno),errno);
  istatus=&shm->intstatus;
  astat=&shm->alarmstatus;


  /* check if server process is alive... */

  if (verbose_mode) {
    printf("server pid=%d\n",shm->pid);
    printf("shmversion: '%s'\n",shm->shmversion);
    printf("last updated: %s\n",timestampstr(shm->last_updated));
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



  /* print panel event log ... */

  if (log_mode) {
    int p = astat->comm_stack_ptr;
    int size = astat->last_log;
    const char *e,*s;

    //printf("Panel Event Log: %d (%d)\n",log_mode,size);
    if (log_mode > size) log_mode=size;

    for (i=p-log_mode+1;i<=p;i++) {
      int pos = i;
      if (pos < 0) { pos=size+pos; }
      //printf("%d %d : %d\n",p,i,pos);
      if ((astat->log[pos].msgno & NX_MSG_MASK ) == NX_LOG_EVENT_MSG) {
	s=nx_log_event_str(&astat->log[pos]);
	if ((e=strstr(s,": ")) != NULL) e=e+2;
	else  e=s;
	printf("%s\n",e);
      }
    }
    return 0;
  }




  printf("NX-8/NX-8V2/NX-8E Alarm Panel status\n\n");

  printf(" Active Partitions: %-12d    Interface version: %s\n",apart,istatus->version);
  printf("      Active Zones: %-12d             Panel ID: %d\n",azones,astat->panel_id);
  printf("      Phone in-use: %-12s             AC Power: %s\n",
	 (astat->off_hook?"Panel":(astat->house_phone_offhook?"House-Phone":"NO")),
	 (astat->ac_fail ? "FAIL" : (astat->ac_power?"OK":"OFF")));
  printf("            Tamper: %-12s              Battery: %s\n",
	 (astat->box_tamper?"Panel":(astat->siren_tamper?"Siren":(astat->exp_tamper?"Expansion":"OK"))),
	 (astat->low_battery?"LOW":"OK"));
  printf("             Phone: %-12s                 Fuse: %s\n",
	 (astat->phone_fault?"Fault (Phone)":(astat->phone_line_fault?"Fault (Line)":"OK")),
	 (astat->fuse_fault?"Fail":"OK"));
  printf("    Communications: %-12s               Ground: %s\n",
	 (astat->fail_to_comm?"FAIL":"OK"),
	 (astat->ground_fault?"FAULT":"OK"));

  printf("\n");

  printf("Part  Ready Armed  Stay Instant Chime  Fire Delay Alarm Sound\n");
  printf("----  ----- -----  ---- ------- -----  ---- ----- ----- ---------\n");
  
  for (i=0;i<astat->last_partition;i++) {
    nx_partition_status_t *p = &astat->partitions[i];

    if (!p->valid) continue;
    
    printf("%-4d  %-5s %-5s  %-4s %-7s %-5s  %-4s %-5s %-5s %-5s\n",
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

  printf("\n");
  now=time(NULL);

  if (zones_mode==1) {
    nx_zone_status_t *zn;
    int z;
    int cols = 3;
    int rows = (azones/cols)+(azones%cols>0?1:0);

    printf("Zones:\n");
    
    for (i=0;i<azones;i++) {
      z=(i%3)*rows+(i/3);
      if (z < azones) {
	zn=&astat->zones[z];

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
    char tmp[16],tmp2[16];

    printf("Zone  Name              Mode      Status  Last Change\n"
	   "----  ----------------  --------  ------  --------------------------------\n"
	   );

    for(i=0;i<azones;i++) {
      zn=&astat->zones[i];
      if (zn->last_updated > shm->daemon_started) {
	snprintf(tmp,sizeof(tmp)-1,"(%s ago)",timedeltastr(now - zn->last_updated));
	tmp[sizeof(tmp)-1]=0;
      } else {
	tmp[0]=0;
      }

      snprintf(tmp2,sizeof(tmp2)-1,"Zone %2d",i+1);
      tmp2[sizeof(tmp)-1]=0;
      
      if (strncmp(tmp2,zn->name,strlen(tmp2))!=0 || zn->last_updated > shm->daemon_started)
	printf("%02d    %-16s  %-8s  %-6s  %s %s\n",
	       i+1,
	       zn->name,
	       (zn->bypass?"Bypassed":"Active"),
	       (zn->fault?"Fault":(zn->trouble?"Trouble":"OK")),
	       (zn->last_updated > shm->daemon_started ? timestampstr(zn->last_updated):"n/a"),
	       tmp
	       );
    }
  }

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

  //printf(" Last status check: %s\n",timestampstr(astat->last_statuscheck));
  //printf("Last clock sync: %s\n",timestampstr(astat->last_timesync));

  return 0;
}

