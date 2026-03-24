/******************************************************************************
 * Laboratoire 5
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 * 
 * Fichier principal
 ******************************************************************************/

#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <sys/select.h>

#include "utils.h"
#include "emulateurClavier.h"
#include "tamponCirculaire.h"

#define TAILLE_BUFFER_LECTURE 4096
#define EOT 0x4


static void* threadFonctionClavier(void* args){
    // Implementez ici votre fonction de thread pour l'ecriture sur le bus USB
    // La premiere des choses est de recuperer les arguments (deja fait pour vous)
    struct infoThreadClavier *infos = (struct infoThreadClavier *)args;
    struct requete req;
    int ret;

    // Vous devez ensuite attendre sur la barriere passee dans les arguments
    // pour etre certain de commencer au meme moment que le thread lecteur
    pthread_barrier_wait(infos->barriere);

    // Finalement, ecrivez dans cette boucle la logique du thread, qui doit:
    // 1) Tenter d'obtenir une requete depuis le tampon circulaire avec consommerDonnee()
    // 2) S'il n'y en a pas, attendre un cours laps de temps (par exemple usleep(500))
    // 3) S'il y en a une, appeler ecrireCaracteres avec les informations requises
    // 4) Liberer la memoire du champ data de la requete avec la fonction free(), puisque
    //      la requete est maintenant terminee

    while(1){
        ret = consommerDonnee(&req);
        if(ret == 1){
            ecrireCaracteres(infos->pointeurClavier, req.data, req.taille, infos->tempsTraitementParCaractereMicroSecondes);
            free(req.data);
        } else {
            usleep(500);
        }
    }
    return NULL;
}

static void* threadFonctionLecture(void *args){

    // Implementez ici votre fonction de thread pour la lecture sur le named pipe
    // La premiere des choses est de recuperer les arguments (deja fait pour vous)
    struct infoThreadLecture *infos = (struct infoThreadLecture *)args;
    
    // Ces champs vous seront utiles pour l'appel a select()
    fd_set setFd;
    int nfds = infos->pipeFd + 1;

    char buffer[TAILLE_BUFFER_LECTURE];
    char *msgBuffer = NULL;
    size_t msgLen = 0;

    // Vous devez ensuite attendre sur la barriere passee dans les arguments
    // pour etre certain de commencer au meme moment que le thread lecteur
    pthread_barrier_wait(infos->barriere);

    // Finalement, ecrivez dans cette boucle la logique du thread, qui doit:
    // 1) Remplir setFd en utilisant FD_ZERO et FD_SET correctement, pour faire en sorte
    //      d'attendre sur infos->pipeFd
    // 2) Appeler select(), sans timeout, avec setFd comme argument de lecture (on veut bien
    //      lire sur le pipe)
    // 3) Lire les valeurs sur le named pipe
    // 4) Si une de ses valeurs est le caracteres ASCII EOT (0x4), alors c'est la fin d'un
    //      message. Vous creez alors une nouvelle requete et utilisez insererDonnee() pour
    //      l'inserer dans le tampon circulaire. Notez que le caractere EOT ne doit PAS se
    //      retrouver dans le champ data de la requete! N'oubliez pas egalement de donner
    //      la bonne valeur aux champs taille et tempsReception.

    while(1){
        FD_ZERO(&setFd);
        FD_SET(infos->pipeFd, &setFd);

        int selectRet = select(nfds, &setFd, NULL, NULL, NULL);
        if(selectRet <= 0){
            continue;
        }

        if(FD_ISSET(infos->pipeFd, &setFd)){
            ssize_t nbLus = read(infos->pipeFd, buffer, sizeof(buffer));
            if(nbLus <= 0){
                continue;
            }

            for(ssize_t i = 0; i < nbLus; i++){
                if(buffer[i] == EOT){
                    if(msgLen > 0){
                        struct requete req;
                        req.data = msgBuffer;
                        req.taille = msgLen;
                        req.tempsReception = get_time();
                        insererDonnee(&req);
                    }
                    msgBuffer = NULL;
                    msgLen = 0;
                } else {
                    msgBuffer = realloc(msgBuffer, msgLen + 1);
                    msgBuffer[msgLen] = buffer[i];
                    msgLen++;
                }
            }
        }
    }
    return NULL;
}

int main(int argc, char* argv[]){
    if(argc < 4){
        printf("Pas assez d'arguments! Attendu : ./emulateurClavier cheminPipe tempsAttenteParPaquet tailleTamponCirculaire\n");
        return 1;
    }

    // A ce stade, vous pouvez consider que:
    // argv[1] contient un chemin valide vers un named pipe
    // argv[2] contient un entier valide (que vous pouvez convertir avec atoi()) representant le nombre de microsecondes a
    //      attendre a chaque envoi de paquet
    // argv[3] contient un entier valide (que vous pouvez convertir avec atoi()) contenant la taille voulue pour le tampon
    //      circulaire

    unsigned int tempsAttente = (unsigned int)atoi(argv[2]);
    int tailleTampon = atoi(argv[3]);

    // Vous avez plusieurs taches d'initialisation a faire :
    //
    // 1) Ouvrir le named pipe
    int pipeFd = open(argv[1], O_RDONLY);
    if(pipeFd < 0){
        perror("Erreur ouverture named pipe");
        return 1;
    }

    // 2) Declarer et initialiser la barriere
    pthread_barrier_t barriere;
    pthread_barrier_init(&barriere, NULL, 2);

    // 3) Initialiser le tampon circulaire avec la bonne taille
    if(initTamponCirculaire((size_t)tailleTampon) != 0){
        fprintf(stderr, "Erreur initialisation tampon circulaire\n");
        close(pipeFd);
        return 1;
    }

    // 4) Creer et lancer les threads clavier et lecteur, en leur passant les bons arguments dans leur struct de configuration respective
    FILE* periphClavier = initClavier();
    if(periphClavier == NULL){
        fprintf(stderr, "Erreur initialisation clavier\n");
        close(pipeFd);
        return 1;
    }

    struct infoThreadClavier infosClavier;
    infosClavier.pointeurClavier = periphClavier;
    infosClavier.tempsTraitementParCaractereMicroSecondes = tempsAttente;
    infosClavier.barriere = &barriere;

    struct infoThreadLecture infosLecture;
    infosLecture.pipeFd = pipeFd;
    infosLecture.barriere = &barriere;

    pthread_t threadClavier, threadLecture;
    pthread_create(&threadClavier, NULL, threadFonctionClavier, &infosClavier);
    pthread_create(&threadLecture, NULL, threadFonctionLecture, &infosLecture);

    // La boucle de traitement est deja implementee pour vous. Toutefois, si vous voulez eviter l'affichage des statistiques
    // (qui efface le terminal a chaque fois), vous pouvez commenter la ligne afficherStats().
    struct statistiques stats;
    double tempsDebut = get_time();
    while(1){
        // Affichage des statistiques toutes les 2 secondes
        calculeStats(&stats);
        afficherStats((unsigned int)(round(get_time() - tempsDebut)), &stats);
        resetStats();
        usleep(2e6);
    }
    return 0;
}
