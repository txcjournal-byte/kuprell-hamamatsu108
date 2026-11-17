# Kuprell Hamamatsu 108

Elektronická drnkací harfa: 108 akordů, Live Strum a šest zvukových modelů. Inspirováno japonskými elektronickými drnkacími harfami 80. a 90. let. VST3 a Standalone pro Windows, JUCE 8 (C++17) a CMake.

- Rozhodnutí o funkcích a designu: dokument `claude/hamamatsu81-rozhodnuti.md` v projektu „Kuprell pluginy“
- Návrh GUI: https://claude.ai/artifact/GJQum85YRG9byQS1JKGmAa

## Stav: kompletní verze 0.2 (kroky 1–6)

### PLAY (hlavní okno)
- **108 akordových tlačítek** (12 tónin × 9 typů), tečka = akord patří do tóniny. Klik myší akord zapne (zůstane svítit), další klik ho vypne, klik na jiný akord přepne. Pak můžeš brnkat myší po STRUM PLATE. **HOLD** platí pro MIDI klávesy: zapnutý = akord zní i po puštění kláves, vypnutý = jen dokud držíš.
- **Preset:** klik zahraje jeden krátký úder v tónině, nic nepřepíná a nic nenahrává.
- **Strumplate** s 26 strunami (myš), **Live Strum** v tempu FL (RATE, FEEL, SPEED), **ARP** (6 vzorů, CHANCE, REGEN po kliknutí na ARP; arpeggiuje i noty z piano rollu nebo klávesnice mimo akordové zóny, dokud je držíš), **MEMO** (přehrává paměť akordů), **OFF**.
- **VOICE** (10 barev harfy), **MORPH** (prolnutí vrstev A a B), **DUAL** (klik = výběr druhé barvy).
- **MODEL** (klik na displej): Early 81, System 84, Tone Bank 89, Late 90s, Cartridge 99, Modern Strum.
- **Pult:** HARP, CHORD (klik na štítek = Organ / Pad / Strings / Choir a voicing), BASS (Sub / Organ / Synth, Root / Root+5th / Walking), VIBRATO, FX 1–3, MACROS (AGE, DREAM, WOBBLE), MASTER.
- **Paměť akordů** na přední hraně: banky A–H × 16 akordů, REC, délka akordu, DRAG MIDI do FL.
- **MIDI CAPTURE:** zachytí, co plugin zahraje sám (Live Strum, ARP, strumplate), přetáhni do FL.

### EDIT (7 záložek)
Engine (vrstvy A/B, model, akordová vrstva, bas, vibrato, 8 cílů Morphu) · Strum & Arp (náhled ruky a vzoru) · Chords (tónina, voicing, akordy tóniny, klaviatura se zónami) · Memory (8 bank × 16) · Mod (2 LFO, Function s 8 body posuvnými v čase i úrovni: Shift = jemně, dvojklik = reset bodu, pravé tlačítko = menu s tvary; Random; matice 8 slotů; modulace stojí, když nic nehraje) · FX (8 slotů × 16 efektů, přetahování pořadí) · Character (makra, karty modelů).

### PRESETS
144 továrních presetů (Harp, Pluck, Bell, Keys, Pad, Texture; 4 na model), hledání, filtr podle typu a modelu, oblíbené (srdíčko), vlastní presety (Save, Save As, Delete) v `Dokumenty/Kuprell/Hamamatsu 108/Presets`.

### Horní lišta a SETTINGS
Šipky = další/předchozí preset, Save, **A | B** (porovnání dvou nastavení), kostka (Randomize po sekcích), Undo/Redo, **?** (nápověda s čísly), ozubené kolo (**Settings**: 10 skinů, velikost okna, zóny akordů, polyfonie, MIDI Learn, ladění A4, MTS-ESP), logo Kuprell = About.

### Knoby
Tah, kolečko, dvojklik = výchozí hodnota, Ctrl + tah = jemně. Pravé tlačítko: Reset, zadat hodnotu, kopírovat/vložit, **MIDI Learn**, **zamknout** (presety ani kostka hodnotu nezmění).

### MIDI (názvy not jako ve FL Studiu)
C2–B2 základní tón akordu, C3–G#3 typ (C3 dur, C#3 moll, D3 7, D#3 maj7, E3 m7, F3 dim, F#3 aug, G3 sus4, G#3 add9). Ostatní noty hrají harfu přesně v napsané výšce. Zóny jdou posunout nebo vypnout v Settings. CC1 (mod wheel) a aftertouch jsou zdroje modulace.

## Novinky 0.3 – hraní pro producenty

- **LIVE = lidská ruka**: 12 vzorců úderů (Ballad, Folk, Pop 8ths/16ths, Tap, Brush, Fingerpick, Bossa, Reggae, Waltz 3/4, Half Time, Shimmer) + Swing a Variation (ghost údery, vynechání, fill každý 4. takt). Klik na LIVE, když svítí = menu.
- **YOUR KEYS**: Notes (přesná výška) / Strum (stisk kláves = přejezd přes držené noty, síla = rychlost) / Pattern (vzorec hraje akord z piano rollu).
- **Skládání akordů**: Ctrl/Shift+klik na tlačítko akordu přidá další akord (C + Em = Cmaj7, víc = rozšířené akordy); na MIDI více kořenových kláves najednou.
- **MIDI CAPTURE**: pravý klik → Chords (dlouhé noty pod sebou, mřížka 1/16–takt) / Detail / Both.
- **MEMO**: hotové progrese v tónině (Pop, Ballad, Jazz ii-V-I, Lo-fi, City pop, Blues, moll…), klik na prázdný krok = výběr akordu tóniny, hraje vzorcem LIVE.
- **ARP pro**: Octaves, Gate, Swing, nové vzorce Converge/Pinky/Steps a 16krokový řádek NOTE / VELOCITY / OCT / RATCHET / CHANCE / TIE + předvolby.
- **Modulace**: každý LFO / Function / Random má vlastní cíl + Depth (bez matice); presety Pad/Bell/Keys/Texture je používají.
- **Presety**: Save As s vlastním bankem (vlastní kategorie), pravý klik = přejmenovat / přesunout / smazat, filtr bank v MY PRESETS.
- Cartridge 99 bez šumu, okno se otevírá na 85 %, tlačítka Chords in key přepínají.

## Instalace (Windows)

1. Otevři **Releases** tohoto repozitáře a stáhni nejnovější `Kuprell_Hamamatsu108_…_Windows.exe`.
2. Spusť instalátor (Windows se zeptá na povolení: Ano). Plugin se nainstaluje do `C:\Program Files\Common Files\VST3\Hamamatsu 108.vst3`, Standalone do `C:\Program Files\Kuprell\Hamamatsu 108`.
3. Ve FL Studiu: Channel rack → **+** → **Hamamatsu 108**.

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
