#define _POSIX_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

int main(int argc, char *argv[]){
  //Exits program if too many or too little arguments
  if(argc != 5){
    printf("Error too many or too little arguments.  Need exactly 4\n");
    return 0;
  }

  char * players[4];
  char * positions[4];
  char a;
  int fp;
  int fdT;
  positions[0] = "1B";
  positions[1] = "2B";
  positions[2] = "SS";
  positions[3] = "3B";
  int input, playerTo;
  int pid;
  int selection = 9;
  int PID[4];
  int fd[8];

  pipe(fd);
  pipe(fd + 2);
  pipe(fd + 4);
  pipe(fd + 6);
  
  //Creates forked processes
  for(int i = 0; i <= 3; i++){
    switch(pid = fork()){
    //Child process
    case 0:
      //Close Read
      if(dup2(fd[i * 2], STDIN_FILENO) == -1){
	printf("dup2 error\n");
      }
      
      //printf("Testing %d, %d\n", (i * 2), (1 + i * 2)); 
      close(fd[i * 2]);
      close(fd[1 + i * 2]);
      
      wait();
      execlp("./player", "player", argv[i + 1],positions[i], NULL);
      return 0;
   
    case -1:
      //Error - do nothing
      break;

    //Parent process  
    default:
      close(fd[1 + i * 2]);
      PID[i] = pid;
      break;
    }
  }

  if(getpid() > 0){
    while(selection > -1){
	printf("Main Menu:\n");
	printf("1. Throw ball directory to a player\n");
	printf("2. Field ball\n");
	printf("3. Show player statistics\n");
	printf("4. End game\n");
	selection = scanf("%d", &input);

	switch(input){
	case 1:
	  printf("Throwing ball to who?\n");
	  for(int i = 1; i < 5; i++){
	    printf("%d %s\n", i, argv[i]);
	  }
	  selection = scanf("%d", &playerTo);
	  switch(playerTo){
	  case 1:
	    kill(PID[0], SIGUSR1);
	    break;

	  case 2:
	    kill(PID[1], SIGUSR1);
	    break;

	  case 3:
	    kill(PID[2], SIGUSR1);
	    break;

	  case 4:
	    kill(PID[3], SIGUSR1);
	    break;
     
	  default:
	    printf("Illegal entry, returning to main menu\n");
	    break;
	  }
	  break;
	  
	case 2:
	  printf("Fielding ball...\n");
	  break;
	  
	case 3:
	  printf("Showing player stats...\n");
	  break;
	  
	case 4:
	  printf("Ending game...\n");
	  for(int i = 0; i < 4; i++){
	    kill(PID[i], SIGQUIT);
	  }
	  waitpid();
	  return 0;
	  break;
	  
	case 5:
	  //Test FD is open / closed
	  /*if(fcntl(fd, F_GETFD) == -1){
	    printf("Failure\n");
	  }
	  else{
	    printf("Success\n");
	  }*/
	  if(close(fd[2])){
	    perror("Close");
	  }
	  else{
	    printf("All good\n");
	  }
	  break;

	case 6:
	  fp = open("/dev/urandom", 00);
	  if(fp < 0){
	    fprintf(stderr, "Error opening\n");
	  }
	  else{
	    printf("Opened FD %d\n",fp);
	  }

	  char buf[1];
	  
	  size_t amount_to_read = sizeof(buf); 
	  ssize_t amount_read = read(fp, buf, amount_to_read); 
	  if(amount_read < 0) { 
	    fprintf(stderr,"Error reading\n"); 
	  } 
	  else
	    { 
	      printf("Read %zd bytes\n", amount_read);
	      printf("%d\n", ((int)buf)%4);
	    }
	  close(fp);
	  break;
	default:
	  printf("Value of input is %d\n", input);
	  break;
	}	
      }
  }
 
  
  return 0;
}
