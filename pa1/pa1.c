/**********************************************************************
 * Copyright (c) 2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "types.h"
#include "list_head.h"
#include "parser.h"

static int __process_command(char * command);
LIST_HEAD(history);

struct entry {
    struct list_head list;
    char* string;
};

void push_queue(char* string)
{
    struct entry* data = (struct entry*)malloc(sizeof(struct entry));
    data->string = (char*)malloc(sizeof(char) * (strlen(string)+5));
    strcpy(data->string, string);
   
    list_add_tail(&(data->list), &history);
}

void dump_queue(int idx)
{
    struct list_head* tp;
    struct entry* current_entry;
    int num = 0;

    if(idx == -1){ 
      list_for_each(tp, &history) {
        current_entry = list_entry(tp, struct entry, list);
        fprintf(stderr, "%2d: %s", num, current_entry->string);
        num++;
      }
    }
    else{
      list_for_each(tp, &history) {
        current_entry = list_entry(tp, struct entry, list);
        if(num == idx){
          char* fd = "\n";
          strcat(current_entry->string,fd);
          __process_command(current_entry->string);
        }
        num++;
      }
    }
}

static int run_command(int nr_tokens, char *tokens[])
{
  bool flag=false;
  for(int i=0;i<nr_tokens;i++){
    if(!strcmp(tokens[i], "|"))
      flag=true;
  }

  if(flag){
    int fd[2];                 
    if(pipe(fd)==-1)
      printf("pipe error\n");                       
    
    char* parent_command[10];
    char* child_command[10];

    int i,k;
    for(i=0;strcmp(tokens[i],"|");i++)
      parent_command[i] = tokens[i];
    parent_command[i++] = 0;

    for(k=0;i<nr_tokens;i++,k++){
      child_command[k] = tokens[i];
    }
    child_command[k] = 0;

    /*
    for(int i=0;parent_command[i]!=NULL;i++)
      printf("%s ",parent_command[i]);

    for(int i=0;child_command[i]!=NULL;i++)
      printf("%s ",child_command[i]);
    */

    printf("\n");          

    if(fork()==0){
      close(STDOUT_FILENO);
      dup2(fd[1],STDOUT_FILENO);
      
      close(fd[0]);
      close(fd[1]);
      
      //char* const param[] = {"echo","hello","my","cruel","operating","system","world",0};
      execvp(parent_command[0],parent_command);
      exit(1);
    }

    if(fork()==0){
      close(STDIN_FILENO);
      dup2(fd[0],STDIN_FILENO);
      
      close(fd[0]);
      close(fd[1]);
      
      //char* const param[] = {"cut","-c16-32",0};
      execvp(child_command[0],child_command);
  
      exit(1);
    }

    close(fd[0]);
    close(fd[1]);

    wait(0);
    wait(0);
    return 0;
  }
  else{
    if(!strcmp(tokens[0],"cd")){
      if(nr_tokens==1 || !strcmp(tokens[1],"~"))
        chdir(getenv("HOME"));
      else
        chdir(tokens[1]);
    }
    else if (strcmp(tokens[0], "exit") == 0){
        return 0;
    }
    else if(!strcmp(tokens[0], "history") || !strcmp(tokens[0], "!")){
      if(!strcmp(tokens[0],"history")){
        dump_queue(-1);
      }
      else{
        dump_queue(atoi(tokens[1]));
      }
    }
	  else{
      int status;
      pid_t pid;
      char** params = (char**)malloc(sizeof(char*)*(nr_tokens+1));

      for(int i=0;i<nr_tokens;i++)
        params[i] = tokens[i];

      params[nr_tokens] = NULL;

      if((pid=fork()) > 0) // 부모
        wait(&status);
      else{ // 자식
        if(params[0][0]=='.'){
          if((execv(tokens[0]+2, params))==-1){
            fprintf(stderr, "Unable to execute %s\n", tokens[0]);
	          return -EINVAL;
          }
        }
        else{
          if((execvp(tokens[0], params))==-1){
           fprintf(stderr, "Unable to execute %s\n", tokens[0]);
	         return -EINVAL;
          }
        }
      }
    }
  }

  return 1;
}

/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */


/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */
static void append_history(char * const command)
{
  push_queue(command);
}


/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
static int initialize(int argc, char * const argv[])
{
	return 0;
}


/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
static void finalize(int argc, char * const argv[])
{

}


/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */
/*          ****** BUT YOU MAY CALL SOME IF YOU WANT TO.. ******      */
static int __process_command(char * command)
{
	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
}

static bool __verbose = true;
static const char *__color_start = "[0;31;40m";
static const char *__color_end = "[0m";

static void __print_prompt(void)
{
	char *prompt = "$";
	if (!__verbose) return;

	fprintf(stderr, "%s%s%s ", __color_start, prompt, __color_end);
}

/***********************************************************************
 * main() of this program.
 */
int main(int argc, char * const argv[])
{
  char command[MAX_COMMAND_LEN] = { '\0' };
	int ret = 0;
	int opt;

	while ((opt = getopt(argc, argv, "qm")) != -1) {
		switch (opt) {
		case 'q':
			__verbose = false;
			break;
		case 'm':
			__color_start = __color_end = "\0";
			break;
		}
	}

	if ((ret = initialize(argc, argv))) return EXIT_FAILURE;

	/**
	 * Make stdin unbuffered to prevent ghost (buffered) inputs during
	 * abnormal exit after fork()
	 */
	setvbuf(stdin, NULL, _IONBF, 0);

	while (true) {
		__print_prompt();

		if (!fgets(command, sizeof(command), stdin)) break;

		append_history(command);
		ret = __process_command(command);

		if (!ret) break;
	}

	finalize(argc, argv);

	return EXIT_SUCCESS;
}