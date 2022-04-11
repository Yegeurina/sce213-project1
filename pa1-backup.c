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

static int __process_cmd(char * command);
static int history_command(char* tokens[], int case_num);
static int built_in_command(int nr_tokens, char *tokens[]);

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
	int pipe_point=-1, is_pipe=0,i;
	int status1,status2;		
	
	pid_t pid1,pid2;
	
	char **cmd1,**cmd2;
	int fd[2];
	
	if (strcmp(tokens[0], "exit") == 0) return 0;
	
	if (nr_tokens>1)
	{
		for(int i=0;i<nr_tokens;i++)
		{
			if(strcmp(tokens[i],"|")==0)
			{
				pipe_point=i;
				is_pipe=1;
				break;
			}
		}
		
	}
	
	if(is_pipe!=0)
	{
		cmd1 = (char **)malloc(sizeof(char*)*(pipe_point+1));
	 	for(i=0;i<pipe_point;i++) 
	 	{
	 		cmd1[i]=(char *)malloc(strlen(tokens[i])+1);	
	 		strcpy(cmd1[i],tokens[i]);
	 		//fprintf(stderr,"cmd1[%d] :%s\n",i,cmd1[i]);
	 	}
	 	strcat(cmd1[pipe_point],"\0");
	 	cmd2 = (char **)malloc(sizeof(char*)*(nr_tokens-pipe_point-1));
	 	for(i=pipe_point+1;i<nr_tokens;i++)
	 	{
	 		cmd2[i-(pipe_point+1)]=(char *) malloc(strlen(tokens[i])+1);
	 	 	strcpy(cmd2[i-(pipe_point+1)],tokens[i]);
	 	 	//fprintf(stderr,"cmd2[%d] :%s\n",i-(pipe_point+1),cmd2[i-(pipe_point+1)]);
	 	}
	 	
	 	if(pipe(fd)<0)	return -1;
	 	
	}
	
	if(built_in_command(nr_tokens,tokens)==1) return 1;
	else if(is_pipe==0)
	{
		//fprintf(stderr,"this is basic command area\n");
		pid1=fork();
		if(pid1==0)
		{
			if (execvp(tokens[0],tokens)<0) exit(-1);
			exit(0);
		}
		waitpid(pid1, &status1, 0);
		if (WEXITSTATUS(status1)==0) return 1;
	}
	else
	{
		//fprintf(stderr,"this is pipe area\n");
		pid1=fork();
		if(pid1<0) exit(-1);
		else if(pid1==0)	//child
		{
			//for(i=0;i<pipe_point;i++) fprintf(stderr,"cmd1[%d] :%s\n",i,cmd1[i]);
			//fprintf(stderr,"this is pipe child process area\n");
			close(fd[0]);
			dup2(fd[1],STDOUT_FILENO);
			close(fd[1]);
			if (execvp(cmd1[0],cmd1)<0) 
			{
				fprintf(stderr, "Unable to execute %s\n", cmd1[0]);
				exit(-1);
			}
			exit(0);
		}
		else	//parent
		{
			pid2=fork();
			if(pid2<0) exit(-1);
			else if(pid2==0)	//child
			{
				//for(i=nr_tokens-pipe_point+1;i<nr_tokens;i++) fprintf(stderr,"cmd2[%d] :%s\n",i-(pipe_point+1),cmd2[i-(pipe_point+1)]);
				//fprintf(stderr,"this is pipe parent-child process area\n");
				close(fd[1]);
				dup2(fd[0],STDIN_FILENO);
				close(fd[0]);
				if (execvp(cmd2[0],cmd2)<0)
				{
					fprintf(stderr, "Unable to execute %s\n", cmd2[0]);
					exit(-1);
				}
				exit(0);
			}
			close(fd[1]);
			waitpid(pid2, &status2, 0);
			//fprintf(stderr,"status2 :%d\n",WEXITSTATUS(status2));
			
		}
		waitpid(pid1, &status1, 0);
		for(i=0;i<pipe_point;i++) free(cmd1[i]);
	 	for(i=pipe_point+1;i<nr_tokens;i++) free(cmd2[i-(pipe_point+1)]);
	 	//fprintf(stderr,"this is pipe parent process area\n");
	 	//fprintf(stderr,"status1 : %d status2 :%d\n",WEXITSTATUS(status1),WEXITSTATUS(status2));
	 	if(WEXITSTATUS(status1)==0 && WEXITSTATUS(status2)==0) return 1;
	 	else return -EINVAL;
	 	
		
	}
	
	//while(wait(NULL)!=-1);
	
	fprintf(stderr, "Unable to execute %s\n", tokens[0]);
	return -EINVAL;
}


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

/***********************************************************************
 ********************command_functions**********************************
 ***********************************************************************/
 
static int __process_cmd(char * command)
{
 	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
}

static int history_command(char* tokens[], int case_num)
{
	int num=0,i=0;
	char* cmd;
	int is_history=0;
	struct entry *temp;
	switch(case_num)
	{
		case 0:
			list_for_each_entry_reverse(temp,&history,list)
			{
				fprintf(stderr,"%2d: %s",i,temp->string);
				i++;
			}
			return 1;
		case 1:
			if(tokens[0][1]=='!')
			{
				list_for_each_entry(temp,&history,list)
				{
					if(strchr(temp->string+1,'!')==NULL)
					{
						cmd = (char *) malloc(sizeof(strlen(temp->string)+1));
						strcpy(cmd,temp->string);
						is_history=1;
						break;
					}
				}
			}
			else
			{
				num = atoi(tokens[0]+1);
				list_for_each_entry(temp,&history,list)
				{
					cmd = (char *) malloc(sizeof(strlen(temp->string)+1));
					strcpy(cmd,temp->string);
					if (i==num){ is_history=1; break;}
					i++;
				}
			}
			if (is_history==1) {__process_cmd(cmd); return 1;}
			break;
		default :
			break;
	}
	return -EINVAL;
}

static int built_in_command(int nr_tokens, char *tokens[])
{
	char* path;
	
	if (strcmp(tokens[0],"history")==0 ) return history_command(tokens, 0);
	else if(strchr(tokens[0],'!')!=NULL) return history_command(tokens,1);
	else if(strcmp(tokens[0],"cd")==0)
	{
		if (nr_tokens==1 || strcmp(tokens[1],"~")==0)	//cd,cd ~
		{
			if((path= (char *)getenv("HOME"))==NULL) path=".";
			
			if(chdir(path)==0)	return 1;
		}
		if(chdir(tokens[1])==0)	return 1;
	}
	return -1;
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
