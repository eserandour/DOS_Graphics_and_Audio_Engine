#!/usr/bin/env python3
# pal2img.py — affiche la palette d'un fichier .PAL (format VGA mode 13h, 6 bits)
# Usage : python3 pal2img.py palette.pal
# Usage : python3 pal2img.py palette.pal sortie.png   (nom de sortie optionnel)
#
# Nécessite d'avoir installé au préalable python3-pillow

import sys
from PIL import Image, ImageDraw, ImageFont

# ── Paramètres de rendu ──────────────────────────────────────────────────────
COLS        = 16          # colonnes de swatches
ROWS        = 16          # lignes  de swatches  (16×16 = 256 couleurs)
SWATCH_SIZE = 32          # taille d'une case en pixels
MARGIN      = 20          # marge extérieure
HEADER      = 40          # hauteur de la zone titre en haut
LABEL_H     = 16          # hauteur de la zone d'index en bas de chaque swatch
FONT_SIZE   = 11          # taille de police pour l'index (si disponible)
BG_COLOR    = (18, 18, 18)  # fond général (quasi-noir)
TEXT_COLOR  = (220, 220, 220)

# ── Lecture des arguments ────────────────────────────────────────────────────
if len(sys.argv) < 2:
    print("Usage: python3 pal2img.py <fichier.pal> [sortie.png]")
    sys.exit(1)

pal_path = sys.argv[1]
if len(sys.argv) >= 3:
    out_path = sys.argv[2]
else:
    out_path = pal_path.rsplit('.', 1)[0] + "_palette.png"

# ── Lecture + décodage 6 bits → 8 bits ──────────────────────────────────────
with open(pal_path, "rb") as f:
    raw = f.read()

if len(raw) < 768:
    print(f"Erreur : le fichier fait {len(raw)} octets, attendu 768 (256 × RGB 6 bits).")
    sys.exit(1)

colors = []
for i in range(256):
    r6, g6, b6 = raw[i*3], raw[i*3+1], raw[i*3+2]
    # Conversion 6 bits → 8 bits : décalage de 2 (équivalent ×4)
    colors.append((r6 << 2, g6 << 2, b6 << 2))

# ── Calcul des dimensions de l'image ────────────────────────────────────────
cell_w = SWATCH_SIZE
cell_h = SWATCH_SIZE + LABEL_H
total_w = COLS * cell_w + 2 * MARGIN
total_h = ROWS * cell_h + 2 * MARGIN + HEADER

img  = Image.new("RGB", (total_w, total_h), BG_COLOR)
draw = ImageDraw.Draw(img)

# ── Titre ────────────────────────────────────────────────────────────────────
import os
title = os.path.basename(pal_path)

try:
    font_title = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 16)
    font_label = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", FONT_SIZE)
except Exception:
    font_title = ImageFont.load_default()
    font_label = font_title

bbox = draw.textbbox((0, 0), title, font=font_title)
tx = (total_w - (bbox[2] - bbox[0])) // 2
draw.text((tx, (HEADER - (bbox[3] - bbox[1])) // 2), title, fill=TEXT_COLOR, font=font_title)

# ── Swatches ─────────────────────────────────────────────────────────────────
for idx, color in enumerate(colors):
    col = idx % COLS
    row = idx // COLS

    x0 = MARGIN + col * cell_w
    y0 = MARGIN + HEADER + row * cell_h

    # Carré de couleur
    draw.rectangle([x0, y0, x0 + cell_w - 1, y0 + SWATCH_SIZE - 1], fill=color)

    # Index en bas de chaque case
    label = f"{idx:3d}"
    lbbox = draw.textbbox((0, 0), label, font=font_label)
    lw = lbbox[2] - lbbox[0]
    lh = lbbox[3] - lbbox[1]
    lx = x0 + (cell_w - lw) // 2
    ly = y0 + SWATCH_SIZE + (LABEL_H - lh) // 2

    # Luminosité pour choisir la couleur du texte d'index
    r, g, b = color
    luminance = 0.299 * r + 0.587 * g + 0.114 * b
    label_color = (240, 240, 240) if luminance < 128 else (20, 20, 20)

    # Petit rectangle de fond pour la lisibilité de l'index
    draw.rectangle([x0, y0 + SWATCH_SIZE, x0 + cell_w - 1, y0 + cell_h - 1], fill=color)
    draw.text((lx, ly), label, fill=label_color, font=font_label)

# ── Sauvegarde ───────────────────────────────────────────────────────────────
img.save(out_path)
print(f"Palette : {len(colors)} couleurs")
print(f"Image sauvegardée : {out_path}  ({total_w}×{total_h} px)")
