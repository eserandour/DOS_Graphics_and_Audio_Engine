/* =========================================================
   SCENE4.C — Affiche HELLO et WORLD avec font2 (mode disque)
   =========================================================
   Charge la palette font.pal puis affiche :
     - "HELLO" centre horizontalement a y=84
     - "WORLD" en (0, 100) pour illustrer le positionnement
       libre (non centre) de font2DiskDrawText.

   Vide le buffer clavier a l'entree (pour absorber la touche
   qui a declenche la fin de scene3), attend une nouvelle
   touche, puis passe a SCENE_5.
   ========================================================= */

#include <conio.h>
#include "video.h"
#include "graphics.h"
#include "image.h"
#include "font2.h"
#include "scene.h"

void scene4(void)
{
    static int initialized = 0;

    if (!initialized)
    {
        int err;

        err = loadImagePal("images\\font.pal");
        if (err != IMG_OK) { setScene(SCENE_1); return; }

        clearScreen(0);
        font2DiskDrawTextCentered("HELLO", 84, FONT2_BG);
        font2DiskDrawText("WORLD", 0, 120, FONT2_BG);
        flip();

        /* Vider le buffer clavier : la touche qui a declenche
           la fin de scene3 ne doit pas passer en scene4. */
        while (kbhit()) getch();

        initialized = 1;
    }

    if (kbhit())
    {
        getch();
        /* Vider a nouveau avant de passer a scene5. */
        while (kbhit()) getch();
        initialized = 0;
        setScene(SCENE_5);
    }
}
