/* nxgipd.c
 * 
 * Copyright (C) 2009,2013 Timo Kokkonen. All Rights Reserved.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#if HAVE_GETOPT_H && HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "getopt.h"
#endif


#define DEBUG 0

#include "nx-584.h"
#include "nxgipd.h"

int verbose_mode = 0;

nx_shm_t *shm = NULL;
int shmid = -1;
nx_interface_status_t *istatus;
nx_system_status_t *astat;
nx_configuration_t configuration;
nx_configuration_t *config = &configuration;



int dump_log(int fd, int protocol, nx_system_status_t *astat, nx_interface_status_t *istatus)
{
  int ret;
  nxmsg_t msgout,msgin;
  int i = 0;
  int last = 255;

  if ((istatus->sup_cmd_msgs[1] & 0x04) == 0) {
    logmsg(0,"Log Event Request command not supported. Skipping log dump.");
    return -1;
  }


  while (i < last) {
    msgout.msgnum=NX_LOG_EVENT_REQ;
    msgout.len=2;
    msgout.msg[0]=i;
    ret=nx_send_message(fd,protocol,&msgout,5,3,NX_LOG_EVENT_MSG,&msgin);
    if (ret==1 && msgin.msgnum == NX_LOG_EVENT_MSG) {
      process_message(&msgin,0,0,astat,istatus);
      last=msgin.msg[1];
      printf("."); 
      fflush(stdout);
    } else {
      fprintf(stderr,"failed to get log entry: %d",i);
    } 
    i++;
  }

  printf("\n");
  return 0;
}


int get_system_status(int fd, int protocol)
{
  int ret;
  nxmsg_t msgout,msgin;
  int i;

  for (i=0;i<NX_PARTITIONS_MAX;i++) {
    astat->partitions[i].valid=-1;
  }

  astat->last_partition=((config->partitions > 0 && config->partitions < NX_PARTITIONS_MAX) ? 
			 config->partitions : NX_PARTITIONS_MAX);

  astat->statuscheck_interval = (config->statuscheck > 0 ? config->statuscheck : 30);
  astat->timesync_interval = config->timesync;

  if ( (astat->timesync_interval > 0) &&
       ((istatus->sup_cmd_msgs[3] & 0x08) == 0) ) {
    logmsg(0,"Set Clock / Calendar command not enabled. Disabling clock sync.");
    astat->timesync_interval=0;
  }


  /* make sure that basic commands are enabled in NX interface */
  if ((istatus->sup_cmd_msgs[1] & 0x01) == 0) die("System Status Request command not enabled");
  if ((istatus->sup_cmd_msgs[0] & 0x80) == 0) die("Partitions Snapshot Request command not enabled");
  if ((istatus->sup_cmd_msgs[0] & 0x40) == 0) die("Partition Status Request command not enabled");
  if ((istatus->sup_cmd_msgs[0] & 0x08) == 0) die("Zone Name Request command not enabled");
  if ((istatus->sup_cmd_msgs[0] & 0x10) == 0) die("Zone Status Request command not enabled");


  /* get alarm system status */
  msgout.msgnum=NX_SYS_STATUS_REQ;
  msgout.len=1;
  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_SYS_STATUS_MSG,&msgin);
  if (!(ret == 1 && msgin.msgnum == NX_SYS_STATUS_MSG)) return -1;
  process_message(&msgin,0,0,astat,istatus);


  /* get partition statuses */
  msgout.msgnum=NX_PART_SNAPSHOT_REQ;
  msgout.len=1;
  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_PART_SNAPSHOT_MSG,&msgin);
  if (!(ret == 1 && msgin.msgnum == NX_PART_SNAPSHOT_MSG)) return -2;
  process_message(&msgin,0,0,astat,istatus);

  for(i=0;i<astat->last_partition;i++) {
    if (astat->partitions[i].valid) {
      msgout.msgnum=NX_PART_STATUS_REQ;
      msgout.len=2;
      msgout.msg[0]=i;
      ret=nx_send_message(fd,protocol,&msgout,5,3,NX_PART_STATUS_MSG,&msgin);
      if (!(ret == 1 && msgin.msgnum == NX_PART_STATUS_MSG)) return -3;
      process_message(&msgin,1,0,astat,istatus);
    }
  }


 
  /* get zone info & names */
  astat->last_zone=((config->zones > 0 && config->zones < NX_ZONES_MAX) ? config->zones : 48);

  for (i=0;i<astat->last_zone;i++) {
    astat->zones[i].valid=1;

    msgout.msgnum=NX_ZONE_NAME_REQ;
    msgout.len=2;
    msgout.msg[0]=i;
    ret=nx_send_message(fd,protocol,&msgout,5,3,NX_ZONE_NAME_MSG,&msgin);
    if (ret != 1 || msgin.msgnum != NX_ZONE_NAME_MSG) return -4;
    process_message(&msgin,1,0,astat,istatus);

    msgout.msgnum=NX_ZONE_STATUS_REQ;
    msgout.len=2;
    msgout.msg[0]=i;
    ret=nx_send_message(fd,protocol,&msgout,5,3,NX_ZONE_STATUS_MSG,&msgin);
    if (ret != 1 || msgin.msgnum != NX_ZONE_STATUS_MSG) return -5;
    process_message(&msgin,1,0,astat,istatus);

    printf(".");
    fflush(stdout);
  }
  printf("\n");

  for (i=0;i<astat->last_zone;i++) {
    printf("Zone %2d %16s %10s %8s\n",i+1,astat->zones[i].name,
	   (astat->zones[i].bypass?"Bypassed":"Active"),(astat->zones[i].fault?"Fault":"OK"));
    logmsg(1,"Zone %2d %16s %10s %8s",i+1,astat->zones[i].name,
	   (astat->zones[i].bypass?"Bypassed":"Active"),(astat->zones[i].fault?"Fault":"OK"));

  }

    
  return 0;
}



int init_shared_memory(int shmkey, int shmmode, size_t size, int *shmidptr, nx_shm_t **shmptr)
{
  int id;
  void *seg;
  

  id = shmget(shmkey,size,(shmmode & 0x1ff)|IPC_CREAT|IPC_EXCL);
#if 0
  if (id < 0 && errno==EEXIST) {
    fprintf(stderr,"Shared memory segment (key=%x) already exists\n",shmkey);
    id = shmget(shmkey,1,0);
    if (id >= 0) {
      fprintf(stderr,"Trying to remove existing shared memory segment...\n");
      shmctl(id,IPC_RMID,NULL);
      id = shmget(shmkey,size,(shmmode & 0x1ff)|IPC_CREAT|IPC_EXCL);
    }
  }
#endif

  if (id < 0) {
    if (errno==EEXIST) {
      fprintf(stderr,"Shared memory segment (key=%x) already exists\n",shmkey);
      fprintf(stderr,"Another instance still running?\n");
    }
    else if (errno==EINVAL) 
      fprintf(stderr,"Shared memory segment (key=%x) already exists with wrong size (?)\n",shmkey);
    else
      fprintf(stderr,"shmget() failed: %s (%d)\n",strerror(errno),errno);
    return -1; 
  }
  *shmidptr=id;

  seg = shmat(id,NULL,0);
  if (seg == (void*)-1) { 
    fprintf(stderr,"shmat() failed: %s (%d)\n",strerror(errno),errno);
    return -1;
  }

  *shmptr=seg;
  strncpy((*shmptr)->shmversion,SHMVERSION,sizeof((*shmptr)->shmversion));
  (*shmptr)->pid=getpid();
  (*shmptr)->last_updated=0;
  return 0;
}


void release_shared_memory(int shmid, void *shmseg)
{
  if (shmseg != NULL) {
    if (shmdt(shmseg) < 0)
      fprintf(stderr,"shmdt() failed: %s errno=%d\n",strerror(errno),errno);
  }

  if (shmctl(shmid,IPC_RMID,NULL) < 0)
    fprintf(stderr,"failed to delete shmid (%d): %s (errno=%d)\n",shmid,strerror(errno),errno);
}


void signal_handler(int sig)
{
  fprintf(stderr,"program terminated (%d)\n",sig);
  logmsg(0,"program terminated (%d)",sig);
  exit(1);
}

void exit_cleanup()
{
  if (shm != NULL) 
    release_shared_memory(shmid,shm);
}


int main(int argc, char **argv)
{
  int fd;
  nxmsg_t msgin,msgout;
  int ret, retry;
  int opt_index = 0;
  int opt;
  int scan_mode = 0;
  int scan_node = 0;
  int scan_loc = -1;
  int log_mode = 0;
  int daemon_mode = 0;
  char *config_file = CONFIG_FILE;
  char *pid_file = NULL;
  struct sigaction sigact;
  struct option long_options[] = {
    {"config",1,0,'c'},
    {"daemon",0,0,'d'},
    {"help",0,0,'h'},
    {"log",0,0,'l'},
    {"log-only",0,0,'L'},
    {"pid",1,0,'p'},
    {"probe",0,0,'P'},
    {"scan",1,0,'s'},
    {"status",0,0,'S'},
    {"verbose",0,0,'v'},
    {"version",0,0,'V'},
    {NULL,0,0,0}
  };


  umask(022);

  while ((opt=getopt_long(argc,argv,"vVhc:dp:l",long_options,&opt_index)) != -1) {
    switch (opt) {
      
    case 'c':
      config_file=strdup(optarg);
      break;

    case 'p':
      pid_file=strdup(optarg);
      break;

    case 'd':
      daemon_mode=1;
      break;

    case 'v':
      verbose_mode=1;
      break;

    case 'V':
      fprintf(stderr,"%s v%s  %s\nCopyright (C) 2009,2013 Timo Kokkonen. All Rights Reserved.\n",
	      PRGNAME,VERSION,HOST_TYPE);
      exit(0);
      
    case 's':
      if (sscanf(optarg,"%d,%d",&scan_node,&scan_loc)==2) {
	scan_mode=1;
	if (scan_loc < 0 || scan_loc > 1024) die("invalid location number: %d",scan_loc);
      }
      else if (sscanf(optarg,"%d",&scan_node)==1) {
	scan_mode=1;
	scan_loc=-1;
      }
      if (scan_node < 0 || scan_node > 255) die("invalid module number: %d",scan_node);
      break;

    case 'S':
      scan_mode=2;
      break;

    case 'P':
      scan_mode=3;
      break;

    case 'l':
      log_mode=1;
      break;

    case 'L':
      log_mode=2;
      break;
      
    case 'h':
    default:
      fprintf(stderr,"Usage: %s [OPTIONS]\n\n",PRGNAME);
      fprintf(stderr,
	      "  --config=<configfile>, -c <configfile>\n"
	      "                          use specified config file\n"
	      "  --pid=<pidfile>, -p <pidfile>\n"
	      "                          save process pid in file\n"
	      "  --daemon, -d            run as background daemon process\n"
	      "  --help, -h              display this help and exit\n"
	      "  --log                   dump panel log when starting\n"
	      "  --log-only              dump panel log and exit\n"
	      "  --probe                 probe bus for modules and exit\n"
	      "  --scan=<module>         dump full config of a module and exit\n"
	      "  --scan=<module>,<loc>   dump single config location of a module and exit\n"
	      "  --status                display NX gateway status/settings\n"
	      "  --verbose, -v           enable verbose output to stdout\n"
	      "  --version, -V           print program version\n"
	      "\n");
      exit(1);
    }
  }


  printf("Loading configuration...\n");
  if (load_config(config_file,config,1))
    die("failed to open configuration file: %s",config_file);

  /* setup signal handling */
  sigact.sa_handler=signal_handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags=0;
  sigaction(SIGTERM,&sigact,NULL);
  sigaction(SIGINT,&sigact,NULL);
  sigact.sa_handler=SIG_IGN;
  sigaction(SIGHUP,&sigact,NULL);

  atexit(exit_cleanup);

  /* initialize shared memory segment */
  if (init_shared_memory(config->shmkey,config->shmmode,sizeof(nx_shm_t),&shmid,&shm))
    die("failed to initalice IPC shared memory segment");
  istatus=&shm->intstatus;
  astat=&shm->alarmstatus;



  printf("Opening device %s\n",config->serial_device);
  fd = openserialdevice(config->serial_device,config->serial_speed);



  printf("Establishing communications...\n");

  /* clear anypending messages */
  do {
    ret=nx_receive_message(fd,config->serial_protocol,&msgin,1);
  } while (ret==1);

  /* request interface configuration */
  msgout.msgnum=NX_INT_CONFIG_REQ;
  msgout.len=1;
  retry=0;
  do {
    ret=nx_send_message(fd,config->serial_protocol,&msgout,5,2,NX_INT_CONFIG_MSG,&msgin);
    if (ret < 0) printf("Failed to send message\n");
    if (ret == 0) printf("No response from NX device\n");
  } while (ret != 1 && retry++ < 3);
  if (ret == 1) {
    if (msgin.msgnum != NX_INT_CONFIG_MSG) die("NX device refused the command\n");
    if (scan_mode==2) {
      nx_print_msg(stdout,&msgin);
      exit(0);
    }
  } else {
    die("failed to estabilish communications with NX device");
  }

  process_message(&msgin,1,0,astat,istatus);
  printf("Interface version v%s detected\n",istatus->version);



  if (scan_mode > 0) {
    if ( (istatus->sup_cmd_msgs[2] & 0x01) == 0 ) 
      die("Program Data Request command not enabled.");

    if (scan_mode==1) {
      read_config(fd,config->serial_protocol,scan_node,scan_loc);
      exit(0);
    }
    else if (scan_mode==3) {
      probe_bus(fd,config->serial_protocol);
      exit(0);
    }
  }




  /* spawn daemon */
  if (daemon_mode) {
    int fd;
    pid_t pid;

    if (chdir("/") < 0) die("cannot access root directory");

    pid = fork();
    if (pid < 0) die("fork() failed");
    if (pid > 0) {
      printf("Daemon started (pid=%d)...\n",pid);
      _exit(0); /* avoid running atexit functions... */
    }
    
    setsid();
    
    if ((fd = open("/dev/null",O_RDWR)) < 0) die("cannot open /dev/null");
    dup2(fd,0);
    dup2(fd,1);
    dup2(fd,2);
    close(fd);
  }

  shm->pid=getpid();
  
  if (pid_file) {
    FILE *fp = fopen(pid_file,"w");
    if (!fp) die("failed to create pid file: %s",pid_file);
    fprintf(fp,"%d\n",shm->pid);
    fclose(fp);
  }


  logmsg(0,"program started");
  logmsg(1,"NX interface version v%s detected",istatus->version);

  if (log_mode > 0) {
    printf("Dumping panel log...");
    dump_log(fd,config->serial_protocol,astat,istatus);
    if (log_mode == 2) exit(0);
  }


  printf("Getting system status...");
  retry=0;
  while (1) {
    ret=get_system_status(fd,config->serial_protocol);
    printf("\n");
    if (ret < 0) {
      printf("failed to get system status: %d\n",ret);
      if (retry++ >= 3) die("communication problem, giving up");
      sleep(1);
    } else { 
      break;
    }
  }



  printf("Waiting for messages...\n");
  logmsg(0,"waiting for messages");
  shm->daemon_started=time(NULL);
  shm->last_updated=time(NULL);

  while (1) {
    ret=nx_receive_message(fd,config->serial_protocol,&msgin,3);
    if (ret < -1) {
      printf("error reading message\n");
      logmsg(1,"error reading message");
    } else if (ret == -1) {
      printf("invalid message received\n");
      logmsg(1,"invalid message received");
    } else if (ret == 1) {
      if (verbose_mode) printf("got message %02x!\n",msgin.msgnum & NX_MSG_MASK);
      //logmsg(3,"got message %02x",msgin.msgnum & NX_MSG_MASK);
      process_message(&msgin,0,verbose_mode,astat,istatus);
    } else {
      /* printf("timeout\n"); */

      time_t t = time(NULL);

      if (astat->last_statuscheck + (astat->statuscheck_interval*60) < t) {
	msgout.msgnum=NX_SYS_STATUS_REQ;
	msgout.len=1;
	ret=nx_send_message(fd,config->serial_protocol,&msgout,5,3,NX_SYS_STATUS_MSG,&msgin);
	if (ret == 1 && msgin.msgnum == NX_SYS_STATUS_MSG) {
	  process_message(&msgin,0,verbose_mode,astat,istatus);
	  logmsg(1,"panel ok");
	  shm->comm_fail=0;
	} else {
	  fprintf(stderr,"%s: failure to communicate with panel!\n",nx_timestampstr(t));
	  logmsg(0,"failure to communicate with panel!");
	  shm->comm_fail=1;
	}
	astat->last_statuscheck=t;
      }

      if ( (astat->timesync_interval > 0) && 
	   (astat->last_timesync + (astat->timesync_interval*3600) < t) ) {
	struct tm tt;
	if (localtime_r(&t,&tt)) {
	  msgout.msgnum=NX_SET_CLOCK_CMD;
	  msgout.len=7;
	  msgout.msg[0]=(tt.tm_year % 100);
	  msgout.msg[1]=tt.tm_mon+1;
	  msgout.msg[2]=tt.tm_mday;
	  msgout.msg[3]=tt.tm_hour;
	  msgout.msg[4]=tt.tm_min;
	  msgout.msg[5]=tt.tm_wday+1;
	  logmsg(3,"setting panel time to: %02d-%02d-%02d %02d:%02d weedkay=%d",
		 msgout.msg[0],msgout.msg[1],msgout.msg[2],msgout.msg[3],
		 msgout.msg[4],msgout.msg[5]);
	  ret=nx_send_message(fd,config->serial_protocol,&msgout,5,3,NX_POSITIVE_ACK,&msgin);
	  if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK) {
	    logmsg(1,"panel clock synchronized");
	  } else {
	    logmsg(1,"failed to set panel clock");
	  }
	}
	astat->last_timesync=t;
      }
    }
    fflush(stdout);
    shm->last_updated=time(NULL);
  }


  close(fd);
  exit(0);
}

