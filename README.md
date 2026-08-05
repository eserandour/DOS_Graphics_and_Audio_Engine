# Mode 13h — Démo graphique pour FreeDOS

Une démo graphique en C pour **FreeDOS**, compilée avec **Open Watcom 1.9**.  
Elle exploite le **mode vidéo VGA 13h** (320×200, 256 couleurs) pour enchaîner huit scènes animées : pixels aléatoires, effets de palette, polices bitmap, scrollers, rotozoom et primitives 2D.

> **A graphics demo in C for FreeDOS / Open Watcom 1.9.**  
> Eight animated scenes in VGA mode 13h (320×200, 256 colours).

---

## Prérequis

| Outil | Version testée |
|---|---|
| [Open Watcom](https://github.com/open-watcom/open-watcom-v2) | 1.9 |
| [FreeDOS](https://www.freedos.org/) | 1.4 |
| Python (outils uniquement) | 3.x |
| Compilateur C natif (gcc/cc) | pour `OUTILS/*.py` |

La compilation de la démo se fait **sous FreeDOS** (ou un émulateur DOS tel que DOSBox/VirtualBox) avec `wcc` et `wlink` disponibles dans le `PATH`.

---

## Compilation

```bat
BUILD.BAT
```

Ce script compile chaque fichier `.c` avec `wcc` (modèle mémoire **large**, cible **8086**, optimisation taille `-os`) puis lie tous les `.obj` via `wlink @LINK.RSP` pour produire `demo.exe`.

Pour nettoyer les fichiers intermédiaires :

```bat
CLEAN.BAT       :: supprime les .obj, .out et .err
CLEANALL.BAT    :: supprime aussi demo.exe
KILLDEMO.BAT    :: remonte d'un niveau et supprime tout le dossier demo/
```

---

## Lancement

```bat
demo.exe
```

Appuyez sur **Échap** pour quitter proprement à tout moment.

La démo boucle par défaut (`DEMO_LOOP 1` dans `main.c`). Pour qu'elle s'arrête après la dernière scène, mettez cette constante à `0`.

---

## Contenu de la démo

| Scène | Durée | Description |
|---|---|---|
| `scene0` | 1 s | Écran noir (délimiteur de capture vidéo) |
| `scene1` | 6 s | Pixels aléatoires (LCG 32 bits) — fade in 1 s / fade out 3 s |
| `scene2` | 14 s | Palette VGA : cycle gauche/droite, puis interpolation vers la palette **rouge** |
| `scene3` | 36 s | Toutes les polices font1 (BIOS 8×8, Bank 8×8/8×16/16×16), 6 sous-écrans de 6 s |
| `scene4` | 3 s | « HELLO / WORLD » avec la police sprite font2 (32×32), centrées |
| `scene5` | ≈ 39 s | Scroller horizontal (crédits) puis scroller vertical (générique), durée dépendante du texte |
| `scene6` | 10 s | Rotozoom sur `freedos.raw` en virgule fixe 16.16, zoom oscillant ×0,6..×2,4 |
| `scene7` | 18 s | Toutes les primitives 2D en 6 phases (tunnel, spirale, plasma, rosace, rebonds, composition) |

La démo boucle : `scene0` (écran noir) apparaît une seconde fois à la fin de la playlist, avant de recommencer.

---

## Palettes

Le module `palette.c` fournit une palette dégradée (`grayPalette`) et trois palettes teintées noir→couleur (`redPalette`, `bluePalette`, `greenPalette`), en plus d'une palette arc-en-ciel HSV (`rainbowPalette`). Toutes sont générées procéduralement en mémoire ; les fichiers `.pal` correspondants dans `images/` (`red.pal`, `blue.pal`, `green.pal`, `gray.pal`, `rainbow.pal`) sont produits à part par `OUTILS/gen_palettes.py`, pour l'aperçu et le chargement via `loadPalette()`. `default.pal` (la palette BIOS d'origine) est générée séparément par `OUTILS/DUMPPAL.C`, puisqu'elle est lue directement sur le matériel et non calculée par une fonction du projet.

---

## Architecture

```
demo.exe
├── main.c            Point d'entrée — playlist de scènes
├── app.h             Flag global quitRequested
├── timer.h/c         Timer haute résolution (PIT 8253, 70 Hz, ISR INT 08h)
├── keyboard.h/c      ISR clavier INT 09h — détection Échap
├── video.h/c         Mode 13h, double buffer, flip(), waitVRetrace()
├── palette.h/c       Palette DAC VGA 256 couleurs, lerp, fade, cycle, générateurs
├── graphics.h/c      Primitives 2D clippées (ligne, rect, cercle, polygone)
├── image.h/c         Chargement one-shot d'images RAW + PAL
├── sprite.h/c        Sprites préchargés en far heap, blit opaque/colorKey
│                     (compilé et lié, mais non utilisé par les scènes actuelles)
├── font1.h/c         Polices bitmap multi-tailles (ROM BIOS + fichiers C)
│   └── font1/        Données glyphes : f1_8x8.c, f1_8x16.c, f1_16x16.c (police WY-700a)
├── font2.h/c         Texte par feuille de sprites .raw
│   └── font2/        Feuilles : font.raw (32×32), 16X16_F2.raw (16×16)
├── scene.h/c         Gestionnaire de scènes — callbacks onSceneEnd
└── scenes/           scene0.c … scene7.c
```

Pour le détail complet de chaque module (jusqu'au fichier près), voir [`STRUCT.TXT`](STRUCT.TXT).

---

## Outils

Le dossier `OUTILS/` contient des utilitaires Python et un outil C autonome :

| Outil | Rôle |
|---|---|
| `vgatool.py` | Convertit une image en `.raw` + `.pal` + aperçu PNG, ou prévisualise un `.pal` existant |
| `DUMPPAL.C` | Lit la palette BIOS par défaut sur le DAC VGA et écrit `DEFAULT.PAL` |
| `gen_palettes.py` | Découvre automatiquement les générateurs `build<Nom>Palette()` de `palette.c`, les exécute via un mini-programme C natif compilé à la volée, et produit chaque `images/<nom>.pal` + aperçu PNG |
| `fonts/psf2c.py` | Convertit une police PSF1/PSF2 en fichier C de glyphes |
| `fonts/ttf2c.py` | Convertit une police TTF en fichier C de glyphes |
| `fonts/png2c.py` | Convertit un PNG de police bitmap en fichier C de glyphes |
| `fonts/fonteditor.htm` | Éditeur visuel de polices bitmap (HTML/JS, aucune dépendance) |

`DUMPPAL.C` se compile avec Open Watcom, comme le reste du projet ; `gen_palettes.py` et les scripts de `fonts/` s'exécutent avec Python 3 sur la machine de développement (pas sous FreeDOS).

---

## Licence

Ce projet est distribué sous licence **GNU General Public License v3.0** (GPL-3.0).  
Vous êtes libre de l'utiliser, le modifier et le redistribuer, à condition que tout travail dérivé soit également publié sous GPL-3.0.

Voir le fichier [`LICENSE`](LICENSE) pour le texte complet.
