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
#define MAX_CMDS 100
#define WRITE_END 1
#define READ_END 0

typedef struct {
    char *command; // Job command string
    pid_t pid;     // PID of job
} Job;

Job jobs[100];
int job_count = 0;


void sigHandler(){
    return;
}
// Used StackOverflow
void remove_job(Job *array, int index, int array_length){
    // Free the malloc'd command string
    free(array[index].command);
    int i;
    for(i = index; i < array_length - 1; i++) array[i] = array[i + 1];
}

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

int handleBuiltIn(char *args[], int cmd_count){
    // If exit command
    if(strcmp(args[0], "exit") == 0){
        if(args[1] != NULL){
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        // If there are more commands, error
        if((cmd_count-1) > 0){
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        // If jobs, dont exit
        else if(job_count != 0){
            fprintf(stderr, "Error: there are suspended jobs\n");
            return 1;
        }
        else{
            exit(EXIT_SUCCESS);
        }
        
    }
    if(strcmp(args[0], "cd") == 0){
        // If there are more than 1 arg for cd command, give error
        for( int i = 0; args[i] != NULL; i++){
            if(i > 1){
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
        }
        // If there is no directory given
        if(args[1] == NULL){
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        else if(chdir(args[1]) != 0){
            fprintf(stderr, "Error: invalid directory\n");
            return 1;
        }
        return 1;
    }
    if(strcmp(args[0], "jobs") == 0){
        // If there is another arguement given
        if(args[1] != NULL){
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        for(int i = 0; i < job_count; i++){
            printf("[%d] %s\n", (i+1), jobs[i].command);
        }
        return 1;
    }
    if(strcmp(args[0], "fg") == 0){
        // If given more than 1 arguent, error
        for( int i = 0; args[i] != NULL; i++){
            if(i > 1){
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
        }
        // If there is no job id given
        if(args[1] == NULL){
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        // Convert job id arg into integer
        // Used help from StackOverflow
        char *endptr;
        long num = (strtol(args[1], &endptr, 10));
        if(endptr == args[1]){
            fprintf(stderr, "Error: invalid job\n");
            return 1;
        }
        // Convert to zero-based indexing
        num--;
        // If long is not within int range, error
        if(num > INT_MAX || num < INT_MIN){
            fprintf(stderr, "Error: invalid job\n");
            return 1;
        }
        // Safely cast to int
        int selectedJobID = (int) num;

        // If selected ID is within job_count, attempt to resume job
        if(job_count > 0 && selectedJobID < job_count && selectedJobID > -1){
            if(kill(jobs[selectedJobID].pid, SIGCONT) != 0){
                fprintf(stderr, "Error: could not resume job");
                return 1;
            }
            // If the job successfully resumes
            else{
                char jobString[1024];
                int status;
                pid_t pid = waitpid(jobs[selectedJobID].pid, &status, WUNTRACED);
                strcpy(jobString, jobs[selectedJobID].command);

                remove_job(jobs, selectedJobID, job_count);
                job_count--;

                if(pid == -1){
                    fprintf(stderr, "Error: waitpid failed\n");
                    return 1;
                }
                // If job gets suspended again, add it back to the list
                if(WIFSTOPPED(status)){
                    // Allocate space for the job string
                    jobs[job_count].command = (char *)malloc(strlen(jobString) + 1);
                    if(jobs[job_count].command != NULL){
                        // Set job string and PID
                        strcpy(jobs[job_count].command, jobString);
                        jobs[job_count].pid = pid;
                        job_count++;
                    }else{
                        fprintf(stderr, "Error: Failed to allocate memory for job\n");
                    }
                    
                }
            }
            
        }else{
            fprintf(stderr, "Error: invalid job\n");
            return 1;
        }
        return 1;
    }


    return 0; // Indicate this is not a built in command
}

int handleInputOutputRedirection(char *args[], int *input_fd, int *output_fd){
    // Iterate over all args in command
    for(int i = 0; args[i] != NULL; i++){
        // if input redirection
        if(strcmp(args[i], "<") == 0){
            if(args[i+1] == NULL){
                fprintf(stderr, "Error: invalid command\n");
                return -1;
            }

            // open file as read only
            *input_fd = open(args[i+1], O_RDONLY);
            if(*input_fd < 0){
                fprintf(stderr, "Error: invalid file\n");
                return -1;
            }
            args[i] = NULL;
        }
        // if output redirection overwrite
        else if(strcmp(args[i], ">") == 0){
            if(args[i+1] == NULL){
                fprintf(stderr, "Error: invalid command\n");
                return -1;
            }

            *output_fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC);
            if(*output_fd < 0){
                fprintf(stderr, "Error: could not open file\n");
                return -1;
            }
            args[i] = NULL;
        }
        // redirection append
        else if (strcmp(args[i], ">>") == 0) {
            *output_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND);
            if(*output_fd < 0){
                fprintf(stderr, "Error: could not open file\n");
                return -1;
            }
            args[i] = NULL;
        }
        else if (strcmp(args[i], "<<") == 0){
            fprintf(stderr, "Error: invalid command\n");
            return -1;
        }
    }
    return 0;
}

// Reads args array into a string
void argsToString(char *args[], char *result){
    result[0] = '\0'; // Empty result array
    for(int i = 0; args[i] != NULL; i++){
        strcat(result, args[i]);
        // If not the last command add a space
        if(args[i+1] != NULL){
            strcat(result, " ");
        }
    }
}


int main() {
    char *args[MAX_ARGS];
    // Pipe up to 10 commands
    char *cmds[10];
    pid_t child_pids[MAX_CMDS]; // Array to store PIDs
    int child_count = 0;

    char *readBuffer;
    size_t bufferSize = 32;
    ssize_t characters;

    signal(SIGINT, sigHandler);
    signal(SIGTSTP, sigHandler);
    signal(SIGQUIT, sigHandler);



    // Malloc a buffer for characters to be read into
    // NOTE: getline can also allocate memory automatically so this is not entirely necessary
    readBuffer = (char *)malloc(bufferSize * sizeof(char));
    if(readBuffer == NULL){
        fprintf(stderr,"Error: Unable to allocate buffer");
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

        // Check if command is just spaces
        // This is just so no error is thrown when we check for empty spaces in the pipe tokenizing
        // Not super necessary
        int fullOfSpaces = 1;
        for (int i = 0; readBuffer[i] != '\0'; i++) {
            if (!isspace(readBuffer[i])) {
                fullOfSpaces = 0;
                break;
            }
        }
        if(fullOfSpaces){ // if command is just spaces, create a new prompt
            continue;
        }

        // If command begins or ends with a pipe give an error
        if(readBuffer[0] == '|' || ( strlen(readBuffer) > 0 && readBuffer[strlen(readBuffer) - 1] == '|')){
            fprintf(stderr, "Error: invalid command\n");
            continue;
        }

        // Split commands by pipe into cmds array
        int cmd_count = 0;
        char *pipe_tok = strtok(readBuffer, "|");
        while(pipe_tok != NULL){
            if(strcmp(pipe_tok, " ") == 0 || strcmp(pipe_tok, "") == 0){
                cmd_count = 0;
                fprintf(stderr, "Error: invalid command\n");
                break;
            }
            cmds[cmd_count++] = pipe_tok;
            // Move to next command
            pipe_tok = strtok(NULL, "|");
        }

        // Have pipes and an input fd
        // input_fd is all inputs from redirects or pipes
        int pipe_fd[2], input_fd = 0;

        // Iterate over each separate command
        for( int i = 0; i < cmd_count; i++ ){
            // Split args
            parseCommand(cmds[i], args, MAX_ARGS);
            // if no command, exit
            if(args[0] == NULL){
                break;
            }
            // if handle built in is true, exit for loop
            if(handleBuiltIn(args, cmd_count)){
                break;
            }

            // If there is more than one command, create pipes
            if( i < cmd_count - 1){
                if(pipe(pipe_fd) == -1){
                    fprintf(stderr, "Error: pipe failed\n");
                    return -1;
                }
            }

            // FORK
            pid_t pid = fork();

            if(pid < 0){
                fprintf(stderr, "Error: Fork failed");
                return -1;
            }if(pid == 0){
                // If first or last command, handle potential redirection
                // Maybe if last command (i==cmd_count-1), dont pass an inputfd? In case i/o redirect overwrites it
                if(i == 0 || i == (cmd_count-1)){
                    if(handleInputOutputRedirection(args, &input_fd, &pipe_fd[WRITE_END]) == -1){
                        exit(1);
                    }
                }
                // READING PREV COMMAND OR FILE REDIRECTION
                // If input_fd is set from either a previous command or redirection ^, set it to STDIN
                if(input_fd != 0){
                    dup2(input_fd, STDIN_FILENO);
                    close(input_fd);
                }
                
                // If write end is set, pipe the output of command to write pipe
                // The output will come out of the READ_END of the pipe which is switched onto input_fd in the parent
                if(pipe_fd[WRITE_END] != 0){
                    dup2(pipe_fd[WRITE_END], STDOUT_FILENO);
                    close(pipe_fd[WRITE_END]);
                }

                // Execute command
                execvp(args[0], args);
                // If execvp falils exit
                fprintf(stderr, "Error: invalid program\n");
                exit(1);
            } else{
                // Parent wait for child PID to exit
                child_pids[child_count++] = pid;

                // If there is no input file descriptor, close it
                // I am not sure why there would be an input in the parent
                // We are going to set it to the READ_END if its not the last command anyway
                if (input_fd != 0) {
                    close(input_fd);
                }
                // If not the last command:
                if (i < cmd_count - 1) {
                    // IMPORTANT: The next input will be the read end of the current pipe i.e. what just was output
                    // Set the input_fd to the read end of the last pipe
                    input_fd = pipe_fd[READ_END];
                    close(pipe_fd[WRITE_END]); // Close unused write end in parent
                }
            }
        }
        // WAITPID HERE
        for(int i = 0; i < child_count; i++){
             int status;
            pid_t waited = waitpid(child_pids[i], &status, WUNTRACED);
            if (waited == -1) {
                fprintf(stderr, "Error: waitpid failed");
                return -1;
            }

            if(WIFSTOPPED(status)){
                char jobString[1024];
                argsToString(args, jobString);
                
                // Allocate space for the job string
                jobs[job_count].command = (char *)malloc(strlen(jobString) + 1);
                if(jobs[job_count].command != NULL){
                    // Set job string and PID
                    strcpy(jobs[job_count].command, jobString);
                    jobs[job_count].pid = child_pids[i];
                    job_count++;
                }else{
                    fprintf(stderr, "Error: Failed to allocate memory for job\n");
                }
            }
        }
        child_count=0;

    }
    // Free any other command strings
    // In case any remain
    for(int i = 0; i < job_count; i++){
        free(jobs[i].command);
    }

    free(readBuffer);
    return 0;
}
