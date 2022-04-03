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

/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */
static int run_command(int nr_tokens, char *tokens[])
{

  //printf("cmd: %s\n",tokens[0]);
  if(!strcmp(tokens[0],"/bin/echo") || !strcmp(tokens[0],"echo")){
    for(int i=1;i<nr_tokens;i++)
      fprintf(stderr, "%s ", tokens[i]);
    fprintf(stderr, "\n");
  }
  else if(!strcmp(tokens[0],"pwd") || !strcmp(tokens[0],"/bin/pwd")){
    char current_wd[255];
    getcwd(current_wd, 255);

    fprintf(stderr, "%s\n", current_wd);
  }
  else if(!strcmp(tokens[0],"/bin/ls") || !strcmp(tokens[0],"ls")){
    struct dirent* entry = NULL;
    char current_wd[255];
    
    getcwd(current_wd, 255);
    DIR* dir = opendir(current_wd);

    /*
    if(dir == NULL){
      printf("current dircetory error\n");
    }
    */

    while((entry = readdir(dir))!=NULL){
      if(strcmp(entry->d_name,"..") && strcmp(entry->d_name,".")) 
        fprintf(stderr, "%s ", entry->d_name);
    }

    fprintf(stderr,"\n");

    closedir(dir);
  }
  else if(!strcmp(tokens[0],"cp")){
    struct stat sb;
    char buf;
    int src = open(tokens[1], O_RDONLY );  
    int dst = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, 0644 );

    while(read(src, &buf, 1))
      write(dst, &buf, 1);
    
    fstat(src,&sb);
    chmod(tokens[2],sb.st_mode);

    close(src);
    close(dst);
  }
  else if(!strcmp(tokens[0],"cd")){
    if(nr_tokens==1 || !strcmp(tokens[1],"~"))
      chdir(getenv("HOME"));
    else
      chdir(tokens[1]);
    
    //char current_wd[255];

    //printf("dir: %s\n",getcwd(current_wd, 255));
  }
  else if (strcmp(tokens[0], "exit") == 0){
    return 0;
  }
  else if(tokens[0][0] == '.' && tokens[0][1] == '/'){ 
    pid_t pid;
    int status;
    char** params = (char**)malloc(sizeof(char*)*(nr_tokens));
    
    for(int i=0;i<nr_tokens-1;i++)
     params[i] = tokens[i+1];
    params[nr_tokens-1] = NULL;

    if((pid=fork()) == 0){ // 부모
      wait(&status);
    }
    else{ // 자식
      execv(tokens[0]+2, params)
    }
  }
	else{
    fprintf(stderr, "Unable to execute %s\n", tokens[0]);
	  return -EINVAL;
  }

  return 1;
}


/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */
LIST_HEAD(history);


/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */
static void append_history(char * const command)
{

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