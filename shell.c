#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#define true 1
#define false 0

#define MAX_INPUT_SIZE 100
#define MAX_DIRECTORY_SIZE 100
#define MAX_NUM_ARGS 30
#define MAX_NUM_COMMANDS 10
#define MAX_BACKGROUND_PROCESSES 20
#define MAX_HOSTNAME_LEN 20

char working_dir[MAX_INPUT_SIZE];
char temp_dir[MAX_DIRECTORY_SIZE];
int background_table[MAX_BACKGROUND_PROCESSES];
char* home = NULL;

int add_background_pid(int pid)
{
    printf("process %d started\n", pid);
    
    int i = 0;
    for (; i < MAX_BACKGROUND_PROCESSES; ++i)
    {
        if (background_table[i] == 0) {
            background_table[i] = pid;
            return true;
        }
    }
    return false;
}

void nonblocking_wait(void)
{
    int status;
    int i = 0;
    for (; i < MAX_BACKGROUND_PROCESSES; ++i)
    {
        if (background_table[i] &&
              waitpid(background_table[i], &status, WNOHANG) > 0) {
            printf("process %d ended\n", background_table[i]);
            background_table[i] = 0;
        }
    }
}

void blocking_wait(void)
{
    int pid = 0, status;
    while((pid = wait(&status))!= -1)
    {
        printf("process %d ended\n", pid);
    }
}

void execute(char* commands[MAX_NUM_COMMANDS][MAX_NUM_ARGS],
             int numCommands,
             int background)
{
    
    //Handle built-in commands
    if (strcmp(commands[0][0], "exit") == 0)
    {
        exit(EXIT_SUCCESS);
    }
    else if (strcmp(commands[0][0], "wait") == 0)
    {
        blocking_wait();
        return;
    }
    else if (strcmp(commands[0][0], "cd") == 0)
    {
        if (commands[0][1][0] == '~') {
            strcpy(temp_dir, home);
            strcat(temp_dir, "/");
            strcat(temp_dir, commands[0][1]+1);
            chdir(temp_dir);
            return;
        }
        chdir(commands[0][1]);
        return;
    }
    else if (strcmp(commands[0][0], "pwd") == 0)
    {
        printf("%s\n", getcwd(working_dir, MAX_DIRECTORY_SIZE-1));
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
            exit(EXIT_FAILURE); //execvp should not return
        }
        else if (pid > 0)
        {
            pids[i] = pid; //store pids for wait
        }
        else
        {
            perror("Fork");
            return;
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

    if (pid > 0 && background)
    {
        add_background_pid(pid);
    }
    else if (!background)
    {
        int i = 0, status;
        for (; i < numCommands; ++i)
            waitpid(pids[i], &status, 0);
    }
}

int main()
{
    atexit(blocking_wait);
    
    char input[MAX_DIRECTORY_SIZE];
    char host[MAX_HOSTNAME_LEN];
    char* commands[MAX_NUM_COMMANDS][MAX_NUM_ARGS];
    char* arg;
    int background = false;
    
    home = getenv("HOME");
    char* user = getenv("USER");
    gethostname(host, MAX_HOSTNAME_LEN);
    
    while (true)
    {
        nonblocking_wait();
        
        getcwd(working_dir, MAX_DIRECTORY_SIZE-1);

        if (strstr(working_dir, home)) {
            strcpy(temp_dir, working_dir+strlen(home));
            sprintf(working_dir, "~%s", temp_dir);
        }

        if (isatty(fileno(stdin)))  //Hide prompt on redirect
            printf("%s@%s:[%s]>", user, host, working_dir);
        
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL)
            break;

        size_t loc = strlen(input) - 1;
        if (input[loc] == '\n'){    //strip newline left by fgets
            input[loc] = '\0';
        }
        else
        {
            printf("Input exceeded maximum allowed length of %d characters\n", MAX_INPUT_SIZE);
            char c;
            while((c = getchar()) != '\n' && c != EOF){} //discard extra chars
            continue;
        }
        
        if (loc == 0) {                  //do nothing if empty line
            continue;
        }
        else if (input[loc-1] == '&') {  //detect background processes
            background = true;
            input[loc-1] = '\0';
        }
        else{
            background = false;
        }

        //Determine the command
        commands[0][0] = strtok(input, " ");
            
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
