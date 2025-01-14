#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
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
 * Exécute la commande donnée
 * @param commande La commande à exécuter
 * @param args Les arguments de la commande
 */
int exec(char* commande, char** args)
{
    if (strcmp(commande, "cd") == 0)
    {
        // Si la commande est "cd"
        return change_directory(args[0]);
    }

    // TODO - Exécuter la commande
    printf("La commande \"%s\" sera exécutée\n", commande);
    if (args[0] != NULL)
    {
        printf("Avec les arguments suivants:\n");
        for (int i = 0; args[i] != NULL; i++)
        {
            printf("  - %s\n", args[i]);
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
            while ((args[i++] = strtok(NULL, " ")))
            {
            }

            // Exécute la commande
            exec(commande, args);
        }
    }

    return 0;
}
