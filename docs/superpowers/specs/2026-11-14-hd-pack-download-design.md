# Pack HD téléchargé par le jeu (textures FatalPixels + films SeedVR2)

Date : 2026-11-14. Décisions de recompsmith, sauf les points marqués **(choix de Claude)**.

## Intention

Que tous les joueurs aient le rendu HD que recompsmith a validé sur son PC, sans rien
installer ni configurer :
- les textures 3D agrandies avec FatalPixels (mode `auto`, UltraSharp fondu dans
  les zones lisses), le pack validé le 2026-11-14 (12 942 textures, ~1,3 Go) ;
- les 6 cinématiques refaites avec SeedVR2, rendu validé sur l'opening
  (« 1000 fois mieux » que l'ancienne HD Real-ESRGAN).

Aujourd'hui rien n'arrive chez les joueurs : `textures-hd` ne tourne que si
`game\textures-hd-outil` a été installé à la main, et les films HD ne sont faits
que sur le PC de recompsmith (`tools/upscale_movies.py`, Python + ffmpeg).

## Ce que voit le joueur

1. Il lance le jeu et joue tout de suite, avec le rendu habituel (sans pack).
2. Le jeu télécharge le pack en arrière-plan. Une petite barre de progression
   discrète en bas à gauche de la fenêtre montre où il en est (même endroit et
   même style que la barre actuelle de `textures-hd`, `frontend_sdl.c`
   `hdbar_update`, couche `GL_LAYER_HDBAR`).
3. Quand c'est fini, les textures et les films HD sont pris en compte sans
   redémarrer (le jeu sait déjà recharger le pack, voir `gl_render.c`).
4. Pas de clic, pas de question au premier lancement. Le jeu ne doit **jamais
   ramer ni saccader** à cause du téléchargement (anti-but n° 1).
5. Si le téléchargement est interrompu (jeu fermé, connexion coupée), il reprend
   au lancement suivant là où il s'était arrêté.

## Buts, non-buts, anti-buts

Buts
- Pack de textures et films HD prêts à l'emploi, les mêmes pour tous.
- Téléchargement automatique, en arrière-plan, reprise, barre discrète.
- Le joueur sans Internet joue normalement (sans pack), sans message bloquant.

Non-buts
- Plus d'IA chez le joueur : ni FatalPixels, ni SeedVR2, ni outil à installer.
  (`textures-hd` reste pour recompsmith / les développeurs : PR #147.)
- Pas de pack pour les sprites 2D (`dump-2d`), le jeu les lisse déjà (xBR).
- Pas de pack dans le zip principal du jeu.

Anti-buts (échec même si « ça marche »)
- Le jeu rame ou saccade pendant le téléchargement ou l'extraction.
- Le jeu attend le réseau au démarrage, ou affiche une erreur bloquante hors ligne.
- Un pack à moitié extrait est utilisé (textures noires, films coupés).
- Le pack d'un joueur écrase ses propres textures HD (`textures\hd`, autres packs).

## Contraintes

- **Droits** : ce sont les textures et cinématiques de Square Enix, agrandies.
  recompsmith accepte le risque ; si on le lui demande, on retire les fichiers.
  Ils ne sont **jamais commités** (règle CLAUDE.md) : seulement des fichiers
  attachés à une release GitHub du dépôt public `recompsmith/Dragon-Quest-IX-Modern`.
- **Crédits** : citer FatalPixels (twittman, CC-BY-NC-SA-4.0), UltraSharp
  (Kim2091) et SeedVR2 (ByteDance) dans le README et dans la note de la release
  du pack. Usage non commercial uniquement.
- GitHub : 2 Go maximum par fichier attaché.
- Un seul pack pour EU et USA : les noms de textures sont des empreintes du
  contenu, identiques entre les deux versions (`docs/roles/graphismes.md`). Les
  films sont reconnus par leur nom et leur fichier `.sig` : **à vérifier** pour
  la version USA (voir « Reste au builder »).
- Windows d'abord (le jeu et `update.c` utilisent WinHTTP).
- Règle GPU du PC de recompsmith pour fabriquer les films : SeedVR2 par passes de
  21 images via `claude-service\upscale-video.bat` (garde-fou : 16 Go RAM,
  13 Go VRAM, un travail GPU à la fois). Ne jamais lancer le jeu sur son PC pour
  tester : il vérifie lui-même en jeu.

## Comment (décidé)

### Le pack
- Contenu : `textures/` (les 12 942 PNG FatalPixels, actuellement dans
  `J:\DQIX-Enhanced\game\textures\hd`) et `movies/` (pour chaque film :
  `<nom>.mpg` ou `<nom>.1.mpg`, `.2.mpg`... et `<nom>.sig`, le format déjà lu
  par `runtime/hdmovie.c`).
- Films : les 6 refaits avec SeedVR2 sur le PC de recompsmith (l'opening est fait,
  les 5 autres prennent plusieurs heures en arrière-plan), puis encodés au
  format actuel (MPEG-1 1024×768, une image par image du film, signatures
  `.sig`) **(choix de Claude : garder le format actuel, le lecteur existe déjà)**.

### Hébergement **(choix de Claude)**
- Une release à part, `hd-pack-1`, indépendante des versions du jeu : le pack ne
  change pas à chaque version, on ne le réenvoie pas.
- Fichiers : le pack en zip(s) de moins de 2 Go chacun (textures et films
  séparés), plus un petit manifeste texte `hd-pack.txt` : version du pack, et
  pour chaque zip son nom, sa taille et son SHA-256.
- Le jeu connaît l'adresse du manifeste
  (`https://github.com/recompsmith/Dragon-Quest-IX-Modern/releases/download/hd-pack-1/hd-pack.txt`)
  ; une nouvelle version du pack = une nouvelle release et une nouvelle adresse
  dans une version du jeu.

### Téléchargement dans le jeu **(choix de Claude)**
- Un fil d'exécution à part, priorité basse, démarré après le premier écran,
  jamais sur le fil du rendu ni de l'émulation. Débit limité si besoin pour ne
  pas gêner le jeu en ligne (Wi-Fi intégré).
- WinHTTP comme `update.c`, avec reprise par `Range` dans un fichier
  `.part` hors de `textures\` (le jeu lit tout `textures\`).
- Vérification SHA-256, puis extraction dans un dossier temporaire, puis
  déplacement atomique vers `textures\hd-pack\` et `movies\` ; le jeu ne voit le
  pack qu'une fois complet. Les films du pack remplacent les films HD
  existants (après copie de côté).

### Priorité et ménage (décision de recompsmith)
- **Le pack FatalPixels gagne** sur les textures HD que `textures-hd` a faites
  automatiquement (`textures\hd`). Les autres packs personnels qu'un joueur a
  mis à la main dans `textures\` ne sont pas touchés **(choix de Claude :
  ils gardent leur priorité actuelle sur le pack)**.
- **Les textures Real-ESRGAN sont supprimées automatiquement** une fois le pack
  installé et vérifié : le contenu de `textures\hd` quand `textures\hd-style.txt`
  dit Real-ESRGAN (`ia-seule 1` seul, ou pas de fichier mais `textures-hd-outil`
  présent). Un pack fait avec le service (`service-...` dans la marque, le PC de
  recompsmith) n'est pas supprimé. Les dossiers mis de côté avant
  (`textures-methode5`, `textures-avant-*`) suivent la même règle
  **(choix de Claude)**.
- Ensuite `textures-hd`, s'il est là, ne refait pas les textures du pack
  (`pack_names` regarde tous les dossiers de `textures\`).

### Calendrier (décision de recompsmith)
- Le pack sort en une seule fois, quand les 6 films SeedVR2 sont faits. Le
  téléchargeur n'a donc pas besoin d'être incrémental par zip pour `hd-pack-1`.
- État dans un fichier (comme `textures-hd-progress.txt`) que la barre lit.
- Réglage `hd_pack_download = 1` dans `settings.ini` (0 = jamais), et une case
  dans le menu Graphismes.
- Une fois installé, le pack ne se retélécharge que si la version du manifeste
  change.

## Reste au builder
- Comment faire gagner `textures\hd-pack` sur `textures\hd` dans `gl_render.c`
  (fichier partagé : le signaler au coordinateur) : règle de priorité dans
  `pack_scan` plutôt que compter sur l'ordre alphabétique.
- Films : la ROM de recompsmith est la version USA (`rom\dqix_usa.nds`) ; les films
  et `.sig` sont faits sur elle. Pas de ROM EU sur ce PC : vérifier si les
  `.mods` EU ont les mêmes noms et images (sinon films HD réservés à l'USA au
  début, à dire dans la note de release).
- Comment l'extraction reste douce (lecture/écriture par petits morceaux,
  priorité basse) et comment la barre distingue téléchargement et installation.
- Linux / Steam Deck : même téléchargement (libcurl ou `curl`) ou plus tard.
- Outil de fabrication du pack (script dans `tools/`) : rassembler les
  textures, encoder les films SeedVR2, faire les zips et le manifeste.
