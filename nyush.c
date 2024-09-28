#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_ARGS 100

// Takes readBuffer of raw commands and parses it into array called args
void parseCommand(char *input, char *args[], int max_args){
    int arg_count = 0;

    // Split the command into tokens
    char* token = strtok(input, " ");
    while(token != NULL && arg_count < max_args - 1){
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }

    // Null terminator for args array
    args[arg_count] = NULL;
}

void displayPrompt(){
    char cwd[PATH_MAX];
    if(getcwd(cwd, sizeof(cwd)) != NULL){
        // print prompt and flush stdout
        printf("[nyush %s]$ ", (strrchr(cwd, '/')+1));
        fflush(stdout);
    }else{
        fprintf(stderr, "Error: retrieving working directory\n");
    }
}

int handleBuiltIn(char *args[]){
    if(strcmp(args[0], "cd") == 0){
        // If there is no directory given
        if(args[1] == NULL){
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }

        if(chdir(args[1]) != 0){
            fprintf(stderr, "Error: invalid directory\n");
        }
        return 1;
    }

    return 0; // Indicate this is not a built in command
}

int handleInputOutputRedirection(char *args[], int *input_fd, int *output_fd){
    for(int i = 0; args[i] != NULL; i++){
        // if input redirection
        if(strcmp(args[i], "<") == 0){
            // open file as read only
            *input_fd = open(args[i+1], O_RDONLY, 0777);
            if(*input_fd < 0){
                fprintf(stderr, "Error opening file");
                return -1;
            }
            args[i] = NULL;

        }
        // if output redirection overwrite
        else if(strcmp(args[i], ">") == 0){
            *output_fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC);
            if(*output_fd < 0){
                fprintf(stderr, "Error opening file");
                return -1;
            }
            args[i] = NULL;

        }
        // redirection append
        else if (strcmp(args[i], ">>") == 0) {
            *output_fd = open(args[i+1], O_WRONLY | O_CREAT | O_APPEND);
            if(*output_fd < 0){
                fprintf(stderr, "Error opening file");
                return -1;
            }
            args[i] = NULL;
        }
    }
    return 0;
}


int main() {
    char *args[MAX_ARGS];
    // Pipe up to 10 commands
    char *cmds[10];
    
    char *readBuffer;
    size_t bufferSize = 32;
    ssize_t characters;

    // Malloc a buffer for characters to be read into
    // NOTE: getline can also allocate memory automatically so this is not entirely necessary
    readBuffer = (char *)malloc(bufferSize * sizeof(char));
    if(readBuffer == NULL){
        printf("Error: Unable to allocate buffer");
        return -1;
    }

    while(1){
        displayPrompt();

        // read command
        characters = getline(&readBuffer, &bufferSize, stdin);
        if(characters == -1){
            break;
        }
        // removing newline char if it is in readBuffer
        if (readBuffer[characters - 1] == '\n') {
            readBuffer[characters - 1] = '\0';
        }

        if(strcmp(readBuffer, "exit") == 0){
            break;
        }

        int cmd_count = 0;
        char *pipe_tok = strtok(readBuffer, "|");
        while(pipe_tok != NULL){
            cmds[cmd_count++] = pipe_tok;
            pipe_tok = strtok(NULL, "|");
        }
        int input_fd = 0;
        int output_fd = 0;

        // Iterate over each separate command
        for( int i = 0; i < cmd_count; i++ ){
            char *cmd = cmds[i];
            parseCommand(cmd, args, MAX_ARGS);
            
            if(args[0] == NULL){
                continue;
            }

            if(handleBuiltIn(args)){
                continue;
            }


            pid_t pid = fork();
            if(pid < 0){
                printf("Error: Fork failed");
                return -1;
            }if(pid == 0){
                // Child process
                // redirect input file to stdin
                handleInputOutputRedirection(args, &input_fd, &output_fd);
                if(input_fd != 0){
                    dup2(input_fd, STDIN_FILENO);
                    close(input_fd);
                }
                // redirect output file to stdout
                if(output_fd != 1){
                    dup2(output_fd, STDOUT_FILENO);
                    close(output_fd);
                }

                // Execute command
                execvp(args[0], args);
                // If execvp falils exit
                fprintf(stderr, "Error: invalid command\n");
                break;
            } else{
                // Parent wait for child PID to exit
                int status;

                pid_t waited = waitpid(pid, &status, WUNTRACED);
                if (waited == -1) {
                    printf("Error: waitpid failed");
                    return -1;
                }

            }

        }

    }

    free(readBuffer);
    return 0;
}
