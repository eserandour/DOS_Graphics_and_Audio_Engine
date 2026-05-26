/* =========================================================
   SCENE5.C — Scrolling de texte horizontal (credits)
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode video    : 13h (320x200, 256 couleurs)

   Affiche un texte long qui defile de droite a gauche,
   en utilisant le mode RAM de font2 (font2RamLoad / font2RamFree).

   ALGORITHME PAR FRAME
   --------------------
   scrollX : position courante (pixels depuis le debut du ruban)

   Pour chaque colonne c de l'ecran (0..319) :
     posX      = scrollX + c
     charIdx   = (posX / FONT2_CHAR_W) % textLen
     pixInChar = posX % FONT2_CHAR_W
     -> blit de la colonne pixInChar du glyphe charIdx

   SORTIE : n'importe quelle touche -> setScene(SCENE_1).
   ========================================================= */

#include <conio.h>
#include "video.h"
#include "palette.h"
#include "image.h"
#include "timer.h"
#include "graphics.h"
#include "font2.h"
#include "scene.h"

/* =========================================================
   TEXTE DU SCROLLER
   ========================================================= */
#define SCROLL_TEXT \
    "     DEMO DOS MODE 13H  *  OPEN WATCOM 1.9  *  " \
    "VIDEO : MODE 13H 320x200 256 COULEURS          "

/* =========================================================
   PARAMETRES
   ========================================================= */
#define SCROLL_SPEED_MS   20UL
#define SCROLL_BG_COLOR    0
#define SCROLL_BAR_COLOR   8
#define SCROLL_BAR_H       2

/* =========================================================
   ETAT STATIQUE
   ========================================================= */
static long          scrollX     = 0;
static unsigned long lastScroll  = 0;
static int           initialized = 0;
static long          rubanW      = 0;   /* long pour éviter le débordement 16 bits
                                           si le texte s'allonge (int max = 32767) */
static int           textLen     = 0;
static int           textY       = 0;

static const char *scrollText = SCROLL_TEXT;

static int f2len(const char *s) { int n=0; while(s[n]) n++; return n; }

/* =========================================================
   BLIT D'UNE COLONNE
   ========================================================= */
static void blitColumn(int screenCol)
{
    long posX;
    int  charIdx, pixInChar;
    unsigned char c;
    int  glyphIdx, glyphCol, glyphRow, srcX, srcY, row;
    unsigned char far *dst;
    unsigned char pix;

    posX      = scrollX + (long)screenCol;
    charIdx   = (int)((posX / FONT2_CHAR_W) % textLen);
    pixInChar = (int)(posX % FONT2_CHAR_W);

    c   = (unsigned char)scrollText[charIdx];
    dst = backbuffer + OFFSET(screenCol, textY);

    if (c < FONT2_FIRST_CHAR || c > FONT2_LAST_CHAR)
    {
        for (row = 0; row < FONT2_CHAR_H; row++)
        {
            *dst = SCROLL_BG_COLOR;
            dst += SCREEN_WIDTH;
        }
        return;
    }

    glyphIdx = c - FONT2_FIRST_CHAR;
    glyphCol = glyphIdx % FONT2_COLS;
    glyphRow = glyphIdx / FONT2_COLS;
    srcX     = glyphCol * FONT2_CHAR_W + pixInChar;
    srcY     = glyphRow * FONT2_CHAR_H;

    for (row = 0; row < FONT2_CHAR_H; row++)
    {
        pix  = font2RamGetPixel(srcX, srcY + row);
        *dst = (pix == FONT2_BG) ? SCROLL_BG_COLOR : pix;
        dst += SCREEN_WIDTH;
    }
}

/* =========================================================
   SCENE PRINCIPALE
   ========================================================= */
void scene5(void)
{
    unsigned long now = readTimer();
    unsigned long steps;
    int col;

    /* -------------------------------------------------------
       Initialisation
       ------------------------------------------------------- */
    if (!initialized)
    {
        int err;

        err = loadImagePal("images\\font.pal");
        if (err != IMG_OK) { setScene(SCENE_1); return; }

        if (!font2RamLoad()) { setScene(SCENE_1); return; }

        textLen    = f2len(scrollText);
        textY      = (SCREEN_HEIGHT - FONT2_CHAR_H) / 2;
        rubanW     = (long)textLen * FONT2_CHAR_W;   /* cast avant la multiplication */
        scrollX    = 0;

        clearScreen(SCROLL_BG_COLOR);
        drawRectFill(0, textY - SCROLL_BAR_H - 1,
                     SCREEN_WIDTH - 1, textY - 1,
                     SCROLL_BAR_COLOR);
        drawRectFill(0, textY + FONT2_CHAR_H,
                     SCREEN_WIDTH - 1,
                     textY + FONT2_CHAR_H + SCROLL_BAR_H,
                     SCROLL_BAR_COLOR);
        flip();

        /* Vider le buffer clavier : la touche de scene4
           ne doit pas declencher une sortie immediate. */
        while (kbhit()) getch();

        /* Initialiser lastScroll APRES le flush clavier
           et les operations d'init (qui prennent du temps)
           pour que le premier scroll parte d'un timestamp
           coherent avec la fin reelle de l'init. */
        lastScroll = readTimer();

        initialized = 1;
        return;
    }

    /* -------------------------------------------------------
       Clavier
       ------------------------------------------------------- */
    if (kbhit())
    {
        getch();
        font2RamFree();
        initialized = 0;
        setScene(SCENE_1);
        return;
    }

    /* -------------------------------------------------------
       Avancement du scroll
       ------------------------------------------------------- */
    steps = elapsedTimeMs(lastScroll, now) / SCROLL_SPEED_MS;
    if (steps == 0) return;
    /* Plafond de rattrapage : si le programme était bloqué
       (debug, swap...), on limite le saut à 8 px pour éviter
       un scroll trop brusque qui désorienterait le lecteur. */
    if (steps > 8)  steps = 8;

    scrollX += (long)steps;
    if (scrollX >= rubanW)
        scrollX -= rubanW;

    lastScroll += steps * (SCROLL_SPEED_MS * TARGET_HZ) / 1000UL;

    /* -------------------------------------------------------
       Rendu
       ------------------------------------------------------- */
    for (col = 0; col < SCREEN_WIDTH; col++)
        blitColumn(col);

    drawRectFill(0, textY - SCROLL_BAR_H - 1,
                 SCREEN_WIDTH - 1, textY - 1,
                 SCROLL_BAR_COLOR);
    drawRectFill(0, textY + FONT2_CHAR_H,
                 SCREEN_WIDTH - 1,
                 textY + FONT2_CHAR_H + SCROLL_BAR_H,
                 SCROLL_BAR_COLOR);
    flip();
}
