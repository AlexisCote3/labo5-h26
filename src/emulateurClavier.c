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

static int convertirAsciiVersHid(char caractere, unsigned char* modificateur, unsigned char* codeTouche){
    if(caractere >= 'a' && caractere <= 'z'){
        *modificateur = 0;
        *codeTouche = (unsigned char)(4 + (caractere - 'a'));
        return 0;
    }

    if(caractere >= 'A' && caractere <= 'Z'){
        *modificateur = 2;
        *codeTouche = (unsigned char)(4 + (caractere - 'A'));
        return 0;
    }

    if(caractere >= '1' && caractere <= '9'){
        *modificateur = 0;
        *codeTouche = (unsigned char)(30 + (caractere - '1'));
        return 0;
    }

    if(caractere == '0'){
        *modificateur = 0;
        *codeTouche = 39;
        return 0;
    }

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
        default:
            return -1;
    }
}


int ecrireCaracteres(FILE* periphClavier, const char* caracteres, size_t len, unsigned int tempsTraitementParPaquetMicroSecondes){
    // TODO ecrivez votre code ici. Voyez les explications dans l'enonce et dans
    // emulateurClavier.h
    unsigned char paquet[LONGUEUR_USB_PAQUET] = {0};
    unsigned char paquetRelachement[LONGUEUR_USB_PAQUET] = {0};
    size_t nbCaracteresEcrits = 0;
    size_t index = 0;

    if(periphClavier == NULL || caracteres == NULL){
        return -1;
    }

    while(index < len){
        unsigned char modificateur = 0;
        unsigned char codeTouche = 0;
        size_t nbTouchesDansPaquet = 0;

        memset(paquet, 0, sizeof(paquet));
        if(convertirAsciiVersHid(caracteres[index], &modificateur, &codeTouche) != 0){
            index++;
            continue;
        }

        paquet[0] = modificateur;
        paquet[2 + nbTouchesDansPaquet] = codeTouche;
        nbTouchesDansPaquet++;
        index++;

        while(index < len && nbTouchesDansPaquet < 6){
            unsigned char prochainModificateur = 0;
            unsigned char prochainCodeTouche = 0;

            if(convertirAsciiVersHid(caracteres[index], &prochainModificateur, &prochainCodeTouche) != 0){
                index++;
                continue;
            }

            if(prochainModificateur != modificateur){
                break;
            }

            paquet[2 + nbTouchesDansPaquet] = prochainCodeTouche;
            nbTouchesDansPaquet++;
            index++;
        }

        if(fwrite(paquet, LONGUEUR_USB_PAQUET, 1, periphClavier) != 1){
            return -1;
        }

        if(fwrite(paquetRelachement, LONGUEUR_USB_PAQUET, 1, periphClavier) != 1){
            return -1;
        }

        usleep(tempsTraitementParPaquetMicroSecondes);
        nbCaracteresEcrits += nbTouchesDansPaquet;
    }

    return (int)nbCaracteresEcrits;
}
