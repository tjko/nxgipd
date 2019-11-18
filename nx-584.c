/* nx-584.c
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define DEBUG 0

#include "nx-584.h"


/* known panel models / IDs */

const nx_panel_model_t nx_panel_models[] = {
/* { panel-ID, model-name, max-zones, max-partitions } */
/* {   0, "NX-4",      8, 1 },  ???? */
/* {   1, "NX-6",     16, 2 },  ???? */
   {   2, "NX-8",     48, 8 },
   {   4, "NX-8E",   192, 8 },
   {  10, "NX-4-V2",   8, 1 },
   {  11, "NX-6-V2",  16, 2 },
   {  12, "NX-8-V2",  48, 8 },
   { 100, "CS575",    48, 4 },
   {  -1, NULL,        0, 0 }, 
};

const nx_log_event_type_t nx_log_event_types[] = {
  {   0, 'Z', 1, "Alarm" },
  {   1, 'Z', 1, "Alarm restore" },
  {   2, 'Z', 1, "Bypass" },
  {   3, 'Z', 1, "Bypass restore" },
  {   4, 'Z', 1, "Tamper" },
  {   5, 'Z', 1, "Tamper restore" },
  {   6, 'Z', 1, "Trouble" },
  {   7, 'Z', 1, "Trouble restore" },
  {   8, 'Z', 1, "TX low battery" },
  {   9, 'Z', 1, "TX low battery restore" },
  {  10, 'Z', 1, "Zone lost" },
  {  11, 'Z', 1, "Zone lost restore" },
  {  12, 'Z', 1, "Start of cross time" },
  {  17, 'N', 0, "Special expansion event" },
  {  18, 'N', 1, "Duress" },
  {  19, 'N', 1, "Manual fire" },
  {  20, 'N', 1, "Auxiliary 2 panic" },
  {  22, 'N', 1, "Panic" },
  {  23, 'N', 1, "Keypad tamper" },
  {  24, 'D', 0, "Control box tamper" },
  {  25, 'D', 0, "Control box tamper restore" },
  {  26, 'D', 0, "AC fail" },
  {  27, 'D', 0, "AC fail restore" },
  {  28, 'D', 0, "Low battery" },
  {  29, 'D', 0, "Low battery restore" },
  {  30, 'D', 0, "Over-current" },
  {  31, 'D', 0, "Over-current restore" },
  {  32, 'D', 0, "Siren tamper" },
  {  33, 'D', 0, "Siren tamper restore" },
  {  34, 'N', 0, "Telephone fault" },
  {  35, 'N', 0, "Telephone fault restore" },
  {  36, 'D', 0, "Expander trouble" },
  {  37, 'D', 0, "Expander trouble restore" },
  {  38, 'N', 0, "Fail to communicate" },
  {  39, 'N', 0, "Log full" },
  {  40, 'U', 1, "Opening" },
  {  41, 'U', 1, "Closing" },
  {  42, 'U', 1, "Exit error" },
  {  43, 'U', 1, "Recent closing" },
  {  44, 'N', 0, "Auto-test" },
  {  45, 'N', 0, "Start program" },
  {  46, 'N', 0, "End program" },
  {  47, 'N', 0, "Start download" },
  {  48, 'N', 0, "End download" },
  {  49, 'U', 1, "Cancel" },
  {  50, 'N', 0, "Ground fault" },
  {  51, 'N', 0, "Ground fault restore" },
  {  52, 'N', 0, "Manual test" },
  {  53, 'U', 1, "Closed with zones bypassed" },
  {  54, 'N', 0, "Start of listen in" },
  {  55, 'N', 0, "Technician on site" },
  {  56, 'N', 0, "Technician left" },
  {  57, 'N', 0, "Control power up" },
  { 117, 'D', 1, "Enrolled" }, /* not documented */
  { 119, 'N', 0, "Clock set" }, /* not documented */
  { 120, 'U', 1, "First to open" },
  { 121, 'U', 1, "Last to close" },
  { 122, 'U', 1, "PIN entered with bit 7 set" },
  { 123, 'N', 0, "Begin walk-test" },
  { 124, 'N', 0, "End walk-test" },
  { 125, 'N', 1, "Re-exit" },
  { 126, 'U', 0, "Output trip" },
  { 127, 'N', 0, "Data lost" },

  {   0,  0 , 0, NULL }
};



int fletcher_checksum(const void *buf, unsigned int len, 
		       unsigned char *sum1, unsigned char *sum2)
{
  int i;
  unsigned char s1 = 0;
  unsigned char s2 = 0;
  const unsigned char *str = buf;

  if (!buf || !sum1 || !sum2) return -1;

  for(i=0;i<len;i++) {
    if ((255 - s1) < str[i]) s1++;
    s1+=str[i];
    if (s1 == 255) s1=0;
    if ((255 - s2) < s1) s2++;
    s2+=s1;
    if (s2 == 255) s2=0;
  }
 
  *sum1=s1;
  *sum2=s2;
  return 0;
}


const char* nx_timestampstr(time_t t)
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


const char* nx_log_event_text(uchar eventnum)
{
  static char str[256];
  int i=0;

  while(nx_log_event_types[i].description) {
    if (nx_log_event_types[i].type == (eventnum & NX_EVENT_TYPE_MASK))
      return nx_log_event_types[i].description;
    i++;
  }

  snprintf(str,sizeof(str),"Unknown log event (%02x)", eventnum);
  return str;
}


int nx_log_event_partinfo(uchar eventnum)
{
  int i=0;

  while(nx_log_event_types[i].description) {
    if (nx_log_event_types[i].type == (eventnum & NX_EVENT_TYPE_MASK)) 
      return nx_log_event_types[i].partition;
    i++;
  }

  return 0;
}


char nx_log_event_valtype(uchar eventnum)
{
  int i=0;

  while(nx_log_event_types[i].description) {
    if (nx_log_event_types[i].type == (eventnum & NX_EVENT_TYPE_MASK)) 
      return nx_log_event_types[i].valtype;
    i++;
  }

  return 'N';
}



const char* nx_log_event_str(const nx_log_event_t *event)
{
  static char str[256];
  char tmp1[32],tmp2[32];
  int i = 0;
  int ev = -1;

  *str=0;
  if (event == NULL) return str;

  /* search for event type */
  while (nx_log_event_types[i].description) {
    if (nx_log_event_types[i].type == (event->type & NX_EVENT_TYPE_MASK)) ev=i;
    i++;
  }

  if (ev >= 0) {
    if (nx_log_event_types[ev].valtype == 'Z') snprintf(tmp1,sizeof(tmp1)-1,", Zone=%d",event->num+1);
    else if (nx_log_event_types[ev].valtype == 'U') snprintf(tmp1,sizeof(tmp1)-1,", User=%d",event->num+1);
    else if (nx_log_event_types[ev].valtype == 'D') snprintf(tmp1,sizeof(tmp1)-1,", Device=%d",event->num);
    else tmp1[0]=0;
    tmp1[sizeof(tmp1)-1]=0;
    if (nx_log_event_types[ev].partition) snprintf(tmp2,sizeof(tmp2)-1,", Partition=%d",event->part+1);
    else tmp2[0]=0;
    tmp2[sizeof(tmp2)-1]=0;
    
    snprintf(str,sizeof(str)-1,"%slog entry %d/%d: %02d/%02d %02d:%02d %s%s%s",
	     (NX_IS_NONREPORTING_EVENT(event->type)?"non-reporting ":""),
	     event->no+1,event->logsize,
	     event->month,event->day,event->hour,event->min,
	     nx_log_event_types[ev].description,tmp1,tmp2);
  } else {
    snprintf(str,sizeof(str)-1,"%slog entry %d/%d: %02d/%02d %02d:%02d unknown entry %d (num=%d,partition=%d)",
	     (NX_IS_NONREPORTING_EVENT(event->type)?"non-reporting ":""),
	     event->no+1,event->logsize,
	     event->month,event->day,event->hour,event->min,
	     (event->type & NX_EVENT_TYPE_MASK),event->num,event->part);
  }
  
  str[sizeof(str)-1]=0;
  return str;
}


int nx_read_packet(int fd, nxmsg_t *msg, int protocol)
{
  static unsigned char tmp[1024];
  static int len = -1;
  static int msglen = -1;
  static int rawlen = -1;

  int i,r;
  unsigned char csum1,csum2,*tmpptr;
  unsigned char checksumbuf[1024];
  unsigned char startchar;
  int multiplier;



  if (protocol == NX_PROTOCOL_ASCII) { 
    startchar=0x0a; 
    multiplier=2;
  } else {
    startchar=0x7e; 
    multiplier=1;
  }

  if (fd < 0 || !msg) {
    fprintf(stderr,"nx_read_packet(): invalid arguments\n");
    logmsg(0,"nx_read_packet(): invalid arguments");
    exit(1);
  }
  
  if (len < 0) {
    /* look for start of message */
    do {
      do { 
	r = read(fd,tmp,1);
      } while (r == -1 && errno==EINTR);
      
      if (r < 0) {
	if (errno == EAGAIN) return 0;
	fprintf(stderr,"nx_read_packet(): read() failed (%d) while looking start of message\n",errno);
	logmsg(0,"nx_read_packet(): read() failed (%d) while looking start of message",errno);
	exit(1);
      } else if (r == 0) {
	fprintf(stderr,"nx_read_packet(): EOF while looking start of message\n");
	logmsg(0,"nx_read_packet(): EOF while looking start of message");
	exit(1);
      }
    } while (tmp[0] != startchar);
    len=0;
  }

  if (len < 1*multiplier) {
    /* read packet size */
    while (len < multiplier) {
      do {
	r = read(fd,&tmp[len],(multiplier-len));
      } while (r == -1 && errno==EINTR);
      
      if (r < 0) {
	if (errno == EAGAIN) return 0;
	fprintf(stderr,"nx_read_packet(): read() failed (%d) while reading message length\n",errno);
	logmsg(0,"nx_read_packet(): read() failed (%d) while reading message length",errno);
	exit(1);
      }
      
      len+=r;
    }
  }

  if (msglen == -1) {
    if (protocol == NX_PROTOCOL_ASCII) {
      tmp[2]=0;
      if (sscanf((const char*)tmp,"%x",&msglen) != 1) {
	logmsg(3,"nx_read_packet(): invalid packet (length)");
	len=-1;
	msglen=-1;
	return -1;
      }
    } else {
      msglen=tmp[0];
    }

    /* printf("message length = %d (%02x)\n",msglen,msglen); */
    rawlen = multiplier * (1+msglen+2) + (protocol == NX_PROTOCOL_ASCII ? 1 : 0);
  }


  /* read message data */

  if (len <  rawlen) {
    while (len < rawlen) {
      do {
	r = read(fd,&tmp[len],(rawlen-len));
      } while (r == -1 && errno==EINTR);
      
      if (r < 0) {
	if (errno == EAGAIN) return 0;
	fprintf(stderr,"nx_read_packet(): read() failed (%d) while reading message data\n",errno);
	logmsg(0,"nx_read_packet(): read() failed (%d) while reading message data",errno);
	exit(1);
      }

      if (protocol == NX_PROTOCOL_BINARY) {
	/* any byte stuffed bytes means we need to read extra byte... */
	int c = 0;
	int j;
	for (j=0;j<r;j++) {
	  if (tmp[len+j] == 0x7d) c++;
	}
	rawlen+=c;
      }

      len+=r;
    }
  }

  
  /* decode message */

  msg->len=msglen;
  i=1; /* skip the message length in the buffer already */
  tmpptr=tmp+i;

  while (i<=msglen+2) {
    char buf[3];
    int val;

    if (protocol == NX_PROTOCOL_ASCII) {
      buf[0]=tmp[i*2];
      buf[1]=tmp[i*2+1];
      buf[2]=0;
      if (sscanf(buf,"%x",&val) != 1) {
	logmsg(3,"nx_read_packet(): invalid data in packet '%s' (pos=%d)",tmp,i);
	len=-1;
	msglen=-1;
	return -1;
      }
    } else { /* NX_PROTOCOL_BINARY */
      if  (*tmpptr == 0x7e) {
	logmsg(3,"nx_read_packet(): invalid data in packet %x (pos=%d)",*tmpptr,i);
	len=0; /* 0x7e should always be considered as start of new packet */
	msglen=-1;
	return -1;
      } else if (*tmpptr == 0x7d) {
	tmpptr++;
	val=(*tmpptr ^ 0x20);
      } else {
	val=*tmpptr;
      }
    } 

    if (i==1) { 
      msg->msgnum=val;
    } else if (i==msglen+1) {
      msg->sum1=val;
    } else if (i==msglen+2) {
      msg->sum2=val;
    } else { 
      msg->msg[i-2]=val;
    }
    checksumbuf[i]=val;

    i++; 
    tmpptr++;
  }

  checksumbuf[0]=msg->len;
  fletcher_checksum(checksumbuf,msg->len+1,&csum1,&csum2);


#if DEBUG > 0
  fprintf(stderr,"%s: IN len=%02d msg=%02X ack=%d: ",nx_timestampstr(time(NULL)),msg->len,msg->msgnum&NX_MSG_MASK,NX_IS_ACKMSG(msg->msgnum));
  for(i=0;i<msg->len-1;i++) { fprintf(stderr,"%02X ",msg->msg[i]); }
  fprintf(stderr,": chksum=%02X %02X %s\n",msg->sum1,msg->sum2,((msg->sum1==csum1 && msg->sum2==csum2)?"OK":"ERR"));
#endif


  len=-1;
  msglen=-1;

  if ( (msg->sum1 != csum1) || (msg->sum2 != csum2) ) {
    logmsg(3,"nx_read_packet(): invalid packet checksum");
    return -1;
  }

  msg->r_time=time(NULL);
  msg->s_time=0;
  return 1;
}


static inline void byte_stuff(unsigned char **buf, unsigned char c)
{
  if (buf) {
    if (c == 0x7e || c == 0x7d) {
      **buf=0x7d;
      (*buf)++;
      **buf=(c ^ 0x20);
    } else {
      **buf=c;
    }
    (*buf)++;
  }
}


int nx_write_packet(int fd, nxmsg_t *msg, int protocol)
{
  unsigned char out[1024];
  unsigned char tmp[1024];
  unsigned char *p = out;
  unsigned char *c = tmp;
  int i,w;

  
  if (protocol == NX_PROTOCOL_ASCII) {
    *p++=0x0a;
    snprintf((char*)p,5,"%02X%02X",msg->len,msg->msgnum);
    p+=4;
  } else {
    *p++=0x7e;
    byte_stuff(&p,msg->len);
    byte_stuff(&p,msg->msgnum);
  }
  *c++=msg->len;
  *c++=msg->msgnum;


  for(i=0;i<msg->len-1;i++) {
    if (protocol == NX_PROTOCOL_ASCII) {
      snprintf((char*)p,3,"%02X",msg->msg[i]);
      p+=2;
    } else {
      byte_stuff(&p,msg->msg[i]);
    }
    *c++=msg->msg[i];
  }

  fletcher_checksum(tmp,msg->len+1,&msg->sum1,&msg->sum2);

#if DEBUG > 0
  fprintf(stderr,"%s: OUT len=%02d msg=%02X ack=%d: ",nx_timestampstr(time(NULL)),msg->len,msg->msgnum&NX_MSG_MASK,NX_IS_ACKMSG(msg->msgnum));
  for(i=0;i<msg->len-1;i++) { fprintf(stderr,"%02X ",msg->msg[i]); }
  fprintf(stderr,": chksum=%02X %02X\n",msg->sum1,msg->sum2);
#endif

  if (protocol == NX_PROTOCOL_ASCII) {
    snprintf((char*)p,5,"%02X%02X",msg->sum1,msg->sum2);
    p+=4;
    *p++=0x0d;
    *p=0;
    /* fprintf(stderr,"output: '%s' len=%d\n",out,(p-out)); */
  } else {
    byte_stuff(&p,msg->sum1);
    byte_stuff(&p,msg->sum2);

    /* for (i=0;i<(p-out);i++) { printf("%02X ",out[i]); } printf("\n"); */
  }

  do {
    w=write(fd,out,p-out);
  } while (w == -1 && (errno == EAGAIN || errno == EINTR));

  if (w == (p-out)) {
    msg->r_time = 0;
    msg->s_time = time(NULL);
    return 0;
  }
  return -1;
}



int nx_receive_message(int fd, int protocol, nxmsg_t *msg, int timeout)
{
  fd_set rfds;
  struct timeval tv;
  int ret;
  time_t etime,extratime;
  nxmsg_t msgout;
  
  if (fd < 0 || !msg) return -3;

  etime=time(NULL)+timeout;
  extratime=0;

  do {

    FD_ZERO(&rfds);
    FD_SET(fd,&rfds);
    tv.tv_sec=0;
    tv.tv_usec=200000;
    do {
      ret = select(fd+1,&rfds,NULL,NULL,&tv);
    } while (ret == -1 && errno==EINTR);
    if (ret < 0) {
      logmsg(2,"nx_receive_message(): select failed: %d (%s)",errno,strerror(errno));
      return -2;
    }

    if (ret > 0) {
      /* printf("data waiting\n"); */

      int r = nx_read_packet(fd,msg,protocol);
      if (r==1) {
	logmsg(3,"nx_receive_message(): got message %02x (%02x)",msg->msgnum & NX_MSG_MASK, msg->msgnum);
	if ( NX_IS_ACKMSG(msg->msgnum) ) {
	  logmsg(3,"nx_receive_message(): sending ACK as requested");
	  msgout.msgnum=NX_POSITIVE_ACK;
	  msgout.len=1;
	  if (nx_write_packet(fd,&msgout,protocol) < 0) 
	    logmsg(3,"nx_receive_message(): error sending ACK");
	}
	return 1;
      } else if (r==0) {
	logmsg(3,"nx_receive_message(): partial message");
	usleep(100000);
	if (extratime==0) extratime=2;
      } else {
	logmsg(3,"nx_receive_message(): invalid message received");
	msgout.msgnum=NX_MSG_REJECTED;
	msgout.len=1;
	nx_write_packet(fd,&msgout,protocol);
	return -1;
      }
    }
  } while (time(NULL) <= (etime+extratime));

  logmsg(4,"nx_receive_message(): timeout (no message received)");
  return 0;
}


int nx_send_message(int fd, int protocol, nxmsg_t *msg, int timeout, int retry, unsigned char replycmd, nxmsg_t *replymsg)
{
  int res, t;
  int count = 0;

  if (fd < 0 || !msg) return -2;


  do {

    if (nx_write_packet(fd,msg,protocol) < 0) {
      logmsg(3,"nx_send_message(): failed to send message %02d (errno=%d)",msg->msgnum & NX_MSG_MASK, errno);
      return -1;
    }
    logmsg(3,"nx_send_message(): message %02x sent",msg->msgnum & NX_MSG_MASK);

    t=3;
    while (t > 0) {
      usleep(100000);
      res=nx_receive_message(fd,protocol,replymsg,timeout);
      char rnum = replymsg->msgnum;
      if (res==1) {
	if ( rnum == replycmd ||
	     rnum == NX_CMD_FAILED ||
	     rnum == NX_POSITIVE_ACK ||
	     rnum == NX_NEGATIVE_ACK ||
	     rnum == NX_MSG_REJECTED ) {
	  logmsg(3,"nx_send_message(): reply received %02x (%02x)",rnum & NX_MSG_MASK,replymsg->msgnum);
	  return 1;
	} else {
	  logmsg(3,"nx_send_message(): wrong reply %02x (%02x) received",rnum & NX_MSG_MASK,replymsg->msgnum); 
	}
      }
      t--;
    }
    logmsg(3,"nx_send_message(): no response received");

  } while (count++ < retry);

  return 0;
}





#define DEBUG_PRINT_HEADER(name,msg) fprintf(fp,"%s: %s (MSG=0x%02X, ACK=%d):\n",\
					     nx_timestampstr(msg->r_time),name,msg->msgnum&NX_MSG_MASK,\
					     (msg->msgnum&NX_MSG_ACK_FLAG?1:0))    
#define DEBUG_PRINT_STR(name,val)  fprintf(fp,"\t%42s: %s\n",name,val)
#define DEBUG_PRINT_UCHAR(name,val)  fprintf(fp,"\t%42s: %u\n",name,val)
#define DEBUG_PRINT_DATA(name,val)  fprintf(fp,"\t%42s: %03u 0x%02x (%02u,%02u)\n",name,val,val,(val&0x0f),((val&0xf0)>>4))
#define DEBUG_PRINT_INT(name,val)  fprintf(fp,"\t%42s: %d\n",name,val)
#define DEBUG_PRINT_HEX(name,val)  fprintf(fp,"\t%42s: %02x\n",name,val)
#define DEBUG_PRINT_FLAG(name,val) fprintf(fp,"\t%42s: %s\n",name,(val ? "Yes" : "No"))
#define DEBUG_PRINT_CFLAG(name,val) if (val) fprintf(fp,"\t\t- %s\n",name)
#define DEBUG_PRINT_PMASK(name,val)  fprintf(fp,"\t%42s: %c%c%c%c%c%c%c%c (%02x)\n",name, \
					    (val&0x01?'1':'-'),\
					    (val&0x02?'2':'-'),\
					    (val&0x04?'3':'-'),\
					    (val&0x08?'4':'-'),\
					    (val&0x10?'5':'-'),\
					    (val&0x20?'6':'-'),\
					    (val&0x40?'7':'-'),\
					    (val&0x80?'8':'-'),	\
                                           val)
#define DEBUG_PRINT_ZONESNAP(zone,val) fprintf(fp,"\t\t\t\t\t   Zone %02d: %s %s%s%s\n\t\t\t\t\t   Zone %02d: %s %s%s%s\n",zone,\
    (val&0x01?"Faulted":"Okay"),\
    (val&0x02?"(Bypassed)":""),\
    (val&0x04?"(Trouble)":""),\
    (val&0x08?"(Alarm Memory)":""),\
    zone+1,\
    (val&0x10?"Faulted":"Okay"),\
    (val&0x20?"(Bypassed)":""),\
    (val&0x40?"(Trouble)":""),\
    (val&0x80?"(Alarm Memory)":""))




void nx_print_msg(FILE *fp, nxmsg_t *msg)
{
  unsigned char msgnum;
  int i;
  char tmp[256];

  if (!fp || !msg) return;
  msgnum = msg->msgnum & NX_MSG_MASK;

  switch (msgnum) {

  case NX_INT_CONFIG_MSG:
    memcpy(tmp,&msg->msg[0],4);
    tmp[4]=0;
    DEBUG_PRINT_HEADER("INTERFACE CONFIGURATION MESSAGE",msg);    

    DEBUG_PRINT_STR("Firmware version",tmp);

    fprintf(fp,"\tEnabled transition messages:\n");

    DEBUG_PRINT_FLAG("Interface Configuration Message (01h)",msg->msg[4] & 0x02);
    DEBUG_PRINT_FLAG("Zone Status Message (04h)",msg->msg[4] & 0x10);
    DEBUG_PRINT_FLAG("Zones Snapshot Message (05h)",msg->msg[4] & 0x20);
    DEBUG_PRINT_FLAG("Partition Status Message (06h)",msg->msg[4] & 0x40);
    DEBUG_PRINT_FLAG("Partition Snapshot Message (07h)",msg->msg[4] & 0x80);

    DEBUG_PRINT_FLAG("System Status Message (08h)",msg->msg[5] & 0x01);
    DEBUG_PRINT_FLAG("X-10 Message Received (09h)",msg->msg[5] & 0x02);
    DEBUG_PRINT_FLAG("Log Event Message (0Ah)",msg->msg[5] & 0x04);
    DEBUG_PRINT_FLAG("Keypad Message Received (0Bh)",msg->msg[5] & 0x08);

    fprintf(fp,"\tEnabled requests/commands:\n");

    DEBUG_PRINT_FLAG("Interface Configuration Request (21h)",msg->msg[6] & 0x02);
    DEBUG_PRINT_FLAG("Zone Name Request (23h)",msg->msg[6] & 0x08);
    DEBUG_PRINT_FLAG("Zone Status Request (24h)",msg->msg[6] & 0x10);
    DEBUG_PRINT_FLAG("Zones Snapshot Request (25h)",msg->msg[6] & 0x20);
    DEBUG_PRINT_FLAG("Partition Status Request (26h)",msg->msg[6] & 0x40);
    DEBUG_PRINT_FLAG("Partitions Snapshot Request (27h)",msg->msg[6] & 0x80);

    DEBUG_PRINT_FLAG("System Status Request (28h)",msg->msg[7] & 0x01);
    DEBUG_PRINT_FLAG("Send X-10 Message (29h)",msg->msg[7] & 0x02);
    DEBUG_PRINT_FLAG("Log Event Request (2Ah)",msg->msg[7] & 0x04);
    DEBUG_PRINT_FLAG("Send Keypad Text Message (2Bh)",msg->msg[7] & 0x08);
    DEBUG_PRINT_FLAG("Keypad Terminal Mode Request (2Ch)",msg->msg[7] & 0x10);

    DEBUG_PRINT_FLAG("Program Data Request (30h)",msg->msg[8] & 0x01);
    DEBUG_PRINT_FLAG("Program Data Command (31h)",msg->msg[8] & 0x02);
    DEBUG_PRINT_FLAG("User Information Request with PIN (32h)",msg->msg[8] & 0x04);
    DEBUG_PRINT_FLAG("User Information Request without PIN (33h)",msg->msg[8] & 0x08);
    DEBUG_PRINT_FLAG("Set User Code Command with PIN (34h)",msg->msg[8] & 0x10);
    DEBUG_PRINT_FLAG("Set User Code Command without PIN (35h)",msg->msg[8] & 0x20);
    DEBUG_PRINT_FLAG("Set User Authorization with PIN (36h)",msg->msg[8] & 0x40);
    DEBUG_PRINT_FLAG("Set User Authorization withouth PIN (37h)",msg->msg[8] & 0x80);

    DEBUG_PRINT_FLAG("Store Communication Event Command (3Ah)",msg->msg[9] & 0x04);
    DEBUG_PRINT_FLAG("Set Clock / Calendar Command (3Bh)",msg->msg[9] & 0x08);
    DEBUG_PRINT_FLAG("Primary Keypad Function with PIN (3Ch)",msg->msg[9] & 0x10);
    DEBUG_PRINT_FLAG("Primary Keypad Function without PIN (3Dh)",msg->msg[9] & 0x20);
    DEBUG_PRINT_FLAG("Secondary Keypad Function (3Eh)",msg->msg[9] & 0x40);
    DEBUG_PRINT_FLAG("Zone Bypass Toggle (3Fh)",msg->msg[9] & 0x80);
    break;

  case NX_ZONE_NAME_MSG:
    DEBUG_PRINT_HEADER("ZONE NAME MESSAGE",msg);
    DEBUG_PRINT_UCHAR("Zone number",msg->msg[0]+1);
    memcpy(tmp,&msg->msg[1],16);
    tmp[16]=0;
    DEBUG_PRINT_STR("Zone name",tmp);
    break;

  case NX_ZONE_STATUS_MSG:
    DEBUG_PRINT_HEADER("ZONE STATUS MESSAGE",msg);
    DEBUG_PRINT_UCHAR("Zone number",msg->msg[0]+1);
    DEBUG_PRINT_PMASK("Partition mask",msg->msg[1]);

    fprintf(fp,"\tZone type flags:\n");
    
    DEBUG_PRINT_CFLAG("Fire",msg->msg[2] & 0x01);
    DEBUG_PRINT_CFLAG("24 Hour",msg->msg[2] & 0x02);
    DEBUG_PRINT_CFLAG("Key-switch",msg->msg[2] & 0x04);
    DEBUG_PRINT_CFLAG("Follower",msg->msg[2] & 0x08);
    DEBUG_PRINT_CFLAG("Entry / Exit delay 1",msg->msg[2] & 0x10);
    DEBUG_PRINT_CFLAG("Entry / Exit delay 2",msg->msg[2] & 0x20);
    DEBUG_PRINT_CFLAG("Interior",msg->msg[2] & 0x40);
    DEBUG_PRINT_CFLAG("Local only",msg->msg[2] & 0x80);

    DEBUG_PRINT_CFLAG("Keypad sounder",msg->msg[3] & 0x01);
    DEBUG_PRINT_CFLAG("Yelping siren",msg->msg[3] & 0x02);
    DEBUG_PRINT_CFLAG("Steady siren",msg->msg[3] & 0x04);
    DEBUG_PRINT_CFLAG("Chime",msg->msg[3] & 0x08);
    DEBUG_PRINT_CFLAG("Bypassable",msg->msg[3] & 0x10);
    DEBUG_PRINT_CFLAG("Group Bypassable",msg->msg[3] & 0x20);
    DEBUG_PRINT_CFLAG("Force armable",msg->msg[3] & 0x40);
    DEBUG_PRINT_CFLAG("Entry guard",msg->msg[3] & 0x80);

    DEBUG_PRINT_CFLAG("Fast loop response",msg->msg[4] & 0x01);
    DEBUG_PRINT_CFLAG("Double EOL tamper",msg->msg[4] & 0x02);
    DEBUG_PRINT_CFLAG("Trouble",msg->msg[4] & 0x04);
    DEBUG_PRINT_CFLAG("Cross zone",msg->msg[4] & 0x08);
    DEBUG_PRINT_CFLAG("Dialer delay",msg->msg[4] & 0x10);
    DEBUG_PRINT_CFLAG("Swinger shutdown",msg->msg[4] & 0x20);
    DEBUG_PRINT_CFLAG("Restorable",msg->msg[4] & 0x40);
    DEBUG_PRINT_CFLAG("Listen in",msg->msg[4] & 0x80);
      
    fprintf(fp,"\tZone condition flags:\n");
    DEBUG_PRINT_FLAG("Faulted (or delayed trip)",msg->msg[5] & 0x01);
    DEBUG_PRINT_FLAG("Tampered",msg->msg[5] & 0x02);
    DEBUG_PRINT_FLAG("Trouble",msg->msg[5] & 0x04);
    DEBUG_PRINT_FLAG("Bypassed",msg->msg[5] & 0x08);
    DEBUG_PRINT_FLAG("Inhibited (force armed)",msg->msg[5] & 0x10);
    DEBUG_PRINT_FLAG("Low Battery",msg->msg[5] & 0x20);
    DEBUG_PRINT_FLAG("Loss of supervision",msg->msg[5] & 0x40);

    DEBUG_PRINT_FLAG("Alarm memory",msg->msg[6] & 0x01);
    DEBUG_PRINT_FLAG("Bypass memory",msg->msg[6] & 0x02);
    break;

  case NX_ZONE_SNAPSHOT_MSG:
    DEBUG_PRINT_HEADER("ZONE SNAPSHOT MESSAGE",msg);
    DEBUG_PRINT_UCHAR("Zone offset",msg->msg[0]*16);
    DEBUG_PRINT_ZONESNAP(msg->msg[0]*16+1,msg->msg[1]);
    DEBUG_PRINT_ZONESNAP(msg->msg[0]*16+3,msg->msg[2]);
    DEBUG_PRINT_ZONESNAP(msg->msg[0]*16+5,msg->msg[3]);
    DEBUG_PRINT_ZONESNAP(msg->msg[0]*16+7,msg->msg[4]);
    DEBUG_PRINT_ZONESNAP(msg->msg[0]*16+9,msg->msg[5]);
    DEBUG_PRINT_ZONESNAP(msg->msg[0]*16+11,msg->msg[6]);
    DEBUG_PRINT_ZONESNAP(msg->msg[0]*16+13,msg->msg[7]);
    DEBUG_PRINT_ZONESNAP(msg->msg[0]*16+15,msg->msg[8]);
    break;

  case NX_PART_STATUS_MSG:
    DEBUG_PRINT_HEADER("PARTITION STATUS MESSAGE",msg);
    DEBUG_PRINT_UCHAR("Partition number",msg->msg[0]+1);

    fprintf(fp,"\tPartition condition flags:\n");
    DEBUG_PRINT_CFLAG("Bypass code required",msg->msg[1] & 0x01);
    DEBUG_PRINT_CFLAG("Fire trouble",msg->msg[1] & 0x02);
    DEBUG_PRINT_CFLAG("Fire",msg->msg[1] & 0x04);
    DEBUG_PRINT_CFLAG("Pulsing Buzzer",msg->msg[1] & 0x08);
    DEBUG_PRINT_CFLAG("TLM fault memory",msg->msg[1] & 0x10);
    /* DEBUG_PRINT_CFLAG("Reserved",msg->msg[1] & 0x20); */
    DEBUG_PRINT_CFLAG("Armed",msg->msg[1] & 0x40);
    DEBUG_PRINT_CFLAG("Instant",msg->msg[1] & 0x80);

    DEBUG_PRINT_CFLAG("Previous Alarm",msg->msg[2] & 0x01);
    DEBUG_PRINT_CFLAG("Siren on",msg->msg[2] & 0x02);
    DEBUG_PRINT_CFLAG("Steady siren on",msg->msg[2] & 0x04);
    DEBUG_PRINT_CFLAG("Alarm memory",msg->msg[2] & 0x08);
    DEBUG_PRINT_CFLAG("Tamper",msg->msg[2] & 0x10);
    DEBUG_PRINT_CFLAG("Cancel command entered",msg->msg[2] & 0x20);
    DEBUG_PRINT_CFLAG("Code entered",msg->msg[2] & 0x40);
    DEBUG_PRINT_CFLAG("Cancel pending",msg->msg[2] & 0x80);

    /* DEBUG_PRINT_CFLAG("Reserved",msg->msg[3] & 0x01); */
    DEBUG_PRINT_CFLAG("Silent exit enabled",msg->msg[3] & 0x02);
    DEBUG_PRINT_CFLAG("Entryguard (stay mode)",msg->msg[3] & 0x04);
    DEBUG_PRINT_CFLAG("Chime mode on",msg->msg[3] & 0x08);
    DEBUG_PRINT_CFLAG("Entry",msg->msg[3] & 0x10);
    DEBUG_PRINT_CFLAG("Delay expiration warning",msg->msg[3] & 0x20);
    DEBUG_PRINT_CFLAG("Exit1",msg->msg[3] & 0x40);
    DEBUG_PRINT_CFLAG("Exit2",msg->msg[3] & 0x80);

    DEBUG_PRINT_CFLAG("LED extinguish",msg->msg[4] & 0x01);
    DEBUG_PRINT_CFLAG("Cross timing",msg->msg[4] & 0x02);
    DEBUG_PRINT_CFLAG("Recent closing being timed",msg->msg[4] & 0x04);
    /* DEBUG_PRINT_CFLAG("Reserved",msg->msg[4] & 0x08); */
    DEBUG_PRINT_CFLAG("Exit error triggered",msg->msg[4] & 0x10);
    DEBUG_PRINT_CFLAG("Auto home inhibited",msg->msg[4] & 0x20);
    DEBUG_PRINT_CFLAG("Sensor low battery",msg->msg[4] & 0x40);
    DEBUG_PRINT_CFLAG("Sensor lost supervision",msg->msg[4] & 0x80);

    DEBUG_PRINT_CFLAG("Zone bypassed",msg->msg[6] & 0x01);
    DEBUG_PRINT_CFLAG("Force arm triggered b auto arm",msg->msg[6] & 0x02);
    DEBUG_PRINT_CFLAG("Ready to arm",msg->msg[6] & 0x04);
    DEBUG_PRINT_CFLAG("Ready to force arm",msg->msg[6] & 0x08);
    DEBUG_PRINT_CFLAG("Valid PIN accepted",msg->msg[6] & 0x10);
    DEBUG_PRINT_CFLAG("Chime on (sounding)",msg->msg[6] & 0x20);
    DEBUG_PRINT_CFLAG("Error beep (triple beep)",msg->msg[6] & 0x40);
    DEBUG_PRINT_CFLAG("Tone on (activation tone)",msg->msg[6] & 0x80);

    DEBUG_PRINT_CFLAG("Entry 1",msg->msg[7] & 0x01);
    DEBUG_PRINT_CFLAG("Open period",msg->msg[7] & 0x02);
    DEBUG_PRINT_CFLAG("Alarm sent using phone number 1",msg->msg[7] & 0x04);
    DEBUG_PRINT_CFLAG("Alarm sent using phone number 2",msg->msg[7] & 0x08);
    DEBUG_PRINT_CFLAG("Alarm sent using phone number 3",msg->msg[7] & 0x10);
    DEBUG_PRINT_CFLAG("Cancel report is in the stack",msg->msg[7] & 0x20);
    DEBUG_PRINT_CFLAG("Keyswitch armed",msg->msg[7] & 0x40);
    DEBUG_PRINT_CFLAG("Delay Trip in progress (common zone)",msg->msg[7] & 0x80);


    DEBUG_PRINT_UCHAR("Last user number",msg->msg[5]);
    break;


  case NX_SYS_STATUS_MSG:
    DEBUG_PRINT_HEADER("SYSTEM STATUS MESSAGE",msg);
    DEBUG_PRINT_UCHAR("Panel ID number",msg->msg[0]+1);
    DEBUG_PRINT_PMASK("Valid partitions",msg->msg[9]);
    DEBUG_PRINT_UCHAR("Communication stack pointer",msg->msg[10]);

    fprintf(fp,"\tSystem status flags:\n");

    DEBUG_PRINT_FLAG("Line seuizure",msg->msg[1] & 0x01);
    DEBUG_PRINT_FLAG("Off hook",msg->msg[1] & 0x02);
    DEBUG_PRINT_CFLAG("Initial handshake received",msg->msg[1] & 0x04);
    DEBUG_PRINT_CFLAG("Download in progress",msg->msg[1] & 0x08);
    DEBUG_PRINT_CFLAG("Diaer delay in progress",msg->msg[1] & 0x10);
    DEBUG_PRINT_CFLAG("Using backup phone",msg->msg[1] & 0x20);
    DEBUG_PRINT_CFLAG("Listen in active",msg->msg[1] & 0x40);
    DEBUG_PRINT_CFLAG("Two way lockout",msg->msg[1] & 0x80);

    DEBUG_PRINT_FLAG("Ground fault",msg->msg[2] & 0x01);
    DEBUG_PRINT_FLAG("Phone fault",msg->msg[2] & 0x02);
    DEBUG_PRINT_FLAG("Fail to communicate",msg->msg[2] & 0x04);
    DEBUG_PRINT_FLAG("Fuse fault",msg->msg[2] & 0x08);
    DEBUG_PRINT_FLAG("Box tamper",msg->msg[2] & 0x10);
    DEBUG_PRINT_FLAG("Siren tamper / trouble",msg->msg[2] & 0x20);
    DEBUG_PRINT_FLAG("Low Battery",msg->msg[2] & 0x40);
    DEBUG_PRINT_FLAG("AC fail",msg->msg[2] & 0x80);

    DEBUG_PRINT_FLAG("Expander box tamper",msg->msg[3] & 0x01);
    DEBUG_PRINT_FLAG("Expander AC failure",msg->msg[3] & 0x02);
    DEBUG_PRINT_FLAG("Expander low batter",msg->msg[3] & 0x04);
    DEBUG_PRINT_FLAG("Expander loss of supervision",msg->msg[3] & 0x08);
    DEBUG_PRINT_FLAG("Expander auxiliary output over current",msg->msg[3] & 0x10);
    DEBUG_PRINT_FLAG("Auxiliary communication channel failure",msg->msg[3] & 0x20);
    DEBUG_PRINT_FLAG("Expander bell fault",msg->msg[3] & 0x40);

    DEBUG_PRINT_FLAG("6 digit PIN enabled",msg->msg[4] & 0x01);
    DEBUG_PRINT_FLAG("Programming token in use",msg->msg[4] & 0x02);
    DEBUG_PRINT_FLAG("PIN required for local download",msg->msg[4] & 0x04);
    DEBUG_PRINT_FLAG("Global pulsing buzzer",msg->msg[4] & 0x08);
    DEBUG_PRINT_FLAG("Global siren on",msg->msg[4] & 0x10);
    DEBUG_PRINT_FLAG("Global steady siren",msg->msg[4] & 0x20);
    DEBUG_PRINT_FLAG("Bus device has line seized",msg->msg[4] & 0x40);
    DEBUG_PRINT_FLAG("Bus device has requested sniff mode",msg->msg[4] & 0x80);

    DEBUG_PRINT_FLAG("Dynamic batter test",msg->msg[5] & 0x01);
    DEBUG_PRINT_FLAG("AC power on",msg->msg[5] & 0x02);
    DEBUG_PRINT_FLAG("Low battery memory",msg->msg[5] & 0x04);
    DEBUG_PRINT_FLAG("Ground fault memory",msg->msg[5] & 0x08);
    DEBUG_PRINT_FLAG("Fire alarm verification being timed",msg->msg[5] & 0x10);
    DEBUG_PRINT_FLAG("Smoke power reset",msg->msg[5] & 0x20);
    DEBUG_PRINT_FLAG("50Hz line power detected",msg->msg[5] & 0x40);
    DEBUG_PRINT_FLAG("Timing a high voltage battery charge",msg->msg[5] & 0x80);

    DEBUG_PRINT_FLAG("Communication since last autotest",msg->msg[6] & 0x01);
    DEBUG_PRINT_FLAG("Power up delay in progress",msg->msg[6] & 0x02);
    DEBUG_PRINT_FLAG("Walk test mode",msg->msg[6] & 0x04);
    DEBUG_PRINT_FLAG("Loss of system time",msg->msg[6] & 0x08);
    DEBUG_PRINT_FLAG("Enroll requested",msg->msg[6] & 0x10);
    DEBUG_PRINT_FLAG("Test fixture mode",msg->msg[6] & 0x20);
    DEBUG_PRINT_FLAG("Control shutdown mode",msg->msg[6] & 0x40);
    DEBUG_PRINT_FLAG("Timing a cancel window",msg->msg[6] & 0x80);

    DEBUG_PRINT_FLAG("Call back in progress",msg->msg[7] & 0x80);

    DEBUG_PRINT_FLAG("Phone line faulted",msg->msg[8] & 0x01);
    DEBUG_PRINT_FLAG("Voltage preset interrupt active",msg->msg[8] & 0x02);
    DEBUG_PRINT_FLAG("House phone off hook",msg->msg[8] & 0x04);
    DEBUG_PRINT_FLAG("Phone line monitor enabled",msg->msg[8] & 0x08);
    DEBUG_PRINT_FLAG("Sniffing",msg->msg[8] & 0x10);
    DEBUG_PRINT_FLAG("Last read was off hook",msg->msg[8] & 0x20);
    DEBUG_PRINT_FLAG("Listen in requested",msg->msg[8] & 0x40);
    DEBUG_PRINT_FLAG("Listen in trigger",msg->msg[8] & 0x80);

    break;

  case NX_X10_RCV_MSG:
    DEBUG_PRINT_HEADER("X-10 MESSAGE RECEIVED",msg);
    DEBUG_PRINT_UCHAR("House code",msg->msg[0]);
    DEBUG_PRINT_UCHAR("Unit code",msg->msg[1]);
    DEBUG_PRINT_HEX("X-10 function code",msg->msg[2]);
    break;

  case NX_LOG_EVENT_MSG:
    DEBUG_PRINT_HEADER("LOG EVENT MESSAGE",msg);
    DEBUG_PRINT_UCHAR("Event number",msg->msg[0]);
    DEBUG_PRINT_UCHAR("Total log size",msg->msg[1]);
    DEBUG_PRINT_INT("Event type",(msg->msg[2]&NX_EVENT_TYPE_MASK));
    DEBUG_PRINT_UCHAR("Zone/User/Device number",msg->msg[3]);
    DEBUG_PRINT_UCHAR("Partition number",msg->msg[4]);
    fprintf(fp,"\t%42s: %02d/%02d %02d:%02d\n","Timestamp (mm/dd hh:mm)",msg->msg[5],msg->msg[6],msg->msg[7],msg->msg[8]);
    break;

  case NX_KEYPAD_MSG_RCVD:
    DEBUG_PRINT_HEADER("KEYPAD MESSAGE RECEIVED",msg);
    DEBUG_PRINT_UCHAR("Keypad address",msg->msg[0]);
    DEBUG_PRINT_HEX("Key value",msg->msg[0]);
    break;

  case NX_PROG_DATA_REPLY:
    DEBUG_PRINT_HEADER("PROGRAM DATA REPLY",msg);
    DEBUG_PRINT_UCHAR("Device bus address",msg->msg[0]);
    DEBUG_PRINT_INT("Logical location",((msg->msg[1] & 0x0f)<<8) + msg->msg[2]);
    DEBUG_PRINT_STR("Segment size",(msg->msg[1] & 0x10 ? "Nibble":"Byte"));
    DEBUG_PRINT_INT("Segment offset",(msg->msg[1] & 0x40 ? 8 : 0));
    DEBUG_PRINT_INT("Location length",(msg->msg[3] & 0x1f)+1);
    DEBUG_PRINT_INT("Location data type",((msg->msg[3] >> 5) & 0x07));
    DEBUG_PRINT_DATA("Data byte (1)",msg->msg[4]);
    DEBUG_PRINT_DATA("Data byte (2)",msg->msg[5]);
    DEBUG_PRINT_DATA("Data byte (3)",msg->msg[6]);
    DEBUG_PRINT_DATA("Data byte (4)",msg->msg[7]);
    DEBUG_PRINT_DATA("Data byte (5)",msg->msg[8]);
    DEBUG_PRINT_DATA("Data byte (6)",msg->msg[9]);
    DEBUG_PRINT_DATA("Data byte (7)",msg->msg[10]);
    DEBUG_PRINT_DATA("Data byte (8)",msg->msg[11]);
    break;

  case NX_USER_INFO_REPLY: 
    DEBUG_PRINT_HEADER("USER INFORMATION REPLY",msg);
    DEBUG_PRINT_UCHAR("User number",msg->msg[0]);
    fprintf(fp,"\t%42s: %d %d %d %d (%d %d)\n","PIN", 
	    msg->msg[1]&0x0f,((msg->msg[1])>>4)&0x0f,
	    msg->msg[2]&0x0f,((msg->msg[2])>>4)&0x0f,
	    msg->msg[3]&0x0f,((msg->msg[3])>>4)&0x0f);
    DEBUG_PRINT_PMASK("Authority flags",msg->msg[4]);
    DEBUG_PRINT_PMASK("Authorized partition(s) mask",msg->msg[5]);
    break;


  case NX_CMD_FAILED:
    DEBUG_PRINT_HEADER("COMMAND / REQUEST FAILED",msg);
    break;

  case NX_POSITIVE_ACK:
    DEBUG_PRINT_HEADER("POSITIVE ACKNOWLEDGE (ACK)",msg);
    break;

  case NX_NEGATIVE_ACK:
    DEBUG_PRINT_HEADER("NEGATIVE ACKNOWLEDGE (NAK)",msg);
    break;

  case NX_MSG_REJECTED:
    DEBUG_PRINT_HEADER("MESSAGE REJECTED",msg);
    break;


  default:
    DEBUG_PRINT_HEADER("UNKNOWN MESSAGE",msg);
    DEBUG_PRINT_UCHAR("Length",msg->len);
    for(i=0;i<msg->len-1;i++) {
      char name[10];
      snprintf(name,sizeof(name),"Byte %d",i+2);
      DEBUG_PRINT_PMASK(name,msg->msg[i]);
    }

  }

}


const char* nx_prog_datatype_str(uchar datatype)
{
  const char *str = "n/a";

  switch (datatype % 0x07) {

  case NX_PROG_DATA_BIN:
    str="Bin";
    break;

  case NX_PROG_DATA_DEC:
    str="Dec";
    break;

  case NX_PROG_DATA_HEX:
    str="Hex";
    break;

  case NX_PROG_DATA_ASCII:
    str="Asc";
    break;
  }

  return str;
}


/* eof :-) */
