#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <linux/limits.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <readline/readline.h>
#include <readline/history.h>

// PID du processus enfant
static pid_t child_pid = -1;

// Longueur maximale d'une commande
#define MAX_COMMAND_LENGTH 1024


// Structure pour stocker les variables
typedef struct
{
    char name[50];
    char value[50];
} Variable;

Variable variables[100];
int variable_count = 0;


/**
 * Gestionnaire de signal SIGINT
 * Transmet SIGINT au processus enfant si existant.
 */
void handle_sigint(int sig)
{
    if (child_pid > 0)
    {
        kill(child_pid, SIGINT);
    }
}

/**
 * Affiche le prompt de la ligne de commande.
 */
char* get_prompt(char* envp[])
{
    static char prompt[PATH_MAX + 50];
    char cwd[PATH_MAX];
    char* username = NULL;
    char* home = NULL;

    // Retrieve USER and HOME from envp
    for (int i = 0; envp[i] != NULL; i++)
    {
        if (strncmp(envp[i], "USER=", 5) == 0)
        {
            username = envp[i] + 5;
        }
        else if (strncmp(envp[i], "HOME=", 5) == 0)
        {
            home = envp[i] + 5;
        }
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // Replace home directory with ~
    if (home && strstr(cwd, home) == cwd)
    {
        snprintf(prompt, sizeof(prompt), "\033[1;32m%s\033[0m:\033[1;34m~%s\033[0m$> ",
                 username ? username : "unknown", cwd + strlen(home));
    }
    else
    {
        snprintf(prompt, sizeof(prompt), "\033[1;32m%s\033[0m:\033[1;34m%s\033[0m$> ",
                 username ? username : "unknown", cwd);
    }

    return prompt;
}


void set_variable(const char* name, const char* value)
{
    for (int i = 0; i < variable_count; i++)
    {
        if (strcmp(variables[i].name, name) == 0)
        {
            strcpy(variables[i].value, value);
            return;
        }
    }
    strcpy(variables[variable_count].name, name);
    strcpy(variables[variable_count].value, value);
    variable_count++;
}

char* get_variable(const char* name)
{
    for (int i = 0; i < variable_count; i++)
    {
        if (strcmp(variables[i].name, name) == 0)
        {
            return variables[i].value;
        }
    }
    return NULL;
}

char* replace_variables(char* input)
{
    static char buffer[1024];
    char* p = buffer;
    while (*input)
    {
        if (*input == '$')
        {
            input++;
            char var_name[50];
            char* v = var_name;
            while (isalnum(*input) || *input == '_')
            {
                *v++ = *input++;
            }
            *v = '\0';
            char* value = get_variable(var_name);
            if (value)
            {
                while (*value)
                {
                    *p++ = *value++;
                }
            }
        }
        else
        {
            *p++ = *input++;
        }
    }
    *p = '\0';
    return buffer;
}


/**
 * Affiche la liste des fichiers du répertoire avec des couleurs.
 */
void list_files_with_colors(char** args)
{
    FILE* fp;
    char path[1035];
    char command[1024] = "ls";

    // Append arguments to the command
    for (int i = 1; args[i]; i++)
    {
        strcat(command, " ");
        strcat(command, args[i]);
    }

    // Open the command for reading
    fp = popen(command, "r");
    if (fp == NULL)
    {
        perror("popen");
        return;
    }

    // Read the output a line at a time and colorize it
    while (fgets(path, sizeof(path) - 1, fp) != NULL)
    {
        // Remove the newline character
        path[strcspn(path, "\n")] = '\0';

        // Extract the file name from the output
        char* file_name = strrchr(path, ' ');
        if (file_name)
        {
            file_name++;
        }
        else
        {
            file_name = path;
        }

        // Check if the path is a directory
        struct stat statbuf;
        if (stat(file_name, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
        {
            printf("\033[1;34m%s\033[0m\n", path); // Bold blue for directories
        }
        else
        {
            printf("\033[0;32m%s\033[0m\n", path); // Green for files
        }
    }

    // Close the file pointer
    pclose(fp);
}


/**
 * Évalue une expression simple et retourne le résultat.
 */
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


/**
 * Remplace les variables et évalue les expressions dans l'input donné.
 */
/**
 * Replaces variables and evaluates expressions in the given input.
 */
char* interprete_variables(char* input)
{
    static char buffer[1024];
    char* p = buffer;
    while (*input)
    {
        if (*input == '$')
        {
            input++;
            if (*input == '(' && *(input + 1) == '(')
            {
                input += 2;
                char expr[100];
                char* e = expr;
                while (*input && !(*input == ')' && *(input + 1) == ')'))
                {
                    *e++ = *input++;
                }
                *e = '\0';
                input += 2; // Skip the closing ))
                snprintf(p, sizeof(buffer) - (p - buffer), "%d", eval(expr));
                p += strlen(p);
            }
            else
            {
                char var_name[50];
                char* v = var_name;
                while (isalnum(*input) || *input == '_')
                {
                    *v++ = *input++;
                }
                *v = '\0';
                char* value = get_variable(var_name);
                if (value)
                {
                    strcpy(p, value);
                    p += strlen(value);
                }
                else
                {
                    char* env_value = getenv(var_name);
                    if (env_value)
                    {
                        strcpy(p, env_value);
                        p += strlen(env_value);
                    }
                }
            }
        }
        else
        {
            *p++ = *input++;
        }
    }
    *p = '\0';
    return buffer;
}


/**
 * Récupère le chemin complet vers le fichier .mbash_history dans le répertoire personnel.
 * @param buffer Buffer pour stocker le chemin.
 * @param size Taille du buffer.
 * @return 0 si succès, -1 sinon.
 */
int get_history_file_path(char* buffer, size_t size)
{
    const char* home = getenv("HOME");
    if (home == NULL)
    {
        fprintf(stderr, "Impossible de déterminer le répertoire personnel.\n");
        return -1;
    }

    // Construit le chemin complet vers history.txt
    if (snprintf(buffer, size, "%s/%s", home, ".mbash_history") >= size)
    {
        fprintf(stderr, "Chemin du fichier d'historique trop long.\n");
        return -1;
    }

    return 0;
}


/**
 * Ajoute une commande dans le fichier d'historique.
 * @param command La commande à enregistrer.
 */
void add_to_history(const char* command)
{
    char history_path[PATH_MAX];
    if (get_history_file_path(history_path, sizeof(history_path)) != 0)
    {
        return;
    }

    FILE* file = fopen(history_path, "a"); // Ouvre le fichier en mode ajout
    if (file == NULL)
    {
        perror("Erreur d'ouverture du fichier d'historique");
        return;
    }

    fprintf(file, "%s\n", command); // Écrit la commande dans le fichier
    fclose(file);
}

/**
 * Affiche l'historique des commandes.
 */
int display_history()
{
    char history_path[PATH_MAX];
    if (get_history_file_path(history_path, sizeof(history_path)) != 0)
    {
        return 0;
    }

    FILE* file = fopen(history_path, "r"); // Ouvre le fichier en lecture
    if (file == NULL)
    {
        perror("Erreur d'ouverture du fichier d'historique");
        return -1;
    }

    char line[MAX_COMMAND_LENGTH];
    int line_number = 1;

    // Lit et affiche chaque ligne du fichier
    while (fgets(line, sizeof(line), file) != NULL)
    {
        line[strcspn(line, "\n")] = '\0'; // Supprime le saut de ligne
        printf("%d  %s\n", line_number++, line);
    }

    fclose(file);
    return 0;
}

/**
 * Change le répertoire courant.
 * Retourne 0 si succès, -1 sinon.
 */
int change_directory(char** args)
{
    char* path = args[1];
    if (!path)
    {
        path = getenv("HOME");
        if (!path)
        {
            fprintf(stderr, "Impossible de trouver le répertoire personnel.\n");
            return -1;
        }
    }

    if (chdir(path) != 0)
    {
        perror("cd");
        return -1;
    }
    return 0;
}

/**
 * Commande echo avec support des échappements et variables.
 */
int echo_command(char** args)
{
    for (int i = 1; args[i]; i++)
    {
        char* arg = interprete_variables(args[i]);
        for (int k = 0; arg[k]; k++)
        {
            if (arg[k] == '\\' && arg[k + 1])
            {
                k++;
                switch (arg[k])
                {
                case 'n': putchar('\n');
                    break;
                case 't': putchar('\t');
                    break;
                default: putchar(arg[k]);
                    break;
                }
            }
            else
            {
                putchar(arg[k]);
            }
        }
        if (args[i + 1]) putchar(' ');
    }
    putchar('\n');
    return 0;
}

/**
 * Commande pwd pour afficher le répertoire courant.
 */
int exec_pwd()
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)))
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


// Table des commandes internes
typedef int (*command_fn)(char**);

typedef struct
{
    char* command;
    command_fn exec_fn;
} command_map;

command_map commands[] = {
    {"cd", (command_fn)change_directory},
    {"echo", echo_command},
    {"pwd", (command_fn)exec_pwd},
    {"history", display_history},
    {NULL, NULL}
};

/**
 * Résout le chemin absolu d'une commande externe.
 */
char* find_command_path(const char* command)
{
    char* path_env = getenv("PATH");
    if (!path_env) return NULL;

    char* path_copy = strdup(path_env);
    char* token = strtok(path_copy, ":");
    static char resolved_path[PATH_MAX];

    while (token)
    {
        snprintf(resolved_path, sizeof(resolved_path), "%s/%s", token, command);
        if (access(resolved_path, X_OK) == 0)
        {
            free(path_copy);
            return resolved_path;
        }
        token = strtok(NULL, ":");
    }
    free(path_copy);
    return NULL;
}

/**
 * Exécute une commande externe via execve.
 */
void execute_command(const char* command, char* args[], char* envp[])
{
    char* resolved_path = find_command_path(command);
    if (!resolved_path)
    {
        fprintf(stderr, "Commande '%s' introuvable.\n", command);
        exit(EXIT_FAILURE);
    }

    if (execve(resolved_path, args, envp) == -1)
    {
        perror("execve");
        exit(EXIT_FAILURE);
    }
}

/**
 * Exécute une commande (interne ou externe).
 */
int exec_command(char* command, char** args, char* envp[])
{
    if (strchr(command, '='))
    {
        char* name = strtok(command, "=");
        char* value = strtok(NULL, "=");
        if (name && value)
        {
            set_variable(name, value);
        }
        return 0;
    }

    //cherche commande interne
    for (int i = 0; commands[i].command; i++)
    {
        if (strcmp(command, commands[i].command) == 0)
        {
            return commands[i].exec_fn(args);
        }
    }

    // Intercepte le ls pour afficher les fichiers avec des couleurs
    if (strcmp(command, "ls") == 0)
    {
        list_files_with_colors(args);
        return 0;
    }

    // S'il ne s'agit pas d'une commande interne
    child_pid = fork();
    if (child_pid < 0)
    {
        perror("fork");
        return EXIT_FAILURE;
    }
    else if (child_pid == 0)
    {
        execute_command(command, args, envp);
    }
    else
    {
        int status;
        waitpid(child_pid, &status, 0);
        child_pid = -1;
    }
    return 0;
}

/**
 * Programme principal du shell.
 */
int main(int argc, char* argv[], char* envp[])
{
    signal(SIGINT, handle_sigint);

    char* input;
    char history_path[PATH_MAX];

    // Charger l'historique depuis le fichier
    if (get_history_file_path(history_path, sizeof(history_path)) == 0)
    {
        read_history(history_path);
    }

    while (1)
    {
        char* prompt = get_prompt(envp);
        input = readline(prompt); // Utilise readline pour lire l'entrée de l'utilisateur

        if (!input)
        {
            putchar('\n');
            break;
        }

        if (strlen(input) > 0)
        {
            add_history(input); // Ajoute la commande à l'historique
            add_to_history(input); // Ajoute la commande dans le fichier d'historique
        }

        if (strcmp(input, "exit") == 0)
        {
            free(input);
            break;
        }

        char* command = strtok(input, " ");
        char* args[1024];
        int i = 0;
        args[i++] = command;
        while ((args[i++] = strtok(NULL, " ")))
        {
        }

        exec_command(command, args, envp);

        free(input);
    }

    // Sauvegarder l'historique dans le fichier
    if (get_history_file_path(history_path, sizeof(history_path)) == 0)
    {
        write_history(history_path);
    }

    return 0;
}
