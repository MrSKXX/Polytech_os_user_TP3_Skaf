#ifndef CREME_H
#define CREME_H

#define CREME_VERSION "3.1"
#define BROADCAST_IP "192.168.88.255"

void beuip_start(char *pseudo);
void beuip_stop(void);
void beuip_liste(void);
void beuip_mess_pseudo(char *target, char *msg);
void beuip_mess_all(char *msg);
void beuip_ls(char *pseudo);
void beuip_get(char *pseudo, char *nomfic);

#endif