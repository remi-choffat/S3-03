#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <linux/limits.h>


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

int exec_unknown(char* commande, char** args)
{
    printf("La commande %s n'est pas reconnue.\n", commande);
    printf("Tapez COMMANDES pour afficher la liste des commandes implémentées dans mbash.\n");
    return 1;
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
    {NULL, exec_unknown} // Commande par défaut
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
    return exec_unknown(commande, args);
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
            while ((args[i++] = strtok(NULL, " ")))
            {
            }

            // Exécute la commande
            exec(commande, args);
        }
    }

    return 0;
}
