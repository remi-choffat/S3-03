#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <linux/limits.h>
#include <sys/wait.h>

// PID du processus enfant
static pid_t child_pid = -1;

/**
 * Gestionnaire de signal SIGINT
 * Transmet SIGINT au processus enfant si existant.
 */
void handle_sigint(int sig) {
    if (child_pid > 0) {
        kill(child_pid, SIGINT);
    }
}

/**
 * Affiche le prompt de la ligne de commande.
 */
void print_prompt() {
    char cwd[PATH_MAX];
    char *username = getenv("USER");

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // Dernier répertoire du chemin
    char *dir = strrchr(cwd, '/');
    dir = (dir && *(dir + 1) != '\0') ? dir + 1 : cwd;

    printf("%s:%s:$> ", username ? username : "inconnu", dir);
}

/**
 * Évalue une expression simple et retourne le résultat.
 */
int eval(const char *expr) {
    char command[1024];
    snprintf(command, sizeof(command), "echo $((%s))", expr);

    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("popen");
        return 0;
    }

    char result[1024];
    if (fgets(result, sizeof(result), fp) == NULL) {
        pclose(fp);
        return 0;
    }

    pclose(fp);
    return atoi(result);
}

/**
 * Remplace les variables et évalue les expressions dans l'input donné.
 */
char* replace_variables(char *input) {
    static char buffer[1024];

    if (input[0] == '$') {
        if (input[1] == '(' && input[2] == '(') {
            char *expr = input + 3;
            char *end = strstr(expr, "))");
            if (end) {
                *end = '\0';
                snprintf(buffer, sizeof(buffer), "%d", eval(expr));
                return buffer;
            }
        } else {
            char *var_name = input + 1;
            char *env_value = getenv(var_name);
            return env_value ? env_value : "";
        }
    }
    return input;
}

/**
 * Change le répertoire courant.
 * Retourne 0 si succès, -1 sinon.
 */
int change_directory(char **args) {
    char *path = args[1];
    if (!path) {
        path = getenv("HOME");
        if (!path) {
            fprintf(stderr, "Impossible de trouver le répertoire personnel.\n");
            return -1;
        }
    }

    if (chdir(path) != 0) {
        perror("cd");
        return -1;
    }
    return 0;
}

/**
 * Commande echo avec support des échappements et variables.
 */
void echo_command(char **args) {
    for (int i = 1; args[i]; i++) {
        char *arg = replace_variables(args[i]);
        for (int k = 0; arg[k]; k++) {
            if (arg[k] == '\\' && arg[k + 1]) {
                k++;
                switch (arg[k]) {
                    case 'n': putchar('\n'); break;
                    case 't': putchar('\t'); break;
                    default: putchar(arg[k]); break;
                }
            } else {
                putchar(arg[k]);
            }
        }
        if (args[i + 1]) putchar(' ');
    }
    putchar('\n');
}

/**
 * Commande pwd pour afficher le répertoire courant.
 */
int exec_pwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
        return 0;
    } else {
        perror("pwd");
        return -1;
    }
}

/**
 * Affiche la liste des commandes disponibles.
 */
void afficher_liste_commandes() {
    printf("Liste des commandes disponibles :\n");
    printf("- cd\n- echo\n- pwd\n- exit\n");
}

// Table des commandes internes
typedef int (*command_fn)(char **);

typedef struct {
    char *command;
    command_fn exec_fn;
} command_map;

command_map commands[] = {
    {"cd", (command_fn)change_directory},
    {"echo", echo_command},
    {"pwd", (command_fn)exec_pwd},
    {"help", (command_fn)afficher_liste_commandes},
    {NULL, NULL}
};

/**
 * Résout le chemin absolu d'une commande externe.
 */
char* find_command_path(const char *command) {
    char *path_env = getenv("PATH");
    if (!path_env) return NULL;

    char *path_copy = strdup(path_env);
    char *token = strtok(path_copy, ":");
    static char resolved_path[PATH_MAX];

    while (token) {
        snprintf(resolved_path, sizeof(resolved_path), "%s/%s", token, command);
        if (access(resolved_path, X_OK) == 0) {
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
void execute_command(const char *command, char *args[]) {
    // Définit les variables d'environnement pour le programme enfant
    char lang[256];
    char path[256];
    char pwd[256];
    char home[256];
    char user[256];
    char shell[256];

    // Construction des paires clé=valeur
    snprintf(lang, sizeof(lang), "LANG=%s", getenv("LANG"));
    snprintf(path, sizeof(path), "PATH=%s", getenv("PATH"));
    snprintf(pwd, sizeof(pwd), "PWD=%s", getenv("PWD"));
    snprintf(home, sizeof(home), "HOME=%s", getenv("HOME"));
    snprintf(user, sizeof(user), "USER=%s", getenv("USER"));
    snprintf(shell, sizeof(shell), "SHELL=%s", getenv("SHELL"));

    // Tableau des environnements
    char* env[] = {
        path,
        pwd,
        home,
        user,
        shell,
        lang,
        NULL // Terminaison du tableau
    };
    char *resolved_path = find_command_path(command);
    if (!resolved_path) {
        fprintf(stderr, "Commande '%s' introuvable.\n", command);
        exit(EXIT_FAILURE);
    }

    if (execve(resolved_path, args, env) == -1) {
        perror("execve");
        exit(EXIT_FAILURE);
    }
}

/**
 * Exécute une commande (interne ou externe).
 */
int exec_command(char *command, char **args) {
     printf("Arguments:\n");
    for (int i = 0; args[i] != NULL; i++) {
        printf("args[%d]: %s\n", i, args[i]);
    }
    //cherche commande interne
    for (int i = 0; commands[i].command; i++) {
        if (strcmp(command, commands[i].command) == 0) {
            return commands[i].exec_fn(args);
        }
    }
    //si c'est pas commande interne alors commande externe ou inconnue.
    child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    } else if (child_pid == 0) {
        execute_command(command, args);
    } else {
        int status;
        waitpid(child_pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("Code de sortie : %d\n", WEXITSTATUS(status));
        } else {
            fprintf(stderr, "Commande terminée anormalement.\n");
        }
        child_pid = -1;
    }
    return 0;
}

/**
 * Programme principal du shell.
 */
int main() {
    signal(SIGINT, handle_sigint);

    char input[1024];

    while (1) {
        print_prompt();
        if (!fgets(input, sizeof(input), stdin)) break;

        input[strcspn(input, "\n")] = '\0';
        if (strcmp(input, "exit") == 0) break;

        char *command = strtok(input, " ");
        char *args[1024];
        int i = 0;
        args[i++] = command;
        while ((args[i++] = strtok(NULL, " ")));

        exec_command(command, args);
    }

    return 0;
}
