#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <linux/limits.h>
#include <sys/wait.h>

pid_t child_pid = -1; // Variable pour stocker le PID du processus enfant

#define MAX_COMMAND_LENGTH 1024   // Longueur maximale d'une commande


/**
 * Fonction pour gérer le signal SIGINT (Ctrl+C)
 * @param sig Signal à gérer
 */
void handle_sigint(int sig)
{
    if (child_pid > 0)
    {
        kill(child_pid, SIGINT); // Transmet SIGINT au processus enfant
    }
}


/**
 * Affiche le prompt de commande
 */
void print_prompt()
{
    char cwd[PATH_MAX];
    char* username = getenv("USER");

    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // Récupère le nom du dernier répertoire
    // char* dir = strrchr(cwd, '/'); // Pour afficher uniquement le dernier dossier
    char* dir = cwd; // Pour afficher le chemin complet
    dir = (dir != NULL && *(dir + 1) != '\0') ? dir + 1 : cwd;

    // Affiche le prompt
    printf("\e[32m"); // Couleur verte
    printf("%s:%s:$> ", username ? username : "inconnu", dir);
    printf("\e[0m"); // Réinitialise la couleur
}


/**
 * Récupère le chemin complet vers le fichier history.txt dans le répertoire personnel.
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
 * Évalue une expression simple
 * @param expr Expression à évaluer
 * @return Résultat de l'expression
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
 * Fonction pour traiter et remplacer les variables
 */
char* replace_variables(char* input)
{
    static char buffer[1024]; // Tampon pour stocker le résultat
    if (input[0] == '$')
    {
        if (input[1] == '(' && input[2] == '(')
        {
            // Supprime le '$((' et '))'
            char* expr = input + 3;
            char* end = strstr(expr, "))");
            if (end != NULL)
            {
                *end = '\0';
                // Évalue l'expression arithmétique
                const int result = eval(expr);
                snprintf(buffer, sizeof(buffer), "%d", result);
                return buffer;
            }
        }
        else
        {
            // Supprime le '$'
            char* var_name = input + 1;
            // Recherche de la variable d'environnement
            char* env_value = getenv(var_name);
            if (env_value != NULL)
            {
                return env_value;
            }
            else
            {
                return ""; // Si la variable n'est pas définie, retourne une chaîne vide
            }
        }
    }
    return input; // Si pas de '$', retourne l'argument tel quel
}


/**
 * Change le répertoire courant
 * @param path Le chemin vers lequel changer
 * @return 0 si succès, -1 sinon
 */
int change_directory(const char* path)
{
    if (path == NULL)
    {
        // Si aucun chemin n'est donné, va dans le répertoire personnel HOME
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


/**
 * Exécute la commande echo
 * @param args Les arguments de la commande echo
 */
void echo_command(char** args)
{
    for (int i = 1; args[i] != NULL; i++)
    {
        char* arg = args[i];
        char buffer[1024]; // Tampon pour le texte avec interprétation des échappements
        int j = 0;

        // Remplacer les variables d'environnement
        arg = replace_variables(arg);

        // Parcourir chaque caractère de l'argument
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
        buffer[j] = '\0'; // Terminaison du tampon

        // Afficher l'argument traité
        printf("%s", buffer);

        // Ne pas ajouter d'espace entre les arguments, sauf entre les mots
        if (args[i + 1] != NULL)
        {
            printf(" ");
        }
    }
    printf("\n");
}


// Définition des fonctions d'exécution des commandes

int exec_cd(char** args)
{
    change_directory(args[1]);
    return 0;
}

int exec_echo(char** args)
{
    echo_command(args);
    return 0;
}


int exec_pwd()
{
    char cwd[PATH_MAX]; // Tampon pour stocker le chemin courant

    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        // Si le chemin est récupéré avec succès
        printf("%s\n", cwd);
        return 0;
    }
    else
    {
        // Si une erreur survient
        perror("pwd");
        return -1;
    }
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


void afficherListeCommandesDispo();

// Tableau de fonctions
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
    {"history", display_history},
    {"clear", exec_clear},
    {NULL, NULL}
};


/**
 * Affiche la liste des commandes disponibles dans mbash
 */
void afficherListeCommandesDispo()
{
    printf("Liste des commandes disponibles :\n");
    for (int i = 1; commands[i].command != NULL; i++) // N'affiche pas la commande COMMANDES
    {
        printf("- %s\n", commands[i].command);
    }
}

// Fonction pour résoudre le chemin absolu d'une commande à partir de la variable PATH
char* findCommandPath(const char* command)
{
    // Récupère la variable d'environnement PATH
    char* pathEnv = getenv("PATH");
    if (!pathEnv)
    {
        fprintf(stderr, "Erreur : La variable PATH n'est pas définie.\n");
        return NULL;
    }
    // Crée une copie de PATH pour éviter de modifier l'original
    char* pathCopy = strdup(pathEnv);
    if (!pathCopy)
    {
        perror("Erreur lors de la duplication de PATH");
        return NULL;
    }
    // Initialisation pour analyser PATH et rechercher le chemin de la commande
    char* token = strtok(pathCopy, ":");
    static char resolvedPath[1024];
    // Parcours des répertoires dans PATH
    while (token != NULL)
    {
        // Construit le chemin complet pour la commande
        snprintf(resolvedPath, sizeof(resolvedPath), "%s/%s", token, command);
        // Vérifie si le fichier est exécutable
        if (access(resolvedPath, X_OK) == 0)
        {
            free(pathCopy); // Libère la mémoire allouée pour pathCopy
            return resolvedPath; // Retourne le chemin résolu
        }
        // Passe au répertoire suivant dans PATH
        token = strtok(NULL, ":");
    }
    // Si la commande n'a pas été trouvée, nettoie et retourne NULL
    free(pathCopy);
    return NULL;
}

// Fonction pour exécuter une commande externe
void executeCommand(const char* command, char* arguments[])
{
    // Résout le chemin absolu de la commande
    char* resolvedPath = findCommandPath(command);
    if (!resolvedPath)
    {
        fprintf(stderr, "La commande '%s' n'existe pas.\n", command);
        exit(EXIT_FAILURE);
    }

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
    // Exécute la commande avec son chemin absolu
    if (execve(resolvedPath, arguments, env) == -1)
    {
        perror("Erreur lors de l'exécution de execve");
        exit(EXIT_FAILURE);
    }
}

/**
 * Exécute la commande donnée
 * @param commande La commande à exécuter
 * @param args Les arguments de la commande
 */
int exec(char* commande, char** args)
{
    // Recherche de la commande dans le tableau
    for (int i = 0; commands[i].command != NULL; i++)
    {
        if (strcmp(commande, commands[i].command) == 0)
        {
            return commands[i].exec_fn(args);
        }
    }
    // Si commande inconnue, appeler exec_unknown
    // Crée un processus enfant
    child_pid = fork();

    if (child_pid < 0)
    {
        perror("Erreur lors de la création du processus enfant");
        return EXIT_FAILURE;
    }
    else if (child_pid == 0)
    {
        args[0] = commande;
        // Code du processus enfant
        executeCommand(commande, args);
    }
    else
    {
        // Code du processus parent
        int status;
        waitpid(child_pid, &status, 0); // Attend la fin du processus enfant
        child_pid = -1;
    }

    return 0;
}


/**
 * Méthode principale
 */
int main()
{
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Se positionne dans le répertoire par défaut
    change_directory(NULL);

    char input[1024];

    while (1)
    {
        print_prompt();

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break; // Quitte sur Ctrl+D
        }

        input[strcspn(input, "\n")] = '\0'; // Supprime le saut de ligne

        if (strcmp(input, "exit") == 0)
        {
            // Quitte sur "exit"
            break;
        }

        if (strlen(input) > 0)
        {
            // Ajoute la commande dans l'historique
            add_to_history(input);

            // Exécute la commande
            char* commande = strtok(input, " ");
            char* args[1024];
            // Remplit la liste des arguments
            int i = 1;
            while ((args[i] = strtok(NULL, " ")))
            {
                i++;
            }
            // Exécute la commande
            exec(commande, args);
        }
    }

    return 0;
}
