#ifndef FONT1DAT_H
#define FONT1DAT_H

/* =========================================================
   FONT1DAT.H — Déclarations internes des chargeurs de glyphes
   =========================================================
   Ces trois fonctions sont appelées exclusivement par
   font1.c (dans font1InitBank8x8/8x16/16x16) après _initFont1Bank().
   Elles ne font partie de l'API publique : ne pas les
   appeler directement depuis les scènes ou main.c.

   Pour ajouter une nouvelle police ou de nouveaux glyphes :
   modifier uniquement font1dat.c, sans toucher à font1.c.
   ========================================================= */

/* Charge les glyphes 8x8 dans font1Bank8x8.
   Appelle font1DefineChar8x8() pour chaque caractère défini. */
void _initFont1_8x8(void);

/* Charge les glyphes 8x16 dans font1Bank8x16.
   Appelle font1DefineChar8x16() pour chaque caractere defini. */
void _initFont1_8x16(void);

/* Charge les glyphes 16x16 dans font1Bank16x16.
   Appelle font1DefineChar16x16() pour chaque caractère défini. */
void _initFont1_16x16(void);

#endif /* FONT1DAT_H */
