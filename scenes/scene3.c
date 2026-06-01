/* =========================================================
   SCENE3.C — Scène : Démonstration des polices
   =========================================================
   6 sous-écrans affichés automatiquement, 3 s chacun :
     0 — font1Bios  8x8    (0..127)
     1 — font1Bank  8x8    (0..255)
     2 — font1Bank  8x16   (0..127)
     3 — font1Bank  8x16   (128..255)
   Fade in  non bloquant : 0 → fade_in_ms  (t : 0.0 → 1.0)
   Fade out non bloquant : (scene_ms - fade_out_ms) → scene_ms (t : 1.0 → 0.0)
   Aucune gestion clavier (sauf Échap global via INT 09h).
   ========================================================= */

#include "timer.h"
#include "video.h"
#include "palette.h"
#include "graphics.h"
#include "font1.h"
#include "scene.h"

#define NB_SCREENS  4
#define SCREEN_MS   5000UL

static void drawScreen(int screen)
{
    int c, col, row;

    clearScreen(0);

    switch (screen)
    {
        case 0:
        {
            int startX = (SCREEN_WIDTH  - 16 * 10) / 2;
            int startY = (SCREEN_HEIGHT -  8 * 10) / 2 + 9;
            font1DrawTextCentered(4, "font1Bios 8x8 - 0..127", 255, &FONT1_BIOS);
            drawLine(4, 15, 315, 15, 100);
            for (c = 0; c < 128; c++)
            {
                col = c % 16; row = c / 16;
                font1DrawChar(startX + col * 10, startY + row * 10,
                              (unsigned char)c, 255, &FONT1_BIOS);
            }
            break;
        }
        case 1:
        {
            int startX = (SCREEN_WIDTH  - 16 * 10) / 2;
            int startY = (SCREEN_HEIGHT - 16 * 10) / 2 + 9;
            font1DrawTextCentered(4, "font1Bank 8x8 - 0..255", 255, &FONT1_BIOS);
            drawLine(4, 15, 315, 15, 100);
            for (c = 0; c < 256; c++)
            {
                col = c % 16; row = c / 16;
                font1DrawChar(startX + col * 10, startY + row * 10,
                              (unsigned char)c, 255, &FONT1_BANK_8X8);
            }
            break;
        }
        case 2:
        case 3:
        {
            int base   = (screen - 2) * 128;
            int startX = (SCREEN_WIDTH  - 16 * 10) / 2;
            int startY = (SCREEN_HEIGHT -  8 * 18) / 2 + 9;
            font1DrawTextCentered(4, screen == 2
                ? "font1Bank 8x16 - 0..127"
                : "font1Bank 8x16 - 128..255",
                255, &FONT1_BIOS);
            drawLine(4, 15, 315, 15, 100);
            for (c = 0; c < 128; c++)
            {
                col = c % 16; row = c / 16;
                font1DrawChar(startX + col * 10, startY + row * 18,
                              (unsigned char)(base + c), 255, &FONT1_BANK_8X16);
            }
            break;
        }
    }

    flip();
}

void scene3(void)
{
    static int           screen      = 0;
    static int           initialized = 0;
    static unsigned long screenStart = 0;

    const unsigned long scene_ms     = NB_SCREENS * SCREEN_MS;
    const unsigned long fade_in_ms   = 2000UL;  /* durée du fondu entrant  */
    const unsigned long fade_out_ms  = 1000UL;  /* durée du fondu sortant  */

    unsigned long now     = readTimer();
    unsigned long elapsed = elapsedTimeMs(sceneStart, now);
    float         t;

    if (!initialized)
    {
        initialized = 1;
        screen      = 0;
        screenStart = now;
        
        font1InitBios();
        font1InitBank8x8();
        font1InitBank8x16();

        generatePinkPalette(pinkPalette);
        copyPalette(workingPalette, pinkPalette);
        setPalette(workingPalette);
        drawScreen(screen);
    }

    /* -------------------------------------------------------
       Avancement des sous-écrans
       ------------------------------------------------------- */
    if (elapsedTimeMs(screenStart, now) >= SCREEN_MS)
    {
        screen++;

        if (screen >= NB_SCREENS)
        {
            initialized = 0;
            screen      = 0; 
            /* Libere les Font1Bank */
            font1FreeBank(&font1Bank8x8);
            font1FreeBank(&font1Bank8x16);

            sceneSignalEnd();
            return;
        }

        drawScreen(screen);
        screenStart = now;
    }

    /* -------------------------------------------------------
       Calcul du facteur de fondu (non bloquant)
       Basé sur elapsed total depuis sceneStart,
       indépendamment du sous-écran courant.
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

    fadePalette(workingPalette, t);
}
