#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

int is_background(int count, char** arglist)
{
	printf("lala");
	if (count > 1 && strcmp(arglist[count-1], "&") == 0) {
		arglist[count-1] = NULL;
		return 1;
	}
	return 0;
}

int is_output_redirection(int count, char** arglist)
{
	if (count > 2 && strcmp(arglist[count-2], ">>") == 0) {
		arglist[count-2] = NULL;
		return 1;
	}
	return 0;
}

int is_piping(int count, char** arglist, int* pipe_index)
{
	for (int index = 1; index < count-1; index++){
		if (strcmp(arglist[index], "|") == 0) {
			arglist[index] = NULL;
			pipe_index = &index;
			return 1;
		}
	}
	return 0;
}

int regular_command(char** arglist)
{
	int pid;
	pid = fork();
	if (pid < 0) { // fork failed
		fprintf(stderr, "ERROR: %s\n", strerror(errno));
		return 1;
	}
	else if (pid == 0) { // child's process
		if (execvp(arglist[0], arglist) < 0) {
			fprintf(stderr, "ERROR: %s\n", strerror(errno));
			exit(1);
		}
	}
	else { // parent's process
		if (waitpid(pid, 0 , 0) < 0 ) {
			//TODO
		}
	}
	return 1;
}

int background_command(char** arglist)
{
	int pid;
	pid = fork();
	if (pid < 0) { // fork failed
		fprintf(stderr, "ERROR: %s\n", strerror(errno));
		return 1;
	}
	else if (pid == 0) { // child's process
		if (execvp(arglist[0], arglist) < 0) {
			fprintf(stderr, "ERROR: %s\n", strerror(errno));
			exit(1);
		}
	}
	else { // parent's process
	}
	return 1;
}

int output_redirection_command(int count, char** arglist) {
	int pid, output_file;
	pid = fork();
	if (pid < 0) { // fork failed
		fprintf(stderr, "ERROR: %s\n", strerror(errno));
		return 1;
	}
	else if (pid == 0) { // child's process
		printf("1");
		output_file = open(arglist[count-1], O_CREAT | O_WRONLY | O_APPEND, 0644);
		printf("2");
		if (output_file < 0) {
			fprintf(stderr, "ERROR: %s\n", strerror(errno));
			exit(1);
		}
		if (dup2(output_file,1) < 0) {
			fprintf(stderr, "ERROR: %s\n", strerror(errno));
			exit(1);
		}
		if (execvp(arglist[0], arglist) < 0) {
			fprintf(stderr, "ERROR: %s\n", strerror(errno));
			exit(1);
		}
	}
	else { // parent's process
	    if (waitpid(pid, 0 , 0) < 0 ) {
			//TODO
		}
	}
	return 1;
}

int piping_command(char** arglist, int pipe_index)
{
	int p[2], pid1, pid2;
	if (pipe(p) < 0) {
		fprintf(stderr, "ERROR: %s\n", strerror(errno));
		return 1;
	}
	pid1 = fork();
	if (pid1 < 0) { // fork failed
		fprintf(stderr, "ERROR: %s\n", strerror(errno));
		return 1;
	}
	else if (pid1 == 0) { // child's process
	    if (dup2(p[1], 1) < 0) {
			fprintf(stderr, "ERROR: %s\n", strerror(errno));
			exit(1);
		}
		if (execvp(arglist[0], arglist) < 0) {
			fprintf(stderr, "ERROR: %s\n", strerror(errno));
			exit(1);
		}
	}
	else { // parent's process
		pid2 = fork();
		if (pid2 < 0) { // fork failed
			fprintf(stderr, "ERROR: %s\n", strerror(errno));
			return 1;
		}
		else if (pid2 == 0) { // child's process
			if (dup2(p[0], 0) < 0) {
				fprintf(stderr, "ERROR: %s\n", strerror(errno));
				exit(1);
			}
			if (execvp(arglist[pipe_index+1], arglist) < 0) {
				fprintf(stderr, "ERROR: %s\n", strerror(errno));
				exit(1);
			}
		}
		else { // parent's process
			if (waitpid(pid1, 0 , 0) < 0 || waitpid(pid2, 0 , 0) < 0) {
			//TODO
			}
		}
	}
	return 1;
}

int process_arglist(int count, char** arglist)
{
	printf("start process_arglist\n");
	int backgroundFlag, outputRedirectionFlag, pipingFlag, pipe_index = 0;
	
	// check is the command is regular, background, output redirection or piping
	backgroundFlag = is_background(count, arglist);
	printf("1");
	outputRedirectionFlag = is_output_redirection(count, arglist);
	printf("2");
	pipingFlag = is_piping(count, arglist, &pipe_index);
	printf("3");

	printf("backgroundFlag: %d\n", backgroundFlag);
	printf("outputRedirectionFlag: %d\n", outputRedirectionFlag);
	printf("pipingFlag: %d\n", pipingFlag);
	
	if (backgroundFlag == 1) {
		background_command(arglist);
	}
	if (outputRedirectionFlag == 1) {
		output_redirection_command(count, arglist);
	}
	if (pipingFlag == 1) {
		piping_command(arglist, pipe_index);
	}
	if (backgroundFlag == 0 && outputRedirectionFlag == 0 && pipingFlag == 0) {
		regular_command(arglist);
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
