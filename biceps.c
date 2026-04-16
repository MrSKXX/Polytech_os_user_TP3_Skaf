#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "gescom.h"
#include "creme.h"

int Sortie(int N, char **P)
{
    (void)N;
    (void)P;
    beuip_stop();
    exit(0);
}

int CommandeCD(int N, char **P)
{
    if (N > 1) {
        if (chdir(P[1]) != 0) {
            perror("biceps: cd");
        }
    } else {
        char *home = getenv("HOME");
        if (home != NULL) {
            if (chdir(home) != 0) {
                perror("biceps: cd");
            }
        } else {
            fprintf(stderr, "biceps: cd: variable HOME non definie\n");
        }
    }
    return 1;
}

int CommandePWD(int N, char **P)
{
    char cwd[1024];
    (void)N; 
    (void)P;

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("biceps: pwd");
    }
    return 1;
}

int CommandeVers(int N, char **P)
{
    (void)N; 
    (void)P;
    printf("biceps version finale (TP3)\n");
    return 1;
}

int CommandeBeuip(int N, char **P)
{
    if (N < 2) return 1;
    
    if (strcmp(P[1], "start") == 0 && N >= 3) {
        beuip_start(P[2]);
    } else if (strcmp(P[1], "stop") == 0) {
        beuip_stop();
    } else if (strcmp(P[1], "list") == 0) {
        beuip_liste();
    } else if (strcmp(P[1], "ls") == 0 && N >= 3) {
        beuip_ls(P[2]);
    } else if (strcmp(P[1], "get") == 0 && N >= 4) {
        beuip_get(P[2], P[3]);
    } else if (strcmp(P[1], "message") == 0 && N >= 4) {
        char msg[1024] = "";
        int i;
        
        for (i = 3; i < N; i++) {
            strcat(msg, P[i]);
            if (i < N - 1) strcat(msg, " ");
        }

        if (strcmp(P[2], "all") == 0) {
            beuip_mess_all(msg);
        } else {
            beuip_mess_pseudo(P[2], msg);
        }
    }
    return 1;
}

void majComInt(void)
{
    ajouteCom("exit", Sortie);
    ajouteCom("cd", CommandeCD);
    ajouteCom("pwd", CommandePWD);
    ajouteCom("vers", CommandeVers);
    ajouteCom("beuip", CommandeBeuip);
}

int main(void)
{
    char *line;
    char *prompt;
    char *user;
    char hostname[256];
    char promptchar;
    int i;
    char hist_file[512];
    char *home;

    signal(SIGINT, SIG_IGN);
    majComInt();

    home = getenv("HOME");
    if (home != NULL) {
        snprintf(hist_file, sizeof(hist_file), "%s/.biceps_history", home);
        read_history(hist_file);
    }

    prompt = malloc(512 * sizeof(char));
    if (prompt == NULL) {
        fprintf(stderr, "biceps: erreur malloc prompt\n");
        exit(1);
    }

    user = getenv("USER");
    if (user == NULL) user = "unknown";

    if (gethostname(hostname, sizeof(hostname)) != 0)
        strcpy(hostname, "unknown");

    if (getuid() == 0)
        promptchar = '#';
    else
        promptchar = '$';

    snprintf(prompt, 512, "%s@%s%c ", user, hostname, promptchar);

    while ((line = readline(prompt)) != NULL) {
        if (*line != '\0') {
            add_history(line);
        }

        char *line_ptr = line;
        char *cmd_seq;

        while ((cmd_seq = strsep(&line_ptr, ";")) != NULL) {
            execPipeline(cmd_seq);
        }
        free(line);
    }

    if (home != NULL) {
        write_history(hist_file);
    }

    free(prompt);
    
    if (Mots != NULL) {
        for (i = 0; i < NMots; i++) {
            free(Mots[i]);
        }
        free(Mots);
    }

    beuip_stop();

    /* Liberation de l'historique readline pour eviter les fuites memoire */
    clear_history();

    return 0;
}