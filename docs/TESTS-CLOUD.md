# Tests cloud

Les tests qui construisent et lancent le jeu tournent dans le cloud, jamais sur
le PC de recompsmith : rien n'y est compilé ni lancé pour tester, et la ROM n'y est
pas touchée.

## Pourquoi

- Le PC de recompsmith sert à jouer. Un build ou une partie de test y prend du temps
  et des ressources, et ne se fait qu'avec son accord.
- Un conteneur cloud neuf est reproductible : même système, mêmes paquets, même
  ROM, à chaque fois.
- Il a clang et LLVM, donc `tests/semantics/run.py` y tourne (absents du PC).

## Le dépôt privé

Dépôt `recompsmith/dqix-test-prive` (privé, il le reste). Il contient la ROM EU
(version européenne) en 3 parties, recollées au démarrage, le script
`tools/cloud-test.sh` et les captures. Rien de ce dépôt ne va dans le dépôt
public, une release ou Discord : ni ROM, ni capture du jeu.

## Lancer une session de test

1. recompsmith le demande dans un message (aucune session cloud ne démarre sans lui).
2. La session contrôleur ouvre, dans l'application Claude desktop, une nouvelle
   session **Cloud** sur le dépôt `recompsmith/dqix-test-prive`.
3. Dans cette session :

```
sh tools/cloud-test.sh <ref> <dossier de sortie> [images supplémentaires]
```

- `<ref>` : branche, tag ou commit du dépôt public à tester (ex. `main`).
- `<dossier de sortie>` : reçoit `report.md` et les captures.
- `[images supplémentaires]` : numéros d'images à photographier en plus de ceux
  par défaut.

Le script recolle la ROM, clone le dépôt public, installe les paquets
(`libsdl2-dev python3-yaml xvfb imagemagick clang lld llvm`), compile avec
`sh tools/build-game.sh`, lance `tests/semantics/run.py`, joue une partie
scriptée sans écran (`DQIX_FAST=1 DQIX_QUIT_NOASK=1 DQIX_INPUT_FILE=...
DQIX_WINSHOT_AT=...`, avec un `settings.ini` en `language = french` et
`last_version`), convertit les captures en PNG et écrit `report.md`.

Images par défaut : 1400, 1800 et 3600 (création du personnage).

Mode `WHATSNEW=1` : même chose, mais le `settings.ini` déclare
`last_version = v2.0` et le jeu tourne avec `DQIX_WHATSNEW_TEST=90`, pour
ouvrir la fenêtre « Nouveautés » et la photographier (`whatsnew-f90.png`).

Le script est `tools/cloud-test.sh` du dépôt privé (branche
`tests/cloud-test-script`, prouvé le 15/11/2026 sur `main` d1f9335 : les deux
modes donnent `result: PASS`).

## Résultats

Le résultat est poussé sur la branche `tests/<sujet>` du dépôt privé.

Contrat du rapport : les **deux premières lignes** de `report.md` sont
exactement :

```
commit: <sha de 40 caractères du commit testé>
result: PASS|FAIL
```

`PASS` = build avec code de sortie 0, `tests/semantics` à 0 échec, et toutes les
captures prises. Sinon `FAIL`. Suit le tableau des étapes (commande, code de
sortie, durée, résultat).

Pour une release, `tools/release.py check vX` lit
`captures/release-vX/report.md` sur la branche `tests/release-vX` du dépôt
privé et vérifie que le commit et le résultat correspondent.

## Captures avant / après

Pour un changement visible, on garde des captures avant et après dans le dépôt
privé et on les montre à recompsmith avant la fusion.

## Ce qu'on sait (essai du 14/11/2026)

- Conteneur Ubuntu 24.04, 4 vCPU, 15 Go de RAM, sans GPU (Mesa llvmpipe), sans
  carte son (« no audio » dans le journal : sans gravité).
- Build : environ 90 s. Paquets : environ 15 s. `tests/semantics` : 5 s,
  480000 vérifications, 0 échec.
- Vitesse : environ 22 images par seconde, soit environ 3 minutes pour une
  minute de jeu.
- `last-run.log` n'existe que sous Windows : sous Linux, lire `run.log` (la
  sortie standard).
- La fenêtre « Nouveautés » bloque les lancements sans écran quand
  `last_version` manque dans `settings.ini` (le lancement s'arrête à l'image 90
  et finit en timeout). `tests/smoke/run.py` écrit désormais `last_version`,
  lu dans `runtime/version.h`.
- Réseau : apt, github.com et raw.githubusercontent.com sont accessibles.
