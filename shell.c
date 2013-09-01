#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define true 1
#define false 0

#define MAX_INPUT_SIZE 100
#define MAX_DIRECTORY_SIZE 100
#define MAX_NUM_ARGS 30
#define MAX_NUM_COMMANDS 10


int internal_wait()
{
    int pid = 0;
    
    while((pid = wait())!= -1)
    {
        printf("Process %d finished\n", pid);
    }

    return (errno == ECHILD) ? 0 : -1;
}

int num_args(char** args, unsigned int num)
{
    if (args[num+1] != NULL) 
    {
        printf("Invalid number of arguments for command\n");
        return false;
    }
    return true;
}


void execute(char* commands[MAX_NUM_COMMANDS][MAX_NUM_ARGS],
             int numCommands,
             int background)
{    
    //Handle built-in commands (i.e. cd)
    if (strcmp(commands[0][0], "exit") == 0)
    {
        if (!num_args(commands[0], 0)) //Exit must have no other args
            return;
        internal_wait();
        exit(0);
    }
    else if (strcmp(commands[0][0], "wait") == 0)
    {
        if (!num_args(commands[0], 0))
            return;
        internal_wait();
        return;
    }
    else if (strcmp(commands[0][0], "cd") == 0)
    {
        if (!num_args(commands[0], 1))
            return;
        chdir(commands[0][1]);
        return;
    }

    
    int pipes[MAX_NUM_COMMANDS][2];
    int pids[MAX_NUM_COMMANDS];
    int pipeNum;
    for(pipeNum=0; pipeNum < numCommands; ++pipeNum)
    {
        pipe(pipes[pipeNum]);
    }

    int savedOUT = dup(STDOUT_FILENO);
    int savedIN  = dup(STDIN_FILENO);

    pid_t pid_last = 0;
    pid_t pid = fork();
    if (pid == 0) // Special case for 1st command
    {
        
        if (numCommands > 1)
        {
            dup2(pipes[0][1], 1); //if in a pipe, redirect output
            
            close(pipes[0][0]);
            close(pipes[0][1]);
        }

        execvp(commands[0][0], commands[0]);
        perror(commands[0][0]);
        exit(-1);
    }
    else
    {
        pids[0] = pid;
    }
    
    if (numCommands > 1) {

        int i=1;
        for(; i < numCommands-1; ++i)
        {
            pid_t pid_inner = fork();
            if (pid_inner == 0) //child
            {
                
                dup2(pipes[i-1][0], 0); //Make previous pipe read into STDIN
                dup2(pipes[i][1], 1);   //Make current pipe write into STDOUT

                int j = 0;
                for (; j < numCommands; ++j)
                {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                
                execvp(commands[i][0], commands[i]);
                perror(commands[i][0]);
                exit(-1); //execvp should not return
            }
            else
            {
                pids[i] = pid_inner;
            }
        }

        pid_last = fork();  //Special case for last command
        if (pid_last == 0)
        {

            dup2(pipes[i-1][0], 0);
            dup2(pipes[i][1], savedOUT); //send output to stdout

            int j = 0;
            for (; j < numCommands; ++j)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            execvp(commands[i][0], commands[i]);
            perror(commands[i][0]);
            exit(-1);
        }
        else
        {
            pids[i] = pid_last;
        }
    }

    pipeNum = 0;
    for (; pipeNum < numCommands; ++pipeNum)
    {
        if (pipes[pipeNum][0] != STDOUT_FILENO)
            close(pipes[pipeNum][0]);
        if (pipes[pipeNum][1] != STDIN_FILENO)
            close(pipes[pipeNum][1]);
    }

    if ( (pid > 0 && !background && numCommands == 1))
    {
        waitpid(pid);
    }
    else if (numCommands > 1 && pid_last > 0)
    {
        int i = 0;
        for (; i < numCommands; ++i)
            waitpid(pids[i]);
    }
}

int main()
{
    char working_dir[MAX_INPUT_SIZE];
    char input[MAX_DIRECTORY_SIZE];

    char* commands[MAX_NUM_COMMANDS][MAX_NUM_ARGS];
    char* arg;
    int background = false;
          
    while (true)
    {
        getcwd(working_dir, MAX_DIRECTORY_SIZE-1);
        strcat(working_dir, ">");

        if (isatty(fileno(stdin)))  //Hide prompt on redirect
            printf("%s", working_dir);
        
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL)
            break;

        size_t loc = strlen(input) - 1;
        if (input[loc] == '\n'){    //strip newline left by fgets
            input[loc] = '\0';
        }
        else
        {
            printf("Input too long\n");
            continue;
        }
        
        if (loc == 0)               //do nothing if empty line
            continue;
        if (input[loc-1] == '&') {  //detect background processes
            background = true;
            input[loc-1] = '\0';
        }
        else
        {
            background = false;
        }

        commands[0][0] = strtok(input, " "); //Determine the command
            
        //Parse arguments
        size_t index = 1;
        size_t command_num = 0;
        while (input != NULL)
        {
            arg = strtok(NULL, " ");
            if (arg == NULL) {
                break;
            }
            else if (strcmp(arg, "|") == 0) {
                commands[command_num][index] = NULL;
                ++command_num;
                index=0;
            }
            else {
                commands[command_num][index++] = arg;
            }
        }
        commands[command_num][index] = NULL;

        execute(commands, command_num+1, background);
        
    }
    return 0;
}
