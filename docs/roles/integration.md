# Rôle : Intégration, tests et revue

Thread `[Intégration] Fusions et build`, branches `integ/<sujet>`. Un seul
thread à la fois tient ce rôle.

## Périmètre
Revue de chaque PR, fusion dans `main`, tests cloud, releases, tenue du suivi
central.

## Possède (seul à écrire)
- `main` (fusions), le clone `J:\DQIX-Enhanced-repo` (reste sur `main`),
  le dossier de jeu `J:\DQIX-Enhanced`.
- `docs/SUIVI.md`, `CLAUDE.md`, `docs/roles/`, `.claude/`, `CONTRIBUTING.md`,
  `docs/STATE.md`, et les branches `orga/`.
- Build, installation et versions : `BUILD.bat`, `build-pc.bat`, `recomp.bat`,
  `Install Dragon Quest IX.bat`, `tools/make-release.py`, `tools/release.py`,
  `tools/release_lib.py`, `tools/setup-wizard.ps1`, `tools/uninstall.ps1`,
  `tools/rom_icon.py`, `tools/where.sh`, `release-notes/`, `.github/`.
- Documentation : `README.md`, `ROADMAP.md`, `START HERE.txt`, `docs/TECHNICAL.md`,
  `docs/TESTS-CLOUD.md` (chaque rôle propose ses changements dans sa propre PR).

## Procédure pour chaque PR
1. Le thread de dev a déjà commité sa branche (compte rendu commun).
2. Sous-agent `pr-review-toolkit:code-simplifier` sur le diff, puis
   `/pr-review-toolkit:review-pr` (code-reviewer, silent-failure-hunter ;
   pr-test-analyzer si `tests/` change). Les corrections reviennent au thread de dev.
3. Test cloud de la branche (`dqix-testeur`, `docs/TESTS-CLOUD.md`) : rapport
   PASS pour le commit exact. Rien ne se lance sur le PC de recompsmith.
4. Changement visuel : captures avant/après montrées à recompsmith avant la fusion
   (seule validation gardée).
5. Fusion dans `main` (autonome : nos PR et celles de TitanFr199, règles de
   `CLAUDE.md`) ; une ligne à recompsmith après la fusion d'une PR de TitanFr199 ; mise à
   jour de `docs/SUIVI.md`.
6. Dossier de jeu : mis à jour par la mise à jour intégrée du jeu, on n'y touche
   pas. Si besoin, avec l'accord de recompsmith : `git -C J:\DQIX-Enhanced status`
   (doit être propre), `git pull --ff-only`, BUILD.bat, puis `dqix-build-check`
   sur `J:\DQIX-Enhanced`.

## À savoir
- Sauvegarde du dossier de jeu avant une mise à jour risquée : `J:\DQIX-Backups\<date>-<raison>`.
- La suppression de branches distantes se demande à recompsmith (déjà validées le
  14/11 : les branches fusionnées et `integ/suivi-2026-10-22`).
- Vérification : tests cloud (`docs/TESTS-CLOUD.md`), jamais le jeu sur le PC de recompsmith.

## Release (sur demande, autonome depuis le 2026-11-14)
Publication **seulement quand recompsmith dit « release »** (plus de release
hebdomadaire automatique). Tout passe par `tools/release.py` (agent
`dqix-release-manager`) ; `--dry-run` affiche chaque commande git/gh au lieu de
l'exécuter.

Numérotation (recompsmith, 2026-10-30) : versions entières à partir de v32 ; les
v31.1 à v31.11 l'ont précédée. Depuis le 2026-11-13 : après v47 vient **v2.0** ;
corrections = v2.1, v2.2... ; gros ajouts = v3, v4... `update.c` range un
majeur < 26 après v47. Les jeux en v47 ou avant lisent la première ligne
`DQIX_VERSION` de `version.h`, même en commentaire : la ligne
`#define DQIX_VERSION "v48 (vX)"` (garder « v48 » devant) reste **la première**
ligne `DQIX_VERSION`. Nos numéros ne suivent plus ceux de l'upstream : un port
devient la release suivante.

0. Créer d'abord un worktree (le clone `J:\DQIX-Enhanced-repo` reste sur `main`,
   `prepare` refuse de tourner dans le clone principal) :
   `git -C J:\DQIX-Enhanced-repo worktree add J:\DQIX-wt\integ-release-vX origin/main`,
   puis lancer toutes les commandes ci-dessous dedans.
1. `python tools/release.py prepare vX` : vérifie `gh auth status`, arbre propre,
   `origin/main` à jour ; crée `integ/release-vX` depuis `origin/main` ; met à
   jour `runtime/version.h` (`DQIX_VERSION` et date) ; affiche les PR fusionnées
   depuis le dernier tag `v*`. Aucune : pas de release.
2. Écrire `release-notes/vX.md` (anglais : « What's new since v(N-1) », comment
   installer, rien du jeu inclus ; une ligne par point, sans retour à la ligne
   forcé), plus `release-notes/fr/vX.md`, `de/`, `it/`, `es/` pour la fenêtre
   « Nouveautés » (`runtime/whatsnew.c`) : résumé et parties nouveautés /
   corrections seulement, mêmes noms d'options que `lang/*.txt`. Le jeu coupe la
   fin du fichier anglais à `**How to install**` (ou à
   `<!-- in-game notes end here -->`) ; une langue sans fichier montre l'anglais.
   Commit et push de la branche.
3. Test cloud sur le commit exact de la branche (`dqix-testeur`). Rapport :
   branche `tests/release-vX` de `recompsmith/dqix-test-prive`
   (`captures/release-vX/report.md`), première ligne `commit: <sha40>`, seconde
   `result: PASS|FAIL`. Rien du dépôt privé ne sort vers le dépôt public.
4. **Pré-version d'essai** (recommandée) :
   `python tools/release.py publish vX-rc1 --prerelease --title "vX-rc1 : ..."` crée la pré-version
   `vX-rc1` sur la branche de release, sans fusion, sans `--latest` et **sans
   jamais déplacer `stable`** ; `verify vX-rc1` contrôle le workflow et les zips ;
   puis `python tools/release.py delete-rc vX-rc1` supprime pré-version et tag.
   C'est la seule suppression de release autorisée ; le motif est
   `^v\d+(\.\d+)?-rc\d+$` et un vrai tag (`v3`) est refusé.
5. `python tools/release.py check vX` : aucun fichier interdit suivi par git,
   notes valides, `tools/lang_check.py` et `python -m unittest discover -s
   tests/release` à 0, rapport cloud = `HEAD` et PASS. Rapport d'un autre
   commit : refus, « relancer le test cloud sur <sha> ». Le script fait
   `git fetch origin` et refuse si la branche ne contient pas `origin/main`
   (rebaser la branche, puis relancer le test cloud).
6. `python tools/release.py publish vX --title "vX : ..."` : relance `check`,
   pousse la branche, ouvre la PR, `gh pr merge --merge`, vérifie que
   `origin/main` a le même contenu que le commit testé, puis
   `git push origin origin/main:stable` (commit de fusion, même arbre que le
   commit testé) **sans `--force`** (si `stable` n'est pas en avance rapide, le
   script s'arrête avant de publier), puis `gh release create vX --target stable
   --notes-file release-notes/vX.md --latest`. **`--latest` est obligatoire**,
   sinon RecompLauncher garde l'ancienne version.
7. `python tools/release.py verify vX` : attend le workflow `release-zip.yml` du
   tag (délai 15 min ; un échec arrête le script avec un message, la release
   reste publiée, pas de nouvelle tentative silencieuse) et contrôle les **2 zips**,
   `Dragon-Quest-IX-Modern-vX-Windows-x64.zip` et
   `Dragon-Quest-IX-Modern-vX-Linux-x64.zip`, et que la release est `latest`.
8. `python tools/release.py announce vX` : annonce Discord en français
   (< 2000 caractères) écrite par l'agent dans `J:\DQIX-Passation\annonces\vX.txt`
   (jamais dans git). Si `DISCORD_WEBHOOK_URL` existe, le script la poste ;
   sinon le fichier reste là et le script le dit. Ne jamais demander l'URL à
   recompsmith. Texte trop long ou webhook en erreur : erreur visible, fichier conservé.

**Reprise après un arrêt** (relancer `publish vX` ne reprend pas : la PR est
déjà fusionnée, `gh pr create` échoue) :
- Arrêt après la fusion, avant `stable` : vérifier `git diff --quiet <sha testé>
  origin/main`, puis `git push origin origin/main:stable` à la main (sans
  `--force`), puis `gh release create vX --target stable --title "vX : ..."
  --notes-file release-notes/vX.md --latest --repo recompsmith/Dragon-Quest-IX-Modern`.
- Arrêt après le push de `stable`, avant la release : lancer seulement ce
  `gh release create`.
- `verify` en échec ou en délai : la release reste publiée, corriger puis
  relancer `verify`.

- **Jamais dans une release** : ROM, `dqix.exe`, C généré, `build/`, `game/`,
  textures ou films upscalés, `wfc/` et les extras (données du jeu).
- Aucune étape n'attend un clic de recompsmith.
