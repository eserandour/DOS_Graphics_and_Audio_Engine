/* =========================================================
   SCENE5.C — Scrolling de texte horizontal (credits)
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode video    : 13h (320x200, 256 couleurs)

   Affiche un texte long qui défile de droite à gauche,
   en utilisant font2 (font2Load / font2Free).
   Après un tour complet du ruban, retour automatique en SCENE_1.
   Aucune gestion clavier (sauf Échap global via INT 09h).

   ALGORITHME PAR FRAME
   --------------------
   scrollX : position courante (pixels depuis le debut du ruban)

   Pour chaque colonne c de l'ecran (0..319) :
     posX      = scrollX + c
     charIdx   = (posX / char_w) % textLen
     pixInChar = posX % char_w
     -> blit de la colonne pixInChar du glyphe charIdx

   ========================================================= */

#include "video.h"
#include "palette.h"
#include "image.h"
#include "timer.h"
#include "graphics.h"
#include "font2.h"
#include "scene.h"
#include "trans.h"
#include "app.h"

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
static Font2Desc     scrollFont  = FONT2_DESC_16X16_F2;
static long          scrollX     = 0;
static unsigned long lastScroll  = 0;
static int           initialized = 0;
static long          rubanW      = 0;
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
    unsigned char ck;

    posX      = scrollX + (long)screenCol;
    charIdx   = (int)((posX / scrollFont.char_w) % textLen);
    pixInChar = (int)(posX % scrollFont.char_w);

    c   = (unsigned char)scrollText[charIdx];
    dst = backbuffer + OFFSET(screenCol, textY);
    ck  = (unsigned char)scrollFont.colorKey;

    if (c < (unsigned char)scrollFont.first_char ||
        c > (unsigned char)scrollFont.last_char)
    {
        for (row = 0; row < scrollFont.char_h; row++)
        {
            *dst = SCROLL_BG_COLOR;
            dst += SCREEN_WIDTH;
        }
        return;
    }

    glyphIdx = c - (unsigned char)scrollFont.first_char;
    glyphCol = glyphIdx % scrollFont.cols;
    glyphRow = glyphIdx / scrollFont.cols;
    srcX     = glyphCol * scrollFont.char_w + pixInChar;
    srcY     = glyphRow * scrollFont.char_h;

    for (row = 0; row < scrollFont.char_h; row++)
    {
        pix  = font2GetPixel(&scrollFont, srcX, srcY + row);
        *dst = (pix == ck) ? SCROLL_BG_COLOR : pix;
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

        err = loadImagePal("images\\font2\\16X16_F2.pal");
        if (err != IMG_OK) { quitRequested = 1; return; }

        if (!font2Load(&scrollFont)) { quitRequested = 1; return; }

        textLen    = f2len(scrollText);
        textY      = (SCREEN_HEIGHT - scrollFont.char_h) / 2;
        rubanW     = (long)textLen * scrollFont.char_w;
        scrollX    = 0;

        clearScreen(SCROLL_BG_COLOR);
        drawRectFill(0, textY - SCROLL_BAR_H - 1,
                     SCREEN_WIDTH - 1, textY - 1,
                     SCROLL_BAR_COLOR);
        drawRectFill(0, textY + scrollFont.char_h,
                     SCREEN_WIDTH - 1,
                     textY + scrollFont.char_h + SCROLL_BAR_H,
                     SCROLL_BAR_COLOR);
        flip();

        lastScroll = readTimer();

        initialized = 1;
        return;
    }

    /* -------------------------------------------------------
       Avancement du scroll
       ------------------------------------------------------- */
    steps = elapsedTimeMs(lastScroll, now) / SCROLL_SPEED_MS;
    if (steps == 0) return;
    if (steps > 8)  steps = 8;

    scrollX += (long)steps;

    if (scrollX >= rubanW)
    {
        font2Free(&scrollFont);
        initialized = 0;
        transitionRequest(SCENE_1, TRANS_CUT, 0UL);
        return;
    }

    lastScroll += steps * (SCROLL_SPEED_MS * TARGET_HZ) / 1000UL;

    /* -------------------------------------------------------
       Rendu
       ------------------------------------------------------- */
    for (col = 0; col < SCREEN_WIDTH; col++)
        blitColumn(col);

    drawRectFill(0, textY - SCROLL_BAR_H - 1,
                 SCREEN_WIDTH - 1, textY - 1,
                 SCROLL_BAR_COLOR);
    drawRectFill(0, textY + scrollFont.char_h,
                 SCREEN_WIDTH - 1,
                 textY + scrollFont.char_h + SCROLL_BAR_H,
                 SCROLL_BAR_COLOR);
    flip();
}
