# Dragon Quest IX Modern

<a href="https://recomplauncher.com"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

**Dragon Quest IX: Sentinels of the Starry Skies, as a native PC game.**
*Version française plus bas : [aller au français](#francais).*

**Join us on Discord:** [discord.gg/z9X9PH8AH4](https://discord.gg/z9X9PH8AH4)
(help, new versions, feedback, screenshots).

> **Project status:** a **full decompilation** of the game is in progress. The current version of this repository is a **draft**: a working base for the **recompilation** and for the **upcoming remake** (a real PC game in Godot, on one screen, with modernised graphics). Everything done here is a test bed and a reference for that remake.

This project turns **your own** Dragon Quest IX cartridge (DS, European or USA
version) into a native Windows program, built on your PC from the ROM you
dumped from your cartridge.
**This repository contains no game data**: no code, graphics, music or ROM.
It isn't an emulator: the game's ARM code is **recompiled into C** and linked
to a runtime that rebuilds the DS hardware (2D, 3D, sound, cartridge, input)
with SDL2 and OpenGL.

Developed by **recompsmith** ([recompsmith](https://github.com/recompsmith))
and **TitanFr199**, helped by Claude.

> **Rights and use, please read.** This project is for **educational purposes only** (studying recompilation and game preservation). Dragon Quest IX and all its content belong to their rights holders (Square Enix, Level-5, Armor Project, Bird Studio, Koichi Sugiyama, Nintendo). **No ROM is provided, and none ever will be.** To make it work, you must use **a ROM dumped from your own cartridge** of Dragon Quest IX, which you legally own. Don't download ROMs, and don't share ROMs, the built game or any game files.

---

## Done so far

### European ROM and languages
- **European ROM fix**: the EU version's ARM9 code is shifted by 16 bytes
  from the USA version; addresses are now converted automatically.
- **Language menu**: English, French, German, Italian, Spanish.
- Saves move from the USA version to the EU version and back.
- **PC interface translated** into 5 languages (menu bar, windows, in-game
  options menu).
- **HD text** with one readable PC font (Source Sans 3), reworked spacing,
  mixed-up letters in dialogue fixed.

### One screen, like a real PC game
- **Single screen** in the field and in battle, in one window.
- **Minimap** at the top right (M / R3 to hide it), with **markers**: chests,
  pots, barrels, bookshelves, quest givers, chest ranks in grottoes and the
  possible ranks on treasure maps. Interior maps drawn from the game's
  collisions, map legend.
- Item and equipment cards as **pop-ups**, **shops** on one screen with a
  modern item card.
- **Character creation on one screen**, laid out for wide screens (choices in
  pop-ups, name typed on the keyboard as an option), also at Patty's for
  companions.
- **16:9 and 21:9 widescreen**: the picture follows the shape of the window or
  screen, with no black bars; menu sides blurred.
- Skill points table next to the stats.

### Windows and interface
- **Modern window style** (default) on every window: see-through dark
  background, thin grey line. Minimal and white outline styles also available.
- **Modern battles**: battle HUD over the 3D, party cards and battle menus in
  the modern style, also for a lone hero.
- **In-game options menu** (Esc or the PS / Xbox / Home button), works with a
  controller, game paused.
- **PC-style start**: the logos are skipped.
- **Modern title screen**: the opening movie plays behind "Start your
  adventure", and the adventure log menu sits small at the bottom right.
- **Button hints** with your controller's real buttons, or your keyboard key
  drawn in the game's style (AZERTY included).
- Quest card in a modern window, vocation icons in colour, confirmation before
  quitting.

### Graphics
- 3D drawn on the graphics card at up to 12x the DS resolution (Enhanced
  mode), Classic mode pixel-identical to the DS (F5 to switch).
- **AI HD textures**: every 3D texture is taken from your ROM and upscaled x4
  by AI (Real-ESRGAN) automatically at build time, with a progress bar and
  resume. Your own HD texture packs work too, with a tool to check them.
- **HD movies** (the 6 cutscenes upscaled x4).
- **HD pack** (Windows): the game downloads it by itself in the background,
  with HD textures (FatalPixels) and the 6 cutscenes remade with SeedVR2. A bar
  at the bottom left shows "HD pack: N%" then "HD pack ready"; the game doesn't
  slow down and resumes a cut download. To turn it off: Graphics > *Download
  the HD pack*. The older Real-ESRGAN textures are set aside in
  `hd-pack-ancien` (never deleted): empty that folder to free space.
- **Townspeople in 3D** (Graphics > *Townspeople in 3D*, Enhanced mode, on by default):
  townspeople drawn as real 3D characters instead of flat pictures. A few come
  from Dragon Quest IX itself; for many more, put your own Dragon Quest VII
  and/or VIII (3DS) in `rom\3DS` and run BUILD.bat again. Nothing from those
  games is included.
- xBR textures, smoothed 2D, soft shadows, thin outlines, sharper distant
  NPCs, optional character outlines.
- **HDR**: normal brightness in full screen with Windows HDR.

### Comfort
- **Journal** (J / L3): quests, party progress, vocations, alchemy (known
  recipes, missing ingredients) and items.
- **Faster battles** (x2, x3, x4 during the turn only), automatic victory
  messages.
- **Use several items at once** (European version, not in battles).
- **Autosave** every 10 minutes (option), save backups. **Safer saves**: if
  writing fails (crash, power cut, full disk), `dqix.sav` is no longer left
  empty or cut short; the previous save stays whole.
- **Cheats menu** (Action Replay data codes, all off by default): gold, mini
  medals, skill points, monster HP to 0, guaranteed rare items, etc.
- Automatic gold farming (Ctrl+G), fast forward (Tab), fully remappable
  keyboard and controller (Game > Controls), left stick to walk in every
  direction.
- Full bug report with F10.

### Other features
- Multiplayer (Tag mode and co-op up to 4) over the Internet, offline **DQVC**
  shop and downloadable quests, automatic updates.
- **What's new**: after an update, the game shows once what changed, in the
  game's language (Help > *What's new* shows it again).

### Versions, tests and organisation
- **Frequent releases**, the latest is **v3.2**, with release notes in
  [`release-notes/`](release-notes/). The game offers the update itself.
  After v47 came v2.0: fixes raise the decimal (v3.1, v3.2…), big additions
  the whole number (v4, v5…).
- More than 160 pull requests merged, with automatic tests before code
  changes are merged.
- Full recap: [docs/RECAP.md](docs/RECAP.md); task tracking:
  [docs/SUIVI.md](docs/SUIVI.md); technical map:
  [docs/TECHNICAL.md](docs/TECHNICAL.md) (all in French).

---

## Install

You need the ROM (`.nds`) of **your own** cartridge (European or USA version),
Windows 10 or 11 (64-bit), about 3 GB free, and Internet the first time.

1. Download the latest version from the
   **[releases page](https://github.com/recompsmith/Dragon-Quest-IX-Modern/releases)**
   (*Dragon-Quest-IX-Modern-…-Windows-x64.zip*, or *-Linux-x64.zip*) and unzip it.
2. Double-click **`DQIX Launcher.exe`** (`dqix-launcher` on Linux and the Steam
   Deck). The first time it asks for your ROM (**CHOOSE THE ROM**), then builds
   the game and gets the HD pack, with two bars: installing the tools (MSYS2)
   and building take 10 to 20 minutes the first time. **PLAY** lights up when
   everything is there.
3. Then open the launcher the same way: it brings the game and the HD pack up
   to date before you play, and shows the news, Discord and the options.
   On Linux and the Steam Deck the launcher builds the game and shows the news
   and options; updates and the HD pack are Windows only for now.

With shortcuts and an uninstaller instead: **`Install Dragon Quest IX.bat`**
(it builds the game, then the launcher takes over).

Or install it from **RecompLauncher** (button at the top).

Without the install window: put your ROM in the `rom` folder and double-click
**`BUILD.bat`**; the game appears in `game\dqix.exe`.

The save (`dqix.sav`) and settings (`settings.ini`) are in the `game` folder.
**Never share the `game` and `build` folders**: they contain the game.

**Linux and Steam Deck**: the game also runs on Linux and on the Steam Deck
(with RecompLauncher, or `sh BUILD.sh` in a terminal), and each release has a
Linux zip. Step by step: **[docs/LINUX-STEAMDECK.md](docs/LINUX-STEAMDECK.md)**.

**Updates** (Windows): the game offers the new version when it starts (or
Help > Check for updates). Saves and settings are kept, and the previous
version stays in `update` (`update\rollback.bat`).

## Controls

| DS | Keyboard | Controller |
|---|---|---|
| D-pad | arrows | D-pad / left stick |
| A / B / X / Y | X / Z / S / A | right / bottom / top / left buttons |
| L / R (camera) | Q / W | shoulder buttons, or right stick while walking |
| START / SELECT | Enter / Right Shift | Start / Back |
| touch screen | left click | |

Useful keys: Esc options menu, J Journal, M minimap, Tab (held) fast forward,
P pause, Ctrl+R reset, F5 Classic / Enhanced, F11 full screen, F12 screenshot,
F10 bug report, Ctrl+G gold farming. Everything can be changed in
**Game > Controls**.

## Something wrong?

- **The install fails**: RecompLauncher shows why on its last line (for
  example Python, gcc or make missing); the full log is in `build/build.log`.
  Installing and updating are more reliable since v3.1 and v3.2 (thanks
  TitanFr199): the build tools are fully updated first, a lock left by a
  stopped build is removed, and the tools are checked before building. If you
  are stuck on an error, run the install again from the latest version.
- **In the game**: press **F10**, a report is saved in `game\reports\`.

Ask your questions on [Discord](https://discord.gg/z9X9PH8AH4).

---

<a id="francais"></a>

## Français

**Dragon Quest IX : Les Sentinelles du firmament, en vrai jeu PC.**

**Rejoignez la communauté sur Discord :** [discord.gg/z9X9PH8AH4](https://discord.gg/z9X9PH8AH4)
(aide, nouvelles versions, retours, captures).

> **Où en est le projet ?** Une **décompilation complète** du jeu est en cours. La version actuelle de ce dépôt est une **ébauche** : elle sert de base de travail pour la **recompilation** du jeu et pour le **futur remake** (un vrai jeu PC sous Godot, sur un seul écran, avec des graphismes modernisés). Tout ce qui est fait ici sert de banc d'essai et de référence pour ce remake.

Ce projet transforme **votre propre** cartouche de Dragon Quest IX (DS, version
européenne ou américaine) en un programme Windows natif, compilé sur votre PC à
partir de la ROM que vous avez extraite de votre cartouche.
**Ce dépôt ne contient aucune donnée du jeu** : ni code, ni graphismes, ni musique, ni ROM.
Ce n'est pas un émulateur : le code ARM du jeu est **recompilé en C**, puis lié
à un runtime qui recrée le matériel de la DS (2D, 3D, son, cartouche, entrées)
avec SDL2 et OpenGL.

Projet développé par **recompsmith** ([recompsmith](https://github.com/recompsmith))
et **TitanFr199**, avec l'aide de Claude.

> **Droits et usage — à lire.** Ce projet est réalisé **à des fins éducatives uniquement** (étude de la recompilation et de la préservation des jeux). Dragon Quest IX et tout son contenu appartiennent à leurs ayants droit (Square Enix, Level-5, Armor Project, Bird Studio, Koichi Sugiyama, Nintendo). **Aucune ROM n'est fournie et aucune ne le sera.** Pour que le projet fonctionne, vous devez utiliser **une ROM extraite de votre propre cartouche** de Dragon Quest IX, que vous possédez légalement. Ne téléchargez pas de ROM et ne partagez ni ROM, ni jeu compilé, ni fichiers du jeu.

### Travaux réalisés jusqu'à présent

#### ROM européenne et langues
- **Correctif de la ROM européenne** : le code ARM9 de la version EU est décalé
  de 16 octets par rapport à la version USA ; les adresses sont maintenant
  converties automatiquement.
- **Menu Langue** : anglais, français, allemand, italien, espagnol.
- Les sauvegardes passent de la version USA à la version EU et inversement.
- **Interface PC traduite** en 5 langues (barre de menus, fenêtres, menu
  d'options dans le jeu).
- **Texte HD** avec une seule police PC lisible (Source Sans 3), espacement
  revu, lettres mélangées dans les dialogues corrigées.

#### Un seul écran, comme un vrai jeu PC
- **Écran unique** en exploration et en combat, dans une seule fenêtre.
- **Mini-carte** en haut à droite (touche M / R3 pour la cacher), avec
  **marqueurs** : coffres, pots, tonneaux, étagères, donneurs de quêtes, rang
  des coffres dans les grottes et rangs possibles sur les cartes au trésor.
  Plans des intérieurs tirés des collisions du jeu, légende de la carte.
- Fiches d'objets et d'équipement en **pop-up**, **magasins** sur un seul
  écran avec fiche d'objet moderne.
- **Création du personnage sur un seul écran**, pensée pour les écrans larges
  (choix en pop-up, nom tapé au clavier en option), aussi chez Tulipe pour les
  compagnons.
- **Écran large 16:9 et 21:9** : l'image suit la forme de la fenêtre ou de
  l'écran, sans bandes noires ; côtés des menus floutés.
- Tableau des points de compétence à côté des caractéristiques.

#### Fenêtres et interface
- **Style de fenêtres Moderne** (par défaut) sur toutes les fenêtres : fond
  sombre transparent, fin trait gris. Styles Minimal et contour blanc aussi
  disponibles.
- **Combat moderne** : HUD de combat par-dessus la 3D, cartes de l'équipe et
  menus de combat au style moderne, aussi pour un héros seul.
- **Menu d'options dans le jeu** (Échap ou bouton PS / Xbox / Home), jouable à
  la manette, jeu en pause.
- **Démarrage façon PC** : les logos sont passés.
- **Écran titre moderne** : le film d'ouverture joue derrière « Commencer
  l'aventure », et le menu du journal d'aventure se place en petit en bas à
  droite.
- **Indications de boutons** avec les vrais boutons de votre manette, ou votre
  touche clavier dessinée dans le style du jeu (AZERTY compris).
- Fiche de quête en fenêtre moderne, icônes de vocation en couleur,
  confirmation avant de quitter.

#### Graphismes
- Rendu 3D sur la carte graphique jusqu'à 12x la résolution de la DS (mode
  Enhanced), mode Classic identique à la DS au pixel près (F5 pour passer de
  l'un à l'autre).
- **Textures HD par IA** : toutes les textures 3D sont extraites de votre ROM
  et agrandies x4 par IA (Real-ESRGAN) automatiquement à la compilation, avec
  barre de progression et reprise. Packs de textures HD personnels possibles,
  et un outil pour les vérifier.
- **Films HD** (les 6 cinématiques agrandies x4).
- **Pack HD** (Windows) : le jeu le télécharge tout seul en arrière-plan, avec
  des textures HD (FatalPixels) et les 6 cinématiques refaites avec SeedVR2.
  Une barre en bas à gauche affiche « Pack HD : N% » puis « Pack HD prêt » ; le
  jeu ne ralentit pas et reprend un téléchargement coupé. Pour le désactiver :
  Graphismes > *Télécharger le pack HD*. Les anciennes textures Real-ESRGAN
  sont mises de côté dans `hd-pack-ancien` (jamais supprimées) : videz ce
  dossier pour gagner de la place.
- **Habitants en 3D** (Graphismes > *Habitants en 3D*, mode Enhanced, activé par défaut) :
  les habitants deviennent de vrais personnages en 3D au lieu d'images plates.
  Quelques-uns viennent de Dragon Quest IX lui-même ; pour beaucoup d'autres,
  mettez votre propre Dragon Quest VII et/ou VIII (3DS) dans `rom\3DS` et
  relancez BUILD.bat. Rien de ces jeux n'est fourni.
- Textures xBR, 2D lissée, ombres douces, fins traits de contour, PNJ lointains
  plus nets, contours des personnages au choix.
- **HDR** : luminosité normale en plein écran avec le HDR de Windows.

#### Confort
- **Journal** (touche J / L3) : quêtes, progression de l'équipe, vocations,
  alchimie (recettes connues, ingrédients manquants) et objets.
- **Combats plus rapides** (x2, x3, x4 pendant le tour seulement), messages de
  victoire automatiques.
- **Utiliser plusieurs objets d'un coup** (version européenne, pas en combat).
- **Sauvegarde automatique** toutes les 10 minutes (option), copies de
  sauvegarde. **Sauvegardes plus sûres** : si l'écriture échoue (plantage,
  coupure de courant, disque plein), `dqix.sav` ne reste plus vide ou tronqué ;
  la sauvegarde précédente reste entière.
- **Menu Cheats** (codes Action Replay de données, tous désactivés par
  défaut) : or, mini-médailles, points de compétence, PV des monstres à 0,
  objets rares garantis, etc.
- Farm d'or automatique (Ctrl+G), avance rapide (Tab), clavier et manette
  entièrement reconfigurables (Jeu > Commandes), stick gauche pour marcher dans
  toutes les directions.
- Rapport de bug complet avec F10.

#### Autres fonctions
- Multijoueur (mode Tag et coopération jusqu'à 4) par Internet, boutique
  **DQVC** et quêtes téléchargeables hors ligne, mises à jour automatiques.
- **Nouveautés** : après une mise à jour, le jeu montre une fois ce qui a
  changé, dans la langue du jeu (Aide > *Nouveautés* le montre de nouveau).

#### Versions, tests et organisation
- **Des versions fréquentes**, la dernière est la **v3.2**, avec notes de
  version dans [`release-notes/`](release-notes/). Le jeu propose lui-même la
  mise à jour. Après la v47 est venue la v2.0 : les corrections changent la
  décimale (v3.1, v3.2…), les gros ajouts le nombre entier (v4, v5…).
- Plus de 160 pull requests fusionnées, avec des tests automatiques avant
  la fusion de tout changement de code.
- Récapitulatif complet : [docs/RECAP.md](docs/RECAP.md) ; suivi des tâches :
  [docs/SUIVI.md](docs/SUIVI.md) ; carte technique :
  [docs/TECHNICAL.md](docs/TECHNICAL.md).

### Installer

Il vous faut la ROM (`.nds`) de **votre propre** cartouche (version européenne
ou américaine), Windows 10 ou 11 (64 bits), environ 3 Go libres, et Internet la
première fois.

1. Téléchargez la dernière version sur la
   **[page des versions](https://github.com/recompsmith/Dragon-Quest-IX-Modern/releases)**
   (*Dragon-Quest-IX-Modern-…-Windows-x64.zip*, ou *-Linux-x64.zip*) et décompressez-la.
2. Double-cliquez sur **`DQIX Launcher.exe`** (`dqix-launcher` sous Linux et sur
   Steam Deck). La première fois, il demande votre ROM (**CHOISIR LA ROM**), puis
   compile le jeu et télécharge le pack HD, avec deux barres : l'installation des
   outils (MSYS2) et la compilation prennent 10 à 20 minutes la première fois.
   **JOUER** s'allume quand tout est là.
3. Ensuite, ouvrez le launcher de la même façon : il met le jeu et le pack HD à
   jour avant de jouer, et montre les nouveautés, Discord et les options.
   Sous Linux et sur Steam Deck, le launcher compile le jeu et montre les
   nouveautés et les options ; les mises à jour et le pack HD sont pour Windows
   seulement pour l'instant.

Avec raccourcis et désinstallation : **`Install Dragon Quest IX.bat`** (il
compile le jeu, puis le launcher prend le relais).

Ou installez-le depuis **RecompLauncher** (bouton en haut de la page).

Sans fenêtre d'installation : mettez votre ROM dans le dossier `rom` et
double-cliquez sur **`BUILD.bat`** ; le jeu apparaît dans `game\dqix.exe`.

La sauvegarde (`dqix.sav`) et les réglages (`settings.ini`) sont dans le
dossier `game`. **Ne partagez jamais les dossiers `game` et `build`** : ils
contiennent le jeu.

**Linux et Steam Deck** : le jeu tourne aussi sous Linux et sur Steam Deck
(avec RecompLauncher, ou `sh BUILD.sh` dans un terminal), et chaque version a un
zip Linux. Le guide pas à pas est dans
**[docs/LINUX-STEAMDECK.md](docs/LINUX-STEAMDECK.md)**.

**Mises à jour** (Windows) : le jeu propose la nouvelle version au démarrage (ou
Aide > Rechercher des mises à jour). Sauvegardes et réglages sont conservés,
et la version précédente reste dans `update` (`update\rollback.bat`).

### Commandes

| DS | Clavier | Manette |
|---|---|---|
| Croix | flèches | croix / stick gauche |
| A / B / X / Y | X / Z / S / A | boutons droite / bas / haut / gauche |
| L / R (caméra) | Q / W | gâchettes hautes, ou stick droit en marchant |
| START / SELECT | Entrée / Maj droite | Start / Back |
| écran tactile | clic gauche | |

Touches utiles : Échap menu d'options, J Journal, M mini-carte, Tab (maintenu)
avance rapide, P pause, Ctrl+R réinitialiser, F5 Classic / Enhanced,
F11 plein écran, F12 capture, F10 rapport de bug, Ctrl+G farm d'or. Tout se
change dans **Jeu > Commandes**.

### Un problème ?

- **L'installation échoue** : RecompLauncher affiche pourquoi sur sa dernière
  ligne (par exemple Python, gcc ou make absent) ; le journal complet est dans
  `build/build.log`. L'installation et la mise à jour sont plus fiables depuis
  les v3.1 et v3.2 (merci TitanFr199) : les outils de compilation sont d'abord
  mis à jour en entier, un verrou laissé par une compilation interrompue est
  retiré, et les outils sont vérifiés avant de compiler. Si vous restez bloqué
  sur une erreur, relancez l'installation depuis la dernière version.
- **Dans le jeu** : appuyez sur **F10**, un rapport est enregistré dans
  `game\reports\`.

Posez vos questions sur le [Discord](https://discord.gg/z9X9PH8AH4).

---

## Crédits / Credits

- Projet / project : **recompsmith** ([recompsmith](https://github.com/recompsmith))
  et / and **TitanFr199** ([TitanFr199](https://github.com/TitanFr199)), avec
  l'aide de / helped by Claude.
- Noms et limites des fonctions : projet de décompilation de Dragon Quest IX
  (domaine public). Puce Wi-Fi, rendu 3D et son : recherches de l'émulateur
  melonDS. / Function names and boundaries from the public-domain DQIX
  decompilation project; wireless chip, 3D renderer and sound follow melonDS's
  research.
- Pack HD / HD pack : textures agrandies avec **FatalPixels** (twittman,
  CC-BY-NC-SA-4.0) et **UltraSharp** (Kim2091), cinématiques refaites avec
  **SeedVR2** (ByteDance). Usage non commercial uniquement. / Textures upscaled
  with FatalPixels (twittman, CC-BY-NC-SA-4.0) and UltraSharp (Kim2091), movies
  remade with SeedVR2 (ByteDance). Non-commercial use only.

Dragon Quest IX est © Square Enix / Level-5 / Armor Project / Bird Studio /
Koichi Sugiyama. Ce projet n'est ni affilié à Square Enix ou Nintendo, ni
approuvé par eux. Il ne contient aucun fichier du jeu et ne fonctionne qu'avec
une ROM extraite de votre propre cartouche. / This project isn't affiliated
with or endorsed by Square Enix or Nintendo. It contains no game files and only
works with a ROM dumped from your own cartridge.

Licence / License : GPL-3.0-or-later (voir / see `LICENSE`). Inclut des données
du domaine public du projet de décompilation de DQIX (`config/`), libslirp
(BSD, `third_party/libslirp`), stb_truetype (domaine public / public domain) et
des parties traduites de melonDS (GPL-3.0-or-later).
