#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

// MAIN COMMENT:
// Assume we do not have a command that contains both | , &.  In case we have a command with &,  we execute the command by creating a son with fork.  In addition, in the parent,  we create a thread that will wait for the son  (to prevent the son from being a zombie). The thread performs start_Routine , in which it waits with waitpid to the son process which executes the background command.
// Pipes:  we create 2  sons with fork.  In the parent process, we also create 2 threads:  One thread that waits for the first son,  and the other thread waits for the second son.  This is to prevent the sons from being zombies:   for example if one of the sons finishes way ahead of the other son,  we do not want that son to be a zombie during that time. The main thread of the parent process calls pthread_join on both waiting threads.




void *start_Routine (void *son_pid){
  
  //pid_t waitpid(pid_t pid, int *stat_loc, int options);
  int routineStatus;
  pid_t wait_ret =  waitpid( (pid_t)son_pid, &routineStatus, 0);  
 
  if ( wait_ret < 0) {
      printf("ERROR in waitpid : %s\n", strerror(errno));
      exit(-1);   
  }
  pthread_exit( (void*)wait_ret );
}

// RETURNS - 1 if should cotinue, 0 otherwise
int process_arglist(int count, char** arglist)
{
  
  int status;   int pipe_index;   int k,j;    int is_ampersand = 0;    int is_pipe = 0;
  
  //check if the last argument is &:
  if( strcmp(arglist[count-1], "&") == 0 ){
    is_ampersand = 1;
    is_pipe = 0;    //assume we don't have | and & in the same command
    
    arglist = (char**) realloc(arglist, sizeof(char*) * (count + 0));
    if (arglist == NULL) {
	printf("ERROR: malloc failed: %s\n", strerror(errno));
	exit(-1);   
    }
    arglist[count-1] = NULL;	
  } //closes case where we have &
  
  //check if we have | :
  if(is_ampersand == 0){  
    for(k=0; k < count; k++){
	if( strcmp(arglist[k], "|") == 0 ){
	  is_pipe = 1;
	  pipe_index = k;
	  break;
	}
    }
  }
  
  if(is_pipe == 1){
    int pipe_fd[2];
    int pipe_val = pipe(pipe_fd);
    
    if(pipe_val == -1 ) {
      printf("ERROR:  pipe failed: %s\n", strerror(errno));
      exit(-1);
    }
    pid_t leftFork = fork();
    if(leftFork < 0){
      printf("ERROR:  fork failed: %s\n", strerror(errno));
      exit(-1);
    }
    if (leftFork == 0) {  // inside left son process - execute with arglist
      int dup_val = dup2(pipe_fd[1], STDOUT_FILENO);
      if(dup_val == -1 ) {
	fprintf(stderr, "ERROR: dup2 failed: %s\n", strerror(errno));
	exit(-1);
      }
      
     close(pipe_fd[0]);
     close(pipe_fd[1]);
      
      
      arglist[pipe_index] = NULL;
      execvp(arglist[0], arglist);
      fprintf(stderr, "execvp failed: %s\n", strerror(errno));
      exit(-1);
    }//closes leftFork == 0
    
    pid_t right_fork = fork();
    if(right_fork < 0){
      printf("ERROR: fork failed: %s\n", strerror(errno));
      exit(-1);
    }
    if (right_fork == 0) {
      int dup_right_val = dup2(pipe_fd[0], STDIN_FILENO);
      if(dup_right_val == -1 ) {
	printf("ERROR: dup2 failed: %s\n", strerror(errno));
	exit(-1);
      }
      
      close(pipe_fd[1]);
      close(pipe_fd[0]);
      
      execvp(arglist[pipe_index + 1], arglist+ pipe_index + 1);
      printf("execvp failed: %s\n", strerror(errno));
      exit(-1);
    }
    
    // in the father process:
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    
    
    pthread_t left_thread;
    pthread_t right_thread;
    int lt, rt;
    void *status_for_pipe;
    
    
    lt = pthread_create(&left_thread, NULL, start_Routine, (void *)leftFork);
    if (lt != 0){
	printf("ERROR; return code from pthread_create() is %d\n", lt);
	exit(-1);
    }
    
    rt = pthread_create(&right_thread, NULL, start_Routine, (void *)right_fork);
    if (rt != 0){
	printf("ERROR; return code from pthread_create() is %d\n", rt);
	exit(-1);
    }
    
    int left_join_val = pthread_join(left_thread, &status_for_pipe);
    if (left_join_val != 0){
	printf("ERROR: return code from pthread_join() is %d\n", left_join_val);
	exit(-1);
    }
    
    int right_join_val = pthread_join(right_thread, &status_for_pipe);
    if (right_join_val != 0) {
	printf("ERROR; return code from pthread_join() is %d\n", right_join_val);
	exit(-1);
    }
    
    
    return 1;
  
  } //closes case we have |
  
  
  pid_t f = fork();
  
  if(f < 0){
    printf("fork failed: %s\n", strerror(errno));
    //kill(father_id, SIGTERM);
    exit(-1);
  }
		
  if (f == 0) {  // inside son process - execute with arglist
    execvp(arglist[0], arglist);
    printf("execvp failed: %s\n", strerror(errno));
    exit(-1);
  } else {  //inside parent process - wait for son to finish, then continue
    
      if(is_ampersand == 1){
	pthread_t th;
	int rc;
	
	rc = pthread_create(&th, NULL, start_Routine, (void *)f);
	if (rc != 0){
	  printf("ERROR; return code from pthread_create() is %d\n", rc);
	  exit(-1);
	}
	
	return 1; //in this manner we return to main and perform more commands like
	//we're supposed to
      }else { //no & in command
	  pid_t wait_val = waitpid(f, &status, 0);
	  if ( wait_val < 0) {
	    printf("ERROR in waitpid : %s\n", strerror(errno));
	    exit(-1);   
	  }
	
	  return 1;
	}
	
  } //closes parent process case
}


