/* =========================================================
   SCENE7.C — Scène : démonstration des primitives graphics
   =========================================================
   Mise en oeuvre de TOUTES les fonctions de graphics.h :
     putPixel, getPixel, clearScreen
     drawLine, drawRect, drawRectFill
     drawPolygon, drawPolygonFill
     drawCircle, drawCircleFill

   Structure en 6 phases de 3 s chacune (18 s au total) :
     Phase 1 — clearScreen + putPixel (pluie de pixels)
     Phase 2 — drawLine  (étoile de lignes depuis le centre)
     Phase 3 — drawRect / drawRectFill (grille de rectangles)
     Phase 4 — drawCircle / drawCircleFill (ondes concentriques)
     Phase 5 — drawPolygon / drawPolygonFill (étoile 5 branches
                + triangle tournant, getPixel pour XOR)
     Phase 6 — composition finale : toutes les primitives
                animées ensemble

   Fade in 1 s / fade out 1 s sur la durée totale.
   Aucune gestion clavier (sauf Échap global via INT 09h).
   Cible : Open Watcom 1.9, FreeDOS, mode 13h (320x200).
   ========================================================= */

#include <math.h>       /* sin, cos, fabs                  */
#include <stdlib.h>     /* abs                             */
#include "timer.h"
#include "video.h"
#include "palette.h"
#include "graphics.h"
#include "scene.h"

/* =========================================================
   CONSTANTES
   ========================================================= */

#define PI          3.14159265f
#define TWO_PI      6.28318530f

#define PHASE_MS    3000UL      /* durée d'une phase (ms)   */
#define NB_PHASES   6
#define SCENE_MS    (NB_PHASES * PHASE_MS)  /* 18 s          */
#define FADE_MS     1000UL      /* fade in / fade out        */

#define CX          160         /* centre écran X            */
#define CY          100         /* centre écran Y            */

/* =========================================================
   UTILITAIRES
   ========================================================= */

/* Retourne un index de couleur entre 1 et 255 (jamais 0). */
static unsigned char hueColor(int offset)
{
    return (unsigned char)(((unsigned int)(offset & 0xFF) % 255) + 1);
}

/* =========================================================
   PHASE 1 — clearScreen + putPixel
   =========================================================
   Affiche une "pluie de pixels" : chaque frame on efface et
   on dessine N pixels aléatoires avec putPixel.
   getPixel est utilisé pour n'écrire que sur le fond noir
   (pixel inchangé si déjà coloré → effet d'accumulation). */

static void phase1(unsigned long t_ms)
{
    /* LCG interne (state persistant entre appels). */
    static unsigned long lcg = 12345UL;
    int i, x, y;
    unsigned char col;
    /* Nombre de pixels proportionnel au temps (montée en charge). */
    int n = 200 + (int)((float)t_ms / (float)PHASE_MS * 1800.0f);

    clearScreen(0);

    for (i = 0; i < n; i++)
    {
        lcg = lcg * 1664525UL + 1013904223UL;
        x   = (int)((lcg >> 16) % SCREEN_WIDTH);
        lcg = lcg * 1664525UL + 1013904223UL;
        y   = (int)((lcg >> 16) % SCREEN_HEIGHT);
        lcg = lcg * 1664525UL + 1013904223UL;
        col = hueColor((int)(lcg >> 16));

        /* getPixel : on ne redessine pas un pixel déjà posé. */
        if (getPixel(x, y) == 0)
            putPixel(x, y, col);
    }
}

/* =========================================================
   PHASE 2 — drawLine
   =========================================================
   Étoile de 36 rayons depuis le centre, qui tourne dans
   le temps. Les rayons dépassent les bords : le clipping
   de drawLine (Cohen-Sutherland) les tronque proprement. */

static void phase2(unsigned long t_ms)
{
    float angle;
    float progress = (float)t_ms / (float)PHASE_MS;   /* 0→1 */
    float baseAngle = progress * TWO_PI;               /* rotation */
    int   i, x2, y2;
    int   len = 200;   /* longueur des rayons (dépasse l'écran) */
    unsigned char col;

    clearScreen(0);

    for (i = 0; i < 36; i++)
    {
        angle = baseAngle + (float)i * (TWO_PI / 36.0f);
        x2    = CX + (int)((float)cos(angle) * (float)len);
        y2    = CY + (int)((float)sin(angle) * (float)len);

        /* Teinte progressive sur les 36 rayons. */
        col = hueColor(i * 7 + (int)(progress * 80.0f));
        drawLine(CX, CY, x2, y2, col);
    }

    /* Deuxième couche : 18 rayons plus courts en sens inverse. */
    for (i = 0; i < 18; i++)
    {
        angle = -baseAngle * 0.5f + (float)i * (TWO_PI / 18.0f);
        x2    = CX + (int)((float)cos(angle) * 80.0f);
        y2    = CY + (int)((float)sin(angle) * 80.0f);
        col   = hueColor(i * 14 + 128 + (int)(progress * 60.0f));
        drawLine(CX, CY, x2, y2, col);
    }
}

/* =========================================================
   PHASE 3 — drawRect + drawRectFill
   =========================================================
   Grille de rectangles pleins au fond, puis contours animés
   qui se déplacent. Certains rectangles dépassent les bords
   pour valider le clipping de drawRectFill. */

static void phase3(unsigned long t_ms)
{
    float progress = (float)t_ms / (float)PHASE_MS;
    int   i, j;
    int   x, y, size;
    unsigned char col;
    /* Déplacement horizontal pour les rectangles de contour. */
    int   shift = (int)(progress * 60.0f) - 30;

    clearScreen(0);

    /* Fond : grille 8x5 de rectangles pleins (10 px de marge). */
    for (j = 0; j < 5; j++)
        for (i = 0; i < 8; i++)
        {
            x   = 10 + i * 38;
            y   = 10 + j * 36;
            col = hueColor(i * 8 + j * 33 + (int)(progress * 40.0f));
            drawRectFill(x, y, x + 30, y + 28, col);
        }

    /* Contours : série de rectangles concentriques centrés,
       qui s'élargissent avec le temps (clipping en jeu
       pour les grands). */
    for (i = 0; i < 12; i++)
    {
        size = 10 + i * 14 + (int)(progress * 30.0f);
        col  = hueColor(i * 21 + (int)(progress * 50.0f));
        drawRect(CX - size + shift, CY - size,
                 CX + size + shift, CY + size, col);
    }
}

/* =========================================================
   PHASE 4 — drawCircle + drawCircleFill
   =========================================================
   Ondes concentriques : cercles remplis de couleurs
   différentes dont le rayon croit avec le temps, puis des
   cercles de contour en surimpression.
   Des cercles partiellement hors écran valident le clipping. */

static void phase4(unsigned long t_ms)
{
    float progress = (float)t_ms / (float)PHASE_MS;
    int   i, r;
    int   maxR = 220;   /* rayon max > écran → clipping activé */
    unsigned char col;

    clearScreen(0);

    /* Onde principale : cercles remplis concentriques du
       plus grand au plus petit (ordre de dessin important). */
    for (i = 12; i >= 0; i--)
    {
        r   = (int)(progress * (float)maxR) - i * 15;
        col = hueColor(i * 19 + (int)(progress * 120.0f));
        if (r > 0)
            drawCircleFill(CX, CY, r, col);
    }

    /* Second centre en bas à gauche (partiellement hors écran). */
    for (i = 0; i < 8; i++)
    {
        r   = 20 + i * 18 + (int)(progress * 40.0f);
        col = hueColor(i * 31 + 64 + (int)(progress * 80.0f));
        drawCircle(-20, 220, r, col);   /* clipping Cohen-S. */
    }

    /* Cercles de contour animés par-dessus. */
    for (i = 0; i < 6; i++)
    {
        r   = 20 + i * 22 + (int)((float)sin(progress * TWO_PI + i) * 10.0f);
        col = hueColor(i * 41 + 200 + (int)(progress * 60.0f));
        drawCircle(CX, CY, r, col);
    }
}

/* =========================================================
   PHASE 5 — drawPolygon + drawPolygonFill + getPixel
   =========================================================
   - Étoile 5 branches remplie au centre.
   - Triangle tournant en contour.
   - getPixel utilisé pour un effet XOR : chaque pixel
     déjà coloré est réécrit avec sa couleur complémentaire
     (décalage de 128 dans la palette). */

/* Calcule les sommets d'une étoile à 5 branches. */
static void starPoints(int cx, int cy,
                       int r_out, int r_in,
                       float angle, int *pts)
{
    int i;
    float a;
    for (i = 0; i < 5; i++)
    {
        /* Sommet externe. */
        a           = angle + (float)i * (TWO_PI / 5.0f);
        pts[i*4+0]  = cx + (int)((float)cos(a) * (float)r_out);
        pts[i*4+1]  = cy + (int)((float)sin(a) * (float)r_out);
        /* Sommet interne (entre deux branches). */
        a           = angle + ((float)i + 0.5f) * (TWO_PI / 5.0f);
        pts[i*4+2]  = cx + (int)((float)cos(a) * (float)r_in);
        pts[i*4+3]  = cy + (int)((float)sin(a) * (float)r_in);
    }
}

static void phase5(unsigned long t_ms)
{
    float progress  = (float)t_ms / (float)PHASE_MS;
    float angle     = progress * TWO_PI;
    /* pts_star : 10 sommets interleaved (externe/interne). */
    int   pts_star[20];
    /* pts_tri  : 3 sommets de triangle. */
    int   pts_tri[6];
    unsigned char col, existing;
    int   i, x, y;
    float a;

    clearScreen(0);

    /* Étoile remplie (10 sommets = 5 externes + 5 internes
       en alternance). */
    starPoints(CX, CY, 80, 35, angle, pts_star);
    col = hueColor((int)(progress * 150.0f));
    drawPolygonFill(pts_star, 10, col);

    /* Contour de l'étoile avec une teinte décalée. */
    col = hueColor((int)(progress * 150.0f) + 128);
    drawPolygon(pts_star, 10, col);

    /* Triangle tournant en sens inverse. */
    for (i = 0; i < 3; i++)
    {
        a            = -angle * 0.7f + (float)i * (TWO_PI / 3.0f);
        pts_tri[i*2]   = CX + (int)((float)cos(a) * 110.0f);
        pts_tri[i*2+1] = CY + (int)((float)sin(a) * 110.0f);
    }
    col = hueColor((int)(progress * 90.0f) + 85);
    drawPolygonFill(pts_tri, 3, col);
    drawPolygon(pts_tri, 3, 255);   /* contour blanc */

    /* Effet getPixel / XOR palette :
       Pour chaque pixel de la diagonale principale, on lit
       sa couleur et on la "complémente" en décalant l'index
       de 127 dans la palette arc-en-ciel. */
    for (i = 0; i < SCREEN_WIDTH; i++)
    {
        x        = i;
        y        = (int)((float)i * (float)SCREEN_HEIGHT
                          / (float)SCREEN_WIDTH);
        existing = getPixel(x, y);
        if (existing != 0)
        {
            col = (unsigned char)(((int)existing + 127) % 255 + 1);
            putPixel(x, y, col);
        }
    }
}

/* =========================================================
   PHASE 6 — Composition finale
   =========================================================
   Toutes les primitives en même temps :
   - fond : clearScreen puis drawRectFill en bordure
   - étoile de lignes (drawLine)
   - cercle rempli central (drawCircleFill)
   - cercles de contour (drawCircle)
   - polygone rempli (drawPolygonFill)
   - rectangle de contour (drawRect)
   - putPixel pour des petits ornements */

static void phase6(unsigned long t_ms)
{
    float progress = (float)t_ms / (float)PHASE_MS;
    float angle    = progress * TWO_PI;
    int   i, pts[8];
    int   x2, y2;
    unsigned char col;
    float a;

    clearScreen(0);

    /* Bordure : 4 rectangles pleins fins sur les bords. */
    col = hueColor((int)(progress * 30.0f));
    drawRectFill(0, 0, SCREEN_WIDTH-1, 3, col);
    drawRectFill(0, SCREEN_HEIGHT-4, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, col);
    drawRectFill(0, 0, 3, SCREEN_HEIGHT-1, col);
    drawRectFill(SCREEN_WIDTH-4, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, col);

    /* Rayons (drawLine) depuis le centre. */
    for (i = 0; i < 24; i++)
    {
        a   = angle + (float)i * (TWO_PI / 24.0f);
        x2  = CX + (int)((float)cos(a) * 250.0f);
        y2  = CY + (int)((float)sin(a) * 250.0f);
        col = hueColor(i * 10 + (int)(progress * 60.0f));
        drawLine(CX, CY, x2, y2, col);
    }

    /* Disque central (drawCircleFill). */
    col = hueColor(200 + (int)(progress * 55.0f));
    drawCircleFill(CX, CY, 40, col);

    /* Anneau de cercles de contour (drawCircle). */
    for (i = 0; i < 8; i++)
    {
        a   = angle * 1.3f + (float)i * (TWO_PI / 8.0f);
        x2  = CX + (int)((float)cos(a) * 70.0f);
        y2  = CY + (int)((float)sin(a) * 70.0f);
        col = hueColor(i * 32 + (int)(progress * 80.0f));
        drawCircle(x2, y2, 18, col);
    }

    /* Losange rempli (drawPolygonFill, 4 sommets). */
    {
        int size = 25 + (int)((float)sin(angle * 2.0f) * 10.0f);
        pts[0] = CX;        pts[1] = CY - size;
        pts[2] = CX + size; pts[3] = CY;
        pts[4] = CX;        pts[5] = CY + size;
        pts[6] = CX - size; pts[7] = CY;
        col    = hueColor(80 + (int)(progress * 120.0f));
        drawPolygonFill(pts, 4, col);
    }

    /* Rectangle de contour animé (drawRect). */
    {
        int margin = 15 + (int)((float)sin(angle) * 8.0f);
        col = hueColor(170 + (int)(progress * 40.0f));
        drawRect(margin, margin,
                 SCREEN_WIDTH-1-margin, SCREEN_HEIGHT-1-margin, col);
    }

    /* Ornements putPixel : couronne de points autour du disque. */
    for (i = 0; i < 64; i++)
    {
        a   = angle * 2.0f + (float)i * (TWO_PI / 64.0f);
        x2  = CX + (int)((float)cos(a) * 50.0f);
        y2  = CY + (int)((float)sin(a) * 50.0f);
        col = hueColor(i * 4 + (int)(progress * 100.0f));
        putPixel(x2, y2, col);
    }
}

/* =========================================================
   SCÈNE PRINCIPALE
   ========================================================= */

void scene7(void)
{
    static int           initialized = 0;
    static unsigned long lastRender  = 0UL;
    static int           lastPhase   = -1;

    const unsigned long render_ms = 33UL;   /* ~30 fps */

    unsigned long now     = readTimer();
    unsigned long elapsed = elapsedTimeMs(sceneStart, now);
    int           phase;
    unsigned long phase_t;
    float         fade_t;

    if (!initialized)
    {
        initialized = 1;
        lastRender  = now;
        lastPhase   = -1;

        buildRainbowPalette(rainbowPalette);
        copyPalette(workingPalette, rainbowPalette);
        setPalette(workingPalette);
        clearScreen(0);
        flip();
    }

    /* -------------------------------------------------------
       Calcul de la phase courante et du temps dans la phase
       ------------------------------------------------------- */
    if (elapsed >= SCENE_MS)
    {
        initialized = 0;
        sceneSignalEnd();
        return;
    }

    phase   = (int)(elapsed / PHASE_MS);
    if (phase >= NB_PHASES) phase = NB_PHASES - 1;
    phase_t = elapsed - (unsigned long)phase * PHASE_MS;

    /* -------------------------------------------------------
       Rendu (interval-based, ~30 fps)
       ------------------------------------------------------- */
    if (elapsedTimeMs(lastRender, now) < render_ms)
        goto do_fade;   /* pas encore l'heure de redessiner */

    lastRender = now;

    /* clearScreen appelé dans chaque phase, sauf transition
       (on efface ici si la phase vient de changer). */
    if (phase != lastPhase)
    {
        clearScreen(0);
        lastPhase = phase;
    }

    switch (phase)
    {
        case 0: phase1(phase_t); break;
        case 1: phase2(phase_t); break;
        case 2: phase3(phase_t); break;
        case 3: phase4(phase_t); break;
        case 4: phase5(phase_t); break;
        case 5: phase6(phase_t); break;
    }

    flip();

do_fade:
    /* -------------------------------------------------------
       Calcul du facteur de fondu global
       ------------------------------------------------------- */
    if (elapsed < FADE_MS)
        fade_t = (float)elapsed / (float)FADE_MS;
    else if (elapsed >= SCENE_MS - FADE_MS)
    {
        fade_t = (float)(SCENE_MS - elapsed) / (float)FADE_MS;
        if (fade_t < 0.0f) fade_t = 0.0f;
    }
    else
        fade_t = 1.0f;

    fadePalette(workingPalette, fade_t);
}
