/* misc.c
 *
 * Copyright (C) 2011 Timo Kokkonen.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <syslog.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "nxgipd.h"


char *program_name = NULL;

void die(char *format, ...)
{
  va_list args;

  fprintf(stderr, "%s: ",(program_name?program_name:PRGNAME));
  va_start(args,format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr,"\n");
  fflush(stderr);
  exit(1);
}


void warn(char *format, ...)
{
  va_list args;

  fprintf(stderr, "%s: ",(program_name?program_name:PRGNAME));
  va_start(args,format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr,"\n");
  fflush(stderr);
}



const char* timestampstr(time_t t)
{
  static char str[32];
  struct tm tm;

  *str=0;
  localtime_r(&t,&tm);
  snprintf(str,sizeof(str)-1,"%04d-%02d-%02d %02d:%02d:%02d",
           tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,
           tm.tm_hour,tm.tm_min,tm.tm_sec);
  str[sizeof(str)-1]=0;
  return str;
}





void logmsg(int priority, char *format, ...)
{
  va_list args;
  char buf[256];
  FILE *fp;

  va_start(args,format);
  vsnprintf(buf,sizeof(buf),format,args);
  va_end(args);

  // if (priority > debug_mode) return;

  if (priority < 1)
    fprintf(stderr,"%s\n",buf);

  if (priority <= config->syslog_mode) {
    openlog((program_name?program_name:PRGNAME),LOG_PID,LOG_USER);
    syslog((priority>0?LOG_INFO:LOG_NOTICE),"%s",buf);
    closelog();
  }

  if (config->log_file && priority <= config->debug_mode) {
    fp=fopen(config->log_file,"a");
    if (fp) {
      fprintf(fp,"%s: %s\n",timestampstr(time(NULL)),buf);
      fclose(fp);
    }
  }
}



int openserialdevice(const char *device, const char *speed)
{
  int fd;
  int i;
  struct termios t;
  speed_t spd;

  fd = open(device,O_RDWR|O_NONBLOCK);
  if (fd < 0) die("failed to open %s (errno=%d)\n",device,errno);

  if (tcgetattr(fd,&t)) die("tcgetattr() failed on %s\n",device);

  if (sscanf(speed,"%d",&i) != 1) die("invalid serial speed setting: %s",speed);

  switch (i) {
  case 19200:
    spd=B19200;
    break;
  case 38400:
    spd=B38400;
    break;
  case 57600:
    spd=B57600;
    break;
  case 115200:
    spd=B115200;
    break;
    
  default: 
    spd=B9600;
  }

  cfmakeraw(&t); 
  cfsetspeed(&t,spd);
  t.c_cflag |= CLOCAL;

  if (tcsetattr(fd,TCSANOW,&t)) die("tcsetattr() failed on %s\n",device);

  return fd;
}



/* eof :-) */
