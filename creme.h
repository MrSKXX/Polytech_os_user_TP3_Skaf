#ifndef CREME_H
#define CREME_H

#define CREME_VERSION "3.0"

void beuip_start(char *pseudo);
void beuip_stop(void);
void beuip_liste(void);
void beuip_mess_pseudo(char *target, char *msg);
void beuip_mess_all(char *msg);

#endif