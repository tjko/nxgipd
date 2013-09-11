/* probe.c
 *
 * Copyright (C) 2013 Timo Kokkonen.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "nx-584.h"
#include "nxgipd.h"


const char* nx_program_data_types[] = { "Binary","Decimal","Hex","ASCII",
					"unused(4)","unused(5)","unused(6)","unused(7)" };



int read_config(int fd, int protocol, uchar node, int location)
{
  nxmsg_t msgout,msgin,msgin2;
  int ret,i,nibble,size,len,type;
  uint loc;
  int maxloc = 512;
  int locstart = 0;
  char tmp[16];

  printf("Scanning module %d\n",node);

  if (location >=0) {
    locstart=location;
    maxloc=location;
  }

  for(loc=locstart;loc<=maxloc;loc++) {
    
    memset(&msgin,0,sizeof(msgin));
    memset(&msgin2,0,sizeof(msgin2));

    msgout.msgnum=NX_PROG_DATA_REQ;
    msgout.len=4;
    msgout.msg[0]=node;
    msgout.msg[1]=(loc >> 8) & 0x0f;
    msgout.msg[2]=(loc & 0xff);

    //printf("Reading location %04x (%d) [%02x %02x %02x %02x]\n",loc,loc,msgout.msgnum,msgout.msg[0],msgout.msg[1],msgout.msg[2]);
    ret=nx_send_message(fd,protocol,&msgout,5,3,NX_PROG_DATA_REPLY,&msgin);
    if (ret==1) {
      if (msgin.msgnum == NX_PROG_DATA_REPLY) {
	//nx_print_msg(stdout,&msgin);
	
	nibble=((msgin.msg[1] & 0x10) == 0x10 ? 1 : 0);
	len=(msgin.msg[3] & 0x1f) + 1;
	type=(msgin.msg[3] >> 5 & 0x07);
	if (nibble) size=len/2;
	else size=len;
		      
	//printf("nibble=%d,len=%d,type=%d,size=%d\n",nibble,len,type,size);
	if (size > 8) {
	  msgout.msg[1] |= 0x40;
	  //printf("Reading location %04x offset=1 (%d) [%02x %02x %02x %02x]\n",loc,loc,msgout.msgnum,msgout.msg[0],msgout.msg[1],msgout.msg[2]);
	  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_PROG_DATA_REPLY,&msgin2);
	  if (ret==1 && msgin2.msgnum == NX_PROG_DATA_REPLY) {
	    //nx_print_msg(stdout,&msgin2);
	  } else {
	    printf("Location=%03d: failed to get full location data (offset=1)\n",loc);
	  }
	}

	printf("Location=%03d (len=%02d type=%-8s): ",loc,len,nx_program_data_types[type]);
	for(i=0;i<len;i++) {
	  int va;
	  if (nibble) {
	    if (i<16) va=msgin.msg[4+i/2];
	    else va=msgin2.msg[4+(i-16)/2];
	    if (i%2==0) {va=0x0f & va; } else { va=(va >> 4) & 0x0f; }
	  } else {
	    if (i<8) va=msgin.msg[4+i];
	    else va=msgin2.msg[4+(i-8)];
	  }

	  switch (type) {
	  case 0:
	    strncpy(tmp,(va&0x01?"1":"-"),2);
	    strncat(tmp,(va&0x02?"2":"-"),1);
	    strncat(tmp,(va&0x04?"3":"-"),1);
	    strncat(tmp,(va&0x08?"4":"-"),1);
	    strncat(tmp,(va&0x10?"5":"-"),1);
	    strncat(tmp,(va&0x20?"6":"-"),1);
	    strncat(tmp,(va&0x40?"7":"-"),1);
	    strncat(tmp,(va&0x80?"8":"-"),1);
	    printf("[%s] ",tmp);
	    break;
	  case 1:
	    if (nibble) printf("%01d ",va);
	    else printf("%02d ",va);
	    break;
	  case 2:
	    if (nibble) printf("%01x ",va);
	    else printf("%02x ",va);
	    break;
	  case 3:
	    printf("%c", va);
	    break;
	  default:
	    printf("s%d=%02X ",i+1,va);
	  }
	}
	printf("\n");
	
      } else {
	printf("Location=%03d: failed to get data (reply %02x)\n",loc, msgin.msgnum & NX_MSG_MASK);
      }
    } else {
      printf("Location=%03d: no reply\n",loc);
    }
    fflush(stdout);
  }


  return 0;
}



int probe_bus(int fd, int protocol)
{
  nxmsg_t msgout,msgin,msgin2;
  int ret,len,type;
  uint node;

  printf("Scanning bus for nodes...\n");

  for(node=0;node<256;node++) {
    uint loc = 0;

    memset(&msgin,0,sizeof(msgin));
    memset(&msgin2,0,sizeof(msgin2));

    msgout.msgnum=NX_PROG_DATA_REQ;
    msgout.len=4;
    msgout.msg[0]=node;
    msgout.msg[1]=(loc >> 8) & 0x0f;
    msgout.msg[2]=(loc & 0xff);

    ret=nx_send_message(fd,protocol,&msgout,5,3,NX_PROG_DATA_REPLY,&msgin);
    if (ret==1) {
      if (msgin.msgnum == NX_PROG_DATA_REPLY) {
	//nx_print_msg(stdout,&msgin);
	len=(msgin.msg[3] & 0x1f) + 1;
	type=(msgin.msg[3] >> 5 & 0x07);
		      
	printf("Node=%03d: A DEVICE FOUND! (first location length=%d,type=%s)\n",node,len,nx_program_data_types[type]);
      } else {
	printf("Node=%03d: failed to get data (reply %02x)\n",node, msgin.msgnum & NX_MSG_MASK);
      }
    } else {
      printf("Node=%03d: no reply\n",node);
    }
    fflush(stdout);
  }


  return 0;
}


/* eof :-) */
