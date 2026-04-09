#ifndef GESCOM_H
#define GESCOM_H

#define GESCOM_VERSION "1.2"

extern char **Mots;
extern int NMots;

int analyseCom(char *b);
void ajouteCom(char *nom, int (*func)(int, char **));
void listeComInt(void);
int execComInt(int N, char **P);
int execComExt(char **P);
int execPipeline(char *cmd_seq);

#endif