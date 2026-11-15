# Kuprell Hamamatsu 108

Elektronická drnkací harfa: 108 akordů, Live Strum a šest zvukových modelů. Inspirováno japonskými elektronickými drnkacími harfami 80. a 90. let. VST3 a Standalone pro Windows, JUCE 8 (C++17) a CMake.

- Rozhodnutí o funkcích a designu: dokument `claude/hamamatsu81-rozhodnuti.md` v projektu „Kuprell pluginy“
- Návrh GUI: https://claude.ai/artifact/GJQum85YRG9byQS1JKGmAa

## Stav: krok 2 – akordy, harfa, Live Strum

- **Nástroj v okně PLAY:** kovový panel, dřevěné bočnice, stříbrný ovládací pult, tyrkysová světla. Statické části se kreslí jednou do mezipaměti, takže okno se neseká.
- **108 akordových kláves** (12 tónin × 9 typů). Klik akord zapne, druhý klik ho vypne. Akordy v tónině mají tyrkysovou kontrolku.
- **Strumplate:** 26 strun, svítí při drnknutí. Myší se po nich dá přejet.
- **Live Strum:** po zapnutí akordu plugin drnká sám, s proměnlivou silou, rychlostí a časováním. Synchronizace s tempem DAW, rychlost 1/2 až 1/16 (RATE), FEEL (volnost ruky), SPEED (rychlost přejetí strun). OFF drnkání vypne.
- **10 barev harfy** (knob VOICE): Classic Chime, Celesta, Vibes, Glass, Pluck, Flute, Marimba, Steel, E-Piano, Strings. Vše syntézou.
- **Pult:** HARP (Tone, Decay, Range), VIBRATO (Rate, Depth), FX (Chorus, Delay, Reverb), MASTER. CHORD, BASS a MACROS jsou zatím zašedlé (krok 3 a 4).
- **KEY** (klik vybere tóninu), **TRANSPOSE** ±12, **HOLD**, LED měřák OUT.
- **MIDI:** C1–B1 volí základní tón akordu, C2–G#2 typ (C2 dur, C#2 moll, D2 7, D#2 maj7, E2 m7, F2 dim, F#2 aug, G2 sus4, G#2 add9), C3–C6 hraje struny strumplate.
- **Ovládání knobů:** tah myší, kolečko, dvojklik = výchozí hodnota, pravé tlačítko = Reset / zadat hodnotu, Ctrl + tah = jemně. Stavová lišta ukazuje, na co sahám.
- **Okno** se poprvé otevře tak velké, aby se vešlo na obrazovku, jde zmenšit na 50 % a zvětšit na 200 %. Velikost se ukládá s projektem.

## Instalace (Windows)

1. Otevři **Releases** tohoto repozitáře a stáhni nejnovější `Kuprell_Hamamatsu108_…_Windows.exe`.
2. Spusť instalátor. Plugin se nainstaluje do `C:\Program Files\Common Files\VST3\Hamamatsu 108.vst3`, Standalone aplikace do `C:\Program Files\Kuprell\Hamamatsu 108`.
3. Ve **FL Studiu** plugin najdeš jako **Hamamatsu 108** od výrobce **Kuprell**.

## Co otestovat

1. Klikni na akord: plugin hned začne drnkat. Zkus jiný akord, pak klikni na stejný znovu (vypne).
2. Otoč VOICE a poslechni si všech 10 barev.
3. Přejeď myší po strumplate.
4. Pusť přehrávání ve FL: drnkání sedí do tempa. Zkus RATE, FEEL, SPEED.
5. Zahraj z klaviatury: C1–B1 + C2–G#2 akordy, C3–C6 struny.
6. Pravé tlačítko na knobu, dvojklik na knobu, kolečko myši.

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
  source/                  KuprellColours, KuprellLookAndFeel, KuprellTopBar, KuprellKnob
plugins/hamamatsu108/      jen tento plugin
  source/                  PluginProcessor (zvuk, Live Strum), HarpVoice (10 barev harfy),
                           Chords (108 akordů), Instrument (okno PLAY), PluginEditor
installer/                 Inno Setup skript
.github/workflows/         sestavení na GitHubu
```

## Plán

1. Kostra (hotovo)
2. **Akordy, harfa a Live Strum** ← teď
3. Hraní a vrstvy: voice leading a obraty, arpeggio (Chance, Regen), paměť akordů s exportem MIDI, Dual, akordová vrstva, bas
4. Zbytek zvuku: 6 modelů, Morph, makra, 16 efektů v 8 slotech, modulace, stránky EDIT
5. Presety: 144 továrních, prohlížeč, ukládání
6. Doladění: skiny, Settings, About, nápověda, A/B, Undo, výkon
