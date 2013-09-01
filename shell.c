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

char working_dir[MAX_INPUT_SIZE];

int internal_wait()
{
    int pid = 0;
    
    while((pid = wait())!= -1)
    {
        printf("Process %d finished\n", pid);
    }

    return (errno == ECHILD) ? 0 : -1;
}

void execute(char* commands[MAX_NUM_COMMANDS][MAX_NUM_ARGS],
             int numCommands,
             int background)
{    
    //Handle built-in commands (i.e. cd)
    if (strcmp(commands[0][0], "exit") == 0)
    {
        internal_wait();
        exit(0);
    }
    else if (strcmp(commands[0][0], "wait") == 0)
    {
        internal_wait();
        return;
    }
    else if (strcmp(commands[0][0], "cd") == 0)
    {
        chdir(commands[0][1]);
        return;
    }
    else if (strcmp(commands[0][0], "pwd") == 0)
    {
        printf("%s\n", working_dir);
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

    pid_t pid;
    
    int i=0;
    for(; i < numCommands; ++i)
    {
        pid  = fork();
        if (pid == 0) //child
        {
            if (i > 0)
                dup2(pipes[i-1][0], 0); //Make previous pipe read into STDIN
                
            if ( i < numCommands-1 )
                dup2(pipes[i][1], 1);   //Make current pipe write into STDOUT
            else
                dup2(pipes[i][1], savedOUT);

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
        else if (pid > 0)
        {
            pids[i] = pid;
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
    else if (pid > 0 && background)
    {
        printf("Spawning process %d\n", pid);
    }
    else if (numCommands > 1)
    {
        int i = 0;
        for (; i < numCommands; ++i)
            waitpid(pids[i]);
    }
}

int main()
{
    char input[MAX_DIRECTORY_SIZE];

    char* commands[MAX_NUM_COMMANDS][MAX_NUM_ARGS];
    char* arg;
    int background = false;
          
    while (true)
    {
        getcwd(working_dir, MAX_DIRECTORY_SIZE-1);

        if (isatty(fileno(stdin)))  //Hide prompt on redirect
            printf("%s>", working_dir);
        
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
