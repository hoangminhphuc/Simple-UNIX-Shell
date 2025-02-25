#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 100   // The maximum length command

char last_command[MAX_LINE]; // Stores last command for !! feature

int main(void) {
    char input[MAX_LINE];
    char *args[MAX_LINE/2 + 1];  // command line arguments
    int should_run = 1;        // flag to check whether the program runs or not

    while (should_run) {
        // Print prompt
        printf("osh> ");
        fflush(stdout);

        if (!fgets(input, MAX_LINE, stdin)) continue;


        // Remove the new line character
        input[strcspn(input, "\n")] = '\0';

	// Continue to next iteration if user enters blank command
	if (strlen(input) == 0) {
	    continue;
	}

        // If the command is "exit", then terminate the shell.
        if ((strcmp(input, "exit") == 0) || (strcmp(input, "EXIT")==0)) break;

	// Check if the user wants to execute the last command with "!!"
        if (strcmp(input, "!!") == 0) {
            if (strlen(last_command) == 0) {
                printf("No commands in history.\n");
                continue;
            } else {
                printf("Executing: %s\n", last_command);
	            strcpy(input, last_command);
            }
        } else {
            // Updating last command
            strcpy(last_command, input);
        }



        
        // Split input into tokens and check for specific commands
        int token_count = 0, background = 0, pipe_index = -1;
        char *token = strtok(input, " "); //seperate by whitespace
        while (token != NULL) {
            if (strcmp(token, "&") == 0) {
                background = 1;
            } else if (strcmp(token, "|") == 0) {
                args[token_count] = token;
                pipe_index = token_count;
                token_count++; 
                // args[token_count] = NULL; // End first command
            } else {
                args[token_count++] = token;
            }
            token = strtok(NULL, " "); 
        }
        
        args[token_count] = NULL;
        
        // If a pipe is detected, handle the pipe case
        if (pipe_index != -1) {
            // Split into 2 commands
            args[pipe_index] = NULL;
            char **args1 = args;             // command before pipe
            char **args2 = &args[pipe_index + 1];  // command after pipe

    
            int fd[2];
            if (pipe(fd) < 0) {
                perror("pipe failed");
                exit(1);
            }

            // First child: executes the command on the left side of the pipe
            pid_t pid1 = fork();
	    if (pid1 < 0) {
                perror("fork failed");
                exit(1);
            } else if (pid1 == 0) {
                // Redirect stdout to the write end of the pipe
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
                execvp(args1[0], args1);
                perror("execvp failed");
                exit(1);
            }

            // Second child: executes the command on the right side of the pipe
            pid_t pid2 = fork();
            if (pid2 < 0) {
                perror("fork failed");
                exit(1);
            } else if (pid2 == 0) {
                // Redirect stdin to the read end of the pipe
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]);
                close(fd[0]);
                execvp(args2[0], args2);
                perror("execvp failed");
                exit(1);
            }
            // Parent closes both ends of the pipe
            close(fd[0]);
            close(fd[1]);

            // Wait for both children to complete
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);

            continue;
        }


        // redirect type 1 for output, 2 for input
        int redirect_index = -1;
        int redirect_type = 0; // 1 for output, 2 for input
        for (int i = 0; i < token_count; i++) {
            if (strcmp(args[i], ">") == 0) {
                redirect_index = i;
                redirect_type = 1;
                break;
            }
            if (strcmp(args[i], "<") == 0) {
                redirect_index = i;
                redirect_type = 2;
            break;
            }
        }

            // If redirection is found, handle it
            if (redirect_index != -1) {
                if (args[redirect_index + 1] == NULL) {
                    fprintf(stderr, "Error: No file specified for redirection\n");
                    continue;
                }
                char *file = args[redirect_index + 1];
                args[redirect_index] = NULL;
            
                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork failed");
                    exit(1);
                } else if (pid == 0) {
                    if (redirect_type == 1) { // Output redirection: command > file
                        int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0) {
                            perror("open failed");
                            exit(1);
                        }
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                    } else if (redirect_type == 2) { // Input redirection: command < file
                        int fd = open(file, O_RDONLY);
                        if (fd < 0) {
                            perror("open failed");
                            exit(1);
                        }
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                    }
                    execvp(args[0], args);
                    perror("execvp failed");
                    exit(1);
                }
                if (!background) {
                    waitpid(pid, NULL, 0);
                }
            }
            else {
                // No pipe or redirection; execute the command normally.
                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork failed");
                    exit(1);
                } else if (pid == 0) {
                    execvp(args[0], args);
                    perror("execvp failed");
                    exit(1);
                }
                if (!background) {
                    waitpid(pid, NULL, 0);
                }
            }
        }
    return 0;
}

