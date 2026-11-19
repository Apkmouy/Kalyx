# GEMINI.md - Architecture, Règles Temps Réel & Standards de Développement (JUCE 8 / C++20)

Ce document définit les règles d'ingénierie strictes et l'architecture logicielle du plugin **Kalyx SSL Console Strip** (Tranche de console SSL 4000/9000 Series avec module de coloration harmonique dynamique, compresseur/limiteur, gate/expander, filtres HPF/LPF, égaliseur paramétrique 4 bandes, auto-gain et makeup gain).

---

## 1. Stack Technique & Portabilité (Linux / Windows)

- **Standard C++ :** C++20 strict (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`).
- **Framework :** **JUCE 8** (`8.0.12` / tag stable officiel) récupéré automatiquement via CMake `FetchContent` depuis `https://github.com/juce-framework/JUCE.git`. Aucune dépendance à un chemin local.
- **Formats de sortie :**
  - `VST3` (Windows & Linux)
  - `LV2` (Linux & Windows - indispensable sous Linux avec `LV2URI` explicite)
  - `Standalone` (Test direct sans DAW)
- **Zéro dépendance graphique fragile :**
  - Utilisation exclusive du moteur de rendu vectoriel logiciel standard `juce::Graphics`.
  - **Interdiction absolue** d'activer OpenGL (`juce_opengl`) ou `WebBrowserComponent` (`JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`).
- **Règles de compilation par OS :**
  - **Windows (MSVC) :** Linkage statique du runtime C/C++ (`CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"`) pour garantir un binaire autonome sans erreur de DLL Visual C++ redistribuable manquante.
  - **Linux (GCC/Clang) :** `CMAKE_POSITION_INDEPENDENT_CODE ON` (`-fPIC`) activé globalement sur toutes les cibles. Détection propre des paquets systèmes (`alsa`, `freetype2`, `fontconfig`, `x11`, `xext`, `xrandr`, `xinerama`, `xcursor`) via `pkg-config` (`PkgConfig`). Aucun appel système propriétaire (`windows.h`, API macOS).
  - **Système de fichiers :** Utilisation exclusive de `juce::File` ou `std::filesystem::path` (aucun séparateur `/` ou `\` codé en dur dans le code C++).

---

## 2. Contraintes Temps Réel Strictes (Audio Thread - `processBlock`)

Le thread audio (`processBlock`) est soumis à des contraintes temps réel dures. Toute violation provoque des *xruns* (craquements audio) et est **strictement interdite** :

1. **ZÉRO allocation mémoire dynamique :**
   - Aucun `new`, `delete`, `malloc`, `free`, `std::vector::push_back`, `std::vector::resize`, `std::string`, ou `std::function` allouant sur le tas dans `processBlock()`.
2. **ZÉRO verrouillage (Lock-Free) :**
   - Aucun `std::mutex`, `std::lock_guard`, `juce::CriticalSection`, appel système, accès disque ou réseau dans `processBlock()`.
   - Communication Audio -> GUI exclusivement via `std::atomic<float>` (`std::memory_order_relaxed`).
   - Communication GUI/Host -> Audio via les pointeurs atomiques bruts de `AudioProcessorValueTreeState::getRawParameterValue()`.
3. **Pré-allocation dans `prepareToPlay()` :**
   - Tous les états de filtres biquad, détecteurs d'enveloppe RMS/Peak, buffers internes et suiveurs de gain sont dimensionnés et initialisés dans `prepareToPlay(double sampleRate, int samplesPerBlock)` et réinitialisés dans `releaseResources()` / `reset()`.
4. **Sécurité numérique :**
   - Instanciation obligatoire de `juce::ScopedNoDenormals noDenormals;` en toute première ligne de `processBlock()`.
   - Sanitisation systématique en sortie de chaîne DSP : filtrage de tout `NaN` ou `Inf` (`std::isfinite`) et protection contre les surcharges extrêmes (hard safety clip à $\pm 4.0$, soit $+12\text{ dBFS}$) avant de renvoyer le buffer au DAW.
5. **Lissage des paramètres (Anti-Zipper Noise) :**
   - Tous les paramètres continus contrôlables par l'utilisateur utilisent `juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>` ou `Multiplicative` avec une rampe de lissage adaptée ($10\text{ ms}$ à $20\text{ ms}$) mise à jour par échantillon dans la boucle DSP.

---

## 3. Architecture DSP : Kalyx SSL Console Strip

La chaîne de traitement modélise une tranche de console analogique **SSL 4000 G/E** enrichie d'un étage de **Couleur Harmonique Dynamique** et d'**Auto-Gain** :

1. **Section Channel Input & Filters (HPF / LPF) :**
   - **Input Trim :** $-24\text{ dB}$ à $+24\text{ dB}$ + Inversion de polarité ($\varnothing$ Phase Invert).
   - **High-Pass Filter (HPF) :** Butterworth 2ème ordre ($12\text{ dB/oct}$), réglable de $20\text{ Hz}$ à $500\text{ Hz}$ (avec switch Bypass/Actif).
   - **Low-Pass Filter (LPF) :** Butterworth 2ème ordre ($12\text{ dB/oct}$), réglable de $2\text{ kHz}$ à $20\text{ kHz}$ (avec switch Bypass/Actif).
2. **Section Dynamics (Gate/Expander & Compressor/Limiter SSL Bus/Channel) :**
   - **Gate / Expander :** Seuil ($-70\text{ dB}$ à $0\text{ dB}$), Range ($0\text{ dB}$ à $40\text{ dB}$), Attack rapide, Release doux, hystérésis anti-broutage.
   - **Compressor / Limiter :**
     - Threshold ($-40\text{ dB}$ à $0\text{ dB}$), Ratio ($1:1$ à $20:1$ / mode Limiter $\infty:1$), Attack ($0.5\text{ ms}$ à $50\text{ ms}$), Release ($20\text{ ms}$ à $1000\text{ ms}$), Soft Knee analogique ($3\text{ dB}$).
     - Détecteur RMS/Peak hybride couplé en stéréo (True Stereo Link) pour préserver l'image stéréo.
3. **Section "Color" (Saturation Harmonique Console & VCA) :**
   - **Color Intensity ($0\%$ à $100\%$) :** Modélisation d'étage de console à transformateur et VCA (combinaison d'harmoniques paires asymétriques 2ème ordre et impaires douces 3ème ordre via fonction de transfert saturante compensée en gain, plus filtre d'emphase haute fréquence subtil "Console Air/Warmth").
   - **Color Mode (Static vs Linked to GR) :**
     - *Mode Static (0) :* L'intensité de la coloration est fixe selon le réglage `Color Intensity`.
     - *Mode Comp-Linked (1) :* Le drive de saturation est modulé dynamiquement par le degré de réduction de gain (Gain Reduction, $GR_{\text{dB}}$) du compresseur/limiteur ! Plus le compresseur travaille fort, plus les harmoniques analogiques de console s'enrichissent, exactement comme un VCA/transformateur poussé dans ses retranchements.
4. **Section Parametric Equaliser (SSL 4-Band "Brown/Black Knob" EQ) :**
   - **LF Band (Shelf / Bell) :** $30\text{ Hz}$ à $450\text{ Hz}$, Gain $\pm 15\text{ dB}$ (Low Shelf).
   - **LMF Band (Parametric Bell) :** $200\text{ Hz}$ à $2500\text{ Hz}$, Gain $\pm 15\text{ dB}$, $Q$ réglable ($0.4$ à $4.0$).
   - **HMF Band (Parametric Bell) :** $600\text{ Hz}$ à $7000\text{ Hz}$, Gain $\pm 15\text{ dB}$, $Q$ réglable ($0.4$ à $4.0$).
   - **HF Band (Shelf) :** $1.5\text{ kHz}$ à $18\text{ kHz}$, Gain $\pm 15\text{ dB}$ (High Shelf).
5. **Section Output, Auto-Gain & Makeup Gain :**
   - **Auto-Gain (On/Off) :** Compensation analytique continue du gain basée sur le Threshold et le Ratio du compresseur + compensation d'énergie de la saturation, lissée sans pompage.
   - **Makeup Gain :** $-12\text{ dB}$ à $+24\text{ dB}$.
   - **Output Level & Mix (Dry/Wet) :** Niveau de sortie final ($-24\text{ dB}$ à $+12\text{ dB}$) et Mix parallèle ($0\%$ à $100\%$).

---

## 4. Structure du Dépôt

```text
.
├── GEMINI.md                        # Règles d'architecture et contraintes temps réel
├── CMakeLists.txt                   # Configuration CMake multiplateforme (JUCE 8 FetchContent)
├── scripts/
│   └── dsp_prototype.py             # Simulation mathématique Python (NumPy/SciPy) & tests de stabilité
├── Source/
│   ├── DSP/
│   │   ├── BiquadFilter.h           # Filtres IIR TDF-II stables temps réel sans allocation
│   │   ├── SSLChannelStripDSP.h     # Moteur DSP complet découplé de JUCE
│   │   └── SSLChannelStripDSP.cpp   # Implémentation DSP temps réel
│   ├── PluginProcessor.h            # AudioProcessor + APVTS + atomics de télémétrie
│   ├── PluginProcessor.cpp          # Gestion temps réel, lissage SmoothedValue, sécurité NaN/Inf
│   ├── PluginEditor.h               # Interface vectorielle redimensionnable style console SSL
│   └── PluginEditor.cpp             # Rendu juce::Graphics pur, courbe EQ temps réel & VU-mètres
└── .github/
    └── workflows/
        └── build.yml                # CI/CD GitHub Actions (ubuntu-latest & windows-latest)
```
