/* =========================================================
   FONT2.C — Affichage de texte par feuille de sprites
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode video    : 13h (320x200, 256 couleurs, 1 octet/pixel)

   ATTENTION — arithmetique 16 bits
   ---------------------------------
   Sur DOS, int est 16 bits signes (max 32767).
   FONT2_SHEET_SIZE = 320 * 192 = 61440 depasse cette limite.
   Toutes les tailles et offsets utilisant cette valeur
   doivent etre declares en unsigned long (suffixe UL).

   MODE DISQUE  font2DiskDrawText / font2DiskDrawTextCentered
     Une ouverture de fichier par glyphe via
     loadImageZoneRawKey(). Simple, adapte au texte statique.

   MODE RAM     font2RamLoad / font2RamDrawChar / font2RamDrawText /
                font2RamDrawTextCentered / font2RamGetPixel / font2RamFree
     Charge toute la feuille en RAM far une seule fois.
     Lecture ligne par ligne (FONT2_SHEET_W octets) pour
     eviter le probleme de fread > 32767 octets en une passe.
     Blit direct depuis le buffer : zero acces disque.
   ========================================================= */

#include <malloc.h>   /* _fmalloc, _ffree                   */
#include <stdio.h>    /* FILE, fopen, fread, fclose          */
#include <string.h>   /* _fmemcpy                           */
#include "video.h"    /* backbuffer, SCREEN_WIDTH, OFFSET    */
#include "image.h"    /* loadImageZoneRawKey                 */
#include "font2.h"

#define FONT2_RAW  "images\\font.raw"

/* Buffer RAM : NULL si non charge. */
static unsigned char far *font2Sheet = NULL;


/* =========================================================
   UTILITAIRES INTERNES
   ========================================================= */

static int glyphPos(unsigned char c, int *outSrcX, int *outSrcY)
{
    int idx;
    if (c < FONT2_FIRST_CHAR || c > FONT2_LAST_CHAR) return 0;
    idx      = c - FONT2_FIRST_CHAR;
    *outSrcX = (idx % FONT2_COLS) * FONT2_CHAR_W;
    *outSrcY = (idx / FONT2_COLS) * FONT2_CHAR_H;
    return 1;
}

static int f2strlen(const char *s)
{
    int n = 0; while (s[n]) n++; return n;
}


/* =========================================================
   MODE DISQUE
   ========================================================= */

void font2DiskDrawChar(unsigned char c, int x, int y, int colorKey)
{
    int srcX, srcY;
    if (!glyphPos(c, &srcX, &srcY)) return;
    loadImageZoneRawKey(FONT2_RAW, FONT2_SHEET_W,
                        srcX, srcY,
                        FONT2_CHAR_W, FONT2_CHAR_H,
                        x, y,
                        colorKey);
}

void font2DiskDrawText(const char *text, int x, int y, int colorKey)
{
    int i;
    for (i = 0; text[i] != '\0'; i++)
        font2DiskDrawChar((unsigned char)text[i],
                          x + i * FONT2_CHAR_W, y, colorKey);
}

void font2DiskDrawTextCentered(const char *text, int y, int colorKey)
{
    int x = (SCREEN_WIDTH - f2strlen(text) * FONT2_CHAR_W) / 2;
    if (x < 0) x = 0;
    font2DiskDrawText(text, x, y, colorKey);
}


/* =========================================================
   MODE RAM — Gestion du buffer
   =========================================================
   La feuille fait FONT2_SHEET_W * FONT2_SHEET_H = 61440
   octets. Cette valeur depasse int 16 bits (max 32767),
   donc on utilise unsigned long partout.

   fread() sur DOS : le parametre count est size_t (16 bits),
   donc on ne peut pas lire 61440 octets en un seul appel
   (61440 > 65535 ? non, mais le comportement varie selon
   le runtime). On lit ligne par ligne (FONT2_SHEET_W octets
   par appel = 320 octets, sans risque) pour garantir la
   compatibilite avec tous les runtimes DOS.
   ========================================================= */

int font2RamLoad(void)
{
    FILE *f;
    unsigned long row;
    unsigned char far *dst;

    if (font2Sheet) return 1;   /* deja en RAM */

    /* _fmalloc prend un unsigned long sur Watcom large model. */
    font2Sheet = (unsigned char far *)_fmalloc(FONT2_SHEET_SIZE);
    if (!font2Sheet) return 0;

    f = fopen(FONT2_RAW, "rb");
    if (!f) { _ffree(font2Sheet); font2Sheet = NULL; return 0; }

    dst = font2Sheet;
    for (row = 0; row < FONT2_SHEET_H; row++)
    {
        /* Lire une ligne de FONT2_SHEET_W octets (320 < 32767). */
        if (fread(dst, 1, FONT2_SHEET_W, f) != FONT2_SHEET_W)
        {
            fclose(f);
            _ffree(font2Sheet);
            font2Sheet = NULL;
            return 0;
        }
        dst += FONT2_SHEET_W;
    }

    fclose(f);
    return 1;
}

void font2RamFree(void)
{
    if (font2Sheet) { _ffree(font2Sheet); font2Sheet = NULL; }
}

int font2RamIsLoaded(void)
{
    return font2Sheet != NULL;
}


/* =========================================================
   MODE RAM — Rendu
   ========================================================= */

void font2RamDrawChar(unsigned char c, int x, int y, int colorKey)
{
    int srcX, srcY, row, col;
    unsigned char far *dst;
    unsigned char pix;
    unsigned char ck;

    if (!glyphPos(c, &srcX, &srcY)) return;

    ck  = (unsigned char)colorKey;
    dst = backbuffer + OFFSET(x, y);

    for (row = 0; row < FONT2_CHAR_H; row++)
    {
        for (col = 0; col < FONT2_CHAR_W; col++)
        {
            /* Offset dans la feuille : calcul en unsigned long
               pour eviter le debordement 16 bits. */
            pix = font2Sheet[(unsigned long)(srcY + row)
                             * FONT2_SHEET_W + (srcX + col)];
            if (colorKey < 0 || pix != ck)
                dst[col] = pix;
        }
        dst += SCREEN_WIDTH;
    }
}

void font2RamDrawText(const char *text, int x, int y, int colorKey)
{
    int i;
    for (i = 0; text[i] != '\0'; i++)
        font2RamDrawChar((unsigned char)text[i],
                      x + i * FONT2_CHAR_W, y, colorKey);
}

void font2RamDrawTextCentered(const char *text, int y, int colorKey)
{
    int x = (SCREEN_WIDTH - f2strlen(text) * FONT2_CHAR_W) / 2;
    if (x < 0) x = 0;
    font2RamDrawText(text, x, y, colorKey);
}

unsigned char font2RamGetPixel(int sx, int sy)
{
    /* Offset unsigned long obligatoire : sy * 320 peut
       depasser 32767 des la ligne 103. */
    return font2Sheet[(unsigned long)sy * FONT2_SHEET_W + sx];
}
