/* ipc.c - IPC shared memory and message queue hanling
 *
 * Copyright (C) 2009-2015 Timo Kokkonen.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>

#include "nxgipd.h"




int init_shared_memory(int shmkey, int shmmode, size_t size, int *shmidptr, nx_shm_t **shmptr)
{
  int id;
  void *seg;
  

  /* initialize IPC shared memory segment */

  id = shmget(shmkey,size,(shmmode & 0x1ff)|IPC_CREAT|IPC_EXCL);
#if 0
  if (id < 0 && errno==EEXIST) {
    logmsg(0,"Shared memory segment (key=%x) already exists",shmkey);
    id = shmget(shmkey,1,0);
    if (id >= 0) {
      fprintf(stderr,"Trying to remove existing shared memory segment...\n");
      shmctl(id,IPC_RMID,NULL);
      id = shmget(shmkey,size,(shmmode & 0x1ff)|IPC_CREAT|IPC_EXCL);
    }
  }
#endif

  if (id < 0) {
    if (errno==EEXIST) {
      logmsg(0,"Shared memory segment (key=%x) already exists",shmkey);
      logmsg(0,"Another instance still running?");
    }
    else if (errno==EINVAL) 
      logmsg(0,"Shared memory segment (key=%x) already exists with wrong size (?)\n",shmkey);
    else
      logmsg(0,"shmget() failed: %s (%d)\n",strerror(errno),errno);
    return -1; 
  }
  *shmidptr=id;

  seg = shmat(id,NULL,0);
  if (seg == (void*)-1) { 
    fprintf(stderr,"shmat() failed: %s (%d)\n",strerror(errno),errno);
    return -1;
  }

  *shmptr=seg;
  strlcpy((*shmptr)->shmversion,SHMVERSION,sizeof((*shmptr)->shmversion));
  (*shmptr)->pid=getpid();
  (*shmptr)->last_updated=0;


  /* initialize IPC message queue */
     

  return 0;
}


int init_message_queue(int msgkey, int msgmode)
{
  int id = msgget(msgkey,((msgmode&0777)|IPC_CREAT|IPC_EXCL));
  
  if (id < 0) {
    if (errno==EEXIST) {
      logmsg(0,"Shared memory segment (key=%x) already exists",msgkey);
      logmsg(0,"Another instance already running?");
    } else {
      logmsg(0,"msgget() failed: %d (%s)\n", errno, strerror(errno));
    }
  }

  return id;
}



void release_shared_memory(int shmid, void *shmseg)
{
  if (shmseg != NULL) {
    if (shmdt(shmseg) < 0)
      logmsg(0,"shmdt() failed: %s errno=%d\n",strerror(errno),errno);
  }

  if (shmctl(shmid,IPC_RMID,NULL) < 0)
    logmsg(0,"failed to delete shmid (%x): %s (errno=%d)\n",shmid,strerror(errno),errno);
}



void release_message_queue(int msgid)
{
  if (msgid < 0) return;
  
  if (msgctl(msgid,IPC_RMID,NULL) < 0) {
    logmsg(0,"failed to delete message queue (%x): %s (errno=%d)\n",msgid,strerror(errno),errno);
  }
  msgid=-1;
}



int read_message_queue(int msgid, nx_ipc_msg_t *msg)
{
  ssize_t r;

  if (msgid < 0 || !msg) return -1;

  /* read next message from the queue */
  r = msgrcv(msgid,msg,sizeof(nx_ipc_msg_t),0,IPC_NOWAIT);

  if (r < 0) {
    if (errno==ENOMSG) {
      return 0;
    }
    else if (errno==EINTR) {
      logmsg(1,"msgrcv() interrupted");
      return 0;
    }

    logmsg(0,"failed to read message queue: %s (errno=%d)",strerror(errno),errno);
    return -2;
  }

  if (r != sizeof(nx_ipc_msg_t)) {
    logmsg(0,"received invalid size message: %ld (expected size %ld)",r,sizeof(nx_ipc_msg_t));
    return -3;
  }
 
  return r;
}


/* eof :-) */
