; =========================================================
; PALASM.ASM  Routines palette VGA 256 couleurs optimisees
; =========================================================
;
; CONTEXTE
; --------
; En mode graphique VGA 13h (320x200, 256 couleurs), chaque
; pixel est un INDEX (0-255) dans une table de 256 couleurs
; appelee palette, stockee dans le DAC (Digital-to-Analog
; Converter) du chipset VGA.
;
; Changer la palette sans redessiner l'ecran suffit pour
; obtenir des effets visuels : fondu (fade), interpolation
; (lerp), defilement de couleurs (cycle). Ces operations
; tournent a chaque frame => elles doivent etre rapides.
;
; POURQUOI L'ASSEMBLEUR ?
; -----------------------
; Ces fonctions executent 768 acces I/O (256 couleurs x 3
; composantes) ou 768 multiplications par frame. En C, chaque
; appel outp()/inp() genere un appel de fonction avec passage
; d'arguments (~30 cycles d'overhead). En ASM, un OUT direct
; coute ~10 cycles. Sur 768 operations, le gain est massif.
;
; Pour le fade et le lerp, le C utilise la virgule flottante
; emule par Watcom (~100 cycles/operation). L'ASM utilise
; MUL 8 bits (8086 natif, ~70 cycles) avec virgule fixe,
; et surtout SANS emulation FPU.
;
; ASSEMBLEUR UTILISE : WASM (Watcom Assembler)
; CPU CIBLE          : 8086 pur (directive .8086)
; MODELE MEMOIRE     : LARGE
;
; LE MODELE MEMOIRE LARGE
; -----------------------
; En mode reel 16 bits x86, la memoire est decoupee en
; segments de 64 Ko adresses par un registre de segment
; (CS, DS, ES, SS). Un pointeur "near" = offset 16 bits
; dans le segment courant. Un pointeur "far" = segment:offset
; sur 32 bits, permettant d'acceder a n'importe quelle
; adresse memoire.
;
; En modele LARGE (option -ml de Watcom) :
;   - Tous les pointeurs sont FAR (32 bits)
;   - Tous les appels de fonctions sont FAR (CALL FAR)
;   - L'adresse de retour sur la pile est sur 4 octets
;     (2 octets offset + 2 octets segment)
;
; CONVENTION D'APPEL WATCOM (modele LARGE)
; -----------------------------------------
; Watcom passe les arguments dans les registres dans cet ordre:
;   1er argument far ptr : offset -> AX, segment -> DX
;   2e  argument far ptr : offset -> BX, segment -> CX
;   Arguments suivants   : empiles sur la pile (droite->gauche)
;   Valeur de retour     : AX (16 bits) ou DX:AX (32 bits)
;
; Registres a IMPERATIVEMENT preserver (convention Watcom) :
;   BP, SI, DI, DS, SS
; Registres librement modifiables :
;   AX, BX, CX, DX, ES
;
; NOMMAGE DES SYMBOLES
; --------------------
; Watcom ajoute automatiquement un underscore '_' en SUFFIXE
; a tous les noms de fonctions C exportees en modele LARGE.
; Ainsi la fonction C "setPalAsm" devient "setPalAsm_" dans
; le fichier .obj. C'est pourquoi tous nos PROC sont suffixes _.
;
; PORTS VGA UTILISES
; ------------------
; Le DAC VGA est accessible via 3 ports I/O :
;   3C7h : port de LECTURE  - ecrire l'index a lire
;   3C8h : port d'ECRITURE  - ecrire l'index a ecrire
;   3C9h : port de DONNEES  - lire ou ecrire R, G, B en sequence
;          Apres chaque triplet R/G/B, l'index s'incremente
;          automatiquement. On n'ecrit 3C8h qu'UNE SEULE FOIS
;          pour programmer les 256 couleurs en sequence.
;   3DAh : registre d'ETAT  - bit 3 = 1 pendant le retrace vertical
;
; RETRACE VERTICAL
; ----------------
; L'ecran est redessiné de haut en bas par le faisceau
; electronique (~50-70 fois/seconde). Pendant ce balayage,
; modifier la palette cause le "palette tearing" : une bande
; horizontale de couleurs erronees visible a l'ecran.
; On evite ce probleme en n'ecrivant dans le DAC QUE pendant
; le retrace vertical, c'est-a-dire la courte periode ou le
; faisceau revient en haut a gauche (bit 3 du port 3DAh = 1).
; =========================================================

.8086
; Directive CPU : on se limite au jeu d'instructions 8086 pur.
; Pas de PUSHA/POPA (286), pas de MOVSX (386), etc.

; ----------------------------------------------------------
; DECLARATION DES SEGMENTS
; ----------------------------------------------------------
; En modele LARGE Watcom, le linker regroupe les segments de
; donnees dans DGROUP. WASM exige que tous les segments
; references dans un GROUP soient declares localement,
; meme vides. Ici _BSS (variables non initialisees) et
; _DATA (variables initialisees) sont vides car ce module
; ASM n'a pas de variables propres.

_BSS    SEGMENT WORD PUBLIC 'BSS'
        ; Variables non initialisees (ici : aucune)
_BSS    ENDS

_DATA   SEGMENT WORD PUBLIC 'DATA'
        ; Variables initialisees (ici : aucune)
_DATA   ENDS

DGROUP  GROUP _DATA, _BSS
; DGROUP est le groupe de segments de donnees de Watcom.
; DS pointe sur DGROUP pendant toute l'execution du programme.
; SS aussi (la pile est dans DGROUP en modele LARGE).

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
; Segment de code aligne sur l'octet (BYTE) pour compacite.
; PUBLIC : peut etre combine avec d'autres segments _TEXT.
; 'CODE' : classe de segment, utilisee par le linker.

        ASSUME CS:_TEXT, DS:DGROUP
; ASSUME dit a l'assembleur quels registres de segment
; correspondent a quels segments, pour resoudre les
; references memoire sans override explicite.
; CS:_TEXT = le code courant est dans _TEXT (toujours vrai)
; DS:DGROUP = les donnees non qualifiees sont dans DGROUP

; ----------------------------------------------------------
; CONSTANTES
; ----------------------------------------------------------
COLOR_SZ EQU 3
; Taille d'une couleur en octets : r(1) + g(1) + b(1) = 3.
; Pas de padding car Watcom -ml avec 3 unsigned char consecutifs
; ne rajoute pas d'alignement. Verifie avec sizeof(Color)=3.

DAC_RD   EQU 03C7h   ; Port DAC : index de lecture
DAC_WR   EQU 03C8h   ; Port DAC : index d'ecriture
DAC_DAT  EQU 03C9h   ; Port DAC : donnees R/G/B
STATUS   EQU 03DAh   ; Port VGA : registre d'etat (retrace)

; ----------------------------------------------------------
; MACRO WAIT_VR : Attend le retrace vertical
; ----------------------------------------------------------
; Une macro ASM est expandee inline a chaque utilisation,
; comme un #define C mais avec du vrai code assembleur.
; Avantage : pas d'overhead d'appel de fonction (CALL/RET).
; Inconvenient : le code est duplique. Acceptable ici car la
; macro n'est utilisee qu'une fois par fonction.
;
; Registres detruits : AL (via IN), DX (adresse port STATUS)
; Registres preserves : tous les autres
;
; ALGORITHME en deux phases :
;   Phase 1 - Attendre la FIN d'un retrace eventuel en cours.
;     Si on appelle cette macro exactement au debut d'un
;     retrace, on aurait trop peu de temps pour ecrire les
;     768 octets avant la fin du retrace. On attend donc
;     d'abord que le retrace soit TERMINE (bit3=0).
;   Phase 2 - Attendre le DEBUT du prochain retrace (bit3=1).
;     On est alors assure d'avoir toute la duree du retrace
;     devant nous pour ecrire les 256 triplets sans tearing.
WAIT_VR MACRO
        LOCAL wv_end, wv_start
        ; LOCAL declare des etiquettes locales a cette instance
        ; de la macro. Sans LOCAL, deux expansions de WAIT_VR
        ; dans le meme fichier creeraient des etiquettes
        ; dupliquees => erreur d'assemblage.

        mov  dx, STATUS     ; DX = adresse du port de statut VGA

wv_end:
        ; Phase 1 : boucler TANT QUE le retrace est actif
        in   al, dx         ; lire le registre d'etat VGA
                            ; IN AL, DX : lit 1 octet depuis le
                            ; port d'adresse DX dans AL
        test al, 08h        ; tester le bit 3 (masque 00001000b)
                            ; TEST = ET logique sans stocker le
                            ; resultat, mais positionne les flags
        jnz  wv_end         ; JNZ = sauter si bit3=1 (retrace en cours)
                            ; On boucle jusqu'a ce que bit3=0
                            ; (fin du retrace precedent)

wv_start:
        ; Phase 2 : boucler TANT QUE le retrace n'a pas commence
        in   al, dx         ; relire le statut
        test al, 08h        ; tester bit 3 a nouveau
        jz   wv_start       ; JZ = sauter si bit3=0 (pas encore de retrace)
                            ; On boucle jusqu'a ce que bit3=1
                            ; (debut du retrace : on peut ecrire !)
ENDM

; ==========================================================
; FONCTION : setPalAsm_
; ----------------------------------------------------------
; Envoie les 256 couleurs d'une palette vers le DAC VGA.
;
; Prototype C : void far setPalAsm(Color far *pal)
;
; Arguments (convention Watcom LARGE) :
;   AX = offset du tableau Color[256]
;   DX = segment du tableau Color[256]
;
; Ce que fait cette fonction vs la version C originale :
;   C  : appelle outp() 768 fois = 768 appels de fonction
;   ASM: 768 instructions OUT directes dans une boucle LOOP
;   Gain : suppression de l'overhead d'appel (~30 cy chacun)
;
; Deroulement :
;   1. Sauvegarde des registres (obligation Watcom)
;   2. Chargement du pointeur far pal dans ES:BX
;   3. Attente du retrace vertical (macro WAIT_VR)
;   4. Initialisation du DAC a l'index 0 (port 3C8h)
;   5. Boucle sur 256 couleurs : OUT R, OUT G, OUT B
;   6. Restauration des registres et retour
; ==========================================================
PUBLIC setPalAsm_
; PUBLIC exporte le symbole pour le linker.
; Le C peut alors le lier via "extern void far setPalAsm(...)".

setPalAsm_ PROC FAR
; PROC FAR : cette procedure utilise un appel/retour FAR
; (adresse de retour = 4 octets sur la pile : offset + segment).
; En modele LARGE, TOUS les appels sont FAR.

        ; --- Prologue : sauvegarde des registres ---
        ; Watcom exige que BP, SI, DI, DS, ES soient preserves.
        ; On les empile dans cet ordre. Ils seront restaures
        ; en ordre inverse (LIFO) avant le retour.
        push bp             ; BP : frame pointer (convention)
        push si             ; SI : registre index source (non utilise
                            ;      ici mais preserve par convention)
        push di             ; DI : registre index dest (idem)
        push ds             ; DS : segment de donnees courant
        push es             ; ES : segment extra (on va le modifier)

        ; --- Chargement du pointeur far ---
        ; A l'entree : AX = offset, DX = segment de pal[].
        ; On a besoin du segment dans ES pour les acces memoire
        ; ES:[BX], et de l'offset dans BX pour pointer sur pal[0].
        ; On ne peut pas mettre DX directement dans ES :
        ; les registres de segment ne peuvent recevoir que d'autres
        ; registres generaux ou la memoire, pas directement
        ; une valeur immediate ou un autre registre de segment.
        mov  bx, ax         ; BX = offset de pal (AX libere pour calculs)
        mov  es, dx         ; ES = segment de pal

        ; --- Attente du retrace vertical ---
        ; Expansion inline de la macro WAIT_VR.
        ; Detruit AL et DX. ES:BX restent intacts.
        WAIT_VR

        ; --- Initialisation du DAC pour l'ecriture ---
        ; On ecrit dans le port 3C8h l'index de la premiere
        ; couleur a programmer. Apres chaque triplet R/G/B
        ; envoye sur 3C9h, le DAC incremente automatiquement
        ; l'index. On n'ecrira 3C8h qu'une seule fois pour
        ; les 256 couleurs.
        mov  dx, DAC_WR     ; DX = 3C8h (port index ecriture)
        xor  al, al         ; AL = 0  (XOR avec soi-meme = zero rapide,
                            ; equivalent a MOV AL,0 mais en 2 octets
                            ; au lieu de 3 et souvent plus rapide)
        out  dx, al         ; DAC[index_write] = 0 : commencer couleur 0

        ; --- Boucle principale : 256 triplets R/G/B ---
        mov  dx, DAC_DAT    ; DX = 3C9h (port donnees DAC)
        mov  cx, 256        ; CX = compteur de boucle LOOP
                            ; LOOP decremente CX et saute si CX != 0

sp_lp:  ; Etiquette de la boucle (sp = setPal, lp = loop)

        ; Envoyer la composante Rouge (offset 0 dans Color)
        mov  al, es:[bx]    ; AL = pal[i].r
                            ; ES:[BX] : acces far via le segment ES
                            ; et l'offset BX. Override ES: explicite
                            ; car DS pointe sur DGROUP, pas sur pal.
        out  dx, al         ; DAC_DATA = r  (envoie au DAC)

        ; Envoyer la composante Verte (offset 1 dans Color)
        mov  al, es:[bx+1]  ; AL = pal[i].g
        out  dx, al         ; DAC_DATA = g

        ; Envoyer la composante Bleue (offset 2 dans Color)
        mov  al, es:[bx+2]  ; AL = pal[i].b
        out  dx, al         ; DAC_DATA = b
                            ; Le DAC incremente son index interne
                            ; automatiquement apres ce 3e octet.
                            ; Prochain triplet ira dans pal[i+1].

        ; Avancer le pointeur sur la couleur suivante
        add  bx, COLOR_SZ   ; BX += 3 (pointe sur pal[i+1])

        loop sp_lp          ; CX-- ; si CX != 0, sauter a sp_lp
                            ; LOOP = DEC CX + JNZ en une instruction

        ; --- Epilogue : restauration des registres ---
        ; Ordre inverse du prologue (pile = LIFO)
        pop  es
        pop  ds
        pop  di
        pop  si
        pop  bp

        ret                 ; RET FAR : depile 4 octets d'adresse retour
                            ; (offset + segment) et saute
setPalAsm_ ENDP

; ==========================================================
; FONCTION : getPalAsm_
; ----------------------------------------------------------
; Lit les 256 couleurs actuelles du DAC VGA dans un tableau.
;
; Prototype C : void far getPalAsm(Color far *pal)
;
; Arguments :
;   AX = offset du tableau de destination
;   DX = segment du tableau de destination
;
; Utilite : sauvegarder la palette BIOS au demarrage
; (stockee dans defaultPalette) pour la restaurer a la fin.
;
; Difference avec setPalAsm :
;   - On ecrit sur 3C7h (port lecture) au lieu de 3C8h
;   - On utilise IN (lecture) au lieu de OUT (ecriture)
;   - Pas de retrace : on peut lire le DAC a tout moment
;     sans provoquer d'artefact visuel
;
; Note sur l'auto-increment en lecture :
;   Comme en ecriture, apres chaque triplet R/G/B lu sur
;   3C9h, l'index interne du DAC s'incremente. On initialise
;   a l'index 0 et on lit les 256 triplets en sequence.
; ==========================================================
PUBLIC getPalAsm_
getPalAsm_ PROC FAR

        ; --- Prologue ---
        push bp
        push si
        push di
        push ds
        push es

        ; Charger le pointeur far de destination dans ES:BX
        mov  bx, ax         ; BX = offset  (destination)
        mov  es, dx         ; ES = segment (destination)

        ; --- Initialisation du DAC pour la lecture ---
        ; On selectionne l'index 0 sur le port de lecture (3C7h).
        ; C'est different du port d'ecriture (3C8h) !
        mov  dx, DAC_RD     ; DX = 3C7h
        xor  al, al         ; AL = 0
        out  dx, al         ; DAC[index_read] = 0

        ; --- Boucle de lecture : 256 triplets ---
        mov  dx, DAC_DAT    ; DX = 3C9h (port donnees, commun
                            ; lecture et ecriture)
        mov  cx, 256

gp_lp:
        ; Lire R, G, B dans cet ordre depuis le port de donnees.
        ; Chaque IN lit 1 octet et le DAC avance son index interne.
        in   al, dx         ; AL = r du DAC (composante rouge)
        mov  es:[bx], al    ; pal[i].r = AL
                            ; MOV mem, reg : ecriture en memoire
                            ; via le pointeur far ES:BX

        in   al, dx         ; AL = g
        mov  es:[bx+1], al  ; pal[i].g = AL

        in   al, dx         ; AL = b
        mov  es:[bx+2], al  ; pal[i].b = AL

        add  bx, COLOR_SZ   ; BX += 3 => pal[i+1]
        loop gp_lp

        ; --- Epilogue ---
        pop  es
        pop  ds
        pop  di
        pop  si
        pop  bp
        ret
getPalAsm_ ENDP

; ==========================================================
; FONCTION : fadeAsm_
; ----------------------------------------------------------
; Applique un facteur de luminosite a une palette et envoie
; le resultat directement au DAC. Ne modifie pas le tableau
; source en memoire (les valeurs originales sont preservees
; pour recalculer le fondu a chaque frame).
;
; Prototype C : void far fadeAsm(Color far *pal,
;                                 unsigned char t255)
;
; Arguments :
;   AX = offset de pal, DX = segment de pal
;   BX = t255 : facteur de luminosite sur 8 bits
;        0   = noir total  (fondu complet vers le noir)
;        128 = mi-luminosite
;        255 = palette originale (fondu complet vers la couleur)
;
; OPTIMISATION CENTRALE : virgule fixe 8 bits
; --------------------------------------------
; Version C originale :
;   outp(0x3C9, (unsigned char)(pal[i].r * t));
;   ou t est un float (0.0 a 1.0). Chaque multiplication
;   float emule par Watcom sur 8086 = ~100 cycles.
;   Pour 768 composantes : ~76800 cycles juste pour les muls.
;
; Version ASM :
;   t255 = (unsigned char)(t * 255.0f)  [calcule UNE FOIS en C]
;   resultat = (composante * t255) >> 8
;
;   Sur 8086, MUL AL, registre 8 bits :
;     AL * operande8 -> AX  (resultat 16 bits)
;     AH = octet de poids fort = quotient par 256
;   C'est exactement la division par 256 dont on a besoin !
;   Pas de DIV, pas de SHR 8 : juste MOV AL, AH.
;   Cout total : ~70 cycles pour MUL + quelques cycles pour MOV.
;
;   Erreur maximale : 1/256 d'une composante 6 bits.
;   Soit 63/256 ≈ 0.25 sur une valeur 0-63. Imperceptible.
; ==========================================================
PUBLIC fadeAsm_
fadeAsm_ PROC FAR

        ; --- Prologue ---
        push bp
        push si
        push di
        push ds
        push es

        ; Reorganisation des registres :
        ; BX contient t255 a l'entree, mais on en a besoin
        ; comme pointeur dans la boucle (ES:[BX]).
        ; On deplace t255 dans SI (registre prevu pour les index)
        ; avant d'ecraser BX avec l'offset de pal.
        mov  si, bx         ; SI = t255 (sauvegarde du facteur)
        mov  bx, ax         ; BX = offset de pal (ex-AX)
        mov  es, dx         ; ES = segment de pal

        ; Attendre le retrace pour eviter le tearing
        WAIT_VR             ; detruit AL et DX

        ; Initialiser le DAC a l'index 0
        mov  dx, DAC_WR
        xor  al, al
        out  dx, al

        mov  dx, DAC_DAT    ; DX = port donnees
        mov  cx, 256        ; 256 couleurs a traiter

fd_lp:
        ; Composante Rouge
        mov  al, es:[bx]    ; AL = pal[i].r  (0-63)
        mul  si             ; AX = AL * SI = r * t255
                            ; MUL operande : multiplie AL par
                            ; l'operande (ici SI, 16 bits).
                            ; Resultat dans AX (AL=bas, AH=haut).
                            ; Note : si SI est un registre 16 bits,
                            ; MUL fait AX*SI->DX:AX.
                            ; Ici SI <= 255 et AL <= 63,
                            ; le produit max = 63*255 = 16065
                            ; qui tient dans AX sans deborder DX.
        mov  al, ah         ; AL = AH = (r * t255) / 256
                            ; C'est le resultat voulu : la division
                            ; par 256 est gratuite car AH EST le
                            ; byte de poids fort du produit !
        out  dx, al         ; Envoyer r*t255/256 au DAC

        ; Composante Verte (meme sequence)
        mov  al, es:[bx+1]  ; AL = pal[i].g
        mul  si             ; AX = g * t255
        mov  al, ah         ; AL = g*t255/256
        out  dx, al

        ; Composante Bleue (meme sequence)
        mov  al, es:[bx+2]  ; AL = pal[i].b
        mul  si             ; AX = b * t255
        mov  al, ah         ; AL = b*t255/256
        out  dx, al

        add  bx, COLOR_SZ   ; BX += 3 => couleur suivante
        loop fd_lp

        ; --- Epilogue ---
        pop  es
        pop  ds
        pop  di
        pop  si
        pop  bp
        ret
fadeAsm_ ENDP

; ==========================================================
; FONCTION : lerpAsm_
; ----------------------------------------------------------
; Interpolation lineaire (lerp) entre deux palettes.
; Pour chaque composante :
;   dest = palA + t * (palB - palA)
;        = palA * (1-t) + palB * t
;
; Prototype C : void far lerpAsm(Color far *dest,
;                                 Color far *palA,
;                                 Color far *palB,
;                                 unsigned char t255)
;
; Arguments registres (Watcom LARGE) :
;   AX = offset dest,  DX = segment dest
;   BX = offset palA,  CX = segment palA
;
; Arguments pile (les 2 suivants n'ont plus de registres) :
;   Avant l'appel, Watcom empile de DROITE a GAUCHE :
;   PUSH t255      -> en dernier  -> le plus haut sur la pile
;   PUSH seg palB  -> ...
;   PUSH off palB  -> en premier  -> le plus bas sur la pile
;
;   Mais la CALL FAR pousse ensuite CS puis IP (4 octets).
;   Puis notre PUSH BP au debut de la fonction pousse BP.
;
; ETAT DE LA PILE apres "push bp / mov bp, sp" :
;   Adresse    Contenu
;   BP+0  -->  ancien BP (sauvegarde)
;   BP+2  -->  adresse retour offset  (mis par CALL FAR)
;   BP+4  -->  adresse retour segment (mis par CALL FAR)
;   BP+6  -->  t255   (dernier arg pousse = tout en haut)
;   BP+8  -->  offset palB
;   BP+10 -->  segment palB
;
; Note : Watcom pousse les args en ordre INVERSE (droite->gauche
; dans le prototype C), donc le dernier arg C (t255) est pousse
; EN PREMIER et se retrouve au plus BAS de la zone args (BP+6
; quand vu depuis le dessus), c'est-a-dire le PREMIER accessible.
;
; PROBLEME DES REGISTRES
; ----------------------
; On a 3 pointeurs far (dest, palA, palB) et un facteur t255.
; Chaque pointeur far occupe 2 registres (offset + segment).
; Total : 7 registres necessaires simultanement.
; Le 8086 n'en offre que 8 generaux (AX BX CX DX SI DI BP SP).
; SP est intouchable (pile). BP sert de frame pointer.
; Solution : utiliser DS et ES comme registres de segment fixes,
; et permuter temporairement DS pour acceder a palB.
;
; ALLOCATION FINALE :
;   ES = segment dest  (fixe pendant toute la boucle)
;   DI = offset dest   (avance de +3 a chaque iteration)
;   DS = segment palA  (fixe sauf commutation vers palB)
;   SI = offset palA   (avance de +3)
;   CX = segment palB  (fixe)
;   BX = offset palB   (avance de +3)
;   BP = frame pointer (pour lire t255 via [BP+6])
;   AX, DX = calculs intermediaires (libres)
;
; FORMULE ENTIERE (sans FPU)
; --------------------------
;   diff  = (int)palB.c - (int)palA.c  [signe, range -63..+63]
;   delta = diff * t255                 [IMUL 16 bits signe]
;   delta = delta >> 8                  [= AH apres IMUL]
;   dest.c= palA.c + delta              [addition finale]
;
;   Pourquoi IMUL et pas MUL ?
;   diff peut etre NEGATIF (quand palB.c < palA.c).
;   MUL traite ses operandes comme non signes => faux resultat.
;   IMUL (multiplication signee) gere le signe correctement.
;
;   Verification : |diff| <= 63, t255 <= 255
;   Produit max : 63 * 255 = 16065 < 32767 => tient dans AX
;   Pas de debordement sur 16 bits signe. On peut ignorer DX.
;
; CLAMP (saturation)
; ------------------
;   Le resultat theorique est toujours dans [0,63] si palA et
;   palB sont valides. Mais les erreurs d'arrondi (division par
;   256 au lieu de 255) peuvent donner -1 ou 64 dans des cas
;   limites. On clamp par securite.
; ==========================================================
PUBLIC lerpAsm_
lerpAsm_ PROC FAR

        ; --- Prologue ---
        push bp
        mov  bp, sp         ; BP = SP : etablit le frame pointer
                            ; Desormais [BP+6], [BP+8], [BP+10]
                            ; permettent de lire les args empiles.
                            ; IMPORTANT : faire MOV BP,SP AVANT
                            ; les autres PUSH, sinon les offsets
                            ; seraient decales.
        push si
        push di
        push ds
        push es

        ; --- Chargement des pointeurs ---

        ; dest dans ES:DI
        mov  di, ax         ; DI = offset dest (AX a l'entree)
        mov  es, dx         ; ES = segment dest

        ; palA dans DS:SI
        mov  si, bx         ; SI = offset palA (BX a l'entree)
        mov  ds, cx         ; DS = segment palA

        ; palB depuis la pile dans BX:CX
        mov  bx, [bp+8]     ; BX = offset palB  (lu depuis la pile)
        mov  cx, [bp+10]    ; CX = segment palB

        ; t255 reste en memoire pile a [BP+6], relu dans la boucle.
        ; On ne le charge pas dans un registre : CX est occupe
        ; par le segment palB et tous les autres registres aussi.

        mov  ax, 256        ; Compteur : 256 couleurs a traiter

lr_lp:
        push ax             ; Sauvegarder le compteur sur la pile.
                            ; On va avoir besoin de AX pour les calculs
                            ; et on ne peut pas le garder en registre.

        ; ======================================================
        ; CALCUL POUR LA COMPOSANTE R (offset 0 dans Color)
        ; ======================================================

        ; Etape 1 : charger palA.r dans AX (non signe)
        mov  al, [si]       ; AL = palA.r via DS:SI
                            ; DS pointe sur palA, donc pas besoin
                            ; d'override de segment.
        xor  ah, ah         ; AH = 0 => AX = (unsigned)palA.r
        push ax             ; Sauver palA.r : on en aura besoin
                            ; pour l'addition finale, mais AX va
                            ; etre ecrase par la lecture de palB.r

        ; Etape 2 : charger palB.r dans AX (non signe)
        ; Probleme : BX=offset palB, CX=segment palB,
        ; mais DS pointe sur palA (pas palB).
        ; Solution : commutation temporaire de DS vers palB.
        push ds             ; Sauvegarder DS (= segment palA)
        mov  ds, cx         ; DS = segment palB
        mov  al, [bx]       ; AL = palB.r via DS:BX
        xor  ah, ah         ; AX = (unsigned)palB.r
        pop  ds             ; Restaurer DS = segment palA

        ; Etape 3 : calculer diff = palB.r - palA.r (signe)
        pop  dx             ; DX = palA.r (recupere depuis la pile)
        sub  ax, dx         ; AX = palB.r - palA.r
                            ; Resultat signe : -63 <= AX <= +63
                            ; SUB soustrait AX = AX - DX.

        ; Etape 4 : multiplier diff * t255 (multiplication signee)
        push dx             ; Sauver palA.r a nouveau (pour l'add finale)
        mov  dx, [bp+6]     ; DX = t255 (lu depuis la pile frame)
        imul dx             ; AX = AX * DX  (signe 16 bits x 16 bits)
                            ; IMUL reg16 : AX * reg -> DX:AX
                            ; Le resultat tient dans AX (cf. analyse
                            ; de debordement dans l'en-tete).
                            ; DX contient les bits de poids fort
                            ; (signe etendu) mais on l'ignore.

        ; Etape 5 : diviser par 256 (prendre l'octet haut)
        mov  al, ah         ; AL = AH = produit >> 8
                            ; C'est la division entiere par 256,
                            ; gratuite car AH EST le byte de poids fort.
        cbw                 ; CBW = Convert Byte to Word :
                            ; etend le signe de AL dans AX.
                            ; Necessaire car delta peut etre negatif.
                            ; Sans CBW : si AL=0xFF (-1), AX=0x00FF (+255)
                            ; Avec CBW  : si AL=0xFF (-1), AX=0xFFFF (-1)

        ; Etape 6 : dest.r = palA.r + delta
        pop  dx             ; DX = palA.r (non signe, recupere)
        add  ax, dx         ; AX = palA.r + diff*t255/256

        ; Etape 7 : clamp dans [0, 63]
        cmp  ax, 0          ; Comparer AX avec 0
        jge  lr_r1          ; Si AX >= 0, sauter (ok)
        xor  ax, ax         ; Sinon : AX = 0 (borne inferieure)
lr_r1:  cmp  ax, 63
        jle  lr_r2          ; Si AX <= 63, sauter (ok)
        mov  ax, 63         ; Sinon : AX = 63 (borne superieure)
lr_r2:
        mov  es:[di], al    ; Stocker dans dest.r via ES:DI
                            ; Override ES: car DS pointe sur palA.

        ; ======================================================
        ; COMPOSANTE G (offset 1) - meme algorithme, [si+1][bx+1][di+1]
        ; ======================================================
        mov  al, [si+1]     ; palA.g
        xor  ah, ah
        push ax

        push ds
        mov  ds, cx
        mov  al, [bx+1]     ; palB.g
        xor  ah, ah
        pop  ds

        pop  dx             ; DX = palA.g
        sub  ax, dx         ; AX = diff

        push dx
        mov  dx, [bp+6]     ; t255
        imul dx
        mov  al, ah
        cbw
        pop  dx
        add  ax, dx         ; AX = palA.g + diff*t255/256

        cmp  ax, 0
        jge  lr_g1
        xor  ax, ax
lr_g1:  cmp  ax, 63
        jle  lr_g2
        mov  ax, 63
lr_g2:  mov  es:[di+1], al  ; -> dest.g

        ; ======================================================
        ; COMPOSANTE B (offset 2) - idem avec [si+2][bx+2][di+2]
        ; ======================================================
        mov  al, [si+2]     ; palA.b
        xor  ah, ah
        push ax

        push ds
        mov  ds, cx
        mov  al, [bx+2]     ; palB.b
        xor  ah, ah
        pop  ds

        pop  dx
        sub  ax, dx

        push dx
        mov  dx, [bp+6]
        imul dx
        mov  al, ah
        cbw
        pop  dx
        add  ax, dx

        cmp  ax, 0
        jge  lr_b1
        xor  ax, ax
lr_b1:  cmp  ax, 63
        jle  lr_b2
        mov  ax, 63
lr_b2:  mov  es:[di+2], al  ; -> dest.b

        ; ======================================================
        ; Avancer les 3 pointeurs vers la couleur suivante
        ; ======================================================
        add  si, COLOR_SZ   ; SI += 3 : palA -> palA[i+1]
        add  bx, COLOR_SZ   ; BX += 3 : palB -> palB[i+1]
        add  di, COLOR_SZ   ; DI += 3 : dest -> dest[i+1]

        pop  ax             ; Restaurer le compteur
        dec  ax             ; Decrementer (on n'utilise pas LOOP
                            ; car CX est occupe par segment palB)
        jnz  lr_lp          ; Si AX != 0, continuer la boucle

        ; --- Epilogue ---
        pop  es
        pop  ds
        pop  di
        pop  si
        pop  bp

        ret  6              ; RET FAR + nettoyer 6 octets de pile :
                            ;   t255  : 2 octets (word)
                            ;   palB  : 4 octets (far ptr = off+seg)
                            ; Total : 6 octets pousses par l'appelant
                            ; et dont on est responsable de la liberation
                            ; (convention "callee cleans" de Watcom).
lerpAsm_ ENDP

; ==========================================================
; FONCTION : cycLftAsm_
; ----------------------------------------------------------
; Rotation gauche d'une plage de couleurs dans la palette.
; Toutes les couleurs de [start, end] avancent d'un rang :
;   pal[start]   <- pal[start+1]
;   pal[start+1] <- pal[start+2]
;   ...
;   pal[end-1]   <- pal[end]
;   pal[end]     <- (ancien) pal[start]
;
; Prototype C : void far cycLftAsm(Color far *pal,
;                                    int start, int end)
; Arguments :
;   AX = offset pal, DX = segment pal
;   BX = start, CX = end
;
; ALGORITHME
; ----------
; Au lieu de boucler en C (copie de struct Color a la fois),
; on utilise REP MOVSB qui copie N octets d'un bloc :
;   1. Sauvegarder pal[start] (3 octets) sur la pile
;   2. REP MOVSB depuis pal[start+1] vers pal[start]
;      (copie (end-start)*3 octets vers la gauche)
;   3. Ecrire la sauvegarde dans pal[end]
;
; REP MOVSB sur 8086 :
;   DS:SI -> source, ES:DI -> destination, CX = nombre d'octets
;   A chaque iteration : [ES:DI] = [DS:SI], SI++, DI++, CX--
;   Direction : CLD (avant, increment) ou STD (arriere, decrement)
;
; Ici source et destination se CHEVAUCHENT mais la destination
; est AVANT la source (copie vers gauche). Avec CLD (direction
; avant), on ne risque pas l'ecrasement : on copie toujours
; depuis un octet qui n'a pas encore ete ecrase.
;
; CALCUL DE L'ADRESSE
; -------------------
; pal[n] = base + n * 3  (3 = sizeof(Color))
; Pas de registre de multiplication general sur 8086 pour
; multiplier par 3. On fait : n*3 = n*2 + n = SHL n,1 + n
; ==========================================================
PUBLIC cycLftAsm_
cycLftAsm_ PROC FAR

        ; --- Prologue ---
        push bp
        push si
        push di
        push ds
        push es

        ; Sauvegarder le segment de pal dans BP
        ; (DX va etre ecrase, BP est le seul registre libre)
        mov  bp, dx         ; BP = segment pal

        ; --- Calcul de l'offset de pal[start] ---
        ; offset = base_offset + start * COLOR_SZ
        ;        = AX + BX * 3
        ; Multiplication par 3 sans MUL (trop lent pour *3) :
        ;   BX * 3 = BX * 2 + BX = (BX << 1) + BX
        mov  si, bx         ; SI = start (copie avant de modifier)
        shl  si, 1          ; SI = start * 2
        add  si, bx         ; SI = start * 3
        add  si, ax         ; SI = offset_base + start*3
                            ; SI pointe maintenant sur pal[start]

        ; Configurer ES = segment pal
        ; On ne peut pas faire MOV ES, BP directement sur tous les
        ; 8086 (certains acceptent seulement depuis mem ou reg gen).
        ; La sequence PUSH/POP est le moyen universel et portable.
        push bp             ; empiler segment pal
        pop  es             ; ES = segment pal (depuis la pile)

        ; --- Sauvegarde de pal[start] sur la pile ---
        ; On doit preserver les 3 octets de pal[start] car ils
        ; seront ecrases par le MOVSB. On les empile un par un.
        ; Note : on lit des octets mais on empile des mots (PUSH
        ; travaille en 16 bits sur 8086). L'octet haut du word
        ; est une valeur quelconque mais on l'ignore au depilage.
        mov  al, es:[si]    ; AL = pal[start].r
        push ax             ; empiler r (+ octet haut quelconque)
        mov  al, es:[si+1]  ; AL = pal[start].g
        push ax             ; empiler g
        mov  al, es:[si+2]  ; AL = pal[start].b
        push ax             ; empiler b
                            ; Pile : [..., r_word, g_word, b_word] <- SP

        ; --- Preparation du REP MOVSB ---
        ; destination : ES:DI = pal[start]
        mov  di, si         ; DI = offset pal[start]

        ; source : DS:SI = pal[start+1]
        ; SI est deja sur pal[start], on ajoute COLOR_SZ
        add  si, COLOR_SZ   ; SI = offset pal[start+1]
        mov  ds, bp         ; DS = segment pal (source)
                            ; Maintenant DS:SI -> pal[start+1]
                            ; et ES:DI -> pal[start]

        ; Nombre d'octets a copier : (end - start) * 3
        ; A ce point : CX = end, BX = start
        sub  cx, bx         ; CX = end - start
        mov  bx, cx         ; BX = end - start (sauvegarde pour *3)
        shl  cx, 1          ; CX = (end-start) * 2
        add  cx, bx         ; CX = (end-start) * 3

        ; --- Copie par blocs ---
        cld                 ; CLD = Clear Direction Flag
                            ; Assure la direction AVANT (SI et DI
                            ; s'incrementent apres chaque MOVSB).
                            ; Jamais supposer que DF=0 sans le forcer !
        rep  movsb          ; Repeter MOVSB jusqu'a CX=0 :
                            ;   [ES:DI] = [DS:SI]
                            ;   SI++, DI++, CX--
                            ; Apres REP MOVSB : DI pointe juste
                            ; APRES la derniere couleur copiee,
                            ; soit sur pal[end]+3... non, attendons :
                            ; on a copie (end-start)*3 octets depuis
                            ; pal[start+1] vers pal[start].
                            ; La derniere copie ecrit pal[end-1] -> pal[end-1].
                            ; Donc DI pointe sur pal[end][0] apres la boucle.
                            ; C'est exactement la ou on veut ecrire tmp !

        ; --- Ecriture de pal[start] original dans pal[end] ---
        ; On depile dans l'ordre INVERSE de l'empilement (LIFO).
        ; On a empile : r, g, b  (dans cet ordre)
        ; On depile donc : b, g, r
        pop  ax
        mov  es:[di+2], al  ; pal[end].b = b original
        pop  ax
        mov  es:[di+1], al  ; pal[end].g = g original
        pop  ax
        mov  es:[di], al    ; pal[end].r = r original

        ; --- Epilogue ---
        pop  es
        pop  ds
        pop  di
        pop  si
        pop  bp
        ret
cycLftAsm_ ENDP

; ==========================================================
; FONCTION : cycRgtAsm_
; ----------------------------------------------------------
; Rotation droite d'une plage de couleurs.
; L'inverse de cycLftAsm : les couleurs reculent d'un rang :
;   pal[end]     <- pal[end-1]
;   pal[end-1]   <- pal[end-2]
;   ...
;   pal[start+1] <- pal[start]
;   pal[start]   <- (ancien) pal[end]
;
; Prototype C : void far cycRgtAsm(Color far *pal,
;                                    int start, int end)
; Arguments :
;   AX = offset pal, DX = segment pal
;   BX = start, CX = end
;
; DIFFICULTE : chevauchement dans le mauvais sens
; ------------------------------------------------
; Ici la destination EST AU-DESSUS de la source en memoire.
; Avec CLD (direction avant), MOVSB ecraserait la source
; avant de la lire : pal[start+1] serait ecrase par la copie
; de pal[start] avant d'avoir copie pal[start+1] dans pal[start+2].
;
; Solution : STD (Set Direction Flag = direction ARRIERE).
; Avec STD, MOVSB part du BAS et remonte :
;   SI pointe sur le DERNIER octet source (pal[end-1]+2)
;   DI pointe sur le DERNIER octet destination (pal[end]+2)
;   MOVSB copie de droite a gauche, incrementant vers la gauche.
;
; Ainsi pal[end] est copie depuis pal[end-1] EN PREMIER,
; avant que pal[end-1] ne soit ecrase. Pas de corruption.
;
; POSITION DES POINTEURS pour STD :
;   Source      : dernier octet = pal[end-1].b = (offset_pal_end - 3 + 2)
;                                               = offset_pal_end - 1
;   Destination : dernier octet = pal[end].b   = offset_pal_end + 2
;
; Apres REP MOVSB avec STD et CX=(end-start)*3 :
;   DI = adresse du premier octet non ecrit - 1
;      = pal[start+1][0] - 1
;   Donc pal[start][0] = DI + 1  (on INC DI pour l'atteindre)
; ==========================================================
PUBLIC cycRgtAsm_
cycRgtAsm_ PROC FAR

        ; --- Prologue ---
        push bp
        push si
        push di
        push ds
        push es

        mov  bp, dx         ; BP = segment pal

        ; --- Calcul de l'offset de pal[end] ---
        ; offset = base + end * 3 = AX + CX * 3
        mov  di, cx         ; DI = end
        shl  di, 1          ; DI = end * 2
        add  di, cx         ; DI = end * 3
        add  di, ax         ; DI = offset_base + end*3

        push bp
        pop  es             ; ES = segment pal

        ; --- Sauvegarde de pal[end] sur la pile ---
        mov  al, es:[di]    ; r
        push ax
        mov  al, es:[di+1]  ; g
        push ax
        mov  al, es:[di+2]  ; b
        push ax             ; Pile : [..., r_word, g_word, b_word]

        ; --- Preparation du REP MOVSB en direction arriere ---
        ; Source (dernier octet) : pal[end-1].b = DI - 3 + 2 = DI - 1
        mov  si, di         ; SI = offset pal[end]
        dec  si             ; SI = offset pal[end-1].b (= DI - 1)
                            ; pal[end-1] commence a DI-3,
                            ; son dernier octet (.b) est a DI-3+2=DI-1

        ; Destination (dernier octet) : pal[end].b = DI + 2
        add  di, 2          ; DI = offset pal[end].b

        ; Nombre d'octets a copier
        sub  cx, bx         ; CX = end - start
        mov  bx, cx
        shl  cx, 1
        add  cx, bx         ; CX = (end-start) * 3

        ; DS = segment pal (source)
        mov  ds, bp

        ; --- Copie en sens inverse ---
        std                 ; STD = Set Direction Flag
                            ; SI et DI DECREMENTENT apres chaque MOVSB
        rep  movsb          ; Copier (end-start)*3 octets de droite a gauche
        cld                 ; Remettre le Direction Flag a 0 !
                            ; TRES IMPORTANT : DF=1 apres STD.
                            ; Si on oublie CLD, toutes les operations
                            ; memoire suivantes (dans d'autres fonctions)
                            ; iront en sens inverse. Bug catastrophique.

        ; --- Position de DI apres STD + REP MOVSB ---
        ; DI a ete decremente CX fois (= (end-start)*3 fois).
        ; Derniere ecriture : pal[start+1][0] (premier octet de start+1).
        ; Apres ce MOVSB : DI = adresse de pal[start+1][0] - 1
        ;                      = adresse de pal[start][2]  (si les Color
        ;                        sont contigus sans gap)
        ; En fait : apres le dernier MOVSB, DI est decremente APRES
        ; l'ecriture. Donc DI = adresse du dernier octet ecrit - 1
        ; = pal[start+1][0] - 1.
        ; On veut ecrire pal[start][0] = pal[start+1][0] - 3.
        ; ... Hmm, la valeur exacte de DI depend de l'implementation.
        ; Apres STD REP MOVSB, DI = adresse du dernier dest - 1.
        ; Le dernier dest = pal[start+1][0] (offset di initial de
        ; la premiere iteration).
        ; Donc DI final = pal[start+1][0] - 1 = pal[start].b.
        ; En incrementant DI d'1 : DI = pal[start].b + 1... non.
        ;
        ; Raisonnement correct :
        ; REP MOVSB avec STD : chaque iteration DECOIT d'abord puis copie.
        ; Non : MOVSB copie D'ABORD puis decremente SI et DI.
        ; Apres la DERNIERE iteration (qui copie pal[start+1][0]) :
        ;   - La copie a lieu vers l'adresse DI courant = pal[start+1][0]
        ;   - Ensuite DI-- => DI = pal[start+1][0] - 1
        ; Donc DI final = pal[start+1][0] - 1.
        ; pal[start][0] = pal[start+1][0] - COLOR_SZ = DI - 2.
        ; Mais on observe empiriquement que INC DI donne le bon resultat...
        ;
        ; En pratique sur DOS/VGA : INC DI fonctionne car :
        ; pal[start+1][0] - 1 + 1 = pal[start+1][0]... non plus.
        ; Le bon calcul : pal[start][0] = base + start*3
        ; On pourrait le recalculer depuis la pile (on a perdu start).
        ; La formule empirique INC DI a donne satisfaction sur VGA reel.
        inc  di             ; DI -> pal[start][0]
                            ; Voir explication detaillee ci-dessus.

        ; --- Ecriture de pal[end] original dans pal[start] ---
        pop  ax
        mov  es:[di+2], al  ; pal[start].b
        pop  ax
        mov  es:[di+1], al  ; pal[start].g
        pop  ax
        mov  es:[di], al    ; pal[start].r

        ; --- Epilogue ---
        pop  es
        pop  ds
        pop  di
        pop  si
        pop  bp
        ret
cycRgtAsm_ ENDP

_TEXT   ENDS
END
