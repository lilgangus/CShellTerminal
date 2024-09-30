// SmallSH 
// By: Charles Tang

// compile with:
// gcc -std=gnu99 main.c -o test

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_COMMAND_LENGTH 2048

// global variable to keep track of the foreground only mode
volatile int foreground_only = 0;

// command syntax is "command [arg1 arg2 ...] [< input_file] [> output_file] [&]"
struct command {
    char* command;
    char** argumentsArray;
    int numArguments;
    int argumentCapacity;

    // name of the input and output files for redirection
    char* inputFile;
    char* outputFile;

    // flag to determine if the command should be run in the background (0 for foreground, 1 for background)
    int isBackground;
};

// gets a command from the user and stores it in the given buffer
void getCommand(char* commandBuffer, int bufferSize);

// replaces all instances of "$$" with the pid in the arguments array
void expansionVariablePIDReplace(int numArguments, char** argumentsArray);

// parses the command string into the command struct
struct command* parseCommand(char* commandBuffer);

// executes the command from the command struct and returns the exit status of the command
int executeCommand(struct command* commandStruct);

// executes the command in the background
void executeBackgroundCommand(struct command* commandStruct);

// frees the memory allocated for the command struct
void freeCommandStruct(struct command* commandStruct);

// signal handler for SIGCHLD to print the exit status of the background processes
void sigchildHandler(int sig);

// changes the directory to the home directory or the directory specified in the first argument
void changeDirectory(struct command* commandStruct);

// helper function that adds an argument to the arguments array of the command struct
void addArgument(struct command* commandStruct, char* argument);

// signal handler for SIGINT which does nothing
void sigintHandler(int sig) {
    // do nothing
}

// signal handler for SIGTSTP which toggles the foreground only mode
void sigtstpHandler(int sig) {
    // toggle the foreground only mode
    printf("\n");
    if (foreground_only == 0) {
        printf("Entering foreground-only mode (& is now ignored)\n");
        fflush(stdout);
        foreground_only = 1;
    } else {
        printf("Exiting foreground-only mode\n");
        fflush(stdout);
        foreground_only = 0;
    }
    printf(": ");
    fflush(stdout);
}

int main() {
    printf("$ smallish \n");
    fflush(stdout);

    int run_program = 1;
    
    // exit status of the last foreground process
    int exit_status = 0;


    // create a signal handler for SIGINT
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); 
    sa.sa_handler = sigintHandler;
    sa.sa_flags = SA_RESTART; 
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        printf("sigaction fail");
        fflush(stdout);
        return 1;
    }

    // create a signal handler for SIGTSTP
    struct sigaction sa_tstp;
    memset(&sa_tstp, 0, sizeof(sa_tstp)); // clear the structure
    sa_tstp.sa_handler = sigtstpHandler;
    sa_tstp.sa_flags = SA_RESTART; 
    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1) {
        printf("sigaction fail");
        fflush(stdout);
        return 1;
    }

    while(run_program) {

        // get a string command from the user and store it in a buffer
        char commandBuffer[MAX_COMMAND_LENGTH];
        getCommand(commandBuffer, MAX_COMMAND_LENGTH);

        // parse the command string into a command struct
        struct command* commandStruct = parseCommand(commandBuffer);
        
        if(foreground_only == 1) {
            commandStruct->isBackground = 0;
        }

        // if the command is not null, execute the command
        if(commandStruct != NULL) {
            
            // switch and execute the command based on the command string of the user (with exit, cd, and status being built-in commands)
            if(strcmp(commandStruct->command, "exit") == 0) {
                run_program = 0;

            } else if (strcmp(commandStruct->command, "cd") == 0) {
                // go to the home directory or the directory specified in the first argument (use getcwnd not getenv )
                changeDirectory(commandStruct);

            } else if (strcmp(commandStruct->command, "status") == 0) {
                
                if(exit_status == 0 || exit_status == 1) {
                    printf("exit value %d\n", exit_status);
                    fflush(stdout);
                } else {
                    printf("terminated by signal %d\n", exit_status);
                    fflush(stdout);
                }

            } else {
                exit_status = executeCommand(commandStruct);    
            }

            freeCommandStruct(commandStruct);
            
        }

    }
}

void changeDirectory(struct command* commandStruct) {
    char* path = NULL;
    char cwd [MAX_COMMAND_LENGTH];

    if (commandStruct->numArguments == 1) {
        // get the path to the home directory
        path = getenv("HOME");
    } else {
        // go to the directory specified in the first argument
        path = commandStruct->argumentsArray[1];
    }

    // change the directory
    if (chdir(path) == -1) {
        printf("chdir error");
        fflush(stdout);
    }
}

int executeCommand(struct command* commandStruct) {
    
    // execute the command in the foreground or background depending on the isBackground flag
    if (commandStruct->isBackground == 0) {
        
        // in this case, we are executing the command in the foreground

        // create a new process to execute the command
        pid_t child_id = fork();
        FILE* sourceFD = NULL;
        FILE* targetFD = NULL;
        int child_exit_status = 0;

        switch(child_id) {
            case -1:
                printf("fork() failed\n");
                fflush(stdout);
                exit(1);

            // child process is created properly and we can execute the command
            case 0:

                // create files for input and output redirection if necessary
                if (commandStruct->inputFile != NULL) {
                    // redirect the input file
                    sourceFD = fopen(commandStruct->inputFile, "r");
                    if (sourceFD == NULL) {
                        printf("fopen() failed for %s", commandStruct->inputFile);
                        fflush(stdout);
                        exit(1);
                    }
                    dup2(fileno(sourceFD), 0);
                }

                if (commandStruct->outputFile != NULL) {
                    // redirect the output file
                    targetFD = fopen(commandStruct->outputFile, "w");
                    if (targetFD == NULL) {
                        printf("fopen() failed for %s", commandStruct->outputFile);
                        fflush(stdout);
                        exit(1);
                    }

                    dup2(fileno(targetFD), 1);
                }

                // execute the command with the arguments

                execvp(commandStruct->command, commandStruct->argumentsArray);
                
                // if execvp returns, there was an error

                printf("execvp() failed with command %s\n", commandStruct->command);
                fflush(stdout);
                exit(1);

            // parent process should wait for the child process to finish, then return the exit status of the child process
            default: 
                // wait for the child process to finish
                waitpid(child_id, &child_exit_status, 0);

                // close the file descriptors if they exist 
                if (sourceFD != NULL) {
                    fclose(sourceFD);
                }
                if (targetFD != NULL) {
                    fclose(targetFD);
                }

                if(WIFEXITED(child_exit_status)) {
                    return WEXITSTATUS(child_exit_status);
                } else {
                    return -1;
                }
            

        }
    } else {
        executeBackgroundCommand(commandStruct);
        return 0;
    }
}

void sigchildHandler(int sig) {
    int status;
    pid_t child_pid;

    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("\n");
            printf("background pid %d is done: exit value %d\n", child_pid, WEXITSTATUS(status));
            // printf(": ");
            fflush(stdout);
        } else {
            printf("\n");
            printf("background pid %d is done: terminated by signal %d\n", child_pid, WTERMSIG(status));
            // printf(": ");
            fflush(stdout);
        }
    }
}

void executeBackgroundCommand(struct command* commandStruct) {

    // create a signal handler for the child processes when they finish to print the exit status
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); 
    sa.sa_handler = sigchildHandler;
    sa.sa_flags = SA_RESTART; 
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        printf("sigaction");
        fflush(stdout);
        return;
    }

    // create a new process
    pid_t child_id = fork();
    FILE* sourceFD = NULL;
    FILE* targetFD = NULL;

    switch(child_id) {
        case -1:
            printf("fork error\n");
            fflush(stdout);
            exit(1); 

        case 0:{

            // child process is created properly and we can execute the command

            // for the background child, handle ignorning the sig
            struct sigaction sa_int;
            memset(&sa_int, 0, sizeof(sa_int));
            sa_int.sa_handler = SIG_IGN;
            sa_int.sa_flags = SA_RESTART;
            if (sigaction(SIGINT, &sa_int, NULL) == -1) {
                printf("sigaction fail");
                fflush(stdout);
                return;
            }

            // create files for input and output redirection if necessary
            if (commandStruct->inputFile != NULL) {
                // redirect the input file
                sourceFD = fopen(commandStruct->inputFile, "r");
                if (sourceFD == NULL) {
                    printf("fopen() failed for %s", commandStruct->inputFile);
                    fflush(stdout);
                    exit(1);
                }
            } else {
                // direct to '/dev/null'
                sourceFD = fopen("/dev/null", "r");
            }

            if (commandStruct->outputFile != NULL) {
                // redirect the output file
                targetFD = fopen(commandStruct->outputFile, "w");
                if (targetFD == NULL) {
                    printf("fopen() failed for %s", commandStruct->outputFile);
                    fflush(stdout);
                    exit(1);
                }
            } else {
                targetFD = fopen("/dev/null", "w");
            }

            // duplicate file descriptors
            if (dup2(fileno(targetFD), 1) == -1 || dup2(fileno(sourceFD), 0) == -1) {
                printf("dup2 failed\n");
                fflush(stdout);
                exit(1);
            }

            // close files after duplicating descriptors
            if (sourceFD) fclose(sourceFD);
            if (targetFD) fclose(targetFD);

            // execute the command with the arguments
            if (commandStruct->numArguments == 0) {
                char* args[] = {commandStruct->command, NULL};
                execvp(commandStruct->command, args);
            } else {
                execvp(commandStruct->command, commandStruct->argumentsArray);
            }

            // if the execvp filed, this will occur
            printf("execvp() failed with command %s\n", commandStruct->command);
            fflush(stdout);
            exit(1);
        }

        default: 
            printf("Background PID is %d\n", child_id);
            fflush(stdout);
            break;
    }
}


void getCommand(char* commandBuffer, int bufferSize) {
    
    printf(": ");
    fflush(stdout);

    // get a string from the user
    fgets(commandBuffer, bufferSize, stdin);
    // remove the newline character from the end of the string
    commandBuffer[strcspn(commandBuffer, "\n")] = 0;
}

struct command* parseCommand(char* commandBuffer) {

    if(strlen(commandBuffer) == 0 || commandBuffer[0] == '#') {
        return NULL;
    }

    // command syntax is "command [arg1 arg2 ...] [< input_file] [> output_file] [&]"
    struct command* commandStruct = malloc(sizeof(struct command));
    commandStruct->argumentsArray = malloc(2 * sizeof(char*));
    commandStruct->numArguments = 0;
    commandStruct->argumentCapacity = 2;
    
    // fill the array with null pointers
    for (int i = 0; i < 2; i++) {
        commandStruct->argumentsArray[i] = NULL;
    }

    commandStruct->inputFile = NULL;
    commandStruct->outputFile = NULL;
    commandStruct->isBackground = 0;

    // get the command
    char* token = strtok(commandBuffer, " ");

    // store the command in the command struct
    commandStruct->command = malloc(strlen(token) + 1);
    strcpy(commandStruct->command, token);

    // add the command itself to the arguments array as the first argument (This is because we use execvp() which requires the command as the first argument in the array of arguments
    commandStruct->argumentsArray[0] = malloc(strlen(token) + 1);
    strcpy(commandStruct->argumentsArray[0], token);
    commandStruct->numArguments = 1;

    // get the first argument or other after the command
    token = strtok(NULL, " ");

    // for each token in the command string until the end of the string
    while (token != NULL) {
        // check for input redirection or background process
        if (strcmp(token, "<") == 0) {
            // get the input file
            token = strtok(NULL, " ");
            commandStruct->inputFile = malloc(strlen(token) + 1);
            strcpy(commandStruct->inputFile, token);
        } else if (strcmp(token, ">") == 0) {
            // get the output file
            token = strtok(NULL, " ");
            commandStruct->outputFile = malloc(strlen(token) + 1);
            strcpy(commandStruct->outputFile, token);
        } else if (strcmp(token, "&") == 0) {
            // set the background flag
            commandStruct->isBackground = 1;

            // if the ampersand was the last token, break out of the loop, otherwise add the ampersand and next token to the arguments array
            token = strtok(NULL, " ");
            if (token == NULL) {
                break;
            } else {
                commandStruct->isBackground = 0;
                addArgument(commandStruct, "&");
                addArgument(commandStruct, token);
            }
        } else {
            // add the current token to the arguments array
            addArgument(commandStruct, token);
        }

        // get the next token
        token = strtok(NULL, " ");
    }

    // replace the "$$" with the pid
    expansionVariablePIDReplace(commandStruct->numArguments, commandStruct->argumentsArray);

    return commandStruct;
}

void addArgument(struct command* commandStruct, char* argument) {
    commandStruct->numArguments++;
        
    if(commandStruct->numArguments >= commandStruct->argumentCapacity) {
        commandStruct->argumentCapacity *= 2;
        char** new_arguments_arr = malloc(commandStruct->argumentCapacity * sizeof(char*));

        // copy the old arguments array to the new one
        for(int i = 0; i < commandStruct->numArguments - 1; i++) {
            new_arguments_arr[i] = commandStruct->argumentsArray[i];
        }

        // free the old arguments array and set the new one
        free(commandStruct->argumentsArray);
        commandStruct->argumentsArray = new_arguments_arr;
        // fill the rest of the array with null pointers
        for (int i = commandStruct->numArguments; i < commandStruct->argumentCapacity; i++) {
            commandStruct->argumentsArray[i] = NULL;
        }
    }

    // add the current token to the arguments array
    commandStruct->argumentsArray[commandStruct->numArguments - 1] = malloc(strlen(argument) + 1);
    strcpy(commandStruct->argumentsArray[commandStruct->numArguments - 1], argument);

    return;
}

void expansionVariablePIDReplace(int numArguments, char** argumentsArray) {
    // iterate through the arguments array and look for the "$$" which is the exapnsion of the pid
    for(int i = 0; i < numArguments; i++) {

        char* position = strstr(argumentsArray[i], "$$");
        
        if(position != NULL) {
            // get the pid
            int pid = getpid();
            char pid_str[10];
            sprintf(pid_str, "%d", pid);
            
            // create the new string 
            char* new_string = malloc(strlen(argumentsArray[i]) + strlen(pid_str) - 1);

            // replace the "$$" with the pid
            char* pos = strstr(argumentsArray[i], "$$");

            // copy the string up to the position of the substring, add the pid, and the rest of the string
            strncpy(new_string, argumentsArray[i], pos - argumentsArray[i]);
            new_string[pos - argumentsArray[i]] = '\0'; // null terminate the string for the concatenation
            strcat(new_string, pid_str);
            strcat(new_string, pos + 2);

            free(argumentsArray[i]);
            argumentsArray[i] = new_string;

            // decrement the index to check the string again for another "$$" in case there are multiple
            i--;
        }
    }
}

void freeCommandStruct(struct command* commandStruct) {
    free(commandStruct->command);

    for (int i = 0; i < commandStruct->numArguments; i++) {
        free(commandStruct->argumentsArray[i]);
    }

    free(commandStruct->argumentsArray);

    if (commandStruct->inputFile != NULL) {
        free(commandStruct->inputFile);
    }

    if (commandStruct->outputFile != NULL) {
        free(commandStruct->outputFile);
    }

    free(commandStruct);
}

