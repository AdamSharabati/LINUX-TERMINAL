#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>

//                                 BUGS ::::::  <>||<>   alias  null = ......,         alias ech = 'echo' =)(=
#define MAX_CMD_LENGTH 1025
#define MAX_ALIAS_LENGTH 1025
#define MAX_ARG_LENGTH 1025
#define MAX_CMD_ARGS_NUMBER 5
#define UPDATE_ALIAS_ARRAY 100

// --------------------------- Structures ------------------------------//
typedef struct {
    int success_cmd;
    int active_alias;
    int total_cmd;
    int source_Run;
} prompt;

typedef struct {
    char ali[MAX_ALIAS_LENGTH];
    char cmd[MAX_CMD_LENGTH];
} alias;

int countQuotes = 0;
int alias_update_counter = 0;
int savedWords[3] = {0,0, 0};  // {alias , source , unalias}
// --------------------------- Structures ------------------------------//

// --------------------------- function description ------------------------------//
int main();
void createPrompt(prompt* prom);
void printPrompt(prompt *prom);                     // prompt functions
char** CMD_TO_ARGS(const char* cmd, int* arg_count);
void cmd_run(char** Args, char* cmd, prompt* prom, alias** alias_array);
void update_alias_array(alias ***alias_array, prompt *prom);
void free_args(char** args);
int count_arguments(const char *command);
void printAliasArray(alias** alias_array, prompt *prom);
char** parse_alias_command(char* command);
void free_alias_result(char** result);
void add_alias(alias al, alias*** alias_array, prompt *prom);
int delete_alias(alias ***alias_array, char toRemove[], prompt *prom);
int checkQuotes(const char* cmd);
void runSource(const char* fileName, prompt *prom, alias*** alias_array);

// --------------------------- function description ------------------------------//

// =====================================================================================================================================================================

// ---------------------------  main  --------------------------------------------//
int main()
{
    prompt prom;
    createPrompt(&prom);
    printPrompt(&prom);

    char cmd[MAX_CMD_LENGTH];
    char **Args;

    alias** alias_array = NULL;
    update_alias_array(&alias_array, &prom);

    while (1) {

        fgets(cmd, MAX_CMD_LENGTH, stdin);
        int cmd_len = (int)strlen(cmd);
        cmd[cmd_len - 1] = '\0'; // Remove newline character at the end

        if (strcmp(cmd, "exit_shell") == 0) {
            printf("%d",countQuotes);
            break;
        }

        if (cmd_len >= MAX_CMD_LENGTH - 1 && cmd[cmd_len - 1] != '\0') {
            printf("ERR\n");
            printPrompt(&prom);
            continue;
        } // checks < MAX_CMD_LEN

        if (count_arguments(cmd) > MAX_CMD_ARGS_NUMBER) {
            printf("ERR\n");
            printPrompt(&prom);
            continue;
        }

        int CMD_ARGS_COUNT = 0;
        Args = CMD_TO_ARGS(cmd, &CMD_ARGS_COUNT);

        if (strcmp(Args[0], "alias") == 0 && !savedWords[0]) {
            if(CMD_ARGS_COUNT == 1)
            {
                prom.success_cmd++;
                printAliasArray(alias_array,&prom);
            }
            else
            {
                char** NEW_OLD_ALIAS = parse_alias_command(cmd);
                if(NEW_OLD_ALIAS == NULL)
                {
                    printf("ERR\n");
                    printPrompt(&prom);
                    free_args(Args);
                    continue;
                }
                else
                {
                    countQuotes++;
                    alias al;
                    strcpy(al.ali,NEW_OLD_ALIAS[1]);
                    strcpy(al.cmd,NEW_OLD_ALIAS[0]);
                    add_alias(al,&alias_array,&prom);
                    prom.success_cmd++;                                                     // alias command succeed
                    free_alias_result(NEW_OLD_ALIAS);
                    printPrompt(&prom);
                }
            }
        }
        else if(strcmp(Args[0], "unalias") == 0 && !savedWords[2])
        {
            if(CMD_ARGS_COUNT != 2)
            {
                printf("ERR\n");
                printPrompt(&prom);
                free_args(Args);
                continue;
            }
            int check1 = delete_alias(&alias_array,Args[1],&prom);
            if(check1)
            {
                prom.success_cmd++;
            }
            else
            {
                printf("ERR\n");
            }
            printPrompt(&prom);
            free_args(Args);
            continue;
        }
        else if (strcmp(Args[0], "source") == 0 && !savedWords[1]) {
            if(CMD_ARGS_COUNT != 2)
            {
                printf("ERR\n");
                printPrompt(&prom);
                free_args(Args);
                continue;
            }
            runSource(Args[1],&prom,&alias_array);
        }
        else {
            cmd_run(Args, cmd, &prom, alias_array);
        }
        // Free the dynamically allocated Args array in the main loop after use
        free_args(Args);
    }

    // Free alias array before exiting
    for (int i = 0; i < alias_update_counter * UPDATE_ALIAS_ARRAY; i++) {
        if (alias_array[i] != NULL) {
            free(alias_array[i]);
        }
    }
    free(alias_array);

    return 0;
}
// ---------------------------  main  --------------------------------------------//

// =====================================================================================================================================================================

// --------------------------- functions ------------------------------//
int has_sh_extension(const char *filename) {
    // Find the last occurrence of '.'
    const char *dot = strrchr(filename, '.');
    // Check if the extension is .sh
    return (dot && strcmp(dot, ".sh") == 0);
}

void runSource(const char* fileName, prompt *prom , alias*** alias_array)
{
    prom->source_Run = 1;
    if(!has_sh_extension(fileName))
    {
        printf("ERR\n");
        prom->source_Run = 0;
        printPrompt(prom);
        return;
    }

    FILE* file = fopen(fileName, "r");
    if (file == NULL) {
        printf("ERR\n");
        prom->source_Run = 0;
        printPrompt(prom);
        return;
    }

    char line[MAX_CMD_LENGTH] = "\0";
    char* check_BASH = fgets(line, sizeof(line), file);
    if (check_BASH == NULL || strcmp(line, "#!/bin/bash\n") != 0)
    {
        printf("ERR\n");
        fclose(file);
        prom->source_Run = 0;
        printPrompt(prom);
        return;
    }
    prom->success_cmd++;

    char **Args;
    while(fgets(line, sizeof(line), file))
    {
        prom->total_cmd++;
        // Trim leading and trailing whitespace
        char *trimmed_line = line;
        while (isspace((unsigned char)*trimmed_line)) trimmed_line++;
        char *end = trimmed_line + strlen(trimmed_line) - 1;
        while (end > trimmed_line && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';

        if (strlen(trimmed_line) == 0) {
            // Skip empty lines
            continue;
        }

        if (strcmp(trimmed_line, "exit_shell") == 0) {
            printf("%d", countQuotes);
            exit(0);
        }


        if (count_arguments(trimmed_line) > MAX_CMD_ARGS_NUMBER) {
            printf("ERR\n");
            continue;
        }

        int CMD_ARGS_COUNT = 0;
        Args = CMD_TO_ARGS(trimmed_line, &CMD_ARGS_COUNT);
        if(Args[0][0] == '#')
        {
            free_args(Args);
            continue;
        }

        if (strcmp(Args[0], "alias") == 0 && !savedWords[0]) {
            if (CMD_ARGS_COUNT == 1) {
                prom->success_cmd++;
                printAliasArray(*alias_array, prom);
            } else {
                char** NEW_OLD_ALIAS = parse_alias_command(trimmed_line);
                if (NEW_OLD_ALIAS == NULL) {
                    printf("ERR\n");
                    free_args(Args);
                    continue;
                } else {
                    countQuotes++;
                    alias al;
                    strcpy(al.ali, NEW_OLD_ALIAS[1]);
                    strcpy(al.cmd, NEW_OLD_ALIAS[0]);
                    add_alias(al, alias_array, prom);
                    prom->success_cmd++;  // alias command succeed
                    free_alias_result(NEW_OLD_ALIAS);
                }
            }
        } else if (strcmp(Args[0], "unalias") == 0) {
            if (CMD_ARGS_COUNT != 2) {
                printf("ERR\n");
                free_args(Args);
                continue;
            }
            int check1 = delete_alias(alias_array, Args[1], prom);
            if (check1) {
                prom->success_cmd++;
            } else {
                printf("ERR\n");
            }
            free_args(Args);
            continue;
        } else {
            cmd_run(Args, trimmed_line, prom, *alias_array);
        }

        // Free the dynamically allocated Args array after use
        free_args(Args);
        line[0] = '\0';
    }

    fclose(file);
    prom->source_Run = 0;
    printPrompt(prom);
}

void createPrompt(prompt* prom)  // Creates prompt
{
    prom->success_cmd = 0;
    prom->active_alias = 0;
    prom->total_cmd = 0;
    prom->source_Run = 0;
}

void printPrompt(prompt *prom) // prints prompt
{
    if(!prom->source_Run)
    {
        printf("#cmd:%d|#alias:%d|script lines:%d> ", prom->success_cmd, prom->active_alias, prom->total_cmd);
    }
}

int checkQuotes(const char* cmd) {
    if (cmd == NULL) {
        return 0;
    }

    while (*cmd) {
        if (*cmd == '"' || *cmd == '\'') {
            return 1;
        }
        cmd++;
    }

    return 0;
}

int count_arguments(const char *command) {
    int count = 0;
    const char *ptr = command;

    while (*ptr != '\0') {
        // Skip initial whitespace
        while (isspace((unsigned char)*ptr)) {
            ptr++;
        }
        if (*ptr == '\0') {
            break;
        }

        // Check if current character is a quote
        if (*ptr == '"' || *ptr == '\'') {
            char quote_char = *ptr++;
            while (*ptr != '\0' && *ptr != quote_char) {
                ptr++;
            }
            if (*ptr == quote_char) {
                ptr++;
            }
        } else {
            // Move to the next whitespace or end of string
            while (*ptr != '\0' && !isspace((unsigned char)*ptr)) {
                ptr++;
            }
        }

        count++;

        // Skip trailing whitespace
        while (isspace((unsigned char)*ptr)) {
            ptr++;
        }
    }

    return count;
}

char **CMD_TO_ARGS(const char *cmd, int *arg_count) {
    char **args = malloc((MAX_CMD_ARGS_NUMBER + 1) * sizeof(char *));
    char *buffer = malloc((strlen(cmd) + 1) * sizeof(char));
    int in_quotes = 0;
    char quote_char = '\0';
    int i, j;
    int count = 0;

    // Initialize all args to NULL
    for (i = 0; i < MAX_CMD_ARGS_NUMBER + 1; i++) {
        args[i] = NULL;
    }

    // Copy the input command to a buffer we can modify
    strcpy(buffer, cmd);

    // Check if the command is empty or contains only spaces
    int empty = 1;
    for (i = 0; buffer[i] != '\0'; i++) {
        if (!isspace(buffer[i])) {
            empty = 0;
            break;
        }
    }
    if (empty) {
        args[0] = malloc(2 * sizeof(char));
        strcpy(args[0], "\n");
        args[1] = NULL;
        *arg_count = 1;
        free(buffer);
        return args;
    }

    // Parse the command string
    i = 0;
    while (buffer[i] != '\0' && count < MAX_CMD_ARGS_NUMBER) {
        // Skip leading spaces
        while (isspace(buffer[i])) i++;

        if (buffer[i] == '\0') break;

        // Check for quotes
        if (buffer[i] == '\"' || buffer[i] == '\'') {
            in_quotes = 1;
            quote_char = buffer[i];
            i++;
        } else {
            in_quotes = 0;
            quote_char = '\0';
        }

        // Allocate memory for the argument
        args[count] = malloc(MAX_ARG_LENGTH * sizeof(char));
        j = 0;

        // Extract the argument
        while (buffer[i] != '\0' && (in_quotes || !isspace(buffer[i]))) {
            if (buffer[i] == quote_char && in_quotes) {
                in_quotes = 0;
                i++;
                break;
            }
            args[count][j++] = buffer[i++];
        }
        args[count][j] = '\0';
        count++;

        // Skip trailing spaces
        while (isspace(buffer[i])) i++;
    }

    *arg_count = count;

    // Free the temporary buffer
    free(buffer);

    return args;
}


void free_args(char** args) {
    if (args == NULL) return;
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

char* combine_strings(const char* string1, const char* string2) {
    // Skip any leading spaces in string2
    while (*string2 && isspace(*string2)) {
        string2++;
    }

    // Find the first word boundary in string2
    const char* first_word_end = string2;
    while (*first_word_end && !isspace(*first_word_end)) {
        first_word_end++;
    }

    // Skip any additional spaces to reach the second word
    const char* second_word = first_word_end;
    while (*second_word && isspace(*second_word)) {
        second_word++;
    }

    // Check if there is only one word in string2
    if (*second_word == '\0') {
        // Return a copy of string1 since string2 has only one word
        char* result = (char*)malloc((strlen(string1) + 1) * sizeof(char));
        if (result == NULL) {
            perror("malloc");
            exit(1);
        }
        strcpy(result, string1);
        return result;
    }

    // Calculate the length of the new string
    size_t length1 = strlen(string1);
    size_t length2 = strlen(second_word);
    size_t new_length = length1 + length2 + 2; // +1 for the space and +1 for the null terminator

    // Allocate memory for the new string
    char* result = (char*)malloc(new_length * sizeof(char));
    if (result == NULL) {
        perror("malloc");
        exit(1);
    }

    // Copy the first string to the result
    strcpy(result, string1);

    // Add the space
    result[length1] = ' ';

    // Copy the second word of the second string after the space
    strcpy(result + length1 + 1, second_word);

    return result;
}



void cmd_run(char** Args, char* cmd, prompt* prom, alias** alias_array)
{
    int check_combine = 0;
    if(Args == NULL)
    {
        return;
    }
    for (int i = 0; i < prom->active_alias; i++) {
        if (strcmp(Args[0], alias_array[i]->ali) == 0) {
            char* newCMD = combine_strings(alias_array[i] -> cmd,cmd);
            check_combine = 1;
            int arg_count = 0;
            Args = CMD_TO_ARGS(newCMD, &arg_count);
            cmd = newCMD;
            break;
        }
    }


    if(check_combine && count_arguments(cmd) > MAX_CMD_ARGS_NUMBER)
    {
        printf("ERR\n");
        free_args(Args);
        free(cmd);
        printPrompt(prom);
        return;
    }

    int Check_Son = fork();
    if (Check_Son > 0) {
        int status;
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            prom->success_cmd += 1;
            if(checkQuotes(cmd) == 1)
            {
                countQuotes++;
            }
        }
        printPrompt(prom);
    }
    else if(Check_Son < 0)
    {
        perror("fork");
        _exit(1);
    }
    else {

        execvp(Args[0], Args);
        perror("execvp error");
        free_args(Args); // Free Args before exiting
        _exit(1);
    }
    if(check_combine == 1)
    {
        free(cmd);
    }

}

void update_alias_array(alias ***alias_array, prompt *prom) {
    // Check if the alias_array is NULL and initialize it if necessary
    if (*alias_array == NULL) {
        alias_update_counter = 1;
        *alias_array = (alias **) malloc(UPDATE_ALIAS_ARRAY * sizeof(alias *));
        if (*alias_array == NULL) {
            perror("malloc");
            exit(1);
        }
        for (int i = 0; i < UPDATE_ALIAS_ARRAY; i++) {
            (*alias_array)[i] = NULL;  // Initialize all pointers to NULL
        }
    } else if (prom->active_alias == alias_update_counter * UPDATE_ALIAS_ARRAY) {
        // Increment the counter to allocate the next block
        alias_update_counter++;
        alias **temp = (alias **) realloc(*alias_array, alias_update_counter * UPDATE_ALIAS_ARRAY * sizeof(alias *));
        if (temp == NULL) {
            printf("ERR");
            exit(1);
        }
        *alias_array = temp;
        // Initialize the new block of alias pointers to NULL
        for (int i = (alias_update_counter - 1) * UPDATE_ALIAS_ARRAY; i < alias_update_counter * UPDATE_ALIAS_ARRAY; i++) {
            (*alias_array)[i] = NULL;  // Initialize all pointers to NULL
        }
    }
}

void add_alias(alias al, alias*** alias_array, prompt *prom)  // {l1 : cmd1, l2 : cmd2 }
{
    for(int i = 0; i<prom->active_alias; i++)
    {
        if(strcmp(al.ali,((*(alias_array))[i])->ali) == 0)
        {
            strcpy((*alias_array)[i]->cmd,al.cmd);
            return;
        }
    }
    update_alias_array(alias_array,prom);
    alias *new_alias = (alias *)malloc(sizeof(alias));
    if (new_alias == NULL) {
        perror("malloc");
        exit(1);
    }
    strcpy(new_alias->ali, al.ali);
    strcpy(new_alias->cmd, al.cmd);
    (*alias_array)[prom->active_alias] = new_alias;
    if(strcmp((*alias_array)[prom->active_alias] -> ali,"alias") == 0)
    {
        savedWords[0] = 1;
    }
    else if(strcmp((*alias_array)[prom->active_alias] -> ali,"source") == 0)
    {
        savedWords[1] = 1;
    }
    else if(strcmp((*alias_array)[prom->active_alias] -> ali,"unalias") == 0)
    {
        savedWords[2] = 1;
    }
    prom->active_alias++;
}

char** parse_alias_command(char* command) {
    char *temp = (char*)malloc(MAX_CMD_LENGTH * sizeof(char));
    if (temp == NULL)
    {
        perror("malloc");
        exit(1);  // Exit or handle the error appropriately
    }

    int i = 0;
// Skip leading spaces
    while (i < MAX_CMD_LENGTH && command[i] == ' ')
    {
        i++;
    }

    int j = 0;
// Copy the rest of the command string
    while (j + i < MAX_CMD_LENGTH && command[j + i] != '\0')
    {
        temp[j] = command[j + i];
        j++;
    }
    temp[j] = '\0'; // Null-terminate the new string

// Free the original command and reassign
    command = temp;

    // Validate command length
    if (strlen(command) >= MAX_CMD_LENGTH) {
        free(temp);
        return NULL;
    }

    // Find "alias " at the beginning
    if (strncmp(command, "alias ", 6) != 0) {
        free(temp);
        return NULL;
    }

    // Find the position of the '=' character
    const char* equals_pos = strchr(command, '=');
    if (equals_pos == NULL) {
        free(temp);
        return NULL;
    }

    // Extract newName
    const char* newName_start = command + 6;
    while (isspace(*newName_start)) {
        newName_start++;
    }
    const char* newName_end = equals_pos - 1;
    while (isspace(*newName_end)) {
        newName_end--;
    }

    // Check if newName is valid
    if (newName_start > newName_end) {
        free(temp);
        return NULL;
    }

    // Check for the single quotes around oldName
    const char* oldName_start = equals_pos + 1;
    while (isspace(*oldName_start)) {
        oldName_start++;
    }
    if (*oldName_start != '\'') {
        free(temp);
        return NULL;
    }
    oldName_start++;
    const char* oldName_end = strrchr(oldName_start, '\'');
    if (oldName_end == NULL) {
        free(temp);
        return NULL;
    }

    // Calculate lengths
    size_t newName_len = newName_end - newName_start + 1;
    size_t oldName_len = oldName_end - oldName_start;

    // Allocate memory for the result
    char** result = (char**)malloc(2 * sizeof(char*));
    if (result == NULL) {
        free(temp);
        return NULL;
    }
    result[0] = (char*)malloc(MAX_ALIAS_LENGTH + 1);
    result[1] = (char*)malloc(MAX_ALIAS_LENGTH + 1);

    // Copy names to the result
    strncpy(result[0], oldName_start, oldName_len);
    result[0][oldName_len] = '\0';
    strncpy(result[1], newName_start, newName_len);
    result[1][newName_len] = '\0';
    free(temp);

    return result;
}

int delete_alias(alias ***alias_array, char toRemove[], prompt *prom)
{
    if(strcmp(toRemove,"alias") == 0)
    {
        savedWords[0] = 0;
    }
    else if(strcmp(toRemove,"source") == 0)
    {
        savedWords[1] = 0;
    }
    else if(strcmp(toRemove,"unaliass") == 0)
    {
        savedWords[2] = 0;
    }
    int deleted = 0;
    for(int i = 0; i<prom->active_alias; i++)
    {
        if(strcmp((*alias_array)[i]->ali,toRemove) == 0)
        {
            free((*alias_array)[i]);
            (*alias_array)[i] = NULL;
            for(int j = i; j<prom->active_alias; j++)
            {
                (*alias_array)[j] = (*alias_array)[j + 1];
            }
            (*alias_array)[prom->active_alias] = NULL;
            prom->active_alias--;
            deleted = 1;
            break;
        }
    }
    return deleted;
}

void free_alias_result(char** result) {
    if (result) {
        free(result[0]);
        free(result[1]);
        free(result);
    }
}

void remove_extra_spaces(char *str) {
    if (str == NULL) return;

    char *read = str;  // Pointer for reading the original string
    char *write = str; // Pointer for writing to the new position
    int in_word = 0;   // Flag to check if we are inside a word

    while (*read != '\0') {
        if (!isspace(*read)) {
            if (write != str && !in_word) {
                *write++ = ' ';  // Write a single space before the new word
            }
            *write++ = *read;
            in_word = 1;  // We are inside a word
        } else {
            in_word = 0;  // We are not inside a word
        }
        read++;
    }

    *write = '\0';  // Null-terminate the result string
}

void printAliasArray(alias** alias_array, prompt *prom)
{
    for(int i = 0; i<prom->active_alias;i++)
    {
        remove_extra_spaces(alias_array[i]->ali);
        remove_extra_spaces(alias_array[i]->cmd);
        printf("%s=%s \t",alias_array[i]->ali,alias_array[i]->cmd);
    }
    printf("\n");
    printPrompt(prom);
}
