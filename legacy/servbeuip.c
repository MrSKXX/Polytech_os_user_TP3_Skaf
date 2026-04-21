#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>

#define PORT 9998
#define LBUF 1024
#define MAX_USERS 255

struct User {
    unsigned long ip;
    char pseudo[256];
};

struct User table[MAX_USERS];
int nb_users = 0;
int sid;
char my_pseudo[256];

char * addrip(unsigned long A) {
    static char b[16];
    sprintf(b,"%u.%u.%u.%u",(unsigned int)(A>>24&0xFF),(unsigned int)(A>>16&0xFF),
           (unsigned int)(A>>8&0xFF),(unsigned int)(A&0xFF));
    return b;
}

void print_table(void) {
    int i;
    printf("---- Table des participants ----\n");
    for (i = 0; i < nb_users; i++) {
        printf(" %d : %s - %s\n", i + 1, addrip(ntohl(table[i].ip)), table[i].pseudo);
    }
    printf("--------------------------------\n");
}

void handle_sig(int sig) {
    struct sockaddr_in SockBcast;
    char msg_out[LBUF+1];
    (void)sig;
    bzero(&SockBcast, sizeof(SockBcast));
    SockBcast.sin_family = AF_INET;
    SockBcast.sin_addr.s_addr = inet_addr("192.168.88.255");
    SockBcast.sin_port = htons(PORT);
    sprintf(msg_out, "0BEUIP%s", my_pseudo);
    sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&SockBcast, sizeof(SockBcast));
#ifdef TRACE
    fprintf(stderr, "[TRACE] broadcast depart envoye\n");
#endif
    exit(0);
}

void add_user(unsigned long ip_addr, char *pseudo) {
    int i;
    for (i = 0; i < nb_users; i++) {
        if (table[i].ip == ip_addr && strcmp(table[i].pseudo, pseudo) == 0) return;
    }
    if (nb_users < MAX_USERS) {
        table[nb_users].ip = ip_addr;
        strncpy(table[nb_users].pseudo, pseudo, 255);
        table[nb_users].pseudo[255] = '\0';
        nb_users++;
#ifdef TRACE
        fprintf(stderr, "[TRACE] ajout: %s pseudo=%s (%d couples)\n",
                addrip(ntohl(ip_addr)), pseudo, nb_users);
#endif
    }
}

void remove_user(unsigned long ip_addr, char *pseudo) {
    int i, j;
    for (i = 0; i < nb_users; i++) {
        if (table[i].ip == ip_addr && strcmp(table[i].pseudo, pseudo) == 0) {
#ifdef TRACE
            fprintf(stderr, "[TRACE] suppression: %s pseudo=%s\n",
                    addrip(ntohl(ip_addr)), pseudo);
#endif
            for (j = i; j < nb_users - 1; j++) {
                table[j] = table[j+1];
            }
            nb_users--;
            break;
        }
    }
}

int main(int N, char*P[]) {
    struct sockaddr_in Sock, SockConf, SockBcast;
    socklen_t ls;
    char buf[LBUF+1];
    char msg_out[LBUF+1];
    int on = 1;
    int n;
    unsigned long loc_ip;

    if (N != 2) {
        fprintf(stderr, "Utilisation : %s pseudo\n", P[0]);
        return 1;
    }

    strncpy(my_pseudo, P[1], 255);
    my_pseudo[255] = '\0';

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    if ((sid=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0) return 2;
    if (setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) return 3;

    bzero(&SockConf, sizeof(SockConf));
    SockConf.sin_family = AF_INET;
    SockConf.sin_addr.s_addr = htonl(INADDR_ANY);
    SockConf.sin_port = htons(PORT);

    if (bind(sid, (struct sockaddr *) &SockConf, sizeof(SockConf)) == -1) return 4;

    printf("servbeuip: %s attache au port %d\n", my_pseudo, PORT);

    bzero(&SockBcast, sizeof(SockBcast));
    SockBcast.sin_family = AF_INET;
    SockBcast.sin_addr.s_addr = inet_addr("192.168.88.255");
    SockBcast.sin_port = htons(PORT);

    sprintf(msg_out, "1BEUIP%s", my_pseudo);
    sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&SockBcast, sizeof(SockBcast));
#ifdef TRACE
    fprintf(stderr, "[TRACE] broadcast identification envoye\n");
#endif

    loc_ip = inet_addr("127.0.0.1");
    add_user(loc_ip, my_pseudo);

    for (;;) {
        ls = sizeof(Sock);
        if ((n=recvfrom(sid,(void*)buf,LBUF,0,(struct sockaddr *)&Sock,&ls)) > 0) {
            buf[n] = '\0';
            if (n >= 6 && strncmp(buf+1, "BEUIP", 5) == 0) {
                char code = buf[0];
                char *payload = buf + 6;

#ifdef TRACE
                fprintf(stderr, "[TRACE] recu code='%c' de %s\n",
                        code, addrip(ntohl(Sock.sin_addr.s_addr)));
#endif

                if (code == '1' || code == '2') {
                    printf("Message recu de %s : code=%c pseudo=%s\n", addrip(ntohl(Sock.sin_addr.s_addr)), code, payload);
                    add_user(Sock.sin_addr.s_addr, payload);
                    print_table();
                    
                    if (code == '1') {
                        printf("AR envoye a %s\n", addrip(ntohl(Sock.sin_addr.s_addr)));
                        sprintf(msg_out, "2BEUIP%s", my_pseudo);
                        sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&Sock, ls);
                    }
                }
                else if (code == '0') {
                    printf("Deconnexion de %s\n", payload);
                    remove_user(Sock.sin_addr.s_addr, payload);
                    print_table();
                }
                else if (code == '9') {
                    int i, found = 0;
                    for (i = 0; i < nb_users; i++) {
                        if (table[i].ip == Sock.sin_addr.s_addr) {
                            printf("Message de %s : %s\n", table[i].pseudo, payload);
                            found = 1;
                            break;
                        }
                    }
                    if (!found) printf("Message d'inconnu (%s) : %s\n", addrip(ntohl(Sock.sin_addr.s_addr)), payload);
                }
                else if (Sock.sin_addr.s_addr == loc_ip) {
                    if (code == '3') {
                        print_table();
                    }
                    else if (code == '4') {
                        char *target = payload;
                        char *message = payload + strlen(payload) + 1;
                        int i, found = 0;
                        for (i = 0; i < nb_users; i++) {
                            if (strcmp(table[i].pseudo, target) == 0) {
                                struct sockaddr_in dest;
                                bzero(&dest, sizeof(dest));
                                dest.sin_family = AF_INET;
                                dest.sin_addr.s_addr = table[i].ip;
                                dest.sin_port = htons(PORT);
                                sprintf(msg_out, "9BEUIP%s", message);
                                sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
                                printf("Message prive envoye a %s (%s)\n", target, addrip(ntohl(table[i].ip)));
                                found = 1;
#ifdef TRACE
                                fprintf(stderr, "[TRACE] msg prive -> %s\n", target);
#endif
                                break;
                            }
                        }
                        if (!found) printf("Erreur: pseudo %s introuvable.\n", target);
                    }
                    else if (code == '5') {
                        int i;
                        sprintf(msg_out, "9BEUIP%s", payload);
                        for (i = 0; i < nb_users; i++) {
                            if (table[i].ip != loc_ip) {
                                struct sockaddr_in dest;
                                bzero(&dest, sizeof(dest));
                                dest.sin_family = AF_INET;
                                dest.sin_addr.s_addr = table[i].ip;
                                dest.sin_port = htons(PORT);
                                sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
                            }
                        }
                        printf("Message broadcast envoye a tous.\n");
#ifdef TRACE
                        fprintf(stderr, "[TRACE] msg broadcast envoye\n");
#endif
                    }
                }
            }
        }
    }
    return 0;
}