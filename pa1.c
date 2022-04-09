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
#include <sys/wait.h>

#include <string.h>

#include "types.h"
#include "list_head.h"
#include "parser.h"



/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */
LIST_HEAD(history);

struct entry{
	struct list_head list;
	char *string;
};

 char* cmd1;
 char* cmd2;

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
 
void run_pipe(char * command);
static int __process_cmd(char * command);

static int run_command(int nr_tokens, char *tokens[])
{
	struct entry *temp;
	int i=0,num;
	char *cmd, *pre_cmd;
	char *path;
	//pid_t pid;
	printf("%d\n",nr_tokens);
	for(i=0;i<nr_tokens;i++) printf("%s\n",tokens[i]);
	if (strcmp(tokens[0], "exit") == 0) 	return 0;
	
	/*else if(nr_tokens>1 && strchr(tokens[1],'|')!=NULL)
	{ 
		cmd = (char *)malloc(sizeof((strlen(tokens[0])+1)+(strlen(tokens[1])+1)));
		strcpy(cmd,tokens[0]);
		strcat(cmd,tokens[1]);
		printf("%s\n",cmd);
		run_pipe(cmd);
	} */
	else if(strcmp(tokens[0], "cd") == 0)
	{
		if (nr_tokens==1 || strcmp(tokens[1],"~")==0)	//cd,cd ~
		{
			if((path= (char *)getenv("HOME"))==NULL) path=".";
			
			if(chdir(path)==0)	return 1;
		}
		if(chdir(tokens[1])==0)	return 1;
	}
	else if (strcmp(tokens[0], "history") == 0)
	{
		list_for_each_entry_reverse(temp,&history,list)
		{
			fprintf(stderr,"%2d: %s\n",i,temp->string);
			i++;
		}
		
		return 1;
	}
	else if(tokens[0][0]=='!')
	{
		if (tokens[0][1]=='!')	num = -1;
		else num = atoi(tokens[0]+1);
		list_for_each_entry_reverse(temp,&history,list)
		{
			if(i>=2) free(pre_cmd);
			if (i>=1)
			{
				pre_cmd = (char *) malloc(sizeof(strlen(cmd)+1));
				strcpy(pre_cmd,cmd);
			 	free(cmd);
			}
			cmd = (char *) malloc(sizeof(strlen(temp->string)+1));
			strcpy(cmd,temp->string);
			if (i==num) 	break;
			i++;
		}
		if (num==-1) 
		{
			free(cmd);
			cmd = (char *) malloc(sizeof(strlen(pre_cmd)+1));
			strcpy(cmd,pre_cmd);
			free(pre_cmd);
		}
		if(num==-1 || i==num)
		{
			__process_cmd(cmd);
		}
		free(cmd);
	}
	// 포크하면 자식프로세스한테는 0이 떨어지고
	// 부모프로세스한테는 자식프로세스의 PID가 떨어짐
	
	/*else if((pid=fork())==0)
	{
		if (execvp(tokens[0],tokens)==-1)
		{
			fprintf(stderr, "Unable to execute %s\n", tokens[0]);
		}
		exit(0);
		
	}*/
	while(wait(NULL)!=-1);
	//fprintf(stderr, "Unable to execute %s\n", tokens[0]);
	return -EINVAL;
}

 static int __process_cmd(char * command)
 {
 	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
 }
 


 void run_pipe(char* command)
 {
 	int fd[2];	// handling pipe
 	
 	cmd1 = strtok(command,"|");
 	cmd2 = strtok(NULL,"|");
 	printf("%s\n%s",cmd1,cmd2);
 	if(pipe(fd)<0)	exit(0);
 	
 	if(fork()==0)
 	{
 		close(1);
 		dup2(fd[0],STDIN_FILENO);
 		close(fd[0]);
 		close(fd[1]);
 		__process_cmd(cmd1);
 		wait(NULL);
 		exit(0);
 	}
 	
 	if(fork()==0)
 	{
 		close(0);	
 		dup2(fd[0],STDIN_FILENO);
 		close(fd[0]);
 		close(fd[1]);
 		__process_cmd(cmd2);
 		wait(NULL);
 		exit(0);
 	}
 	
 	
 	
 	while(wait(NULL)!=-1);
 	
 }



/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */
static void append_history(char * const command)
{
	 struct entry *item = malloc(sizeof(struct entry));
	 INIT_LIST_HEAD(&item->list);
	 
	 item->string = (char *)malloc(strlen(command)+1);
	 strcpy(item->string,command);
	 
	 list_add(&item->list,&history);

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
