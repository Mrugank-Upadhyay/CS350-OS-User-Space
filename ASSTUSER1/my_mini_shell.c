#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>

// char **parseCommand(char *input)
// {
//     parseCommand->initialCommand = input;
//     char *tmp = input;
//     char *inputStr = tmp;
//     char *token;
//     // parseCommand->parsedCommand[sizeof(char *)];
//     int index = 0;
//     while ((token = strsep(&inputStr, " ")))
//     {
//         if (strcmp(token, "") == 0)
//         {
//             break;
//         }
//         parseCommand->parsedCommand[index] = token;
//         index++;
//     }
//     parseCommand->len = index;
//     return parseCommand;
// }

int parseCommand(char *input, char *command[])
{
    char *token;
    char *tmp = input;
    char *inputStr = tmp;
    int index = 0;
    int retFlag;
    bool redirectFlag = false;
    bool pipeFlag = false;

    while ((token = strsep(&inputStr, " ")))
    {
        if (strcmp(token, "") == 0)
        {
            break;
        }

        if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0)
        {
            redirectFlag = true;
        }

        if (strcmp(token, "|") == 0)
        {
            pipeFlag = true;
        }
        command[index] = token;
        index++;
    }

    if (redirectFlag == true && pipeFlag == true)
    {
        return 3;
    }
    else if (redirectFlag == true)
    {
        return 2;
    }
    else if (pipeFlag == true)
    {
        return 1;
    }
    return 0;
}

void parseInput(char *input, char *subCommands[], int count)
{
    // struct Command *parsedCommand;
    // parsedCommand->initialCommand = input;

    /// MUST REMOVE LEADING SPACE AFTER PIPES
    if (count > 1)
    {
        int subCommandsIndex = 0;
        char *inputStr = input;
        char *tofree = inputStr;
        char *token;

        while ((token = strsep(&inputStr, "|")))
        {
            // POTENTIAL MEMORY LEAK
            if (' ' == *token)
            {
                token++;
            }

            subCommands[subCommandsIndex] = token;
            subCommandsIndex++;
            if (subCommandsIndex >= count)
            {
                break;
            }
        }
    }
    else
    {
        subCommands[0] = input;
    }
}

void doexec(pid_t pid, char *commandList[], bool redirectFlag, bool pipeFlag)
{
    if (redirectFlag)
    {
        char *inFile = NULL;
        char *outFile = NULL;
        int index = 0;
        while (commandList[index])
        {
            if (strcmp(commandList[index], ">") == 0)
            {
                char *tmp = commandList[index + 1];
                outFile = tmp;
                while (commandList[index])
                {
                    commandList[index] = NULL;
                    index++;
                }
                break;
            }
            else if (strcmp(commandList[index], "<") == 0)
            {
                char *tmp = commandList[index + 1];
                inFile = tmp;
                while (commandList[index])
                {
                    commandList[index] = NULL;
                    index++;
                }
                break;
            }
            index++;
        }
        int fd;
        if (inFile)
        {
            if ((fd = open(inFile, O_RDONLY)) < 0)
            {
                perror(inFile);
                exit(1);
            }
            if (fd != 0)
            {
                dup2(fd, 0);
                close(fd);
            }
        }

        else if (outFile)
        {
            if ((fd = open(outFile, O_WRONLY)) < 0)
            {
                perror(inFile);
                exit(1);
            }
            if (fd != 1)
            {
                dup2(fd, 1);
                close(fd);
            }
        }
    }
    execvp(commandList[0], commandList);
    perror(commandList[0]);
    exit(1);
}

char *runShell(char *command[], char *output, bool redirectFlag, bool pipeFlag)
{
    char *outputStr;
    pid_t pid;
    switch (pid = fork())
    {
    case -1:
        perror("Fork");
        break;
    case 0:
        doexec(pid, command, redirectFlag, pipeFlag);
        break;
    default:
        waitpid(pid, NULL, 0);
    }
    return "";
}

int main(int argc, char **argv)
{

    char *input;
    size_t len = 0;
    __ssize_t lineSize = 0;
    while (1)
    {
        printf("$ ");
        lineSize = getline(&input, &len, stdin);
        // remove trailing newline
        input[strcspn(input, "\n")] = 0;

        // quit shell
        if (strcmp(input, "q") == 0)
        {
            break;
        }

        if (lineSize == -1)
        {
            break;
        }

        // If the user only hits enter, we continue
        if (strcmp(input, "") == 0)
        {
            continue;
        }

        // Count pipes
        // char *tmp = input;
        // char *lastPipe = 0;
        int count = 0;

        // Count number of pipes

        // while (*tmp) {
        //     if ('|' == *tmp) {
        //         count++;
        //         lastPipe = tmp;
        //     }
        //     tmp++;
        // }
        // For the final command after the last pipe symbol
        count++;

        // char *commandList[count];
        // parseInput(input, commandList, count);
        char *command[100] = {NULL};

        char *output;

        if (count == 1)
        {
            int flags = parseCommand(input, command);
            switch (flags)
            {
            case 0:
                output = runShell(command, "", false, false);
                break;
            case 1:
                output = runShell(command, "", false, true);
                break;
            case 2:
                output = runShell(command, "", true, false);
                break;
            case 3:
                output = runShell(command, "", true, true);
            }

            // printf("%s\n", output);
        }
        // else
        // {
        //     output = "";
        //     for (int i = 0; i < count; i++)
        //     {
        //         parseCommand(commandList);
        //         output = runShell(command, output);
        //     }
        //     // printf("%s\n", output);
        // }
    }

    return 0;
}
