#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 9998
#define LBUF 1024

int main(int N, char*P[]) {
    int sid;
    struct sockaddr_in Sock;
    char msg_out[LBUF+1];
    int msg_len;

    if (N < 2) {
        fprintf(stderr, "Utilisation:\n");
        fprintf(stderr, "  %s liste\n", P[0]);
        fprintf(stderr, "  %s pseudo <nom> <message>\n", P[0]);
        fprintf(stderr, "  %s all <message>\n", P[0]);
        return 1;
    }

    if ((sid=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0) return 2;

    bzero(&Sock, sizeof(Sock));
    Sock.sin_family = AF_INET;
    Sock.sin_addr.s_addr = inet_addr("127.0.0.1");
    Sock.sin_port = htons(PORT);

    if (strcmp(P[1], "liste") == 0) {
        sprintf(msg_out, "3BEUIP");
        msg_len = 6;
    }
    else if (strcmp(P[1], "pseudo") == 0 && N >= 4) {
        int offset = sprintf(msg_out, "4BEUIP%s", P[2]);
        msg_out[offset] = '\0';
        offset++;
        strcpy(msg_out + offset, P[3]);
        msg_len = offset + strlen(P[3]);
    }
    else if (strcmp(P[1], "all") == 0 && N >= 3) {
        sprintf(msg_out, "5BEUIP%s", P[2]);
        msg_len = strlen(msg_out);
    }
    else {
        fprintf(stderr, "Commande invalide.\n");
        return 3;
    }

    sendto(sid, msg_out, msg_len, 0, (struct sockaddr *)&Sock, sizeof(Sock));
    return 0;
}