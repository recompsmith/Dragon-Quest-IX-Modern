# DQIX Enhanced (fork recompsmith/Dragon-Quest-IX-Modern)

Dragon Quest IX (DS) recompilé en C et exécuté nativement sur PC. Code :
`recomp/` (recompilateur Python), `runtime/` (C : CPU, GPU 2D/3D, son, Wi-Fi,
interface SDL/OpenGL, menus Windows), `tools/` (build, extraction ROM, textures
et films HD), `tests/`. Carte détaillée : `docs/TECHNICAL.md`. État : `docs/STATE.md`.
Tâches en cours et décisions : **`docs/SUIVI.md`** (à lire en début de session).

Réponds en français. Messages de commit, noms de branches et PR en anglais.

## Où vit quoi
- Dépôt GitHub `recompsmith/Dragon-Quest-IX-Modern`, branche `main` = seule source du code.
- `J:\DQIX-Enhanced-repo` = clone de dev, reste sur `main` (rôle Intégration).
- `J:\DQIX-wt\<role>-<sujet>` = un worktree par thread de dev (voir plus bas).
- `J:\DQIX-Enhanced` = dossier où recompsmith joue (checkout de `main`).
  **Ne jamais l'éditer** : il se met à jour par la mise à jour intégrée du jeu ;
  sinon seulement `git pull --ff-only` puis BUILD.bat, par le rôle Intégration,
  avec l'accord de recompsmith. Sauvegardes : `J:\DQIX-Backups\`.

## Compiler (vérifié sur ce PC)
```
cmd /c "set DQIX_NOPAUSE=1&& <dossier>\BUILD.bat"
```
Code de sortie 0 = OK. Le résultat va dans `<dossier>\game\`. Le premier build
d'un dossier recompile la ROM (plusieurs minutes). La ROM est le premier `.nds`
de `rom\`, sinon le chemin de `rom\last-rom.txt` (ignoré par git : à copier dans
chaque nouveau worktree). Sans ROM trouvable, BUILD.bat ouvre une boîte de
dialogue et reste bloqué : vérifier `rom\` avant un build en arrière-plan.
En PowerShell, lancer BUILD.bat en arrière-plan.
Ce build ne sert qu'à mettre à jour le dossier de jeu, avec l'accord de
recompsmith (`dqix-build-check`) : les tests, eux, sont cloud (section suivante).

## Tester
- **Rien ne se lance sur le PC de recompsmith** (ni le jeu, ni BUILD.bat pour un
  test) : build, jeu sous xvfb, captures et `tests/semantics` tournent dans une
  session cloud à partir du dépôt privé `recompsmith/dqix-test-prive`.
  Recette et format du rapport : **`docs/TESTS-CLOUD.md`**. Sous-agent : `dqix-testeur`.
- Le rapport cloud est la branche `tests/<sujet>` du dépôt privé ; ses deux
  premières lignes : `commit: <sha40>` puis `result: PASS|FAIL`. Il ne vaut que
  pour ce commit exact.
- `tests/semantics/run.py` et `tests/verify_decode.py` demandent clang (cible
  ARM) / LLVM : ils tournent sous Linux, donc en session cloud.
- `tests/gpu3d/t3d.c` se compile avec gcc contre `runtime/gpu3d.c` (écrit
  `t3d.ppm`) ; il recompile depuis la PR #45.
- Ne jamais annoncer un test comme exécuté s'il ne l'a pas été.

## Règles
- Une branche par fonctionnalité, nommée `<role>/<sujet>` : `graph/`, `langues/`,
  `dev/`, `integ/`, `orga/`. Travailler dans son worktree :
  `git -C J:\DQIX-Enhanced-repo worktree add J:\DQIX-wt\<role>-<sujet> -b <role>/<sujet> origin/main`
  (`orga/` = organisation du projet, tenue par le rôle Intégration.)
- Ne jamais commiter : `*.nds`, `build/`, `game/`, sauvegardes, dumps, textures
  ou films upscalés, C généré (dérivé du jeu). Vérifier `git status` avant.
- **Autonomie** (décidé par recompsmith le 2026-11-14) : Claude crée branches et
  worktrees, commite, pousse nos branches, ouvre les PR, fusionne dans `main`
  (nos PR et celles de TitanFr199) après revue et test cloud PASS, avance `stable` en
  avance rapide, pose les tags, publie les releases et poste l'annonce. Seule
  validation gardée : les **captures avant/après** de tout changement visuel,
  montrées à recompsmith avant la fusion. Après la fusion d'une PR de TitanFr199, le
  prévenir en une ligne.
- **À demander à recompsmith avant** : force-push (jamais par défaut, seulement
  s'il le demande explicitement), réécriture d'historique, suppression de
  branches distantes (déjà validées le 14/11 : les branches fusionnées et
  `integ/suivi-2026-10-22`), suppression ou
  modification d'un tag ou d'une release publiés (sauf la pré-version de test
  `vX-rcN`), changement de visibilité d'un dépôt, boîtes de combat au-delà du
  style Moderne, tout ce qui contredit un « DÉCIDÉ (recompsmith) ».
- Avant de modifier un fichier possédé par un autre rôle (`docs/roles/`), le
  demander au coordinateur. **Fichiers partagés** (`runtime/gl_render.c`,
  `runtime/menu_win32.c`, `runtime/frontend_sdl.c`, `runtime/gpu3d.c`,
  `runtime/gpu2d.c`, `runtime/menu.h`, `runtime/mapmarks.c`,
  `build-windows.sh`, `tools/build-game.sh`, `build.mk`) : annoncer au
  coordinateur la branche qui les touche ; il fait noter la ligne dans `docs/SUIVI.md`.
  Seul le rôle Intégration écrit `docs/SUIVI.md`.
- Ressources du jeu (textures, films, polices, ROM) : sauvegarde avant d'écraser.
- Textures HD : IA seule (Real-ESRGAN anime) pour toutes les textures.

## Plugins (installés : superpowers, pr-review-toolkit, code-simplifier, claude-code-setup)
- Avant un gros changement : `superpowers:brainstorming` puis `superpowers:writing-plans`.
- Bug : `superpowers:systematic-debugging`. Avant « fini » : `superpowers:verification-before-completion`.
- Avant fusion : sous-agent `pr-review-toolkit:code-simplifier`, puis `/pr-review-toolkit:review-pr`.
- Ne pas recréer d'agent qu'un plugin fournit déjà.

## Agents (`.claude/agents/`)
- `dqix-coordinateur` : arbitre les fichiers partagés ; à appeler avant d'ouvrir une branche ou de toucher un fichier partagé.
- `dqix-dev` : bug ou fonctionnalité technique (recompilateur, runtime C, son, entrées, Linux).
- `dqix-graphismes` : textures HD, films HD, restyle des fenêtres ; tout changement visible à l'écran.
- `dqix-langues` : traductions, polices, notes de version fr/de/it/es ; tout texte affiché.
- `dqix-testeur` : valide une branche en session cloud (build, xvfb, captures, `tests/semantics`).
- `dqix-relecteur` : relit le diff d'une PR prête, avant fusion.
- `dqix-release-manager` : prépare et publie une release avec `tools/release.py`, seulement quand recompsmith dit « release ».
- `dqix-archiviste` : met à jour `docs/SUIVI.md` et `CLAUDE.md` en fin de session ou après une fusion.
- `dqix-veille` : veille quotidienne (sackchief, PR de TitanFr199) ; signale seulement.
- `dqix-build-check` : compile un dossier avec BUILD.bat, surtout le dossier de jeu après mise à jour.

## Release (seulement quand recompsmith dit « release » ; détail : `docs/roles/integration.md`)
0. Créer d'abord le worktree (le clone principal reste sur `main`) : `git -C J:\DQIX-Enhanced-repo worktree add J:\DQIX-wt\integ-release-vX origin/main`, puis tout lancer dedans.
1. `python tools/release.py prepare vX` : branche `integ/release-vX`, `version.h`, liste des PR.
2. Notes `release-notes/vX.md` (+ fr/de/it/es), test cloud PASS sur le commit exact.
3. Essai : `publish vX-rc1 --prerelease --title "vX-rc1 : ..."`, `verify`, puis `delete-rc` (`stable` ne bouge pas).
4. `check vX` puis `publish vX --title "vX : ..."` : fusion, `stable` en avance rapide, release `--latest`.
5. `verify vX` (2 zips, Windows et Linux), puis `announce vX` (Discord, en français).

## Rôles (instructions : `docs/roles/<role>.md`)
Coordinateur (fil principal du projet) · Graphismes et interface · Langues et
ressources · Développement moteur et QoL · Intégration, tests et revue.

## Compte rendu (à chaque jalon)
**Objectif** · **Fichiers modifiés** · **Tests exécutés** (commande + résultat
réel) · **Résultats** · **Risques** · **Prochaine action** · **État Git**
(branche, commit, poussé ou non, PR).

## Reprendre une tâche dans une nouvelle session
1. Lire ce fichier, `docs/roles/<role>.md` et la ligne de la tâche dans `docs/SUIVI.md`.
2. `git status`, `git log --oneline -10` de la branche, état de la PR.
3. Lire le dernier compte rendu du thread, puis reprendre à « Prochaine action ».
