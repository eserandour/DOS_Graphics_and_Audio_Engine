/* =========================================================
   SCENE1.C — Scène : pixels aléatoires
   =========================================================
   Durée totale : 5 secondes.
   Fade in  non bloquant : 0 → fade_in_ms  (t : 0.0 → 1.0)
   Fade out non bloquant : (scene_ms - fade_out_ms) → scene_ms (t : 1.0 → 0.0)
   Aucune gestion clavier (sauf Échap global via INT 09h).
   ========================================================= */

#include <time.h>
#include "timer.h"
#include "video.h"
#include "palette.h"
#include "graphics.h"
#include "scene.h"

void scene1(void)
{
    static unsigned long lastRender    = 0;
    static int           initialized   = 0;
    static unsigned long lcg_state     = 0;

    const unsigned long render_interval_ms = 100UL;
    const unsigned long scene_ms           = 6000UL;
    const unsigned long fade_in_ms         = 1000UL;  /* durée du fondu entrant  */
    const unsigned long fade_out_ms        = 3000UL;  /* durée du fondu sortant  */

    unsigned long now     = readTimer();
    unsigned long elapsed = elapsedTimeMs(sceneStart, now);
    unsigned int  far *dst;
    unsigned long i;
    unsigned int  pixel;
    float         t;

    if (!initialized)
    {
        initialized = 1;
        lastRender = now;
        lcg_state  = (unsigned long)time(NULL);
        copyPalette(workingPalette, defaultPalette);
        setPalette(workingPalette);
        clearScreen(0);   
    }

    /* -------------------------------------------------------
       Rendu des pixels aléatoires (inchangé)
       ------------------------------------------------------- */
    while (elapsedTimeMs(lastRender, now) >= render_interval_ms)
    {
        dst = (unsigned int far *)backbuffer;

        for (i = 0; i < 32000UL; i++)
        {
            lcg_state = lcg_state * 1664525UL + 1013904223UL;
            pixel     = (unsigned int)(lcg_state >> 16);
            dst[i]    = pixel;
        }

        flip();

        lastRender += (render_interval_ms * TARGET_HZ) / 1000UL;
    }

    /* -------------------------------------------------------
       Calcul du facteur de fondu courant (non bloquant)
       ------------------------------------------------------- */
    if (elapsed < fade_in_ms)
    {
        /* Fade in : 0 → fade_in_ms */
        t = (float)elapsed / (float)fade_in_ms;
    }
    else if (elapsed >= scene_ms - fade_out_ms)
    {
        /* Fade out : (scene_ms - fade_out_ms) → scene_ms */
        t = (float)(scene_ms - elapsed) / (float)fade_out_ms;
        if (t < 0.0f) t = 0.0f;
    }
    else
    {
        /* Pleine luminosité */
        t = 1.0f;
    }

    /* -------------------------------------------------------
       Application du fondu sur la palette (non bloquant)
       fadePalette() applique t à workingPalette et envoie
       directement au DAC sans modifier workingPalette en RAM.
       ------------------------------------------------------- */
    fadePalette(workingPalette, t);

    if (elapsedTimeMs(sceneStart, now) > scene_ms)
    {
        initialized = 0;
        sceneSignalEnd();
    }
}
