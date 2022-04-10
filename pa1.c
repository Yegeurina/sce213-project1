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
static int run_pipe(int nr_tokens, char *tokens[],int pt);

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
	char* path;
	int is_pipe = 0;
	pid_t pid;
	
	/*fprintf(stderr,"nr_tokens : %d\n",nr_tokens);
	for (int i=0;i<nr_tokens;i++)
		fprintf(stderr,"tokens[%d] : %s\n",i, tokens[i]);*/
	
	if (strcmp(tokens[0], "exit") == 0) return 0;
	
	if(nr_tokens>1)	// pipe
	{ 
		for(int i=0;i<nr_tokens;i++)
		{
			if(strcmp(tokens[i],"|")==0)	
			{
				is_pipe=1;
				return run_pipe(nr_tokens,tokens,i);
			}
		}
	}
	
	if(is_pipe == 0)
	{
		//if (strcmp(tokens[0], "exit") == 0) return 0;
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
		else if((pid = fork())==0)
		{
			if (execvp(tokens[0],tokens)==-1)
				fprintf(stderr, "Unable to execute %s\n", tokens[0]);
			exit(0);
			
		}
	}

	while(wait(NULL)!=-1);
	//fprintf(stderr, "Unable to execute %s\n", tokens[0]);
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
 * ******************command_functions**********************************
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
				list_for_each_entry_reverse(temp,&history,list)
				{
					cmd = (char *) malloc(sizeof(strlen(temp->string)+1));
					strcpy(cmd,temp->string);
					if (i==num){ is_history=1; break;}
					i++;
				}
			}
			if (is_history==1) __process_cmd(cmd);
			break;
		default :
			break;
	}
	return -EINVAL;
}

static int run_pipe(int nr_tokens, char *tokens[],int pt)
{
	int fd[2];
	int i;
	char **cmd1,**cmd2;
	pid_t p1,p2;
	
	cmd1 = (char **)malloc(sizeof(char*)*pt);
 	for(i=0;i<pt;i++) cmd1[i]=tokens[i];
 	strcat(cmd1[pt-1],"\0");
 	cmd2 = (char **)malloc(sizeof(char*)*(nr_tokens-pt-1));
 	for(i=pt+1;i<nr_tokens;i++) cmd2[i-(pt+1)]=tokens[i];
 	
 	if(pipe(fd)<0)	return -1;
 	
 	if((p1=fork())<0) return -1;
 	else if(p1==0)
 	{
 		close(fd[0]);
 		dup2(fd[1],STDOUT_FILENO);
 		//close(fd[1]);
 		//close(fd[0]);
 		run_command(pt,cmd1);
 		free(cmd1);
 		wait(NULL);
 		exit(0);
 	}
 	
 	if((p2=fork())<0) return -1;
 	else if(p2==0)
	{
		close(fd[1]);
	 	dup2(fd[0],STDIN_FILENO);
	 	//close(fd[0]);
	 	//close(fd[1]);
	 	run_command(nr_tokens-pt-1,cmd2);
	 	free(cmd2);
	 	wait(NULL);
	 	exit(0);
	}
 	
 	
 	close(fd[0]); 	close(fd[1]);
 	
 	wait(NULL); wait(NULL);
 	//while(wait(NULL)!=-1);
 	
	return 1;
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
