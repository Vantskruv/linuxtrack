#define __USE_GNU
#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#ifndef LIBLINUXTRACK_SRC
  #include "ipc_utils.h"
  #include "utils.h"
#endif

static pid_t child;

bool ltr_int_fork_child(char *args[])
{
  if((child = fork()) == 0){
    //Child here
    execv(args[0], args);
    perror("execv");
    return false;
  }
  return true;
}

#ifndef LIBLINUXTRACK_SRC
bool ltr_int_wait_child_exit(int limit)
{
  pid_t res;
  int status;
  int cntr = 0;
  do{
//    printf("Waiting!\n");
    usleep(100000);
    res = waitpid(child, &status, WNOHANG);
    ++cntr;
  }while((res != child) && (cntr < limit));
  if((res == child) && (WIFEXITED(status))){
    return true;
  }else{
    return false;
  }
}

semaphore_p ltr_int_server_running_already(char *lockName)
{
  char *lockFile;
  semaphore_p pfSem;
  lockFile = ltr_int_get_default_file_name(lockName);
  if(lockFile == NULL){
    ltr_int_log_message("Can't determine pref file path!\n");
    return NULL;
  }
  pfSem = ltr_int_createSemaphore(lockFile);
  if(pfSem == NULL){
    ltr_int_log_message("Can't create semaphore!");
    return NULL;
  }
  if(ltr_int_tryLockSemaphore(pfSem) == false){
    ltr_int_closeSemaphore(pfSem);
    pfSem= NULL;
    ltr_int_log_message("Can't lock - server runs already!\n");
    return NULL;
  }
  return pfSem;
}


#endif

struct semaphore_t{
  int fd;
}semaphore_t;

static semaphore_p ltr_int_semaphoreFromFd(int fd)
{
  semaphore_p res = (semaphore_p)ltr_int_my_malloc(sizeof(semaphore_t));
  res->fd = fd;
  return res;
}

#ifndef LIBLINUXTRACK_SRC
semaphore_p ltr_int_createSemaphore(char *fname)
{
  if(fname == NULL){
    return NULL;
  }
  int fd = open(fname, O_RDWR | O_CREAT, 0600);
  if(fd == -1){
    perror("open: ");
    return NULL;
  }
  return ltr_int_semaphoreFromFd(fd);
}
#endif

bool ltr_int_lockSemaphore(semaphore_p semaphore)
{
  if(semaphore == NULL){
    return false;
  }
  int res = lockf(semaphore->fd, F_LOCK,0);
  if(res == 0){
    return true;
  }else{
    return false;
  }
}


#ifndef LIBLINUXTRACK_SRC
bool ltr_int_tryLockSemaphore(semaphore_p semaphore)
{
  if(semaphore == NULL){
    return false;
  }
  int res = lockf(semaphore->fd, F_TLOCK,0);
  if(res == 0){
    return true;
  }else{
    return false;
  }
}
#endif

bool ltr_int_unlockSemaphore(semaphore_p semaphore)
{
  if(semaphore == NULL){
    return false;
  }
  int res = lockf(semaphore->fd, F_ULOCK,0);
  if(res == 0){
    return true;
  }else{
    return false;
  }
}

#ifndef LIBLINUXTRACK_SRC
void ltr_int_closeSemaphore(semaphore_p semaphore)
{
  close(semaphore->fd);
  free(semaphore);
}
#endif

bool ltr_int_mmap_file(const char *fname, size_t tmp_size, struct mmap_s *m)
{
  m->fname = ltr_int_my_strdup(fname);
  umask(S_IWGRP | S_IWOTH);
  int fd = open(fname, O_RDWR | O_CREAT | O_NOFOLLOW, 0700);
  if(fd < 0){
    perror("open: ");
    return false;
  }
  m->size = tmp_size;
  
  //Check if file does have needed length...
  struct stat file_stat;
  bool truncate = true;
  if(fstat(fd, &file_stat) == 0){
    if(file_stat.st_size == (ssize_t)m->size){
      truncate = false;
    }
  }
  
  if(truncate){
    int res = ftruncate(fd, m->size);
    if (res == -1) {
      perror("ftruncate: ");
      close(fd);
      return false;
    }
  }
  m->data = mmap(NULL, m->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(m->data == (void*)-1){
    perror("mmap: ");
    close(fd);
    return false;
  }
//  close(fd);
  m->sem = ltr_int_semaphoreFromFd(fd);
  m->sem->fd = fd;
  return true;
}

#ifndef LIBLINUXTRACK_SRC
bool ltr_int_unmap_file(struct mmap_s *m)
{
  if(m->data == NULL){
    return true;
  }
  ltr_int_closeSemaphore(m->sem);
  if(m->fname != NULL){
    free(m->fname);
    m->fname = NULL;
  }
  int res = munmap(m->data, m->size);
  m->data = NULL;
  m->size = 0;
  if(res < 0){
    perror("munmap: ");
  }
  return res == 0;
}

//the fname argument should end with XXXXXX;
//  it is also modified by the call to contain the actual filename.
//
//Returns opened file descriptor
int ltr_int_open_tmp_file(char *fname)
{
  umask(S_IWGRP | S_IWOTH);
  return mkstemp(fname);
}

//Closes and removes the file...
void ltr_int_close_tmp_file(char *fname, int fd)
{
  unlink(fname);
  close(fd);
}
#endif

char *ltr_int_get_com_file_name()
{
  return ltr_int_get_default_file_name("linuxtrack.comm");
}
