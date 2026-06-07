#ifndef SPRITE_H
#define SPRITE_H

/* =========================================================
   SPRITE.H — Sprites préchargés en far heap, mode 13h
   =========================================================
   Environnement : Open Watcom 1.9, FreeDOS 1.4
   Mode vidéo    : 13h (320x200, 256 couleurs, 1 octet/pixel)

   PRINCIPE
   --------
   image.c charge les pixels depuis le disque à chaque blit.
   Sur FreeDOS avec un accès disque lent (disquette, FAT12),
   un fread() par frame crée un goulot d'étranglement visible.

   Ce module charge le .raw UNE SEULE FOIS en far heap au
   démarrage (spriteLoad), puis blit depuis la RAM à chaque
   frame (spriteBlit / spriteBlitKey). Le disque n'est plus
   touché pendant le rendu.

   CONTRAINTE _fmalloc 16 BITS
   ----------------------------
   _fmalloc ne peut pas allouer plus de 65 535 octets en un
   seul bloc (limite du segment 16 bits). Un sprite de
   320×200 = 64 000 octets tient juste. Au-delà (ex. feuille
   de 320×256 = 81 920 o), utiliser spriteLoadSplit qui
   découpe en deux blocs de 32 768 octets, exactement comme
   scene6.c le fait manuellement pour tex0/tex1.

   Les sprites dont w*h <= 65 535 utilisent spriteLoad.
   Au-delà, spriteLoadSplit (voir section dédiée plus bas).

   TRANSPARENCE (colorKey)
   -----------------------
   Même convention qu'image.c :
     colorKey <  0  : blit opaque, tous les pixels copiés.
     colorKey >= 0  : les pixels d'index == colorKey ne sont
                      pas écrits (fond laissé intact).

   FEUILLE DE SPRITES (sprite sheet)
   -----------------------------------
   Un Sprite peut représenter une feuille entière (ex. la
   police font2). spriteBlitZone / spriteBlitZoneKey
   extraient un rectangle (srcX, srcY, zoneW, zoneH) depuis
   la feuille et le copient dans le backbuffer, sans aucun
   accès disque.

   CODES DE RETOUR
   ---------------
   SPR_OK           0   succès
   SPR_ERR_MEM      1   _fmalloc a retourné NULL
   SPR_ERR_FILE     2   fopen a échoué
   SPR_ERR_READ     3   fread incomplet (fichier trop court)
   SPR_ERR_SIZE     4   w*h > 65535 (utiliser spriteLoadSplit)

   FLUX TYPIQUES
   -------------
   Sprite simple, opaque :
       Sprite spr;
       if (spriteLoad(&spr, "bullet.raw", 8, 8) != SPR_OK) ...
       ...
       spriteBlit(&spr, x, y);          // chaque frame
       ...
       spriteFree(&spr);                // en fin de scène

   Sprite avec transparence (index 0 = fond) :
       spriteBlitKey(&spr, x, y, 0);

   Feuille de sprites, glyphe à (col*16, row*16) taille 16x16 :
       spriteBlitZoneKey(&sheet, col*16, row*16, 16, 16,
                         dstX, dstY, colorKey);

   Texture > 64 Ko (feuille 320x256 = 81920 o) :
       SpriteSplit big;
       if (spriteLoadSplit(&big, "bg.raw", 320, 256) != SPR_OK) ...
       spriteBlitSplit(&big, dstX, dstY);
       spriteFreeSplit(&big);
   ========================================================= */


/* =========================================================
   CODES DE RETOUR
   ========================================================= */

#define SPR_OK        0   /* succès                           */
#define SPR_ERR_MEM   1   /* _fmalloc a retourné NULL         */
#define SPR_ERR_FILE  2   /* fopen a échoué                   */
#define SPR_ERR_READ  3   /* fread incomplet                  */
#define SPR_ERR_SIZE  4   /* w*h > 65535, utiliser SplitSprite*/


/* =========================================================
   STRUCTURE Sprite — w*h <= 65 535 octets
   ========================================================= */

typedef struct {
    unsigned char far *data;  /* pixels en far heap            */
    int w;                    /* largeur en pixels             */
    int h;                    /* hauteur en pixels             */
} Sprite;


/* =========================================================
   STRUCTURE SpriteSplit — w*h > 65 535 octets
   =========================================================
   Deux blocs far de 32 768 octets chacun.
   blk[0] contient les octets 0..32767,
   blk[1] contient les octets 32768..w*h-1.
   Exige w*h <= 65 536 (deux blocs de 32 768 max).
   ========================================================= */

typedef struct {
    unsigned char far *blk[2]; /* deux demi-blocs far          */
    int w;                     /* largeur en pixels            */
    int h;                     /* hauteur en pixels            */
} SpriteSplit;


/* =========================================================
   SPRITE SIMPLE  (w*h <= 65 535)
   ========================================================= */

/* Charge le fichier .raw (largeur w, hauteur h) en far heap.
   Remplit spr->data, spr->w, spr->h.
   Retourne SPR_OK ou un code SPR_ERR_*. */
int spriteLoad(Sprite *spr, const char *rawFile, int w, int h);

/* Libère le far heap alloué par spriteLoad.
   Met spr->data à NULL. Sans effet si déjà NULL. */
void spriteFree(Sprite *spr);

/* Blit opaque : copie le sprite entier dans le backbuffer
   avec le coin supérieur gauche en (dstX, dstY).
   Clipping intégré sur les quatre bords. */
void spriteBlit(const Sprite *spr, int dstX, int dstY);

/* Blit avec colorKey : identique à spriteBlit mais les
   pixels dont l'index == colorKey ne sont pas écrits.
   colorKey < 0 → blit opaque (même comportement que spriteBlit). */
void spriteBlitKey(const Sprite *spr, int dstX, int dstY,
                   int colorKey);

/* Blit d'une zone (rectangle) de la feuille de sprites.
   srcX, srcY   : coin supérieur gauche dans la feuille.
   zoneW, zoneH : dimensions de la zone à extraire.
   dstX, dstY   : position dans le backbuffer.
   Opaque (tous les pixels copiés). */
void spriteBlitZone(const Sprite *spr,
                    int srcX, int srcY, int zoneW, int zoneH,
                    int dstX, int dstY);

/* Idem spriteBlitZone avec colorKey. */
void spriteBlitZoneKey(const Sprite *spr,
                       int srcX, int srcY, int zoneW, int zoneH,
                       int dstX, int dstY,
                       int colorKey);


/* =========================================================
   SPRITE SPLIT  (w*h > 65 535, max 2 × 32 768)
   ========================================================= */

/* Charge le .raw en deux blocs far de 32 768 octets.
   Retourne SPR_OK ou un code SPR_ERR_*. */
int spriteLoadSplit(SpriteSplit *spr, const char *rawFile,
                    int w, int h);

/* Libère les deux blocs far. Met les pointeurs à NULL. */
void spriteFreeSplit(SpriteSplit *spr);

/* Blit opaque d'un SpriteSplit dans le backbuffer. */
void spriteBlitSplit(const SpriteSplit *spr, int dstX, int dstY);

/* Blit avec colorKey d'un SpriteSplit. */
void spriteBlitSplitKey(const SpriteSplit *spr, int dstX, int dstY,
                        int colorKey);


#endif /* SPRITE_H */
