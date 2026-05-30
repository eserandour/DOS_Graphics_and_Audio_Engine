#include <stddef.h>
/* =========================================================
   TRANSITION.C — Moteur de transitions entre scènes
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode vidéo    : 13h (320x200, 256 couleurs)

   Machine à états interne :
   ┌─────────────┐    transitionRequest()    ┌──────────────┐
   │  IDLE       │ ─────────────────────────>│  OUT         │
   │  (pas de    │                           │  (effet de   │
   │   transit.) │                           │   sortie)    │
   └─────────────┘                           └──────┬───────┘
         ^                                          │ effet terminé
         │                                          v
         │                                   ┌──────────────┐
         │         transition terminée        │  SWITCH      │
         └────────────────────────────────────│  (setScene,  │
                                             │  1 tick)     │
                                             └──────┬───────┘
                                                    │
                                                    v
                                             ┌──────────────┐
                                             │  IN          │
                                             │  (effet      │
                                             │   d'entrée)  │
                                             └──────────────┘

   TRANS_CUT : passe directement de IDLE à SWITCH (0 ms).
   TRANS_FADE / TRANS_FADEPAL :
       OUT = fondu vers le noir (ou vers la palette cible)
       IN  = fondu depuis le noir (ou depuis la palette cible)
   TRANS_WIPE_L / TRANS_WIPE_R :
       OUT = balayage de colonnes noires sur la moitié gauche
       IN  = dévoilement de la nouvelle scène sur la moitié droite
   ========================================================= */

#include "trans.h"
#include "timer.h"
#include "video.h"
#include "palette.h"
#include "graphics.h"
#include "scene.h"

/* =========================================================
   ÉTATS INTERNES
   ========================================================= */

typedef enum {
    TS_IDLE,    /* aucune transition en cours       */
    TS_OUT,     /* effet de sortie (écran → noir)   */
    TS_SWITCH,  /* setScene() appelé, 1 tick pause  */
    TS_IN       /* effet d'entrée (noir → écran)    */
} TransState;

/* Contexte de la transition courante. */
static TransState      ts_state       = TS_IDLE;
static TransitionType  ts_type        = TRANS_CUT;
static Scene           ts_next        = SCENE_1;
static unsigned long   ts_duration_ms = 0UL;   /* durée d'un demi-effet */
static unsigned long   ts_start       = 0UL;   /* tick de début de l'état courant */
static Color          *ts_pal         = 0;   /* palette cible (FADEPAL uniquement) */

/* Palette sauvegardée au moment du déclenchement de la
   transition, pour le fondu sortant. */
static Color ts_savedPal[256];

/* =========================================================
   HELPERS INTERNES
   ========================================================= */

/* Calcule t ∈ [0.0, 1.0] selon le temps écoulé depuis
   ts_start et la durée ts_duration_ms.
   Retourne 1 si la durée est écoulée (t atteint 1.0). */
static int calcT(unsigned long now, float *t_out)
{
    unsigned long elapsed = elapsedTimeMs(ts_start, now);
    float t;

    if (ts_duration_ms == 0UL)
    {
        *t_out = 1.0f;
        return 1;
    }

    t = (float)elapsed / (float)ts_duration_ms;
    if (t >= 1.0f) { *t_out = 1.0f; return 1; }
    *t_out = t;
    return 0;
}

/* Peint `count` colonnes noires à partir de `x` dans le
   backbuffer, puis appelle flip(). Utilisé par les wipes. */
static void wipeColumns(int x, int count)
{
    int c, col, row;
    unsigned char far *p;

    for (c = 0; c < count; c++)
    {
        col = x + c;
        if (col < 0 || col >= SCREEN_WIDTH) continue;
        p = backbuffer + col;
        for (row = 0; row < SCREEN_HEIGHT; row++)
        {
            *p = 0;
            p += SCREEN_WIDTH;
        }
    }
    flip();
}

/* =========================================================
   API PUBLIQUE
   ========================================================= */

void transitionRequest(Scene next,
                       TransitionType type,
                       unsigned long  duration_ms)
{
    /* Ignorer si une transition est déjà en cours. */
    if (ts_state != TS_IDLE) return;

    ts_next        = next;
    ts_type        = type;
    ts_duration_ms = duration_ms;
    ts_pal         = 0;
    ts_start       = readTimer();

    /* Sauvegarder la palette courante pour le fade-out. */
    copyPalette(ts_savedPal, workingPalette);

    if (type == TRANS_CUT)
    {
        /* Pas d'effet : aller directement au SWITCH. */
        ts_state = TS_SWITCH;
    }
    else
    {
        ts_state = TS_OUT;
    }
}

void transitionRequestWithPal(Scene         next,
                               TransitionType type,
                               unsigned long  duration_ms,
                               Color         *pal)
{
    transitionRequest(next, type, duration_ms);
    ts_pal = pal;   /* palette cible, écrase NULL posé plus haut */
}

int transitionPending(void)
{
    return (ts_state != TS_IDLE);
}

/* =========================================================
   BOUCLE PRINCIPALE DU MOTEUR
   ========================================================= */

int transitionUpdate(void)
{
    unsigned long now;
    float         t;
    int           done;
    int           wipe_x;   /* colonne du front de balayage */

    if (ts_state == TS_IDLE) return 0;

    now  = readTimer();
    done = 0;

    switch (ts_state)
    {
        /* -------------------------------------------------------
           Phase OUT : l'écran disparaît
           ------------------------------------------------------- */
        case TS_OUT:
            done = calcT(now, &t);

            switch (ts_type)
            {
                case TRANS_FADE:
                    /* Fondu de la palette courante vers le noir.
                       On applique (1.0 - t) à ts_savedPal. */
                    fadePalette(ts_savedPal, 1.0f - t);
                    break;

                case TRANS_FADEPAL:
                    /* Fondu de ts_savedPal vers ts_pal (ou le noir
                       si ts_pal est NULL). */
                    if (ts_pal != 0)
                        lerpPalette(workingPalette,
                                    ts_savedPal, ts_pal, t);
                    else
                        fadePalette(ts_savedPal, 1.0f - t);
                    setPalette(workingPalette);
                    break;

                case TRANS_WIPE_L:
                    /* Colonnes noires de gauche vers droite. */
                    wipe_x = (int)((float)SCREEN_WIDTH * t) - 1;
                    if (wipe_x >= 0)
                        wipeColumns(0, wipe_x + 1);
                    break;

                case TRANS_WIPE_R:
                    /* Colonnes noires de droite vers gauche. */
                    wipe_x = SCREEN_WIDTH - 1
                             - (int)((float)SCREEN_WIDTH * t);
                    if (wipe_x < SCREEN_WIDTH)
                        wipeColumns(wipe_x, SCREEN_WIDTH - wipe_x);
                    break;

                default:
                    break;
            }

            if (done)
            {
                ts_state = TS_SWITCH;
                /* ts_start n'est pas mis à jour : SWITCH dure
                   un seul tick, pas besoin de chronométrer. */
            }

            /* Pour les fondus palette (FADE / FADEPAL), le backbuffer
               n'est pas touché : la scène peut continuer à dessiner
               pendant la phase OUT, ce qui permet à l'animation de
               la scène 1 de tourner pendant le fondu sortant.
               Pour les wipes, on bloque car wipeColumns() écrase le
               backbuffer et appellerait flip() en double. */
            if (ts_type == TRANS_FADE || ts_type == TRANS_FADEPAL)
                return 0;   /* laisser runCurrentScene() s'exécuter */

            break;

        /* -------------------------------------------------------
           Phase SWITCH : activer la nouvelle scène
           Un seul tick : setScene() met à jour sceneStart et
           remet la scène à son état initial (via initialized=0
           dans chaque scène). On passe immédiatement à IN.
           ------------------------------------------------------- */
        case TS_SWITCH:
            /* Écran en noir complet avant de dessiner la
               nouvelle scène pour éviter un flash résiduel. */
            generateBlackPalette(workingPalette);
            setPalette(workingPalette);

            /* Pour TRANS_FADEPAL : restaurer la palette cible dans
               workingPalette (sans l'envoyer au DAC) afin que
               fadePalette(workingPalette, t) dans TS_IN puisse
               interpoler depuis le noir vers les bonnes couleurs.
               Le DAC reste à noir grâce au setPalette ci-dessus. */
            if (ts_type == TRANS_FADEPAL && ts_pal != 0)
                copyPalette(workingPalette, ts_pal);

            setScene(ts_next);

            ts_state = TS_IN;
            ts_start = readTimer();
            break;

        /* -------------------------------------------------------
           Phase IN : la nouvelle scène apparaît
           ------------------------------------------------------- */
        case TS_IN:
            /* Laisser la scène dessiner son premier frame
               (elle s'exécutera dans runCurrentScene après
               notre return 0 final). On gère seulement la
               palette ici pour ne pas dupliquer le rendu. */
            done = calcT(now, &t);

            switch (ts_type)
            {
                case TRANS_FADE:
                    /* Fondu du noir vers la palette de la scène.
                       La scène dessine dans le backbuffer ; on
                       contrôle seulement la luminosité via le DAC. */
                    fadePalette(workingPalette, t);
                    break;

                case TRANS_FADEPAL:
                    /* La scène a déjà chargé sa palette. On
                       l'illumine progressivement. */
                    fadePalette(workingPalette, t);
                    break;

                case TRANS_WIPE_L:
                    /* Dévoilement de gauche vers droite : la
                       moitié gauche (déjà dessinée par la scène)
                       est visible, la droite reste noire. */
                    wipe_x = (int)((float)SCREEN_WIDTH * t);
                    /* Laisser flip() de la scène s'exécuter,
                       puis recouvrir la partie non encore révélée. */
                    if (wipe_x < SCREEN_WIDTH)
                        wipeColumns(wipe_x,
                                    SCREEN_WIDTH - wipe_x);
                    break;

                case TRANS_WIPE_R:
                    /* Dévoilement de droite vers gauche. */
                    wipe_x = SCREEN_WIDTH
                             - (int)((float)SCREEN_WIDTH * t);
                    if (wipe_x > 0)
                        wipeColumns(0, wipe_x);
                    break;

                default:
                    break;
            }

            if (done)
            {
                /* Transition terminée : remettre la palette
                   à sa valeur normale (t = 1.0). */
                setPalette(workingPalette);
                ts_state = TS_IDLE;
                ts_pal   = 0;
                /* Retourner 0 pour que runCurrentScene()
                   s'exécute normalement dès ce tick. */
                return 0;
            }

            /* Pendant la phase IN, on laisse la scène
               s'exécuter pour qu'elle construise son
               backbuffer à chaque tick. */
            return 0;

        default:
            break;
    }

    /* Pendant la phase SWITCH et les wipes OUT, bloquer la scène.
       Les fondus (FADE/FADEPAL) en phase OUT laissent la scène
       s'exécuter (voir return 0 ci-dessus dans TS_OUT). */
    return (ts_state != TS_IDLE) ? 1 : 0;
}
