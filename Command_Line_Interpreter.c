//assumptions:
//at most 10 command line arguments for each job
//each command line argument has less than 19 characters
//at most 10 chained jobs in a command
//if the command path is not provided, assume that the command is located in /bin

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

char instruction[2500];
struct stat var; 
void simpleCommandExec();
void multipleCommandExec();

char **commStr;
char **readTokens(int maxTokenNum, int maxTokenSize, int *readTokenNum, char *buffer);
void freeTokenArray(char **strArr, int size);

int main() {
    int p = 1;
    while (p) {
        printf("PROMPT> ");

        //store the entire instruction in the character array- instruction[]
        fgets(instruction, sizeof instruction, stdin);
        instruction[strcspn(instruction, "\n")] = 0; //indicate end of string

        //if user wants to quit
        if (strcmp(instruction, "quit") == 0) {
            printf("Goodbye! \n");
            p = 0;
            exit(0);
        }

        //extract all the commands and store in commStr
        char *command;
        command = strtok(instruction, "|\n"); //first command
        int numOfCommands = 0;
        commStr = (char **)malloc(sizeof(char *) * 11); //allocate memory

        //numOfCommands will have number of commands, commStr is an array of all commands
        while (numOfCommands < 11 && (command != NULL)) {
            commStr[numOfCommands] = (char *)malloc(sizeof(char *) * 209);
            strncpy(commStr[numOfCommands], command, 208);
            commStr[numOfCommands][209] = '\0'; //indicate end of string
            command = strtok(NULL, "|\n");
            numOfCommands++;
        }

        char input[400];
        strcpy(input, commStr[0]); //input contains the first command
        int *numOfTokens = (int *)malloc(sizeof(int *));
        char **args;
        args = readTokens(11, 20, numOfTokens, input); //read the tokens/arguments in the first command into args

        //support for setting, using and unsetting of environment variables
        if (strcmp(args[0], "set") == 0) {
            if (setenv(args[1], args[2], 1) < 0) {
                printf("error in setting\n");
            }
        }
        else if (strcmp(args[0], "unset") == 0) {
            if (unsetenv(args[1]) < 0) {
                printf("error in unsetting\n");
            }
        }
        
        
        else {
            if (numOfCommands == 1) {
                simpleCommandExec();
            }
            else {
                multipleCommandExec(numOfCommands);
            }
        }

        freeTokenArray(args, sizeof(args));
    }

    return 0;
}

void simpleCommandExec() {
    char **argStr;
    char input[120];
    strcpy(input, commStr[0]);
    int *numOfTokens = (int *)malloc(sizeof(int *));
    argStr = (char **)malloc(sizeof(char *) * 10); //allocate memory
    argStr = readTokens(11, 20, numOfTokens, input);
    //argStr contains all the tokens

    if (*argStr[0] != '/') {
        char *s = "/bin/";
        char *dup;
        sprintf(argStr[0], "%s%s", s, (dup = strdup(argStr[0])));
        free(dup);
    }

    int child;
    int progStatus = stat(argStr[0], &var); //to check if it is a valid command
    if (progStatus != 0) {
        printf("%s not found \n", argStr[0]);
    }

    else {
        int result = fork();
        if (result != 0) {
            waitpid(result, NULL, 0);
        }
        else {
            execvp(argStr[0], argStr);
        }
    }
}

void multipleCommandExec(int numOfCommands) {

    int pipefd[numOfCommands - 1][2]; //array of pipes (number of pipes needed is always one less than no: of commands)
    int status = 0;
    int p; // for child processes
    char input[120];
    int *numOfTokens = (int *)malloc(sizeof(int *));
    char ***arguments = (char ***)malloc(sizeof(char *) * 20 * numOfCommands);

    for (int i = 0; i < numOfCommands; ++i) {
        strcpy(input, commStr[i]);
        arguments[i] = readTokens(11, 20, numOfTokens, input);
    }

    for (int i = 0; i < numOfCommands; ++i) {
        if (i != numOfCommands - 1) { //so that no unneccessary last pipe is created
            pipe(pipefd[i]);
        }
        if (i == 0) { 
            p = fork();
            if (p < 0) {
                printf("fork failed\n");
                exit(-1);
            }
            if (p == 0) {
                close(pipefd[0][0]); //close read end
                if (dup2(pipefd[0][1], STDOUT_FILENO)) { //write end- STDOUT
                    printf("unsuccessful dup");
                }
                close(pipefd[0][1]);  //close write end
                execvp(arguments[0][0], arguments[0]); //write data into pipefd[0][1] instead of console
            }
            else { //parent executing
                close(pipefd[0][1]);
                while (wait(&status) != -1)
                    ;
            }
        }
        else if (i == numOfCommands - 1) { //last command
            p = fork();
            if (p < 0) {
                printf("fork failed");
                exit(-1);
            }
            if (p == 0) {  //child process
                close(pipefd[i - 1][1]); //close write end
                if (dup2(pipefd[i - 1][0], STDIN_FILENO) < 0) { //link to stdin
                    printf("unsuccessful dup");
                }
                close(pipefd[i - 1][0]); //close read end
                execvp(arguments[i][0], arguments[i]); //read data from pipefd[i-1][0] instead of console
            }
            else { //parent must wait
                close(pipefd[i - 1][1]);
                close(pipefd[i - 1][0]);
                while (wait(&status) != -1);
            }
        }
        else
        { //middle commands
            p = fork();
            if (p < 0) {
                printf("fork failed");
                exit(-1);
            }
            if (p == 0) {
                close(pipefd[i - 1][1]);
                close(pipefd[i][0]);
                if (dup2(pipefd[i][1], STDOUT_FILENO) < 0) { //op to its write end
                    printf("dup unsuccessful\n");
                }
                if (dup2(pipefd[i - 1][0], STDIN_FILENO) < 0) { //to read from read end
                    printf("unsuccessful dup\n");
                }
                close(pipefd[i - 1][0]);
                close(pipefd[i][1]);
                execvp(arguments[i][0], arguments[i]); //use pipefd[i][1] as input and pipefd[i-1][0] as output
            }
            else {
                close(pipefd[i][1]);
                while (wait(&status) != -1);
            }
        }
    }
    freeTokenArray(*arguments, sizeof(arguments));
}

char **readTokens(int maxTokenNum, int maxTokenSize, int *readTokenNum, char *buffer) {
    char **tokenStrArr;
    char *token;
    int i;

    //allocate token array, each element is a char*
    tokenStrArr = (char **)malloc(sizeof(char *) * maxTokenNum);

    //Nullify all entries
    for (int i = 0; i < maxTokenNum; i++) {
        tokenStrArr[i] = NULL;
    }

    token = strtok(buffer, " \n");
    char *temp = (char *)malloc(sizeof(char *) * maxTokenSize);

    i = 0;
    while (i < maxTokenNum && (token != NULL)) {
        //Allocate space for token string
        tokenStrArr[i] = (char *)malloc(sizeof(char *) * maxTokenSize);
        if (token[0] == '$') {

            int n = strlen(token);
            for (int j = 0; j < n; j++) {
                temp[j] = token[j + 1];
            }

            if (strcmp(tokenStrArr[0], "unset") == 0) {
                strcpy(token, temp);
            }
            else {
                token = getenv(temp);
            }
        }
        //Ensure at most 19 + null characters are copied
        if (token != NULL) {
            strncpy(tokenStrArr[i], token, maxTokenSize - 1);

            //Add NULL terminator in the worst case
            tokenStrArr[i][maxTokenSize - 1] = '\0';
        }
        i++;
        token = strtok(NULL, " \n");
    }

    *readTokenNum = i;

    return tokenStrArr;
}

void freeTokenArray(char **tokenStrArr, int size) {
    int i = 0;

    //Free every string stored in tokenStrArr
    for (i = 0; i < size; i++) {
        if (tokenStrArr[i] != NULL) {
            free(tokenStrArr[i]);
            tokenStrArr[i] = NULL;
        }
    }
    //Free entire tokenStrArr
    free(tokenStrArr);
}
