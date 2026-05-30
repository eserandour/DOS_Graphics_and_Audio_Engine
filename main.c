/* =========================================================
   MAIN.C — Point d'entrée et arrêt propre
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode vidéo    : 13h (320x200, 256 couleurs)

   Échap est détecté par un handler INT 09h installé dans
   timer.c, qui lève quitRequested sans consommer la touche.
   main ne lit jamais le buffer clavier.
   ========================================================= */

#include <stdlib.h>
#include <time.h>
#include "timer.h"
#include "keyboard.h"
#include "app.h"
#include "video.h"
#include "palette.h"
#include "font1.h"
#include "scene.h"
#include "trans.h"

void shutdown(void)
{
    restoreKeyboard();
    restoreTimer();
    setVideoMode(0x03);
    freeBackbuffer();
}

int main(void)
{
    srand((unsigned int)time(NULL));

    if (!initBackbuffer())
        return 1;

    setVideoMode(0x13);

    font1InitBios();
    font1InitBank8x8();
    font1InitBank8x16();
    font1InitBank16x16();

    getPalette(defaultPalette);
    generatePinkPalette(pinkPalette);

    installTimer();
    installKeyboard();

    setScene(SCENE_1);

    while (!quitRequested)
    {
        if (!transitionUpdate())
            runCurrentScene();
    }

    shutdown();
    return 0;
}
