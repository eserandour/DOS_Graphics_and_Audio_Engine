/* =========================================================
   PALOPT.C  Palette VGA optimisee (appels vers PALASM.ASM)
   ========================================================= */

#include <stdio.h>
#include <string.h>
#include <conio.h>
#include "palette.h"
#include "video.h"

/* =========================================================
   PALETTES GLOBALES
   ========================================================= */

Color defaultPalette[256];
Color workingPalette[256];
Color paletteA[256];
Color paletteB[256];
Color grayPalette[256];
Color pinkPalette[256];
Color rainbowPalette[256];

/* =========================================================
   DECLARATIONS FONCTIONS ASSEMBLEUR
   =========================================================
   En modele LARGE, Watcom passe :
     1er far ptr : offset->AX, segment->DX
     2e  far ptr : offset->BX, segment->CX
     args suivants : sur la pile
   Les fonctions sont declarees FAR (appels far automatiques).
   ========================================================= */

extern void far setPalAsm(Color far *pal);
extern void far getPalAsm(Color far *pal);
extern void far fadeAsm(Color far *pal, unsigned char t255);
extern void far lerpAsm(Color far *dest, Color far *palA,
                         Color far *palB, unsigned char t255);
extern void far cycLftAsm(Color far *pal, int start, int end);
extern void far cycRgtAsm(Color far *pal, int start, int end);

/* =========================================================
   FICHIER .PAL
   ========================================================= */

int loadPalette(const char *palFile)
{
    FILE *f;
    unsigned char buf[768];
    int i;

    f = fopen(palFile, "rb");
    if (!f) return PAL_ERR_FILE;

    if (fread(buf, 1, 768, f) != 768)
    {
        fclose(f);
        return PAL_ERR_READ;
    }
    fclose(f);

    for (i = 0; i < 256; i++)
    {
        workingPalette[i].r = buf[i * 3];
        workingPalette[i].g = buf[i * 3 + 1];
        workingPalette[i].b = buf[i * 3 + 2];
    }

    setPalette(workingPalette);
    return PAL_OK;
}

int savePalette(const Color *pal, const char *filename)
{
    FILE *f;
    int i;
    unsigned char buf[3];

    f = fopen(filename, "wb");
    if (!f) return 0;

    for (i = 0; i < 256; i++)
    {
        buf[0] = pal[i].r;
        buf[1] = pal[i].g;
        buf[2] = pal[i].b;
        if (fwrite(buf, 1, 3, f) != 3)
        {
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return 1;
}

/* =========================================================
   ACCES DAC VGA
   ========================================================= */

void setPaletteColor(unsigned char index,
                     unsigned char r,
                     unsigned char g,
                     unsigned char b)
{
    outp(0x3C8, index);
    outp(0x3C9, r);
    outp(0x3C9, g);
    outp(0x3C9, b);
}

void setPalette(Color *pal)
{
    setPalAsm(pal);
}

void getPalette(Color *pal)
{
    getPalAsm(pal);
}

/* =========================================================
   MANIPULATION DE PALETTE
   ========================================================= */

void copyPalette(Color *dest, Color *src)
{
    memcpy(dest, src, 256 * sizeof(Color));
}

void lerpPalette(Color *dest, Color *palA, Color *palB, float t)
{
    /* Conversion float->virgule fixe une seule fois hors boucle */
    unsigned char t255 = (unsigned char)(t * 255.0f + 0.5f);
    lerpAsm(dest, palA, palB, t255);
}

void fadePalette(Color *pal, float t)
{
    unsigned char t255 = (unsigned char)(t * 255.0f + 0.5f);
    fadeAsm(pal, t255);
}

void cyclePaletteLeft(Color *pal, int start, int end)
{
    cycLftAsm(pal, start, end);
    setPalette(pal);
}

void cyclePaletteRight(Color *pal, int start, int end)
{
    cycRgtAsm(pal, start, end);
    setPalette(pal);
}

/* =========================================================
   GENERATEURS (appeles une seule fois : C pur suffisant)
   ========================================================= */

void buildGrayPalette(Color *pal)
{
    int i;
    unsigned char v;
    for (i = 0; i < 256; i++)
    {
        v = (unsigned char)(i >> 2);
        pal[i].r = v;
        pal[i].g = v;
        pal[i].b = v;
    }
}

void buildPinkPalette(Color *pal)
{
    int i;
    pal[0].r = 0; pal[0].g = 0; pal[0].b = 0;
    for (i = 1; i < 256; i++)
    {
        pal[i].r = 63;
        pal[i].g = (unsigned char)((i * 63) / 255);
        pal[i].b = (unsigned char)((i * 63) / 255);
    }
}

void buildRainbowPalette(Color *pal)
{
    int i, hi;
    float h, f, r, g, b;

    pal[0].r = 0; pal[0].g = 0; pal[0].b = 0;

    for (i = 1; i < 256; i++)
    {
        h  = (float)(i - 1) / 255.0f * 360.0f;
        hi = (int)(h / 60.0f) % 6;
        f  = h / 60.0f - (int)(h / 60.0f);

        switch (hi)
        {
            case 0: r=1.0f;   g=f;      b=0.0f;   break;
            case 1: r=1.0f-f; g=1.0f;   b=0.0f;   break;
            case 2: r=0.0f;   g=1.0f;   b=f;      break;
            case 3: r=0.0f;   g=1.0f-f; b=1.0f;   break;
            case 4: r=f;      g=0.0f;   b=1.0f;   break;
            default:r=1.0f;   g=0.0f;   b=1.0f-f; break;
        }

        pal[i].r = (unsigned char)(r * 63.0f);
        pal[i].g = (unsigned char)(g * 63.0f);
        pal[i].b = (unsigned char)(b * 63.0f);
    }
}
