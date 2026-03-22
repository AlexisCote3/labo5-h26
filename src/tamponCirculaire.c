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
    // On alloue la memoire pour le tampon circulaire, qui doit pouvoir contenir 'taille' elements de type struct requete. Si l'allocation echoue, on retourne -1 pour indiquer l'erreur.
    memoire = calloc(taille, sizeof(struct requete));
    if(memoire == NULL){
        return -1;
    }

    // Initialisation des positions de lecture, d'ecriture et de longueur courante du tampon circulaire. 
    // La position de lecture et d'ecriture commencent a 0, et la longueur courante est 0.
    memoireTaille = taille;
    posLecture = 0;
    posEcriture = 0;
    longueurCourante = 0;

    // Initialisation du mutex, et verification de la reussite de celle-ci. En cas d'erreur, on libere la memoire allouee et on retourne -1 pour indiquer l'erreur.
    if(pthread_mutex_init(&mutexTampon, NULL) != 0){
        free(memoire);
        memoire = NULL;
        return -1;
    }

    // Initialisation des variables de statistiques en appelant la fonction resetStats(), qui reinitialise les variables de statistique et fixe le temps de debut de periode a l'instant actuel.
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
    double dureePeriode; // Variable pour stocker la duree de la periode de mesure des statistiques, qui est calculee en soustrayant le temps de debut de periode du temps actuel.

    if(stats == NULL){
        return;
    }

    // On doit proteger les operations de lecture des variables partagees par le mutex, pour eviter les conditions de course et garantir la coherence des statistiques calculees.
    pthread_mutex_lock(&mutexTampon);

    //  Calcul de la duree de la periode de mesure des statistiques en soustrayant le temps de debut de periode du temps actuel.
    // Si la duree de periode est negative ou nulle (ce qui peut arriver si le temps actuel est egal ou inferieur au temps de debut de periode, 
    // par exemple en cas d'erreur dans la fonction get_time()), 
    // on fixe la duree de periode a 1.0 pour eviter les divisions par zero ou les statistiques incoherentes.
    dureePeriode = get_time() - tempsDebutPeriode;
    if(dureePeriode <= 0.0){
        dureePeriode = 1.0;
    }

    // Calcul des statistiques en utilisant les variables partagees. On calcule le nombre de requetes en attente, 
    // le nombre de requetes traitees, le nombre de requetes perdues, 
    // le temps de traitement moyen (en divisant la somme des temps d'attente par le nombre de requetes traitees,
    // en s'assurant de ne pas diviser par zero), 
    // le taux d'arrivee lambda (en divisant le nombre de requetes recues par la duree de periode), 
    // le taux de service mu (en divisant le nombre de requetes traitees par la duree de periode), 
    // et le taux d'utilisation rho (en divisant lambda par mu, 
    // en s'assurant que mu est superieur a zero pour eviter les divisions par zero). 
    // Les statistiques calculees sont ensuite stockees dans la structure 'stats' passee en argument.
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

    // Verification des arguments, on retourne -1 en cas d'erreur (par exemple, 
    // si req est NULL, ou si la memoire n'est pas allouee ou si la taille de memoire est nulle).
    if(req == NULL || memoire == NULL || memoireTaille == 0){
        return -1;
    }

    pthread_mutex_lock(&mutexTampon);

    // On determine l'endroit ou copier la requete dans le tampon circulaire en utilisant la position d'ecriture actuelle (posEcriture).
    tampon = (struct requete*)memoire;
    nombreRequetesRecues++;

    // Si le tampon circulaire est plein (c'est a dire, si la longueur courante est egale a la taille de memoire), 
    // alors on doit "mordre la queue" du tampon en ecrasant la requete la plus ancienne (celle a la position d'ecriture actuelle,
    // qui est aussi la position de lecture actuelle dans ce cas), 
    // et on doit mettre a jour posLecture pour que le prochain element lu soit le plus ancien. 
    // On doit aussi incrementer le nombre de requetes perdues pour les statistiques.
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

    // On determine si une requete est disponible dans le tampon circulaire en verifiant si la longueur courante est superieure a zero. 
    //Si elle est egale a zero, cela signifie que le tampon est vide et qu'il n'y a pas de requete a consommer, donc on retourne 0.
    if(longueurCourante == 0){
        pthread_mutex_unlock(&mutexTampon);
        return 0;
    }

    // S'il y en a une, alors on copie cette requete dans la structure passee en argument (req),
    // on modifie la valeur de posLecture en l'incrementant de 1
    tampon = (struct requete*)memoire;
    *req = tampon[posLecture];
    posLecture = (posLecture + 1) % memoireTaille;
    longueurCourante--; // on modifie la valeur de longueurCourante en la decrementant de 1, car on a consomme une requete du tampon circulaire
    nombreRequetesTraitees++; // on incremente le nombre de requetes traitees pour les statistiques
    sommeTempsAttente += get_time() - req->tempsReception; // On met à jour somme temps d'attente

    pthread_mutex_unlock(&mutexTampon);

    return 1;
}

unsigned int longueurFile(){
    // Retourne la longueur courante de la file contenue dans votre tampon circulaire.
    
    // TODO
    unsigned int longueur;
    // On va chercher la logneur courante du tampon circulaire en accedant a la variable partagee longueurCourante.
    pthread_mutex_lock(&mutexTampon);
    longueur = longueurCourante;
    pthread_mutex_unlock(&mutexTampon);

    return longueur;
}
