# Kuprell Hamamatsu 108

Elektronická drnkací harfa: 108 akordů, Live Strum a šest zvukových modelů. Inspirováno japonskými elektronickými drnkacími harfami 80. a 90. let. VST3 a Standalone pro Windows, JUCE 8 (C++17) a CMake.

- Rozhodnutí o funkcích a designu: dokument `claude/hamamatsu81-rozhodnuti.md` v projektu „Kuprell pluginy“
- Návrh GUI: https://claude.ai/artifact/GJQum85YRG9byQS1JKGmAa

## Stav: krok 1 – kostra

Tahle verze ověřuje, že sestavení a instalace u tebe fungují. Obsahuje:

- **Horní lištu Kuprell:**
  - logo (krémové SVG, 12 px) a název „Hamamatsu 108“,
  - displej presetu se šipkami, Save, A | B, kostku Randomize,
  - Undo, Redo, „?“, Settings a tyrkysové tlačítko EDIT.
  
  Tlačítka zatím jen napíšou, ve kterém kroku přibude jejich funkce.
- **Okno 1280 × 820,** jehož velikost jde měnit tažením rohu (75–200 %, pevný poměr stran).
- **Testovací zvuk:** jednoduchý zvonivý tón na MIDI noty, aby bylo slyšet, že MIDI a audio fungují.
- **MIDI displej:** ukazuje poslední notu a kolik not držíš. Ve stavové liště je měřák výstupu.
- **Ikonu Standalone aplikace:** `kuprell-icon-dark-1024.png`.

## Instalace (Windows)

1. Otevři **Releases** tohoto repozitáře (vpravo na hlavní stránce) a stáhni nejnovější `Kuprell_Hamamatsu108_…_Windows.exe`.
2. Spusť instalátor. Plugin se nainstaluje do `C:\Program Files\Common Files\VST3\Hamamatsu 108.vst3`, Standalone aplikace do `C:\Program Files\Kuprell\Hamamatsu 108`.
3. Ve **FL Studiu** otevři **Options → Manage plugins → Find more plugins**. Plugin najdeš jako **Hamamatsu 108** od výrobce **Kuprell**. Přidej ho do Channel rack jako nástroj.

Bez instalátoru: v Releases je i `Kuprell-Hamamatsu108-Windows.zip`. Složku `Hamamatsu 108.vst3` z něj zkopíruj do `C:\Program Files\Common Files\VST3`.

## Co otestovat

1. Plugin se ve FL Studiu načte a nespadne.
2. Logo a horní lišta jsou ostré a čitelné.
3. Při tažení za pravý dolní roh se okno zmenšuje a zvětšuje a zůstává ostré.
4. Při hraní z MIDI klaviatury (nebo z Piano rollu) zní zvonivý tón, MIDI displej ukazuje notu a měřák OUT se hýbe.
5. Klik na tlačítka v liště vypíše zprávu uprostřed okna.
6. Uložíš projekt, zavřeš FL Studio, znovu otevřeš: plugin se obnoví.

Zpětnou vazbu (co nefunguje, co se nelíbí) napiš do chatu.

## Sestavení (automaticky)

Každý push do `main` spustí GitHub Actions (`.github/workflows/build.yml`) na Windows:

1. CMake stáhne JUCE 8.0.4.
2. Sestaví se VST3 a Standalone.
3. Inno Setup vytvoří instalátor.
4. Všechno se publikuje jako GitHub Release.

Ruční sestavení (Visual Studio 2022 a CMake 3.22 a novější):

```bash
cmake -B build -A x64
cmake --build build --config Release
```

Výstupy jsou v `build/plugins/hamamatsu108/Hamamatsu108_artefacts/Release/`.

## Struktura

```
kuprell-core/              sdílené pro všechny pluginy Kuprell
  assets/brand/            logo (cream, ink) a ikona aplikace
  source/                  KuprellColours, KuprellLookAndFeel, KuprellTopBar
plugins/hamamatsu108/      jen tento plugin
  source/                  PluginProcessor, PluginEditor
installer/                 Inno Setup skript
.github/workflows/         sestavení na GitHubu
```

## Plán

1. **Kostra** ← teď
2. Akordy a harfa: 108 akordových kláves, strumplate, základní zvuk harfy, 3D nástroj
3. MIDI a Live Strum: MIDI režimy, Hold, Live Strum, arpeggio, paměť akordů
4. Zbytek zvuku: akordová vrstva, bas, efekty, modely, Morph, modulace
5. Presety: 144 továrních, prohlížeč, ukládání
6. Doladění: skiny, Settings, About, nápověda, A/B, Undo, výkon
