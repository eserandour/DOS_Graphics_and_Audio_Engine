#ifndef FONT2_H
#define FONT2_H

/* =========================================================
   FONT2.H — Affichage de texte par feuille de sprites
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode video    : 13h (320x200, 256 couleurs, 1 octet/pixel)

   PRINCIPE
   --------
   Les glyphes sont stockes dans une feuille de sprites .raw
   (images\font.raw), organisee en grille reguliere :

     Feuille : 320 x 192 pixels
     Grille  : 6 lignes x 10 colonnes
     Glyphe  : 32 x 32 pixels
     Plage   : 0x20 (espace) a 0x5B ([)

   DEUX MODES D'UTILISATION
   ------------------------
   1. Mode disque (statique) — font2DiskDrawText / font2DiskDrawTextCentered
      Lit font.raw sur le disque a chaque appel.
      Pratique pour du texte fixe affiche une seule fois
      (ecran titre, score...). Pas adapte a l'animation.

   2. Mode RAM (anime) — font2RamLoad / font2RamDrawText / font2RamFree
      Charge font.raw en RAM far une seule fois via font2RamLoad().
      Tous les blits suivants lisent depuis la RAM : aucune
      acces disque pendant le rendu, adapte au scroll et a
      tout affichage repete chaque frame.
      A liberer avec font2RamFree() en quittant la scene.

   TRANSPARENCE
   ------------
   colorKey <  0 : copie opaque (fond du glyphe ecrase le BB).
   colorKey >= 0 : pixels d'index colorKey non ecrits.
   FONT2_BG (127) est l'index de fond de la feuille fournie.

   UTILISATION — MODE DISQUE
   -------------------------
     loadImagePal("images\\font.pal");
     clearScreen(0);
     font2DiskDrawText("HELLO", 80, 84, FONT2_BG);
     font2DiskDrawTextCentered("WORLD", 120, FONT2_BG);
     flip();

   UTILISATION — MODE RAM
   ----------------------
     loadImagePal("images\\font.pal");
     if (!font2RamLoad()) { ... erreur ... }
     ...
     clearScreen(0);
     font2RamDrawText("SCORE 1000", 10, 84, FONT2_BG);
     font2RamDrawChar('A', 160, 84, FONT2_BG);
     flip();
     ...
     font2RamFree();
   ========================================================= */


/* ---------------------------------------------------------
   Dimensions de la feuille et des glyphes
   --------------------------------------------------------- */

#define FONT2_SHEET_W    320   /* largeur totale du .raw (px) */
#define FONT2_SHEET_H    192   /* hauteur totale du .raw (px) */
#define FONT2_CHAR_W      32   /* largeur d'un glyphe (px)    */
#define FONT2_CHAR_H      32   /* hauteur d'un glyphe (px)    */
#define FONT2_COLS        10   /* colonnes dans la grille     */

/* Taille totale de la feuille en octets. */
/* ATTENTION : 320 * 192 = 61440 depasse int 16 bits (max 32767).
   Le suffixe UL force le calcul en unsigned long. */
#define FONT2_SHEET_SIZE (320UL * 192UL)   /* 61440 octets */

/* ---------------------------------------------------------
   Plage de caracteres supportes
   --------------------------------------------------------- */

#define FONT2_FIRST_CHAR 0x20  /* espace — premier glyphe     */
#define FONT2_LAST_CHAR  0x5B  /* [     — dernier glyphe      */

/* ---------------------------------------------------------
   Index de fond dans la palette font.pal
   --------------------------------------------------------- */

#define FONT2_BG         127   /* colorKey pour la transparence */


/* =========================================================
   MODE DISQUE
   =========================================================
   Lit font.raw depuis le disque a chaque appel.
   Utiliser uniquement pour du texte statique (une seule
   ecriture par frame au maximum).
   ========================================================= */

/* Dessine le caractère c à (x, y) en lisant font.raw sur le disque.
   Caractère hors plage ([FONT2_FIRST_CHAR..FONT2_LAST_CHAR]) :
   rien n'est dessiné, le curseur n'avance pas. */
void font2DiskDrawChar(unsigned char c, int x, int y, int colorKey);

/* Dessine text à partir de (x, y) dans le backbuffer.
   Les caractères hors plage sont ignorés (curseur avance quand même). */

/* Dessine text centre horizontalement sur SCREEN_WIDTH.
   Si la chaine depasse 320 px, callee a gauche (x=0). */
void font2DiskDrawTextCentered(const char *text, int y, int colorKey);


/* =========================================================
   MODE RAM
   =========================================================
   Charger une fois avec font2RamLoad(), utiliser librement,
   liberer avec font2RamFree() en fin de scene.
   ========================================================= */

/* Charge font.raw en RAM far (_fmalloc).
   Retourne 1 si succes, 0 si echec (memoire ou fichier). */
int font2RamLoad(void);

/* Libere la RAM allouee par font2RamLoad().
   Met le pointeur interne a NULL. Appel sans effet si
   font2RamLoad() n'a pas ete appele. */
void font2RamFree(void);

/* Retourne 1 si la feuille est chargee en RAM, 0 sinon.
   Permet a une scene de verifier l'etat avant de dessiner. */
int font2RamIsLoaded(void);

/* Dessine le caractere c a (x, y) depuis la RAM.
   Precondition : font2RamLoad() a retourne 1.
   Caractere hors plage : rien n'est dessine. */
void font2RamDrawChar(unsigned char c, int x, int y, int colorKey);

/* Dessine text a partir de (x, y) depuis la RAM.
   Equivalent RAM de font2DiskDrawText. */
void font2RamDrawText(const char *text, int x, int y, int colorKey);

/* Dessine text centre horizontalement depuis la RAM.
   Equivalent RAM de font2DiskDrawTextCentered. */
void font2RamDrawTextCentered(const char *text, int y, int colorKey);

/* Retourne l'octet a (sx, sy) dans la feuille RAM.
   Precondition : font2RamIsLoaded() == 1.
   Usage : acces colonne par colonne pour le scroller.
   Pas de verification de bornes pour la performance. */
unsigned char font2RamGetPixel(int sx, int sy);

#endif /* FONT2_H */
