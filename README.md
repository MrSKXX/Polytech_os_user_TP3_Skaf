NOM: SKAF, EL SHAZLY
PRENOM: Georges, Ahmed

# BICEPS - Bel Interpréteur de Commandes des Élèves de Polytech Sorbonne

Interpréteur de commandes intégrant le protocole BEUIP (UDP) pour la
messagerie réseau décentralisée et un serveur TCP pour l'échange de fichiers.

## Fonctionnalités

### Shell
- Commandes internes : `exit`, `cd`, `pwd`, `vers`, `beuip`
- Commandes externes via `fork` + `execvp`
- Pipelines avec `|`
- Redirections `<`, `>`, `>>`, `2>`, `2>>`
- Séquences avec `;`
- Historique persistant (`~/.biceps_history`) via readline
- Ctrl-C ignoré dans le shell, rétabli dans les commandes filles
- Sortie via `exit` ou Ctrl-D

### Protocole BEUIP (UDP port 9998)
- Auto-détection des interfaces réseau via `getifaddrs`
- Broadcast complémentaire sur l'adresse définie par `BROADCAST_IP`
- Codes : `0` (départ), `1` (identification), `2` (AR), `9` (message)
- Serveur dans un thread séparé, synchronisation par mutex
- Liste chaînée triée par pseudo

### Échange de fichiers (TCP port 9998)
- Thread serveur TCP dédié
- `beuip ls <pseudo>` : liste les fichiers du répertoire `reppub/` distant
- `beuip get <pseudo> <fichier>` : télécharge un fichier distant

## Structure du code

- `biceps.c` : point d'entrée, boucle REPL, commandes internes
- `gescom.c` / `gescom.h` : analyse des commandes, exécution, pipes, redirections
- `creme.c` / `creme.h` : serveurs UDP/TCP, liste chaînée, protocole BEUIP

## Compilation

| Commande | Résultat |
|----------|----------|
| `make` | produit l'exécutable `biceps` |
| `make memory-leak` | produit `biceps-memory-leaks` avec `-g -O0` |
| `make clean` | supprime tous les binaires et fichiers objets |

## Commandes BEUIP

| Commande | Description |
|----------|-------------|
| `beuip start <pseudo>` | Démarre les serveurs UDP et TCP |
| `beuip stop` | Arrête proprement les serveurs |
| `beuip list` | Affiche les utilisateurs connectés au format `IP : pseudo` |
| `beuip message <user> <message>` | Envoie un message privé |
| `beuip message all <message>` | Diffuse un message à tous les utilisateurs |
| `beuip ls <pseudo>` | Liste les fichiers du `reppub/` distant |
| `beuip get <pseudo> <fichier>` | Télécharge un fichier dans le `reppub/` local |

## Vérification mémoire

```
make memory-leak
valgrind --leak-check=full --track-origins=yes --error-exitcode=1 ./biceps-memory-leaks
```

Aucune fuite provenant du code utilisateur (`definitely lost`, `indirectly lost`
et `possibly lost` sont tous à 0). Les blocs `still reachable` reportés sont
intrinsèques à la bibliothèque `readline` et à `libtinfo` (initialisation du
terminal non libérée par readline), hors du contrôle du code utilisateur.

## Adresse broadcast

L'adresse `192.168.88.255` est définie dans un unique `#define BROADCAST_IP`
dans `creme.h` et peut être modifiée à cet endroit unique.