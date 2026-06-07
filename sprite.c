/* =========================================================
   SPRITE.C — Sprites préchargés en far heap, mode 13h
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode vidéo    : 13h (320x200, 256 couleurs, 1 octet/pixel)

   Voir sprite.h pour la documentation complète de l'API,
   les conventions colorKey, et les flux d'utilisation.
   ========================================================= */

#include <stdio.h>    /* FILE, fopen, fread, fclose          */
#include <malloc.h>   /* _fmalloc, _ffree                    */
#include <string.h>   /* _fmemcpy                            */
#include "video.h"    /* backbuffer, OFFSET, SCREEN_*        */
#include "sprite.h"

/* =========================================================
   MACROS INTERNES
   ========================================================= */

/* Copie len octets de src (far) vers dst (far).
   _fmemcpy gère correctement les pointeurs far 32 bits. */
#define FCOPY(dst, src, len)  _fmemcpy((dst), (src), (size_t)(len))

/* Adresse far d'un pixel (px, py) dans le backbuffer. */
#define BB_PTR(px, py)  (backbuffer + OFFSET((px),(py)))

/* Adresse far d'un pixel (px, py) dans les données d'un Sprite.
   Le stride source est spr->w (pas SCREEN_WIDTH). */
#define SPR_PTR(spr, px, py)  ((spr)->data + (long)(py) * (spr)->w + (px))


/* =========================================================
   SPRITE SIMPLE  (w*h <= 65 535)
   ========================================================= */

int spriteLoad(Sprite *spr, const char *rawFile, int w, int h)
{
    FILE *f;
    unsigned char rowBuf[320];
    long size = (long)w * h;
    int row;

    spr->data = NULL;
    spr->w    = w;
    spr->h    = h;

    /* Vérification de la limite _fmalloc 16 bits. */
    if (size > 65535L) return SPR_ERR_SIZE;

    spr->data = (unsigned char far *)_fmalloc((size_t)size);
    if (!spr->data) return SPR_ERR_MEM;

    f = fopen(rawFile, "rb");
    if (!f) { _ffree(spr->data); spr->data = NULL; return SPR_ERR_FILE; }

    for (row = 0; row < h; row++)
    {
        if (fread(rowBuf, 1, (size_t)w, f) != (size_t)w)
        {
            fclose(f);
            _ffree(spr->data);
            spr->data = NULL;
            return SPR_ERR_READ;
        }
        FCOPY(spr->data + (long)row * w, rowBuf, w);
    }

    fclose(f);
    return SPR_OK;
}

void spriteFree(Sprite *spr)
{
    if (spr->data)
    {
        _ffree(spr->data);
        spr->data = NULL;
    }
}


/* =========================================================
   spriteBlit — blit opaque du sprite entier
   =========================================================
   Copie spr->h lignes de spr->w octets depuis le far heap
   vers le backbuffer, avec clipping sur les quatre bords.

   Clipping :
     srcX0 / srcY0 : premier pixel source visible (quand le
                     sprite dépasse le bord gauche / haut).
     dstX0 / dstY0 : premier pixel destination dans le bb.
     blitW / blitH : dimensions de la zone effectivement
                     copiée après clipping.
   ========================================================= */

void spriteBlit(const Sprite *spr, int dstX, int dstY)
{
    int srcX0 = 0, srcY0 = 0;
    int dstX0 = dstX, dstY0 = dstY;
    int blitW = spr->w, blitH = spr->h;
    int row;
    unsigned char far *src;
    unsigned char far *dst;

    /* Clipping gauche. */
    if (dstX0 < 0) { srcX0 -= dstX0; blitW += dstX0; dstX0 = 0; }
    /* Clipping haut. */
    if (dstY0 < 0) { srcY0 -= dstY0; blitH += dstY0; dstY0 = 0; }
    /* Clipping droit. */
    if (dstX0 + blitW > SCREEN_WIDTH)  blitW = SCREEN_WIDTH  - dstX0;
    /* Clipping bas. */
    if (dstY0 + blitH > SCREEN_HEIGHT) blitH = SCREEN_HEIGHT - dstY0;

    /* Sprite entièrement hors écran. */
    if (blitW <= 0 || blitH <= 0) return;

    src = SPR_PTR(spr, srcX0, srcY0);
    dst = BB_PTR(dstX0, dstY0);

    for (row = 0; row < blitH; row++)
    {
        FCOPY(dst, src, blitW);
        src += spr->w;      /* ligne suivante dans la feuille  */
        dst += SCREEN_WIDTH; /* ligne suivante dans le backbuffer */
    }
}


/* =========================================================
   spriteBlitKey — blit avec colorKey
   =========================================================
   Même logique que spriteBlit mais les pixels d'index
   == colorKey ne sont pas écrits. La boucle interne écrit
   pixel par pixel uniquement pour les lignes concernées.
   colorKey < 0 → bascule en blit opaque (_fmemcpy).
   ========================================================= */

void spriteBlitKey(const Sprite *spr, int dstX, int dstY,
                   int colorKey)
{
    int srcX0 = 0, srcY0 = 0;
    int dstX0 = dstX, dstY0 = dstY;
    int blitW = spr->w, blitH = spr->h;
    int row, col;
    unsigned char far *src;
    unsigned char far *dst;
    unsigned char far *s;
    unsigned char far *d;
    unsigned char ck;

    /* Blit opaque si pas de colorKey. */
    if (colorKey < 0) { spriteBlit(spr, dstX, dstY); return; }

    /* Clipping. */
    if (dstX0 < 0) { srcX0 -= dstX0; blitW += dstX0; dstX0 = 0; }
    if (dstY0 < 0) { srcY0 -= dstY0; blitH += dstY0; dstY0 = 0; }
    if (dstX0 + blitW > SCREEN_WIDTH)  blitW = SCREEN_WIDTH  - dstX0;
    if (dstY0 + blitH > SCREEN_HEIGHT) blitH = SCREEN_HEIGHT - dstY0;
    if (blitW <= 0 || blitH <= 0) return;

    src = SPR_PTR(spr, srcX0, srcY0);
    dst = BB_PTR(dstX0, dstY0);
    ck  = (unsigned char)colorKey;

    for (row = 0; row < blitH; row++)
    {
        s = src;
        d = dst;
        for (col = 0; col < blitW; col++)
        {
            if (*s != ck) *d = *s;
            s++; d++;
        }
        src += spr->w;
        dst += SCREEN_WIDTH;
    }
}


/* =========================================================
   spriteBlitZone — blit d'une zone de la feuille, opaque
   =========================================================
   Extrait le rectangle (srcX, srcY, zoneW, zoneH) depuis
   la feuille et le copie dans le backbuffer en (dstX, dstY).
   Clipping sur les quatre bords du backbuffer.
   Aucun accès disque : tout est déjà en far heap.
   ========================================================= */

void spriteBlitZone(const Sprite *spr,
                    int srcX, int srcY, int zoneW, int zoneH,
                    int dstX, int dstY)
{
    int dstX0 = dstX, dstY0 = dstY;
    int sx0 = srcX, sy0 = srcY;
    int blitW = zoneW, blitH = zoneH;
    int row;
    unsigned char far *src;
    unsigned char far *dst;

    /* Clipping gauche. */
    if (dstX0 < 0) { sx0 -= dstX0; blitW += dstX0; dstX0 = 0; }
    /* Clipping haut. */
    if (dstY0 < 0) { sy0 -= dstY0; blitH += dstY0; dstY0 = 0; }
    /* Clipping droit. */
    if (dstX0 + blitW > SCREEN_WIDTH)  blitW = SCREEN_WIDTH  - dstX0;
    /* Clipping bas. */
    if (dstY0 + blitH > SCREEN_HEIGHT) blitH = SCREEN_HEIGHT - dstY0;

    if (blitW <= 0 || blitH <= 0) return;

    /* Pointeur source : pixel (sx0, sy0) dans la feuille.
       Stride source = spr->w (largeur totale de la feuille). */
    src = spr->data + (long)sy0 * spr->w + sx0;
    dst = BB_PTR(dstX0, dstY0);

    for (row = 0; row < blitH; row++)
    {
        FCOPY(dst, src, blitW);
        src += spr->w;
        dst += SCREEN_WIDTH;
    }
}


/* =========================================================
   spriteBlitZoneKey — blit d'une zone avec colorKey
   ========================================================= */

void spriteBlitZoneKey(const Sprite *spr,
                       int srcX, int srcY, int zoneW, int zoneH,
                       int dstX, int dstY,
                       int colorKey)
{
    int dstX0 = dstX, dstY0 = dstY;
    int sx0 = srcX, sy0 = srcY;
    int blitW = zoneW, blitH = zoneH;
    int row, col;
    unsigned char far *src;
    unsigned char far *dst;
    unsigned char far *s;
    unsigned char far *d;
    unsigned char ck;

    if (colorKey < 0)
    {
        spriteBlitZone(spr, srcX, srcY, zoneW, zoneH, dstX, dstY);
        return;
    }

    /* Clipping. */
    if (dstX0 < 0) { sx0 -= dstX0; blitW += dstX0; dstX0 = 0; }
    if (dstY0 < 0) { sy0 -= dstY0; blitH += dstY0; dstY0 = 0; }
    if (dstX0 + blitW > SCREEN_WIDTH)  blitW = SCREEN_WIDTH  - dstX0;
    if (dstY0 + blitH > SCREEN_HEIGHT) blitH = SCREEN_HEIGHT - dstY0;
    if (blitW <= 0 || blitH <= 0) return;

    src = spr->data + (long)sy0 * spr->w + sx0;
    dst = BB_PTR(dstX0, dstY0);
    ck  = (unsigned char)colorKey;

    for (row = 0; row < blitH; row++)
    {
        s = src;
        d = dst;
        for (col = 0; col < blitW; col++)
        {
            if (*s != ck) *d = *s;
            s++; d++;
        }
        src += spr->w;
        dst += SCREEN_WIDTH;
    }
}


/* =========================================================
   SPRITE SPLIT  (w*h > 65 535, max 2 × 32 768 octets)
   =========================================================
   Même principe que le split manuel de scene6.c (tex0/tex1).
   Les octets 0..32767      → blk[0]
   Les octets 32768..w*h-1  → blk[1]

   Les blits reconstituent les lignes en testant si elles
   chevauchent la frontière entre les deux blocs.
   ========================================================= */

int spriteLoadSplit(SpriteSplit *spr, const char *rawFile,
                    int w, int h)
{
    FILE *f;
    unsigned char rowBuf[320];
    unsigned char far *dst0;
    unsigned char far *dst1;
    long written = 0;
    long half = 32768L;
    int row, chunk0, chunk1;

    spr->blk[0] = NULL;
    spr->blk[1] = NULL;
    spr->w = w;
    spr->h = h;

    spr->blk[0] = (unsigned char far *)_fmalloc(32768U);
    if (!spr->blk[0]) return SPR_ERR_MEM;

    spr->blk[1] = (unsigned char far *)_fmalloc(32768U);
    if (!spr->blk[1]) { _ffree(spr->blk[0]); spr->blk[0] = NULL; return SPR_ERR_MEM; }

    f = fopen(rawFile, "rb");
    if (!f)
    {
        _ffree(spr->blk[0]); _ffree(spr->blk[1]);
        spr->blk[0] = spr->blk[1] = NULL;
        return SPR_ERR_FILE;
    }

    dst0 = spr->blk[0];
    dst1 = spr->blk[1];

    for (row = 0; row < h; row++)
    {
        if (fread(rowBuf, 1, (size_t)w, f) != (size_t)w)
        {
            fclose(f);
            _ffree(spr->blk[0]); _ffree(spr->blk[1]);
            spr->blk[0] = spr->blk[1] = NULL;
            return SPR_ERR_READ;
        }

        /* Écrire la ligne dans blk[0] et/ou blk[1] selon
           la position de la frontière (offset 32768). */
        if (written + w <= half)
        {
            /* Ligne entière dans blk[0]. */
            FCOPY(dst0, rowBuf, w);
            dst0 += w;
        }
        else if (written >= half)
        {
            /* Ligne entière dans blk[1]. */
            FCOPY(dst1, rowBuf, w);
            dst1 += w;
        }
        else
        {
            /* Ligne à cheval sur les deux blocs. */
            chunk0 = (int)(half - written);   /* octets dans blk[0] */
            chunk1 = w - chunk0;              /* octets dans blk[1] */
            FCOPY(dst0, rowBuf,          chunk0);
            FCOPY(dst1, rowBuf + chunk0, chunk1);
            dst0 += chunk0;
            dst1 += chunk1;
        }

        written += w;
    }

    fclose(f);
    return SPR_OK;
}

void spriteFreeSplit(SpriteSplit *spr)
{
    if (spr->blk[0]) { _ffree(spr->blk[0]); spr->blk[0] = NULL; }
    if (spr->blk[1]) { _ffree(spr->blk[1]); spr->blk[1] = NULL; }
}


/* =========================================================
   spriteBlitSplit — blit opaque d'un SpriteSplit
   =========================================================
   Pour chaque ligne de la zone blittée, on récupère un
   pointeur far dans blk[0] ou blk[1] selon l'offset absolu.
   Les lignes à cheval sur les deux blocs sont copiées en
   deux passes (chunk0 + chunk1) vers un rowBuf near, puis
   le rowBuf est copié dans le backbuffer.
   ========================================================= */

void spriteBlitSplit(const SpriteSplit *spr, int dstX, int dstY)
{
    int srcY0 = 0;
    int dstX0 = dstX, dstY0 = dstY;
    int blitW = spr->w, blitH = spr->h;
    unsigned char rowBuf[320];
    unsigned char far *dst;
    int row;
    long offset;
    int chunk0, chunk1;
    long half = 32768L;

    /* Clipping haut. */
    if (dstY0 < 0) { srcY0 -= dstY0; blitH += dstY0; dstY0 = 0; }
    /* Clipping bas. */
    if (dstY0 + blitH > SCREEN_HEIGHT) blitH = SCREEN_HEIGHT - dstY0;
    /* Clipping gauche/droit : pour simplifier, blitW reste spr->w.
       Pour un clipping complet en X, voir spriteBlit. */
    if (dstX0 < 0) dstX0 = 0;
    if (blitW <= 0 || blitH <= 0) return;

    dst = BB_PTR(dstX0, dstY0);

    for (row = 0; row < blitH; row++)
    {
        offset = (long)(srcY0 + row) * spr->w;

        if (offset + blitW <= half)
        {
            /* Ligne entière dans blk[0]. */
            FCOPY(rowBuf, spr->blk[0] + offset, blitW);
        }
        else if (offset >= half)
        {
            /* Ligne entière dans blk[1]. */
            FCOPY(rowBuf, spr->blk[1] + (offset - half), blitW);
        }
        else
        {
            /* Ligne à cheval. */
            chunk0 = (int)(half - offset);
            chunk1 = blitW - chunk0;
            FCOPY(rowBuf,          spr->blk[0] + offset, chunk0);
            FCOPY(rowBuf + chunk0, spr->blk[1],          chunk1);
        }

        FCOPY(dst, rowBuf, blitW);
        dst += SCREEN_WIDTH;
    }
}


/* =========================================================
   spriteBlitSplitKey — blit avec colorKey d'un SpriteSplit
   ========================================================= */

void spriteBlitSplitKey(const SpriteSplit *spr, int dstX, int dstY,
                        int colorKey)
{
    int srcY0 = 0;
    int dstX0 = dstX, dstY0 = dstY;
    int blitW = spr->w, blitH = spr->h;
    unsigned char rowBuf[320];
    unsigned char far *dst;
    unsigned char far *d;
    unsigned char *s;
    int row, col;
    long offset;
    int chunk0, chunk1;
    long half = 32768L;
    unsigned char ck;

    if (colorKey < 0) { spriteBlitSplit(spr, dstX, dstY); return; }

    if (dstY0 < 0) { srcY0 -= dstY0; blitH += dstY0; dstY0 = 0; }
    if (dstY0 + blitH > SCREEN_HEIGHT) blitH = SCREEN_HEIGHT - dstY0;
    if (dstX0 < 0) dstX0 = 0;
    if (blitW <= 0 || blitH <= 0) return;

    dst = BB_PTR(dstX0, dstY0);
    ck  = (unsigned char)colorKey;

    for (row = 0; row < blitH; row++)
    {
        offset = (long)(srcY0 + row) * spr->w;

        if (offset + blitW <= half)
            FCOPY(rowBuf, spr->blk[0] + offset, blitW);
        else if (offset >= half)
            FCOPY(rowBuf, spr->blk[1] + (offset - half), blitW);
        else
        {
            chunk0 = (int)(half - offset);
            chunk1 = blitW - chunk0;
            FCOPY(rowBuf,          spr->blk[0] + offset, chunk0);
            FCOPY(rowBuf + chunk0, spr->blk[1],          chunk1);
        }

        s = rowBuf;
        d = dst;
        for (col = 0; col < blitW; col++)
        {
            if (*s != ck) *d = *s;
            s++; d++;
        }

        dst += SCREEN_WIDTH;
    }
}
