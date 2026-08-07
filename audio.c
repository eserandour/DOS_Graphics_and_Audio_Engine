/* =========================================================
   AUDIO.C — Moteur audio (musique S3M + effets WAV)
   =========================================================
   Voir audio.h pour la documentation complète de l'API.

   HISTORIQUE : le mixage a été brièvement déplacé DANS l'ISR
   (pour ne plus dépendre de la vitesse des scènes), mais c'est
   revenu en arrière : pendant tout le temps où l'ISR s'exécute,
   TOUTES les interruptions sont bloquées — y compris le timer
   qui cadence l'animation à 70 Hz (voir timer.c). Le mixage se
   déclenchant ~10-11 fois/seconde (une fois par moitié de
   buffer), ça revenait à geler périodiquement le rythme de
   TOUT le programme, pas seulement le son — nettement pire que
   le problème d'origine. Le mixage reste donc dans audioUpdate(),
   appelée depuis la boucle principale (main.c), l'ISR se
   contentant à nouveau de lever un simple drapeau (travail
   minimal, seule approche sûre pour du code tournant avec les
   IRQ désactivées).

   Pour compenser sans toucher aux scènes, deux leviers purement
   côté audio :
     1. MIX_RATE abaissé (voir audio.h) : moins d'échantillons
        à mixer par seconde = moins de travail CPU total pour
        le même contenu audible.
     2. Buffer nettement agrandi (voir MIX_BUFFER_SAMPLES) :
        marge de sécurité large avant qu'une frame lente ne
        fasse rejouer une moitié de buffer pas rafraîchie.
   ========================================================= */

#include <string.h>   /* _fmemset                            */
#include <malloc.h>   /* _ffree                              */
#include "sblaster.h"
#include "s3m.h"
#include "wav.h"
#include "audio.h"

/* Taille d'une moitié de double-buffer, en octets/échantillons
   (8 bits mono). 4096 échantillons ~= 186 ms à 22050 Hz (voir
   MIX_RATE dans audio.h) : marge pour tolérer une frame scène
   ponctuellement très lente sans rejouer de contenu périmé.
   Contrepartie : latence de playSound() du même ordre (jusqu'à
   ~186-372 ms avant qu'un effet déclenché ne devienne audible)
   — sans incidence pour de la musique de fond. */
#define MIX_BUFFER_SAMPLES  4096U

static int audioReady = 0;

static unsigned char far *bufA = NULL;
static unsigned char far *bufB = NULL;
static unsigned char far *bufRaw = NULL;
static unsigned long physBase = 0;

/* Modifiées par l'ISR, lues/acquittées par audioUpdate() :
   volatile car changées en dehors du flux normal du programme
   (voir timer_ticks dans timer.c pour la même convention). */
static volatile int needFillA = 0;
static volatile int needFillB = 0;
static volatile int currentHalf = 0;   /* moitié actuellement jouée par le DMA */

/* ---------------------------------------------------------
   ISR — travail minimal (voir HISTORIQUE ci-dessus) : le DMA
   boucle tout seul en matériel (mode auto-init), l'ISR n'a
   donc qu'à noter quelle moitié vient de se libérer. Tout le
   reste est fait par audioUpdate() depuis la boucle principale,
   HORS contexte d'interruption.
   --------------------------------------------------------- */
static void interrupt audioISR(void)
{
    sbAckIRQ();   /* acquitte le DSP et envoie l'EOI au(x) PIC(s) */

    if (currentHalf == 0)
    {
        needFillA = 1;    /* A vient de finir de jouer : libre pour remplissage */
        currentHalf = 1;  /* le DMA enchaîne déjà tout seul sur B               */
    }
    else
    {
        needFillB = 1;
        currentHalf = 0;  /* le DMA enchaîne déjà tout seul sur A               */
    }
}

/* ---------------------------------------------------------
   Mixage d'une moitié de buffer : musique puis effets, sans
   aucune atténuation (volume maximum demandé — voir audio.h).
   --------------------------------------------------------- */
static void mixBuffer(unsigned char far *buf, unsigned int n)
{
    s3mMix(buf, n);   /* remplit intégralement buf (silence si pas de musique) */
    wavMix(buf, n);   /* ajoute les effets par-dessus, avec écrêtage           */
}

/* ---------------------------------------------------------
   Cycle de vie
   --------------------------------------------------------- */

int audioInit(void)
{
    unsigned char tc;

    audioReady = 0;
    needFillA = 0;
    needFillB = 0;

    if (sbDetect() != SB_OK) return AUD_ERR_NOCARD;
    if (sbReset()  != SB_OK) return AUD_ERR_NOCARD;

    sbSetMixerVolumeMax();

    /* Un seul buffer PHYSIQUEMENT CONTIGU de 2*MIX_BUFFER_SAMPLES
       octets : bufA = première moitié, bufB = seconde moitié.
       Nécessaire pour le mode DMA auto-init (sbStartOutputLoop),
       qui boucle en matériel sur toute la plage sans réarmement
       CPU — voir sblaster.h. */
    bufA = sbAllocDmaBuffer(MIX_BUFFER_SAMPLES * 2U, &physBase, &bufRaw);
    if (!bufA)
    {
        bufA = bufB = NULL;
        bufRaw = NULL;
        return AUD_ERR_MEM;
    }
    bufB = bufA + MIX_BUFFER_SAMPLES;

    _fmemset(bufA, 128, MIX_BUFFER_SAMPLES);
    _fmemset(bufB, 128, MIX_BUFFER_SAMPLES);

    s3mInit(MIX_RATE);
    wavInit(MIX_RATE);

    /* Constante de temps DSP (commande 0x40) : compatible avec
       toutes les cartes SB, contrairement à 0x41 (DSP >= 4.xx). */
    tc = (unsigned char)(256U - (unsigned int)(1000000UL / MIX_RATE));
    sbSetTimeConstant(tc);

    sbInstallIRQ(audioISR);

    currentHalf = 0;
    /* Programmation UNIQUE du DMA + DSP en boucle auto-init :
       aucun réarmement à chaque IRQ, donc aucun clic entre les
       moitiés, même en silence total (voir sblaster.c). */
    sbStartOutputLoop(physBase, MIX_BUFFER_SAMPLES);

    audioReady = 1;
    return AUD_OK;
}

void audioUpdate(void)
{
    if (!audioReady) return;

    if (needFillA) { mixBuffer(bufA, MIX_BUFFER_SAMPLES); needFillA = 0; }
    if (needFillB) { mixBuffer(bufB, MIX_BUFFER_SAMPLES); needFillB = 0; }
}

void audioShutdown(void)
{
    if (!audioReady) return;

    sbStopOutput();     /* coupe le DMA et remet le DSP au repos */
    sbRestoreIRQ();     /* restaure le vecteur IRQ d'origine     */

    s3mUnload();
    wavStopAll();

    if (bufRaw) _ffree(bufRaw);
    bufA = bufB = NULL;
    bufRaw = NULL;

    audioReady = 0;
}

/* ---------------------------------------------------------
   Lecture
   --------------------------------------------------------- */

int playMusic(const char *filename)
{
    int r;

    if (!audioReady) return AUD_ERR_NOCARD;

    r = s3mLoad(filename);
    switch (r)
    {
    case S3M_OK:         return AUD_OK;
    case S3M_ERR_FILE:   return AUD_ERR_FILE;
    case S3M_ERR_READ:   return AUD_ERR_FORMAT;
    case S3M_ERR_FORMAT: return AUD_ERR_FORMAT;
    default:              return AUD_ERR_MEM;
    }
}

void stopMusic(void)
{
    if (!audioReady) return;

    /* s3mUnload() coupe la musique en cours ET libère intégralement
       la mémoire far qu'elle occupait (échantillons + motifs) — pas
       seulement un silence en façade. Utile en particulier vu la
       contrainte mémoire du moteur (voir s3mcheck.py) : ça permet de
       libérer de la place avant playMusic() sur un module suivant,
       sans attendre qu'il écrase l'ancien via son propre s3mUnload()
       interne. audioUpdate() continue de tourner normalement : la
       sortie DMA reste active, elle joue simplement du silence
       (voir s3mMix, qui teste s3mLoaded/s3mPlaying). Les effets WAV
       en cours ne sont pas affectés. */
    s3mUnload();
}

int isMusicPlaying(void)
{
    if (!audioReady) return 0;
    return s3mIsPlaying();
}

int hasMusicLooped(void)
{
    if (!audioReady) return 0;
    return s3mConsumeLoopFlag();
}

void fadeMusicIn(unsigned long durationMs)
{
    if (!audioReady) return;
    s3mFadeTo(100, durationMs);
}

void fadeMusicOut(unsigned long durationMs)
{
    if (!audioReady) return;
    s3mFadeTo(0, durationMs);
}

int playSound(const char *filename)
{
    if (!audioReady) return AUD_ERR_NOCARD;
    return (wavPlay(filename) == WAV_OK) ? AUD_OK : AUD_ERR_MEM;
}
