#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void ignore_sigint()
{
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		fprintf(stderr, "(Sigaction Failed) ERROR: %s\n", strerror(errno));
		exit(1);
	}
}

void unignore_sigint()
{
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		fprintf(stderr, "(Sigaction Failed) ERROR: %s\n", strerror(errno));
		exit(1);
	}
}

int process_arglist(int count, char** arglist)
{
	//define flags (and pipe index)
	int backgroundFlag = 0, outputRedirectionFlag = 0, pipingFlag = 0, pipe_index = 0;
	// check if background command
	if (count > 1 && strcmp(arglist[count-1], "&") == 0) {
		arglist[count-1] = NULL;
		backgroundFlag = 1;
	}
	// check if output redirection commnd
	if (count > 2 && strcmp(arglist[count-2], ">>") == 0) {
		arglist[count-2] = NULL;
		outputRedirectionFlag = 1;
	}
	//check if piping command
	for (int index = 1; index < count-1; index++){
		if (arglist[index] != NULL && strcmp(arglist[index], "|") == 0) {
			arglist[index] = NULL;
			pipe_index = index;
			pipingFlag = 1;
			break;
		}
	}
	// handle background command
	if (backgroundFlag == 1) {
		int pid = fork();
		if (pid < 0) { // fork failed
			fprintf(stderr, "(Fork Failed) ERROR: %s\n", strerror(errno));
			return 0;
		}
		else if (pid == 0) { // child's process
			if (execvp(arglist[0], arglist) < 0) {
				fprintf(stderr, "(Execvp Failed) ERROR: %s\n", strerror(errno));
				exit(1);
			}
		}
		else { // parent's process
		}
		return 1;
	}
	// handle output redirection command
	if (outputRedirectionFlag == 1) {
		int pid = fork();
		if (pid < 0) { // fork failed
			fprintf(stderr, "(Fork Failed) ERROR: %s\n", strerror(errno));
			return 0;
		}
		else if (pid == 0) { // child's process
			unignore_sigint();
			int output_file = open(arglist[count-1], O_CREAT | O_WRONLY | O_APPEND, 0644);
			if (output_file < 0) {
				fprintf(stderr, "(Open Failed) ERROR: %s\n", strerror(errno));
				exit(1);
			}
			if (dup2(output_file,1) < 0) {
				fprintf(stderr, "(Dup2 Failed) ERROR: %s\n", strerror(errno));
				exit(1);
			}
			if (execvp(arglist[0], arglist) < 0) {
				fprintf(stderr, "(Execvp Failed) ERROR: %s\n", strerror(errno));
				exit(1);
			}
		}
		else { // parent's process
		    if (waitpid(pid, 0 , 0) < 0 && errno != ECHILD && errno != EINTR) {
				fprintf(stderr, "(Waitpid Failed) ERROR: %s\n", strerror(errno));
				return 0;
			}
		}
		return 1;
	}
	// handle piping command
	if (pipingFlag == 1) {
		int p[2];
		if (pipe(p) < 0) {
			fprintf(stderr, "(Pipe Failed) ERROR: %s\n", strerror(errno));
			return 0;
		}
		int pid1 = fork();
		if (pid1 < 0) { // fork failed
			fprintf(stderr, "(Fork Failed) ERROR: %s\n", strerror(errno));
			return 0;
		}
		else if (pid1 == 0) { // child's process
			unignore_sigint();
			close(p[0]);
		    if (dup2(p[1], 1) < 0) {
				fprintf(stderr, "(Dup2 Failed) ERROR: %s\n", strerror(errno));
				exit(1);
			}
			close(p[1]);
			if (execvp(arglist[0], arglist) < 0) {
				fprintf(stderr, "(Execvp Failed) ERROR: %s\n", strerror(errno));
				exit(1);
			}
			return 1;
		}
		else { // parent's process
			int pid2 = fork();
			if (pid2 < 0) { // fork failed
				fprintf(stderr, "(Fork Failed) ERROR: %s\n", strerror(errno));
				return 0;
			}
			else if (pid2 == 0) { // child's process
				unignore_sigint();
				close(p[1]);
				if (dup2(p[0], 0) < 0) {
					fprintf(stderr, "(Dup2 Failed) ERROR: %s\n", strerror(errno));
					exit(1);
				}
				close(p[0]);
				if (execvp(arglist[pipe_index+1], arglist + (pipe_index + 1)) < 0) {
					fprintf(stderr, "(Execvp Failed) ERROR: %s\n", strerror(errno));
					exit(1);
				}
				return 1;
			}
			else { // parent's process
				close(p[0]);
				close(p[1]);				
				if (waitpid(pid1, 0 , 0) < 0 && errno != ECHILD && errno != EINTR) {
					fprintf(stderr, "(Waitpid Failed) ERROR: %s\n", strerror(errno));
					return 0;
				}
				if (waitpid(pid2, 0 , 0) < 0 && errno != ECHILD && errno != EINTR) {
					fprintf(stderr, "(Waitpid Failed) ERROR: %s\n", strerror(errno));
					return 0;
				}
			}
		}
		return 1;
	}
	// handle regular command
	if (backgroundFlag == 0 && outputRedirectionFlag == 0 && pipingFlag == 0) {
		int pid = fork();
		if (pid < 0) { // fork failed
			fprintf(stderr, "(Fork Failed) ERROR: %s\n", strerror(errno));
			return 0;
		}
		else if (pid == 0) { // child's process
			unignore_sigint();
			if (execvp(arglist[0], arglist) < 0) {
				fprintf(stderr, "(Execvp Failed) ERROR: %s\n", strerror(errno));
				exit(1);
			}
		}
		else { // parent's process
			if (waitpid(pid, 0 , 0) < 0 && errno != ECHILD && errno != EINTR) {
				fprintf(stderr, "(Waitpid Failed) ERROR: %s\n", strerror(errno));
				return 0;
			}
		}
		return 1;
	}
	return 1;
}

int prepare(void)
{
	// ignore SIGINT on parent shell
	ignore_sigint();
	return 0;
}

int finalize(void)
{
	return 0;
}
