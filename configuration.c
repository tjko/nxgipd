/* configuration.c - load/save configuration files for nxgipd
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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <mxml.h>
#include "nx-584.h"
#include "nxgipd.h"


mxml_node_t* load_xml_file(const char *filename)
{
  FILE *fp;
  mxml_node_t *tree;

  if (!filename) return NULL;
  fp = fopen(filename, "r");
  if (!fp) return NULL;

  tree=mxmlLoadFile(NULL, fp, MXML_OPAQUE_CALLBACK);
  fclose(fp);

  return tree;
}


mxml_node_t* search_xml_tree(mxml_node_t *tree, mxml_type_t type, int len, ...)
{
  va_list args;
  mxml_node_t *node;
  char *s;

  if (!tree || len < 1) return NULL;

  node=tree;
  va_start(args,len);
  while(len-- > 0) {
    s=va_arg(args,char *);
    node=mxmlFindElement(node,node,s,NULL,NULL,MXML_DESCEND);
    if (!node) return NULL;
  }
  va_end(args);

  /* if the requested node's type doesnt match, assume its the child that we want... */
  if (mxmlGetType(node) != type) {
    node=mxmlGetFirstChild(node);
    if (node && mxmlGetType(node) != type) node=NULL;
  }

  return node;
}


const char* xml_whitespace_cb(mxml_node_t *node, int where)
{
  const char *name = mxmlGetElement(node);

  if ( !strcmp(name,"AlarmZones") || !strcmp(name,"AlarmPartitions") || strstr(name,"?xml ")==name ) {
    if ( where == MXML_WS_AFTER_OPEN || where == MXML_WS_AFTER_CLOSE ) return("\n");
  } else if ( !strcmp(name,"Zone") || !strcmp(name,"Partition") ) {
    if (where == MXML_WS_BEFORE_OPEN || where == MXML_WS_BEFORE_CLOSE) return("  ");
    return("\n");
  } else {
    if (where == MXML_WS_BEFORE_OPEN) return("    ");
    if (where == MXML_WS_AFTER_OPEN && !mxmlGetFirstChild(node)) return("\n");
    if (where == MXML_WS_AFTER_CLOSE) return("\n");
  }

  return NULL;
}



#define EXPAND_FILENAME(buf,dir,name) {					\
    if (name[0]=='/') { strlcpy(buf,name,sizeof(buf)); }		\
    else { snprintf(buf,sizeof(buf),"%s/%s",dir,name); }		\
  }


int load_config(const char *configfile, nx_configuration_t *config, int logtest)
{
  mxml_node_t *configxml, *node;
  int i;
  FILE *fp;
  char tmpstr[1024];
  const char *dir;
  struct passwd *pw;
  struct group *gr;


  if (!configfile || !config) return -1;

  /* zero out the configuration structure... */
  memset(config,0,sizeof(nx_configuration_t));

  configxml=load_xml_file(configfile);
  if (!configxml) return 1;

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","serial","device");
  if (!node) die("cannot find serial device in configuration");
  config->serial_device=strdup(mxmlGetOpaque(node));

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","serial","speed");
  if (!node) die("cannot find serial speed in configuration");
  config->serial_speed=strdup(mxmlGetOpaque(node));

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","serial","protocol");
  if (!node) die("cannot find serial protocol in configuration");
  if (strstr(mxmlGetOpaque(node),"ascii")) {
    config->serial_protocol=NX_PROTOCOL_ASCII;
  } else if (strstr(mxmlGetOpaque(node),"binary")) {
    config->serial_protocol=NX_PROTOCOL_BINARY;
  } else {
    die("invalid serial protocol setting in configuration");
  }


  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","alarm","partitions");
  if (!node) die("cannot find alarm partitions in configuration");
  if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->partitions=i;
  else die("invalid alarm partitions setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","alarm","zones");
  if (!node) die("cannot find alarm zones in configuration");
  if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->zones=i;
  else die("invalid alarm zones setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","alarm","timesync");
  if (!node) die("cannot find alarm timesync in configuration");
  if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->timesync=i;
  else die("invalid alarm timesync setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","alarm","statuscheck");
  if (!node) die("cannot find alarm statuscheck in configuration");
  if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->statuscheck=i;
  else die("invalid alarm statuscheck setting");


  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","shm","shmkey");
  if (!node) die("cannot find shm shmkey in configuration");
  if (sscanf(mxmlGetOpaque(node),"%x",&i)==1) config->shmkey=i;
  else die("invalid shm shmkey setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","shm","shmmode");
  if (!node) die("cannot find shm shmmode in configuration");
  if (sscanf(mxmlGetOpaque(node),"%o",&i)==1) config->shmmode=i;
  else die("invalid shm shmmode setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","shm","shmgroup");
  if (node) {
    if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->shm_gid=i;
    else if ((gr=getgrnam(mxmlGetOpaque(node)))) {
      config->shm_gid=gr->gr_gid;
    }
    else die("invalid shm shmgroup setting");
  } else {
    config->shm_gid=-1;
  }

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","shm","shmuser");
  if (node) {
    if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->shm_uid=i;
    else if ((pw=getpwnam(mxmlGetOpaque(node)))) {
      config->shm_uid=pw->pw_uid;
    }
    else die("invalid shm shmuser setting");
  } else {
    config->shm_uid=-1;
  }



  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","shm","msgkey");
  if (!node) die("cannot find msgkey in configuration");
  if (sscanf(mxmlGetOpaque(node),"%x",&i)==1) config->msgkey=i;
  else die("invalid shm msgkey setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","shm","msgmode");
  if (!node) die("cannot find msgmode in configuration");
  if (sscanf(mxmlGetOpaque(node),"%o",&i)==1) config->msgmode=i;
  else die("invalid shm msgmode setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","shm","msggroup");
  if (node) {
    if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->msg_gid=i;
    else if ((gr=getgrnam(mxmlGetOpaque(node)))) {
      config->msg_gid=gr->gr_gid;
    }
    else die("invalid shm msggroup setting");
  } else {
    config->msg_gid=-1;
  }

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","shm","msguser");
  if (node) {
    if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->msg_uid=i;
    else if ((pw=getpwnam(mxmlGetOpaque(node)))) {
      config->msg_uid=pw->pw_uid;
    }
    else die("invalid shm shmuser setting");
  } else {
    config->msg_uid=-1;
  }



  config->syslog_mode=0;
  node=search_xml_tree(configxml,MXML_OPAQUE,2,"configuration","syslog");
  if (node && sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->syslog_mode=i;

  config->debug_mode=0;
  node=search_xml_tree(configxml,MXML_OPAQUE,2,"configuration","log");
  if (node && sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->debug_mode=i;


  node=search_xml_tree(configxml,MXML_OPAQUE,2,"configuration","directory");
  if (!node) die("cannot find directory element in configuration");
  dir=mxmlGetOpaque(node);
  if (strlen(dir) < 1) die("directory element empty in configuration");

  node=search_xml_tree(configxml,MXML_OPAQUE,2,"configuration","logfile");
  if (node) {
    EXPAND_FILENAME(tmpstr,dir,mxmlGetOpaque(node));
    config->log_file=strdup(tmpstr);
    if (config->debug_mode >= 0 && logtest) {
      fp=fopen(config->log_file,"a");
      if (!fp) die("cannot write to logfile: %s",config->log_file);
      fclose(fp);
    }
  }

  node=search_xml_tree(configxml,MXML_OPAQUE,2,"configuration","statusfile");
  if (node) {
    EXPAND_FILENAME(tmpstr,dir,mxmlGetOpaque(node));
    config->status_file=strdup(tmpstr);
  }

  node=search_xml_tree(configxml,MXML_OPAQUE,2,"configuration","savestatus");
  if (node) {
    if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->status_save_interval=i;
    else die("invalid savestatus setting value");
  }


  node=search_xml_tree(configxml,MXML_OPAQUE,2,"configuration","alarmprogram");
  if (node) {
    EXPAND_FILENAME(tmpstr,dir,mxmlGetOpaque(node));
    config->alarm_program=strdup(tmpstr);
  }

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","triggers","logentry");
  if (!node) die("cannot find 'logentry' inside 'triggers' section in configuration");
  if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->trigger_log=i;
  else die("invalid 'triggers::logentry' setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","triggers","partitionstatus");
  if (!node) die("cannot find 'partitionstatus' inside 'triggers' section in configuration");
  if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->trigger_partition=i;
  else die("invalid 'triggers::partitionstatus' setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","triggers","zonestatus");
  if (!node) die("cannot find 'zonestatus' inside 'triggers' section in configuration");
  if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->trigger_zone=i;
  else die("invalid 'triggers::zonestatus' setting");

  node=search_xml_tree(configxml,MXML_OPAQUE,3,"configuration","triggers","maxprocesses");
  if (!node) die("cannot find 'maxprocesses' inside 'triggers' section in configuration");
  if (sscanf(mxmlGetOpaque(node),"%d",&i)==1) config->max_triggers=i;
  else die("invalid 'zonestatus' setting");



  mxmlDelete(configxml);
  return 0;
}


int save_status_xml(const char *filename, nx_system_status_t *astat)
{
  FILE *fp;
  mxml_node_t *xml,*zones,*partitions,*z,*e,*p;
  int i;
  int part_count = 0;
  int zone_count = 0;

  if (!filename || !astat) return -1;

  xml=mxmlNewXML("1.0");
  zones=mxmlNewElement(xml,"AlarmZones");
  partitions=mxmlNewElement(xml,"AlarmPartitions");


  /* loop through partitions */

  for (i=0; i<astat->last_partition; i++) {
    nx_partition_status_t *pt = &astat->partitions[i];

    if (pt->valid) {
      p=mxmlNewElement(partitions,"Partition");
      mxmlElementSetAttrf(p,"id","%u",i+1);

      e=mxmlNewElement(p,"LastChange");
      mxmlElementSetAttrf(e,"time","%lu",(unsigned long)pt->last_updated);

      part_count++;
    }
  }


  /* loop through zones */

  for (i=0; i<astat->last_zone; i++) {
    nx_zone_status_t *zn = &astat->zones[i];

    if (zn->valid) {
      z=mxmlNewElement(zones,"Zone");
      mxmlElementSetAttrf(z,"id","%u",i+1);
      mxmlElementSetAttrf(z,"name","%s",zn->name);

      e=mxmlNewElement(z,"LastChange");
      mxmlElementSetAttrf(e,"time","%lu",(unsigned long)zn->last_tripped);

      zone_count++;
    }
  }


  if (zone_count < 1) {
    mxmlDelete(xml);
    return -2;
  }


  fp=fopen(filename,"w");
  if (!fp) {
    warn("failed to create file: %s", filename);
    return -3;
  }

  mxmlSetWrapMargin(0);
  mxmlSaveFile(xml,fp,xml_whitespace_cb);
  fclose(fp);

  mxmlDelete(xml);

  return 0;
}


int load_status_xml(const char *filename, nx_system_status_t *astat)
{
  mxml_node_t *xml, *zones, *partitions, *node;
  nx_zone_status_t *zn;
  nx_partition_status_t *pt;

  if (!filename || !astat) return -1;

  xml=load_xml_file(filename);
  if (!xml) {
    warn("failed to load/parse XML file: %s", filename);
    return -2;
  }

  zones=mxmlFindElement(xml,xml,"AlarmZones",NULL,NULL,MXML_DESCEND);
  partitions=mxmlFindElement(xml,xml,"AlarmPartitions",NULL,NULL,MXML_DESCEND);
  if (!zones || !partitions) {
    warn("invalid status file: %s", filename);
    mxmlDelete(xml);
    return -3;
  }



  /* parse AlarmZones section */

  node=zones;
  zn=NULL;
  while ((node=mxmlWalkNext(node,zones,MXML_DESCEND))) {
    const char *nname = mxmlGetElement(node);
    const char *id_s, *name_s;

    if (mxmlGetType(node) != MXML_ELEMENT) continue;

    //printf("node: %s\n",(nname != NULL?nname:"NULL"));

    if (!strcmp(nname,"Zone")) {
      int id;

      id_s=mxmlElementGetAttr(node,"id");
      name_s=mxmlElementGetAttr(node,"name");

      zn=NULL;
      if (id_s && (sscanf(id_s,"%d", &id)==1)) {
	if (id > 0 && id <= NX_ZONES_MAX) {
	  zn=&astat->zones[id-1];
	  if (name_s)  {
	    if (strcmp(zn->name,name_s)) {
	      logmsg(0,"zone renamed: '%s' -> '%s'",name_s,zn->name);
	    }
	  }
	}
      }
    }

    if (!zn) continue;

    if (!strcmp(nname,"LastChange")) {
      unsigned long t;

      const char *time_s = mxmlElementGetAttr(node,"time");
      if (time_s && (sscanf(time_s,"%lu",&t)==1)) {
	logmsg(3,"restore zone last change: %s %lu (%lu)",zn->name,t,zn->last_updated);
	zn->last_updated=t;
	zn->last_tripped=t;
      }
    }

  }


  /* parse AlarmPartitions section */

  node=partitions;
  pt=NULL;
  while ((node=mxmlWalkNext(node,partitions,MXML_DESCEND))) {
    const char *nname = mxmlGetElement(node);

    if (mxmlGetType(node) != MXML_ELEMENT) continue;

    //printf("node: %s\n",(nname != NULL?nname:"NULL"));

    if (!strcmp(nname,"Partition")) {
      int id;
      const char *id_s = mxmlElementGetAttr(node,"id");

      pt=NULL;
      if (id_s && (sscanf(id_s,"%d", &id)==1)) {
	if (id > 0 && id <= NX_PARTITIONS_MAX) {
	  pt=&astat->partitions[id-1];
	  //printf("pt=%x id=%d\n",pt,id);
	}
      }
    }

    if (!pt) continue;

    if (!strcmp(nname,"LastChange")) {
      unsigned long t;

      const char *time_s = mxmlElementGetAttr(node,"time");
      if (time_s && (sscanf(time_s,"%lu",&t)==1)) {
	logmsg(3,"restore partition last change: %lu (%lu)",t,pt->last_updated);
	pt->last_updated=t;
      }
    }


  }


  mxmlDelete(xml);
  return 0;
}

/* eof :-) */
