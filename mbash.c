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
    char* last_dir = strrchr(cwd, '/');
    last_dir = (last_dir != NULL && *(last_dir + 1) != '\0') ? last_dir + 1 : cwd;

    // Affiche le prompt
    printf("%s:%s:$> ", username ? username : "inconnu", last_dir);
}


int main()
{
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
            printf("La commande \"%s\" sera exécutée\n", input); // TODO - Exécuter la commande
        }
    }

    return 0;
}
