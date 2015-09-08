/* nxcmd.c
 * 
 * Tool to send commands to alarm panel via nxgipd daemon.
 * Loosely based on nxcmd created by David Whaley
 * 
 * Copyright (C) 2015 Timo Kokkonen <tjko@iki.fi>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#if HAVE_GETOPT_H && HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "getopt.h"
#endif
#define DEBUG 0

#include "nxgipd.h"


int verbose_mode = 0;
int msgid = -1;
nx_configuration_t configuration;
nx_configuration_t *config = &configuration;
nx_shm_t *shm = NULL;
int shmid = -1;


/* Based on old GNU getpass() implementation
   http://www.gnu.org/s/hello/manual/libc/getpass.html */

int getpassword(const char *prompt, char **lineptr, size_t *n, FILE *stream)
{
  struct termios old, new;
  int nread;
  int istty;
  char *p;

  /* sanity check */
  if (!lineptr || !n || !stream)
    return -4;

  istty = isatty(fileno(stream));

  /* Turn echoing off, if reading from a terminal */
  if (istty) {
    if (tcgetattr (fileno (stream), &old) != 0) 
      return -3;
    new = old;
    new.c_lflag &= ~ECHO;
    if (tcsetattr (fileno (stream), TCSAFLUSH, &new) != 0)
      return -2;
  }

  /* Display prompt, if reading from a terminal */
  if (istty && prompt) 
    printf("%s", prompt);

  /* Read the password. */
  nread = getline (lineptr, n, stream);

  if (istty) 
    printf("\n");

  /* Restore terminal. */
  if (istty) {
    (void) tcsetattr (fileno (stream), TCSAFLUSH, &old);
  }

  /* remove any trailing new-line */
  if (nread > 0) {
    p=(*lineptr) + nread -1 ; 
    while (p >= *lineptr && (*p == 10 || *p == 13)) {
      *p--=0;
      nread--;
    }
  }

  return nread;
}


void parse_program_data(int datatype, char *progdata, int *datalen, int argc, char **argv)
{
  int i,j,val;
  int len = 0;


  if (argc > 32)
    die("too many arguments to setprogram");

  if (datatype == 3 && argc > 1)
    die("ASCII format data should be in one block");

  memset(progdata,0,32);

  switch (datatype) {

  case 0: // binary
    {
      if (argc > 16)
	die("too many binary arguments to setprogram");
      for(i=0; i<argc; i++) {
	val=0;
	for(j=0; j<strlen(argv[i]); j++) {
	  if (isdigit(argv[i][j])) {
	    int tmp = argv[i][j] - '0';
	    if (tmp > 8) die("invalid bit defined '%s'",argv[i]);
	    if (tmp > 0) {
	      //printf("%d\n",tmp);
	      val |= (1 << (tmp-1));
	    }
	  }
	}
	//printf("val:%x\n",val);
	progdata[i]=val;
	len++;
      }
    }
    break;

  case 1: // decimal
    {
      for(i=0;i<argc;i++) {
	if (sscanf(argv[i],"%d",&val) != 1)
	  die("invalid decimal data value '%s'",argv[i]);
	if (val > 255)
	  die("too large decimal value '%s'",argv[i]);
	progdata[i]=val;
	len++;
      }
    }
    break;

  case 2: // hexadecimal
    {
      for(i=0;i<argc;i++) {
	if (sscanf(argv[i],"%x",&val) != 1)
	  die("invalid hexadecimal data value '%s'",argv[i]);
	if (val > 0xff) 
	  die("too large hexadecimal value '%s'",argv[i]);
	progdata[i]=val;
	len++;
      }
    }
    break;

  case 3: // ascii
    {
      len=strlen(argv[0]);
      if (len > 16)
	die("too long ascii string: '%s'",argv[0]);
      for(i=0; i<len; i++) {
	progdata[i]=argv[0][i];
      }
    }
    break;

  default:
    die("internal error: parse_program_data failed");
  }

  // for(i=0; i <argc; i++) printf("argv[%d]='%s'\n",i,argv[i]);

  *datalen=len;
}

void print_usage()
{
  fprintf(stderr,"Usage: %s [OPTIONS] <command>\n\n",program_name);
  fprintf(stderr,
	  " Commands (no PIN required):\n"
	  "  exit                                Exit (one button arm / toggle instant)\n"
	  "  stay                                Stay (one button arm / toggle interiors)\n"
	  "  chime                               Toggle chime mode\n"
	  "  bypass                              Bypass interiors\n"
	  "  grpbypass                           Group bypass\n"
	  "  smokereset                          Smoke detector reset\n"
	  "  sounder                             Start keypad sounder\n\n"
	  "  zonebypass <n>                      Toggle zone bypass status\n"
	  "  x10 <house> <unit> <func>           Send X-10 Message/Command\n"
	  "  message <n> <line1> <line2> <sec>   Display text message on keypad\n"
	  "  getprogram <dev> <loc>              Display program data from device\n"
	  "  setprogram <dev> <loc> <type> -- <data1> ... <dataN>\n"
	  "                                      Program a device location\n"
	  "\n Commands (PIN required):\n"
	  "  armaway                            Arm in Away mode\n"
	  "  armstay                            Arm in Stay mode\n"
 	  "  disarm                             Disarm partition\n"
	  "  silence                            Turn off any sounder or alarm\n"
	  "  cancel                             Cancel alarm\n"
	  "  autoarm                            Initiate auto-arm\n\n"
	  " Options:\n"
	  "  --config=<configfile>              use specified config file\n"
	  "  -c <configfile>\n"
	  "  --partition=<n>, -p <n>            partition for the command (default 1)\n"
	  "  --help, -h                         display this help and exit\n"
	  "  --nowait, -n                       do not wait for response from server\n"
	  "  --timeout=<n>, -t <n>              timeout for waiting response from server (default 10)\n"
	  "  --verbose, -v                      enable verbose output to stdout\n"
	  "  --version, -V                      print program version\n"
	  "\n");
}



#define MESSAGE_LINE_LEN 16

int main(int argc, char **argv)
{
  int args = 0;
  int opt_index = 0;
  uchar partition = 0;
  char *config_file = CONFIG_FILE;
  int opt,i;
  int nxcmd = 0;
  int msgtype = 0;
  int zone = -1;
  int device = -1;
  int location = -1;
  int datatype = -1;
  int keypad = -1;
  int msgtime = -1;
  int x10house = 0;
  int x10unit = 0;
  int x10func = 0;
  int nowait = 0;
  int timeout = 10;
  int force_mode = 0;
  char text1[MESSAGE_LINE_LEN+1];
  char text2[MESSAGE_LINE_LEN+1];
  char progdata[32];
  int datalen = -1;
  nx_ipc_msg_t  ipcmsg;
  const char* cmd = NULL;
  char *pin = NULL;
  size_t pinsize = 0;
  time_t tm;

  struct option long_options[] = {
    {"config",1,0,'c'},
    {"help",0,0,'h'},
    {"partition",1,0,'p'},
    {"verbose",0,0,'v'},
    {"version",0,0,'V'},
    {"nowait",0,0,'n'},
    {"timeout",1,0,'t'},
    {"force",0,0,'f'},
    {NULL,0,0,0}
  };
  program_name="nxcmd";



  while ((opt=getopt_long(argc,argv,"t:nvVhc:p:",long_options,&opt_index)) != -1) {
    switch (opt) {
      
    case 'c':
      config_file=strdup(optarg);
      break;

    case 'f':
      force_mode=1;
      break;

    case 'n':
      nowait=1;
      break;

    case 'p':
      if (sscanf(optarg,"%d",&i)==1) {
	if (i > 0 && i<=NX_PARTITIONS_MAX) {
	  partition |= (0x01 <<(i-1));
	}
	else {
	  fprintf(stderr,"invalid partition specified (valid values: 1..8)\n");
	  exit(1);
	}
      } else {
	fprintf(stderr,"invalid partition specified (valid values: 1..8)\n");
	exit(1);
      }
      break;

    case 't':
      if (sscanf(optarg,"%d",&i)==1) {
	if (i < 0) {
	  fprintf(stderr,"invalid timeout value\n");
	  exit(1);
	}
	timeout=i;
      } else {
	fprintf(stderr,"invalid timeout value\n");
	exit(1);
      }
      break;

    case 'v':
      verbose_mode=1;
      break;

    case 'V':
      fprintf(stderr,"%s v%s (%s)  %s\n"
	      "Copyright (C) 2009-2015 Timo Kokkonen. All Rights Reserved.\n",
	      program_name,VERSION,BUILDDATE,HOST_TYPE);
      exit(0);
      
    case 'h':
    default:
      print_usage();
      exit(1);
    }
  }


  /* check for non-option arguments */
  if (optind < argc) {
    cmd=argv[optind];
    args=argc-optind;
  } else {
    warn("missing command");
    print_usage();
    exit(1);
  }

  if (verbose_mode) 
    printf("Loading configuration...\n");

  if (load_config(config_file,config,0))
    die("failed to open configuration file: %s",config_file);




  /* get message queue id */
  msgid = msgget(config->msgkey,0);
  if (msgid < 0) {
    if (errno==ENOENT) {
      die("no message queue found (server not running?)");
    }
    else if (errno==EACCES) {
      die("no permissions to access message queue for the server");
    }
    else {
      die("msgget() failed: %d (errno=%d)",strerror(errno),errno);
    }
  }


  /* initialize shared memory segment */

  shmid = shmget(config->shmkey,sizeof(nx_shm_t),0);
  if (shmid < 0) {
    if (errno==EINVAL) 
      die("nxstat version mismatch with daemon version (shared memory segment wrong size)");
    if (errno==ENOENT)
      die("cannot find share memory segment (server not running?)");
    die("shmget() failed: %s (%d)\n",strerror(errno),errno);
  }
  shm = shmat(shmid,NULL,SHM_RDONLY);
  if (shm == (void*)-1)  
    die("shmat() failed: %d (%s)\n",errno,strerror(errno));

  if (strcmp(shm->shmversion, SHMVERSION))
    die("nxstat version mismatch with daemon (shared memory) version: %s vs %s", 
	SHMVERSION, shm->shmversion);


  if (verbose_mode) {
    printf("IPC msg: key=0x%08x id=%d\n",config->msgkey,msgid);
    printf("Partitions: 0x%02x\n",partition);
  }



  /* check the command */
  if (!strcasecmp(cmd,"chime")) nxcmd=NX_KEYPAD_FUNC_CHIME;
  else if (!strcasecmp(cmd,"stay")) nxcmd=NX_KEYPAD_FUNC_STAY;
  else if (!strcasecmp(cmd,"exit")) nxcmd=NX_KEYPAD_FUNC_EXIT;
  else if (!strcasecmp(cmd,"bypass")) nxcmd=NX_KEYPAD_FUNC_BYPASS;
  else if (!strcasecmp(cmd,"grpbypass")) nxcmd=NX_KEYPAD_FUNC_GROUP_BYPASS;
  else if (!strcasecmp(cmd,"smokereset")) nxcmd=NX_KEYPAD_FUNC_SMOKE_RESET;
  else if (!strcasecmp(cmd,"sounder")) nxcmd=NX_KEYPAD_FUNC_START_SOUNDER;
  else if (!strcasecmp(cmd,"armaway")) nxcmd=NX_KEYPAD_FUNC_ARM_AWAY;
  else if (!strcasecmp(cmd,"armstay")) nxcmd=NX_KEYPAD_FUNC_ARM_STAY;
  else if (!strcasecmp(cmd,"disarm")) nxcmd=NX_KEYPAD_FUNC_DISARM;
  else if (!strcasecmp(cmd,"silence")) nxcmd=NX_KEYPAD_FUNC_SILENCE;
  else if (!strcasecmp(cmd,"cancel")) nxcmd=NX_KEYPAD_FUNC_CANCEL;
  else if (!strcasecmp(cmd,"autoarm")) nxcmd=NX_KEYPAD_FUNC_AUTO_ARM;
  else if (!strcasecmp(cmd,"zonebypass")) {
    msgtype=NX_IPC_MSG_BYPASS;
    if (args > 1) 
      sscanf(argv[optind+1],"%d",&zone);
    if (zone < 1 || zone > NX_ZONES_MAX) 
      die("zonebypass option requires zone argument");
  } 
  else if (!strcasecmp(cmd,"getprogram")) {
    msgtype=NX_IPC_MSG_GET_PROG;
    if (args > 2) {
      sscanf(argv[optind+1],"%d",&device);
      sscanf(argv[optind+2],"%d",&location);
    }
    if (device < 0 || device > NX_BUS_ADDRESS_MAX || 
	location < 0 || location > NX_LOGICAL_LOCATION_MAX) 
      die("getprogram option requires device and location arguments");
  }
  else if (!strcasecmp(cmd,"setprogram")) {
    msgtype=NX_IPC_MSG_SET_PROG;
    if (args < 5) 
      die("missing arguments for setprogram option");
    sscanf(argv[optind+1],"%d",&device);
    if (device < 0 || device > NX_BUS_ADDRESS_MAX) 
      die("invalid device specified");
    sscanf(argv[optind+2],"%d",&location);
    if (location <0 || location > NX_LOGICAL_LOCATION_MAX) 
      die("invalid location specified");
    if (!strncasecmp("binary",argv[optind+3],strlen(argv[optind+3]))) datatype=0;
    else if (!strncasecmp("decimal",argv[optind+3],strlen(argv[optind+3]))) datatype=1;
    else if (!strncasecmp("hexadecimal",argv[optind+3],strlen(argv[optind+3]))) datatype=2;
    else if (!strncasecmp("ascii",argv[optind+3],strlen(argv[optind+3]))) datatype=3;
    if (datatype < 0)
      die("invalid data type specified");
    parse_program_data(datatype,progdata,&datalen,argc-(optind+4),argv+(optind+4));
    if (location == 910 && !force_mode)
      die("programming this location causes factory-reset of the deivce\n"
	  " (if this is what you really want use option --force)");
  }
  else if (!strcasecmp(cmd,"message")) {
    int i;

    msgtype=NX_IPC_MSG_MESSAGE;
    memset(text1,0,sizeof(text1));
    memset(text2,0,sizeof(text2));
    if (args > 4) {
      sscanf(argv[optind+1],"%d",&keypad);
      strlcpy(text1,argv[optind+2],sizeof(text1));
      strlcpy(text2,argv[optind+3],sizeof(text2));
      sscanf(argv[optind+4],"%d",&msgtime);
    } 
    else 
      die("invalid number or arguments for command 'message'");
    if (keypad < 1 || keypad > 8) 
      die("invalid keypad number specified (valid values: 1..8)");
    if (msgtime < 0 || msgtime > 255) 
      die("invalid display time specified (valid valued: 0..255)");

    for(i=0;i<MESSAGE_LINE_LEN;i++) {
      if (text1[i] == 0) text1[i]=' ';
      if (text2[i] == 0) text2[i]=' ';
    }
  }
  else if (!strcasecmp(cmd,"x10")) {
    msgtype=NX_IPC_X10_CMD;
    if (args <= 3) 
      die("x10 option requires house, unit, and function arguments");
    x10house=tolower(argv[optind+1][0]) - 'a';
    if (x10house < 0 || x10house > 15)
      die("valid range for house code is: a..p"); 
    if ( (sscanf(argv[optind+2],"%d",&x10unit) != 1) ||
	 (x10unit < 1) || (x10unit > 16) )
      die("valid range for unit code is: 1..16");
    x10unit--;
    if (!strcasecmp(argv[optind+3],"units-off"))  x10func=NX_X10_ALL_UNITS_OFF;
    else if (!strcasecmp(argv[optind+3],"lights-on")) x10func=NX_X10_ALL_LIGHTS_ON;
    else if (!strcasecmp(argv[optind+3],"lights-off")) x10func=NX_X10_ALL_LIGHTS_OFF;
    else if (!strcasecmp(argv[optind+3],"on")) x10func=NX_X10_ON;
    else if (!strcasecmp(argv[optind+3],"off")) x10func=NX_X10_OFF;
    else if (!strcasecmp(argv[optind+3],"dim")) x10func=NX_X10_DIM;
    else if (!strcasecmp(argv[optind+3],"bright")) x10func=NX_X10_BRIGHT;
    else die ("valid function codes are: on, off, dim, bright, lights-on, lights-off, units-off");
  }
  else {
    warn("unknown command: %s", cmd);
    print_usage();
    exit(1);
  }



  /* initialize message structure */
  memset(&ipcmsg,0,sizeof(ipcmsg));

  if (nxcmd) {

    /* this is keypad command */
    ipcmsg.msgtype=NX_IPC_MSG_CMD;
    ipcmsg.data[0]=(nxcmd >> 8);
    ipcmsg.data[1]=(nxcmd & 0xff);
    ipcmsg.data[2]=(partition == 0 ? 0x01 : partition);


    /* add PIN to the message, if needed */
    if (NX_KEYPAD_FUNC_NEED_PIN(nxcmd)) {
      int offset = 3;
      int len = getpassword("PIN: ",&pin,&pinsize,stdin);
      
      if (len < 0) 
	die("failed to get PIN (%d)", len);
      if (len != 4 && len != 6)
	die("invalid PIN (PIN must be 4 or 6 digits long)");
      
      /* pack PIN into format used by the alarm panel... */
      for (i=0;i<len;i+=2) {
	if (!isdigit(pin[i]) || !isdigit(pin[i+1]))
	  die("invalid PIN (PIN can only contain numbers)");
	ipcmsg.data[offset++] = ( ((pin[i+1] - '0') << 4) | (pin[i] - '0') );
      }
      
      /* clear PIN from memory */
      memset(pin,0,pinsize);
      free(pin);
    }
  } else {

    /* this is non-keypad command */
    ipcmsg.msgtype=msgtype;

    switch (msgtype) {
    case NX_IPC_MSG_BYPASS:
      ipcmsg.data[0]=zone-1;
      break;
    case NX_IPC_MSG_GET_PROG:
      ipcmsg.data[0]=device;
      ipcmsg.data[1]=(location >> 8) & 0x0f;
      ipcmsg.data[2]=location & 0xff;
      break;
    case NX_IPC_MSG_SET_PROG:
      ipcmsg.data[0]=device;
      ipcmsg.data[1]=(location >> 8) & 0x0f;
      ipcmsg.data[2]=location & 0xff;
      ipcmsg.data[3]=datatype;
      ipcmsg.data[4]=datalen;
      for (i=0; i<datalen; i++)
	ipcmsg.data[5+i]=progdata[i];
      if (datatype==3) {
	// pad ASCII data with spaces...
	for(i=datalen; i<32; i++)
	  ipcmsg.data[5+i]=' ';
      }
      break;
    case NX_IPC_MSG_MESSAGE:
      //printf("keypad=%d,text1='%s',text2='%s'\n",keypad,text1,text2);
      ipcmsg.data[0]=NX_MODULE_KEYPAD1 + (keypad-1)*8;
      ipcmsg.data[1]=msgtime;
      memcpy(&ipcmsg.data[2],text1,MESSAGE_LINE_LEN);
      memcpy(&ipcmsg.data[2+MESSAGE_LINE_LEN],text2,MESSAGE_LINE_LEN);
      break;
    case NX_IPC_X10_CMD:
      ipcmsg.data[0]=x10house & 0x0f;;
      ipcmsg.data[1]=x10unit & 0x0f;
      ipcmsg.data[2]=x10func;
      break;

    default:
      die("internal error: unknown msgtype=%d",msgtype);
    }
  }


  ipcmsg.msgid[0]=time(NULL);
  ipcmsg.msgid[1]=getpid();

#if DEBUG
  printf("msgtype: %d data: ",ipcmsg.msgtype);
  for(i=0;i<36;i++) { printf("%02x ",ipcmsg.data[i]); }
  printf("\n");
#endif

  /* send IPC message */
  i=msgsnd(msgid,&ipcmsg,sizeof(ipcmsg)-sizeof(long),0);
  /* clear message data so no PIN left in memory */
  memset(ipcmsg.data,0,sizeof(ipcmsg.data));
  if (i<0) {
    die("error sending message to server process: %s (errno=%d)\n",
	strerror(errno),errno);
  } else {
    if (verbose_mode)
      printf("Message sent successfully\n");
  }

  if (nowait)
    return 0;

  if (verbose_mode) 
    printf("Waiting for reply...\n");

  tm=time(NULL) + timeout;
  while (time(NULL) <= tm) {
    for(i=0;i<IPC_MSG_REPLY_TABLE_SIZE;i++) {
      nx_ipc_msg_reply_t *r = &shm->replies[i];

      if (r->msgid[0] == ipcmsg.msgid[0] &&
	  r->msgid[1] == ipcmsg.msgid[1]) {
	//printf("reply[%d]: %d,%d result=%d: %s\n",i,r->msgid[0],r->msgid[1],r->result,r->data);
	printf("%s\n",r->data);
	exit(r->result);
      }
    }
    usleep(100000);
  }

  die("timeout waiting response from server");
  return 0;
}

