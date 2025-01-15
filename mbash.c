#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <linux/limits.h>
#include <sys/wait.h>

/**
 * Affiche le prompt de l'interpréteur de commande
 */
void print_prompt()
{
    char cwd[PATH_MAX];

    // Récupère le nom d'utilisateur
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
    printf("%s:%s:$> ", username ? username : "inconnu", dir);
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
    for (int i = 0; args[i] != NULL; i++)
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
        fprintf(stderr, "Erreur : Commande '%s' inconnue.\n", command);
        exit(EXIT_FAILURE);
    }
    // Définit les variables d'environnement pour le programme enfant
    char* env[] = {
        getenv("PATH"), // Inclut PATH
        getenv("PWD"), // Répertoire courant
        getenv("HOME"), // Répertoire personnel
        getenv("USER"), // Nom de l'utilisateur
        getenv("SHELL"), //programme shell de l'utilisateur
        getenv("LANG"), // Langue
        NULL // Terminaison du tableau d'environnement
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
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("Erreur lors de la création du processus enfant");
        return EXIT_FAILURE;
    }
    else if (pid == 0)
    {
        // Code du processus enfant
        executeCommand(commande, args);
    }
    else
    {
        // Code du processus parent
        int status;
        waitpid(pid, &status, 0); // Attend la fin du processus enfant
        if (WIFEXITED(status))
        {
            printf("La commande s'est terminée avec le code de sortie %d\n", WEXITSTATUS(status));
        }
        else
        {
            fprintf(stderr, "La commande n'a pas terminé normalement.\n");
        }
    }

    return 0;
}


/**
 * Méthode principale
 */
int main()
{
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
            // Exécute la commande
            char* commande = strtok(input, " ");
            char* args[1024];
            // Remplit la liste des arguments
            int i = 0;
            while ((args[i] = strtok(NULL, " ")))
            {
                i++;
            }
            args[i] = NULL;

            // Exécute la commande
            exec(commande, args);
        }
    }

    return 0;
}
