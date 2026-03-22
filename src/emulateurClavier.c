/******************************************************************************
 * Laboratoire 5
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de l'emulateur de clavier
 ******************************************************************************/

#include "emulateurClavier.h"

FILE* initClavier(){
    // Deja implementee pour vous
    FILE* f = fopen(FICHIER_CLAVIER_VIRTUEL, "wb");
    setbuf(f, NULL);        // On desactive le buffering pour eviter tout delai
    return f;
}

// Cette fonction convertit un caractere ASCII en code de touche HID et modificateur correspondant
// Si le caractere n'est pas supporte, elle retourne -1. 
// Sinon, elle retourne 0 et remplit les variables modificateur et codeTouche pointé par les arguments.
static int convertirAsciiVersHid(char caractere, unsigned char* modificateur, unsigned char* codeTouche){

    // Les lettres minuscules de a a z sont consecutives dans la table ASCII et dans la table HID, débute à 4 dans la table HID
    if(caractere >= 'a' && caractere <= 'z'){
        *modificateur = 0;
        *codeTouche = (unsigned char)(4 + (caractere - 'a'));
        return 0;
    }

    // Les lettres majuscules sont les memes que les minuscules, mais avec le modificateur de majuscule (bit 1) actif
    if(caractere >= 'A' && caractere <= 'Z'){
        *modificateur = 2;
        *codeTouche = (unsigned char)(4 + (caractere - 'A'));
        return 0;
    }

    // Les chiffres de 1 a 9 sont consecutifs dans la table ASCII et dans la table HID, mais pas le '0', débute à 30 dans la table HID
    if(caractere >= '1' && caractere <= '9'){
        *modificateur = 0;
        *codeTouche = (unsigned char)(30 + (caractere - '1'));
        return 0;
    }

    // Le '0' est un cas particulier, car il vient apres le '9' dans la table ASCII, mais avant dans la table HID
    if(caractere == '0'){
        *modificateur = 0;
        *codeTouche = 39;
        return 0;
    }

    // Certains caracteres speciaux sont aussi supportes, mais pas tous.
    switch(caractere){
        case '\n':
            *modificateur = 0;
            *codeTouche = 40;
            return 0;
        case ' ':
            *modificateur = 0;
            *codeTouche = 44;
            return 0;
        case ',':
            *modificateur = 0;
            *codeTouche = 54;
            return 0;
        case '.':
            *modificateur = 0;
            *codeTouche = 55;
            return 0;
        // Par défaut, si le caractère n'est pas supporté, on retourne -1
        default:
            return -1;
    }
}


int ecrireCaracteres(FILE* periphClavier, const char* caracteres, size_t len, unsigned int tempsTraitementParPaquetMicroSecondes){
    // TODO ecrivez votre code ici. Voyez les explications dans l'enonce et dans
    // emulateurClavier.h
    unsigned char paquet[LONGUEUR_USB_PAQUET] = {0};  // Un paquet HID de 8 octets, initialisé à zéro (aucune touche pressée) pour commencer
    unsigned char paquetRelachement[LONGUEUR_USB_PAQUET] = {0}; // Un paquet de relâchement, qui est simplement un paquet vide (aucune touche pressée)
    size_t nbCaracteresEcrits = 0; // Compteur du nombre de caractères écrits, qui sera retourné à la fin de la fonction
    size_t index = 0; // Index pour parcourir la chaîne de caractères à écrire

    if(periphClavier == NULL || caracteres == NULL){ // Vérification des arguments, on retourne -1 en cas d'erreur
        return -1;
    }

    while(index < len){ // Tant qu'il reste des caractères à écrire
        unsigned char modificateur = 0;
        unsigned char codeTouche = 0;
        size_t nbTouchesDansPaquet = 0;

        memset(paquet, 0, sizeof(paquet)); // Réinitialisation du paquet à zéro pour chaque nouveau paquet à envoyer
        if(convertirAsciiVersHid(caracteres[index], &modificateur, &codeTouche) != 0){ // Si le caractère n'est pas supporté, on passe au suivant
            index++;
            continue;
        }

        paquet[0] = modificateur; // Le premier octet du paquet est le modificateur, qui indique les touches de modification (comme majuscule, ctrl, alt, etc.)
        paquet[2 + nbTouchesDansPaquet] = codeTouche; // Les octets suivants du paquet (à partir de l'index 2) sont les codes de touche des touches pressées. On commence à l'index 2 car les index 0 et 1 sont réservés pour les modificateurs et une valeur de padding.
        nbTouchesDansPaquet++; // On incrémente le nombre de touches dans le paquet, qui nous permet de savoir à quel index du paquet placer la prochaine touche si elle a le même modificateur
        index++;

        // On essaie d'ajouter autant de caractères que possible dans le même paquet tant qu'ils ont le même modificateur et que nous n'avons pas atteint la limite de 6 touches par paquet
        while(index < len && nbTouchesDansPaquet < 6){
            unsigned char prochainModificateur = 0;
            unsigned char prochainCodeTouche = 0;

            // Si le prochain caractère n'est pas supporté, on l'ignore et on passe au suivant
            if(convertirAsciiVersHid(caracteres[index], &prochainModificateur, &prochainCodeTouche) != 0){
                index++;
                continue;
            }

            // Si le prochain caractère a un modificateur différent du modificateur actuel, 
            //  on ne peut pas l'ajouter dans le même paquet, car un paquet ne peut avoir qu'un seul modificateur. 
            // On doit alors envoyer le paquet actuel avant de commencer un nouveau paquet pour le prochain caractère.
            if(prochainModificateur != modificateur){
                break;
            }

            // Si le prochain caractère a le même modificateur, on peut l'ajouter dans le même paquet tant que nous n'avons pas 
            // atteint la limite de 6 touches par paquet
            paquet[2 + nbTouchesDansPaquet] = prochainCodeTouche;
            nbTouchesDansPaquet++;
            index++;
        }

        // Une fois que nous avons rempli le paquet avec autant de caractères que possible, nous l'envoyons au périphérique de clavier virtuel en écrivant les 8 octets du paquet dans le fichier correspondant.
        if(fwrite(paquet, LONGUEUR_USB_PAQUET, 1, periphClavier) != 1){
            return -1;
        }

        // Après avoir envoyé le paquet de touche, nous devons envoyer un paquet de relâchement pour simuler le relâchement des touches.
        if(fwrite(paquetRelachement, LONGUEUR_USB_PAQUET, 1, periphClavier) != 1){
            return -1;
        }

        // Ensuite, nous attendons le temps de traitement par paquet spécifié avant de continuer à envoyer les prochains caractères.
        usleep(tempsTraitementParPaquetMicroSecondes);
        nbCaracteresEcrits += nbTouchesDansPaquet;
    }

    return (int)nbCaracteresEcrits;
}
