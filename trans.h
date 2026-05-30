#ifndef TRANSITION_H
#define TRANSITION_H

/* =========================================================
   TRANSITION.H — Moteur de transitions entre scènes
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode vidéo    : 13h (320x200, 256 couleurs)

   Ce module centralise toutes les transitions entre scènes.
   Les scènes n'ont plus à gérer ni leur durée, ni l'effet
   de passage vers la scène suivante : elles appellent
   transitionRequest() quand elles sont "prêtes" à partir,
   et c'est le moteur qui orchestre le reste.

   PRINCIPE DE FONCTIONNEMENT
   --------------------------
   La boucle principale (main.c) appelle transitionUpdate()
   à chaque tick, AVANT runCurrentScene(). Pendant une
   transition, transitionUpdate() renvoie 1 et la boucle
   doit sauter l'appel à runCurrentScene() pour laisser le
   moteur contrôler l'écran.

   Exemple dans main.c :
       while (!quitRequested)
       {
           if (!transitionUpdate())
               runCurrentScene();
       }

   EFFETS DISPONIBLES
   ------------------
   TRANS_CUT      : changement immédiat, sans effet visuel.
   TRANS_FADE     : fondu au noir, puis fondu entrant.
   TRANS_FADEPAL  : fondu vers une palette cible (ex: rose),
                    puis fondu entrant avec la palette cible.
   TRANS_WIPE_L   : balayage vertical gauche → droite en noir.
   TRANS_WIPE_R   : balayage vertical droite → gauche en noir.

   UTILISATION DANS UNE SCENE
   --------------------------
   1. Demander une transition vers la scène suivante :
          transitionRequest(SCENE_2, TRANS_FADE, 500UL);

   2. Optionnel — fade vers une palette précise (TRANS_FADEPAL) :
          transitionRequestWithPal(SCENE_3, TRANS_FADEPAL,
                                   500UL, pinkPalette);

   3. La scène peut vérifier si une transition est en cours
      pour ne pas déclencher une deuxième :
          if (!transitionPending()) transitionRequest(...);
   ========================================================= */

#include "scene.h"
#include "palette.h"

/* =========================================================
   TYPES D'EFFETS
   ========================================================= */

typedef enum {
    TRANS_CUT,        /* cut instantané                      */
    TRANS_FADE,       /* fondu au noir puis fondu entrant     */
    TRANS_FADEPAL,    /* fondu vers palette cible             */
    TRANS_WIPE_L,     /* balayage gauche → droite            */
    TRANS_WIPE_R      /* balayage droite → gauche            */
} TransitionType;

/* =========================================================
   API PUBLIQUE
   ========================================================= */

/* Demande une transition vers la scène `next` avec l'effet
   `type` et une durée totale `duration_ms` (en ms).
   Pour TRANS_FADE, la durée s'applique à chaque demi-fondu.
   Pour TRANS_WIPE, c'est la durée totale du balayage.
   Sans effet sur une transition déjà en cours. */
void transitionRequest(Scene next,
                       TransitionType type,
                       unsigned long  duration_ms);

/* Variante de transitionRequest pour TRANS_FADEPAL :
   `pal` est la palette vers laquelle on fonde. */
void transitionRequestWithPal(Scene         next,
                               TransitionType type,
                               unsigned long  duration_ms,
                               Color         *pal);

/* À appeler à chaque tick dans la boucle principale,
   AVANT runCurrentScene().
   Retourne 1 si une transition est en cours (la scène
   courante ne doit PAS être appelée ce tick).
   Retourne 0 si aucune transition n'est active. */
int transitionUpdate(void);

/* Retourne 1 si une transition a été demandée et n'est
   pas encore terminée. Permet aux scènes de ne pas
   déclencher une deuxième transition en parallèle. */
int transitionPending(void);

#endif /* TRANSITION_H */
