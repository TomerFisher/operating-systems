#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char** arglist)
{
	int pid, status_code, pipe_index;
	int background = 0, output_redirection = 0, piping = 0;
	if (count > 1 && strcmp(arglist[count-1], "&") == 0) {
		background = 1;
		arglist[count-1] = NULL;
	}
	else if (count > 2 && strcmp(arglist[count-2], ">>") == 0) {
		output_redirection = 1;
		arglist[count-2] = NULL;
	}
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
			int p[2], new_pid;
			pipe(p);
			new_pid = fork();
			if (new_pid > 0) {
				dup2(p[1], 1);
				status_code = execvp(arglist[0], arglist);
			}
			else {
				waitpid(new_pid);
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
			waitpid(pid);

		}
		else
			printf("#this backgroup command\n");
	}
	return 1;
}

// prepare and finalize calls for initialization and destruction of anything required
int prepare(void)
{
	return 0;
}

int finalize(void)
{
	return 0;
}
