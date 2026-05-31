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

/* Callback appelé par sceneSignalEnd() quand une scène se déclare
   terminée. L'implémentation (ex: main.c) décide quelle scène vient
   ensuite et avec quelle transition.
   Signature : void handler(Scene sceneQuiVientDeFinir); */
typedef void (*SceneEndHandler)(Scene);
extern SceneEndHandler onSceneEnd;

void setScene(Scene s);
void runCurrentScene(void);

/* Appelé par une scène pour signaler qu'elle est terminée.
   La scène ne choisit PAS la suivante : c'est onSceneEnd qui décide. */
void sceneSignalEnd(void);

#endif /* SCENE_H */
