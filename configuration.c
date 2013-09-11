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

  if ( !strcmp(name,"ZWaveNetwork") || strstr(name,"?xml ")==name ) {
    if ( where == MXML_WS_AFTER_OPEN || where == MXML_WS_AFTER_CLOSE ) return("\n");
  } else if ( !strcmp(name,"Node") ) {
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

  mxmlDelete(configxml);
  return 0;
}





/* eof :-) */
