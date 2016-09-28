#define _POSIX_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int numHandles = 0;

void my_handler(int);

int main(int argc, char *argv[]){
  usleep(100); //So players load in after menu
  char * name = argv[1];
  char * position = argv[2];
  int fd[2];
  close(fd[0]);
  close(fd[1]);
  dup2(fd, STDIN_FILENO);
  sigset_t mask;
  sigemptyset(&mask);
  struct sigaction sa = {
    .sa_handler = my_handler,
    .sa_mask = mask,
    .sa_flags = 0
  };
  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGUSR2, &sa, NULL);
  printf("#%ld:%s (%s)\n",getpid(), name, position);
  
  while(1){
    sleep(1);
  }
  return 0;
}

void my_handler(int sig){
  if(sig == SIGUSR1){   
    numHandles++;
    printf("%d - Caught signal SIGUSR1 %d times.\n", getpid(), numHandles);
  }
  else if(sig == SIGUSR2){
    printf("Caught SIGUSR2\n");
  }
}
