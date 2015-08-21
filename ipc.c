/* ipc.c - IPC shared memory and message queue hanling
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
  int id, ret;
  nx_shm_t *shm;
  struct shmid_ds shminfo;

  /* initialize IPC shared memory segment */

  id = shmget(shmkey,size,(shmmode & 0x1ff)|IPC_CREAT|IPC_EXCL);
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


  /* get segment status */
  ret = shmctl(id, IPC_STAT, &shminfo);
  if (ret < 0) {
    logmsg(0,"Failed to get shared memory segment status (id=%d): %s (%d)",
	   id,strerror(errno),errno);
    return -2;
  }

  /* update segment permissions */
  if (config->shm_uid >= 0 || config->shm_gid >= 0) {
    if (config->shm_uid >= 0)
      shminfo.shm_perm.uid=config->shm_uid;
    if (config->shm_gid >= 0)
      shminfo.shm_perm.gid=config->shm_gid;;
    ret = shmctl(id, IPC_SET, &shminfo);
    if (ret < 0) {
      logmsg(0,"Failed to update shared memory segment permissions: %s (%d)",
	     strerror(errno),errno);
      return -3;
    }
  }

  shm = shmat(id,NULL,0);
  if (shm == (void*)-1) { 
    fprintf(stderr,"shmat() failed: %s (%d)\n",strerror(errno),errno);
    return -4;
  }


  /* initialize shared memory segment */

  memset(shm,0,size);
  strlcpy(shm->shmversion,SHMVERSION,sizeof(shm->shmversion));
  *shmptr=shm;

  return 0;
}



int init_message_queue(int msgkey, int msgmode)
{
  int id, ret;
  struct msqid_ds msginfo;

  /* initialize IPC message queue */
  id = msgget(msgkey,((msgmode&0777)|IPC_CREAT|IPC_EXCL));
  
  if (id < 0) {
    if (errno==EEXIST) {
      logmsg(0,"Message queue (key=%x) already exists",msgkey);
      logmsg(0,"Another instance already running?");
    } else {
      logmsg(0,"msgget() failed: %d (%s)\n", errno, strerror(errno));
    }
    return -1;
  }


  /* get message queue status */
  ret = msgctl(id, IPC_STAT, &msginfo);
  if (ret < 0) {
    logmsg(0,"Failed to get message queue status: %d (%d)",
	   strerror(errno),errno); 
    return -2;
  }


  /* set message queue permissions */
  if (config->msg_uid >= 0 || config->msg_gid >= 0) {
    if (config->msg_uid >=0)
      msginfo.msg_perm.uid = config->msg_uid;
    if (config->msg_gid >=0)
      msginfo.msg_perm.gid = config->msg_gid;
    ret = msgctl(id, IPC_SET, &msginfo);
    if (ret < 0) {
      logmsg(0,"Failed to update message queue permissions: %s (%d)",
	     strerror(errno),errno);
      return -3;
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
