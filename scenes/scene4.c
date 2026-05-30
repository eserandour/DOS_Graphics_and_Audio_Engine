/* =========================================================
   SCENE4.C — Affiche HELLO et WORLD avec font2
   =========================================================
   Charge la palette font.pal puis affiche :
     - "HELLO" centre horizontalement a y=84
     - "WORLD" en (0, 120) pour illustrer le positionnement
       libre (non centre) de font2DrawText.

   Duree : 3 secondes, puis passage automatique a SCENE_5
   (cut). Echap gere globalement par le handler INT 09h.
   ========================================================= */

#include "timer.h"
#include "video.h"
#include "graphics.h"
#include "image.h"
#include "font2.h"
#include "scene.h"
#include "trans.h"
#include "app.h"

/* Descripteur global (init statique au niveau fichier = OK pour Watcom) */
static Font2Desc s4_font = FONT2_DESC_DEFAULT;

void scene4(void)
{
    static int           initialized = 0;
    static unsigned long knownStart  = 0;

    const unsigned long scene_ms = 3000UL;
    unsigned long now = readTimer();

    if (initialized && sceneStart != knownStart)
        initialized = 0;

    if (!initialized)
    {
        int err;

        err = loadImagePal("images\\font2\\font.pal");
        if (err != IMG_OK) { quitRequested = 1; return; }

        if (!font2Load(&s4_font)) { quitRequested = 1; return; }

        clearScreen(0);
        font2DrawTextCentered(&s4_font, "HELLO", 84);
        font2DrawText(&s4_font, "WORLD", 0, 120);
        flip();

        knownStart  = sceneStart;
        initialized = 1;
    }

    if (elapsedTimeMs(sceneStart, now) >= scene_ms)
    {
        if (!transitionPending())
        {
            font2Free(&s4_font);
            initialized = 0;
            transitionRequest(SCENE_5, TRANS_CUT, 0UL);
        }
    }
}
