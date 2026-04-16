#define _GNU_SOURCE

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
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "creme.h"

#define PORT 9998
#define LBUF 1024
#define LPSEUDO 23

struct elt {
    char nom[LPSEUDO+1];
    char adip[16];
    struct elt * next;
};

static struct elt *liste_utilisateurs = NULL;
static int sid = -1;
static int tcp_sid = -1;
static char my_pseudo[LPSEUDO+1];
static pthread_t server_thread;
static pthread_t tcp_thread;
static int server_running = 0;
static pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

static char * addrip(unsigned long A) {
    static char b[16];
    sprintf(b,"%u.%u.%u.%u",(unsigned int)(A>>24&0xFF),(unsigned int)(A>>16&0xFF),
           (unsigned int)(A>>8&0xFF),(unsigned int)(A&0xFF));
    return b;
}

static void ajouteElt(char *pseudo, char *adip) {
    struct elt *courant, *precedent, *nouveau;
    pthread_mutex_lock(&table_mutex);
    courant = liste_utilisateurs;
    while (courant != NULL) {
        if (strcmp(courant->adip, adip) == 0 && strcmp(courant->nom, pseudo) == 0) {
            pthread_mutex_unlock(&table_mutex);
            return;
        }
        courant = courant->next;
    }
    nouveau = malloc(sizeof(struct elt));
    if (!nouveau) {
        pthread_mutex_unlock(&table_mutex);
        return;
    }
    strncpy(nouveau->nom, pseudo, LPSEUDO);
    nouveau->nom[LPSEUDO] = '\0';
    strncpy(nouveau->adip, adip, 15);
    nouveau->adip[15] = '\0';
    nouveau->next = NULL;

    courant = liste_utilisateurs;
    precedent = NULL;
    while (courant != NULL && strcmp(courant->nom, nouveau->nom) < 0) {
        precedent = courant;
        courant = courant->next;
    }
    if (precedent == NULL) {
        nouveau->next = liste_utilisateurs;
        liste_utilisateurs = nouveau;
    } else {
        nouveau->next = courant;
        precedent->next = nouveau;
    }
    pthread_mutex_unlock(&table_mutex);
}

static void supprimeElt(char *adip) {
    struct elt *courant, *precedent;
    pthread_mutex_lock(&table_mutex);
    courant = liste_utilisateurs;
    precedent = NULL;
    while (courant != NULL) {
        if (strcmp(courant->adip, adip) == 0) {
            if (precedent == NULL) {
                liste_utilisateurs = courant->next;
            } else {
                precedent->next = courant->next;
            }
            free(courant);
            pthread_mutex_unlock(&table_mutex);
            return;
        }
        precedent = courant;
        courant = courant->next;
    }
    pthread_mutex_unlock(&table_mutex);
}

static void listeElts(void) {
    struct elt *courant;
    pthread_mutex_lock(&table_mutex);
    courant = liste_utilisateurs;
    while (courant != NULL) {
        if (strcmp(courant->adip, "127.0.0.1") != 0) {
            printf("%s : %s\n", courant->adip, courant->nom);
        }
        courant = courant->next;
    }
    pthread_mutex_unlock(&table_mutex);
}

static void commande(char octet1, char *message, char *pseudo) {
    struct sockaddr_in dest;
    char msg_out[LBUF];
    struct elt *courant;

    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);

    pthread_mutex_lock(&table_mutex);
    if (octet1 == '0') {
        sprintf(msg_out, "0BEUIP%s", my_pseudo);
        courant = liste_utilisateurs;
        while (courant != NULL) {
            if (strcmp(courant->adip, "127.0.0.1") != 0) {
                dest.sin_addr.s_addr = inet_addr(courant->adip);
                sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
            }
            courant = courant->next;
        }
    } else if (octet1 == '4' && pseudo != NULL) {
        sprintf(msg_out, "9BEUIP%s", message);
        courant = liste_utilisateurs;
        while (courant != NULL) {
            if (strcmp(courant->nom, pseudo) == 0) {
                dest.sin_addr.s_addr = inet_addr(courant->adip);
                sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
                break;
            }
            courant = courant->next;
        }
    } else if (octet1 == '5') {
        sprintf(msg_out, "9BEUIP%s", message);
        courant = liste_utilisateurs;
        while (courant != NULL) {
            if (strcmp(courant->adip, "127.0.0.1") != 0) {
                dest.sin_addr.s_addr = inet_addr(courant->adip);
                sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
            }
            courant = courant->next;
        }
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
    setsockopt(sid, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) return NULL;

    bzero(&SockConf, sizeof(SockConf));
    SockConf.sin_family = AF_INET;
    SockConf.sin_addr.s_addr = htonl(INADDR_ANY);
    SockConf.sin_port = htons(PORT);

    if (bind(sid, (struct sockaddr *) &SockConf, sizeof(SockConf)) == -1) {
        close(sid);
        return NULL;
    }

    sprintf(msg_out, "1BEUIP%s", my_pseudo);
    if (getifaddrs(&ifaddr) != -1) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;
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
    
    bzero(&SockBcast, sizeof(SockBcast));
    SockBcast.sin_family = AF_INET;
    SockBcast.sin_addr.s_addr = inet_addr(BROADCAST_IP);
    SockBcast.sin_port = htons(PORT);
    sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&SockBcast, sizeof(SockBcast));

    ajouteElt(my_pseudo, "127.0.0.1");

    while (server_running) {
        ls = sizeof(Sock);
        if ((n = recvfrom(sid, (void*)buf, LBUF, 0, (struct sockaddr *)&Sock, &ls)) > 0) {
            buf[n] = '\0';
            if (n >= 6 && strncmp(buf+1, "BEUIP", 5) == 0) {
                char code = buf[0];
                char *payload = buf + 6;
                char *sender_ip = addrip(ntohl(Sock.sin_addr.s_addr));

                if (code == '1' || code == '2') {
                    ajouteElt(payload, sender_ip);
                    if (code == '1') {
                        sprintf(msg_out, "2BEUIP%s", my_pseudo);
                        sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&Sock, ls);
                    }
                }
                else if (code == '0') {
                    supprimeElt(sender_ip);
                }
                else if (code == '9') {
                    struct elt *courant;
                    pthread_mutex_lock(&table_mutex);
                    courant = liste_utilisateurs;
                    while (courant != NULL) {
                        if (strcmp(courant->adip, sender_ip) == 0) {
                            printf("\nMessage de %s : %s\n", courant->nom, payload);
                            break;
                        }
                        courant = courant->next;
                    }
                    pthread_mutex_unlock(&table_mutex);
                }
            }
        }
    }
    close(sid);
    return NULL;
}

static void *serveur_tcp(void *p) {
    struct sockaddr_in SockConf;
    int on = 1;

    if ((tcp_sid = socket(AF_INET, SOCK_STREAM, 0)) < 0) return NULL;
    setsockopt(tcp_sid, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&SockConf, sizeof(SockConf));
    SockConf.sin_family = AF_INET;
    SockConf.sin_addr.s_addr = htonl(INADDR_ANY);
    SockConf.sin_port = htons(PORT);

    if (bind(tcp_sid, (struct sockaddr *)&SockConf, sizeof(SockConf)) == -1) {
        close(tcp_sid);
        return NULL;
    }
    listen(tcp_sid, 5);

    while (server_running) {
        struct sockaddr_in client_sock;
        socklen_t l = sizeof(client_sock);
        int fd = accept(tcp_sid, (struct sockaddr *)&client_sock, &l);
        if (fd >= 0) {
            char c;
            if (read(fd, &c, 1) == 1) {
                if (c == 'L') {
                    if (fork() == 0) {
                        if (fork() == 0) {
                            dup2(fd, STDOUT_FILENO);
                            dup2(fd, STDERR_FILENO);
                            close(fd);
                            execlp("ls", "ls", "-l", "reppub/", NULL);
                            exit(1);
                        }
                        exit(0);
                    }
                    wait(NULL);
                } else if (c == 'F') {
                    char fname[256];
                    int i = 0;
                    while (read(fd, &c, 1) == 1 && c != '\n' && i < 255) {
                        fname[i++] = c;
                    }
                    fname[i] = '\0';
                    if (fork() == 0) {
                        if (fork() == 0) {
                            char path[512];
                            dup2(fd, STDOUT_FILENO);
                            dup2(fd, STDERR_FILENO);
                            close(fd);
                            snprintf(path, sizeof(path), "reppub/%s", fname);
                            execlp("cat", "cat", path, NULL);
                            exit(1);
                        }
                        exit(0);
                    }
                    wait(NULL);
                }
            }
            close(fd);
        }
    }
    return NULL;
}

void beuip_start(char *pseudo) {
    if (server_running) return;
    strncpy(my_pseudo, pseudo, LPSEUDO);
    my_pseudo[LPSEUDO] = '\0';
    
    mkdir("reppub", 0755);
    
    server_running = 1;
    pthread_create(&server_thread, NULL, serveur_udp, NULL);
    pthread_create(&tcp_thread, NULL, serveur_tcp, NULL);
}

void beuip_stop(void) {
    struct elt *courant, *suivant;
    if (!server_running) return;
    commande('0', NULL, NULL);
    server_running = 0;
    
    pthread_cancel(server_thread);
    pthread_cancel(tcp_thread);
    pthread_join(server_thread, NULL);
    pthread_join(tcp_thread, NULL);
    if (tcp_sid != -1) close(tcp_sid);
    
    pthread_mutex_lock(&table_mutex);
    courant = liste_utilisateurs;
    while (courant != NULL) {
        suivant = courant->next;
        free(courant);
        courant = suivant;
    }
    liste_utilisateurs = NULL;
    pthread_mutex_unlock(&table_mutex);
}

void beuip_liste(void) {
    listeElts();
}

void beuip_mess_pseudo(char *target, char *msg) {
    if (!server_running) return;
    commande('4', msg, target);
}

void beuip_mess_all(char *msg) {
    if (!server_running) return;
    commande('5', msg, NULL);
}

void beuip_ls(char *pseudo) {
    char ip[16] = "";
    struct elt *c;
    int s;
    struct sockaddr_in sin;
    char buf[1024];
    int n;

    if (!server_running) return;

    pthread_mutex_lock(&table_mutex);
    c = liste_utilisateurs;
    while(c) {
        if(strcmp(c->nom, pseudo) == 0) {
            strcpy(ip, c->adip);
            break;
        }
        c = c->next;
    }
    pthread_mutex_unlock(&table_mutex);

    if (ip[0] == '\0') {
        printf("Erreur: Utilisateur %s introuvable.\n", pseudo);
        return;
    }

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) return;
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = inet_addr(ip);

    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Connexion TCP echouee");
        close(s);
        return;
    }

    write(s, "L", 1);
    while((n = read(s, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, n);
    }
    close(s);
}

void beuip_get(char *pseudo, char *nomfic) {
    char ip[16] = "";
    struct elt *c;
    int s, fd, n;
    struct sockaddr_in sin;
    char path[512];
    char req[512];
    char buf[1024];

    if (!server_running) return;

    pthread_mutex_lock(&table_mutex);
    c = liste_utilisateurs;
    while(c) {
        if(strcmp(c->nom, pseudo) == 0) {
            strcpy(ip, c->adip);
            break;
        }
        c = c->next;
    }
    pthread_mutex_unlock(&table_mutex);

    if (ip[0] == '\0') {
        printf("Erreur: Utilisateur %s introuvable.\n", pseudo);
        return;
    }

    snprintf(path, sizeof(path), "reppub/%s", nomfic);
    if (access(path, F_OK) == 0) {
        printf("Erreur: Le fichier %s existe deja localement.\n", nomfic);
        return;
    }

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) return;
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = inet_addr(ip);

    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Connexion TCP echouee");
        close(s);
        return;
    }

    snprintf(req, sizeof(req), "F%s\n", nomfic);
    write(s, req, strlen(req));

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Erreur d'ouverture fichier local");
        close(s);
        return;
    }

    while((n = read(s, buf, sizeof(buf))) > 0) {
        write(fd, buf, n);
    }
    close(fd);
    close(s);
    printf("Fichier %s telecharge avec succes depuis %s.\n", nomfic, pseudo);
}