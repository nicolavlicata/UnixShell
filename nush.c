#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "tokenize.h"
#include "svec.h"

void
execute(char* cmd)
{
    int cpid;
    int pipe_cpid;
    svec* tokens = tokenize(cmd);
    int cmd_pipe = -1;
    int cmd_input = -1;
    int cmd_output = -1;
    int cmd_and = -1;
    int cmd_or = -1;
    int cmd_split = -1;
    int cmd_bg = -1;
    // int og_out = -1;
    char** tokensArray = malloc((tokens->size + 1) * sizeof(char*));
    //char** tokensArrayRout;
    for (int ii = 0; ii < tokens->size; ++ii) {
	tokensArray[ii] = svec_get(tokens, ii);
	if (strcmp(svec_get(tokens, ii), "|") == 0) {
	    cmd_pipe = ii;
	}
	if (strcmp(svec_get(tokens, ii), "<") == 0) {
	    cmd_input = ii;
	}
	if (strcmp(svec_get(tokens, ii), ">") == 0) {
	    cmd_output = ii;
	}
	if (strcmp(svec_get(tokens, ii), "&&") == 0) {
	    cmd_and = ii;
	}
	if (strcmp(svec_get(tokens, ii), "||") == 0) {
	    cmd_or = ii;
	}
	if (strcmp(svec_get(tokens, ii), ";") == 0) {
	    cmd_split = ii;
	}
	if (strcmp(svec_get(tokens, ii), "&") == 0) {
	    cmd_bg = ii;
	}
    }
    tokensArray[tokens->size] = 0;
    
    

    if (tokens->size == 0) {
	free_svec(tokens);
	free(tokensArray);
	return;
    }

    if (cmd_split != -1) {
	char left[256];
	strcpy(left, "");
	char right[256];
	strcpy(right, "");
	for (int ii = 0; ii < tokens->size; ++ii) {
	    if (ii < cmd_split) {
		strcat(left, svec_get(tokens, ii));
		strcat(left, " ");
	    }
	    if (ii > cmd_split) {
		strcat(right, svec_get(tokens, ii));
	 	strcat(right, " ");
	    }
	}
	//printf("%s ", left);
	//printf("%s ", right);
	execute(left);
	execute(right);
	free_svec(tokens);
	free(tokensArray);
	return;
    }
    
    if (cmd_bg != -1) {
	if ((cpid = fork())) {
	    free_svec(tokens);
	    free(tokensArray);
	    return;
	}
	else {
	    char command[256];
	    strcpy(command, "");
	    for (int ii = 0; ii < tokens->size; ++ii) {
		if (ii != cmd_bg) {
		    strcat(command, svec_get(tokens, ii));
		    strcat(command, " ");
		}
	    }
	    execute(command);
	}
    }

    if (strcmp(svec_get(tokens, 0), "exit") == 0) {
	free_svec(tokens);
	free(tokensArray);
	exit(0);
    }

    if (strcmp(svec_get(tokens, 0), "cd") == 0) {
	if (tokens->size < 2 || chdir(svec_get(tokens, 1)) < 0) {
	    perror("fail");
	}
	free_svec(tokens);
	free(tokensArray);
	return;
    }

    if (cmd_or != -1) {
	if ((cpid = fork())) {
	    int status;
	    waitpid(cpid, &status, 0);
	    free_svec(tokens);
	    free(tokensArray);
	    return;
	}
	
	else {
	    char rightSide[256];
	    strcpy(rightSide, "");
	    for (int ii = 0; ii < tokens->size; ++ii) {
		if (ii > cmd_or) {
		    strcat(rightSide, svec_get(tokens, ii));
		    strcat(rightSide, " ");
		}	
	    }
	    tokensArray[cmd_or] = "\0";
	    
	    if (strcmp(svec_get(tokens, 0), "true") == 0) {
		free_svec(tokens);
		free(tokensArray);
		exit(0);
	    }
	    else if (strcmp(svec_get(tokens, 0), "false") == 0) {
		execute(rightSide);
		free_svec(tokens);
		free(tokensArray);
		exit(0);
	    }
	    else if (execvp(svec_get(tokens, 0), tokensArray) == -1) {
		execute(rightSide);
		free(tokensArray);
		free_svec(tokens);
		exit(0);
	    }
	    else {
		free(tokensArray);
		free_svec(tokens);
		exit(0);
	    }
	}
    }
    
    if (cmd_and != -1) {
	char rightSide[256];
	strcpy(rightSide, "");
	for (int ii = 0; ii < tokens->size; ++ii) {
	    if (ii > cmd_and) {
		strcat(rightSide, svec_get(tokens, ii));
		strcat(rightSide, " ");
	    }
	}
	tokensArray[cmd_and] = "\0";

	if ((cpid = fork())) {
	    int status;
	    waitpid(cpid, &status, 0);
	    if (status == 0) {
		execute(rightSide);
	    }
	    free_svec(tokens);
	    free(tokensArray);
	    return;
	}

	else {
	    execvp(svec_get(tokens, 0), tokensArray);
	}
    }

    if (cmd_output != -1) {
	//char** tokensArrayRout = malloc((cmd_output + 1) * sizeof(char*));

	char* outputName = svec_get(tokens, cmd_output + 1);
	if (fopen(outputName, "r") < 0) {
		return;
	}
	FILE* output = fopen(outputName, "r");
	//for (int ii = 0; ii < cmd_output; ++ii) {
	//    tokensArrayRout[ii] = tokensArray[ii];
	//}
	//tokensArrayRout[cmd_output] = 0;
	//tokensArray = tokensArrayRout;
	dup2(fileno(output), 1);
	fclose(output);
    }

    if (cmd_input != -1) {
	if ((cpid = fork())) {
	    int status;
	    waitpid(cpid, &status, 0);
	    free_svec(tokens);
	    free(tokensArray);
	    return;
	}
	else {
	    char clean_cmd[256];
	    strcpy(clean_cmd, "");

	    close(0);
	    char* inputName = svec_get(tokens, cmd_input + 1);
	    if (fopen(inputName, "r") < 0) {
		return;
	    }
	    FILE* input = fopen(inputName, "r");
	    dup(fileno(input));
	    fclose(input);
	    for(int ii = 0; ii < tokens->size; ++ii) {
		if (ii == cmd_input || ii == cmd_input + 1) {
		    continue;
		}
		if (ii < cmd_input) {
		    strcat(clean_cmd, svec_get(tokens, ii));
		    strcat(clean_cmd, " ");
		}
	    }
	    
	    execute(clean_cmd);
	    exit(0);

	}
    }

    if (cmd_pipe != -1) {
	if ((cpid = fork())) {
	    int status;
	    waitpid(cpid, &status, 0);
	    free_svec(tokens);
	    free(tokensArray);
	    return;
	}
	else {
	    char lcmd[256];
	    char rcmd[256];
	    strcpy(lcmd, "");
	    strcpy(rcmd, "");
	
	    for (int ii = 0; ii < tokens->size; ++ii) {
		if (ii == cmd_pipe) {
		    continue; 
		}
		if (ii < cmd_pipe) {
		    strcat(lcmd, svec_get(tokens, ii));
		    strcat(lcmd, " ");
		}
		
		if (ii > cmd_pipe) {
		    strcat(rcmd, svec_get(tokens, ii));
		    strcat(rcmd, " ");
		}
	    }
	    
	    int pipes[2];
	    pipe(pipes);

	    if ((pipe_cpid = fork())) {
		close(pipes[1]);
		close(0);
		dup(pipes[0]);		

		int status_2;
		waitpid(pipe_cpid, &status_2, 0);
		
		execute(rcmd);
		exit(0);
		
	    }
	    else {
		close(pipes[0]);
		close(1);
		dup(pipes[1]);
		
		execute(lcmd);
		exit(0);
	    }
	}
    }

    if ((cpid = fork())) {

        int status;
        waitpid(cpid, &status, 0);
	free_svec(tokens);
	free(tokensArray);
	return;
    }
    else {
    
        execvp(svec_get(tokens, 0), tokensArray);
	free_svec(tokens);
	free(tokensArray);
    }
}

int
main(int argc, char* argv[])
{
    char cmd[256];
    int return_value;

    if (argc == 1) {
	while (1) {
            printf("nush$ ");
            fflush(stdout);
            char* return_value = fgets(cmd, 256, stdin);
	    if (return_value == 0) {
	        break;
	    }
	    execute(cmd);
	}
    }
    else {
        char* scriptName = argv[1];
	if (fopen(scriptName, "r") < 0) {
	    return 0;
	}
        FILE* script = fopen(scriptName,"r");
	while (1) {
	    char* return_value = fgets(cmd, sizeof(cmd), script);
	    if (return_value == 0) {
		fclose(script);
		return 0;
	    }
	    execute(cmd);
	}
    }

    return 0;
}
