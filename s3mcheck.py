#!/usr/bin/env python3
# s3mcheck.py — Vérifie qu'un module .s3m tient dans le budget mémoire
#               far du moteur audio (s3m.c) AVANT de le tester sous
#               DOSBox/FreeDOS.
#
# Reproduit exactement les calculs de s3mLoad()/loadSampleData() :
# chaque échantillon PCM est tronqué à 65535 octets (limite d'un seul
# bloc _fmalloc en mode large), puis converti en 8 bits mono — donc
# la taille en mémoire = min(longueur source, 65535) par instrument,
# plus les motifs packés (rarement significatifs par comparaison).
#
# USAGE
# -----
#   python3 s3mcheck.py <fichier.s3m> [--budget Ko]
#
# EXEMPLES
#   python3 s3mcheck.py ../audios/musique.s3m
#   python3 s3mcheck.py ../audios/musique.s3m --budget 350
#
# Le budget par défaut (350 Ko) est une estimation prudente de la
# mémoire conventionnelle DOS restant disponible pour les échantillons
# une fois DOS, la vidéo, le code/données du programme et le buffer
# DMA audio soustraits d'un total de 640 Ko — à ajuster selon le
# profil mémoire réel de la démo (voir CLEAN.BAT / rapport de link).

import sys
import struct

S3M_MAX_INSTRUMENTS = 100
S3M_MAX_PATTERNS    = 64
SAMPLE_BLOCK_CAP     = 65535   # taille max d'un seul _fmalloc (large model)
DEFAULT_BUDGET_KB    = 350


def u16(data, off):
    return struct.unpack_from("<H", data, off)[0]


def u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def check(path, budget_kb):
    with open(path, "rb") as f:
        data = f.read()

    if len(data) < 0x60 or data[0x2C:0x30] != b"SCRM":
        print(f"Erreur : {path} n'est pas un fichier S3M valide (signature 'SCRM' absente).")
        sys.exit(1)

    title    = data[0:28].split(b"\x00")[0].decode("latin1").strip()
    ordnum   = u16(data, 0x20)
    insnum   = u16(data, 0x22)
    patnum   = u16(data, 0x24)

    if insnum > S3M_MAX_INSTRUMENTS:
        print(f"ATTENTION : {insnum} instruments dans le fichier, "
              f"le moteur n'en charge que {S3M_MAX_INSTRUMENTS} max (le reste sera ignoré).")
    if patnum > S3M_MAX_PATTERNS:
        print(f"ATTENTION : {patnum} motifs dans le fichier, "
              f"le moteur n'en charge que {S3M_MAX_PATTERNS} max (le reste sera ignoré).")

    ins_to_read = min(insnum, S3M_MAX_INSTRUMENTS)
    pat_to_read = min(patnum, S3M_MAX_PATTERNS)

    insptr_off = 0x60 + ordnum
    insptrs = [u16(data, insptr_off + 2 * i) for i in range(ins_to_read)]

    patptr_off = insptr_off + 2 * insnum   # le fichier avance de insnum entrées,
                                            # qu'on les charge toutes ou non
    patptrs = [u16(data, patptr_off + 2 * i) for i in range(pat_to_read)]

    sample_total  = 0
    truncated     = []
    for idx, p in enumerate(insptrs):
        if p == 0:
            continue
        off = p * 16
        if off + 0x30 > len(data):
            continue
        itype = data[off]
        if itype != 1:          # vide / instrument Adlib : pas chargé en RAM
            continue
        packflag = data[off + 0x1E]
        if packflag != 0:       # compression non supportée : pas chargé
            continue
        length = u32(data, off + 0x10)
        dest   = min(length, SAMPLE_BLOCK_CAP)
        sample_total += dest
        if length > SAMPLE_BLOCK_CAP:
            truncated.append((idx, length))

    pattern_total = 0
    for p in patptrs:
        if p == 0:
            continue
        off = p * 16
        if off + 2 > len(data):
            continue
        pattern_total += u16(data, off)

    total = sample_total + pattern_total
    budget_bytes = budget_kb * 1024

    print(f"Fichier      : {path}")
    print(f"Titre        : {title!r}")
    print(f"Taille disque: {len(data)} octets")
    print(f"OrdNum={ordnum}  InsNum={insnum}  PatNum={patnum}")
    print()
    print(f"Échantillons : {sample_total:>8} octets  ({sample_total/1024:.1f} Ko)")
    print(f"Motifs packés: {pattern_total:>8} octets  ({pattern_total/1024:.1f} Ko)")
    print(f"TOTAL far    : {total:>8} octets  ({total/1024:.1f} Ko)")
    print()

    if truncated:
        print("Échantillons tronqués à 65535 octets (limite d'un bloc _fmalloc) :")
        for idx, length in truncated:
            print(f"  - instrument {idx} : {length} -> 65535 octets "
                  f"(perte de {length - SAMPLE_BLOCK_CAP} octets en fin d'échantillon ; "
                  f"une éventuelle boucle au-delà sera désactivée)")
        print()

    print(f"Budget testé : {budget_kb} Ko ({budget_bytes} octets)")
    if total <= budget_bytes:
        marge = budget_bytes - total
        print(f"OK — tient dans le budget, marge de {marge/1024:.1f} Ko.")
    else:
        depassement = total - budget_bytes
        print(f"DEPASSEMENT de {depassement/1024:.1f} Ko — risque élevé de "
              f"'mémoire far insuffisante' au chargement (S3M_ERR_MEM).")
        print("Pistes : réduire le nombre/la durée des échantillons, "
              "les convertir en 8 bits avant export si ce n'est pas déjà fait, "
              "ou choisir un module plus compact.")


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 s3mcheck.py <fichier.s3m> [--budget Ko]")
        sys.exit(1)

    path = sys.argv[1]
    budget_kb = DEFAULT_BUDGET_KB
    if "--budget" in sys.argv:
        i = sys.argv.index("--budget")
        if i + 1 < len(sys.argv):
            budget_kb = int(sys.argv[i + 1])

    check(path, budget_kb)


if __name__ == "__main__":
    main()
