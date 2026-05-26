#ifndef PALETTE_H
#define PALETTE_H

/* =========================================================
   PALETTE.H — Gestion de la palette VGA 256 couleurs
   =========================================================
   En mode 13h, chaque pixel est un index (0-255) dans une
   table de 256 couleurs appelée palette. La palette est
   stockée dans le DAC (Digital-to-Analog Converter) VGA.

   Chaque couleur du DAC est définie par trois composantes
   R, G, B sur 6 bits (valeurs 0 à 63, et non 0 à 255
   comme en RGB moderne). Pour convertir : val_6bits * 4.

   Changer la palette ne nécessite pas de redessiner l'écran
   ce qui permet des effets (fade, cycle) très efficaces.
   ========================================================= */

/* ---------------------------------------------------------
   Structure Color
   --------------------------------------------------------- */

/* Représente une couleur VGA.
   Valeurs sur 6 bits : 0 (noir) à 63 (intensité maximale).
   Ne pas dépasser 63 : le DAC ignore les bits supérieurs. */
typedef struct {
    unsigned char r;   /* composante rouge  (0-63) */
    unsigned char g;   /* composante verte  (0-63) */
    unsigned char b;   /* composante bleue  (0-63) */
} Color;

/* ---------------------------------------------------------
   Palettes globales
   --------------------------------------------------------- */

/* Palette lue au démarrage : état initial du DAC VGA.
   Sert de référence pour restaurer les couleurs d'origine. */
extern Color defaultPalette[256];

/* Palette de travail : celle qu'on manipule (fade, cycle).
   C'est cette palette qu'on envoie au DAC avec setPalette. */
extern Color workingPalette[256];

/* Palettes temporaires pour les interpolations (lerp).
   paletteA = palette de départ, paletteB = palette d'arrivée. */
extern Color paletteA[256];
extern Color paletteB[256];

/* Palette dégradé noir → blanc (niveaux de gris). */
extern Color grayPalette[256];

/* Palette rose : index 0 = noir, index 1-255 = rouge max
   avec vert et bleu croissants (rouge → blanc). */
extern Color pinkPalette[256];

/* ---------------------------------------------------------
   Fonctions — Accès matériel DAC
   --------------------------------------------------------- */

/* Définit une seule couleur dans le DAC VGA.
   index : numéro de la couleur (0-255)
   r, g, b : composantes sur 6 bits (0-63). */
void setPaletteColor(unsigned char index,
                     unsigned char r, unsigned char g, unsigned char b);

/* Envoie les 256 couleurs d'une palette vers le DAC VGA.
   Attend le retrace vertical avant d'écrire pour éviter
   le palette tearing (bandes de couleurs parasites). */
void setPalette(Color *pal);

/* Lit les 256 couleurs actuelles du DAC dans un tableau.
   Utile pour sauvegarder la palette par défaut au démarrage. */
void getPalette(Color *pal);

/* ---------------------------------------------------------
   Fonctions — Manipulation de palette
   --------------------------------------------------------- */

/* Copie src vers dest (256 * sizeof(Color) octets). */
void copyPalette(Color *dest, Color *src);

/* Interpolation linéaire entre palA et palB.
   t = 0.0 → résultat = palA
   t = 1.0 → résultat = palB
   t = 0.5 → mélange à 50%
   Formule : dest[i] = palA[i] + t * (palB[i] - palA[i]) */
void lerpPalette(Color *dest, Color *palA, Color *palB, float t);

/* Applique un facteur de luminosité à une palette et
   l'envoie directement au DAC (sans modifier pal en RAM).
   t = 0.0 → tout noir (fade out complet)
   t = 1.0 → palette originale (fade in complet)
   Utile pour les effets de fondu enchaîné. */
void fadePalette(Color *pal, float t);

/* Décale toutes les couleurs d'un cran vers la gauche dans
   l'intervalle [start, end]. La couleur start est perdue,
   la couleur end reçoit l'ancienne valeur de start.
   Crée un effet de défilement des couleurs vers la gauche. */
void cyclePaletteLeft(Color *pal, int start, int end);

/* Décale toutes les couleurs d'un cran vers la droite dans
   l'intervalle [start, end]. La couleur end est sauvegardée,
   chaque entrée reçoit la valeur de son voisin de gauche, puis
   la sauvegarde est replacée en start.
   Crée un effet de défilement des couleurs vers la droite. */
void cyclePaletteRight(Color *pal, int start, int end);

/* ---------------------------------------------------------
   Fonctions — Générateurs de palette
   --------------------------------------------------------- */

/* Remplit toute la palette de noir (r=g=b=0). */
void generateBlackPalette(Color *pal);

/* Génère un dégradé linéaire du noir au blanc.
   index 0 → r=g=b=0 (noir)
   index 255 → r=g=b=63 (blanc)
   Conversion : val_6bits = index / 4 (décalage de 2 bits). */
void generateGrayPalette(Color *pal);

/* Génère une palette rose/rouge :
   index 0   → noir (r=g=b=0)
   index 1-255 → rouge fixe à 63, vert et bleu croissants
   de 0 à 63. L'effet va du rouge pur vers le blanc rosé. */
void generatePinkPalette(Color *pal);

#endif /* PALETTE_H */
