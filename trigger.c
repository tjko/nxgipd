/* trigger.c - handle invoking trigger program for alarm events
 *
 * Copyright (C) 2009-2015 Timo Kokkonen.
 * All Rights Reserved.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "nxgipd.h"


extern char **environ;


#define MAX_ENV_ENTRIES 64

void run_trigger_program(const char **envv)
{
  const char *env[MAX_ENV_ENTRIES+1];
  char *argv[2];
  const char **e;
  int envc = 0;
  pid_t pid;
  int fd;

  logmsg(3,"run_trigger_program() called");


  /* filter out environment for the trigger program */
  e=(const char **)environ;
  while (*e) {
    if (!strncmp("PATH=",*e,5) ||
	!strncmp("SHELL=",*e,6) ||
	!strncmp("USER=",*e,5) ||
	!strncmp("PWD=",*e,4) ||
	!strncmp("HOME=",*e,5) ||
	!strncmp("LANG=",*e,5)
	) {
      if (envc < MAX_ENV_ENTRIES) env[envc++]=*e;
    }
    e++;
  }

  /* add the alarm event environment variables */
  e=envv;
  while (*e) {
    if (envc < MAX_ENV_ENTRIES) env[envc++]=*e;
    e++;
  }

  env[envc]=NULL;
  argv[0]=config->alarm_program;
  argv[1]=NULL;
  

  pid=fork();
  if (pid < 0) {
    logmsg(0,"run_trigger_program(): fork failed: %d (%s)",errno,strerror(errno));
  } 
  else if (pid == 0) {
    /* this is the child process */

    /* make sure (somewhat) that file descriptors dont "leak"... */
    if ((fd = open("/dev/null",O_RDWR)) < 0) _exit(-1);
    dup2(fd,0);
    dup2(fd,1);
    dup2(fd,2);
    close(fd);
    /* hack: try closing first few file descriptors to avoid anything leaking...
       maybe someday Linux will have closefrom() ... */
    for (fd=3; fd<32; fd++) close(fd);

    if (execvpe((const char *)config->alarm_program,(char *const*)argv,(char *const*)env) < 1) {
      logmsg(0,"run_trigger_program(): excecvpe(%s,...) failed %d (%s)",
	     config->alarm_program,errno,strerror(errno));
    }  
  }
  else {
    logmsg(3,"trigger (child) process created: pid=%u", pid);
  }

}






#define MAX_TRIG_ENV 32

#define BUF_snprintf(env,envsize,envc,tmp, ...) 	\
    if (envc < envsize &&				\
	snprintf(tmp,sizeof(tmp),__VA_ARGS__) >= 0) {	\
      if ((env[envc]=strdup(tmp)) != NULL) envc++;	\
    }							



void run_zone_trigger(int zonenum,const char* zonename, int fault, int bypass, int trouble,
		       int tamper, int armed, const char* zonestatus)
{
  char* env[MAX_TRIG_ENV+1];
  int envc = 0;
  char tmp[255],**e;

  logmsg(3,"run_zone_trigger() called");

  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_TYPE=%s","zone");
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_STATUS=%s",zonestatus);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_ZONE=%d",zonenum);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_ZONE_NAME=%s",zonename);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_ZONE_FAULT=%d",fault);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_ZONE_TROUBLE=%d",trouble);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_ZONE_TAMPER=%d",tamper);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_ZONE_BYPASS=%d",bypass);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_ZONE_ARMED=%d",armed);
  env[envc]=NULL;

  run_trigger_program((const char**)env);

  /* free the strings allocated earlier */
  e=env;
  while (*e) {
    free(*e);
    e++;
  }

}


void run_partition_trigger(int partnum, const char* partitionstatus,int armed, int ready,
			   int stay, int chime, int entryd, int exitd, int palarm)
{
  char* env[MAX_TRIG_ENV+1];
  int envc = 0;
  char tmp[255],**e;

  logmsg(3,"run_partition_trigger() called");

  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_TYPE=%s","partition");
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_STATUS=%s",partitionstatus);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_PARTITION=%d",partnum);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_PARTITION_ARMED=%d",armed);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_PARTITION_READY=%d",ready);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_PARTITION_STAY=%d",stay);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_PARTITION_CHIME=%d",chime);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_PARTITION_ENTRY=%d",entryd);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_PARTITION_EXIT=%d",exitd);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_PARTITION_ALARM=%d",palarm);
  env[envc]=NULL;

  run_trigger_program((const char**)env);


  /* free the strings allocated earlier */
  e=env;
  while (*e) {
    free(*e);
    e++;
  }

}


void run_log_trigger(nx_log_event_t *log)
{
  char* env[MAX_TRIG_ENV+1];
  int envc = 0;
  char tmp[255],**e;

  logmsg(3,"run_log_trigger() called");

  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_TYPE=%s","log");
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_STATUS=%s",nx_log_event_text(log->type));
  if (nx_log_event_partinfo(log->type)) 
    BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_PARTITION=%d",log->part+1);
  
  switch (nx_log_event_valtype(log->type)) {
  case 'Z':
    BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_ZONE=%d",log->num+1);
    break;
  case 'U':
    BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_USER=%d",log->num);
    break;
  case 'D':
    BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_DEVICE=%d",log->num);
    break;

  default:
    break;
  }

  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_TYPE=%d",log->type);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_NUM=%d",log->no+1);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_LOGSIZE=%d",log->logsize);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_MONTH=%d",log->month);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_DAY=%d",log->day);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_HOUR=%02d",log->hour);
  BUF_snprintf(env,MAX_TRIG_ENV,envc,tmp,"ALARM_EVENT_LOG_MIN=%02d",log->min);
  env[envc]=NULL;

  run_trigger_program((const char**)env);

  /* free the strings allocated earlier */
  e=env;
  while (*e) {
    free(*e);
    e++;
  }

}



/* eof :-) */
