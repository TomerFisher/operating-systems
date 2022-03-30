#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int isBackground(int count, char** arglist)
{
	if (count > 1 && strcmp(arglist[count-1], "&") == 0) {
		arglist[count-1] = NULL;
		return 1;
	}
	return 0;
}

int isOutputRedirection(int count, char** arglist)
{
	if (count > 2 && strcmp(arglist[count-2], ">>") == 0) {
		arglist[count-2] = NULL;
		return 1;
	}
	return 0;
}

int isPiping(int count, char** arglist, int* pipe_index)
{
	for (int index = 1; index < count-1; index++){
		if (strcmp(arglist[index], "|") == 0) {
			arglist[index] = NULL;
			pipe_index = index;
			return 1;
		}
	}
	return 0;
}

int process_arglist(int count, char** arglist)
{
	int backgroundFlag, outputRedirectionFlag, pipingFlag, pipe_index;
	
	// check is the command is regular, background, output redirection or piping
	backgroundFlag = isBackground(count, arglist);
	outputRedirectionFlag = isOutputRedirection(count, arglist);
	pipingFlag = isPiping(count, arglist, &pipe_index);
	
	
	int pid, status_code, pipe_index, new_pid;
	int background = 0, output_redirection = 0, piping = 0;
	
	// check if there is '&' in arglist
	if (count > 1 && strcmp(arglist[count-1], "&") == 0) {
		background = 1;
		arglist[count-1] = NULL;
	}

	// check if there is '>>' in arglist
	else if (count > 2 && strcmp(arglist[count-2], ">>") == 0) {
		output_redirection = 1;
		arglist[count-2] = NULL;
	}

	// check is there is '|' in arglist
	else if (count > 2) {
		for (int index = 1; index < count-1; index++){
			if (strcmp(arglist[index], "|") == 0) {
				piping = 1;
				arglist[index] = NULL;
				pipe_index = index;
				break;
			}
		}
	}
	pid = fork();
	if (pid == 0){
		if (output_redirection == 1) {
			printf("#this command have output redirection\n");
			int file_desc = open(arglist[count-1], O_CREAT | O_WRONLY | O_APPEND, 0644);
			dup2(file_desc, 1);
			status_code = execvp(arglist[0], arglist);
		}
		else if (piping == 1){
			printf("#this command have pipe\n");
			int p[2];
			pipe(p);
			new_pid = fork();
			if (new_pid > 0) {
				dup2(p[1], 1);
				status_code = execvp(arglist[0], arglist);
			}
			else {
				//waitpid(new_pid);
				dup2(p[0], 0);
				status_code = execvp(arglist[pipe_index+1], &arglist[pipe_index+1]);
			}
		}
		else {
			status_code = execvp(arglist[0], arglist);
		}
	}
	else {
		if (background == 0) {
			printf("#this regular command\n");
			if (piping == 1){
				waitpid(new_pid);
			}
			waitpid(pid);
		}
		else
			printf("#this backgroup command\n");
	}
	return 1;
}

int prepare(void)
{
	return 0;
}

int finalize(void)
{
	return 0;
}
