/*
 * CSE 3033 - Project #2
 * Mert Kelkit - 150115013
 * Furkan Nakıp - 150115032
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
#define MAX_ARGS 32 /* Assumed as max number of arguments */
#define MAX_PATH_VAR 20 /* Assumed as max number of path variables */
#define NO_ALIAS -1 /* To check if a command is alias or not */

/* File opening modes for I/O redirection */
#define TRUNC_OUT 2     // For truncation
#define APPEND_OUT 3    // For append
#define ERR_OUT 4       // For printing err
#define IN_MODE 5       // For using file as input

// A struct that contains information about path variables
typedef struct {
    char** paths;
    int no_of_paths;
} PATH_INFO;

// Linked list node in order to store aliases
typedef struct alias_node {
    char* alias;
    char** original_args;
    int no_of_args;
    struct alias_node* next;
} ALIAS_NODE;

// Background process queue data structure
typedef struct background_queue {
    pid_t pid;
    pid_t group_pid;
    char* command;
    struct background_queue* next;
} BACKGROUND_PROCESS_QUEUE;

// Set NULL background process queue and alias list first.
BACKGROUND_PROCESS_QUEUE* bg_process_queue = NULL;
ALIAS_NODE* alias_root = NULL;

// Variables that hold info about background process
bool ANY_FOREGROUND_PROCESS = false;
int CURRENT_FOREGROUND_PROCESS;


/* FUNCTION PROTOTYPES */

// parses path variables
PATH_INFO* parse_path();
// argument parser function
void setup(char inputBuffer[], char* args[], int *background);
// sets count to the number of given arguments
void get_args_count(char* args[], int* count);
// checks a command is in path variables
int check_path(char* args[], PATH_INFO* path_info, char** exec_path);

// stores alias that user entered
void store_alias(char* args[], int arg_no);
// removes alias that user wanted
void remove_alias(char* alias);
// checks if an alias exists or not
int alias_exists(char* alias);
// returns alias index of given command, if doesn't exist, returns NO_ALIAS (-1)
int get_alias_index(char* arg);
// returns original command of given alias index
char** get_alias(int index);
// generates output of alias -l
void print_alias();

// checks a command includes redirection or not, executes if it's a redirection operation
int check_redirection(char* args[], int arg_count, int* background, PATH_INFO* path_info);
// checks a command is builtin or not
int check_is_builtin(char* args[], int arg_count);
// executes a command that found in path
void exec_command(char* args[], char* exec_path, int* background);

/* QUEUE UTILITY FUNCTIONS */
void enqueue(pid_t pid, pid_t group_pid, char* command);
void dequeue(int* pid, int* group_pid);
/* QUEUE UTILITY FUNCTIONS */

// an utility function to slice strings - used with aliasing
void slice_string(char** dest, const char* src, int from, int to);

// handler function for child process that terminated
void handle_child();
// handler function for ^Z
static void sigtstp_handler(int signum);
/* FUNCTION PROTOTYPES */


/* DEBUG */
// iterates through queue and prints information
void printq() {
    BACKGROUND_PROCESS_QUEUE* iter = bg_process_queue;
    if(iter == NULL) {
        printf("No background processes found\n");
        return;
    }
    int i = 0;
    while(iter != NULL) {
        printf("[%d]  %d\t\t%s\n", i, iter->pid, iter->command);
        iter = iter->next;
        i++;
    }
}

int main(void) {
    // declare required variables
    char inputBuffer[MAX_LINE];
    int background;
    char *args[MAX_LINE/2 + 1];
    int in_path;
    char* exec_path;
    int args_count;
    int alias_index;
    char** aliased_args;
    char** send_arg;

    /* Initialize signal */
    struct sigaction act;
    act.sa_handler = sigtstp_handler;
    act.sa_flags = SA_RESTART;

    // Set up signal handler for ^Z signal
    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGTSTP, &act, NULL) == -1)) {
        fprintf(stderr, "Failed to set SIGTSTP handler\n");
        return 1;
    }

    // Get path variable
    PATH_INFO* path_info = parse_path();

    while (1) {
        // reset variables for new argument
        args_count = 0;
        background = 0;
        printf("myshell: ");
        fflush(stdout);

        setup(inputBuffer, args, &background);

        // If empty command entered, continue getting input
        if(args[0] == NULL) {
            continue;
        }

        // Check given command is aliased or not
        alias_index = get_alias_index(args[0]);
        // If it's an alias for some command
        if (alias_index != NO_ALIAS) {
            aliased_args = get_alias(alias_index);
            // Update send args to call execl properly
            send_arg = aliased_args;
        } else {
            // If it's not an alias, send args directly
            send_arg = args;
        }

        // Check send_arg contains ampersand
        int i = 0;
        while(send_arg[i] != NULL) {
            if(!strcmp(send_arg[i], "&")) {
                background = 1;
                send_arg[i] = NULL;
                break;
            }
            i++;
        }

        // Initialize number of arguments
        get_args_count(send_arg, &args_count);

        // Check if command is builtin, if it is, execute
        if(check_is_builtin(send_arg, args_count) == 0) {
            // SUCCESS
            continue;
        }

        // Check if command requires redirection, if it is, execute
        else if(check_redirection(send_arg, args_count, &background, path_info) == 0) {
            // SUCCESS
            continue;
        }

        // If not redirection and builtin command
        else {
            // Check path in order to determine given command is executable or not
            in_path = check_path(send_arg, path_info, &exec_path);

            // If command found in path - aliased or not -
            if (!in_path) {
                exec_command(send_arg, exec_path, &background);
            }
            // If command is not executable
            else {
                fprintf(stderr, "%s: Command not found.\n", send_arg[0]);
            }
            // Reset values for new command
            alias_index = NO_ALIAS;
            aliased_args = NULL;
            send_arg = NULL;
        }
    }
}

int check_is_builtin(char* args[], int arg_count) {
    int status;
    // If user entered exit
    if(!strcmp(args[0], "exit")) {
        // Check background processes
        if(bg_process_queue != NULL) {
            // inform user
            fprintf(stderr, "there are some processes that are running background\n");
            fprintf(stderr, "type 'fg' and ^Z to terminate these processes one by one\n");
            printq();
            // return to get new commands
            return 0;
        }
        // If no background processes running currently, exit program
        else {
            printf("session ends\nbye!\n");
            exit(0);
        }
    }
    // If user entered clr
    else if(!strcmp(args[0], "clr")) {
        // This command means clear, implemented with escape characters
        // [H for clearing down, [J for setting cursor to beginning
        printf("\033[H\033[J");
        return 0;
    }
    // If user entered fg
    else if(!strcmp(args[0], "fg")) {
        // If no process in background, inform user and return
        if(bg_process_queue == NULL) {
            fprintf(stderr, "no currently running background processes found.\n");
            return 0;
        }
        // If there is a process in the background
        // This loop will continue until no more background processes left
        while(bg_process_queue != NULL) {
            int pid, gpid;
            // dequeue operation in order to get the process from background
            dequeue(&pid, &gpid);
            // set foreground process variables because we have a foreground process now
            ANY_FOREGROUND_PROCESS = true;
            CURRENT_FOREGROUND_PROCESS = pid;
            // let the process continue in foreground
            // new command will be taken after all background processes terminated on foreground
            kill(pid, SIGCONT);
            // wait until a background process terminates
            waitpid(pid, &status, WUNTRACED);
        }
        return 0;
    }
    // If user entered alias
    else if(!strcmp(args[0], "alias")) {
        // If user wants to list all aliases
        if(!strcmp(args[1], "-l")) {
            print_alias();
            return 0;
        }
        // If user wants to create an alias - assumed as all aliased arguments should start with "
        if(args[1][0] == '"') {
            store_alias(args, arg_count);
            return 0;
        }
        // If alias is not used properly
        else {
            fprintf(stderr, "alias: wrong command usage %s\n", args[1]);
            return 0;
        }
    }
    // If user entered unalias
    else if(!strcmp(args[0], "unalias")) {
        if(args[1] != NULL) {
            remove_alias(args[1]);
        }
        else {
            fprintf(stderr, "Alias to be removed must be specified.\n");
        }
        return 0;
    }
    // Returns -1 is none of the commands are builtin
    return -1;
}

// Checks a command contains redirection operation
int check_redirection(char* args[], int arg_count, int* background, PATH_INFO* path_info) {
    int i;
    int is_redirection_command = 0;
    // If any redirection operation found, exit the loop
    for(i=0; i<arg_count; i++) {
        if(!strcmp(args[i], "<") || !strcmp(args[i], ">") ||
                        !strcmp(args[i], ">>") || !strcmp(args[i], "2>")) {
            is_redirection_command = 1;
            break;
        }
    }
    // If there is no redirection operation, return from the function
    if(!is_redirection_command) {
        return -1;
    }

    // Create child process
    pid_t child_pid = fork();

    // fork error
    if(child_pid < 0) {
        fprintf(stderr, "Failed to fork\n");
        return -1;
    }

    // child process
    if(child_pid == 0) {
        // In order to determine redirection mode
        int mode = 0;
        // Input and output file names to be created
        char in_file[32];
        char out_file[32];
        for(i=0; i<arg_count; i++) {
            // If redirect is in input mode, there must be a file name in args[i+1]
            if(!strcmp(args[i], "<")) {
                // If there is a file name
                if(args[i+1] != NULL) {
                    // remove "<" from the args in order to call execl properly
                    args[i] = NULL;
                    // set mode and get input file name
                    mode = IN_MODE;
                    strcpy(in_file, args[i+1]);
                    // Check if there is any output redirection
                    if(args[i+2] != NULL) {
                        // If there is an output redirection too
                        if(!strcmp(args[i+2], ">")) {
                            // And there is a file name for output
                            if(args[i+3] != NULL) {
                                // set mode variable
                                mode += TRUNC_OUT;
                                // get output file name
                                strcpy(out_file, args[i+3]);
                                break;
                            }
                            // If no output file name provided after >
                            else {
                                fprintf(stderr, "Output file not provided\n");
                                exit(-1);
                            }
                        }
                        else {
                            // Only input redirection, ignore other arguments
                            break;
                        }
                    }
                    else {
                        // Only input redirection
                        break;
                    }
                }
                // If no input file name provided after <
                else {
                    fprintf(stderr, "Input file not provided\n");
                    exit(-1);
                }
            }

            // if redirect is in output&truncation mode, there must be a file name in args[i+1]
            else if(!strcmp(args[i], ">")) {
                // Get file name if not null
                if(args[i+1] != NULL) {
                    // Remove ">" in order to call execly properly
                    args[i] = NULL;
                    // Set mode variable and get output file name
                    mode = TRUNC_OUT;
                    strcpy(out_file, args[i+1]);
                    break;
                }
                // If no file name provided after >
                else {
                    fprintf(stderr, "Output file not provided\n");
                    exit(-1);
                }
            }

            // if redirect is in output&append mode, there must be a file name in args[i+1]
            else if(!strcmp(args[i], ">>")) {
                if(args[i+1] != NULL) {
                    // Remove ">>" in order to call execly properly
                    args[i] = NULL;
                    // Set mode variable and get output file name
                    mode = APPEND_OUT;
                    strcpy(out_file, args[i+1]);
                    break;
                }
                // If no output file name provided after >>
                else {
                    fprintf(stderr, "Output file not provided\n");
                    exit(-1);
                }
            }

            // if redirect is in error output mode, there must be a file name in args[i+1]
            else if(!strcmp(args[i], "2>")) {
                if(args[i+1] != NULL) {
                    // Remove "2>" in order to call execly properly
                    args[i] = NULL;
                    // Set mode variable and get output file name
                    mode = ERR_OUT;
                    strcpy(out_file, args[i+1]);
                    break;
                }
                // If no output file name provided
                else {
                    fprintf(stderr, "Output file not provided\n");
                    exit(-1);
                }
            }
        }

        // If x < y or x < y > z used properly, redirect standard input to the given file
        if(mode == IN_MODE || mode == IN_MODE + TRUNC_OUT) {
            int fd_in;
            // open in read only mode, if opening fails...
            if ((fd_in = open(in_file, O_RDONLY)) < 0) {
                fprintf(stderr, "Couldn't open input file\n");
                exit(-1);
            }
            // duplicate standard input file
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);                   // close file
        }
        // If > used properly
        else if(mode == TRUNC_OUT) {
            int fd_trunc;
            // open file with write permission, create if doesn't exists, truncate if exists...
            // with desired read, write, execute permissions (0644 in our program)
            if ((fd_trunc = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
                fprintf(stderr, "Couldn't open output file\n");
                exit(-1);
            }
            // duplicate standard output to the given file
            dup2(fd_trunc, STDOUT_FILENO);
            close(fd_trunc);                   // close file
        }
        // If user wants to redirect standard output to the given file with append mode
        else if(mode == APPEND_OUT) {
            int fd_append;
            // open file with write permission, create if it doesn't exists, append if exists...
            // with desired read, write, execute permissions (0644 in our program)
            if ((fd_append = open(out_file, O_WRONLY | O_CREAT | O_APPEND, 0644)) < 0) {
                fprintf(stderr, "Couldn't open output file\n");
                exit(-1);
            }
            // duplicate standard output to the given file
            dup2(fd_append, STDOUT_FILENO);
            close(fd_append);                   // close file
        }
        // If user wants to redirect standard error to the given file
        else if(mode == ERR_OUT) {
            int fd_err;
            // open file with write permission, create if doesn't exists, truncate if exists...
            // with desired read, write, execute permissions (0644 in our program)
            if ((fd_err = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
                fprintf(stderr, "Couldn't open output file\n");
                exit(-1);
            }
            // redirect standard error file to the given file
            dup2(fd_err, STDERR_FILENO);
            close(fd_err);                   // close file
        }
        // This condition is for opening output file of the command "x < y > z"
        if(mode == IN_MODE + TRUNC_OUT) {
            int fd_out;
            // open file with write permission, create if doesn't exists, truncate if exists...
            // with desired read, write, execute permissions (0644 in our program)
            if ((fd_out = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
                fprintf(stderr, "Couldn't open output file\n");
                exit(-1);
            }
            // duplicate standard output
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);                   // close file
        }

        // Declare variables to run command
        int alias_index;
        int in_path;
        char** aliased_args;
        char** send_arg = (char**)malloc(MAX_ARGS * sizeof(char*));
        char* exec_path;

        // Get alias if exists
        alias_index = get_alias_index(args[0]);

        // It will be equal to NO_ALIAS if no alias found with given arg[0]
        if(alias_index != NO_ALIAS) {
            // get alias from index
            aliased_args = get_alias(alias_index);
            // send_arg will be sent to execl
            send_arg = aliased_args;
        }
        // If command is not alias, call execl with it directly
        else {
            send_arg = args;
        }

        in_path = check_path(send_arg, path_info, &exec_path);

        // If command found in path - aliased or not -
        if(!in_path) {
            // execute command
            execl(exec_path, send_arg[0], send_arg[1], send_arg[2], send_arg[3], send_arg[4], send_arg[5], send_arg[6],
                    send_arg[7], send_arg[8], send_arg[9], send_arg[10], send_arg[11], send_arg[12], send_arg[13],
                    send_arg[14], send_arg[15], send_arg[16], send_arg[17], send_arg[18], send_arg[19], send_arg[20],
                    send_arg[21], send_arg[22], send_arg[23], send_arg[24], send_arg[25], send_arg[26], send_arg[27],
                    send_arg[28], send_arg[29], send_arg[30], send_arg[31], NULL);
            fprintf(stderr, "Execl error\n");
            exit(-1);
        }
        // If command cannot be found in path, print error
        else {
            fprintf(stderr, "%s: Command not found.\n", send_arg[0]);
            exit(-1);
        }
    }
    // parent process
    if(child_pid > 0) {
        int status;
        // If not in background mode
        if(*background == 0) {
            // Update foreground process variables
            ANY_FOREGROUND_PROCESS = true;
            CURRENT_FOREGROUND_PROCESS = child_pid;
            // Wait for child to end
            waitpid(child_pid, &status, 0);
            // No foreground process left
            ANY_FOREGROUND_PROCESS = false;
            *background = 0;
            return 0;
        }
        // If in background mode
        else {
            // Add process to background process queue
            enqueue(child_pid, getpgrp(), args[0]);
            // Print background process queue
            printq();
            *background = 0;
            return 0;
        }
    }
    return -1;
}

// Returns index of given arg if it's an alias
int get_alias_index(char* arg) {
    int index = 0;
    ALIAS_NODE* iter = alias_root;
    // iterate through alias list
    while(iter != NULL) {
        if(!strcmp(iter->alias, arg)) {
            return index;
        }
        iter = iter->next;
        index++;
    }
    // return NO_ALIAS (-1) if no alias found
    return NO_ALIAS;
}

// Returns original command of alias with given index
char** get_alias(int index) {
    int i = 0;
    // iterates through list
    ALIAS_NODE* iter = alias_root;
    while(i < index) {
        iter = iter->next;
        i++;
    }
    // returns real argument
    return iter->original_args;
}

// Argument parser function
void setup(char inputBuffer[], char *args[], int *background)
{
    ssize_t length;
    int i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    // printf("%s<<",inputBuffer);

    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
            case ' ':
            case '\t' :               /* argument separators */
                if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if (start != -1){
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&') {
                    *background = 1;
                    inputBuffer[i-1] = '\0';
                    // args[ct] = NULL;
                    i++;
                }
        } /* end of switch */
    } /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */
}

// Parses path variable returns as a PATH_INFO struct pointer (Contains information about path variable)
PATH_INFO* parse_path() {
    int i=0;
    int max_path_var;
    char** path_values = NULL;
    char* path_str = getenv("PATH");
    char* token;

    max_path_var = MAX_PATH_VAR;
    path_values = (char**)malloc(max_path_var * sizeof(char*));

    // Split path variable with :, because they are concatenated with :
    while((token=strtok_r(path_str, ":", &path_str))) {

        path_values[i] = token;
        i++;

        if(i == max_path_var) {
            max_path_var = max_path_var * 2;
            path_values = (char **) realloc(path_values, max_path_var * sizeof(char *));
        }
    }
    // Create PATH_INFO struct pointer
    PATH_INFO* path_info = (PATH_INFO*)malloc(sizeof(PATH_INFO));
    path_info->paths = path_values;
    path_info->no_of_paths = i;
    // Then return it
    return path_info;
}

// Checks a command is executable in path variable
int check_path(char* args[], PATH_INFO* path_info, char** exec_path) {

    int i;
    int size = path_info->no_of_paths;

    for(i=0; i<size; i++) {
        char temp_path[128];
        strcpy(temp_path, path_info->paths[i]);
        strcat(temp_path, "/");
        strcat(temp_path, args[0]);
        temp_path[strlen(temp_path)] = '\0';

        int accessible = access(temp_path, F_OK | X_OK) ;
        // If accessible, initialize exec_path to the executable path
        if(accessible == 0) {
            *(exec_path) = strndup(temp_path, strlen(temp_path) + 1);
            // return (success)
            return 0;
        }
    }
    // If no executable found in path, return -1 (failure)
    return -1;
}

// Executes command which found in path
void exec_command(char* args[], char* exec_path, int* background) {

    // If it's a background process, set SIGCHLD's handler
    // It will be used if a background process ends, it'll be kicked from the background queue
    if(*background != 0)
        signal(SIGCHLD, handle_child);

    pid_t child_pid;
    child_pid = fork();

    // Fork error
    if(child_pid == -1) {
        fprintf(stderr, "Failed to fork.\n");
        return;
    }
    // child process
    if(child_pid == 0) {
        // execute command
        execl(exec_path, args[0], args[1], args[2], args[3], args[4], args[5], args[6],
              args[7], args[8], args[9], args[10], args[11], args[12], args[13],
              args[14], args[15], args[16], args[17], args[18], args[19], args[20],
              args[21], args[22], args[23], args[24], args[25], args[26], args[27],
              args[28], args[29], args[30], args[31], NULL);
        fprintf(stderr, "Error with running execl!\n");
        return;
    }
    // Parent process
    int status;
    // If process works on foreground
    if(*background == 0) {
        // Set foreground variables
        ANY_FOREGROUND_PROCESS = true;
        CURRENT_FOREGROUND_PROCESS = child_pid;
        // and wait until child process ends
        waitpid(child_pid, &status, 0);
        // After it ends, we have no foreground process currently
        ANY_FOREGROUND_PROCESS = false;
    }
    // If process works on background
    else {
        // Enqueue background process and print info
        enqueue(child_pid, getpgrp(), args[0]);
        printq();
    }
    *background = 0;
}

// Storing a new alias
void store_alias(char* args[], int arg_no) {
    int i;
    int aliased_arg_ctr = 0;
    int original_status = 0;
    size_t arg_len;
    char* alias = NULL;
    char** original_args;

    original_args = (char**)malloc(MAX_ARGS * sizeof(char*));
    // Iterate through all arguments
    for(i=1; i<arg_no; i++) {
        arg_len = strlen(args[i]);
        // starting of original arguments (will be aliased)
        if(original_status == 0) {
            // If starting character is "
            if (args[i][0] == '"') {
                char* starting_arg = NULL;
                // We are getting original argument right now
                original_status = 1;
                // If last character is " too, our original argument has the length 1
                if(args[i][arg_len-1] == '"') {
                    // If no alias given
                    if(args[i+1] == NULL) {
                        fprintf(stderr, "alias didn't given\n");
                        return;
                    }
                    // We get the command which has the length 1
                    starting_arg = (char *)malloc((arg_len - 1) * sizeof(char));
                    slice_string(&starting_arg, args[i], 1, (int)(arg_len - 1));
                    original_args[aliased_arg_ctr] = starting_arg;
                    aliased_arg_ctr++;
                    // Now we'll get alias of it
                    original_status = 0;
                    continue;
                }
                starting_arg = (char *)malloc(arg_len * sizeof(char));
                slice_string(&starting_arg, args[i], 1, (int)arg_len);
                original_args[aliased_arg_ctr] = starting_arg;
                aliased_arg_ctr++;
                continue;
            }
            // If argument is not between " ", that means it's an alias according to original_status variable = 0
            else {
                alias = strdup(args[i]);
                // If alias exists
                if(alias_exists(alias) == 0) {
                    fprintf(stderr, "alias %s exists, type \"alias -l\" to see it's corresponding command.\n", alias);
                    return;
                }
                // If it's not given
                if(alias == NULL) {
                    fprintf(stderr, "alias didn't given!\n");
                    return;
                }
                break;
            }
        }
        // getting other aliased arguments
        if(original_status == 1) {
            // end of original command
            // If no alias given
            if(args[i][arg_len-1] == '"') {
                if(args[i+1] == NULL) {
                    fprintf(stderr, "alias didn't given\n");
                    return;
                }
                original_status = 0;
                char* ending_arg = (char*)malloc(arg_len * sizeof(char));
                slice_string(&ending_arg, args[i], 0, (int)arg_len - 1);
                original_args[aliased_arg_ctr] = ending_arg;
            }
            // Get arguments between quotation marks
            else {
                char* temp_arg;
                temp_arg = strdup(args[i]);
                original_args[aliased_arg_ctr] = temp_arg;
            }
            aliased_arg_ctr++;
            continue;
        }
    }
    // Add a NULL to the end in order to check end of original arguments
    original_args[aliased_arg_ctr] = NULL;

    /* Linked list insertions */
    // If there are no aliases in this session, create the root
    if(alias_root == NULL) {
        alias_root = (ALIAS_NODE*)malloc(sizeof(ALIAS_NODE));
        alias_root->original_args = original_args;
        alias_root->alias = alias;
        alias_root->no_of_args = aliased_arg_ctr;
        alias_root->next = NULL;
    }
    // If there are some aliases in the list
    else {

        ALIAS_NODE* iter = alias_root;
        // Find a space to insert new alias
        while(iter->next != NULL)
            iter = iter->next;

        ALIAS_NODE* new_alias = (ALIAS_NODE*)malloc(sizeof(ALIAS_NODE));
        new_alias->original_args = original_args;
        new_alias->alias = alias;
        new_alias->no_of_args = aliased_arg_ctr;
        iter->next = new_alias;
        new_alias->next = NULL;
    }
    // Inform user
    printf("alias %s created for ", alias);
    int j;
    for(j=0; j<aliased_arg_ctr; j++)
        printf("%s ", original_args[j]);
    printf("\n");
}

// Remove alias from list
void remove_alias(char* alias) {
    // If alias is not found
    if(alias_exists(alias) < 0) {
        fprintf(stderr, "alias %s cannot found\n", alias);
        return;
    }

    ALIAS_NODE* removed = NULL;

    // first check root
    if(!strcmp(alias_root->alias, alias)) {
        // If it's found in the root, remove it
        removed = alias_root;
        alias_root = removed->next;
        removed->next = NULL;
        free(removed);
        printf("alias %s removed.\n", alias);
        return;
    }

    ALIAS_NODE* iter = alias_root;
    // Search alias in the remaining list
    while(iter->next != NULL) {
        if(!strcmp(iter->next->alias, alias))       // When it's found
            break;
        iter = iter->next;
    }
    // Remove alias
    removed = iter->next;
    iter->next = removed->next;
    removed->next = NULL;
    free(removed);
    // Inform user
    printf("alias %s removed.\n", alias);
}

// Printing alias list
void print_alias() {
    int ctr = 1;

    // If no alias
    if(alias_root == NULL) {
        fprintf(stderr, "no recorded alias found!\n");
        return;
    }

    printf("\t%-16s%s\n", "alias", "command");
    printf("\t%-16s%s\n", "------", "--------");

    // Print each alias and it's original command
    ALIAS_NODE* iter = alias_root;
    while(iter != NULL) {
        printf("[%d]\t%-16s", ctr++, iter->alias);
        int i = 0;
        while(iter->original_args[i] != NULL) {
            printf("%s ", iter->original_args[i]);
            i++;
        }
        printf("\n");
        iter = iter->next;
    }
}

// This checks existence of given alias
int alias_exists(char* alias) {
    ALIAS_NODE* iter = alias_root;
    while(iter != NULL) {
        // If exists, return 90
        if(!strcmp(iter->alias, alias))
            return 0;
        iter = iter->next;
    }
    // If the code reaches here, it means alias is not found, return -1
    return -1;
}

// This function counts arguments, initialize count parameter with count.
void get_args_count(char* args[], int* count) {
    int i = 0;
    while(args[i] != NULL) {
        *(count) = *(count) + 1;
        i++;
    }
}

// Enqueue function for background process queue
void enqueue(pid_t pid, pid_t group_pid, char* command) {
    // If no background processes, enqueue a background process with given parameters
    if(bg_process_queue == NULL) {
        bg_process_queue = (BACKGROUND_PROCESS_QUEUE*)malloc(sizeof(BACKGROUND_PROCESS_QUEUE));
        bg_process_queue->pid = pid;
        bg_process_queue->group_pid = group_pid;
        bg_process_queue->command = (char*)malloc(sizeof(char) * strlen(command));
        strcpy(bg_process_queue->command, command);
        bg_process_queue->next = NULL;
        return;
    }
    // If there are some background processes, add new background process to end of the queue with given parameters
    BACKGROUND_PROCESS_QUEUE* iter = bg_process_queue;
    while(iter->next != NULL)
        iter = iter->next;

    BACKGROUND_PROCESS_QUEUE* new_node = (BACKGROUND_PROCESS_QUEUE*)malloc(sizeof(BACKGROUND_PROCESS_QUEUE));

    new_node->pid = pid;
    new_node->group_pid = group_pid;
    new_node->command = (char*)malloc(sizeof(char) * strlen(command));
    strcpy(new_node->command, command);
    new_node->next = NULL;
    iter->next = new_node;
}

// Dequeue operation for background process queue
void dequeue(int* pid, int* group_pid) {
    // If there is no background process
    if(bg_process_queue == NULL) {
        fprintf(stderr, "There is no currently background process running.\n");
        return;
    }

    // initialize values that user wanted from function
    *pid = bg_process_queue->pid;
    *group_pid = bg_process_queue->group_pid;

    // remove background process from the queue
    BACKGROUND_PROCESS_QUEUE* deleted = bg_process_queue;
    bg_process_queue = deleted->next;
    deleted->next = NULL;
    free(deleted);
}

// Handler for ^Z signal, if this signal received, that means user wants to stop foreground process and its descendants
static void sigtstp_handler(int signum) {
    int status;
    // If there is a foreground process
    if(ANY_FOREGROUND_PROCESS) {
        // Check if this process still running
        kill(CURRENT_FOREGROUND_PROCESS, 0);
        // If not running, errno will set to ESRCH
        if(errno == ESRCH) {
            // Inform user
            fprintf(stderr, "\nprocess %d not found\n", CURRENT_FOREGROUND_PROCESS);
            // Set any foreground process to false
            ANY_FOREGROUND_PROCESS = false;
            printf("myshell: ");
            fflush(stdout);
        }
        // If foreground process is still running
        else {
            // Send a kill signal to it
            kill(CURRENT_FOREGROUND_PROCESS, SIGKILL);
            // Then wait for its group to terminate with option WNOHANG
            waitpid(-CURRENT_FOREGROUND_PROCESS, &status, WNOHANG);
            printf("\n");
            // If code reaches here, there is no foreground process remaining
            ANY_FOREGROUND_PROCESS = false;
        }
    }
    // If there is no background process, ignore the signal
    else {
        printf("\nmyshell: ");
        fflush(stdout);
    }
}

// Signal handler for terminated child
// This handler will be called if a child process terminates
void handle_child() {
    int status;
    union wait wst;
    pid_t pid;

    while(1) {
        // Get pid of terminating process
        pid = wait3(&status, WNOHANG, NULL);
        if (pid == 0)
            return;
        else if (pid == -1)
            return;
        // With child process pid
        else {
            // Kick that process from the background process queue because it's not running anymore
            // Search for terminated process in the queue
            // If it's found in the head of queue
            if(bg_process_queue != NULL && bg_process_queue->pid == pid) {
                // Remove process from the queue
                BACKGROUND_PROCESS_QUEUE *kicked = bg_process_queue;
                bg_process_queue = kicked->next;
                kicked->next = NULL;
                free(kicked);
                return;
            }
            // If it's not on the head of queue
            else if(bg_process_queue != NULL) {
                BACKGROUND_PROCESS_QUEUE* iter = bg_process_queue;
                // Find process in the queue
                while(iter->next != NULL) {
                    if(iter->next->pid == pid) {
                        // after finding, kick it from the queue
                        BACKGROUND_PROCESS_QUEUE *kicked = iter->next;
                        kicked->next = iter->next;
                        kicked->next = NULL;
                        free(kicked);
                        return;
                    }
                }
            }
        }
    }
}

// String slicing utility function
void slice_string(char** dest, const char* src, int from, int to) {
    int i;
    int ctr = 0;
    // Add characters placed from 'from' to 'to'    -     string[from:to]
    for(i=from; i<to; i++) {
        (*dest)[ctr] = src[i];
        ctr++;
    }
    // Add a NULL to the end of string
    (*dest)[ctr] = '\0';
}
