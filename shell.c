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


int internal_wait()
{
    int pid = 0;
    
    while(pid != -1)
    {
        pid = wait();
        if (pid == -1) break;
        printf("Process %d finished\n", pid);
        continue;
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


void execute(char** args, int background)
{    
    //Handle built-in commands (i.e. cd)
    if (strcmp(args[0], "exit") == 0)
    {
        if (!num_args(args, 0)) //Exit must have no other args
            return;
        internal_wait();
        exit(0);
    }
    else if (strcmp(args[0], "wait") == 0)
    {
        if (!num_args(args, 0))
            return;
        internal_wait();
        return;
    }
    else if (strcmp(args[0], "cd") == 0)
    {
        if (!num_args(args, 1))
            return;
        chdir(args[1]);
        return;
    }
    
    //Send other commands to the OS
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Error forking process");
        return;
    }
    else if (pid == 0)
    {   
        if (execvp(*args, args) != -1)
        {
            exit(0);
        }
        else     //An error has occured
        {
            perror(*args);
            exit(-1);
        }
    }
    else 
    {
        if (!background)
            while(wait() != pid) return;
        else
            printf("Starting new process %lu\n", (unsigned long)pid);
    }
}

int main()
{
    char working_dir[MAX_INPUT_SIZE];
    char input[MAX_DIRECTORY_SIZE];

    char* commands[MAX_NUM_COMMANDS][MAX_NUM_ARGS];
    char* args[MAX_NUM_ARGS];
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

        args[0] = strtok(input, " "); //Determine the command
            
        //Parse arguments
        size_t index = 1;
        while (input != NULL)
        {
            arg = strtok(NULL, " ");
            if (arg == NULL)
                break;
        
            args[index++] = arg;
        }
        args[index] = NULL;
    
        execute(args, background);

    }
    return 0;
}
