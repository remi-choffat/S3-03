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
#define MAX_COMMAND_LENGTH 1024   // Longueur maximale d'une commande

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

    // Affiche le prompt
    printf("\e[32m"); // Couleur verte
    printf("%s:%s:$> ", username ? username : "inconnu", dir);
    printf("\e[0m"); // Réinitialise la couleur
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
void display_history()
{
    char history_path[PATH_MAX];
    if (get_history_file_path(history_path, sizeof(history_path)) != 0)
    {
        return;
    }

    FILE* file = fopen(history_path, "r"); // Ouvre le fichier en lecture
    if (file == NULL)
    {
        perror("Erreur d'ouverture du fichier d'historique");
        return;
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


/**
 * Efface l'écran
 * @param args Les arguments de la commande clear
 * @return 0 si succès, -1 sinon
 */
int exec_clear(char** args)
{
    printf("\033[H\033[J");
    return 0;
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
    {"history", display_history},
    {"clear", exec_clear},
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
void execute_command(const char *command, char *args[], char* envp[]) {
    char *resolved_path = find_command_path(command);
    if (!resolved_path) {
        fprintf(stderr, "Commande '%s' introuvable.\n", command);
        exit(EXIT_FAILURE);
    }

    if (execve(resolved_path, args, envp) == -1) {
        perror("execve");
        exit(EXIT_FAILURE);
    }
}

/**
 * Exécute une commande (interne ou externe).
 */
int exec_command(char *command, char **args, char * envp[]) {
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
        execute_command(command, args, envp);
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
int main(int argc, char* argv[], char* envp[]) {
    signal(SIGINT, handle_sigint);

    char input[1024];

    while (1) {
        print_prompt();
        if (!fgets(input, sizeof(input), stdin)) break;

        input[strcspn(input, "\n")] = '\0';
        // Ajoute la commande dans l'historique
        add_to_history(input);
        if (strcmp(input, "exit") == 0) break;

        char *command = strtok(input, " ");
        char *args[1024];
        int i = 0;
        args[i++] = command;
        while ((args[i++] = strtok(NULL, " ")));

        exec_command(command, args, envp);
    }

    return 0;
}