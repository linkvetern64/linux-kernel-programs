
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
void childProcess(void); //Prototype of child process
void parentProcess(void); //Prototype of parent process

int main(int argc, char *argv[]){
  if(argc != 5){
    return 0;
  }
  //int input;
  //int selection = 0;
  int pid = fork();
  for(int i = 0; i < 4; i++){
  
    printf("CP - %d & PID = %d",i, pid);
    if(pid == 0){
      childProcess();
      execlp("./player", "23", NULL);
    }
    else{
      parentProcess();
    }
  }
  /*
  //Menu
  while(true){
  printf("Main Menu:\n");
  printf("1. Throw ball directory to a player\n");
  printf("2. Field ball\n");
  printf("3. Show player statistics\n");
  printf("4. End game\n");
  selection = scanf("%d", input);
  }
  //End menu
  
  FILE *file = fopen("/dev/urandom", "r");
  int rand = fscanf(file, "%[^n]");
  printf("Num - %d\n", rand);
  for(int i = 1; i < argc; i++){
    printf("Name %d - %s\n",i, argv[i]);
  }
  fclose(file);
  */
  return 0;
}

void childProcess(void){
  printf("Child Process\n");
}

void parentProcess(void){
  printf("Parent Process\n");
}
