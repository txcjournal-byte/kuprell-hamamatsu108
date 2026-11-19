# Kuprell Hamamatsu 144

Elektronická drnkací harfa pro dnešní produkci: 144 akordů, okno MOTION (GRID, LINES, RHYTHM, STRINGS), DRAG MIDI do piano rollu. Inspirováno japonskými elektronickými drnkacími harfami 80. a 90. let. VST3 a Standalone pro Windows, JUCE 8 (C++17) a CMake.

- Rozhodnutí o funkcích a designu: dokument `claude/hamamatsu81-rozhodnuti.md` v projektu „Kuprell pluginy“
- Návrh GUI: https://claude.ai/artifact/GJQum85YRG9byQS1JKGmAa

## Stav: verze 0.2.62 – hybridní strum harfa

### Hlavní stránka (PLAY)
- **Horní lišta je v desce:** presety (šipky, seznam, Save, A | B, kostka), Undo/Redo, nápověda, nastavení, EDIT.
- **Akordová tlačítka:** 3 řady (dur, moll, 7) × 12 tónů. Proužek = akord v tónině, tyrkysový = hraje. Pravý klik = další typy (maj7, m7, dim, aug, sus4, add9, sus2, m9, 6/9), Ctrl+klik = skládání. 12 tónů × 12 typů = 144 akordů.
- **MOTION** (okno pod akordy) – jak se akord hraje. Záložky:
  - **GRID:** kostičky, řádky = struny akordu (skutečné tóny), sloupce = kroky.
  - **LINES:** nakreslená čára = prst přes struny v čase (pravé tlačítko = guma).
  - **RHYTHM:** šipky úderů, patterny (3-3-2, Half-Time …), klik na krok = vlastní rytmus.
  - **STRINGS:** každá struna vlastní puls (rychlost, začátek, síla) – polyrytmy.
  - Vedle: **PLAY** (akord hraje dokola bez kláves), **MOTION** (zap/vyp), rychlost kroku, počet kroků, **IDEAS ▾** (hotové vzory, náhoda, posun, převrácení), **CLEAR**, **DRAG MIDI →** (4 takty do piano rollu).
- **VOICE:** displej (klik nahoře = barva zvuku, dole = co dělají klávesy: Split, Notes, Flourish nahoru/dolů …), knoby VOICE, MORPH, RATE, FEEL, SPEED, tlačítka DUAL, HOLD a **FLOURISH** (jedna klávesa = celý melodický běh; pravý klik = nahoru / dolů).
- **MIDI CAPTURE:** nahrává, co zahraješ; přetáhni do piano rollu (menu Detail / Smooth / Reset).
- **KEY / TRANSPOSE**, výstupní metr.
- **Spodní lišta:** HARP (Tone, Range), ENVELOPE (A D S R), FILTER (Cutoff, Reso, Saturation), FX (3 sloty), MACROS (Age, Dream, Wobble), MASTER.

### Klávesy (názvy jako ve FL: střední C = C5)
- **Split** (výchozí): pod C5 levá ruka = akord (jedna klávesa = akord tóniny na daném stupni), od C5 bílé = struny, černé = celý strum.
- **Notes:** přesná výška (melodie z piano rollu). **Flourish:** jedna klávesa = celý běh akordu.
- **STRUM KLÁVESY** (jedna oktáva, výchozí C0 = nejníž v piano rollu, v menu KEYS lze dát C1, C2, C3 podle klávesnice). Každá klávesa je gesto ruky na akordu, který hraje:
  C zespodu · C# zespodu pomalu · D shora · D# shora pomalu · E od středu ven · F krátce dole · F# krátce nahoře · **G = MOTION, dokud držíš** (hraje vzor z okna: kostičky, čáry, rytmus, struny) · G# = MOTION jeden takt · A = tremolo, dokud držíš · A# = glissando nahoru a dolů · B = ztlumit struny. Síla úhozu = síla.
- Myš na PLATE = ruka přes struny.
- Pitch wheel i mod wheel = prst po strunách. PC klávesnice: Q/A/Z řady akordy, čísla struny, `-` `=` strum.

### EDIT (5 záložek)
LAYERS (vrstva A + vrstva B, 14 enginů: 10 harfových + Analog, Wavetable, Digital FM, Air; SHAPE, MORPH, Level, Detune, Octave; STEREO: Spread, Width, Polish; vibrato, obálka, cíle Morphu) · ARP & GATE (arpeggio + kroky, gate + kroky) · FILTER (filtr, saturace, makra, charakter Warm/Classic/Bright/Clean/Lo-fi/Hi-fi) · MOD (2 LFO, Function, Random, matice; pravý klik na knob = Modulate with) · FX (8 slotů, 20 efektů vč. PRISM, DISPERSE, SHAPE, WIDE).

### PRESETS
320 továrních v 8 typech (Harp, Pluck, Bell, Keys, Pad, Texture, Hybrid, Cinematic) vč. String Harp a Epic Harp; klik = hned zazní. Vlastní presety v `Dokumenty/Kuprell/Hamamatsu 144/Presets`.

## Instalace (Windows)

1. Otevři **Releases** tohoto repozitáře a stáhni nejnovější `Kuprell_Hamamatsu144_…_Windows.exe`.
2. Spusť instalátor (Windows se zeptá na povolení: Ano). Plugin se nainstaluje do `C:\Program Files\Common Files\VST3\Hamamatsu 144.vst3`, Standalone do `C:\Program Files\Kuprell\Hamamatsu 144`.
3. Ve FL Studiu: Channel rack → **+** → **Hamamatsu 144**.

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
  source/                  PluginProcessor (zvuk, Live Strum, arp, paměť, modulace), HarpVoice (10 barev harfy),
                           Layers (akordová vrstva, bas), Effects (16 efektů), Modulation (LFO, Function, Random),
                           Chords (108 akordů, voicing), Presets (144 továrních), Skins (10 edic),
                           Instrument (PLAY), EditView (EDIT), BrowserView (PRESETS), Overlays (Settings, About, Help)
third_party/mts-esp/       MTS-ESP klient (ODDSound, volná licence)
installer/                 Inno Setup skript
.github/workflows/         sestavení na GitHubu
```

## Plán

1. Kostra (hotovo)
2. Akordy, harfa a Live Strum (hotovo)
3. Hraní a vrstvy: voice leading, arpeggio, paměť akordů, Dual, akordová vrstva, bas (hotovo)
4. Zbytek zvuku: 6 modelů, Morph, makra, 16 efektů v 8 slotech, modulace, stránky EDIT (hotovo)
5. Presety: 144 továrních, prohlížeč, ukládání (hotovo)
6. Doladění: skiny, Settings, About, nápověda, A/B, Undo, MIDI Learn (hotovo)
7. Test majitelem ve FL Studiu, ladění zvuku a presetů podle poslechu, licence
