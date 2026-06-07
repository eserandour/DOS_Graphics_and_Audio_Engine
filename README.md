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

La compilation se fait **sous FreeDOS** (ou un émulateur DOS tel que DOSBox) avec `wcc` et `wlink` disponibles dans le `PATH`.

---

## Compilation

```bat
BUILD.BAT
```

Ce script compile chaque fichier `.c` avec `wcc` (modèle mémoire **large**, cible **8086**, optimisation taille `-os`) puis lie tous les `.obj` via `wlink @LINK.RSP` pour produire `demo.exe`.

Pour nettoyer les fichiers intermédiaires :

```bat
CLEAN.BAT       :: supprime les .obj et .out
CLEANALL.BAT    :: supprime aussi demo.exe
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
| `scene1` | 6 s | Pixels aléatoires LCG — fade in/out |
| `scene2` | 14 s | Palette VGA : cycle gauche/droite, interpolation vers la palette rose |
| `scene3` | 36 s | Toutes les polices font1 (BIOS 8×8, Bank 8×8/8×16/16×16) |
| `scene4` | 3 s | « HELLO / WORLD » avec la police sprite font2 (32×32) |
| `scene5` | — | Scroller horizontal puis scroller vertical |
| `scene6` | 10 s | Rotozoom sur `freedos.raw` en virgule fixe 16.16 |
| `scene7` | 18 s | Toutes les primitives 2D (tunnel, spirale, plasma, rosace, rebonds) |

---

## Architecture

```
demo.exe
├── main.c            Point d'entrée — playlist de scènes
├── app.h             Flag global quitRequested
├── timer.h/c         Timer haute résolution (PIT 8253, 70 Hz, ISR INT 08h)
├── keyboard.h/c      ISR clavier INT 09h — détection Échap
├── video.h/c         Mode 13h, double buffer, flip(), waitVRetrace()
├── palette.h/c       Palette DAC VGA 256 couleurs, lerp, fade, cycle
├── graphics.h/c      Primitives 2D clippées (ligne, rect, cercle, polygone)
├── image.h/c         Chargement one-shot d'images RAW + PAL
├── sprite.h/c        Sprites préchargés en far heap, blit opaque/colorKey
├── font1.h/c         Polices bitmap multi-tailles (ROM BIOS + fichiers C)
│   └── font1/        Données glyphes : f1_8x8.c, f1_8x16.c, f1_16x16.c
├── font2.h/c         Texte par feuille de sprites .raw
│   └── font2/        Feuilles : font.raw (32×32), 16X16_F2.raw (16×16)
├── scene.h/c         Gestionnaire de scènes — callbacks onSceneEnd
└── scenes/           scene0.c … scene7.c
```

Pour le détail complet de chaque module, voir [`STRUCT.TXT`](STRUCT.TXT).

---

## Outils

Le dossier `OUTILS/` contient des utilitaires Python et un outil C autonome :

| Outil | Rôle |
|---|---|
| `vgatool.py` | Convertit une image en `.raw` + `.pal` + aperçu PNG |
| `DUMPPAL.C` | Lit la palette BIOS par défaut et écrit `DEFAULT.PAL` |
| `fonts/psf2c.py` | Convertit une police PSF1/PSF2 en fichier C de glyphes |
| `fonts/ttf2c.py` | Convertit une police TTF en fichier C de glyphes |
| `fonts/png2c.py` | Convertit un PNG de police bitmap en fichier C de glyphes |
| `fonts/fonteditor.htm` | Éditeur visuel de polices bitmap (HTML/JS, aucune dépendance) |

---

## Licence

Ce projet est distribué sous licence **GNU General Public License v3.0** (GPL-3.0).  
Vous êtes libre de l'utiliser, le modifier et le redistribuer, à condition que tout travail dérivé soit également publié sous GPL-3.0.

Voir le fichier [`LICENSE`](LICENSE) pour le texte complet.
