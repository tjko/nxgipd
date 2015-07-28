/* nxcmd.c
 * 
 * Copyright (C) 2015 Timo Kokkonen. All Rights Reserved.
 *
 * Loosely based on nxcmd created by David Whaley
 *
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <termio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
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



void print_usage()
{
  fprintf(stderr,"Usage: %s [OPTIONS] <command>\n\n",program_name);
  fprintf(stderr,
	  " Commands (no PIN required):\n"
	  "  exit                    Exit (one button arm / toggle instant)\n"
	  "  stay                    Stay (one button arm / toggle interiors)\n"
	  "  chime                   Toggle chime mode\n"
	  "  bypass                  Bypass interiors\n"
	  "  grpbypass               Group bypass\n"
	  "  smokereset              Smoke detector reset\n"
	  "  sounder                 Start keypad sounder\n\n"
	  " Commands (PIN required):\n"
	  "  armaway                 Arm in Away mode\n"
	  "  armstay                 Arm in Stay mode\n"
 	  "  disarm                  Disarm partition\n"
	  "  silence                 Turn off any sounder or alarm\n"
	  "  cancel                  Cancel alarm\n"
	  "  autoarm                 Initiate auto-arm\n\n"
	  " Options:\n"
	  "  --config=<configfile>   use specified config file\n"
	  "  -c <configfile>\n"
	  "  --partition=<n>, -p <n> partition for the command (default 1)\n"
	  "  --help, -h              display this help and exit\n"
	  "  --verbose, -v           enable verbose output to stdout\n"
	  "  --version, -V           print program version\n"
	  "\n");
}


int main(int argc, char **argv)
{
  int opt_index = 0;
  int opt;
  uchar partition = 0;
  char *config_file = CONFIG_FILE;
  int i,nxcmd;
  nx_ipc_msg_t  ipcmsg;
  const char* cmd = NULL;
  char *pin = NULL;
  size_t pinsize = 0;

  struct option long_options[] = {
    {"config",1,0,'c'},
    {"help",0,0,'h'},
    {"partition",1,0,'p'},
    {"verbose",0,0,'v'},
    {"version",0,0,'V'},
    {NULL,0,0,0}
  };
  program_name="nxcmd";



  while ((opt=getopt_long(argc,argv,"vVhc:p:",long_options,&opt_index)) != -1) {
    switch (opt) {
      
    case 'c':
      config_file=strdup(optarg);
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

    case 'v':
      verbose_mode=1;
      break;

    case 'V':
      fprintf(stderr,"%s v%s  %s\nCopyright (C) 2009-2015 Timo Kokkonen. All Rights Reserved.\n",
	      program_name,VERSION,HOST_TYPE);
      exit(0);
      
    case 'h':
    default:
      print_usage();
      exit(1);
    }
  }


  /* check for non-option arguments */
  if (optind < argc) {
    if (verbose_mode) printf("non-option ARGV-elements: ");
    while (optind < argc) {
      if (!cmd) cmd=argv[optind];
      if (verbose_mode) printf("%s ", argv[optind]);
      optind++;
    }
    if (verbose_mode) printf("\n");
  }

  if (!cmd) {
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
  else {
    warn("unknown command: %s", cmd);
    print_usage();
    exit(1);
  }




  /* initialize message structure */
  memset(&ipcmsg,0,sizeof(ipcmsg));
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
    //printf("PIN='%s'\n",pin);
    for (i=0;i<len;i+=2) {
      if (!isdigit(pin[i]) || !isdigit(pin[i+1]))
	die("invalid PIN (PIN can only contain numbers)");
      ipcmsg.data[offset++] = ( ((pin[i] - '0') << 4) | (pin[i+1] - '0') );
    }

    /* clear PIN from memory */
    memset(pin,0,pinsize);
    free(pin);
  }

  printf("msgtype: %d data: ",ipcmsg.msgtype);
  for(i=0;i<8;i++) { printf("%02x ",ipcmsg.data[i]); }
  printf("\n");


  /* send IPC message */
  i=msgsnd(msgid,&ipcmsg,sizeof(ipcmsg),0);
  /* clear message so no PIN left in memory */
  memset(&ipcmsg,0,sizeof(ipcmsg));
  if (i<0) {
    die("error sending message to server process: %s (errno=%d)\n",strerror(errno),errno);
  } else {
    printf("message sent successfully\n");
  }

  return 0;
}

