/* process.c - routines for hanlding NX protocol messages
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
#include <stdlib.h>
#include <time.h>
#include "nx-584.h"
#include "nxgipd.h"




#define LOG_STATUS_CHANGE(oldstate,newstate,chg,t,f) {			\
    if (oldstate != newstate) {						\
      char *logtext = (newstate ? t : f);				\
      if (!init_mode && logtext != NULL) logmsg(0,"%s", logtext);	\
      oldstate=newstate;						\
      chg++;								\
    }									\
  }

#define CHECK_STATUS_CHANGE(t,v,chg,tmp,tmsg,fmsg) {			\
    if (v != t) {							\
      char *logtext = (t ? tmsg : fmsg);				\
      v=t;								\
      if (logtext != NULL) {						\
	if (tmp[0])							\
	  strlcat(tmp,", ",sizeof(tmp));				\
	strlcat(tmp,logtext,sizeof(tmp));				\
	chg++;								\
      }									\
    }									\
  }

#define SET_MSG_REPLY(reply,msg,retval,loglevel, ...) {			\
    logmsg(loglevel,__VA_ARGS__);					\
    set_message_reply(reply,msg,retval,__VA_ARGS__);			\
  }



void process_message(nxmsg_t *msg, int init_mode, int verbose_mode, nx_system_status_t *astat, nx_interface_status_t *istatus) 
{
  unsigned char msgnum;
  int i;

  if (!msg) return;

  if (verbose_mode) nx_print_msg(stdout,msg);

  msgnum = msg->msgnum & NX_MSG_MASK;

  switch (msgnum) {

  case NX_INT_CONFIG_MSG:
    memcpy(istatus->version,&msg->msg[0],4);
    istatus->version[4]=0;
    memcpy(istatus->sup_trans_msgs,&msg->msg[4],2);
    memcpy(istatus->sup_cmd_msgs,&msg->msg[6],4);
    break;

  case NX_ZONE_NAME_MSG:
    {
      int zone = msg->msg[0];
      if (astat->zones[zone].valid) {
	memcpy(astat->zones[zone].name,&msg->msg[1],16);
	astat->zones[zone].name[16]=0;
      }
    }
    break;

  case NX_ZONE_STATUS_MSG:
    {
      int zonenum = msg->msg[0];

      if (zonenum >= astat->last_zone) break;

      if (astat->zones[zonenum].valid) {
	int fault = (msg->msg[5] & 0x01 ? 1:0);
	int tamper = (msg->msg[5] & 0x02 ? 1:0);
	int trouble = (msg->msg[5] & 0x04 ? 1:0);
	int bypass = (msg->msg[5] & 0x08 ? 1:0);
	int inhibited = (msg->msg[5] & 0x10 ? 1:0);
	int low_battery = (msg->msg[5] & 0x20 ? 1:0);
	int loss_supervision = (msg->msg[5] & 0x40 ? 1:0);
	int alarm_mem = (msg->msg[6] & 0x01 ? 1:0);
	int bypass_mem = (msg->msg[6] & 0x02 ? 1:0);
	char tmp[255];
	nx_zone_status_t *zone = &astat->zones[zonenum];
	int change=0;
	int change2=0;

	tmp[0]=0;
	CHECK_STATUS_CHANGE(fault,zone->fault,change,tmp,"Fault","Ok");
	CHECK_STATUS_CHANGE(tamper,zone->tamper,change,tmp,"Tamper","Tamper Clear");
	CHECK_STATUS_CHANGE(trouble,zone->trouble,change,tmp,"Trouble","Trouble Clear");
	CHECK_STATUS_CHANGE(bypass,zone->bypass,change2,tmp,"Bypass enabled","Bypass disabled");
	CHECK_STATUS_CHANGE(inhibited,zone->inhibited,change2,tmp,"Inhibited","(Inhibited)");
	CHECK_STATUS_CHANGE(low_battery,zone->low_battery,change2,tmp,"Battery Low","Battery OK");
	CHECK_STATUS_CHANGE(loss_supervision,zone->loss_supervision,change2,tmp,"Supervision Lost","Supervision OK");
	CHECK_STATUS_CHANGE(alarm_mem,zone->alarm_mem,change2,tmp,"Alarm Memory","Alarm Memory Clear");
	CHECK_STATUS_CHANGE(bypass_mem,zone->bypass_mem,change2,tmp,"Bypass Memory","Bypass Memory Clear");

	if (zone->partition_mask != msg->msg[1]) zone->partition_mask=msg->msg[1];
	if (zone->type_flags[0] != msg->msg[2]) zone->type_flags[0]=msg->msg[2];
	if (zone->type_flags[1] != msg->msg[3]) zone->type_flags[1]=msg->msg[3];
	if (zone->type_flags[2] != msg->msg[4]) zone->type_flags[2]=msg->msg[4];


	if (change || change2) {
	  zone->last_updated=msg->r_time;
	  if (!init_mode) {
	    logmsg(0,"%s zone status: %02d %s: %s",
		   (zone->bypass ? "bypassed" : (astat->armed ? "armed" : "normal")),
		   zonenum+1,
		   zone->name,
		   tmp);

	    if (change)
	      zone->last_tripped=msg->r_time;

	    if (config->trigger_enable && 
		( (change && config->trigger_zone > 0) ||
		  (config->trigger_zone > 1) ) )
	      run_zone_trigger(zonenum+1,zone->name,fault,bypass,trouble,tamper,astat->armed,tmp);
	  }
	}
      	if (zone->last_updated <=0) zone->last_updated=msg->r_time;
      }
    }
    break;


  case NX_ZONE_SNAPSHOT_MSG:
    {
      int offset = msg->msg[0];
      int zonenum;

      for (zonenum=offset; zonenum<offset+16; zonenum++) {
	if (astat->zones[zonenum].valid) {
	  char tmp[255];
	  int change = 0;
	  int change2 = 0;
	  int fault, bypass, trouble, alarm_mem;
	  nx_zone_status_t *zone = &astat->zones[zonenum];
	  uchar s = msg->msg[1+((zonenum-offset)/2)];

	  if (zonenum % 2 == 1) s = s >> 4;
	  fault = (s & 0x01 ? 1:0);
	  bypass = (s & 0x02 ? 1:0);
	  trouble = (s & 0x04 ? 1:0);
	  alarm_mem = (s & 0x08 ? 1:0);

	  tmp[0]=0;
	  CHECK_STATUS_CHANGE(fault,zone->fault,change,tmp,"Fault","Ok");
	  CHECK_STATUS_CHANGE(bypass,zone->bypass,change2,tmp,"Bypass enabled","Bypass disabled");
	  CHECK_STATUS_CHANGE(trouble,zone->trouble,change,tmp,"Trouble","Trouble Clear");
	  CHECK_STATUS_CHANGE(alarm_mem,zone->alarm_mem,change2,tmp,"Alarm Memory","Alarm Memory Clear");
	  
	  if (change || change2) {
	    zone->last_updated=msg->r_time;
	    if (!init_mode) {
	      logmsg(0,"%s zone status (snapshot): %02d %s: %s",
		     (zone->bypass ? "bypassed" : (astat->armed ? "armed" : "normal")),
		     zonenum+1,
		     zone->name,
		     tmp);
	      
	      if (change)
		zone->last_tripped=msg->r_time;

	      if (config->trigger_enable && 
		  ( (change && config->trigger_zone > 0) ||
		    (config->trigger_zone > 1) ) )
		run_zone_trigger(zonenum+1,zone->name,fault,bypass,trouble,zone->tamper,astat->armed,tmp);
	    }
	  }
	}
      }
    }
    break;


  case NX_PART_STATUS_MSG:
    {
      int partnum = msg->msg[0];

      if (partnum >= astat->last_partition) break;
      if (astat->partitions[partnum].valid) {
	char tmp[1024];
	nx_partition_status_t *part = &astat->partitions[partnum];
	int change = 0;
	int change2 = 0;
	int p,armed_count;



	/* same statuses as we get from partitions snapshot message */
	tmp[0]=0;

	/* armed */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x40?1:0),part->armed,change,tmp,"Armed","Not Armed");
	/* ready */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x04?1:0),part->ready,change2,tmp,"Ready","Not Ready");
	/* stay mode */
	CHECK_STATUS_CHANGE((msg->msg[3]&0x04?1:0),part->stay_mode,change,tmp,"Stay Mode On","Stay Mode Off");
	/* chime mode */
	CHECK_STATUS_CHANGE((msg->msg[3]&0x08?1:0),part->chime_mode,change,tmp,"Chime Mode On","Chime Mode Off");
	/* entry delay */
	CHECK_STATUS_CHANGE((msg->msg[3]&0x10?1:0),part->entry_delay,change,tmp,"Entry Delay Start","Entry Delay End");
	/* exit delay */
	CHECK_STATUS_CHANGE((msg->msg[3]&0xc0?1:0),part->exit_delay,change,tmp,"Exit Delay Start","Exit Delay End");
	/* previous alarm */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x01?1:0),part->prev_alarm,change,tmp,"Previous Alarm","Previous Alarm Clear");

	/* additional statuses...*/

	/* fire trouble */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x02?1:0),part->fire_trouble,change,tmp,"Fire Trouble","Fire Trouble Clear");
	/* fire */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x04?1:0),part->fire,change,tmp,"Fire","Fire Clear");
	/* buzzer on */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x08?1:0),part->buzzer_on,change,tmp,"Buzzer On","Buzzer Off");
	/* instant */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x80?1:0),part->instant,change,tmp,"Instant Enabled","Instant Disabled");

	/* siren on */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x02?1:0),part->siren_on,change,tmp,"Siren On","Siren Off");
	/* steady siren on */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x04?1:0),part->steadysiren_on,change,tmp,"Steady Siren On","Steady Siren Off");
	/* alarm memory */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x08?1:0),part->alarm_mem,change,tmp,"Alarm Memory","Alarm Memory Cleared");
	/* tamper */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x10?1:0),part->tamper,change,tmp,"Tamper","Tamper Clear");
	/* cancel entered */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x20?1:0),part->cancel_entered,change,tmp,"Cancel Entered",NULL);
	/* code entered */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x40?1:0),part->code_entered,change,tmp,"Code Entered",NULL);

	/* silent exit enabled */
	CHECK_STATUS_CHANGE((msg->msg[3]&0x02?1:0),part->silent_exit,change,tmp,"Silent Exit Enabled","Silent Exit Disabled");

	/* sensor low battery */
	CHECK_STATUS_CHANGE((msg->msg[4]&0x40?1:0),part->low_battery,change,tmp,"Sensor Battery Low","Sensor Battery OK");
	/* sensor loss of supervision */
	CHECK_STATUS_CHANGE((msg->msg[4]&0x80?1:0),part->lost_supervision,change,tmp,"Sensor Supervision Lost","Sensor Supervision OK");


	/* zones bypassed */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x01?1:0),part->zones_bypassed,change,tmp,"Zone(s) Bypassed","No Zone(s) Bypassed");
	/* valid pin */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x10?1:0),part->valid_pin,change,tmp,"Valid PIN",NULL);
	/* chime on */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x20?1:0),part->chime_on,change,tmp,"Chime On","Chime Off");
	/* error beep on */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x40?1:0),part->errorbeep_on,change,tmp,"Error Beep On","Error Beep Off");
	/* tone on */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x80?1:0),part->tone_on,change,tmp,"Tone On","Tone Off");

	/* alarm sent */
	CHECK_STATUS_CHANGE((msg->msg[7]&0x1c?1:0),part->alarm_sent,change,tmp,"Alarm Sent",NULL);
	/* keyswitch armed */
	CHECK_STATUS_CHANGE((msg->msg[7]&0x40?1:0),part->keyswitch_armed,change,tmp,"Keyswitch Armed","Keyswitch Unarmed");

	

	/* last user */
	if (msg->msg[5] != part->last_user) {
	  char tstr[64];
	  part->last_user=msg->msg[5];
	  change++;
	  if (tmp[0]) strlcat(tmp,", ",sizeof(tmp));
	  if (part->last_user == NX_NO_USER) 
	    snprintf(tstr,sizeof(tstr),"Last User = <None>");
	  else 
	    snprintf(tstr,sizeof(tstr),"Last User = %03u",part->last_user);
	  strlcat(tmp,tstr,sizeof(tmp));
	}

	if (change || change2) {
	  part->last_updated=msg->r_time;
	  if (!init_mode) {
	    logmsg(0,"Partition %d status change: %s",partnum+1,tmp);

	    if (config->trigger_enable && 
		( (change && config->trigger_zone > 0) ||
		  (change2 && config->trigger_zone > 1) ) 
		) {
	      run_partition_trigger(partnum+1,tmp,part->armed,part->ready,
				    part->stay_mode,part->chime_mode,part->entry_delay,
				    part->exit_delay,part->prev_alarm);
	    }
	  }
	}

	/* update global armed flag... */
	armed_count=0;
	for(p=0;p < astat->last_partition;p++) {
	  if (astat->partitions[p].valid && astat->partitions[p].armed) armed_count++;
	}
	astat->armed=(armed_count > 0 ? 1:0);

      }
    }
    break;

  case NX_PART_SNAPSHOT_MSG:
    {
      int armed_count=0;

      for(i=0;i<NX_PARTITIONS_MAX;i++) {
	char tmp[255];
	unsigned char s = msg->msg[i];
	unsigned char v = (s & 0x01 ? 1:0);
	unsigned char t;
	nx_partition_status_t *part = &astat->partitions[i]; 
	int change = 0;
	int change2 = 0;
	
	if (part->valid != v) {
	  part->valid=v;
	  logmsg(1,"Partition %d %s",i+1,(v ? "Active":"Disabled"));
	}
	
	if (part->valid > 0) {
	  tmp[0]=0;
	  
	  /* armed */
	  t=(s & 0x04 ? 1:0);
	  if (t) armed_count++;
	  CHECK_STATUS_CHANGE(t,part->armed,change,tmp,"Armed","Not Armed");
	  
	  /* ready */
	  t=(s & 0x02 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->ready,change2,tmp,"Ready","Not Ready");
	  
	  /* stay mode */
	  t=(s & 0x08 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->stay_mode,change,tmp,"Stay Mode On","Stay Mode Off");
	  
	  /* chime mode */
	  t=(s & 0x10 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->chime_mode,change,tmp,"Chime Mode On","Chime Mode Off");
	  
	  /* entry delay */
	  t=(s & 0x20 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->entry_delay,change,tmp,"Entry Delay Start","Entry Delay End");
	  
	  /* exit delay */
	  t=(s & 0x40 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->exit_delay,change,tmp,"Exit Delay Start","Exit Delay End");

	  /* previous alarm */
	  t=(s & 0x80 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->prev_alarm,change,tmp,"Previous Alarm","Previous Alarm Cleared");

	  
	  if (change || change2) {
	    part->last_updated=msg->r_time;
	    if (!init_mode) {
	      logmsg(0,"Partition %d status change: %s",i+1,tmp);

	      if (config->trigger_enable && 
		  ( (change && config->trigger_zone > 0) ||
		    (change2 && config->trigger_zone > 1) ) 
		  ) {
		run_partition_trigger(i+1,tmp,part->armed,part->ready,
				      part->stay_mode,part->chime_mode,part->entry_delay,
				      part->exit_delay,part->prev_alarm);
	      }
	    }
	  }
	}
      }

      /* update global armed flag... */
      astat->armed=(armed_count > 0 ? 1:0);

    }
    break;

  case NX_SYS_STATUS_MSG:
    {
      int change = 0;
      const char *panel_model = NULL;
      int i;

      /* panel ID (model) */
      if (astat->panel_id != (uchar) msg->msg[0]) {
	logmsg(3,"Panel ID: %d",msg->msg[0]);
	for(i=0; nx_panel_models[i].id >= 0; i++) {
	  if (nx_panel_models[i].id == msg->msg[0]) {
	    panel_model=nx_panel_models[i].name;
	    break;
	  }
	}
	if (panel_model)
	  logmsg(0,"Panel Model: %s", panel_model);
	else
	  logmsg(0,"Panel Model: Unknown (Panel ID=%d). Please report your alarm panel model and id to nxgipd developers.",
		 msg->msg[0]);
	astat->panel_id=msg->msg[0];
	change++;
      }

      /* panel communication stack (log) pointer */
      if (astat->comm_stack_ptr != (uchar) msg->msg[10]) {
	logmsg(3,"panel communication stack pointer change: %d -> %d",astat->comm_stack_ptr,msg->msg[10]);
	astat->comm_stack_ptr=msg->msg[10];
	change++;
      }


      LOG_STATUS_CHANGE(astat->partitions[0].valid,(msg->msg[9]&0x01 ? 1:0),change,"Partition 1 Active","Partition 1 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[1].valid,(msg->msg[9]&0x02 ? 1:0),change,"Partition 2 Active","Partition 2 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[2].valid,(msg->msg[9]&0x04 ? 1:0),change,"Partition 3 Active","Partition 3 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[3].valid,(msg->msg[9]&0x08 ? 1:0),change,"Partition 4 Active","Partition 4 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[4].valid,(msg->msg[9]&0x10 ? 1:0),change,"Partition 5 Active","Partition 5 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[5].valid,(msg->msg[9]&0x20 ? 1:0),change,"Partition 6 Active","Partition 6 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[6].valid,(msg->msg[9]&0x40 ? 1:0),change,"Partition 7 Active","Partition 7 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[7].valid,(msg->msg[9]&0x80 ? 1:0),change,"Partition 8 Active","Partition 8 Disabled");


      LOG_STATUS_CHANGE(astat->line_seizure,(msg->msg[1]&0x01 ? 1:0),change,"Line Seizure start","Line Seizure end");
      LOG_STATUS_CHANGE(astat->off_hook,(msg->msg[1]&0x02 ? 1:0),change,"Off Hook","On Hook");
      LOG_STATUS_CHANGE(astat->handshake_rcvd,(msg->msg[1]&0x04 ? 1:0),change,"Initial Handshake Start","Initial Handshake End");
      LOG_STATUS_CHANGE(astat->download_in_progress,(msg->msg[1]&0x08 ? 1:0),change,"Download in progress","Download end");
      LOG_STATUS_CHANGE(astat->dialerdelay_in_progress,(msg->msg[1]&0x10 ? 1:0),change,"Dialer delay in progress","Dialer Delay end");
      LOG_STATUS_CHANGE(astat->backup_phone,(msg->msg[1]&0x20 ? 1:0),change,"Using backup phone","Using primary phone");
      LOG_STATUS_CHANGE(astat->listen_in,(msg->msg[1]&0x40 ? 1:0),change,"Listen in active","Listen in inactive");
      LOG_STATUS_CHANGE(astat->twoway_lockout,(msg->msg[1]&0x80 ? 1:0),change,"Two way lockout start","Two way lockout end");

      LOG_STATUS_CHANGE(astat->ground_fault,(msg->msg[2]&0x01 ? 1:0),change,"Ground Fault","Ground OK");
      LOG_STATUS_CHANGE(astat->phone_fault,(msg->msg[2]&0x02 ? 1:0),change,"Phone Fault","Phone OK");
      LOG_STATUS_CHANGE(astat->fail_to_comm,(msg->msg[2]&0x04 ? 1:0),change,"Fail to communicate","Communications Restored");
      LOG_STATUS_CHANGE(astat->fuse_fault,(msg->msg[2]&0x08 ? 1:0),change,"Fuse Fault","Fuse OK");
      LOG_STATUS_CHANGE(astat->box_tamper,(msg->msg[2]&0x10 ? 1:0),change,"Box Tamper Detected","Box Tamper Cleared");
      LOG_STATUS_CHANGE(astat->siren_tamper,(msg->msg[2]&0x20 ? 1:0),change,"Siren Tamper Detected","Siren Tamper Cleared");
      LOG_STATUS_CHANGE(astat->low_battery,(msg->msg[2]&0x40 ? 1:0),change,"Low Battery","Battery OK");
      LOG_STATUS_CHANGE(astat->ac_fail,(msg->msg[2]&0x80 ? 1:0),change,"AC Fail","AC OK");

      LOG_STATUS_CHANGE(astat->exp_tamper,(msg->msg[3]&0x01 ? 1:0),change,"Expander box Tamper Detected","Expander box Tamper Cleared");
      LOG_STATUS_CHANGE(astat->exp_ac_fail,(msg->msg[3]&0x02 ? 1:0),change,"Expander AC failure","Expander AC OK");
      LOG_STATUS_CHANGE(astat->exp_low_battery,(msg->msg[3]&0x04 ? 1:0),change,"Expander battery LOW","Expander battery OK");
      LOG_STATUS_CHANGE(astat->exp_loss_supervision,(msg->msg[3]&0x08 ? 1:0),change,"Expander loss of supervision","Expander supervision restored");
      LOG_STATUS_CHANGE(astat->exp_aux_overcurrent,(msg->msg[3]&0x10 ? 1:0),change,"Expander aux ouput overcurrent","Expander aux output current normal");
      LOG_STATUS_CHANGE(astat->aux_com_channel_fail,(msg->msg[3]&0x20 ? 1:0),change,"Aux communication channel FAILURE","Aux communication channel OK");
      LOG_STATUS_CHANGE(astat->exp_bell_fault,(msg->msg[3]&0x40 ? 1:0),change,"Expander bell fault","Expander bell OK");
      
      LOG_STATUS_CHANGE(astat->sixdigitpin,(msg->msg[4]&0x01 ? 1:0),change,"6 Digit PIN enabled","4 Digit PIN enabled");
      LOG_STATUS_CHANGE(astat->prog_token_inuse,(msg->msg[4]&0x02 ? 1:0),change,"GO TO PROGRAM code entered",NULL);
      LOG_STATUS_CHANGE(astat->pin_local_dl,(msg->msg[4]&0x04 ? 1:0),change,"PIN required for local download","PIN not required for local download");
      LOG_STATUS_CHANGE(astat->global_pulsing_buzzer,(msg->msg[4]&0x08 ? 1:0),change,"Global pulsing buzzer ON","Global pulsing buzzer OFF");
      LOG_STATUS_CHANGE(astat->global_siren,(msg->msg[4]&0x10 ? 1:0),change,"Global siren ON","Global siren OFF");
      LOG_STATUS_CHANGE(astat->global_steady_siren,(msg->msg[4]&0x20 ? 1:0),change,"Global steady siren ON","Global steady siren OFF");
      LOG_STATUS_CHANGE(astat->bus_seize_line,(msg->msg[4]&0x40 ? 1:0),change,"Bus device has line seized",NULL);
      LOG_STATUS_CHANGE(astat->bus_sniff_mode,(msg->msg[4]&0x80 ? 1:0),change,"Bus device has requested sniff mode",NULL);

      LOG_STATUS_CHANGE(astat->battery_test,(msg->msg[5]&0x01 ? 1:0),change,"Dynamic Battery Test start","Dynamic Battery Test end");
      LOG_STATUS_CHANGE(astat->ac_power,(msg->msg[5]&0x02 ? 1:0),change,"AC power ON","AC power OFF");
      LOG_STATUS_CHANGE(astat->low_battery_memory,(msg->msg[5]&0x04 ? 1:0),change,"Low battery memory","Low battery memory cleared");
      LOG_STATUS_CHANGE(astat->ground_fault_memory,(msg->msg[5]&0x08 ? 1:0),change,"Ground fault memory","Ground fault memory cleared");
      LOG_STATUS_CHANGE(astat->fire_alarm_verification,(msg->msg[5]&0x10 ? 1:0),change,"Fire Alarm verification timing start","Fire Alarm verification timing end");
      LOG_STATUS_CHANGE(astat->smoke_power_reset,(msg->msg[5]&0x20 ? 1:0),change,"Smoke detector power reset",NULL);
      LOG_STATUS_CHANGE(astat->line_power_50hz,(msg->msg[5]&0x40 ? 1:0),change,"50 Hz line power detected","60 Hz line power detected");
      LOG_STATUS_CHANGE(astat->high_voltage_charge,(msg->msg[5]&0x80 ? 1:0),change,"Timing a high voltage battery charge start","Timing a high voltage battery charge end");
      
      LOG_STATUS_CHANGE(astat->comm_since_autotest,(msg->msg[6]&0x01 ? 1:0),change,"Communication since last autotest","No Communication since last autotest");
      LOG_STATUS_CHANGE(astat->powerup_delay,(msg->msg[6]&0x02 ? 1:0),change,"Power up delay in progress","Power up delay end");
      LOG_STATUS_CHANGE(astat->walktest_mode,(msg->msg[6]&0x04 ? 1:0),change,"Walk test mode ON","Walk test mode OFF");
      LOG_STATUS_CHANGE(astat->system_time_loss,(msg->msg[6]&0x08 ? 1:0),change,"System time lost","System time restored");
      LOG_STATUS_CHANGE(astat->enroll_request,(msg->msg[6]&0x10 ? 1:0),change,"Enroll start","Enroll end");
      LOG_STATUS_CHANGE(astat->testfixture_mode,(msg->msg[6]&0x20 ? 1:0),change,"Test fixture mode ON","Test fixture mode OFF");
      LOG_STATUS_CHANGE(astat->controlshutdown_mode,(msg->msg[6]&0x40 ? 1:0),change,"Control shutdown mode ON","Control shutdown mode OFF");
      LOG_STATUS_CHANGE(astat->cancel_window,(msg->msg[6]&0x80 ? 1:0),change,"Timing cancel window start","Timing cancel window end");
      
      LOG_STATUS_CHANGE(astat->callback_in_progress,(msg->msg[7]&0x80 ? 1:0),change,"Call back in progress","Call back end");
      
      LOG_STATUS_CHANGE(astat->phone_line_fault,(msg->msg[8]&0x01 ? 1:0),change,"Phone line Faulted","Phone line OK");
      LOG_STATUS_CHANGE(astat->voltage_present_int,(msg->msg[8]&0x02 ? 1:0),change,"Voltage present interrupt active","Voltage present interrupt inactive");
      LOG_STATUS_CHANGE(astat->house_phone_offhook,(msg->msg[8]&0x04 ? 1:0),change,"House phone OFF hook","House phone ON hook");
      LOG_STATUS_CHANGE(astat->phone_monitor,(msg->msg[8]&0x08 ? 1:0),change,"Phone line monitor enabled","Phone line monitor disabled");
      LOG_STATUS_CHANGE(astat->phone_sniffing,(msg->msg[8]&0x10 ? 1:0),change,"Phone sniffing start","Phone sniffing end");
      /* LOG_STATUS_CHANGE(astat->offhook_memory,(msg->msg[8]&0x20 ? 1:0),change,"Last read was off hook","(Last read was off hook)"); */
      LOG_STATUS_CHANGE(astat->listenin_request,(msg->msg[8]&0x40 ? 1:0),change,"Listen in requested",NULL);
      LOG_STATUS_CHANGE(astat->listenin_trigger,(msg->msg[8]&0x80 ? 1:0),change,"Listen in trigger",NULL);
      
      if (change) astat->last_updated=msg->r_time;
    }
    break;


  case NX_X10_RCV_MSG:
    {
      uchar house = msg->msg[0] + 'A';
      uchar unit = msg->msg[1] + 1;
      char *func;
      
      switch (msg->msg[2]) {

      case NX_X10_ALL_UNITS_OFF: 
	func="All units OFF"; break;
      case NX_X10_ALL_LIGHTS_ON: 
	func="All lights ON";break;
      case NX_X10_ON: 
	func="On"; break;
      case NX_X10_OFF:
	func="Off"; break;
      case NX_X10_DIM:
	func="Dim"; break;
      case NX_X10_BRIGHT:
	func="Bright"; break;
      case NX_X10_ALL_LIGHTS_OFF:
	func="All lights OFF"; break;
      default: 
	func="Unknown";

      }

      logmsg(1,"X-10 Message Received: House=%c, Unit=%d, Function=%02x (%s)",
	     house,unit,msg->msg[2],func);
    }
    break;


  case NX_LOG_EVENT_MSG:
    {
      nx_log_event_t *e;
      uchar num = msg->msg[0];
      uchar maxnum = msg->msg[1];

      if (astat->last_log != maxnum) {
	if (astat->last_log != 0)
	  logmsg(2,"panel log size change: %d -> %d",astat->last_log,maxnum);
	astat->last_log=maxnum;
      }
      
      /* save log message */
      e=&astat->log[num];
      e->msgno=msg->msgnum;
      e->no=num;
      e->logsize=maxnum;
      e->type=msg->msg[2];
      e->num=msg->msg[3];
      e->part=msg->msg[4];
      e->month=msg->msg[5];
      e->day=msg->msg[6];
      e->hour=msg->msg[7];
      e->min=msg->msg[8];
      
      logmsg((NX_IS_NONREPORTING_EVENT(e->type)?1:0),"%s",nx_log_event_str(e));

      if (config->trigger_enable &&
	  ( ((config->trigger_log > 0) && NX_IS_REPORTING_EVENT(e->type)) ||
	    ((config->trigger_log > 1) && (e->type==40 || e->type==41)) ||
	    (config->trigger_log > 2) ) ) {
	run_log_trigger(e);
      }
    }
    break;


  case NX_KEYPAD_MSG_RCVD:
    {
      uchar keypad = msg->msg[0];
      uchar key = msg->msg[1];

      logmsg(1,"Terminal Mode keypad keystroke received (keypad=%d,len=%d): %02x",
	     keypad,msg->len,key);
    }
    break;




  case NX_CMD_FAILED:
    logmsg(2,"command /request failed");
    break;

  case NX_POSITIVE_ACK:
    logmsg(2,"positive ACK received");
    break;

  case NX_NEGATIVE_ACK:
    logmsg(2,"negative ACK received");
    break;

  case NX_MSG_REJECTED:
    logmsg(2,"message rejected");
    break;



  default:
    if (verbose_mode) 
      fprintf(stderr,"%s: unhandled message 0x%02x\n",nx_timestampstr(msg->r_time),msgnum); 
    logmsg(1,"unhandled message 0x%02x",msgnum);
    
  }

}



void process_command(int fd, int protocol, const nx_ipc_msg_t *msg, 
		     nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply)
{
  nxmsg_t msgout,msgin;
  int func,ret,len,offset;
  char *funcname = NULL;
  const uchar *data;

  if (!msg || !istatus || !reply) return;

  data=msg->data;
  func=(data[0]<<8 | data[1]);

  switch (func) {
  case NX_KEYPAD_FUNC_STAY:
    funcname="Stay (one button arm / toggle interiors)";
    break;
  case NX_KEYPAD_FUNC_CHIME:
    funcname="Chime (toggle chime mode)";
    break;
  case NX_KEYPAD_FUNC_EXIT:
    funcname="Exit (one button arm / toggle instant)";
    break;
  case NX_KEYPAD_FUNC_BYPASS:
    funcname="Bypass interiors";
    break;
  case NX_KEYPAD_FUNC_GROUP_BYPASS:
    funcname="Group Bypass";
    break;
  case NX_KEYPAD_FUNC_SMOKE_RESET:
    funcname="Smoke detector reset";
    break;
  case NX_KEYPAD_FUNC_START_SOUNDER:
    funcname="Start keypad sounder";
    break;
  case NX_KEYPAD_FUNC_ARM_AWAY:
    funcname="Arm in Away mode";
    break;
  case NX_KEYPAD_FUNC_ARM_STAY:
    funcname="Arm in Stay mode";
    break;
  case NX_KEYPAD_FUNC_DISARM:
    funcname="Disarm partition";
    break;
  case NX_KEYPAD_FUNC_SILENCE:
    funcname="Turn off any sounder or alarm";
    break;
  case NX_KEYPAD_FUNC_CANCEL:
    funcname="Cancel (alarm)";
    break;
  case NX_KEYPAD_FUNC_AUTO_ARM:
    funcname="Initiate Auto-Arm";
    break;

  default:
    funcname=NULL;
  }

  if (!funcname) {
    SET_MSG_REPLY(reply,msg,-1,0,"Invalid command message received: 0x%04x",func);
    return;
  }


  /* check if required commands are enabled on the interface */
  if (data[0] == NX_PRI_KEYPAD_FUNC_PIN) {
    if ((istatus->sup_cmd_msgs[3] & 0x10) == 0) {
      logmsg(0,"Keypad function ignored (partitions=0x%02x): %s",data[2],funcname);
      SET_MSG_REPLY(reply,msg,-1,0,
		    "Ignoring disabled command: Primary Keypad function with PIN command (0x3c)");
      return;
    }
  } 
  else if (data[0] == NX_SEC_KEYPAD_FUNC) {
    if ((istatus->sup_cmd_msgs[3] & 0x40) == 0) {
      logmsg(0,"Keypad function ignored (partitions=0x%02x): %s",data[2],funcname);
      SET_MSG_REPLY(reply,msg,-1,0,
		    "Ignoring disabled command: Secondary Keypad Function with PIN (0x3e)");
      return;
    }
  } 


  msgout.msgnum=data[0];
  if (data[0] == NX_PRI_KEYPAD_FUNC_PIN) {
    len=6;
    offset=3;
    msgout.msg[0]=data[3]; // PIN digits 1&2
    msgout.msg[1]=data[4]; // PIN digits 3&4
    msgout.msg[2]=data[5]; // PIN digits 5&6
  } else {
    len=3;
    offset=0;
  }
  msgout.len=len;
  msgout.msg[offset]=data[1]; // keypad function
  msgout.msg[offset+1]=data[2]; // partition mask


  logmsg(2,"Sending keypad function command: %s (partitions=0x%02x)",funcname,data[2]);

  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_POSITIVE_ACK,&msgin);
  memset(msgout.msg,0,5); // clear buffer so PIN won't be left in memory
  if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK) {
    SET_MSG_REPLY(reply,msg,0,1,
		  "Keypad function success (partitions=0x%02x): %s",data[2],funcname);
  } else if (ret == 1 && msgin.msgnum == NX_MSG_REJECTED) {
    SET_MSG_REPLY(reply,msg,1,0,
		  "Keypad function rejected (partitions=0x%02x): %s",data[2],funcname);
  } else {
    SET_MSG_REPLY(reply,msg,2,0,
		  "Keypad function failed (partitions=0x%02x,ret=%d,msg=0x%02x): %s",
		  data[2],ret,msgin.msgnum,funcname);
  }

}



void process_keypadmsg_command(int fd, int protocol, const nx_ipc_msg_t *msg, 
			       nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply)
{
  nxmsg_t msgout,msgin;
  int ret,loc;
  int count=0;
  const uchar *data;

  if (!msg || !istatus || !reply) return;

  data=msg->data;

  if (data[2+16+16] != 0) {
    /* text string in the message should terminate to a null... */
    SET_MSG_REPLY(reply,msg,-1,0,
		  "process_keypad_message: invalid message ignored");
    return;
  }

  if ((istatus->sup_cmd_msgs[1] & 0x08) == 0) {
    SET_MSG_REPLY(reply,msg,-1,0,
		  "Send Keypad Text Message not enabled. Message not sent: '%s'",
		  (char*)&data[2]);
    return;
  }


  msgout.msgnum=NX_KEYPAD_MSG_SEND;
  msgout.len=12;
  msgout.msg[0]=data[0];
  msgout.msg[1]=0;


  logmsg(1,"Sending keypad text (keypad=%d)...",data[0]);

  /* we need to send the text in four 8 character blocks */
  for (loc=0;loc<4;loc++) {
    msgout.msg[2]=loc*8;
    memcpy(&msgout.msg[3],data+2+(loc*8),8); 


    ret=nx_send_message(fd,protocol,&msgout,5,3,NX_POSITIVE_ACK,&msgin);
    if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK) {
      logmsg(3,"Keypad text message success (keypad=%d loc=%d)",data[0],loc*8);
      count++;
    } else {
      SET_MSG_REPLY(reply,msg,1,0,"Keypad text message failed (keypad=%d). No such keypad?",data[0]);
      break;
    }
  }


  if (count >= 4)  {
    logmsg(1,"Keypad text set successfully (keypad=%d): '%s'",data[0],(char*)&data[2]);

    if (data[1] > 0) {
      /* set keypad to terminal mode to display the message */
      if ((istatus->sup_cmd_msgs[1] & 0x10) == 0) {
	SET_MSG_REPLY(reply,msg,-1,0,"Keypad Terminal Mode Request not supported. Message not displayed.");
	return;
      }
      
      msgout.msgnum=NX_KEYPAD_TM_REQ;
      msgout.len=3;
      msgout.msg[0]=data[0];
      msgout.msg[1]=data[1];

      ret=nx_send_message(fd,protocol,&msgout,5,3,NX_POSITIVE_ACK,&msgin);
      if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK) {
	SET_MSG_REPLY(reply,msg,0,1,"Keypad terminal mode enabled for %d seconds (keypad=%d)",data[1],data[0]);
      } else {
	SET_MSG_REPLY(reply,msg,2,0,"Keypad terminal mode failed (keypad=%d)",data[0]);
      }
      
    }
  }

}


int read_program_data(int fd, int protocol, int device, int location, int mode, 
		      char **datastr, uchar *datatype, uchar *datanibble) 
{
  nxmsg_t msgout,msgin,msgin2;
  char tmp[16];
  char buf[1024];
  int ret,i,nibble,size,len,type;

  if (!datastr || !datatype) return -10;
  if (device < 0 || device > NX_BUS_ADDRESS_MAX) return -11;
  if (location < 0 || location > NX_LOGICAL_LOCATION_MAX) return -12;


  memset(&msgin,0,sizeof(msgin));
  memset(&msgin2,0,sizeof(msgin2));

  msgout.msgnum=NX_PROG_DATA_REQ;
  msgout.len=4;
  msgout.msg[0]=device;
  msgout.msg[1]=(location >> 8) & 0x0f;
  msgout.msg[2]=(location & 0xff);
  
  
  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_PROG_DATA_REPLY,&msgin);
  if (ret==1 && msgin.msgnum == NX_PROG_DATA_REPLY) {
    nibble=((msgin.msg[1] & 0x10) == 0x10 ? 1 : 0);
    len=(msgin.msg[3] & 0x1f) + 1;
    type=(msgin.msg[3] >> 5 & 0x07);
    if (nibble) 
      size=len/2;
    else 
      size=len;
      
    if (size > 8) {
      /* there is more data to be read, so request second segment */ 
      msgout.msg[1] |= 0x40;
      ret=nx_send_message(fd,protocol,&msgout,5,3,NX_PROG_DATA_REPLY,&msgin2);
      if (!(ret==1 && msgin2.msgnum == NX_PROG_DATA_REPLY)) {
	/* failed to get second data segment */
	return -2;
      }
    }
  } else {
    /* failed to get first data segment */ 
    return -1;
  }



  /* reformat the program data */    
  if (type == 3) {
    buf[0]='"';
    buf[1]=0;
  } else {
    buf[0]=0;
  }

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
    
    if (mode > 0) {
      switch (type) {
      case 0:
	// binary
	snprintf(tmp,sizeof(tmp),"[%c%c%c%c%c%c%c%c] ",
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
	// decimal
	if (nibble) snprintf(tmp,sizeof(tmp),"%01d ",va);
	else snprintf(tmp,sizeof(tmp),"%02d ",va);
	break;
      case 2:
	// hexadecimal
	if (nibble) snprintf(tmp,sizeof(tmp),"%01x ",va);
	else snprintf(tmp,sizeof(tmp),"%02x ",va);
	break;
      case 3:
	// ASCII
	snprintf(tmp,sizeof(tmp),"%c", va);
	break;
      default:
	snprintf(tmp,sizeof(tmp),"%02X ",va);
      }
    } else {
      snprintf(tmp,sizeof(tmp),"%02x",va);
    }

    strlcat(buf,tmp,sizeof(buf));
  }

  if (type == 3) 
    strlcat(buf,"\"",sizeof(buf));

  *datatype = type;
  *datanibble = nibble;
  *datastr = strdup(buf);
  if (!datastr) return -3;

  return len;
}


void process_get_program_command(int fd, int protocol, const nx_ipc_msg_t *msg, nx_interface_status_t *istatus,
				 nx_ipc_msg_reply_t *reply)
{
  int ret,loc;
  char *datastr = NULL;
  uchar datatype = 0;
  uchar datanibble = 0;
  const uchar* data;

  if (!msg || !istatus || !reply) return;

  data=msg->data;

  loc = (data[1]&0xf)<<8 | data[2];

  if ((istatus->sup_cmd_msgs[2] & 0x01) == 0) {
    SET_MSG_REPLY(reply,msg,-1,0,
		  "Program Data Request not enabled. Message not sent (device=%d,loc=%d).",
		  data[0],loc);
    return;
  }

  logmsg(1,"Sending Program Data Request (device=%d,location=%d)...",data[0],loc);

  ret = read_program_data(fd,protocol,data[0],loc,1,
			  &datastr,&datatype,&datanibble);
  if (ret > 0) {
    SET_MSG_REPLY(reply,msg,0,1,
		  "Program data (dev=%03d,loc=%03d,len=%02d,type=%s): %s",
		  data[0],loc,ret,nx_prog_datatype_str(datatype),datastr);
    free(datastr);
  } else {
    SET_MSG_REPLY(reply,msg,1,0,
		      "Program Data Request failed (device=%d,location=%d).",data[0],loc); 
  }

}


void process_set_program_command(int fd, int protocol, const nx_ipc_msg_t *msg, nx_interface_status_t *istatus,
				 nx_ipc_msg_reply_t *reply)
{
  nxmsg_t msgout,msgout2,msgin;
  int ret,loc,i;
  char *datastr = NULL;
  uchar datatype = 0;
  uchar datanibble = 0;
  int datalen = 0;
  int outcount = 0;
  const uchar* data;

  if (!msg || !istatus || !reply) return;

  data=msg->data;

  loc = (data[1]&0xf)<<8 | data[2];

  if ((istatus->sup_cmd_msgs[2] & 0x01) == 0) {
    SET_MSG_REPLY(reply,msg,-1,0,
		  "Program Data Request not enabled. Message not sent (device=%d,loc=%d).",
		  data[0],loc);
    return;
  }

  if ((istatus->sup_cmd_msgs[2] & 0x02) == 0) {
    SET_MSG_REPLY(reply,msg,-1,0,
		  "Program Data Command not enabled. Message not sent (device=%d,loc=%d).",
		  data[0],loc);
    return;
  }

  logmsg(2,"Start Programming (device=%d,location=%d)...",data[0],loc);


  if (loc == 910) {
    /* this is special location used to do "factory reset" on the device/module */
    datatype=0;
    datalen=0;
  } else {

    ret = read_program_data(fd,protocol,data[0],loc,1,
			  &datastr,&datatype,&datanibble);
    if (ret > 0) {
      datalen=ret;
      
      logmsg(2,"Current Program data (dev=%03d,loc=%03d,len=%02d,type=%s): %s",
	     data[0],loc,ret,nx_prog_datatype_str(datatype),datastr);
      free(datastr);
    } else {
      SET_MSG_REPLY(reply,msg,1,0,
		    "Program Data Request failed (device=%d,location=%d)",data[0],loc); 
      return;
    }


    if (data[3] != datatype) {
      SET_MSG_REPLY(reply,msg,2,0,
		    "Location data type mismatch (device=%d,location=%d): location stores '%s' data",
		    data[0],loc,nx_prog_datatype_str(datatype));
      return;
    }

    if (datatype == 3) {
      if (data[4] > datalen) {
	SET_MSG_REPLY(reply,msg,2,0,
		      "Location length mismatch (device=%d,location=%d): location stores string (max length %d)",
		      data[0],loc,datalen);
	return;
      }
      /* nxcmd sends any ASCII data padded so padding needed here */
    } else {
      if (data[4] != datalen) {
	SET_MSG_REPLY(reply,msg,2,0,
		      "Location length mismatch (device=%d,location=%d): location stores %d segments",
		      data[0],loc,datalen);
	return;
      }
    }
  }


  /* prepare program commands */

  memset(&msgout,0,sizeof(msgout));
  msgout.msgnum=NX_PROG_DATA_CMD;
  msgout.len=13;
  msgout.msg[0]=data[0];
  msgout.msg[1]=(datanibble?0x10:0x00)|0x20|(data[1] & 0x0f);
  msgout.msg[2]=data[2];
  msgout.msg[3]=((datatype & 0x07)<<5)|((datalen-1) & 0x01f);

  memcpy(&msgout2,&msgout,sizeof(msgout2));
  msgout2.msg[1] |= 0x40;
  
  for(i=0; i < datalen; (datanibble ? i+=2 : i++)) {
    uchar *p = (outcount < 8 ? &msgout.msg[4+outcount] : &msgout2.msg[4+(outcount-8)]);
    if (datanibble) {
      *p= ((data[i+6] & 0x0f) << 4) | (data[i+5] & 0x0f);
    } else {
      *p=data[5+i];
    }
    outcount++;
  }


  /* send first program command */

  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_POSITIVE_ACK,&msgin);
  if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK && loc != 910) {
    logmsg(3,"Program Data Command (#1) succeeded");
  } else {
    if (loc == 910) 
      SET_MSG_REPLY(reply,msg,0,0,"Factory Reset request sent (device=%d)",data[0])
    else 
      SET_MSG_REPLY(reply,msg,3,0,
		    "Program Data Command (#1) failed (device=%d,location=%d,type=%d,len=%d): %x",
		    data[0],loc,datatype,datalen,msgin.msgnum);
    return;
  }

  /* send seconf program command (if needed) */

  if (outcount > 8) {
    ret=nx_send_message(fd,protocol,&msgout2,5,3,NX_POSITIVE_ACK,&msgin);
    if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK) {
      logmsg(3,"Program Data Command (#2) succeeded");
    } else {
      SET_MSG_REPLY(reply,msg,4,0,"Program Data Command (#2) failed (device=%d,location=%d,type=%d,len=%d)",
		    data[0],loc,datatype,datalen);
      return;
    }
  }

  logmsg(3,"Programming successful (device=%d,location=%d,len=%d,type=%s,nibble=%d)",
	 data[0],loc,datalen,nx_prog_datatype_str(datatype),datanibble);



  /* re-read programmed location to verify the new contents... */

  ret = read_program_data(fd,protocol,data[0],loc,1,
			  &datastr,&datatype,&datanibble);
  if (ret > 0) {
    datalen=ret;

    SET_MSG_REPLY(reply,msg,0,1,"Programming complete (dev=%03d,loc=%03d,len=%02d,type=%s): %s",
		  data[0],loc,ret,nx_prog_datatype_str(datatype),datastr);
    free(datastr);
  } else {
    SET_MSG_REPLY(reply,msg,5,0,
		      "Programming complete, but Program Data Request failed (device=%d,location=%d)",data[0],loc); 
    return;
  }

}


void process_zone_bypass_command(int fd, int protocol, const nx_ipc_msg_t *msg, 
				 nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply)
{
  nxmsg_t msgout,msgin;
  int ret;
  const uchar *data;

  if (!msg || !istatus || !reply) return;

  data=msg->data;

  if ((istatus->sup_cmd_msgs[3] & 0x80) == 0) {
    SET_MSG_REPLY(reply,msg,-1,0,"Zone Bypass Toggle not supported. Message not sent (Zone=%d).",
		  data[0]+1);
    return;
  }

  logmsg(2,"Sending Bypass Toggle (zone=%d)...",data[0]+1);


  msgout.msgnum=NX_ZONE_BYPASS_TOGGLE;
  msgout.len=2;
  msgout.msg[0]=data[0];

  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_POSITIVE_ACK,&msgin);
  if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK) {
    SET_MSG_REPLY(reply,msg,0,1,"Zone Bypass Toggle success: Zone=%d",data[0]+1);
  } else {
    SET_MSG_REPLY(reply,msg,1,0,"Zone Bypass Toggle failure: Zone=%d",data[0]+1);
  }

}



void process_x10_command(int fd, int protocol, const nx_ipc_msg_t *msg, 
			 nx_interface_status_t *istatus, nx_ipc_msg_reply_t *reply)
{
  nxmsg_t msgout,msgin;
  int ret;
  uchar house;
  int unit;
  const uchar *data;

  if (!msg || !istatus || !reply) return;

  data=msg->data;

  if ((istatus->sup_cmd_msgs[1] & 0x02) == 0) {
    SET_MSG_REPLY(reply,msg,-1,0,"Send X-10 Message not enabled. Message not sent (Zone=%d).",
		  data[0]+1);
    return;
  }


  msgout.msgnum=NX_X10_SEND_MSG;
  msgout.len=4;
  msgout.msg[0]=(data[0] & 0x0f);
  msgout.msg[1]=(data[1] & 0x0f);;
  msgout.msg[2]=data[2];

  house=msgout.msg[0] + 'A';
  unit=msgout.msg[1] + 1;


  logmsg(2,"Sending X-10 Message (house=%c, unit=%d, func=%02x)...",house,unit,data[2]);

  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_POSITIVE_ACK,&msgin);
  if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK) {
    SET_MSG_REPLY(reply,msg,0,1,"Send X-10 Message success: House=%c, Unit=%d, Function=%02x",house,unit,data[2]);
  } else {
    SET_MSG_REPLY(reply,msg,1,0,"Send X-10 Message failure: House=%c, Unit=%d, Function=%02x",house,unit,data[2]);
  }

}


void process_set_clock(int fd, int protocol, nx_system_status_t *astat,
		       const nx_ipc_msg_t *msg, nx_interface_status_t *istatus,
		       nx_ipc_msg_reply_t *reply)
{
  nxmsg_t msgout,msgin;
  struct tm tt;
  time_t t = time(NULL);
  int ret; 
  int cmdmode = 0;

  if (!astat) return;
  if (msg && istatus && reply) cmdmode=1;


  if (istatus && (istatus->sup_cmd_msgs[3] & 0x08) == 0) {
    if (cmdmode) {
      SET_MSG_REPLY(reply,msg,-1,0,"Set Clock / Calendar Command not enabled. Message not sent.");
    } else {
      logmsg(0,"Set Clock / Calendar Command not enabled. Message not sent.");
    }
    return;
  }



  if (localtime_r(&t,&tt)) {
    msgout.msgnum=NX_SET_CLOCK_CMD;
    msgout.len=7;
    msgout.msg[0]=(tt.tm_year % 100);
    msgout.msg[1]=tt.tm_mon+1;
    msgout.msg[2]=tt.tm_mday;
    msgout.msg[3]=tt.tm_hour;
    msgout.msg[4]=tt.tm_min;
    msgout.msg[5]=tt.tm_wday+1;
    logmsg(2,"setting panel time to: %02d-%02d-%02d %02d:%02d weedkay=%d",
	   msgout.msg[0],msgout.msg[1],msgout.msg[2],msgout.msg[3],
	   msgout.msg[4],msgout.msg[5]);
    ret=nx_send_message(fd,protocol,&msgout,5,3,NX_POSITIVE_ACK,&msgin);
    if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK) {
      if (cmdmode) {
	SET_MSG_REPLY(reply,msg,0,1,"panel clock synchronized successfully");
      } else {
	logmsg(1,"panel clock synchronized successfully");
      }
    } else {
      if (cmdmode) {
	SET_MSG_REPLY(reply,msg,1,0,"failed to set panel clock");
      } else {
	logmsg(0,"failed to set panel clock");
      }
    }
  } else {
    logmsg(1,"localtime_r() call failed");
  }
  
  astat->last_timesync=t;
}



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


  logmsg(0,"Fetching log entries from alarm...");

  while (i < last) {
    msgout.msgnum=NX_LOG_EVENT_REQ;
    msgout.len=2;
    msgout.msg[0]=i;
    ret=nx_send_message(fd,protocol,&msgout,5,3,NX_LOG_EVENT_MSG,&msgin);
    if (ret==1 && msgin.msgnum == NX_LOG_EVENT_MSG) {
      process_message(&msgin,0,0,astat,istatus);
      last=msgin.msg[1];
    } else {
      logmsg(0,"failed to get log entry: %d",i);
    } 
    i++;
  }

  return 0;
}



int get_system_status(int fd, int protocol, nx_system_status_t *astat, nx_interface_status_t *istatus)
{
  int ret;
  nxmsg_t msgout,msgin;
  int skip_zone_names = 0;
  int i;


  /* default make sure all zones/partitions are disabled initially */
  for (i=0;i<NX_PARTITIONS_MAX;i++) 
    astat->partitions[i].valid=-1;
  for (i=0;i<NX_ZONES_MAX;i++) {
    astat->zones[i].valid=-1;
    astat->zones[i].last_updated=0;
    astat->zones[i].num=i+1;
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
  if ((istatus->sup_cmd_msgs[1] & 0x01) == 0) {
    logmsg(0,"System Status Request command not enabled");
    die("System Status Request command not enabled");
  }
  if ((istatus->sup_cmd_msgs[0] & 0x80) == 0) {
    logmsg(0,"Partitions Snapshot Request command not enabled");
    die("Partitions Snapshot Request command not enabled");
  }
  if ((istatus->sup_cmd_msgs[0] & 0x40) == 0) {
    logmsg(0,"Partition Status Request command not enabled");
    die("Partition Status Request command not enabled");
  }
  if ((istatus->sup_cmd_msgs[0] & 0x08) == 0) {
    logmsg(0,"Zone Name Request command not enabled. Unable to get zone names.");
    skip_zone_names=1;
  }
  if ((istatus->sup_cmd_msgs[0] & 0x10) == 0) {
    logmsg(0,"Zone Status Request command not enabled");
    die("Zone Status Request command not enabled");
  }


  /* get alarm system status */
  msgout.msgnum=NX_SYS_STATUS_REQ;
  msgout.len=1;
  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_SYS_STATUS_MSG,&msgin);
  if (!(ret == 1 && msgin.msgnum == NX_SYS_STATUS_MSG)) return -1;
  process_message(&msgin,0,0,astat,istatus);


  /* get partition statuses */
  logmsg(0,"Querying partition statuses...");
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
      process_message(&msgin,0,0,astat,istatus);
    }
  }


 
  /* get zone info & names */
  astat->last_zone=((config->zones > 0 && config->zones <= NX_ZONES_MAX) ? config->zones : 48);
  logmsg(0,"Querying zone names and statuses...");

  for (i=0;i<astat->last_zone;i++) {
    astat->zones[i].valid=1;

    /* get zone name */
    if (skip_zone_names) {
      snprintf(astat->zones[i].name,sizeof(astat->zones[i].name),"Zone %02d",i+1);
    } else {
      msgout.msgnum=NX_ZONE_NAME_REQ;
      msgout.len=2;
      msgout.msg[0]=i;
      ret=nx_send_message(fd,protocol,&msgout,5,3,NX_ZONE_NAME_MSG,&msgin);
      if (ret != 1 || msgin.msgnum != NX_ZONE_NAME_MSG) { 
	logmsg(1,"failed to get name for zone %d (no NX-148E present?)",i+1);
	snprintf(astat->zones[i].name,sizeof(astat->zones[i].name),"Zone %02d",i+1);
	skip_zone_names++;
      } else {
	process_message(&msgin,1,0,astat,istatus);
      }
    } 

    /* get zone status */
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


  if (config->status_file) {
    logmsg(0,"loading status file: %s",config->status_file);
    int r = load_status_xml(config->status_file,astat);
    if (r != 0) {
      logmsg(0,"error loading status file: %d",r);
    }
  }


  for (i=0;i<astat->last_zone;i++) {
    logmsg(1,"Zone %2d %16s %10s %8s %s",i+1,
	   astat->zones[i].name,
	   (astat->zones[i].bypass?"Bypassed":"Active"),
	   (astat->zones[i].fault?"Fault":"OK"),
	   (astat->zones[i].last_tripped > 0 ? timestampstr(astat->zones[i].last_tripped):"n/a")
	   );
  }

    
  return 0;
}


/* eof :-) */
