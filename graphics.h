#ifndef GRAPHICS_H
#define GRAPHICS_H

/* =========================================================
   GRAPHICS.H — Primitives de dessin 2D en mode 13h
   =========================================================
   Toutes les fonctions dessinent dans le backbuffer en RAM.
   Rien n'est visible à l'écran tant que flip() n'est pas
   appelé (voir video.h).

   Clipping : putPixel vérifie les bornes et ignore silencieusement
   les coordonnées hors écran. drawCircle et drawCircleFill ont
   aussi un clipping intégré. En revanche drawLine, drawRect,
   drawPolygon et drawPolygonFill n'ont pas de clipping : veiller
   à rester dans les bornes (0..319 en X, 0..199 en Y) lorsqu'on
   les appelle.
   ========================================================= */

/* ---------------------------------------------------------
   Effacement
   --------------------------------------------------------- */

/* Remplit tout le backbuffer avec une couleur uniforme.
   color : index de palette (0-255).
   Utilise _fmemset pour remplir les 64000 octets en une
   seule opération rapide. */
void clearScreen(unsigned char color);

/* ---------------------------------------------------------
   Pixel
   --------------------------------------------------------- */

/* Écrit un pixel dans le backbuffer à la position (x, y).
   Vérification de bornes intégrée : les coordonnées hors écran
   sont ignorées silencieusement (pas de crash, pas d'écriture
   hors du backbuffer).
   color : index de palette (0-255). */
void putPixel(int x, int y, unsigned char color);

/* Lit l'index de couleur d'un pixel dans le backbuffer. */
unsigned char getPixel(int x, int y);

/* ---------------------------------------------------------
   Lignes et rectangles
   --------------------------------------------------------- */

/* Trace un segment entre (x1,y1) et (x2,y2).
   Utilise l'algorithme de Bresenham qui ne travaille qu'en
   arithmétique entière (pas de division, pas de virgule
   flottante) : idéal pour le 8086. */
void drawLine(int x1, int y1, int x2, int y2, unsigned char color);

/* Trace le contour d'un rectangle (4 appels à drawLine).
   (x1,y1) = coin supérieur gauche
   (x2,y2) = coin inférieur droit */
void drawRect(int x1, int y1, int x2, int y2, unsigned char color);

/* Trace un rectangle plein.
   Remplit chaque ligne horizontale avec _fmemset, ce qui
   est bien plus rapide qu'appeler putPixel pour chaque pixel. */
void drawRectFill(int x1, int y1, int x2, int y2, unsigned char color);

/* ---------------------------------------------------------
   Polygones
   --------------------------------------------------------- */

/* Trace le contour d'un polygone à n sommets.
   pts : tableau de coordonnées [x0,y0, x1,y1, ..., xn,yn]
   n   : nombre de sommets (pas de paires).
   Le polygone est automatiquement fermé (dernier → premier). */
void drawPolygon(int *pts, int n, unsigned char color);

/* Trace un polygone plein par scanline filling.
   Pour chaque ligne horizontale (scanline), calcule les
   intersections avec les arêtes du polygone, les trie,
   puis remplit les segments entre paires d'intersections.
   Algorithme : even-odd rule (règle pair-impair).
   Clipping vertical et horizontal intégré. */
void drawPolygonFill(int *pts, int n, unsigned char color);

/* ---------------------------------------------------------
   Cercles
   --------------------------------------------------------- */

/* Trace le contour d'un cercle de centre (xc,yc) et de
   rayon r. Utilise l'algorithme de Bresenham pour cercle
   (mid-point circle) : entier pur, exploite les 8
   symétries du cercle pour ne calculer qu'1/8 de l'arc.
   Clipping intégré pour chaque octant. */
void drawCircle(int xc, int yc, int r, unsigned char color);

/* Trace un cercle plein.
   Même algorithme que drawCircle mais remplit des lignes
   horizontales entre les points symétriques avec _fmemset.
   Clipping intégré. */
void drawCircleFill(int xc, int yc, int r, unsigned char color);

#endif /* GRAPHICS_H */
