/* =========================================================
   SCENE.C — Gestionnaire de scenes
   ========================================================= */

#include "timer.h"
#include "scene.h"

void scene1(void);
void scene2(void);
void scene3(void);
void scene4(void);
void scene5(void);

Scene currentScene = SCENE_1;
unsigned long sceneStart = 0;

typedef void (*SceneFunc)(void);

static SceneFunc scenes[] = {
    scene1,  /* SCENE_1 = 0 */
    scene2,  /* SCENE_2 = 1 */
    scene3,  /* SCENE_3 = 2 */
    scene4,  /* SCENE_4 = 3 */
    scene5,  /* SCENE_5 = 4 */
};

void setScene(Scene s)
{
    currentScene = s;
    sceneStart   = readTimer();
}

void runCurrentScene(void)
{
    scenes[currentScene]();
}
