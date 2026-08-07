# Moteur graphique et audio pour DEMO (Open Watcom 1.9 sous FreeDOS 1.4)

*Dernière version : 07/08/2026 à 15h15*

Un **moteur graphique et audio** en C89 pur pour **FreeDOS**, compilé avec
**Open Watcom 1.9**, sans aucune dépendance externe au runtime : mode vidéo
**VGA 13h** (320×200, 256 couleurs) piloté directement via les ports,
timer et clavier gérés par interruptions matérielles, musique **S3M** et
effets **WAV** joués sur Sound Blaster par DMA.

Le dépôt inclut neuf **scènes de démonstration** (`scenes/scene0.c` à
`scene8.c`) qui illustrent chacun des modules du moteur : pixels bruts,
palette VGA, polices bitmap, texte par feuille de sprites, scrollers,
rotozoom, primitives 2D, et lecture audio S3M.

> **A graphics and audio engine in C for FreeDOS / Open Watcom 1.9.**
> VGA mode 13h (320×200, 256 colours) and Sound Blaster S3M/WAV playback,
> with nine demo scenes showcasing each module of the engine.

---

## Prérequis

| Outil | Version testée |
|---|---|
| [Open Watcom](https://github.com/open-watcom/open-watcom-v2) | 1.9 |
| [FreeDOS](https://www.freedos.org/) | 1.4 |
| Python (outils uniquement) | 3.x |
| Carte **Sound Blaster** (ou compatible) | facultative — voir [Moteur audio](#moteur-audio) |

La compilation se fait **sous FreeDOS** (ou un émulateur DOS tel que
DOSBox/VirtualBox) avec `wcc` et `wlink` disponibles dans le `PATH`.

---

## Compilation

```bat
BUILD.BAT
```

Ce script compile chaque fichier `.c` avec `wcc` (modèle mémoire **large**,
cible **8086**, optimisation taille `-os`) puis lie tous les `.obj` via
`wlink @LINK.RSP` pour produire `demo.exe`.

Pour nettoyer les fichiers intermédiaires :

```bat
CLEAN.BAT       :: supprime les .obj, .out et .err
CLEANALL.BAT    :: supprime aussi les .exe
KILLDEMO.BAT    :: remonte d'un niveau et supprime tout le dossier demo/
```

---

## Lancement

```bat
demo.exe
```

Appuyez sur **Échap** pour quitter proprement à tout moment (coupe le DMA
audio avant toute autre restauration, pour ne jamais laisser de bruit
résiduel).

`demo.exe` enchaîne les neuf scènes de démonstration ci-dessous et boucle
par défaut (`DEMO_LOOP 1` dans `main.c`) ; mettez cette constante à `0`
pour qu'elle s'arrête après la dernière scène.

---

## Le moteur

Chaque module est indépendant et réutilisable en dehors du contexte des
scènes de démonstration :

| Module | Rôle |
|---|---|
| `video.h/c` | Mode 13h, double buffer en RAM far, `flip()`, synchronisation `waitVRetrace()` |
| `palette.h/c` | Palette DAC VGA 256 couleurs (6 bits/canal) : lecture/écriture, interpolation, cyclage, générateurs procéduraux |
| `graphics.h/c` | Primitives 2D avec clipping intégré : pixel, ligne (Bresenham + Cohen-Sutherland), rectangle, polygone (scanline), cercle (mid-point) |
| `image.h/c` | Chargement one-shot d'images `.raw`/`.pal` (fond d'écran, écran titre) |
| `sprite.h/c` | Sprites préchargés en far heap, blit opaque ou avec transparence (`colorKey`), sans accès disque par frame |
| `font1.h/c` | Moteur de texte bitmap multi-tailles : police ROM BIOS ou banques personnelles (8×8, 8×16, 16×16) |
| `font2.h/c` | Texte par feuille de sprites `.raw`, décrite par un descripteur (`Font2Desc`) |
| `timer.h/c` | Timer haute résolution 70 Hz (PIT 8253 reprogrammé, ISR chaînée au BIOS) |
| `keyboard.h/c` | Détection Échap par ISR INT 09h, sans consommer le buffer clavier |
| `sblaster.h/c` | Pilote bas niveau Sound Blaster (DSP + DMA), détection via `BLASTER` |
| `s3m.h/c` | Lecteur de modules **S3M** (Scream Tracker 3), jusqu'à 16 voies mixées en logiciel |
| `wav.h/c` | Mixeur d'effets sonores **WAV**, jusqu'à 4 voies simultanées |
| `audio.h/c` | API audio de haut niveau (`playMusic`, `playSound`, fondus) assemblant `sblaster`/`s3m`/`wav` |
| `scene.h/c` | Gestionnaire de scènes (playlist, callback de fin de scène) utilisé par l'application de démonstration |

---

## Scènes de démonstration

| Scène | Durée | Module illustré |
|---|---|---|
| `scene0` | 1 s | Écran noir (délimiteur de capture vidéo) |
| `scene8` | 32 s | **Moteur audio** : cycle de vie complet d'un module S3M (fade in 6 s → lecture 16 s → fade out 6 s → arrêt 4 s) sur `musique.s3m`, avec affichage en direct de la phase et du statut |
| `scene1` | 6 s | **video/palette** : pixels aléatoires (LCG 32 bits) — fade in 1 s / fade out 3 s |
| `scene2` | 14 s | **palette** : cycle gauche/droite, puis interpolation vers la palette rouge |
| `scene3` | 36 s | **font1** : les quatre polices (BIOS 8×8, Bank 8×8/8×16/16×16), 6 sous-écrans de 6 s |
| `scene4` | 3 s | **font2** : « HELLO » / « WORLD » avec la police sprite 32×32, centrées horizontalement |
| `scene5` | ≈ 39 s | **font2** : scroller horizontal (crédits) puis scroller vertical (générique) ; durée dépendante de la longueur des textes |
| `scene6` | 10 s | **graphics/image** : rotozoom sur `images/freedos.raw` en virgule fixe 16.16, zoom oscillant ×0,6..×2,4 |
| `scene7` | 18 s | **graphics** : toutes les primitives 2D en 6 phases de 3 s (tunnel, spirale, plasma, flocon de Koch, rebonds, composition) |
| `scene0` | 1 s | Écran noir (délimiteur de capture vidéo, seconde occurrence en fin de playlist) |

Durée totale d'un cycle de la playlist par défaut : **≈ 2 min 40 s**, avant
bouclage.

---

## Moteur audio

La détection de carte se fait via la variable d'environnement `BLASTER` ;
en son absence, `audioInit()` échoue proprement et `playMusic()`/
`playSound()` deviennent de simples no-op — le reste du moteur (vidéo,
timer, clavier) continue de fonctionner normalement, sans plantage.

---

## Palettes

Le module `palette.c` fournit une palette dégradée (`grayPalette`) et trois
palettes teintées noir→couleur (`redPalette`, `bluePalette`,
`greenPalette`), en plus d'une palette arc-en-ciel HSV (`rainbowPalette`).
Toutes sont générées procéduralement en mémoire ; les fichiers `.pal`
correspondants dans `images/` (`red.pal`, `blue.pal`, `green.pal`,
`gray.pal`, `rainbow.pal`) sont produits à part par `OUTILS/gen_palettes.py`,
pour l'aperçu et le chargement via `loadPalette()`. `default.pal` (la
palette BIOS d'origine) est générée séparément par `OUTILS/DUMPPAL.C`,
puisqu'elle est lue directement sur le matériel et non calculée par une
fonction du projet. `freedos.pal` (utilisée par `scene6`) est quant à elle
issue d'une conversion d'image via `OUTILS/vgatool.py`, pas d'un
générateur procédural.

---

## Architecture

```
demo.exe
├── main.c            Point d'entrée — playlist de scènes de démonstration
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
│   └── font1/        Données glyphes : f1_8x8.c, f1_8x16.c (CP850),
│                     f1_16x16.c (police WY-700a, CP437)
├── font2.h/c         Texte par feuille de sprites .raw
│   └── font2/        Feuilles : font.raw (32×32), 16X16_F2.raw (16×16)
├── sblaster.h/c      Pilote bas niveau Sound Blaster (DSP + DMA)
├── s3m.h/c           Lecteur de modules S3M (Scream Tracker 3)
├── wav.h/c           Mixeur d'effets sonores WAV
├── audio.h/c         API audio de haut niveau (playMusic, playSound, fondus)
│   └── audios/       musique.s3m — module joué par scene8
├── images/           freedos.raw/.pal (scene6) + palettes .pal ci-dessus
├── scene.h/c         Gestionnaire de scènes — callbacks onSceneEnd
└── scenes/           scene0.c … scene8.c — application de démonstration
```

---

## Outils

Le dossier `OUTILS/` contient des utilitaires Python et un outil C
autonome, non nécessaires pour compiler ou exécuter la démo :

| Outil | Rôle |
|---|---|
| `vgatool.py` | Convertit une image en `.raw` + `.pal` + aperçu PNG, ou prévisualise un `.pal` existant |
| `DUMPPAL.C` | Lit la palette BIOS par défaut sur le DAC VGA et écrit `DEFAULT.PAL` |
| `gen_palettes.py` | Découvre automatiquement les générateurs `build<Nom>Palette()` de `palette.c`, les exécute via un mini-programme C natif compilé à la volée, et produit chaque `images/<nom>.pal` + aperçu PNG |
| `s3mcheck.py` | Vérifie qu'un module `.s3m` tient dans le budget mémoire far du moteur audio, en reproduisant les calculs de `s3mLoad()` |
| `fonts/psf2c.py` | Convertit une police PSF1/PSF2 en fichier C de glyphes |
| `fonts/ttf2c.py` | Convertit une police TTF en fichier C de glyphes |
| `fonts/png2c.py` | Convertit un PNG de police bitmap en fichier C de glyphes |
| `fonts/fonteditor.htm` | Éditeur visuel de polices bitmap (HTML/JS, aucune dépendance) |

`DUMPPAL.C` se compile avec Open Watcom, comme le reste du projet ;
`gen_palettes.py`, `s3mcheck.py` et les scripts de `fonts/` s'exécutent
avec Python 3 sur la machine de développement (pas sous FreeDOS).

---

## Licence

Ce projet est distribué sous licence **GNU General Public License v3.0**
(GPL-3.0).
Vous êtes libre de l'utiliser, le modifier et le redistribuer, à condition
que tout travail dérivé soit également publié sous GPL-3.0.

Voir le fichier [`LICENSE`](LICENSE) pour le texte complet.
