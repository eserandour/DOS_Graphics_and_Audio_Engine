#ifndef SCENE_H
#define SCENE_H

/* =========================================================
   SCENE.H — Gestionnaire de scenes
   ========================================================= */

typedef enum {
    SCENE_1 = 0,   /* pixels aleatoires (LCG)        */
    SCENE_2 = 1,   /* demonstration palette VGA       */
    SCENE_3 = 2,   /* demonstration des polices       */
    SCENE_4 = 3,   /* affichage image RAW+PAL         */
    SCENE_5 = 4    /* scrolling de texte horizontal   */
} Scene;

extern Scene currentScene;
extern unsigned long sceneStart;

void setScene(Scene s);
void runCurrentScene(void);

#endif /* SCENE_H */
