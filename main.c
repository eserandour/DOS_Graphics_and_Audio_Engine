/* =========================================================
   MAIN.C — Point d'entrée et arrêt propre
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode vidéo    : 13h (320x200, 256 couleurs)
   Compilation   : voir BUILD.BAT et LINK.RSP  
   Date          : 05/05/2026 à 22:56

   Séquence de démarrage :
   1. Allouer le backbuffer (avant le mode graphique).
   2. Passer en mode graphique 13h.
   3. Initialiser les polices.
   4. Sauvegarder la palette par défaut.
   5. Générer les palettes custom.
   6. Installer le timer haute résolution.
   7. Lancer la première scène.
   8. Boucle principale jusqu'à appui sur une touche.
   9. Arrêt propre.
   ========================================================= */

#include <stdlib.h>   /* exit                              */
#include <time.h>     /* time (graine aléatoire)           */
#include <conio.h>    /* kbhit, getch                      */
#include "timer.h"    /* installTimer, restoreTimer        */
#include "video.h"    /* initBackbuffer, setVideoMode, etc */
#include "palette.h"  /* getPalette, generatePinkPalette   */
#include "font1.h"     /* font1InitBios, font1Init*()             */
#include "scene.h"    /* setScene, runCurrentScene         */

/* =========================================================
   ARRÊT PROPRE
   =========================================================
   Cette fonction est aussi appelée depuis scene3.c (fin
   normale de la démo), d'où son prototype public dans
   scene3.c plutôt que dans un header.

   Ordre d'arrêt important :
   1. Restaurer le timer BIOS en premier (les interruptions
      ne doivent pas appeler notre ISR après la sortie).
   2. Repasser en mode texte (remet le BIOS en état normal).
   3. Réafficher le curseur texte.
   4. Libérer la mémoire du backbuffer.
   ========================================================= */
void shutdown(void)
{
    restoreTimer();       /* PIT → 18.2 Hz, ISR BIOS restaurée */
    setVideoMode(0x03);   /* retour au mode texte 80x25        */
    freeBackbuffer();     /* libérer les 64000 octets          */
}

/* =========================================================
   POINT D'ENTRÉE
   ========================================================= */
int main(void)
{
    /* Initialiser le générateur de nombres pseudo-aléatoires
       avec l'heure courante comme graine. Sans ça, la
       séquence LCG de scene1 serait identique à chaque
       lancement. */
    srand((unsigned int)time(NULL));

    /* Allouer le backbuffer AVANT de passer en mode
       graphique. En cas d'échec on peut encore afficher
       un message texte et quitter proprement. */
    if (!initBackbuffer())
    {
        /* Pas de puts() ici : on est encore en mode texte. */
        return 1;
    }

    /* Passer en mode graphique VGA 13h (320x200, 256 coul).
       À partir de là, plus d'affichage texte possible. */
    setVideoMode(0x13);

    /* Initialiser les polices.
       font1InitBios() lit l'adresse fixe de la table ROM 8x8
       du BIOS. font1InitBank*() allouent chacune une Font1Bank
       en mémoire far et chargent leurs glyphes depuis font1dat.c. */
    font1InitBios();
    font1InitBank8x8();
    font1InitBank8x16();
    font1InitBank16x16();

    /* Lire la palette VGA actuelle (définie par le BIOS).
       Elle sera utilisée dans scene1 comme palette
       de base pour l'affichage des pixels aléatoires. */
    getPalette(defaultPalette);

    /* Précalculer la palette rose une seule fois.
       Elle sera utilisée par scene2 et scene3. */
    generatePinkPalette(pinkPalette);

    /* Installer le timer haute résolution (70 Hz).
       À faire après les initialisations graphiques et
       polices qui peuvent prendre du temps. */
    installTimer();

    /* Démarrer la première scène. */
    setScene(SCENE_1);

    /* Boucle principale : appeler la scène courante en
       boucle jusqu'à ce que l'utilisateur appuie sur une
       touche. kbhit() retourne 1 s'il y a une touche en
       attente dans le buffer clavier, sans bloquer. */
    while (!kbhit())
        runCurrentScene();

    /* Vider le buffer clavier pour éviter que le caractère
       appuyé s'affiche dans le terminal après la sortie. */
    getch();

    /* Arrêt propre. */
    shutdown();
    return 0;
}
