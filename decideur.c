#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

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

// Fonction principale
int main()
{
    // Nom de la commande à exécuter
    char* commande = "javac";

    // Arguments de la commande, le premier étant le nom de la commande lui-même
    char* arguments[] = {
        commande,
        "Macouille.java",
        NULL // Fin du tableau
    };

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
        executeCommand(commande, arguments);
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
