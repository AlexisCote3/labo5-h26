/******************************************************************************
 * Laboratoire 5
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de gestion du tampon circulaire
 ******************************************************************************/

#include "tamponCirculaire.h"

// Plusieurs variables globales statiques (pour qu'elles ne soient accessible que dans les
// fonctions de ce fichier) sont declarees ici. Elle servent a conserver l'etat du tampon
// circulaire ainsi qu'a mesurer certains elements utiles au calcul des statistiques.
// Vous etes libres d'en creer d'autres si vous en voyez le besoin.

// Pointe vers la memoire allouee pour le tampon circulaire
static char* memoire;

// Taile du tampon circulaire (en nombre d'elements de type struct requete)
static size_t memoireTaille;

// Positions de lecture et d'ecriture, et longueur actuelle du tampon circulaire
static unsigned int posLecture, posEcriture, longueurCourante;

// Mutex permettant de proteger les acces au tampon circulaire
// N'oubliez pas que _deux_ threads vont tenter de faire des operations en parallele!
pthread_mutex_t mutexTampon;

// Pour les statistiques
static unsigned int nombreRequetesRecues, nombreRequetesTraitees, nombreRequetesPerdues;

// On calcule les statistiques par période de N secondes.
// Le tempsDebutPeriode permet de se rappeler du temps de debut de la periode où les statistiques sont mesurées
// La variable sommeTempsAttente contient la somme de toutes les periodes d'attente pour les requetes
// (vous pourrez donc calculer la moyenne du temps d'attente en utilisant les autres variables sur le
// nombre de requetes).
static double tempsDebutPeriode, sommeTempsAttente;

int initTamponCirculaire(size_t taille){
    // Initialisez ici:
    // La memoire, en utilisant malloc ou calloc (rappelez-vous que votre tampon circulaire doit
    // pouvoir contenir _taille_ fois la taille d'une struct requete)
    //
    // Les positions de lecture, d'ecriture et de longueur courante.
    //
    // Le mutex
    //
    // Les variables de statistiques

    // TODO
    memoire = calloc(taille, sizeof(struct requete));
    if(memoire == NULL){
        return -1;
    }

    memoireTaille = taille;
    posLecture = 0;
    posEcriture = 0;
    longueurCourante = 0;

    if(pthread_mutex_init(&mutexTampon, NULL) != 0){
        free(memoire);
        memoire = NULL;
        return -1;
    }

    resetStats();

    return 0;

}

void resetStats(){
    // Reinitialise les variables de statistique

    // TODO
    nombreRequetesRecues = 0;
    nombreRequetesTraitees = 0;
    nombreRequetesPerdues = 0;
    sommeTempsAttente = 0.0;
    tempsDebutPeriode = get_time();
}

void calculeStats(struct statistiques *stats){
    // TODO
    double dureePeriode;

    if(stats == NULL){
        return;
    }

    pthread_mutex_lock(&mutexTampon);

    dureePeriode = get_time() - tempsDebutPeriode;
    if(dureePeriode <= 0.0){
        dureePeriode = 1.0;
    }

    stats->nombreRequetesEnAttente = longueurCourante;
    stats->nombreRequetesTraitees = nombreRequetesTraitees;
    stats->nombreRequetesPerdues = nombreRequetesPerdues;
    stats->tempsTraitementMoyen = (nombreRequetesTraitees > 0)
        ? (sommeTempsAttente / (double)nombreRequetesTraitees)
        : 0.0;
    stats->lambda = (double)nombreRequetesRecues / dureePeriode;
    stats->mu = (double)nombreRequetesTraitees / dureePeriode;
    stats->rho = (stats->mu > 0.0) ? (stats->lambda / stats->mu) : 0.0;

    resetStats();

    pthread_mutex_unlock(&mutexTampon);
}

int insererDonnee(struct requete *req){
    // Dans cette fonction, vous devez :
    //
    // Determiner a quel endroit copier la requete req dans le tampon circulaire
    //
    // Copier celle-ci
    //
    // Mettre a jour posEcriture et longueurCourante (toujours) et possiblement
    // posLecture (si vous vous etes "mordu la queue" et que vous etes revenu au
    // debut de votre tampon circulaire, il faut aussi repousser le pointeur de lecture
    // pour que le prochain element lu soit le plus ancien!)
    //
    // Mettre a jour les variables necessaires aux statistiques (comme nombreRequetesRecues, par exemple)
    //
    // N'oubliez pas de proteger les operations qui le necessitent par un mutex!
   
    // TODO
    struct requete* tampon;

    if(req == NULL || memoire == NULL || memoireTaille == 0){
        return -1;
    }

    pthread_mutex_lock(&mutexTampon);

    tampon = (struct requete*)memoire;
    nombreRequetesRecues++;

    if(longueurCourante == memoireTaille){
        free(tampon[posEcriture].data);
        tampon[posEcriture] = *req;
        posEcriture = (posEcriture + 1) % memoireTaille;
        posLecture = posEcriture;
        nombreRequetesPerdues++;
    }else{
        tampon[posEcriture] = *req;
        posEcriture = (posEcriture + 1) % memoireTaille;
        longueurCourante++;
    }

    pthread_mutex_unlock(&mutexTampon);

    return 0;
}

int consommerDonnee(struct requete *req){
    // Dans cette fonction, vous devez :
    //
    // Determiner si une requete est disponible dans le tampon circulaire
    //
    // S'il n'y en a _pas_, retourner 0.
    //
    // S'il y en a une, alors :
    //      Copier cette requete dans la structure passee en argument
    //      Modifier la valeur de posLecture et longueurCourante
    //      Mettre a jour les variables necessaires aux statistiques (comme sommeTempsAttente)
    //      Retourner 1 pour indiquer qu'une requete disponible a ete copiee dans req.
    //
    // N'oubliez pas de proteger les operations qui le necessitent par un mutex!
    
    // TODO
    struct requete* tampon;

    if(req == NULL || memoire == NULL){
        return -1;
    }

    pthread_mutex_lock(&mutexTampon);

    if(longueurCourante == 0){
        pthread_mutex_unlock(&mutexTampon);
        return 0;
    }

    tampon = (struct requete*)memoire;
    *req = tampon[posLecture];
    posLecture = (posLecture + 1) % memoireTaille;
    longueurCourante--;
    nombreRequetesTraitees++;
    sommeTempsAttente += get_time() - req->tempsReception;

    pthread_mutex_unlock(&mutexTampon);

    return 1;
}

unsigned int longueurFile(){
    // Retourne la longueur courante de la file contenue dans votre tampon circulaire.
    
    // TODO
    unsigned int longueur;

    pthread_mutex_lock(&mutexTampon);
    longueur = longueurCourante;
    pthread_mutex_unlock(&mutexTampon);

    return longueur;
}
