/* =========================================================
   SCENE1.C — Scène : pixels aléatoires
   =========================================================
   Durée totale : 5 secondes.
   Aucune gestion clavier (sauf Échap global via INT 09h).
   ========================================================= */

#include <time.h>
#include "timer.h"
#include "video.h"
#include "palette.h"
#include "scene.h"

void scene1(void)
{
    static unsigned long lastRender    = 0;
    static int           initialized   = 0;
    static unsigned long lcg_state     = 0;
    static unsigned long knownStart    = 0;

    const unsigned long render_interval_ms = 100UL;
    const unsigned long scene_ms           = 5000UL;

    unsigned long  now = readTimer();
    unsigned int far *dst;
    unsigned long  i;
    unsigned int   pixel;

    if (initialized && sceneStart != knownStart)
        initialized = 0;

    if (!initialized)
    {
        lastRender = now;
        lcg_state  = (unsigned long)time(NULL);
        knownStart = sceneStart;

        copyPalette(workingPalette, defaultPalette);
        setPalette(workingPalette);

        initialized = 1;
    }

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

    if (elapsedTimeMs(sceneStart, now) > scene_ms)
    {
        sceneSignalEnd();
    }
}
