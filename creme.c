#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <netdb.h>
#include "creme.h"

#define PORT 9998
#define LBUF 1024
#define MAX_USERS 255

struct User {
    unsigned long ip;
    char pseudo[256];
};

static struct User table[MAX_USERS];
static int nb_users = 0;
static int sid = -1;
static char my_pseudo[256];
static pthread_t server_thread;
static int server_running = 0;
static pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

static char * addrip(unsigned long A) {
    static char b[16];
    sprintf(b,"%u.%u.%u.%u",(unsigned int)(A>>24&0xFF),(unsigned int)(A>>16&0xFF),
           (unsigned int)(A>>8&0xFF),(unsigned int)(A&0xFF));
    return b;
}

static void add_user(unsigned long ip_addr, char *pseudo) {
    int i;
    pthread_mutex_lock(&table_mutex);
    for (i = 0; i < nb_users; i++) {
        if (table[i].ip == ip_addr && strcmp(table[i].pseudo, pseudo) == 0) {
            pthread_mutex_unlock(&table_mutex);
            return;
        }
    }
    if (nb_users < MAX_USERS) {
        table[nb_users].ip = ip_addr;
        strncpy(table[nb_users].pseudo, pseudo, 255);
        table[nb_users].pseudo[255] = '\0';
        nb_users++;
    }
    pthread_mutex_unlock(&table_mutex);
}

static void remove_user(unsigned long ip_addr, char *pseudo) {
    int i, j;
    pthread_mutex_lock(&table_mutex);
    for (i = 0; i < nb_users; i++) {
        if (table[i].ip == ip_addr && strcmp(table[i].pseudo, pseudo) == 0) {
            for (j = i; j < nb_users - 1; j++) {
                table[j] = table[j+1];
            }
            nb_users--;
            break;
        }
    }
    pthread_mutex_unlock(&table_mutex);
}

static void commande(char octet1, char *message, char *pseudo) {
    struct sockaddr_in dest;
    char msg_out[LBUF];
    int i;

    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);

    pthread_mutex_lock(&table_mutex);
    
    if (octet1 == '0') {
        sprintf(msg_out, "0BEUIP%s", my_pseudo);
        for (i = 0; i < nb_users; i++) {
            if (table[i].ip != inet_addr("127.0.0.1")) {
                dest.sin_addr.s_addr = table[i].ip;
                sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
            }
        }
    } else if (octet1 == '4' && pseudo != NULL) {
        int found = 0;
        sprintf(msg_out, "9BEUIP%s", message);
        for (i = 0; i < nb_users; i++) {
            if (strcmp(table[i].pseudo, pseudo) == 0) {
                dest.sin_addr.s_addr = table[i].ip;
                sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
                printf("Message prive envoye a %s (%s)\n", pseudo, addrip(ntohl(table[i].ip)));
                found = 1;
                break;
            }
        }
        if (!found) printf("Erreur: pseudo %s introuvable.\n", pseudo);
    } else if (octet1 == '5') {
        sprintf(msg_out, "9BEUIP%s", message);
        for (i = 0; i < nb_users; i++) {
            if (table[i].ip != inet_addr("127.0.0.1")) {
                dest.sin_addr.s_addr = table[i].ip;
                sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
            }
        }
        printf("Message broadcast envoye a tous.\n");
    }

    pthread_mutex_unlock(&table_mutex);
}

static void *serveur_udp(void *p) {
    struct sockaddr_in Sock, SockConf, SockBcast;
    socklen_t ls;
    char buf[LBUF+1];
    char msg_out[LBUF+1];
    int on = 1;
    int n;
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    if ((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) return NULL;
    if (setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) return NULL;

    bzero(&SockConf, sizeof(SockConf));
    SockConf.sin_family = AF_INET;
    SockConf.sin_addr.s_addr = htonl(INADDR_ANY);
    SockConf.sin_port = htons(PORT);

    if (bind(sid, (struct sockaddr *) &SockConf, sizeof(SockConf)) == -1) return NULL;

    sprintf(msg_out, "1BEUIP%s", my_pseudo);

    if (getifaddrs(&ifaddr) != -1) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
                continue;

            if (ifa->ifa_broadaddr != NULL) {
                if (getnameinfo(ifa->ifa_broadaddr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
                    if (strcmp(host, "127.0.0.1") != 0 && strcmp(host, "0.0.0.0") != 0) {
                        bzero(&SockBcast, sizeof(SockBcast));
                        SockBcast.sin_family = AF_INET;
                        SockBcast.sin_addr.s_addr = inet_addr(host);
                        SockBcast.sin_port = htons(PORT);
                        sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&SockBcast, sizeof(SockBcast));
                    }
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    add_user(inet_addr("127.0.0.1"), my_pseudo);

    while (server_running) {
        ls = sizeof(Sock);
        if ((n = recvfrom(sid, (void*)buf, LBUF, 0, (struct sockaddr *)&Sock, &ls)) > 0) {
            buf[n] = '\0';
            if (n >= 6 && strncmp(buf+1, "BEUIP", 5) == 0) {
                char code = buf[0];
                char *payload = buf + 6;

                if (code == '1' || code == '2') {
                    printf("\n[Serveur] Message recu de %s : code=%c pseudo=%s\n", addrip(ntohl(Sock.sin_addr.s_addr)), code, payload);
                    add_user(Sock.sin_addr.s_addr, payload);
                    
                    if (code == '1') {
                        sprintf(msg_out, "2BEUIP%s", my_pseudo);
                        sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&Sock, ls);
                    }
                }
                else if (code == '0') {
                    printf("\n[Serveur] Deconnexion de %s\n", payload);
                    remove_user(Sock.sin_addr.s_addr, payload);
                }
                else if (code == '9') {
                    int i, found = 0;
                    pthread_mutex_lock(&table_mutex);
                    for (i = 0; i < nb_users; i++) {
                        if (table[i].ip == Sock.sin_addr.s_addr) {
                            printf("\n[Message de %s] : %s\n", table[i].pseudo, payload);
                            found = 1;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&table_mutex);
                    if (!found) printf("\n[Message d'inconnu %s] : %s\n", addrip(ntohl(Sock.sin_addr.s_addr)), payload);
                }
            }
        }
    }
    close(sid);
    return NULL;
}

void beuip_start(char *pseudo) {
    if (server_running) return;
    strncpy(my_pseudo, pseudo, 255);
    my_pseudo[255] = '\0';
    server_running = 1;
    pthread_create(&server_thread, NULL, serveur_udp, NULL);
}

void beuip_stop(void) {
    if (!server_running) return;
    commande('0', NULL, NULL);
    server_running = 0;
    pthread_cancel(server_thread);
    pthread_join(server_thread, NULL);
    
    pthread_mutex_lock(&table_mutex);
    nb_users = 0;
    pthread_mutex_unlock(&table_mutex);
}

void beuip_liste(void) {
    int i;
    pthread_mutex_lock(&table_mutex);
    printf("---- Table des participants ----\n");
    for (i = 0; i < nb_users; i++) {
        printf(" %d : %s - %s\n", i + 1, addrip(ntohl(table[i].ip)), table[i].pseudo);
    }
    printf("--------------------------------\n");
    pthread_mutex_unlock(&table_mutex);
}

void beuip_mess_pseudo(char *target, char *msg) {
    if (!server_running) return;
    commande('4', msg, target);
}

void beuip_mess_all(char *msg) {
    if (!server_running) return;
    commande('5', msg, NULL);
}