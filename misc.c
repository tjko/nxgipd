/* misc.c - miscellaneous functions used by nxgipd
 *
 * 
 * Copyright (C) 2009-2015 Timo Kokkonen <tjko@iki.fi>
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


void set_message_reply(nx_ipc_msg_reply_t *reply, const nx_ipc_msg_t *msg, int result, const char *format, ...)
{
  va_list args;
  char buf[256];
  
  va_start(args,format);
  vsnprintf(buf,sizeof(buf),format,args);
  va_end(args);

  reply->msgid[0]=msg->msgid[0];
  reply->msgid[1]=msg->msgid[1];
  reply->timestamp=time(NULL);
  reply->result=result;
  strlcpy(reply->data,buf,sizeof(reply->data));
}




int openserialdevice(const char *device, const char *speed)
{
  int fd;
  int baudrate;
  struct termios t;
  speed_t spd;


  if (sscanf(speed,"%d",&baudrate) != 1) {
    warn("invalid serial speed setting: %s",speed);
    return -1;
  }

  fd = open(device,O_RDWR|O_NONBLOCK|O_CLOEXEC|O_NOCTTY);
  if (fd < 0) {
    warn("failed to open %s (errno=%d)\n",device,errno);
    return -2;
  }

  if (tcgetattr(fd,&t)) {
    warn("tcgetattr() failed on %s\n",device);
    return -3;
  }


  switch (baudrate) {
  case 9600:
    spd=B9600;
    break;
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
    warn("invalid serial port speed setting (%d) falling back to default",
	 speed);
  }

  cfmakeraw(&t); 
  cfsetspeed(&t,spd);
  t.c_cflag |= CLOCAL;

  if (tcsetattr(fd,TCSANOW,&t)) {
    warn("tcsetattr() failed on %s\n",device);
    return -4;
  }

  return fd;
}



/* eof :-) */
