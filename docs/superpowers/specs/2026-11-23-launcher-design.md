# Launcher DQIX Modern : description

Date : 2026-11-23. Branche `dev/launcher`. Maquette validée par Romuald : v8 (description validée le 2026-11-23)
(gardée hors du dépôt, captures à joindre à la PR).

## Pourquoi

Depuis v3, le jeu télécharge le pack HD en arrière-plan pendant qu'on joue :
un pack à moitié installé donne un jeu laid, et les joueurs le signalent. Romuald
veut un launcher comme celui des autres jeux : tout se met à jour **avant** de
jouer. Dr.Stone (équipe) a dit ce qui compte : « simple et surtout utile » ;
Discord, mises à jour récentes listées, mises à jour faites en amont, accès aux
réglages avant de lancer.

Réussite : un joueur ouvre le launcher, voit ce qui se passe (deux barres qui
disent vrai), peut régler le jeu en attendant, et ne peut lancer le jeu que
quand tout est installé.

## Objectifs

1. **Un écran unique** (maquette v6), à la sauce DQIX :
   ciel étoilé bleu nuit, cadres bleus à bord blanc comme les fenêtres du jeu,
   or pour les titres et JOUER, police Marcellus (`fonts/`). Disposition
   inspirée de la capture de référence (autre jeu), sans rien en reprendre :
   ni nom, ni logo, ni image.
   - En haut à gauche : « DRAGON QUEST IX / MODERN » en texte.
   - En haut à droite : langue (drapeau + nom), **Discord**
     (`https://discord.gg/z9X9PH8AH4`, `DISCORD_INVITE` de `tools/release_lib.py`),
     GitHub (page du dépôt), dossier du jeu, réduire, fermer.
   - À gauche : carte **Nouveautés**, 3 dernières versions (date, version, titre,
     une ligne) ; « tout voir » ouvre la page des releases.
   - Fond : l'image HD de l'écran titre (`title-back`) quand le pack HD est là ;
     sinon le ciel étoilé seul.
   - En bas : VÉRIFIER, OPTIONS ; **deux barres séparées** ; bouton JOUER.
2. **Deux barres**, chacune avec son mot **à gauche de la barre**
   (TÉLÉCHARGEMENT, INSTALLATION) et le détail de l'étape **au-dessus de la
   barre** (texte coupé par « … » s'il est trop long ; colonne du mot assez
   large pour l'allemand).
   - Téléchargement : sources de la mise à jour, zips du pack HD ; Mo faits / total,
     vitesse, temps restant.
   - Installation : compilation du jeu depuis la ROM (étapes de `step()` de
     `tools/build-game.sh`), mise en place du pack HD (fichiers posés / total).
   - Les deux peuvent avancer en même temps quand l'outil le permet.
   - Fin : « Tout est à jour, prêt à jouer ! ».
3. **JOUER** est éteint tant que la mise à jour du jeu et le pack HD ne sont pas
   complets. En cas d'échec : la barre concernée passe en rouge avec la raison et
   RÉESSAYER ; JOUER reste éteint.
4. **Mises à jour faites par le launcher** : la mise à jour du jeu (même source
   que l'updater du jeu : branche `stable`, `version.h`) puis la recompilation
   depuis la ROM, et le pack HD (`hd-pack.exe`, au premier plan, attendu).
5. **Options** (panneau qui remplace les Nouveautés ; la préparation continue
   en dessous) :
   - deux grands boutons **Classique** (comme à l'époque sur DS : deux écrans,
     image d'origine) et **Moderne** (écran unique, 3D nette, pack HD, menus
     modernes). **Aucune case** pour l'aspect du jeu : le mode règle tout d'un coup ;
   - en dessous seulement : plein écran / fenêtre, volume, touches de la manette
     et du clavier ;
   - « Rétablir par défaut ».
   Écrit dans `game/settings.ini`, pris en compte au lancement. Les menus du jeu
   gardent tous leurs réglages détaillés.
6. **Une seule langue** pour le launcher et le jeu (en, fr, de, it, es). Tout le
   launcher est traduit, Nouveautés comprises (`release-notes/<lang>/`, anglais
   par défaut). Choisir une langue écrit `language` et `menu_language` dans
   `settings.ini`. Limite : avec une ROM USA, les dialogues du jeu restent en
   anglais (la cartouche n'a que l'anglais) ; nos menus suivent la langue.
7. **Tout le monde passe par le launcher** :
   - raccourcis (bureau, menu Démarrer) vers le launcher ; les raccourcis déjà
     faits par `tools/setup-wizard.ps1` vers `game\dqix.exe` sont repointés ;
   - `dqix.exe` lancé directement (ancien raccourci, `apply.bat` d'une ancienne
     version, RecompLauncher) ouvre le launcher et se ferme ; le launcher lance
     ensuite `dqix` avec `DQIX_LAUNCHER=1` dans l'environnement ;
   - RecompLauncher : il fait toujours l'installation (`BUILD.bat --prepare`) ;
     `recomplauncher.json` `start` pointe vers le launcher. Les installs
     RecompLauncher existantes basculent d'elles-mêmes à la mise à jour suivante.
8. **HD de l'écran titre dans le pack HD** : `DÉCIDÉ (Romuald, 2026-11-23)` : on
   les distribue ; s'il faut les retirer un jour, on les retire.

## Non-objectifs

- ~~Pas d'installation de zéro dans le launcher~~ : remplacé le 2026-11-24, voir
  « Premier lancement » ci-dessous. RecompLauncher garde son propre build ;
  `Install Dragon Quest IX.bat` reste en secours (raccourcis, désinstallation).
- Pas le mode « deux écrans façon DS » lui-même : le bouton Classique écrit
  `mode = classic` (image DS exacte, existe déjà). La disposition à deux écrans
  viendra plus tard sans changer l'écran des options.
- Pas de compte, pas de télémétrie, pas d'icône YouTube.
- Pas de nouveau réglage dans les menus du jeu.

## Anti-objectifs (échec même si ça « marche »)

- Laisser jouer avec un pack HD incomplet, ou une barre qui ment.
- Casser RecompLauncher, l'installeur, le build Linux, ou un joueur qui n'a
  jamais ouvert le launcher.
- Perdre une sauvegarde ou `settings.ini` (un réglage du joueur écrasé hors de
  ce qu'il a touché dans le launcher).
- Embarquer une donnée du jeu (ROM, image extraite) dans le dépôt.
- Une fenêtre noire de console pendant la préparation.

## Contraintes

- Le dépôt ne contient aucune donnée du jeu. Le launcher vit dans `launcher/`
  (C), compilé par `tools/build-game.sh` comme `hd-pack.exe`, sortie
  `game/dqix-launcher.exe` (Windows) / `game/dqix-launcher` (Linux).
- Tests : rien ne tourne sur le PC de Romuald ; tests cloud (`docs/TESTS-CLOUD.md`).
- Fichiers partagés touchés (à annoncer au coordinateur) : `tools/build-game.sh`,
  `runtime/frontend_sdl.c` (redirection vers le launcher, `DQIX_LAUNCHER=1`),
  sans doute `build-windows.sh`.
- `tools/hd-pack.c` est aussi changé par la PR 167 (`graph/hd-pack-movies-only`) :
  se coordonner avec sa session, partir de `main` après sa fusion si possible.
- Sécurité : le launcher télécharge et lance des mises à jour (revue
  `agent-skills:security-and-hardening`). Téléchargements en HTTPS seulement
  (github.com, codeload.github.com, api.github.com, objects.githubusercontent.com),
  tailles bornées, rien d'exécuté depuis un chemin fourni par le réseau.

## Choix techniques (décidés par Claude, Romuald m'a confié la technique)

- **C + SDL2 + OpenGL**, le même outillage que le jeu : marche sous Windows,
  Linux, Steam Deck ; léger ; reprend les polices et `runtime/stb_*.h`.
  Écarté : Tauri/WebView (Rust en plus, composant système pas toujours présent
  sur Steam Deck, ~10 Mo), Win32 natif (Windows seul, ce look impossible),
  launcher dans `dqix.exe` (n'existe pas avant le build, ne peut pas se
  remplacer en marche).
- **Compilé par `build-game.sh`** et non livré tout fait dans le zip :
  l'updater télécharge le zip des sources de `stable`, qui ne contient pas de
  binaire. Conséquence (à dire à Romuald : je lui avais dit « livré tout fait ») :
  le launcher existe à partir de la fin de la première installation, ce qui
  colle à son déroulé (installation, puis launcher). Mise à jour du launcher
  par lui-même : sous Windows un exe en marche peut être renommé ;
  `build-game.sh` renomme l'ancien en `.old` avant d'écrire le nouveau, et le
  launcher se relance à la fin.
- **Mise à jour du jeu dans le launcher** : reprendre `runtime/update.c`
  (lecture de `version.h` sur `stable`, téléchargement du zip) et les étapes de
  `APPLY_BAT` (copie de sauvegarde dans `update\previous`, dépaquetage, copie,
  `rollback.bat`), mais exécutées par le launcher, puis la compilation lancée
  avec `DQIX_PROGRESS=1` et lue ligne à ligne (`progress <fraction> <titre>`,
  `BUILD FAILED (...)`, `build/build.log`).
- **`hd-pack.exe`** : lancé au premier plan (priorité normale, fenêtre cachée),
  attendu. À ajouter :
  - **code de sortie** : 0 = installé ou déjà à jour, ≠ 0 = échec ;
  - **état « failed »** avec la raison, au lieu d'effacer le fichier ;
  - progression **séparée** téléchargement / installation (octets faits / total,
    fichiers posés / total), sans casser la lecture actuelle par le jeu
    (`working N 100`, `done 0 100`) : une deuxième ligne, que l'ancien lecteur ignore.
- **Nouveautés** : lues sur GitHub (API releases : titre, date, corps en
  anglais ; la traduction vient de `release-notes/<lang>/<tag>.md` du dossier
  installé, à jour après chaque mise à jour). Hors ligne : les notes locales
  seules.
- **Linux et Steam Deck** : mêmes écran, Nouveautés, options, langue, JOUER.
  Aujourd'hui `hd-pack.exe` et l'updater sont Windows seulement (WinHTTP).
  Choix pour la v1 du launcher : sous Linux, les mises à jour et le pack HD
  restent comme aujourd'hui (pas de pack HD, mise à jour en retéléchargeant le
  zip), et la ligne d'état le dit ; JOUER n'attend donc que ce qui existe sur
  cette plateforme. Porter le pack HD sous Linux = tâche suivante.

## Réglages décidés par Claude après la relecture

- **Choix déjà faits par le joueur** : `hd_pack_download = 0` → JOUER n'attend
  pas le pack et le launcher ne le télécharge pas ; mode Classique → JOUER
  n'attend pas le pack (il ne sert qu'en Moderne) ; `update_check = 0` → le
  launcher ne met pas le jeu à jour tout seul (il affiche la version disponible).
- **Mise à jour automatique**, sans question : c'est la demande (« les majs se
  font en amont »). Le joueur sur une version nightly reste sur `main` ; les
  autres suivent `stable`. Jamais de retour à une version plus ancienne.
- **Boutons de mode** : Classique écrit seulement `mode = classic`, Moderne
  `mode = enhanced`. Le jeu ignore déjà les réglages « enhanced » en Classique,
  donc rien d'autre n'est écrasé, et revenir en Moderne retrouve les réglages du
  joueur.
- **Touches** : la v1 ouvre la configuration existante (remappage de la manette
  du jeu, `controls.ini`) ; un écran de remappage dans le launcher est une
  tâche suivante.
- **Pendant la partie** : le launcher se ferme en lançant le jeu.
  `dqix` avec `DQIX_LAUNCHER=1` dans l'environnement ne cherche pas de mise à jour et ne lance pas
  `hd-pack.exe` (pas deux outils sur les mêmes fichiers).
- **Launcher absent** (build optionnel raté, vieille install) : `dqix` démarre
  normalement, comme aujourd'hui. La redirection vers le launcher vaut aussi sous
  Linux quand il existe.

- **Hors ligne** (ou GitHub en panne) : `DÉCIDÉ (Romuald, 2026-11-23)` : si le
  jeu est complet, JOUER s'allume, avec « Hors ligne : mises à jour non
  vérifiées ». Un pack HD incomplet ne s'installe pas (tout ou rien) : le jeu
  part alors sans le pack HD.

## Laissé à l'auteur du plan

Structure du code de `launcher/`, découpage des tests, rendu exact (tailles,
ombres), traduction des textes (rôle Langues), noms des clés internes.

## Image de fond

Romuald a fourni l'image (2026-11-23) : illustration 1376×768 (ange
célestellien, Observatoire, fygg, château), gardée dans
`J:\DQIX-Backups\launcher-art\launcher-back.jpg`, jamais dans le dépôt.
Distribuée dans le pack HD (`DÉCIDÉ (Romuald, 2026-11-23)`), affichée en
« cover » avec un dégradé sombre à gauche sous les Nouveautés (maquette v7).
Sans pack HD : ciel étoilé. Elle entre dans le pack en même temps que la
prochaine version du pack (coordination avec la PR 167, `hd-pack-2`).

## Premier lancement (`DÉCIDÉ (Romuald, 2026-11-24)`)

Romuald : « il devra demander où est la ROM pour pouvoir bosser avec au premier
lancement ». Ses trois oui :
1. **Un launcher tout fait dans le zip de la release**, compilé par GitHub
   Actions : `DQIX Launcher.exe` (+ `SDL2.dll`) à la racine du zip Windows,
   `dqix-launcher` à la racine du zip Linux. C'est lui qu'on ouvre en premier.
2. **Au premier lancement** (jeu pas construit, pas de `game/data/dqix.nds`), la
   ligne d'état demande la ROM et le bouton JOUER devient « CHOISIR LA ROM »
   (fenêtre de choix de fichier : Windows, `zenity`/`kdialog` sous Linux et
   Steam Deck). La ROM (`.nds`, ou `.zip` qui la contient) est copiée dans `rom/`
   puis construite (`BUILD.bat --prepare rom\<nom>` / `BUILD.sh`), barres comprises,
   puis le pack HD. Il remplace l'installeur pour la première fois.
3. **Ensuite, il passe la main** à `game/dqix-launcher`, recompilé à chaque mise
   à jour : le launcher de la racine ne se met pas à jour lui-même, celui de
   `game/` si. Aucun binaire dans le dépôt.

Réglages (Claude) : le launcher de la racine travaille dans `game/` (créé au
besoin) comme l'autre ; ses polices viennent de `fonts/` du projet tant que
`game/fonts/` n'existe pas ; le chemin de la ROM du joueur ne passe jamais par
`cmd.exe` (copie par l'API Windows, puis chemin relatif). Pas de raccourcis ni
de désinstallation dans le launcher (l'installeur garde ce rôle).

## Téléchargement qui échoue (`DÉCIDÉ (Romuald, 2026-11-24)`)

Un téléchargement (mise à jour ou pack HD) qui casse en cours de route : la barre
reste rouge avec RÉESSAYER ; en mode **Classique** JOUER s'allume (le jeu est
complet), en mode **Moderne** il reste éteint. Hors ligne dès la vérification :
inchangé (jeu complet → JOUER allumé).

Note (Graphismes, 2026-11-24) : l'image voyage dans le zip des films
(`movies/launcher-back.jpg`, `make_hd_pack.py --extra`), pas dans un zip de
textures, qui ferait retélécharger toutes les textures avec la PR 167. Elle
arrivera avec un `hd-pack-3` ; sans elle, le launcher montre le ciel étoilé.
