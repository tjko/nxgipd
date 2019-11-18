/* probe.c - routines for scanning bus and displaying device configs
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
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "nx-584.h"
#include "nxgipd.h"




int read_config(int fd, int protocol, uchar node, int location)
{
  nxmsg_t msgout,msgin,msgin2;
  int ret,i,nibble,size,len,type;
  uint loc;
  int maxloc = NX_LOGICAL_LOCATION_MAX;
  int locstart = 0;


  /* ignore any pending messages */
  do {
    ret=nx_receive_message(fd,protocol,&msgin,0);
  } while (ret==1);


  printf("Scanning module %d\n",node);

  if (location >=0) {
    locstart=location;
    maxloc=location;
  }

  for(loc=locstart;loc<=maxloc;loc++) {
    int retries = 3;
    
    memset(&msgin,0,sizeof(msgin));
    memset(&msgin2,0,sizeof(msgin2));

    msgout.msgnum=NX_PROG_DATA_REQ;
    msgout.len=4;
    msgout.msg[0]=node;
    msgout.msg[1]=(loc >> 8) & 0x0f;
    msgout.msg[2]=(loc & 0xff);

    //printf("Reading location %04x (%d) [%02x %02x %02x %02x]\n",loc,loc,msgout.msgnum,msgout.msg[0],msgout.msg[1],msgout.msg[2]);
    do {
      if (retries < 3) {
	//printf("retry %d\n",retries);
	do {
	  ret=nx_receive_message(fd,protocol,&msgin,0);
	  //if (ret==1) printf("ignoring pending message: %d\n",msgin.msgnum);
	} while (ret==1);
      }
      ret=nx_send_message(fd,protocol,&msgout,5,3,NX_PROG_DATA_REPLY,&msgin);
    } while (retries-- > 0 && (ret==1 && msgin.msgnum == NX_NEGATIVE_ACK));
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

	printf("Location=%03d (len=%02d type=%3s): ",loc,len,nx_prog_datatype_str(type));
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
	    printf("[%c%c%c%c%c%c%c%c] ",
		     (va&0x01?'1':'-'),
		     (va&0x02?'2':'-'),
		     (va&0x04?'3':'-'),
		     (va&0x08?'4':'-'),
		     (va&0x10?'5':'-'),
		     (va&0x20?'6':'-'),
		     (va&0x40?'7':'-'),
		     (va&0x80?'8':'-')
		     );
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
	if ((msgin.msgnum & NX_MSG_MASK) == NX_MSG_REJECTED) {
	  printf("Location=%03d: Invalid location (not used)\n",loc);
	} else {
	  printf("Location=%03d: failed to get data (reply %02x)\n",loc, msgin.msgnum & NX_MSG_MASK);
	}
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
		      
	printf("Node=%03d: A DEVICE FOUND! (first location length=%d,type=%s)\n",
	       node,len,nx_prog_datatype_str(type));
      } else {
	printf("Node=%03d: failed to get data (reply %02x)\n",
	       node, msgin.msgnum & NX_MSG_MASK);
      }
    } else {
      printf("Node=%03d: no reply\n",node);
    }
    fflush(stdout);
  }


  return 0;
}


int detect_panel(int fd, int protocol, nx_system_status_t *astat, nx_interface_status_t *istatus, int verbose)
{
  nxmsg_t msgout,msgin;
  int ret,retry,i;
  uchar zonelist[4] = {192,48,16,8};
  int maxzones = 0;
  int maxparts = 0;
  const char *model = NULL;
  uchar partmask = 0;
  uchar panel_id = 0;
  char tmpstr[16];

  printf("Detecting panel model...\n");

  /* request interface configuration */
  msgout.msgnum=NX_INT_CONFIG_REQ;
  msgout.len=1;
  retry=0;
  do {
    ret=nx_send_message(fd,config->serial_protocol,&msgout,5,3,NX_INT_CONFIG_MSG,&msgin);
    if (ret < 0) warn("Failed to send message to panel");
    if (ret == 0) warn("No response from panel");
  } while (ret != 1 && retry++ < 3);
  if (ret == 1) {
    if (msgin.msgnum != NX_INT_CONFIG_MSG) {
      warn("Panel refused interface status command");
      return -1;
    }
    if (verbose) {
      nx_print_msg(stdout,&msgin);
    }
  } else {
    warn("failed to estabilish communications with NX device");
    return -2;
  }
  process_message(&msgin,1,0,astat,istatus);


  /* get alarm system status */
  msgout.msgnum=NX_SYS_STATUS_REQ;
  msgout.len=1;
  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_SYS_STATUS_MSG,&msgin);
  if (!(ret == 1 && msgin.msgnum == NX_SYS_STATUS_MSG)) return -3;
  panel_id=msgin.msg[0];
  printf("Panel ID: %u\n",panel_id);

  /* look for max partitions supported */

  if ((istatus->sup_cmd_msgs[0] & 0x80) == 0) {
    warn("Required Partitions Snapshot Request command not enabled");
    return -4;
  }

  msgout.msgnum=NX_PART_SNAPSHOT_REQ;
  msgout.len=1;
  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_PART_SNAPSHOT_MSG,&msgin);
  if (!(ret == 1 && (msgin.msgnum == NX_PART_SNAPSHOT_MSG))) {
    warn("Failed to get Partition status");
    return -5;
  }
  for (i=0;i<NX_PARTITIONS_MAX;i++) {
    if (msgin.msg[i] && 0x01) maxparts=i+1;
  }
  for (i=0; i<maxparts; i++) {
    partmask |= (0x01<<i);
  }



  /* look for max zones supported */

  if ((istatus->sup_cmd_msgs[0] & 0x10) == 0) {
    warn("Required Zone Status Request command not enabled");
    return -6;
  }

  for (i=0; i < sizeof(zonelist); i++) {
    /* get zone status */
    msgout.msgnum=NX_ZONE_STATUS_REQ;
    msgout.len=2;
    msgout.msg[0]=zonelist[i]-1;
    ret=nx_send_message(fd,protocol,&msgout,5,3,NX_ZONE_STATUS_MSG,&msgin);
    if (ret != 1 || msgin.msgnum != NX_ZONE_STATUS_MSG) {
      if (verbose)
	printf("no reply for zone %d (%d)\n",zonelist[i],msgin.msgnum);
    } else {
      //{int j; for (j=0;j<7;j++) { printf(" %02x",msgin.msg[j]); }; printf(" (%d)\n",maxzones); }
      if ( maxzones == 0 
	   && (msgin.msg[1] & partmask) != 0  
	   && (msgin.msg[1] & ~partmask) == 0 ) {
	maxzones=zonelist[i];
	break;
      }
    }
  }
  


  /* check for "known" Panel ID */
  for(i=0; nx_panel_models[i].id >= 0; i++) {
    if (nx_panel_models[i].id == panel_id) {
      model=nx_panel_models[i].name;
      maxzones=nx_panel_models[i].max_zones;
      maxparts=nx_panel_models[i].max_partitions;
      break;
    }
  }

  if (maxparts == 0) 
    maxparts=1;
  if (maxzones == 0)
    maxzones=8;
  if (!model) {
    snprintf(tmpstr,sizeof(tmpstr),"Unknown (%d)",panel_id);
    model=tmpstr;
  }
  

  printf("Detected panel: %s (maxzones=%d, maxpartitions=%d)\n",model,maxzones,maxparts);

  if (config->partitions == 0)
    config->partitions = maxparts;
  if (config->zones <= 0)
    config->zones = maxzones;

  strlcpy(astat->panel_model,model,sizeof(astat->panel_model));

  return 0;
}

/* eof :-) */
