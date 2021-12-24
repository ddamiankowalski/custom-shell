// Compilation command: gcc -o microshell microshell.c -lreadline

#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>

char *command_generator(const char*, int);
char **sysf_completion(const char*, int, int);
char **bin_dirinit();

char **bin_dirinit()
{
    char **directories = malloc(sizeof(char**) * 1024);
    char *env = getenv("PATH");
    int i = 0, j = 0, k = 0, state = 1;

    while(state)
    {
        char *path = malloc(sizeof(char) * 1024);
        while(env[i] != ':' && env[i] != '\0')
            path[j++] = env[i++];

        state = (env[i] == '\0') ? 0 : 1;
        path[j] = '\0';
        directories[k] = path;
        j = 0;
        k++;
        i++;
    }
    directories[k] = NULL;
    return directories;
}

DIR *sys_dir;
struct dirent *entry;

const char *flist[] = {
    "microdiff",
    "microgrep",
    "cd",
    "help",
    "exit",
    "END"
};

char *command_generator(const char *text, int state)
{
    static int di = 0, len, builtin;
    char *name;
    char **dirs = bin_dirinit();

    if(!state)
    {
        builtin = 0;
        di = 0;
        len = strlen(text);
        if(!len)
            return (char*)NULL;
    }

    while(strcmp(flist[builtin], "END") != 0)
    {
        if(strncmp(text, flist[builtin], len) == 0)
            return strdup(flist[builtin++]);
        builtin++;
    }

    while(dirs[di] != NULL)
    {
        if(sys_dir != NULL)
            closedir(sys_dir);
        sys_dir = opendir(dirs[di++]);
        if(sys_dir == NULL)
        {
            closedir(sys_dir);
            continue;
        }
        while((entry = readdir(sys_dir)) != NULL)
        {
            name = entry->d_name;
            if(strncmp(text, name, len) == 0)
                return strdup(name);
        }
    }
    return (char*)NULL;
}

char **sysf_completion(const char *text, int start, int end)
{
    char **list;
    list = (char **)NULL;
    if(start == 0)
        list = rl_completion_matches(text, command_generator);
    closedir(sys_dir);
    return list;
}

char *getcommand();
char **getarg(char *line);
int execute(char **arguments);

void loop()
{
    rl_attempted_completion_function = sysf_completion;
    static char *command;
    static char **arg;
    int status;
    do {
        command = getcommand();
        arg = getarg(command);
        status = execute(arg);
    } while(status);
}

int main(int argc, char *argv[])
{
    loop();

    exit(EXIT_SUCCESS);
}

char *getcommand()
{
    char usrname[1024];
    getlogin_r(usrname, 1024);
    char hostname[1024];
    gethostname(hostname, 1024);
    char cwd[PATH_MAX], *command, *prompt;
    getcwd(cwd, sizeof(cwd));
    char *affix[] = {
        usrname,
        "@",
        hostname,
        ":[{",
        cwd,
        "}] $ "
    };

    if(cwd == NULL)
    {
        perror("MSH_ERR (getcwd)");
        exit(EXIT_FAILURE);
    }
    else if((prompt = calloc(PATH_MAX, sizeof(char))) == NULL)
    {
        perror("MSH_ERR (calloc)");
        exit(EXIT_FAILURE);
    }
    else
    {
        for(int i = 0; i < 6; i++)
            strcat(prompt, affix[i]);
        if((command = readline(prompt)) != NULL && *command)
            add_history(command);
    }
    free(prompt);
    return command;
}

char **getarg(char *line)
{
    int i = 0, j = 0, k = 0;
    int letter = 0, word = 0, currargs = 64, currwords = 1024;
    char **arguments = calloc(64, sizeof(char *));
    char *path = getenv("HOME");
    char *temp = malloc(sizeof(char) * PATH_MAX);

    while(line[i] != '\0')
    {
        if(line[i] == '~')
        {
            while(path[k] != '\0')
                temp[j++] = path[k++];
            k = 0;
        }
        else
            temp[j++] = line[i];
        i++;
    }
    i = 0;
    temp[j] = '\0';
    line = temp;

    while(line[i] != '\0')
    {
        if(isspace(line[i]) != 0)
            i++;
        else
        {
            char *arg = malloc(sizeof(char) * 1024);
            for(letter = 0; isspace(line[i]) == 0 && line[i] != '\0'; letter++)
                arg[letter] = line[i++];
            arg[letter] = '\0';
            arguments[word++] = arg;
        }
    }
    arguments[word] = NULL;
    return arguments;
}

int m_help(char **args);
int m_exit(char **args);
int m_cd(char **args);
int m_grep(char **args);
int m_diff(char **args);

int (*pfuncarray[])(char **) = {
    &m_diff,
    &m_grep,
    &m_cd,
    &m_help,
    &m_exit
};

int execute (char **arguments)
{
    if(arguments[0] == NULL)
        return 1;

    int i = 0;
    while(strcmp(flist[i], "END") != 0)
    {
        if(strcmp(arguments[0], flist[i]) == 0)
            return (*pfuncarray[i])(arguments);
        i++;
    }

    pid_t id = fork();
    if(id == 0)
    {
        int status;
        if(status = execvp(arguments[0], arguments) == -1)
            perror("MSH_ERR: execvp");
        exit(EXIT_SUCCESS);
    }
    else
    {
        wait(NULL);
    }
    return 1;
}

int m_help(char **args)
{
    printf("Microshell - Damian Kowalski\n\n");
    printf("- Shell wyświetla znak zachęty w postaci [{path}], gdzie path jest ścieżką do bieżącego katalogu roboczego\n");
    printf("- Shell obsługuje również polecenie exit, które kończy działanie programu powłoki\n");
    printf("- Shell obsługuje polecenie help, gdzie opisane są wszystkie wymagane funkcjonalności\n\n");
    printf("WYBRANE POLECENIA POWŁOKI:\n");
    printf("- microgrep [plik tekstowy] [klucz] komenda sprawdza ile razy w danym pliku tekstowym znajduje się klucz\n");
    printf("- microdiff [plik tekstowy] [plik tekstowy] komenda porównuje dwa pliki tekstowe i zwraca ewentualne różnice\n\n");
    printf("Microshell umożliwia uruchamianie programów znajdujących się w katalogu opisanym przez ścieżkę środowiskową $PATH\n");
    printf("Jednocześnie microshell posługuje się funkcjami fork() oraz execvp() przy uruchamianiu skryptów, umożliwiając wywołanie z argumentami\n");
    printf("Microshell wypisuje błędy w przypadku, gdy polecenie wpisane przez użytkownika jest niepoprawne\n\n");
    printf("DODATKOWE BAJERY:\n");
    printf("- obsługa historii za pomocą strzałek\n");
    printf("- uzupełnianie składni\n");
    printf("- wyświetlanie loginu aktualnie zalogowanego użytkownika\n");

    return 1;
}

int m_exit(char **args)
{
    return 0;
}

int m_cd(char **args)
{
    static int status = 0;
    static char *temp, oldpath[PATH_MAX];
    temp = getcwd(temp, sizeof(char) * PATH_MAX);
    char *line = getenv("HOME");
    if(args[2] != NULL)
        fprintf(stderr, "MSH_ERR: cd: Too many arguments. Type \"help -cd\" for more information.\n");
    else if(args[1] == NULL || (strcmp(args[1], "/") == 0))
    {
        if(chdir((const char*)line) == -1)
            perror("MSH_ERROR, cd()");
        status = 1;
    }
    else if(strcmp(args[1], "-") == 0)
    {
        if(status == 0)
            fprintf(stderr, "MSH_ERROR: cd(): OLDPATH is not set\n");
        else if(chdir((const char*)oldpath) == -1)
            perror("MSH_ERROR: cd()");
    }
    else
    {
        if(chdir((const char*)args[1]) == -1)
            perror("MSH_ERROR: cd()");
        status = 1;
    }
    strcpy(oldpath, temp);
    return 1;
}

int m_grep(char **args)
{
    if(args[3] != NULL)
        fprintf(stderr, "MSH_ERR: microgrep: Too many arguments. See \"help -microgrep\" for more information\n");
    else if(args[1] == NULL)
        fprintf(stderr, "MSH_ERR: microgrep: Insufficient argument. See \"help -microgrep\" for more information\n");
    else
    {
        int linecounter = 0, occurence = 0, i = 0;
        char line[100];
        FILE *read;
        read = fopen(args[1], "r");
        if(read == NULL)
        {
            perror("MSH_ERROR: microgrep");
            return 1;
        }
        while(fgets(line, 100, read) != NULL)
        {
            int i = 0;
            while(line[i] != '\n')
                i++;
            if(line[i] == '\n')
                line[i] = '\0';
            linecounter++;
            if(strstr(line, args[2]) == NULL)
                continue;
            else
                occurence++;
                printf("Line number: %d, the occurence: %d - \"%s\"\n", linecounter, occurence, line);
        }
        fclose(read);
    }
    return 1;
}

int m_diff(char **args)
{
    if(args[3] != NULL)
        fprintf(stderr, "MSH_ERR: microdiff: Too many arguments. See \"help -microdiff\" for more information\n");
    else if(args[1] == NULL)
        fprintf(stderr, "MSH_ERR: microdiff: Missing arguments. See \"help -microdiff\" for more information\n");
    int line = 0, statusl = 1, statusr = 1, status = 1;
    FILE *f1;
    FILE *f2;
    if(f1 == NULL || f2 == NULL)
        perror("MSH_ERROR: microdiff");
    char *temp;
    f1 = fopen(args[1], "r");
    f2 = fopen(args[2], "r");
    if(f1 == NULL || f2 == NULL)
    {
        perror("MSH_ERROR: microgrep");
        return 1;
    }
    char linel[1024], liner[1024];
    while(status)
    {
        line++;
        if(fgets(linel, 1024, f1) == 0)
            statusl = 0;
        if(fgets(liner, 1024, f2) == 0)
            statusr = 0;
        if(statusr == 0 && statusl == 0)
            status = 0;
        if(strcmp(linel, liner) != 0 && statusl == 1 && statusr == 1)
            printf("Line number: %d\nLeft line: %sRight line: %s", line, linel, liner);
        else if(statusl == 0 && statusr == 1)
            printf("Line number: %d\nLeft line: EMPTY\nRight line: %s", line, liner);
        else if(statusl == 1 && statusr == 0)
            printf("Line number: %d\nLeft line: %sRight line: EMPTY\n", line, linel);
    }
    fclose(f1);
    fclose(f2);
    return 1;
}
