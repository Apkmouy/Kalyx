# GEMINI.md - Architecture, Règles Temps Réel & Standards de Développement (Kalyx Hybrid Console v3 — Châssis 5-Slots Série 500)

Ce document définit les règles d'ingénierie strictes et l'architecture modulaire **Châssis Studio 5-Slots Série 500 + Entrée/Master** du plugin **Kalyx Hybrid Console** (JUCE 8 / C++20).

---

## 1. Stack Technique & Portabilité (Linux / Windows)

- **Standard C++ :** C++20 strict (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`).
- **Framework :** **JUCE 8** (`8.0.12`) + `clap-juce-extensions` récupérés automatiquement via CMake `FetchContent`.
- **Formats de sortie :** `VST3`, `LV2`, `CLAP` et `Standalone` (Linux & Windows).
- **Zéro dépendance graphique fragile :**
  - Utilisation exclusive du moteur de rendu vectoriel logiciel standard `juce::Graphics`.
  - **Interdiction absolue** d'activer OpenGL (`juce_opengl`) ou `WebBrowserComponent` (`JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`).
- **Règles de compilation par OS :**
  - **Windows (MSVC) :** Linkage statique du runtime C/C++ (`CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"`).
  - **Linux (GCC/Clang) :** `CMAKE_POSITION_INDEPENDENT_CODE ON` (`-fPIC`) global + paquets systèmes (`alsa`, `freetype2`, `fontconfig`, `x11`, `xext`, `xrandr`, `xinerama`, `xcursor`) via `pkg-config`.
  - **Système de fichiers :** `juce::File` ou `std::filesystem::path` exclusivement (gestionnaire de presets dans `Documents/KalyxAudio/Presets/`).

---

## 2. Contraintes Temps Réel & Stabilité Numérique (Non-Négociables)

1. **Zéro allocation mémoire dynamique & zéro verrou dans `processBlock()` :**
   - Aucun `new`, `delete`, `malloc`, `free`, `std::vector::push_back`, `std::string` ou `std::mutex` dans le thread audio.
   - Toutes les instances DSP des 5 slots (A à E) sont pré-allouées dans `prepareToPlay()`.
   - Communication Audio ↔ GUI exclusivement lock-free via `std::atomic<float>` (`std::memory_order_relaxed`) et pointeurs bruts APVTS.
2. **Précision `double` (64 bits) obligatoire :**
   - Tous les calculs internes de `ADAASaturator`, `HysteresisStage`, enveloppes dynamiques (SSL VCA, API 2500 Thrust/Knee/Link, Neve 5254 Diode-Bridge, Limiteur, Gate/De-Esser), balistique VU 300 ms et `ZDFFilters` tournent en `double` (`44.1 kHz` à `192 kHz`).
3. **Correction mathématique ADAA ($F_1(0) = 1.0$) & Silence Gate :**
   - Initialiser `uPrev = bias`, `uPrev2 = bias`, `F1Prev = F1(bias)` (`1.0` quand `bias = 0`), `F2Prev = F2(bias)` dans `reset()`.
   - Seuil de repli de Taylor à `1e-4` renvoyant $f\left(\frac{u_n + u_{n-1}}{2}\right)$.
   - Court-circuit non-linéaire lorsque $|u_n| < 10^{-6}$ ou `drive <= 1e-5` garantissant `< -120 dBFS` (`< 1e-6`) sur une entrée silencieuse (`0.0`) sur tous les modules.
4. **Isolation stricte des bus `4-in / 2-out` :**
   - Extraire explicitement `getBusBuffer(buffer, true, 0)` (audio principal) et `getBusBuffer(buffer, true, 1)` (sidechain externe) ; ne jamais mélanger le sidechain dans la sortie principale (sauf si `SC LISTEN` est actif).
5. **Gel de l'`AUTO-GAIN` sous `-60 dBFS` :**
   - Figer la compensation automatique dès que le signal d'entrée passe sous `-60 dBFS` (`1e-6` en puissance) et la ramener doucement à `0.0 dB` après 2 secondes de silence.
6. **Commutation sans clic (`Click-Free Crossfade`) :**
   - Fondu enchaîné linéaire de `15 ms` dans `processBlock()` lors d'un changement de module dans un slot (A à E) pendant la lecture.
7. **Gain Reduction natif pour REAPER / DAW & Sécurité en sortie :**
   - Exposition temps réel de la réduction de gain cumulée pour les hôtes (REAPER / VST3 / CLAP).
   - `juce::ScopedNoDenormals noDenormals;` en première ligne de `processBlock()`, sanitisation `std::isfinite` et écrêtage `[-4.0f, 4.0f]` (`+12 dBFS`).

---

## 3. Architecture « Châssis Studio 5-Slots + Entrée / Master »

- **Structure :** `[ INPUT (Fixe) | SLOT A | SLOT B | SLOT C | SLOT D | SLOT E | MASTER (Fixe) ]`.
- **Pool de Paramètres APVTS universel (18 paramètres par slot) :**
  - `slotA_module_id .. slotE_module_id` + `slotA_p1..p18` à `slotE_p1..p18` + ordre de routage des 5 slots (`slot_order_0..4`).
- **VU-mètre Analogique Compact 300 ms (IEC 60268-17) :** Unique VU-mètre rétroéclairé dans `MASTER` calibré par défaut sur `0 VU = 0 dBFS` avec sélecteur segmenté `[ IN | OUT | GR ]`.

---

## 4. Design System Permanent : « Tactile Flat / Hardware-Referenced » (Kalyx Audio)

Cette charte graphique officielle s'applique automatiquement à toutes les sessions et à l'ensemble du châssis et des modules :

### Philosophie « Tactile Flat / Hardware-Referenced »
1. **Vectoriel pur & sans textures lourdes :**
   - Abandon total de tout skeuomorphisme (pas de métal brossé, pas d'ombres 3D complexes ni de textures granulées).
   - Tous les contrôles sont dessinés via des primitives vectorielles nettes (`juce::Graphics` : cercles plats `#222630`, pistes `#2E3340`, arcs de valeur colorés, pilules segmentées, touches mécaniques).
2. **Mécanique tactile préservée :**
   - Conserver les comportements physiques emblématiques des machines : touches mécaniques en rangée pour les ratios du 1176 (avec enfoncement simultané en mode `All-Buttons In`), sélecteurs rotatifs crantés avec points de graduation et aimantation (plus option `STEPPED / CONTINUOUS` sur les paramètres crantés), et hiérarchie de diamètre entre réglages principaux et secondaires.
3. **Propagation visuelle des états :**
   - Tout contrôle rendu inactif par un commutateur (ex. `RELEASE` lorsque `AUTO REL` est actif) passe à **35 % d'opacité** avec un arc gris neutre.
4. **Responsive Design Proportionnel (`uiScale`) :**
   - Facteur d'échelle dynamique global calculé dans `PluginEditor::resized()` sur le gabarit de référence `1480 x 820` :
     `const float uiScale = std::clamp(std::min(getWidth() / 1480.0f, getHeight() / 820.0f), 0.75f, 2.20f);`
   - Propagation systématique de `uiScale` à `KalyxLookAndFeel`, `AnalogVUMeter`, `ModuleSlotView` et chaque `IModuleFaceplate` (`11.5f * uiScale` labels, `12.0f * uiScale` valeurs en `tabular-nums`, `11.0f * uiScale` boutons/en-têtes, pilules `22.0f * uiScale`, voyants `3.5f * uiScale`, traits `1.8f * uiScale`).

### Palette de Couleurs Officielle
- **Signature Kalyx Audio (Châssis, Header, Input, Master & Modules Originaux) :**
  - **Fond Châssis Principal :** Anthracite Nuit `#111318`
  - **Surface unifiée de tous les Slots & Cartes :** Gris Ardoise Profond `#181B22` (bordures fines `#262A35`)
  - **Couleur Primaire Kalyx (Aubergine Profond) :** `#5B2344` (actif/survol `#7A325D`)
  - **Couleur Secondaire Kalyx (Bleu Roi Intense) :** `#2348B8` (arcs actifs & voyants `#3B66E0`)
  - **Day Mode :** Aluminium Neutre `#E6E8EC`, surfaces `#F4F5F7`, texte Anthracite `#12151C`, accents Aubergine `#6D2B52` et Bleu Roi `#1E3FA3`.
- **Codes Couleurs Référencés par Machine (sur fond de slot `#181B22`, appliqués aux bandeaux, liserés et arcs actifs) :**
  - **Kalyx Original (Color Stage, Peak Limiter, Gate / De-Esser) :** Bleu Roi `#2348B8` & Aubergine `#5B2344`
  - **SSL 4000 E / G :** Rouge Vin `#9E2A3B` (E) / Rose `#C85A7C` (G), Ardoise `#4A6B82`, Jade `#2D7A64`
  - **API 1608 / 2500 / 550B :** Bleu Cobalt `#1E64B4` & Cyan Clair `#4DA3FF`
  - **Neve 1073 / 5254 :** Rouge Brique Marconi `#A62C3B` & Bleu-Gris RAF `#467294`
  - **Universal Audio 1176 :** Bleu Bluestripe `#2B6CB0` (Rev A) / Ambre `#D9822B` (Rev D) / Argent `#A0AAB8` (Rev G)
  - **Teletronix LA-2A :** Ambre Chaleureux `#D98A2B` & Acier Doux `#8A94A0`
  - **Empirical Labs Distressor :** Rouge Signal `#C93030` & Jaune Ambre `#E0A82E`
  - **Fairchild 660 / 670 (futur) :** Or Vintage `#C59B38` & Cuivre `#9E5834`

