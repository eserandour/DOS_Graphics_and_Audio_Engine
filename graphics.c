/* =========================================================
   GRAPHICS.C — Primitives de dessin 2D en mode 13h
   ========================================================= */

#include <stdlib.h>   /* abs                               */
#include <string.h>   /* _fmemset                         */
#include "video.h"    /* backbuffer, OFFSET, SCREEN_*      */
#include "graphics.h"

/* =========================================================
   EFFACEMENT
   ========================================================= */

/* Remplit le backbuffer entier avec une seule couleur.
   _fmemset est la version far de memset : nécessaire car
   backbuffer est un pointeur far (segment:offset 32 bits). */
void clearScreen(unsigned char color)
{
    _fmemset(backbuffer, color, BACKBUFFER_SIZE);
}

/* =========================================================
   PIXEL
   ========================================================= */

/* Écrit un pixel à l'offset calculé par la macro OFFSET.
   OFFSET(x, y) = y*320 + x, optimisé avec des shifts. */
void putPixel(int x, int y, unsigned char color)
{
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT)
        backbuffer[OFFSET(x, y)] = color;
}

/* Lit la couleur (index palette) d'un pixel du backbuffer. */
unsigned char getPixel(int x, int y)
{
    return backbuffer[OFFSET(x, y)];
}

/* =========================================================
   LIGNE — Algorithme de Bresenham + Clipping Cohen-Sutherland
   =========================================================
   Le clipping Cohen-Sutherland divise le plan en 9 zones
   à l'aide de 4 bits (OUT_LEFT, OUT_RIGHT, OUT_BOTTOM,
   OUT_TOP). On classe chaque extrémité dans sa zone, puis :
   - si les deux codes = 0 → segment entièrement visible.
   - si (code1 & code2) ≠ 0 → segment entièrement dehors.
   - sinon → on calcule l'intersection avec la frontière
     correspondante et on recommence.
   Avantage : rejette rapidement les segments hors écran
   sans aucune division.
   ========================================================= */

#define CS_OUT_LEFT   1
#define CS_OUT_RIGHT  2
#define CS_OUT_BOTTOM 4
#define CS_OUT_TOP    8

static int cs_code(int x, int y)
{
    int code = 0;
    if (x < 0)                code |= CS_OUT_LEFT;
    if (x >= SCREEN_WIDTH)    code |= CS_OUT_RIGHT;
    if (y < 0)                code |= CS_OUT_TOP;
    if (y >= SCREEN_HEIGHT)   code |= CS_OUT_BOTTOM;
    return code;
}

void drawLine(int x1, int y1, int x2, int y2, unsigned char color)
{
    int code1 = cs_code(x1, y1);
    int code2 = cs_code(x2, y2);
    int code, dx, dy, sx, sy, err, e2;

    /* Boucle de clipping Cohen-Sutherland. */
    while (1)
    {
        if (!(code1 | code2))
            break;              /* entièrement visible → on sort */

        if (code1 & code2)
            return;             /* entièrement hors écran → rien à dessiner */

        /* Choisir le point à déplacer (priorité à code1). */
        code = code1 ? code1 : code2;

        /* Calculer l'intersection avec la frontière touchée.
           Formule : x = x1 + (x2-x1)*(bord_y - y1)/(y2-y1)
           et son symétrique pour l'axe Y.
           On évite la division par 0 : les arêtes parallèles
           à l'axe Y ne peuvent pas sortir par OUT_TOP/OUT_BOTTOM
           et vice-versa, donc dy et dx sont ≠ 0 dans chaque cas. */
        dx = x2 - x1;
        dy = y2 - y1;

        if (code & CS_OUT_BOTTOM)           /* sortie par le bas */
        {
            int nx = x1 + dx * (SCREEN_HEIGHT - 1 - y1) / dy;
            int ny = SCREEN_HEIGHT - 1;
            if (code == code1) { x1 = nx; y1 = ny; code1 = cs_code(x1, y1); }
            else               { x2 = nx; y2 = ny; code2 = cs_code(x2, y2); }
        }
        else if (code & CS_OUT_TOP)         /* sortie par le haut */
        {
            int nx = x1 + dx * (-y1) / dy;
            int ny = 0;
            if (code == code1) { x1 = nx; y1 = ny; code1 = cs_code(x1, y1); }
            else               { x2 = nx; y2 = ny; code2 = cs_code(x2, y2); }
        }
        else if (code & CS_OUT_RIGHT)       /* sortie par la droite */
        {
            int ny = y1 + dy * (SCREEN_WIDTH - 1 - x1) / dx;
            int nx = SCREEN_WIDTH - 1;
            if (code == code1) { x1 = nx; y1 = ny; code1 = cs_code(x1, y1); }
            else               { x2 = nx; y2 = ny; code2 = cs_code(x2, y2); }
        }
        else                                /* sortie par la gauche */
        {
            int ny = y1 + dy * (-x1) / dx;
            int nx = 0;
            if (code == code1) { x1 = nx; y1 = ny; code1 = cs_code(x1, y1); }
            else               { x2 = nx; y2 = ny; code2 = cs_code(x2, y2); }
        }
    }

    /* Segment clippé → tracé par Bresenham. */
    dx  = abs(x2 - x1);
    dy  = abs(y2 - y1);
    sx  = (x1 < x2) ? 1 : -1;
    sy  = (y1 < y2) ? 1 : -1;
    err = dx - dy;

    while (1)
    {
        /* Les coordonnées sont désormais garanties dans les bornes :
           putPixel sans vérification serait possible, mais on la
           garde pour la robustesse. */
        putPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = err << 1;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 <  dx) { err += dx; y1 += sy; }
    }
}

/* =========================================================
   RECTANGLE
   ========================================================= */

/* Contour : 4 segments reliant les 4 coins. */
void drawRect(int x1, int y1, int x2, int y2, unsigned char color)
{
    drawLine(x1, y1, x2, y1, color);   /* bord haut   */
    drawLine(x2, y1, x2, y2, color);   /* bord droit  */
    drawLine(x2, y2, x1, y2, color);   /* bord bas    */
    drawLine(x1, y2, x1, y1, color);   /* bord gauche */
}

/* Rectangle plein : remplissage ligne par ligne avec clipping.
   On clamp les coordonnées sur les bornes de l'écran avant de
   commencer : une seule vérification hors de la boucle, puis
   _fmemset travaille uniquement sur des lignes valides. */
void drawRectFill(int x1, int y1, int x2, int y2, unsigned char color)
{
    int y;

    /* Normaliser : s'assurer que x1 ≤ x2 et y1 ≤ y2. */
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }

    /* Clipping : restreindre aux bornes de l'écran. */
    if (x1 < 0)              x1 = 0;
    if (x2 >= SCREEN_WIDTH)  x2 = SCREEN_WIDTH  - 1;
    if (y1 < 0)              y1 = 0;
    if (y2 >= SCREEN_HEIGHT) y2 = SCREEN_HEIGHT - 1;

    /* Rectangle entièrement hors écran. */
    if (x1 > x2 || y1 > y2) return;

    for (y = y1; y <= y2; y++)
        _fmemset(backbuffer + OFFSET(x1, y), color, x2 - x1 + 1);
}

/* =========================================================
   POLYGONE
   ========================================================= */

/* Contour : on trace un segment entre chaque paire de
   sommets consécutifs, puis on ferme le polygone en reliant
   le dernier sommet au premier. */
void drawPolygon(int *pts, int n, unsigned char color)
{
    int i;
    if (n < 2) return;
    for (i = 0; i < n - 1; i++)
        drawLine(pts[i*2], pts[i*2+1], pts[i*2+2], pts[i*2+3], color);

    /* Fermeture : dernier sommet → premier sommet. */
    drawLine(pts[(n-1)*2], pts[(n-1)*2+1], pts[0], pts[1], color);
}

/* =========================================================
   POLYGONE PLEIN — Scanline filling
   =========================================================
   Principe (algorithme "even-odd rule") :
   Pour chaque ligne horizontale (scanline y) :
   1. Trouver toutes les intersections avec les arêtes.
   2. Trier ces intersections par ordre croissant de X.
   3. Remplir les pixels entre les paires (x0,x1), (x2,x3)...
   ========================================================= */

void drawPolygonFill(int *pts, int n, unsigned char color)
{
    int i, j;
    int y, ymin, ymax;
    int x1, y1, x2, y2;
    int intersections[SCREEN_WIDTH];  /* liste des X d'intersection */
    int count, tmp, dx, dy, x;

    if (n < 3) return;   /* il faut au moins un triangle */

    /* Trouver les bornes verticales du polygone. */
    ymin = pts[1]; ymax = pts[1];
    for (i = 1; i < n; i++)
    {
        if (pts[i*2+1] < ymin) ymin = pts[i*2+1];
        if (pts[i*2+1] > ymax) ymax = pts[i*2+1];
    }

    /* Clipping vertical : ne traiter que les lignes visibles. */
    if (ymin < 0)              ymin = 0;
    if (ymax >= SCREEN_HEIGHT) ymax = SCREEN_HEIGHT - 1;

    /* Parcourir chaque scanline dans les bornes verticales. */
    for (y = ymin; y <= ymax; y++)
    {
        count = 0;   /* nombre d'intersections trouvées */

        /* Parcourir chaque arête du polygone.
           j = indice du sommet suivant (avec bouclage). */
        for (i = 0; i < n; i++)
        {
            j  = (i + 1) % n;
            x1 = pts[i*2];   y1 = pts[i*2+1];
            x2 = pts[j*2];   y2 = pts[j*2+1];

            /* L'arête croise la scanline si y est strictement
               entre y1 et y2 (une extrémité incluse, l'autre
               exclue pour éviter de compter deux fois un
               sommet partagé par deux arêtes). */
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y))
            {
                /* Interpolation linéaire entière pour trouver
                   le X d'intersection :
                   x = x1 + (x2-x1) * (y-y1) / (y2-y1) */
                dy = y2 - y1;
                dx = x2 - x1;
                intersections[count++] = x1 + (dx * (y - y1)) / dy;
            }
        }

        /* Tri par insertion des X d'intersection.
           Le nombre d'intersections est toujours petit
           (en pratique 2 ou 4), donc le tri insertion
           est plus efficace qu'un tri rapide ici. */
        for (i = 1; i < count; i++)
        {
            tmp = intersections[i];
            j   = i - 1;
            while (j >= 0 && intersections[j] > tmp)
            {
                intersections[j+1] = intersections[j];
                j--;
            }
            intersections[j+1] = tmp;
        }

        /* Remplissage par paires d'intersections — règle pair-impair.
           En partant du bord gauche, on alterne dehors/dedans à chaque
           arête franchie :
             avant x[0]        : dehors
             de x[0] à x[1]   : dedans  → on remplit
             de x[1] à x[2]   : dehors
             de x[2] à x[3]   : dedans  → on remplit
             etc.
           On remplit donc les segments d'indices pairs (0-1, 2-3...). */
        for (i = 0; i + 1 < count; i += 2)
        {
            int xstart = intersections[i];
            int xend   = intersections[i+1];
            int len;

            /* Clipping horizontal. */
            if (xstart < 0)             xstart = 0;
            if (xend   >= SCREEN_WIDTH) xend   = SCREEN_WIDTH - 1;

            len = xend - xstart;
            if (len > 0)
                _fmemset(backbuffer + OFFSET(xstart, y), color, len);
        }
    }
}

/* =========================================================
   CERCLE — Algorithme de Bresenham (mid-point circle)
   =========================================================
   Le cercle possède 8 axes de symétrie. On calcule les
   points d'un seul octant (x de 0 à r/√2) et on en déduit
   les 7 autres par symétrie.

   Variable de décision d :
   d < 0 : on reste sur la même ligne  → d += 4x + 6
   d ≥ 0 : on descend d'une ligne      → d += 4(x-y) + 10, y--
   ========================================================= */

void drawCircle(int xc, int yc, int r, unsigned char color)
{
    int x = 0;          /* point de départ de l'octant     */
    int y = r;          /* on commence en haut du cercle   */
    int d = 3 - 2 * r;  /* valeur initiale de la décision  */

    while (x <= y)
    {
        /* Dessiner les 8 points symétriques avec clipping.
           Chaque paire de if vérifie que le point est
           dans les bornes de l'écran avant de dessiner. */

        if (xc+x>=0 && xc+x<SCREEN_WIDTH) {
            if (yc+y>=0 && yc+y<SCREEN_HEIGHT) putPixel(xc+x, yc+y, color);
            if (yc-y>=0 && yc-y<SCREEN_HEIGHT) putPixel(xc+x, yc-y, color);
        }
        if (xc-x>=0 && xc-x<SCREEN_WIDTH) {
            if (yc+y>=0 && yc+y<SCREEN_HEIGHT) putPixel(xc-x, yc+y, color);
            if (yc-y>=0 && yc-y<SCREEN_HEIGHT) putPixel(xc-x, yc-y, color);
        }
        if (xc+y>=0 && xc+y<SCREEN_WIDTH) {
            if (yc+x>=0 && yc+x<SCREEN_HEIGHT) putPixel(xc+y, yc+x, color);
            if (yc-x>=0 && yc-x<SCREEN_HEIGHT) putPixel(xc+y, yc-x, color);
        }
        if (xc-y>=0 && xc-y<SCREEN_WIDTH) {
            if (yc+x>=0 && yc+x<SCREEN_HEIGHT) putPixel(xc-y, yc+x, color);
            if (yc-x>=0 && yc-x<SCREEN_HEIGHT) putPixel(xc-y, yc-x, color);
        }

        /* Mise à jour de la variable de décision. */
        if (d < 0) d += 4 * x + 6;
        else      { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

/* =========================================================
   CERCLE PLEIN
   =========================================================
   Même algorithme que drawCircle, mais au lieu de 8 points
   isolés on trace des lignes horizontales entre les points
   symétriques, ce qui remplit le disque.
   xs = x de départ de la ligne, xl = longueur de la ligne.
   ========================================================= */

void drawCircleFill(int xc, int yc, int r, unsigned char color)
{
    int x = 0, y = r;
    int d = 3 - 2 * r;
    int xs, xl;   /* x start et x length de la ligne courante */

    while (x <= y)
    {
        /* Ligne horizontale centrée en (xc, yc-y)
           de xc-x à xc+x (largeur = 2x+1). */
        if (yc-y>=0 && yc-y<SCREEN_HEIGHT) {
            xs = xc - x; xl = 2*x + 1;
            if (xs < 0) { xl += xs; xs = 0; }            /* clip gauche */
            if (xs + xl > SCREEN_WIDTH) xl = SCREEN_WIDTH - xs; /* clip droite */
            if (xl > 0) _fmemset(backbuffer+OFFSET(xs, yc-y), color, xl);
        }

        /* Ligne horizontale centrée en (xc, yc+y). */
        if (yc+y>=0 && yc+y<SCREEN_HEIGHT) {
            xs = xc - x; xl = 2*x + 1;
            if (xs < 0) { xl += xs; xs = 0; }
            if (xs + xl > SCREEN_WIDTH) xl = SCREEN_WIDTH - xs;
            if (xl > 0) _fmemset(backbuffer+OFFSET(xs, yc+y), color, xl);
        }

        /* Ligne horizontale centrée en (xc, yc-x)
           de xc-y à xc+y (largeur = 2y+1). */
        if (yc-x>=0 && yc-x<SCREEN_HEIGHT) {
            xs = xc - y; xl = 2*y + 1;
            if (xs < 0) { xl += xs; xs = 0; }
            if (xs + xl > SCREEN_WIDTH) xl = SCREEN_WIDTH - xs;
            if (xl > 0) _fmemset(backbuffer+OFFSET(xs, yc-x), color, xl);
        }

        /* Ligne horizontale centrée en (xc, yc+x). */
        if (yc+x>=0 && yc+x<SCREEN_HEIGHT) {
            xs = xc - y; xl = 2*y + 1;
            if (xs < 0) { xl += xs; xs = 0; }
            if (xs + xl > SCREEN_WIDTH) xl = SCREEN_WIDTH - xs;
            if (xl > 0) _fmemset(backbuffer+OFFSET(xs, yc+x), color, xl);
        }

        /* Mise à jour de la variable de décision. */
        if (d < 0) d += 4 * x + 6;
        else      { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}
