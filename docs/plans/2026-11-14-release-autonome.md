# Releases autonomes, agents et tests cloud : plan d'implémentation

> **Pour les agents qui exécutent :** SOUS-COMPÉTENCE OBLIGATOIRE : superpowers:subagent-driven-development
> (choisi par recompsmith), une tâche = un sous-agent, double revue (conformité au plan, puis qualité).
> Les étapes utilisent des cases `- [ ]`.

**Objectif :** Claude prépare, teste (dans le cloud), publie et vérifie chaque release GitHub de bout en bout, sans aucune action de recompsmith sur GitHub ni sur son PC. La première release autonome est la **v3**.

**Architecture :** un script `tools/release.py` (Python 3, sans dépendance externe, Windows et Linux) avec des sous-commandes qui suivent `docs/roles/integration.md`. Les étapes qui demandent du jugement (rédiger les notes, les traductions, l'annonce) sont faites par les agents ; le script **vérifie** puis **exécute** les étapes mécaniques (git, `gh`, attente du workflow, contrôle des zips, webhook Discord). Le build et les tests du jeu tournent dans une **session cloud** à partir du dépôt privé `recompsmith/dqix-test-prive`, qui dépose un rapport ; `release.py` refuse de publier sans un rapport réussi pour le commit exact de la release.

**Outils :** Python 3.12 (`unittest`, `subprocess`, `urllib`), GitHub CLI 2.104 (connecté, droits `repo` + `workflow`), sessions cloud Claude Code (Ubuntu 24.04, Mesa llvmpipe, xvfb), git.

**Spécification :** la demande de recompsmith du 2026-11-14 (« Reprise du projet… », règles 1 à 4) + ses réponses du même jour, résumées ci-dessous. Passation : `J:\DQIX-Passation\HISTORIQUE-DQ9-ENHANCED.md`.

## Décisions de recompsmith (2026-11-14), reprises telles quelles

- Prochaine release = **v3** (Linux/Steam Deck + fenêtre Nouveautés), avec #144, #145, #146 et les 3 commits de `claude/project-thread-on5r0q`.
- Annonce Discord en français à chaque release (< 2000 caractères, `len()`), postée par Claude si `DISCORD_WEBHOOK_URL` existe (créée par recompsmith le 14/11, visible après redémarrage de l'app), sinon écrite dans `J:\DQIX-Passation\annonces\vX.txt`. Jamais demander l'URL.
- Pré-version de test : sur le commit de la branche de release, **`stable` jamais déplacée pour un test** ; tag `vX-rcN` et pré-version supprimés ensuite (seule suppression de release autorisée).
- **Rien ne se lance sur le PC de recompsmith** (ni jeu, ni BUILD.bat) : build, tests et captures dans une session cloud à partir de `recompsmith/dqix-test-prive` (essai réussi le 14/11 : build 89 s, jeu sous xvfb, 3 captures, `tests/semantics` 480 000 vérifications, 0 échec).
- Publication **seulement quand recompsmith dit « release »** (pas de release hebdomadaire automatique).
- Autonomie : branches, worktrees, commits, push de nos branches, PR, fusion dans `main` (nos PR et celles de TitanFr199) après revue + tests, `stable` en avance rapide, tags, releases, annonce. Seule validation gardée : **captures avant/après** pour tout changement visuel, montrées avant la fusion. recompsmith est prévenu en une ligne après chaque fusion d'une PR de TitanFr199.
- À lui demander avant : force-push, réécriture d'historique, suppression/modification d'un tag ou d'une release publiés (sauf la pré-version de test et la description de la v2.0, voir tâche 9), changement de visibilité, boîtes de combat au-delà du style Moderne, tout ce qui contredit un « DÉCIDÉ (recompsmith) ».
- Validé aussi : retirer « v48 (v2.0) » de la description publiée de la v2.0 ; fermer les PR #8, #14, #27 chez sackchief ; supprimer les branches distantes déjà fusionnées et `integ/suivi-2026-10-22` (liste montrée dans ce plan, tâche 9) ; dépôt public ; dossier `J:\DQIX-Enhanced` mis à jour par la mise à jour intégrée du jeu (on n'y touche pas) ; refaire `docs/SUIVI.md` en entier ; réécrire `ROADMAP.md` ; « Utiliser ×N » et retrait du code mort des dispositions **après** la v3 (plans séparés) ; remake Godot hors de cette session ; TitanFr199 garde ses branches `dev/<sujet>`.
- Veille quotidienne (tâche planifiée, 07:47 UTC) : sackchief (tags, PR) **et les PR de TitanFr199** sur notre dépôt. Signaler seulement, ne jamais proposer de port.

## Contraintes globales

- Réponses à recompsmith en français, résultat d'abord ; commits, branches, PR en anglais ; fin de commit `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, fin de PR `🤖 Generated with [Claude Code](https://claude.com/claude-code)`.
- Interdit : `wifi.c`, `wfc.c`, `net.c`, `relay.c`, `relay.h`, `relay_url.h`, `upnp.c`, `portmap.c`, `session.c`, `air.c`, `hosting.c`, `third_party/libslirp`, `tools/relay/`.
- Jamais suivi par git dans le dépôt public : `*.nds`, `*.3ds`, `*.cia`, `*.cci`, `*.cxi`, `build/`, `game/`, `gen/`, C généré, textures/films upscalés, images RetroAchievements, `title-back.png`, sauvegardes (`*.sav`). Jamais d'identifiant C nommé `far` ou `near`.
- Jamais de force-push ; `stable` n'avance qu'en avance rapide (`git push origin origin/main:stable` sans `--force`).
- `gh release create … --latest` obligatoire pour une vraie release (sinon RecompLauncher garde l'ancienne).
- Zips attendus après publication : `Dragon-Quest-IX-Modern-vX-Windows-x64.zip` **et** `Dragon-Quest-IX-Modern-vX-Linux-x64.zip`.
- `runtime/version.h` : la ligne de commentaire `#define DQIX_VERSION "v48 (vX)"` reste **la première** ligne `DQIX_VERSION` du fichier.
- Ne jamais éditer `J:\DQIX-Enhanced` ni `J:\DQ9 Décompiler`. Ne jamais présenter sackchief comme l'origine du projet.
- Dépôt privé `recompsmith/dqix-test-prive` : reste privé ; rien n'en sort (ROM, captures du jeu) vers le dépôt public, une release ou Discord.

## Points d'attention pour la revue

1. **Workflow lent ou en échec** : `gh run watch` doit avoir un délai (15 min) ; un échec du workflow arrête le script avec un message clair, la release reste publiée et le rapport le dit (pas de nouvelle tentative silencieuse).
2. **`stable` pas en avance rapide** (quelqu'un a poussé sur `stable`) : le push échoue → le script s'arrête **avant** `gh release create`, rien n'est publié.
3. **Rapport cloud d'un autre commit** (la branche a bougé après le test) : refus de publier, message « relancer le test cloud sur <sha> ».
4. **Annonce de 2000 caractères ou plus**, ou webhook qui répond une erreur : refus/erreur visible, le texte est quand même écrit dans `J:\DQIX-Passation\annonces\`.
5. **Nettoyage de pré-version appelé sur un vrai tag** (`v3` au lieu de `v3-rc1`) : refus net (motif `^v\d+(\.\d+)?-rc\d+$`).

Chaque point a son test dans la tâche qui possède le code (tâches 3 et 4).

---

## Structure des fichiers

| Fichier | Rôle |
|---|---|
| `tools/release.py` | Point d'entrée : sous-commandes `prepare`, `check`, `publish`, `verify`, `announce`, `delete-rc`, option `--dry-run` |
| `tools/release_lib.py` | Fonctions pures testables (version.h, PR depuis le dernier tag, garde-fou droits d'auteur, notes, annonce, motif rc) |
| `tests/release/test_release_lib.py` | Tests `unittest` de `release_lib.py` (tournent sur Windows et Linux, sans réseau) |
| `tests/smoke/run.py` | Correctif : `last_version` dans le `settings.ini` écrit (sinon la fenêtre Nouveautés bloque le test sans écran) |
| `docs/TESTS-CLOUD.md` | Recette : lancer une session cloud de test, format du rapport |
| `.claude/agents/dqix-*.md` | 8 nouveaux agents (tâche 1) |
| `CLAUDE.md`, `docs/roles/integration.md`, `docs/SUIVI.md`, `ROADMAP.md` | Nouveau flux (tâches 2, 7) |
| Dépôt privé : `tools/cloud-test.sh`, `captures/<sujet>/report.md` | Test cloud reproductible (tâche 5) |

---

### Tâche 1 : les agents spécialisés

**Fichiers :** créer `.claude/agents/dqix-coordinateur.md`, `dqix-dev.md`, `dqix-graphismes.md`, `dqix-langues.md`, `dqix-testeur.md`, `dqix-relecteur.md`, `dqix-release-manager.md`, `dqix-archiviste.md`, `dqix-veille.md` ; garder `dqix-build-check.md` (description complétée : « seulement si recompsmith le demande, jamais sur son PC par défaut »).

Chaque fichier : en-tête `name`, `description` (quand l'appeler, en une phrase), `tools` (liste minimale), corps en français : périmètre, fichiers possédés (repris de `docs/roles/`), interdits (contraintes globales), format du compte rendu en 7 points.

| Agent | Outils | Point clé du corps |
|---|---|---|
| dqix-coordinateur | Read, Grep, Glob, Bash | Tient la table des fichiers partagés (`gl_render.c`, `menu_win32.c`, `frontend_sdl.c`, `gpu3d.c`, `gpu2d.c`, `menu.h`, `mapmarks.c`, `build.mk`, `tools/build-game.sh`, `build-windows.sh`) ; refuse deux branches ouvertes sur le même fichier partagé (vérifie avec `gh pr list` + `gh pr diff --name-only`) |
| dqix-dev | Read, Edit, Write, Grep, Glob, Bash | `runtime/`, `recomp/` ; liste noire réseau ; pas de `far`/`near` |
| dqix-graphismes | Read, Edit, Write, Grep, Glob, Bash | Captures avant/après obligatoires via un test cloud, rangées dans le dépôt privé |
| dqix-langues | Read, Edit, Write, Grep, Glob, Bash | `lang/*.txt` vérifiés par `python tools/lang_check.py`, `release-notes/{fr,de,it,es}/` |
| dqix-testeur | Read, Grep, Glob, Bash | Lance et lit les tests cloud (tâche 5) ; ne lance rien sur le PC ; liste toujours ce qui n'a PAS été testé |
| dqix-relecteur | Read, Grep, Glob, Bash | Revue de diff (pr-review-toolkit, code-simplifier), `git diff | grep -nwE 'far|near'` |
| dqix-release-manager | Read, Edit, Write, Grep, Glob, Bash | Suit `docs/roles/integration.md` avec `tools/release.py` |
| dqix-archiviste | Read, Edit, Write, Grep, Glob, Bash | Seul à écrire `docs/SUIVI.md` ; tient `CLAUDE.md` ; ligne datée en fin de session |
| dqix-veille | Read, Bash | `git ls-remote --tags` sackchief, état des PR sackchief, PR ouvertes de TitanFr199 sur notre dépôt ; signale seulement |

- [ ] **Étape 1 :** écrire les 9 fichiers.
- [ ] **Étape 2 : vérification.** `python -c` qui lit chaque `.claude/agents/*.md` et vérifie l'en-tête (`name` = nom du fichier, `description` et `tools` non vides). Attendu : 10 fichiers OK.
- [ ] **Étape 3 : commit** `Agents: one per project role, plus release manager, archivist and watch`.

### Tâche 2 : CLAUDE.md et la procédure de release

**Fichiers :** modifier `CLAUDE.md`, `docs/roles/integration.md`, `CONTRIBUTING.md` (section Workflow, point 3).

- `CLAUDE.md` : nom du dépôt ; « Textures HD : IA seule (Real-ESRGAN anime) » au lieu de « méthode 5 » ; `t3d.c` recompile depuis la PR #45 ; section « Tester » = tests cloud (`docs/TESTS-CLOUD.md`), rien sur le PC de recompsmith ; liste des fichiers partagés complétée (`gpu2d.c`, `menu.h`, `mapmarks.c`) ; règle d'autonomie et liste « à demander avant » (reprises des décisions ci-dessus) ; liste des agents ; nouveau flux de release en 5 lignes renvoyant à `integration.md`.
- `integration.md` : la section « Release » remplacée par la suite de commandes `tools/release.py` (tâche 4), le motif rc, `--latest`, les 2 zips, l'annonce ; la ligne « recompsmith clique sur Publish » supprimée.

- [ ] **Étape 1 :** modifier les 3 fichiers.
- [ ] **Étape 2 : vérification.** `grep -n "méthode 5\|ne compile plus\|Publish" CLAUDE.md docs/roles/integration.md` → aucune ligne qui décrit l'ancien fonctionnement.
- [ ] **Étape 3 : commit** `CLAUDE.md and release procedure: autonomous releases, cloud tests`.

### Tâche 3 : `tools/release_lib.py` (fonctions pures, TDD)

**Fichiers :** créer `tools/release_lib.py`, `tests/release/test_release_lib.py`.

**Produit (utilisé par la tâche 4) :**
- `bump_version_h(text: str, version: str, date: str) -> str` : remplace `#define DQIX_VERSION "…"`, `DQIX_VERSION_DATE` et la ligne de commentaire en `#define DQIX_VERSION "v48 (<version>)"` ; lève `ValueError` si une des 3 lignes manque.
- `merged_prs(log: str) -> list[int]` : lit la sortie de `git log --merges --format=%s <tag>..<ref>` et rend les numéros de « Merge pull request #N », dans l'ordre.
- `forbidden_tracked(paths: list[str]) -> list[str]` : chemins interdits (extensions `.nds .3ds .cia .cci .cxi .sav`, préfixes `build/ game/ gen/`, noms `title-back.png`, dossiers `textures/`, `movies/`, `trophies/` hors `tools/`, et les `*.c` sous `gen/` ou `recomp-out/`).
- `check_notes(version: str, files: dict[str, str]) -> list[str]` : erreurs si `release-notes/<v>.md` ou une des 4 traductions manque, si une traduction contient `**How to install**`, ou si une ligne de liste est coupée (ligne qui suit un `- …` et ne commence ni par `- `, ni par `**`, ni est vide).
- `check_announce(text: str) -> list[str]` : erreur si `len(text) >= 2000` ou si le texte contient `discord.gg/` autre que `z9X9PH8AH4`.
- `is_rc_tag(tag: str) -> bool` : `re.fullmatch(r"v\d+(\.\d+)?-rc\d+", tag)`.
- `parse_cloud_report(text: str) -> tuple[str, bool]` : lit les lignes `commit: <sha40>` et `result: PASS|FAIL` du rapport cloud (format fixé en tâche 5) ; `ValueError` si l'une manque.

- [ ] **Étape 1 : tests qui échouent**, un par fonction et un par point d'attention, par exemple :
```python
def test_bump_keeps_v48_comment_first(self):
    out = bump_version_h(V21, "v3", "2026-11-15")
    lines = [l for l in out.splitlines() if "DQIX_VERSION" in l and "DATE" not in l]
    self.assertIn('"v48 (v3)"', lines[0]); self.assertIn('"v3"', lines[1])
def test_rc_refuses_real_tag(self):
    self.assertFalse(is_rc_tag("v3")); self.assertTrue(is_rc_tag("v3-rc1"))
def test_announce_2000_refused(self):
    self.assertTrue(check_announce("a" * 2000)); self.assertFalse(check_announce("a" * 1999))
def test_forbidden(self):
    self.assertEqual(forbidden_tracked(["rom/x.nds", "game/ui/title-back.png", "tools/ui/equip-shop.png", "runtime/main.c"]),
                     ["rom/x.nds", "game/ui/title-back.png"])
def test_merged_prs(self):
    self.assertEqual(merged_prs("Merge pull request #146 from a/b\nMerge pull request #145 from c/d"), [146, 145])
def test_cloud_report(self):
    self.assertEqual(parse_cloud_report("commit: " + "a"*40 + "\nresult: PASS\n"), ("a"*40, True))
```
`V21` = le contenu actuel de `runtime/version.h` (lu dans le dépôt par le test).
- [ ] **Étape 2 :** `python -m unittest discover -s tests/release -v` → échec (module absent).
- [ ] **Étape 3 :** écrire `tools/release_lib.py`.
- [ ] **Étape 4 :** même commande → tout passe.
- [ ] **Étape 5 : commit** `release_lib: version.h bump, PR list, copyright guard, notes and announce checks`.

### Tâche 4 : `tools/release.py` (commandes)

**Fichiers :** créer `tools/release.py` ; ajouter à `tests/release/test_release_lib.py` les tests du mode `--dry-run` (sous-processus simulé : `run()` remplaçable).

**Consomme :** tout `release_lib`. **Produit :** les sous-commandes ci-dessous, appelées par l'agent release-manager.

| Commande | Fait |
|---|---|
| `prepare vX` | vérifie `gh auth status`, arbre propre, `origin/main` à jour ; crée la branche `integ/release-vX` depuis `origin/main` ; applique `bump_version_h` (date du jour) ; affiche la liste `merged_prs` depuis le dernier tag `v*` (pour que l'agent écrive les notes) |
| `check vX` | `forbidden_tracked(git ls-files)` vide ; `check_notes` vide ; `python tools/lang_check.py` exit 0 ; `python -m unittest discover -s tests/release` exit 0 ; rapport cloud (`gh api repos/recompsmith/dqix-test-prive/contents/captures/release-vX/report.md?ref=tests/release-vX`, branche du dépôt privé poussée par la session cloud) : `parse_cloud_report` → même sha que `HEAD` et `PASS`. Tout échec = arrêt, code 1 |
| `publish vX --title "..."` | relance `check` ; pousse la branche, ouvre la PR, `gh pr merge --merge` ; `git push origin origin/main:stable` (sans force ; même arbre que le commit testé, vérifié) ; `gh release create vX --target stable --title "<titre>" --notes-file release-notes/vX.md --latest` |
| `publish vX-rcN --prerelease --title "..."` | comme `publish` **sans** fusion ni `stable` : `gh release create vX-rcN --target integ/release-vX --prerelease` (pas de `--latest`) |
| `verify vX` | attend le run `release-zip.yml` du tag (`gh run list --workflow release-zip.yml`, puis `gh run watch <id> --exit-status`, délai 15 min) ; `gh release view vX --json assets,isLatest,isPrerelease` : les 2 zips présents, `isLatest` vrai pour une vraie release |
| `announce vX` | lit le fichier `--file` (par défaut `J:\DQIX-Passation\annonces\vX.txt`, écrit par l'agent, jamais dans git) ; `check_announce` ; si `DISCORD_WEBHOOK_URL` existe : POST JSON `{"content": …}` (urllib), sinon laisse le fichier et le dit |
| `delete-rc vX-rcN` | refuse si `not is_rc_tag` ou si la release n'est pas `isPrerelease` ; `gh release delete vX-rcN --cleanup-tag --yes` |

`--dry-run` : affiche chaque commande git/gh au lieu de l'exécuter.

- [ ] **Étape 1 : tests qui échouent** : `publish` en `--dry-run` avec un `stable` non avance rapide simulé (push en échec) → aucune commande `gh release create` émise ; `delete-rc v3` → code 1 sans aucune commande `gh` ; `verify` avec un `run()` qui renvoie un run en échec → code 1 et message.
- [ ] **Étape 2 :** lancer → échec. **Étape 3 :** écrire `release.py`. **Étape 4 :** lancer → passe ; `python tools/release.py publish v3 --dry-run` affiche la suite attendue.
- [ ] **Étape 5 : commit** `tools/release.py: prepare, check, publish, verify, announce, delete-rc`.

### Tâche 5 : tests cloud reproductibles

**Fichiers :** dépôt public : modifier `tests/smoke/run.py` (ajouter `last_version = <DQIX_VERSION lu dans runtime/version.h>` au `settings.ini` qu'il écrit), créer `docs/TESTS-CLOUD.md`. Dépôt privé : créer `tools/cloud-test.sh`.

`cloud-test.sh <ref du dépôt public> <dossier de sortie>` : recolle la ROM, `git clone` + `checkout <ref>`, `apt-get install -y libsdl2-dev python3-yaml xvfb imagemagick clang lld llvm`, `sh tools/build-game.sh`, `tests/semantics/run.py`, lancement sans écran (recette de l'essai du 14/11 : `DQIX_FAST=1 DQIX_QUIT_NOASK=1 DQIX_INPUT_FILE`, `DQIX_WINSHOT_AT`, `settings.ini` avec `language = french` et `last_version`), conversion PNG, puis `report.md` qui commence par :
```
commit: <sha40 testé>
result: PASS|FAIL
```
(`PASS` = build exit 0, semantics 0 échec, toutes les captures prises), suivi du tableau des étapes comme dans l'essai.

- [ ] **Étape 1 :** dans une session cloud, lancer `tests/smoke/run.py` actuel sans écran → constater le blocage (Nouveautés), noter la sortie.
- [ ] **Étape 2 :** corriger `run.py` ; relancer → le script dépasse l'image 90 et prend ses captures.
- [ ] **Étape 3 :** `cloud-test.sh` sur `origin/main` → `report.md` avec `result: PASS`, 3 captures en français.
- [ ] **Étape 4 : commits** (public) `Smoke test: set last_version so What's new does not stop headless runs`, (privé) `cloud-test.sh: build, semantics and screenshots with a report`.

### Tâche 6 : veille quotidienne

**Fichiers :** aucun dans le dépôt. Tâche planifiée locale (outil scheduled-tasks) `dqix-veille`, tous les jours à 07:47 UTC, qui appelle l'agent `dqix-veille`.

Elle compare à l'état gardé dans `J:\DQIX-Passation\veille-etat.json` : tags de sackchief (dernier connu `v34-nightly2`), état des PR sackchief #8, #14, #27, PR ouvertes ou mises à jour par `TitanFr199` sur `recompsmith/Dragon-Quest-IX-Modern`. Notification seulement s'il y a du nouveau ; jamais de proposition de port.

- [ ] **Étape 1 :** créer l'état initial et la tâche. **Étape 2 :** la lancer une fois → « rien de nouveau » (ou la liste réelle). **Étape 3 :** ligne dans `docs/SUIVI.md` (tâche 7).

### Tâche 7 : `docs/SUIVI.md` refait et `ROADMAP.md` réécrit (agent archiviste)

**Fichiers :** `docs/SUIVI.md`, `ROADMAP.md`.

- `SUIVI.md` : tâches ouvertes seulement (Linux/Nouveautés intégrées, points blancs du menu Divers en attente d'un F10, « Utiliser ×N » et code mort planifiés après la v3, contrôles joueurs à obtenir : vrai Steam Deck, manette Xbox, images des trophées, PNJ 3D avec ROM 3DS) ; tableau des décisions complété jusqu'au 14/11 (dont celles de ce plan) ; fichiers partagés ; flux de release ; tests cloud ; ligne datée de fin de session. L'audit du 28/10 déplacé en annexe `docs/archives/audit-2026-10-28.md`. Reprend les 3 commits de `claude/project-thread-on5r0q` (cherry-pick) pour garder leur contenu.
- `ROADMAP.md` : ce projet (sans multijoueur), ce qui est fait, la suite (v3, Utiliser ×N, nettoyage, remake Godot après la décompilation), sans présenter sackchief comme l'origine.

- [ ] **Étape 1 :** cherry-pick `263c36a ecd4f78 e112778`. **Étape 2 :** réécrire les 2 fichiers. **Étape 3 :** relecture par `dqix-relecteur`. **Étape 4 : commit** `SUIVI rewritten for the autonomous release flow; ROADMAP for this project`.

### Tâche 8 : fusion de la branche d'organisation

- [ ] **Étape 1 :** `pr-review-toolkit:code-simplifier` puis `/pr-review-toolkit:review-pr` sur `orga/release-autonome` ; corriger.
- [ ] **Étape 2 :** test cloud (tâche 5) sur la branche → `PASS` (aucun code du jeu ne change, mais `tests/smoke/run.py` oui).
- [ ] **Étape 3 :** PR `orga/release-autonome` → `main`, fusion.

### Tâche 9 : ménage validé

- [ ] **Étape 1 :** description de la v2.0 : retirer la phrase « If you are on v47 or older, the game's update message calls this one "v48 (v2.0)". » (`gh release edit v2.0 --notes-file`), rien d'autre.
- [ ] **Étape 2 :** fermer les PR sackchief #8, #14, #27 avec un commentaire neutre en anglais (« Closing: this work now lives in its own project. Thanks! »).
- [ ] **Étape 3 :** lister les branches distantes entièrement contenues dans `main` (`git branch -r --merged origin/main`, hors `main`, `stable`, `HEAD`) **plus** `integ/suivi-2026-10-22` et `claude/project-thread-on5r0q` (après la tâche 7) ; montrer la liste à recompsmith dans le compte rendu, puis `git push origin --delete` branche par branche. Aucune branche de TitanFr199 non fusionnée n'est supprimée.

### Tâche 10 : essai avec une pré-version

- [ ] **Étape 1 :** `release.py prepare v3` ; l'agent langues écrit `release-notes/v3.md` (anglais, une ligne par point) et `release-notes/{fr,de,it,es}/v3.md` ; titre « v3: Linux and Steam Deck, What's new window ».
- [ ] **Étape 2 :** test cloud sur la branche `integ/release-v3` → `captures/release-v3/report.md` `PASS` sur la branche `tests/release-v3` du dépôt privé, avec une capture de la fenêtre Nouveautés en français (lancement avec `last_version = v2.1`).
- [ ] **Étape 3 :** `release.py publish v3-rc1 --prerelease --title "v3-rc1"`, puis `release.py verify v3-rc1` → 2 zips, `isPrerelease` vrai, `isLatest` faux, `stable` inchangée (`git rev-parse origin/stable` = `1c4f393`).
- [ ] **Étape 4 :** `release.py delete-rc v3-rc1` → `gh release view v3-rc1` en erreur, tag absent (`git ls-remote --tags origin v3-rc1` vide).

### Tâche 11 : release v3

- [ ] **Étape 1 :** `release.py check v3` → OK (le rapport cloud doit être celui du commit final ; sinon relancer la tâche 10, étape 2).
- [ ] **Étape 2 :** `release.py publish v3 --title "v3: ..."` puis `release.py verify v3` → 2 zips, `isLatest` vrai, `origin/stable` = commit de la release.
- [ ] **Étape 3 :** l'agent écrit l'annonce FR ; `release.py announce v3` → postée (ou fichier si la variable n'est pas visible).
- [ ] **Étape 4 :** compte rendu en 7 points ; rappel à recompsmith : ajouter le motif `Dragon-Quest-IX-Modern-*-Linux-x64.zip` dans le formulaire RecompLauncher (une seule fois).

## Hors de ce plan (plans séparés, après la v3)

« Utiliser ×N » (demande de TitanFr199) ; retrait du code mort des dispositions (`MI_STACKED`… dans `frontend_sdl.c`) ; avertissement `cmp` de `tools/build-game.sh` ; `last-run.log` absent sous Linux (relevé par l'essai cloud).
