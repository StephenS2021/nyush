#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ARGS 100

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

int execute_command(char *args[]){
    // FORK
    pid_t pid = fork();

    if(pid < 0){
        printf("Error: Fork failed");
        return -1;
    }if(pid == 0){
        // Child process

        // Execute command
        execvp(args[0], args);
        // If execvp falils exit
        fprintf(stderr, "Error: invalid command\n");
        return -1;
    } else{
        // Parent wait for child PID to exit
        int status;

        pid_t waited = waitpid(pid, &status, WUNTRACED);
        if (waited == -1) {
            fprintf(stderr, "Error: waitpid failed");
            return -1;
        }

    }
    return 1;
}

int main() {
    char *args[MAX_ARGS];


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

        // Split args
        parseCommand(readBuffer, args, MAX_ARGS);

        if(args[0] == NULL){
            continue;
        }

        if(handleBuiltIn(args)){
            continue;
        }

        
        // FORK
        pid_t pid = fork();

        if(pid < 0){
            printf("Error: Fork failed");
            return -1;
        }if(pid == 0){
            // Child process



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

    free(readBuffer);
    return 0;
}
