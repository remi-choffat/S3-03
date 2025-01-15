#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <linux/limits.h>
#include <sys/wait.h>

pid_t child_pid = -1; // Variable pour stocker le PID du processus enfant

void handle_sigint(int sig)
{
    if (child_pid > 0)
    {
        kill(child_pid, SIGINT); // Transmet SIGINT au processus enfant
    }
}

void print_prompt()
{
    char cwd[PATH_MAX];
    char* username = getenv("USER");

    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    char* dir = cwd;
    dir = (dir != NULL && *(dir + 1) != '\0') ? dir + 1 : cwd;

    printf("%s:%s:$> ", username ? username : "inconnu", dir);
}

int eval(const char* expr)
{
    char command[1024];
    snprintf(command, sizeof(command), "echo $((%s))", expr);

    FILE* fp = popen(command, "r");
    if (fp == NULL)
    {
        perror("popen");
        return 0;
    }

    char result[1024];
    if (fgets(result, sizeof(result), fp) == NULL)
    {
        pclose(fp);
        return 0;
    }

    pclose(fp);
    return atoi(result);
}

char* replace_variables(char* input)
{
    static char buffer[1024];
    if (input[0] == '$')
    {
        if (input[1] == '(' && input[2] == '(')
        {
            char* expr = input + 3;
            char* end = strstr(expr, "))");
            if (end != NULL)
            {
                *end = '\0';
                const int result = eval(expr);
                snprintf(buffer, sizeof(buffer), "%d", result);
                return buffer;
            }
        }
        else
        {
            char* var_name = input + 1;
            char* env_value = getenv(var_name);
            if (env_value != NULL)
            {
                return env_value;
            }
            else
            {
                return "";
            }
        }
    }
    return input;
}

int change_directory(const char* path)
{
    if (path == NULL)
    {
        path = getenv("HOME");
        if (path == NULL)
        {
            fprintf(stderr, "Impossible de trouver le répertoire personnel.\n");
            fflush(stdout);
            return -1;
        }
    }

    if (chdir(path) != 0)
    {
        perror("cd");
        return -1;
    }
    fflush(stdout);
    return 0;
}

void echo_command(char** args)
{
    for (int i = 0; args[i] != NULL; i++)
    {
        char* arg = args[i];
        char buffer[1024];
        int j = 0;

        arg = replace_variables(arg);

        for (int k = 0; arg[k] != '\0'; k++)
        {
            if (arg[k] == '\\' && arg[k + 1] != '\0')
            {
                k++;
                switch (arg[k])
                {
                case 'n':
                    buffer[j++] = '\n';
                    break;
                case 't':
                    buffer[j++] = '\t';
                    break;
                case '\\':
                    buffer[j++] = '\\';
                    break;
                case '\"':
                    buffer[j++] = '\"';
                    break;
                case '\'':
                    buffer[j++] = '\'';
                    break;
                default:
                    buffer[j++] = arg[k];
                    break;
                }
            }
            else
            {
                buffer[j++] = arg[k];
            }
        }
        buffer[j] = '\0';

        printf("%s", buffer);

        if (args[i + 1] != NULL)
        {
            printf(" ");
        }
    }
    printf("\n");
}

int exec_cd(char** args)
{
    change_directory(args[0]);
    return 0;
}

int exec_echo(char** args)
{
    echo_command(args);
    return 0;
}

int exec_pwd()
{
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd);
        return 0;
    }
    else
    {
        perror("pwd");
        return -1;
    }
}

void afficherListeCommandesDispo();


typedef int (*command_fn)(char**);

typedef struct
{
    char* command;
    command_fn exec_fn;
} command_map;

command_map commands[] = {
    {"COMMANDES", afficherListeCommandesDispo},
    {"cd", exec_cd},
    {"echo", exec_echo},
    {"pwd", exec_pwd},
    {NULL, NULL}
};

void afficherListeCommandesDispo()
{
    printf("Liste des commandes disponibles :\n");
    for (int i = 1; commands[i].command != NULL; i++)
    {
        printf("- %s\n", commands[i].command);
    }
}

char* findCommandPath(const char* command)
{
    char* pathEnv = getenv("PATH");
    if (!pathEnv)
    {
        fprintf(stderr, "Erreur : La variable PATH n'est pas définie.\n");
        return NULL;
    }
    char* pathCopy = strdup(pathEnv);
    if (!pathCopy)
    {
        perror("Erreur lors de la duplication de PATH");
        return NULL;
    }
    char* token = strtok(pathCopy, ":");
    static char resolvedPath[1024];
    while (token != NULL)
    {
        snprintf(resolvedPath, sizeof(resolvedPath), "%s/%s", token, command);
        if (access(resolvedPath, X_OK) == 0)
        {
            free(pathCopy);
            return resolvedPath;
        }
        token = strtok(NULL, ":");
    }
    free(pathCopy);
    return NULL;
}

void executeCommand(const char* command, char* arguments[])
{
    char* resolvedPath = findCommandPath(command);
    if (!resolvedPath)
    {
        fprintf(stderr, "Erreur : Commande '%s' inconnue.\n", command);
        exit(EXIT_FAILURE);
    }
    char* env[] = {
        getenv("PATH"),
        getenv("PWD"),
        getenv("HOME"),
        getenv("USER"),
        getenv("SHELL"),
        getenv("LANG"),
        NULL
    };
    if (execve(resolvedPath, arguments, env) == -1)
    {
        perror("Erreur lors de l'exécution de execve");
        exit(EXIT_FAILURE);
    }
}

int exec(const char* commande, char** args)
{
    for (int i = 0; commands[i].command != NULL; i++)
    {
        if (strcmp(commande, commands[i].command) == 0)
        {
            return commands[i].exec_fn(args);
        }
    }
    child_pid = fork();

    if (child_pid < 0)
    {
        perror("Erreur lors de la création du processus enfant");
        return EXIT_FAILURE;
    }
    else if (child_pid == 0)
    {
        executeCommand(commande, args);
    }
    else
    {
        int status;
        waitpid(child_pid, &status, 0);
        if (WIFEXITED(status))
        {
            printf("La commande s'est terminée avec le code de sortie %d\n", WEXITSTATUS(status));
        }
        else
        {
            fprintf(stderr, "La commande n'a pas terminé normalement.\n");
        }
        child_pid = -1;
    }

    return 0;
}

int main()
{
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    change_directory(NULL);

    char input[1024];

    while (1)
    {
        print_prompt();

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0)
        {
            break;
        }

        if (strlen(input) > 0)
        {
            char* commande = strtok(input, " ");
            char* args[1024];
            int i = 0;
            while ((args[i] = strtok(NULL, " ")))
            {
                i++;
            }
            args[i] = NULL;

            exec(commande, args);
        }
    }

    return 0;
}
