/* =========================================================
   SCENE2.C — Scène : démonstration palette VGA
   =========================================================
   Phases et durées :
   1.  1 s : affichage statique (palette par défaut)
   2.  5 s : cycle de palette vers la droite
   3.  5 s : cycle de palette vers la gauche
   Puis    : transition vers SCENE_3 (cut)

   La phase 4 (lerp vers pinkPalette) est supprimée :
   plus aucun fondu entre scènes.
   ========================================================= */

#include "timer.h"
#include "video.h"
#include "palette.h"
#include "graphics.h"
#include "scene.h"
#include "trans.h"

static void drawPaletteGrid(void)
{
    const int gridSize = 16;
    const int cellSize = 10;
    const int spacing  = 2;
    const int step     = cellSize + spacing;

    const int gridW = gridSize * cellSize + (gridSize - 1) * spacing;
    const int gridH = gridSize * cellSize + (gridSize - 1) * spacing;

    const int offsetX = (SCREEN_WIDTH  - gridW) / 2;
    const int offsetY = (SCREEN_HEIGHT - gridH) / 2;

    int x, y;
    int i = 0;

    for (y = 0; y < gridSize; y++)
        for (x = 0; x < gridSize; x++)
        {
            int px = offsetX + x * step;
            int py = offsetY + y * step;
            drawRectFill(px, py, px + cellSize - 1, py + cellSize - 1,
                         (unsigned char)i++);
        }
}

void scene2(void)
{
    static unsigned long lastRender  = 0UL;
    static int           initialized = 0;

    const unsigned long D1 = 1000UL;   /* statique      */
    const unsigned long D2 = 5000UL;   /* cycle droite  */
    const unsigned long D3 = 5000UL;   /* cycle gauche  */

    const unsigned long T1 = D1;
    const unsigned long T2 = T1 + D2;
    const unsigned long T3 = T2 + D3;

    const unsigned long render_interval_ms = 25UL;

    unsigned long now     = readTimer();
    unsigned long elapsed = elapsedTimeMs(sceneStart, now);

    if (!initialized)
    {
        initialized = 1;

        copyPalette(workingPalette, defaultPalette);
        setPalette(workingPalette);

        clearScreen(0);
        flip();
    }

    /* Phase 1 : statique */
    if (elapsed < T1)
    {
        drawPaletteGrid();
        flip();
        return;
    }

    /* Phase 2 : cycle droite */
    if (elapsed < T2)
    {
        if (elapsedTimeMs(lastRender, now) >= render_interval_ms)
        {
            cyclePaletteRight(workingPalette, 0, 255);
            drawPaletteGrid();
            flip();
            lastRender = now;
        }
        return;
    }

    /* Phase 3 : cycle gauche */
    if (elapsed < T3)
    {
        if (elapsedTimeMs(lastRender, now) >= render_interval_ms)
        {
            cyclePaletteLeft(workingPalette, 0, 255);
            drawPaletteGrid();
            flip();
            lastRender = now;
        }
        return;
    }

    /* Fin de scène : cut direct vers SCENE_3 */
    if (!transitionPending())
    {
        initialized = 0;
        transitionRequest(SCENE_3, TRANS_CUT, 0UL);
    }
}
