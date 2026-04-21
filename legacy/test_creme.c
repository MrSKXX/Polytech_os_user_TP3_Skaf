#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "creme.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Utilisation: %s pseudo\n", argv[0]);
        return 1;
    }

    beuip_start(argv[1]);
    
    sleep(2);

    beuip_liste();
    
    sleep(2);

    beuip_mess_all("Salut la classe, j'utilise la librairie creme !");
    
    sleep(10);

    beuip_stop();
    return 0;
}