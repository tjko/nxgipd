/* configuration.c
 * 
 * Copyright (C) 2011 Timo Kokkonen. All Rights Reserved.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <mxml.h>
#include "nx-584.h"
#include "nxgipd.h"


mxml_node_t* load_xml_file(const char *filename)
{
  FILE *fp;
  mxml_node_t *tree;

  if (!filename) return NULL;
  fp = fopen(filename,"r");
  if (!fp) return NULL;

  tree=mxmlLoadFile(NULL,fp,MXML_TEXT_CALLBACK);
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
  if (node->type != type) {
    node=node->child;
    if (node && node->type != type) node=NULL;
  }
  
  return node;
}


const char* xml_whitespace_cb(mxml_node_t *node, int where)
{
  const char *name = node->value.element.name;

  if ( !strcmp(name,"AlarmZones") || !strcmp(name,"AlarmPartitions") || strstr(name,"?xml ")==name ) {
    if ( where == MXML_WS_AFTER_OPEN || where == MXML_WS_AFTER_CLOSE ) return("\n");
  } else if ( !strcmp(name,"Zone") || ~!strcmp(name,"Partition") ) {
    if (where == MXML_WS_BEFORE_OPEN || where == MXML_WS_BEFORE_CLOSE) return("  ");
    return("\n");
  } else {
    if (where == MXML_WS_BEFORE_OPEN) return("    ");
    if (where == MXML_WS_AFTER_OPEN && !node->child) return("\n");
    if (where == MXML_WS_AFTER_CLOSE) return("\n");
  }

  return NULL;
}



#define EXPAND_FILENAME(buf,dir,name) { \
            if (name[0]=='/') { strncpy(buf,name,sizeof(buf)); }	\
	    else { snprintf(buf,sizeof(buf),"%s/%s",dir,name); }	\
	 }


int load_config(const char *configfile, nx_configuration_t *config, int logtest)
{
  mxml_node_t *configxml, *node;
  int i;
  FILE *fp;
  char tmpstr[1024];
  const char *dir;

  if (!configfile || !config) return -1;

  configxml=load_xml_file(configfile);
  if (!configxml) return 1;

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","serial","device");
  if (!node) die("cannot find serial device in configuration");
  config->serial_device=strdup(node->value.text.string);

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","serial","speed");
  if (!node) die("cannot find serial speed in configuration");
  config->serial_speed=strdup(node->value.text.string);

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","serial","protocol");
  if (!node) die("cannot find serial protocol in configuration");
  if (strstr(node->value.text.string,"ascii")) {
    config->serial_protocol=NX_PROTOCOL_ASCII;
  } else if (strstr(node->value.text.string,"binary")) {
    config->serial_protocol=NX_PROTOCOL_BINARY;
  } else {
    die("invalid serial protocol setting in configuration");
  }
    

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","alarm","partitions");
  if (!node) die("cannot find alarm partitions in configuration");
  if (sscanf(node->value.text.string,"%d",&i)==1) config->partitions=i;
  else die("invalid alarm partitions setting");

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","alarm","zones");
  if (!node) die("cannot find alarm zones in configuration");
  if (sscanf(node->value.text.string,"%d",&i)==1) config->zones=i;
  else die("invalid alarm zones setting");

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","alarm","timesync");
  if (!node) die("cannot find alarm timesync in configuration");
  if (sscanf(node->value.text.string,"%d",&i)==1) config->timesync=i;
  else die("invalid alarm timesync setting");

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","alarm","statuscheck");
  if (!node) die("cannot find alarm statuscheck in configuration");
  if (sscanf(node->value.text.string,"%d",&i)==1) config->statuscheck=i;
  else die("invalid alarm statuscheck setting");


  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","shm","shmkey");
  if (!node) die("cannot find shm shmkey in configuration");
  if (sscanf(node->value.text.string,"%x",&i)==1) config->shmkey=i;
  else die("invalid shm shmkey setting");

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","shm","shmmode");
  if (!node) die("cannot find shm shmmode in configuration");
  if (sscanf(node->value.text.string,"%o",&i)==1) config->shmmode=i;
  else die("invalid shm shmmode setting");

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","shm","msgkey");
  if (!node) die("cannot find msgkey in configuration");
  if (sscanf(node->value.text.string,"%x",&i)==1) config->msgkey=i;
  else die("invalid shm msgkey setting");

  node=search_xml_tree(configxml,MXML_TEXT,3,"configuration","shm","msgmode");
  if (!node) die("cannot find msgmode in configuration");
  if (sscanf(node->value.text.string,"%o",&i)==1) config->msgmode=i;
  else die("invalid msgmode setting");


  config->syslog_mode=0;
  node=search_xml_tree(configxml,MXML_TEXT,2,"configuration","syslog");
  if (node && sscanf(node->value.text.string,"%d",&i)==1) config->syslog_mode=i;

  config->debug_mode=0;
  node=search_xml_tree(configxml,MXML_TEXT,2,"configuration","log");
  if (node && sscanf(node->value.text.string,"%d",&i)==1) config->debug_mode=i;


  node=search_xml_tree(configxml,MXML_TEXT,2,"configuration","directory");
  if (!node) die("cannot find directory element in configuration");
  dir=node->value.text.string;
  if (strlen(dir) < 1) die("directory element empty in configuration");

  node=search_xml_tree(configxml,MXML_TEXT,2,"configuration","logfile");
  if (node) {
    EXPAND_FILENAME(tmpstr,dir,node->value.text.string);
    config->log_file=strdup(tmpstr);
    if (config->debug_mode >= 0 && logtest) {
      fp=fopen(config->log_file,"a");
      if (!fp) die("cannot write to logfile: %s",config->log_file);
      fclose(fp);
    }
  }

  node=search_xml_tree(configxml,MXML_TEXT,2,"configuration","statusfile");
  if (node) {
    EXPAND_FILENAME(tmpstr,dir,node->value.text.string);
    config->status_file=strdup(tmpstr);
  }



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
      mxmlElementSetAttrf(e,"time","%lu",(unsigned long)zn->last_updated);

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
    const char *nname = node->value.element.name;
    const char *id_s, *name_s;

    if (node->type != MXML_ELEMENT) continue;

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
      }
    }

  }


  /* parse AlarmPartitions section */

  node=partitions;
  pt=NULL;
  while ((node=mxmlWalkNext(node,partitions,MXML_DESCEND))) {
    const char *nname = node->value.element.name;
    
    if (node->type != MXML_ELEMENT) continue;

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
