#define _POSIX_C_SOURCE 200809L


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_STR_LEN 256
#define MAX_TOKENS 20


void signal_handler(int signum) {

    switch (signum) {
    case SIGINT: 
        printf(" SIGINT received. Use ':q' for exiting.\n");
        break;
    case SIGTERM:
        printf(" SIGINT received\n");
        kill(0, SIGTERM);
        break;
    case SIGCHLD:
        printf(" SIGCHLD received\n");
        break;
    case SIGTSTP:
        printf(" SIGTSTP received\n");
        break;
    default:
        printf("Catcher caught unexpected signal %d\n",signum);
  }

      fflush(stdout);
}

void remove_blanks(char *str) {
    if (strlen(str) > 1 && ( str[strlen(str) - 1] == ' ' || str[strlen(str) - 1] == '\n'))
        str[strlen(str) - 1] = '\0';
    if (strlen(str) > 1 && (str[0] == ' ' || str[0] == '\n'))
        memmove(str, str + 1, strlen(str));
}


int getArguments(char *str, char *words[20], int max_words){
    
    int i = 0;  
    char* token = strtok(str, " ");

    while (token != NULL && i < max_words ) {

        if (*token == '\0') {
            continue; // ignore empty tokens
        }

        words[i] = malloc(sizeof(token)+1);

        strcpy(words[i], token);
        i++;
        token = strtok(NULL, " ");
    }

    if (i < max_words)
        words[i] = NULL; 
    else{
        perror("Out of array size\n");
        exit(EXIT_FAILURE);   
    }
    return i;
    


}

int parse_pipes(char* str, char* commands[]) {
    int i = 0;
    char *command;
    command = strtok(str, "|");
    while (command != NULL && i < MAX_TOKENS) {
        commands[i] = command;
        i++;
        command = strtok(NULL, "|");
    }
    free(command);
    return i++;
}

bool parse_redirection(char* str, char* commands[], char c) {
    char *command;
    command = strtok(str, (char[]){c, '\0'});
    if (command != NULL)
    {
        commands[0] = command;
        command = strtok(NULL, (char[]){c, '\0'});

        if (command != NULL)
        {
            commands[1] = command;
            return true;
        }
    }
    free(command);
    return false;
}


bool handle_redirection_fork(char *str, char c){
    int fd,childPid;
    int status;
    char *commands[20];
    char* words[20]; 

    parse_redirection(str, commands, c);
    remove_blanks(commands[0]);
    remove_blanks(commands[1]);

    getArguments(commands[0], words, 10);

    
    childPid = fork();
    if (childPid == 0)
    {

        if (c == '>')
        {
            fd = open(commands[1], O_WRONLY | O_CREAT, 0666);
            if(fd == 0){
                perror("cant opened file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, 1); //output redirection
            close(fd);
        }

        if (c == '<')
        {
            fd = open(commands[1], O_RDONLY | O_CREAT, 0666);
            dup2(fd, 0); //input redirection
            close(fd);
        }
        
        execvp(words[0], words);
        _exit(127);
    }

    else if (childPid == -1)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    else
    {   //parent process
        if (wait(&status) == -1) 
            if (errno != EINTR)  /* Error other than EINTR */
                status = -1;
    }
    return true;
}

bool handle_redirection(char *str, char c){
    int fd;
    char *commands[20];
    char* words[20]; 

    parse_redirection(str, commands, c);
    getArguments(commands[0], words, 10);
        
 
        if (c == '>')
        {
            fd = open(commands[1], O_WRONLY | O_CREAT, 0666);
            if(fd == -1){
                perror("Couldn't opened file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, 1); //input redirection
            close(fd);

        }
        if (c == '<')
        {
            fd = open(commands[1], O_WRONLY | O_CREAT, 0666);
            if(fd == -1){
                perror("Couldn't opened file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, 0); //input redirection
            close(fd);
        }
        execvp(words[0], words);
   
    return true;

    
}


int terminal_emulator(char* commands[], int numOfCommands){

    pid_t childPid;
    char* words[20]; 
    int status;
    int free_words=0;
    int fd[numOfCommands-1][2];
    int pids[numOfCommands];   

     for (int i = 0; i < numOfCommands - 1; i++) {
        if (pipe(fd[i]) == -1) {
            perror("Pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Create log file with a name corresponding to the current timestamp
    char filename[50];
    FILE *logFile;
    time_t t = time(NULL);


    for (int i=0; i < numOfCommands; i++ )
    {
        strftime(filename, sizeof(filename), "log/log_%Y%m%d_%H%M%S.txt", localtime(&t));
        logFile = fopen(filename, "w");
        childPid = fork();

        switch (childPid)
        {
        case -1:
            status = -1;
            perror("Fork");
            break;
        case 0:

            if (numOfCommands != 1)
            {
                if (i == 0 )
                {
                    dup2(fd[i][1], 1);
                    close(fd[i][0]);
                    close(fd[i][1]);

                }
                else if (i == numOfCommands-1)
                {
                    dup2(fd[i-1][0], 0);
                    close(fd[i-1][1]);
                    close(fd[i-1][0]);
                }

                else
                {
                    close(fd[i-1][1]); // Close the read end of pipe i-1
                    close(fd[i][0]); // Close the write end of pipe i
                    dup2(fd[i][1], STDOUT_FILENO); // Redirect stdout to the write end of pipe i-1
                    dup2(fd[i-1][0], STDIN_FILENO); 
                }
            }

            if (strchr(commands[i], '<') != NULL ) {
                handle_redirection(commands[i], '<');

            }
            else if(strchr(commands[i], '>') != NULL) {
                handle_redirection(commands[i], '>');
            }
            else{
                free_words = getArguments(commands[i], words, 10);
                execvp(words[0], words);
                perror("execvp");   
                fclose(logFile);
                for (int i = 0; i <= free_words; i++)
                {
                        free(words[i]);
                }
                exit(EXIT_FAILURE);
            }
        
        default:
            pids[i] = childPid;
            break;          
        }
    }

    // Parent process
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;

    if ((sigemptyset(&sa.sa_mask) == -1) || sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGCHLD, &sa, NULL) == -1 ) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Close all the pipe file descriptors in the parent process
    for (int i = 0; i < numOfCommands - 1; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }

  
    for (int i = 0; i < numOfCommands; i++) {
        if (wait(&status) == -1) {
            if (errno != EINTR) { /* Error other than EINTR */
                status = -1;
                break; /* So exit loop */
            }
        }
        else{
            if (WIFEXITED(status)) 
                fprintf(logFile, "pid: %d  command: %s  exit status: %d\n", pids[i], commands[i], WEXITSTATUS(status));
            else if (WIFSIGNALED(status))
                fprintf(logFile, "pid: %d, command: %s, terminated by signal %d\n", pids[i], commands[i], WTERMSIG(status));
            
        }
    }

    fclose(logFile);
    return 0;
}

int main() {
    int numOfCommand = 0;
    char str[MAX_STR_LEN];
    char *commands[MAX_TOKENS];
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;

    if ((sigemptyset(&sa.sa_mask) == -1) || (sigaction(SIGINT,&sa, NULL) == -1) || (sigaction(SIGTSTP, &sa, NULL) == -1))
        perror("Failed to install signal handlers");

    for(;;){
        printf("\033[36m%s\033[0m", "$: ");
        fflush(stdout);
        if(fgets(str, MAX_STR_LEN, stdin))
        {
            if (strcmp(str, ":q\n") == 0)
                break; 
           
            remove_blanks(str);
            numOfCommand = parse_pipes(str, commands);

            if (numOfCommand == 1 && strchr(commands[0], '<') != NULL)
                handle_redirection_fork(commands[0], '<');

            else if(numOfCommand == 1 && strchr(commands[0], '>') != NULL) 
                handle_redirection_fork(commands[0], '>');
            
            else{
                terminal_emulator(commands, numOfCommand);
            }
        }
    }

    return 0;
}

