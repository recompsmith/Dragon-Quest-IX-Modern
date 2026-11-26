# Rôle : Graphismes et interface

Threads `[Graph] …`, branches `graph/<sujet>`.

## Périmètre
Textures HD (dump, upscale, remplacement), cinématiques HD, skins de la DS
Lite, restyle visuel des fenêtres et des boîtes de dialogue, maquettes.

## Possède (seul à écrire)
- `tools/textures-hd.c`, `tools/textures-rom.c`, `tools/upscale_movies.py`, `tools/split_movies.py`,
  `tools/movie_frames.py`, `runtime/hdmovie.c` / `.h`, `skins/`.
- Hors git, dans le dossier de jeu : `game\textures`, `game\movies`,
  `game\textures-hd-outil`. Ils ne sont jamais versionnés ; sauvegarde dans
  `J:\DQIX-Backups\` avant de les remplacer, et seulement avec l'accord de recompsmith.

## Partagé
Voir la liste des fichiers partagés dans `CLAUDE.md` (surtout `gl_render.c`,
`gpu3d.c`, `menu_win32.c`). Le restyle des dialogues touche aussi `runtime/hdtext.c` (rôle Langues) : à faire
l'un après l'autre, pas en parallèle.

## À savoir
- Textures HD : service d'images local (skill `comfyui-images`,
  `J:\Outil\IA\ComfyUI_windows_portable\claude-service\upscale.bat`, autre
  chemin : `TEXHD_SERVICE`), mode `auto` = FatalPixels avec UltraSharp fondu
  dans les zones lisses (`TEXHD_MODEL` pour un autre mode). Choix de recompsmith du
  2026-11-14 : Real-ESRGAN anime (plastique, contours noirs) est abandonné, la
  méthode 5 aussi (mélange à 50 % avec un agrandissement Lanczos, toujours
  possible avec `TEXHD_SHARP=50`). Sans le service, `textures-hd` reprend
  Real-ESRGAN dans `game\textures-hd-outil` (autres PC).
  Seules les textures 3D (`textures\dump`) passent par l'IA : jamais le texte ni
  l'interface (l'IA déforme les lettres). `dump-2d` n'a pas de pack : le jeu
  lisse la 2D lui-même (Smooth 2D, xBR) ; si un jour il en faut un, `--model scale4x`.
  Pas de version HD pour les textures vides, unies ou de 16 × 16 et moins.
  `textures\hd-style.txt` dit avec quoi le pack est fait (`service-auto 1`,
  `ia-seule 1` = Real-ESRGAN ; `ia-seule 1 service-auto 1` = pack du service que
  l'ancien outil laisse aussi tranquille). S'il change, au lancement suivant de
  `textures-hd` les textures HD qui ont leur original dans `textures\dump` sont
  déplacées dans `game\textures-avant-<ancien style>\` (sans fichier :
  `textures-methode5\`) puis refaites.
  Dossier de travail : `game\textures-hd-tmp\`, hors de `textures\` (le jeu lit
  tous les dossiers de `textures\` : l'ancien `textures\hd-auto-tmp` donnait des
  carrés noirs et des ombres blanches pendant la conversion ; le jeu l'ignore).
- `textures-hd.exe` se compile depuis `tools/textures-hd.c` ; BUILD.bat le
  (re)construit seulement si `game\textures-hd-outil` existe (voir plus bas).
- `textures-rom.exe` (pareil : `gcc -O2 -I runtime -o game/textures-rom.exe tools/textures-rom.c`)
  sort toutes les textures 3D de la ROM du joueur dans `textures\dump` sans jouer
  (12 670, mêmes noms EU et USA), puis lance `textures-hd.exe`. Les visages et
  cheveux du héros, les lettres et les morceaux des PNJ lointains n'y sont pas
  tels quels : ils restent au dump en jouant.
- À chaque build (donc à chaque mise à jour), si `game\textures-hd-outil` existe,
  `tools/build-game.sh` recompile ces deux outils puis lance `textures-rom.exe --auto` :
  extraction une seule fois par ROM (`textures\dump\rom-textures.txt`), puis
  `textures-hd.exe` en arrière-plan. `textures-hd` saute les textures déjà présentes
  dans n'importe quel dossier du pack (`hd\`, `rom\`...) et ne tourne qu'une fois
  à la fois (`textures\hd-auto.lock`).
- Vérifier le rendu en jeu (Graphics > Enhanced) et joindre une capture au compte rendu.
