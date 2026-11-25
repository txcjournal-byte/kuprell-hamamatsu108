#include "ConsoleView.h"
#include "ConsoleStyle.h"
#include "ConsoleLayout.h"
#include "HarpArt.h"
#include "Chords.h"
#include "Presets.h"
#include "KuprellLookAndFeel.h"

namespace hama
{
    namespace
    {
        constexpr float designW = 1600.0f, designH = 920.0f;
        const juce::Colour mint (0xff72f2bd), bone (0xfff4f2ec), ledRed (0xffee2a35);

        juce::RangedAudioParameter* P (Hamamatsu108Processor& p, const char* id) { return p.apvts.getParameter (id); }
        float plainOf (Hamamatsu108Processor& p, const char* id)
        {
            if (auto* v = p.apvts.getRawParameterValue (id)) return v->load();
            return 0.0f;
        }
        void setPlain (Hamamatsu108Processor& p, const char* id, float v)
        {
            if (auto* q = P (p, id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); }
        }
        juce::Font ui (float h, bool bold = true) { return kuprell::LookAndFeel::uiFont (h, bold); }
    }

    // one control of the console: where it is (design pixels) and what it does
    struct ConsoleView::Control
    {
        const console::Def* def = nullptr;
        juce::Rectangle<float> r;
        juce::String param;                        // faders, knobs: the parameter it moves
        std::function<bool()> lit;                 // keys: LED on?
        std::function<void()> press;               // keys: what a click does
        std::function<int()> selected;             // LED lists: which LED is on (-1 = none)
        std::function<void (int)> pick;            // LED lists: click on an item
        std::function<juce::String()> text;        // screens
        juce::String tip;
        int spare = -1;                            // no sound yet: the position is kept in the processor
        bool live() const { return param.isNotEmpty() || spare >= 0 || press != nullptr || pick != nullptr || text != nullptr; }
    };

    namespace
    {
        float valueOf (Hamamatsu108Processor& pr, const ConsoleView::Control& c);
        void setValueOf (Hamamatsu108Processor& pr, ConsoleView::Control& c, float v);
        void gesture (Hamamatsu108Processor& pr, ConsoleView::Control& c, bool begin);
    }

    ConsoleView::ConsoleView (Hamamatsu108Processor& p) : processor (p)
    {
        background = juce::ImageCache::getFromMemory (HarpArt::console_bg_jpg, HarpArt::console_bg_jpgSize);
        knobStrip  = juce::ImageCache::getFromMemory (HarpArt::knob_strip_png, HarpArt::knob_strip_pngSize);
        capLight   = juce::ImageCache::getFromMemory (HarpArt::fadercap_png, HarpArt::fadercap_pngSize);
        capDark    = juce::ImageCache::getFromMemory (HarpArt::fadercap_dark_png, HarpArt::fadercap_dark_pngSize);
        for (int i = 0; i < console::numDefs; ++i)
        {
            auto c = std::make_unique<Control>();
            c->def = &console::defs[i];
            c->r = { c->def->x, c->def->y, c->def->w, c->def->h };
            controls.push_back (std::move (c));
        }
        bindAll();
        setOpaque (true);
        startTimerHz (30);
    }

    ConsoleView::~ConsoleView() = default;

    void ConsoleView::bindAll()
    {
        auto& pr = processor;
        auto find = [this] (const char* sec, const char* label, const char* kind = nullptr) -> Control*
        {
            for (auto& c : controls)
                if (juce::String (c->def->section).equalsIgnoreCase (sec) && juce::String (c->def->label).equalsIgnoreCase (label)
                    && (kind == nullptr || juce::String (c->def->kind) == kind))
                    return c.get();
            return nullptr;
        };
        auto par = [&] (const char* sec, const char* label, const char* id, const char* tip, const char* kind = nullptr)
        {
            if (auto* c = find (sec, label, kind)) { c->param = id; c->tip = tip; }
        };
        auto toggle = [&] (const char* sec, const char* label, const char* id, const char* tip)
        {
            if (auto* c = find (sec, label, "button"))
            {
                const juce::String pid (id);
                c->lit = [&pr, pid] { return plainOf (pr, pid.toRawUTF8()) > 0.5f; };
                c->press = [&pr, pid] { setPlain (pr, pid.toRawUTF8(), plainOf (pr, pid.toRawUTF8()) > 0.5f ? 0.0f : 1.0f); };
                c->tip = tip;
            }
        };
        auto choice = [&] (const char* sec, const char* label, const char* id, int offset, const char* tip)
        {
            // an LED list and its select key both pick from a choice parameter
            const juce::String pid (id);
            auto* list = find (sec, label, "leds");
            auto* key = find (sec, (juce::String (label) + " select").toRawUTF8(), "button");
            const int n = list != nullptr ? list->def->numItems : 0;
            if (list != nullptr)
            {
                list->selected = [&pr, pid, offset, n] { const int v = juce::roundToInt (plainOf (pr, pid.toRawUTF8())) - offset; return v >= 0 && v < n ? v : -1; };
                list->pick = [&pr, pid, offset] (int i) { setPlain (pr, pid.toRawUTF8(), (float) (i + offset)); };
                list->tip = tip;
            }
            if (key != nullptr)
            {
                key->press = [&pr, pid, offset, n] { const int v = juce::roundToInt (plainOf (pr, pid.toRawUTF8())) - offset; setPlain (pr, pid.toRawUTF8(), (float) (((v + 1) % juce::jmax (1, n)) + offset)); };
                key->tip = tip;
            }
        };

        // VOICE: the layers
        if (auto* c = find ("Voice", "Harp A", "button")) { c->lit = [] { return true; }; c->press = [] {}; c->tip = "Layer A: the main harp (always on)"; }
        toggle ("Voice", "Harp B", "dual", "Layer B: a second engine under the harp, MORPH blends A and B");
        if (auto* c = find ("Voice", "Bass", "button"))
        {
            c->lit = [&pr] { return plainOf (pr, "bassType") > 0.5f; };
            c->press = [&pr] { setPlain (pr, "bassType", plainOf (pr, "bassType") > 0.5f ? 0.0f : 1.0f); };
            c->tip = "Bass layer: plays the root of the chord";
        }
        if (auto* c = find ("Voice", "Chord", "button"))
        {
            c->lit = [&pr] { return plainOf (pr, "chordType") > 0.5f; };
            c->press = [&pr] { setPlain (pr, "chordType", plainOf (pr, "chordType") > 0.5f ? 0.0f : 1.0f); };
            c->tip = "Chord layer: a sustained chord under the harp";
        }
        par ("Morph", "Glass", "morph", "MORPH: blends layer A into layer B");
        par ("Morph", "Steel", "bLevel", "Layer B level");
        par ("Morph", "Air", "polish", "Air: polish and sparkle on top");

        // LFOs
        for (int n = 1; n <= 2; ++n)
        {
            const char* sec = n == 1 ? "LFO 1" : "LFO 2";
            const juce::String id ("lfo" + juce::String (n));
            par (sec, "Rate", (id + "Rate").toRawUTF8(), "LFO rate");
            choice (sec, "Waveform", (id + "Shape").toRawUTF8(), 0, "LFO waveform: sine, triangle, saw, square, random");
            if (auto* c = find (sec, "Sync", "button"))
            {
                const juce::String pid = id + "Sync";
                c->lit = [&pr, pid] { return plainOf (pr, pid.toRawUTF8()) > 0.5f; };
                c->press = [&pr, pid] { setPlain (pr, pid.toRawUTF8(), plainOf (pr, pid.toRawUTF8()) > 0.5f ? 0.0f : 5.0f); };   // free / 1/4
                c->tip = "LFO follows the song tempo";
            }
        }
        par ("LFO 2", "Depth", "lfo2QAmt", "LFO 2 depth", "knob");
        for (int n = 1; n <= 2; ++n)
        {
            const char* sec = n == 1 ? "LFO 1" : "LFO 2";
            const juce::String id ("lfo" + juce::String (n));
            par (sec, "Delay", (id + "Delay").toRawUTF8(), "DELAY: the LFO waits after each key");
            par (sec, "Fade", (id + "Fade").toRawUTF8(), "FADE: then it grows in");
            choice (sec, "Offset", (id + "Offset").toRawUTF8(), 0, "OFFSET: the wave above zero, around it, or below");
            // KEY TRIG: one LED, its key switches it on and off
            const juce::String tid = id + "Trig";
            if (auto* list = find (sec, "Keytrig", "leds"))
            {
                list->selected = [&pr, tid] { return plainOf (pr, tid.toRawUTF8()) > 0.5f ? 0 : -1; };
                list->pick = [&pr, tid] (int) { setPlain (pr, tid.toRawUTF8(), plainOf (pr, tid.toRawUTF8()) > 0.5f ? 0.0f : 1.0f); };
                list->tip = "KEY TRIG: the wave starts again with every key";
            }
            if (auto* key = find (sec, "Keytrig select", "button"))
            {
                key->press = [&pr, tid] { setPlain (pr, tid.toRawUTF8(), plainOf (pr, tid.toRawUTF8()) > 0.5f ? 0.0f : 1.0f); };
                key->tip = "KEY TRIG on / off";
            }
        }
        // where the LFOs go: the pitch (STRING Mod + Amt) and the cutoff (FILTER LFO sel + LFO)
        choice ("String", "Mod", "pitchModSrc", 0, "Which LFO bends the pitch (Env: the function envelope)");
        par ("String", "Amt", "pitchModAmt", "Pitch modulation: vibrato, up to 3 semitones");
        choice ("Filter", "LFO sel", "filtLfoSel", 0, "Which LFO moves the filter");
        par ("Filter", "LFO", "filtLfoAmt", "How much the LFO moves the filter");

        // AMP ENV
        par ("Amp env", "Atk", "harpAttack", "Attack: how the string starts");
        par ("Amp env", "Dec", "harpDecay", "Decay: how long the string rings");
        par ("Amp env", "Sus", "harpSustain", "Sustain: how long it holds on");
        par ("Amp env", "Rel", "harpRelease", "Release: after you let go of the key");

        // BODY and the touch (HARP STRING engine)
        choice ("Body", "Material", "bodyMat", 0, "Body material: wood, glass, steel, stone");
        par ("Body", "Size", "bodySize", "Body size: small and high .. big and low");
        par ("Body", "Damp", "bodyDamp", "Body damping");
        par ("Body", "Ring", "bodyRing", "How long the body rings");
        par ("Body", "Mix", "bodyMix", "How much of the body you hear");
        par ("Pluck env", "Hard", "strHard", "Hardness of the touch");
        par ("Pluck env", "Pos", "strPos", "Where the string is touched: the end (thin) .. the middle (round)");

        // HAND
        if (auto* c = find ("Hand", "Arp", "button"))
        {
            c->lit = [&pr] { return juce::roundToInt (plainOf (pr, "strumMode")) == 1; };
            c->press = [&pr]
            {
                const bool on = juce::roundToInt (plainOf (pr, "strumMode")) != 1;
                setPlain (pr, "strumMode", on ? 1.0f : 3.0f);
                if (on) { setPlain (pr, "gridOn", 0.0f); const int k = juce::roundToInt (plainOf (pr, "keysPlay")); if (k == 4 || k == 5) setPlain (pr, "keysPlay", 0.0f); }   // ARP and FLOURISH take turns
            };
            c->tip = "ARP: the held chord plays note by note";
        }
        toggle ("Hand", "Gate", "gateOn", "GATE: the sound is chopped in rhythm");
        if (auto* c = find ("Hand", "Flourish", "button"))
        {
            c->lit = [&pr] { const int k = juce::roundToInt (plainOf (pr, "keysPlay")); return k == 4 || k == 5; };
            c->press = [&pr]
            {
                const int k = juce::roundToInt (plainOf (pr, "keysPlay"));
                const bool on = ! (k == 4 || k == 5);
                setPlain (pr, "keysPlay", on ? 4.0f : 0.0f);
                if (on && juce::roundToInt (plainOf (pr, "strumMode")) == 1) setPlain (pr, "strumMode", 3.0f);   // the arp would swallow the run
            };
            c->tip = "FLOURISH: one key plays a whole run";
        }
        if (auto* c = find ("Hand", "Notes", "button"))
        {
            c->lit = [&pr] { return juce::roundToInt (plainOf (pr, "keysPlay")) == 0; };
            c->press = [&pr] { setPlain (pr, "keysPlay", 0.0f); };
            c->tip = "NOTES: every key plays its own note";
        }
        choice ("Hand", "Direction", "arpPattern", 0, "Direction of the arp: up, down, up and down, thumb");
        par ("Hand", "Speed", "strumSpeed", "Speed of the hand across the strings");
        par ("Hand", "Swing", "arpSwing", "Swing");
        par ("Hand", "Feel", "strumFeel", "Feel: how hard and even the hand is");
        par ("Hand", "Human", "strumVary", "Human: small differences in time and touch");
        par ("Hand", "Octave", "arpOctaves", "Arp octaves", "knob");
        par ("Hand", "Accent", "arpVelScale", "Accent: dynamics of the arp", "knob");

        // STRING
        par ("String", "Tone", "harpTone", "Tone of the string");
        par ("String", "Stiff", "harpShape", "Stiffness: the character of the engine");
        par ("String", "Detune", "bDetune", "Detune of layer B");
        par ("String", "Width", "width", "Stereo width");
        par ("String", "Coarse", "harpOctave", "Octave", "knob");
        if (auto* c = find ("String", "Exciter", "screen"))
        {
            c->text = [&pr]
            {
                auto* q = P (pr, "harpVoice");
                juce::String t = q != nullptr ? q->getCurrentValueAsText() : juce::String();
                if (juce::roundToInt (plainOf (pr, "harpVoice")) == 14) if (auto* x = P (pr, "strExciter")) t = "Harp: " + x->getCurrentValueAsText();
                return t;
            };
            c->press = [this] {
                auto* q = P (processor, "harpVoice");
                if (q == nullptr) return;
                juce::PopupMenu m;
                const int cur = juce::roundToInt (plainOf (processor, "harpVoice"));
                const int touch = juce::roundToInt (plainOf (processor, "strExciter"));
                static const char* touches[] = { "Finger", "Nail", "Pick", "Felt", "Bow" };
                m.addSectionHeader ("HARP STRING: the touch");
                for (int i = 0; i < 5; ++i) m.addItem (200 + i, touches[i], true, cur == 14 && touch == i);
                m.addSectionHeader ("Other engines");
                for (int i = 0; i < q->getNumSteps(); ++i) if (i != 14) m.addItem (i + 1, q->getText (q->convertTo0to1 ((float) i), 40), true, i == cur);
                juce::Component::SafePointer<ConsoleView> safe (this);
                m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
                {
                    if (safe == nullptr || r <= 0) return;
                    if (r >= 200) { setPlain (safe->processor, "harpVoice", 14.0f); setPlain (safe->processor, "strExciter", (float) (r - 200)); }
                    else setPlain (safe->processor, "harpVoice", (float) (r - 1));
                });
            };
            c->tip = "The exciter: which engine plucks the string (click to choose)";
        }

        // FILTER
        par ("Filter", "Freq", "filterCutoff", "Filter cutoff");
        par ("Filter", "Reso", "filterReso", "Filter resonance");
        par ("Filter", "Drive", "drive", "Drive: saturation", "knob");
        par ("Filter", "Spread", "spread", "Stereo spread of the strings", "knob");

        // SPACE
        toggle ("Space", "Freeze", "sw3On", "FREEZE: the sound hangs in the air");
        toggle ("Space", "Shimmer", "sw13On", "SHIMMER: an octave of light on the reverb");

        // everything else on the panel, now with its sound
        par ("Morph", "Wood", "morphWood", "WOOD: a warm low body and soft saturation");
        par ("Pluck env", "Velo", "pVelo", "How much the key velocity changes the pluck envelope");
        {
            const char* lab[] = { "T1", "L1", "T2", "L2", "T3", "L3", "T4", "Tail" };
            const char* ids[] = { "pT1", "pL1", "pT2", "pL2", "pT3", "pL3", "pT4", "pTail" };
            for (int i = 0; i < 8; ++i) par ("Pluck env", lab[i], ids[i], "Pluck envelope: a filter that opens and closes with each note (T = time, L = level)");
        }
        par ("Body", "Tilt", "bodyTilt", "TILT: darker <> brighter");
        par ("Amp env", "Level", "ampLevel", "Level of layer A");
        par ("Amp env", "Velo", "ampVelo", "Velocity: + more touch sensitive, - more even");
        par ("Amp env", "Hold", "ampHold", "HOLD: full level for a moment before the decay");
        par ("Filter", "Key", "filtKey", "Key follow of the filter (with TRACK on)");
        choice ("Filter", "Mode", "filtMode", 0, "Filter mode: high-pass, band-pass, low-pass");
        toggle ("Filter", "24 dB", "filt24", "24 dB: a steeper filter");
        toggle ("Filter", "Track", "filtTrack", "TRACK: the filter follows the notes");
        par ("Filter", "Velo", "filtVelo", "Velocity opens or closes the filter", "knob");
        choice ("Space", "Room", "spRoom", 0, "The room: hall, plate, cave, void");
        par ("Space", "Size", "spSize", "Size of the room");
        par ("Space", "Tone", "spTone", "Tone of the room: dark .. bright");
        par ("Space", "Pre", "spPre", "Pre-delay: the distance to the walls");
        par ("Space", "Mix", "spMix", "How much room");
        par ("Space", "Width", "spWidth", "Width of the room", "knob");
        par ("Space", "Duck", "spDuck", "DUCK: the room steps back while you play", "knob");
        par ("Space", "Mod", "spMod", "Movement in the room", "knob");
        par ("Space", "Low cut", "spLowcut", "Low cut: a cleaner room", "knob");
        toggle ("Space", "Reverse", "spReverse", "REVERSE: the room swells in backwards");
        toggle ("Space", "Tail", "spTail", "TAIL: the room rings out after you stop (off = it stops soon)");
        par ("Hand", "Ratchet", "ratchet", "RATCHET: every string is struck again, fast", "knob");
        toggle ("Hand", "Latch", "hold", "LATCH: keeps playing after you let go");
        toggle ("Hand", "Retrig", "strumRestart", "RETRIG: the pattern starts again with each chord");
        par ("LFO 2", "Phase", "lfo2Phase", "Phase of LFO 2 against LFO 1", "knob");
        par ("LFO 2", "Smooth", "lfo2Smooth", "Smooth the wave of LFO 2", "knob");
        toggle ("LFO 2", "Tempo", "lfo2Bar", "TEMPO: LFO 2 starts again on every bar");
        toggle ("LFO 2", "Invert", "lfo2Invert", "INVERT: LFO 2 upside down");
        par ("String", "Fine", "strFine", "Fine tune in cents", "knob");
        par ("String", "Sympath", "strSympath", "SYMPATHETIC: the other strings ring along", "knob");
        par ("String", "Buzz", "strBuzz", "BUZZ: a buzzing bridge", "knob");
        par ("String", "Harmonic", "strHarmonic", "HARMONIC: the octave flageolet", "knob");
        par ("String", "Release", "strRelTime", "How fast the damper stops the string", "knob");
        toggle ("String", "Damper", "strDamper", "DAMPER: the string stops when the key goes up (off = it rings out)");
        toggle ("String", "Pedal", "strPedal", "PEDAL: like a sustain pedal held down");
        toggle ("Voice", "Active Harp A", "soloA", "Solo layer A");
        toggle ("Voice", "Active Harp B", "soloB", "Solo layer B");
        toggle ("Voice", "Active Bass", "soloBass", "Solo the bass");
        toggle ("Voice", "Active Chord", "soloChord", "Solo the chord");
        toggle ("CONSOLE", "Freeze", "sw3On", "FREEZE");
        toggle ("CONSOLE", "Mono", "mono", "MONO: one string at a time");

        // CONSOLE
        par ("CONSOLE", "Volume", "master", "Volume");
        par ("CONSOLE", "Speed", "strumSpeedMul", "Pattern speed", "hfader");
        toggle ("CONSOLE", "Hold", "hold", "HOLD: the chord stays after you let go");
        if (auto* c = find ("CONSOLE", "Menu", "button")) { c->press = [this] { if (onMenu) onMenu(); }; c->tip = "Sounds: the preset browser"; }
        if (auto* c = find ("CONSOLE", "Setup", "button")) { c->press = [this] { if (onSetup) onSetup(); }; c->tip = "Setup: MIDI, tuning, window size"; }
        if (auto* c = find ("CONSOLE", "Up", "button")) { c->press = [this] { if (onPrev) onPrev(); }; c->tip = "Previous sound"; }
        if (auto* c = find ("CONSOLE", "Down", "button")) { c->press = [this] { if (onNext) onNext(); }; c->tip = "Next sound"; }
        if (auto* c = find ("CONSOLE", "Sound", "button")) { c->lit = [this] { return ! effectsOpen; }; c->press = [this] { if (effectsOpen && onEffects) onEffects(); }; c->tip = "PANEL: the sound"; }
        if (auto* c = find ("CONSOLE", "Effects", "button")) { c->lit = [this] { return effectsOpen; }; c->press = [this] { if (onEffects) onEffects(); }; c->tip = "PANEL: the effects"; }
        if (auto* c = find ("CONSOLE", "Preset", "screen"))
        {
            c->text = [&pr] { return pr.presetManager->currentName(); };
            c->press = [this] { if (onMenu) onMenu(); };
            c->tip = "Click: all sounds";
        }
        if (auto* c = find ("CONSOLE", "Status", "screen")) c->text = [] { return juce::String(); };

        // STRUM: what the strings hold
        if (auto* c = find ("CONSOLE", "Chord", "button")) { c->lit = [&pr] { return pr.stringsMode.load() == 0; }; c->press = [&pr] { pr.stringsMode.store (0); }; c->tip = "CHORD: the strings hold the notes of the chord"; }
        if (auto* c = find ("CONSOLE", "Scale", "button")) { c->lit = [&pr] { return pr.stringsMode.load() == 1; }; c->press = [&pr] { pr.stringsMode.store (1); }; c->tip = "SCALE: the strings hold every note of the key"; }
        if (auto* c = find ("CONSOLE", "Notes", "button")) { c->lit = [&pr] { return pr.stringsMode.load() == 2; }; c->press = [&pr] { pr.stringsMode.store (2); }; c->tip = "NOTES: the strings hold only the notes you hold"; }
        // STRINGS
        if (auto* c = find ("STRINGS", "Tone | Bend", "button")) { c->lit = [this] { return ! keysView && bedLayer == 0; }; c->press = [this] { keysView = false; bedLayer = 0; }; c->tip = "Handles: bottom = where the string is plucked, top = how slack it is"; }
        if (auto* c = find ("STRINGS", "Time | Level", "button")) { c->lit = [this] { return ! keysView && bedLayer == 1; }; c->press = [this] { keysView = false; bedLayer = 1; }; c->tip = "Handles: bottom = how late the string comes in the strum, top = its level"; }
        if (auto* c = find ("STRINGS", "Keys", "button"))
        {
            c->lit = [this] { return keysView; };
            c->press = [this] { if (heldKey >= 0) processor.keyboardState.noteOff (1, heldKey, 0.0f); heldKey = -1; keysView = true; };
            c->tip = "KEYS: the keyboard (TONE | BEND and TIME | LEVEL open the strings, FLOURISH the steps)";
        }
        if (auto* c = find ("STRINGS", "Rec", "button"))
        {
            // REC on: a new take starts, everything you play is kept; REC off: the take waits under DRAG MIDI
            c->lit = [this] { return takeArmed && takeArmed() && ((juce::Time::getMillisecondCounter() / 400) % 2 == 0 || (takeLive && takeLive())); };
            c->press = [this] { if (onArmTake && takeArmed) onArmTake (! takeArmed()); };
            c->tip = "REC: click, play, click again. Then drag the take from DRAG MIDI";
        }
        if (auto* c = find ("STRINGS", "Drag MIDI", "button"))
        {
            c->lit = [this] { return takeCount && takeCount() > 0 && ! (takeArmed && takeArmed()); };
            c->press = [] {};
            c->tip = "Drag the take into your DAW as a MIDI clip (right-click: clean or exact timing)";
        }
        if (auto* c = find ("STRINGS", "MIDI out", "button"))
        {
            c->lit = [&pr] { return pr.midiOut.load(); };
            c->press = [&pr] { pr.midiOut.store (! pr.midiOut.load()); };
            c->tip = "MIDI OUT: the plugin sends what it plays (record it on a MIDI track)";
        }

        // everything else keeps its position until its sound arrives
        auto& spare = pr.consoleSpare;
        for (int i = 0; i < (int) controls.size() && i < Hamamatsu108Processor::numSpare; ++i)
        {
            auto& c = *controls[(size_t) i];
            const juce::String kind (c.def->kind);
            if (kind == "fader" || kind == "knob" || kind == "hfader")
            {
                if (c.param.isEmpty()) { c.spare = i; c.tip = juce::String (c.def->section) + " " + c.def->label + " (sound in the next step)"; }
            }
            else if (kind == "button" && c.press == nullptr && c.lit == nullptr)
            {
                c.lit = [&spare, i] { return spare[(size_t) i].load() > 0.5f; };
                c.press = [&spare, i] { spare[(size_t) i].store (spare[(size_t) i].load() > 0.5f ? 0.0f : 1.0f); };
                c.tip = juce::String (c.def->section) + " " + c.def->label + " (sound in the next step)";
            }
            else if (kind == "leds" && c.pick == nullptr)
            {
                const int n = c.def->numItems;
                c.selected = [&spare, i] { return juce::jmax (0, juce::roundToInt (spare[(size_t) i].load())); };
                c.pick = [&spare, i] (int k) { spare[(size_t) i].store ((float) k); };
                c.tip = juce::String (c.def->section) + " " + c.def->label + " (sound in the next step)";
                // its select key steps through the LEDs
                for (auto& kp : controls)
                    if (juce::String (kp->def->kind) == "button" && juce::String (kp->def->section) == c.def->section
                        && juce::String (kp->def->label) == juce::String (c.def->label) + " select")
                    {
                        kp->lit = nullptr;
                        kp->press = [&spare, i, n] { spare[(size_t) i].store ((float) ((juce::jmax (0, juce::roundToInt (spare[(size_t) i].load())) + 1) % juce::jmax (1, n))); };
                    }
            }
        }
    }

    //==============================================================================
    juce::Point<float> ConsoleView::toDesign (juce::Point<float> p) const
    {
        return { p.x * designW / (float) juce::jmax (1, getWidth()), p.y * designH / (float) juce::jmax (1, getHeight()) };
    }

    int ConsoleView::hitControl (juce::Point<float> d) const
    {
        for (int i = 0; i < (int) controls.size(); ++i)
        {
            auto& c = *controls[(size_t) i];
            if (! c.live()) continue;
            const juce::String kind (c.def->kind);
            auto r = c.r;
            if (kind == "leds")
            {
                for (int k = 0; k < c.def->numItems; ++k)
                {
                    const auto& it = c.def->items[k];
                    if (juce::Rectangle<float> (it.x - 4.0f, it.y - 3.0f, 40.0f, it.h + 6.0f).contains (d)) return i;
                }
                continue;
            }
            if (kind == "strings") continue;
            if (r.expanded (kind == "fader" ? 4.0f : 2.0f).contains (d)) return i;
        }
        return -1;
    }

    //==============================================================================
    void ConsoleView::paint (juce::Graphics& g)
    {
        const float sx = (float) getWidth() / designW, sy = (float) getHeight() / designH;
        {
            // the big rendered panel is scaled once per window size, not on every repaint
            const float ps = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
            const int iw = juce::roundToInt ((float) getWidth() * ps), ih = juce::roundToInt ((float) getHeight() * ps);
            if (bgScaled.getWidth() != iw || bgScaled.getHeight() != ih)
            {
                bgScaled = juce::Image (juce::Image::RGB, juce::jmax (1, iw), juce::jmax (1, ih), false);
                juce::Graphics bg (bgScaled);
                bg.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                bg.drawImage (background, bgScaled.getBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
            }
            g.drawImage (bgScaled, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        }
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::scale (sx, sy));

        auto ledGlow = [&g] (juce::Point<float> c, float rad)
        {
            juce::ColourGradient glowG (ledRed.withAlpha (0.55f), c.x, c.y, ledRed.withAlpha (0.0f), c.x + rad * 3.2f, c.y, true);
            g.setGradientFill (glowG);
            g.fillEllipse (juce::Rectangle<float> (rad * 6.4f, rad * 6.4f).withCentre (c));
            juce::ColourGradient dome (juce::Colour (0xffffc2b8), c.x - rad * 0.3f, c.y - rad * 0.4f, juce::Colour (0xff9d0b14), c.x + rad, c.y + rad, true);
            dome.addColour (0.45, ledRed);
            g.setGradientFill (dome);
            g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
        };

        for (auto& cp : controls)
        {
            auto& c = *cp;
            const juce::String kind (c.def->kind);
            const auto r = c.r;
            if (kind == "fader")
            {
                const float v = valueOf (processor, c);
                const float capH = 18.0f, travel = r.getHeight() - capH;
                const float cy = r.getY() + (1.0f - v) * travel;
                const bool dark = r.getY() > 270.0f;   // the lower tier is dark graphite
                auto& img = dark ? capDark : capLight;
                // the sprite is 64 x 96 render pixels with the cap at (8, 30, 48, 36): 2 render px = 1 design px
                const float scale = 0.5f;
                g.drawImage (img, juce::Rectangle<float> (r.getCentreX() - 32.0f * scale - 1.0f, cy - 30.0f * scale, 64.0f * scale, 96.0f * scale), juce::RectanglePlacement::stretchToFit);
            }
            else if (kind == "hfader")
            {
                const float v = valueOf (processor, c);
                auto cap = juce::Rectangle<float> (18.0f, 24.0f).withCentre ({ r.getX() + 9.0f + v * (r.getWidth() - 18.0f), r.getCentreY() });
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRoundedRectangle (cap.translated (1.5f, 3.0f), 2.0f);
                juce::ColourGradient cg (juce::Colour (0xff45433f), 0.0f, cap.getY(), juce::Colour (0xff0a0a0a), 0.0f, cap.getBottom(), false);
                g.setGradientFill (cg);
                g.fillRoundedRectangle (cap, 2.0f);
                g.setColour (bone);
                g.fillRect (cap.getCentreX() - 1.0f, cap.getY() + 2.0f, 2.0f, cap.getHeight() - 4.0f);
            }
            else if (kind == "knob")
            {
                const float v = valueOf (processor, c);
                const int frames = knobStrip.getHeight() / knobStrip.getWidth();
                const int f = juce::jlimit (0, frames - 1, juce::roundToInt (v * (float) (frames - 1)));
                const float side = 84.0f * 0.5f;   // 84 render px, the knob 60 of them
                // filmstrip frame
                g.drawImage (knobStrip, (int) (r.getCentreX() - side * 0.5f), (int) (r.getCentreY() - side * 0.5f), (int) side, (int) side,
                             0, f * knobStrip.getWidth(), knobStrip.getWidth(), knobStrip.getWidth());
            }
            else if (kind == "button")
            {
                const juce::String label (c.def->label);
                if (label == "Up" || label == "Down")   // the arrows, cut into the keys
                {
                    auto k = r.reduced (r.getWidth() * 0.27f, r.getHeight() * 0.3f).translated (0.0f, -1.0f);
                    juce::Path tri;
                    if (label == "Up") tri.addTriangle (k.getCentreX(), k.getY(), k.getRight(), k.getBottom(), k.getX(), k.getBottom());
                    else tri.addTriangle (k.getX(), k.getY(), k.getRight(), k.getY(), k.getCentreX(), k.getBottom());
                    g.setColour (juce::Colours::white.withAlpha (0.18f));
                    g.fillPath (tri, juce::AffineTransform::translation (0.0f, 1.2f));
                    g.setColour (juce::Colour (0xff070707));
                    g.fillPath (tri);
                    continue;
                }
                if (c.lit && c.lit())
                {
                    const float rad = r.getHeight() > 24.0f ? 4.0f : 3.0f;
                    ledGlow (r.getCentre(), rad);
                }
            }
            else if (kind == "leds" && c.selected)
            {
                const int s = c.selected();
                if (s >= 0 && s < c.def->numItems)
                {
                    const auto& it = c.def->items[s];
                    ledGlow ({ it.x + it.w * 0.5f, it.y + it.h * 0.5f }, 4.0f);
                }
            }
            else if (kind == "screen" && c.text)
            {
                const juce::String label (c.def->label);
                g.setColour (mint);
                if (label == "Exciter")
                {
                    g.setFont (ui (10.5f));
                    g.drawText (c.text(), r.reduced (6.0f, 0.0f), juce::Justification::centredLeft, true);
                }
                else if (label == "Preset")
                {
                    auto a = r.reduced (10.0f, 7.0f);
                    g.setFont (ui (11.0f));
                    const int idx = processor.presetManager->getCurrent();
                    g.drawText (processor.presetManager->currentCategory() + " " + juce::String (idx + 1).paddedLeft ('0', 3), a.removeFromTop (14.0f), juce::Justification::centredLeft, false);
                    g.drawText ("A=" + juce::String (processor.tuningA4.load(), 1) + "Hz", a.withHeight (14.0f).translated (0.0f, -14.0f), juce::Justification::centredRight, false);
                    g.setFont (ui (21.0f));
                    g.drawText (c.text(), a.removeFromTop (34.0f), juce::Justification::centredLeft, true);
                    g.setFont (ui (10.5f));
                    const int root = juce::roundToInt (plainOf (processor, "keyRoot"));
                    const bool minor = plainOf (processor, "keyMode") > 0.5f;
                    g.drawText (juce::String (chords::rootName (root)) + (minor ? " minor" : " major"), a, juce::Justification::bottomLeft, false);
                    auto* rate = P (processor, "strumRate");
                    const bool arp = juce::roundToInt (plainOf (processor, "strumMode")) == 1;
                    g.drawText ((rate != nullptr ? rate->getCurrentValueAsText() : juce::String()) + (arp ? " Arp" : " Strum"), a, juce::Justification::bottomRight, false);
                }
                else if (label == "Status")
                {
                    auto a = r.reduced (10.0f, 7.0f);
                    g.setFont (ui (11.0f));
                    const bool armed = takeArmed && takeArmed();
                    const int count = takeCount ? takeCount() : 0;
                    g.drawText (armed ? "REC  " + juce::String (count) + " notes" : count > 0 ? "TAKE  " + juce::String (count) + " notes" : juce::String (processor.stringsMode.load() == 1 ? "STRINGS: Scale" : processor.stringsMode.load() == 2 ? "STRINGS: Notes" : "STRINGS: Chord"),
                                a.removeFromTop (14.0f), juce::Justification::centredLeft, false);
                    const bool on[] = { true, plainOf (processor, "dual") > 0.5f, plainOf (processor, "bassType") > 0.5f, plainOf (processor, "chordType") > 0.5f };
                    a.removeFromTop (12.0f);
                    auto row = a.removeFromTop (18.0f);
                    const float cw = row.getWidth() / 4.0f;
                    for (int k = 0; k < 4; ++k)
                    {
                        auto cell = row.withX (row.getX() + cw * (float) k).withWidth (cw);
                        g.setColour (on[k] ? mint : mint.withAlpha (0.35f));
                        g.drawText (on[k] ? "ON" : "OFF", cell, juce::Justification::centred, false);
                        g.fillRect (cell.reduced (cw * 0.2f, 0.0f).removeFromBottom (1.0f));
                    }
                    // the notes on the strings
                    std::vector<int> tones;
                    const int chord = processor.activeChord.load() >= 0 ? processor.activeChord.load() : processor.detectedChord.load();
                    if (chords::isValid (chord))
                    {
                        const int mask = chords::pitchClassMask (chord, 0);
                        for (int pc = 0; pc < 12 && (int) tones.size() < 4; ++pc)
                            if (mask & (1 << ((chords::rootOf (chord) + pc) % 12))) tones.push_back ((chords::rootOf (chord) + pc) % 12);
                    }
                    a.removeFromTop (8.0f);
                    for (int k = 0; k < (int) tones.size(); ++k)
                    {
                        g.setColour (mint);
                        g.drawText (chords::rootName (tones[(size_t) k]), a.withX (a.getX() + cw * (float) k).withWidth (cw).withHeight (16.0f), juce::Justification::centred, false);
                    }
                }
            }
            else if (kind == "label")
            {
                const int chord = processor.activeChord.load() >= 0 ? processor.activeChord.load() : processor.detectedChord.load();
                g.setColour (mint);
                g.setFont (juce::Font (juce::FontOptions (30.0f)).withExtraKerningFactor (0.02f));
                g.drawText (chords::isValid (chord) ? chords::shortName (chord) : juce::String ("--"), juce::Rectangle<float> (r.getRight() - 240.0f, r.getY() + 14.0f, 240.0f, 36.0f), juce::Justification::centredRight, false);
            }
        }

        if (keysView) paintKeys (g); else if (stepsView()) paintSteps (g); else { paintStrings (g); paintFx (g); }
        paintHarpTypes (g);
        paintMeter (g);
        if (pressedCtl >= 0 && pressedCtl < (int) controls.size())   // the key is held in
        {
            auto r = controls[(size_t) pressedCtl]->r;
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 7.0f, false));
            g.fillRoundedRectangle (r.withHeight (7.0f), 4.0f);
        }
        paintValueBubble (g);
        // RESET: the strings back to clean
        if (keysView)   // OCT - / OCT +: the keyboard shifts by an octave
        {
            for (int i = 0; i < 2; ++i)
            {
                auto k = i == 0 ? fxRandomKey() : resetKey();
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillRoundedRectangle (k, 3.0f);
                g.setColour (bone.withAlpha (0.7f));
                g.drawRoundedRectangle (k.reduced (0.5f), 3.0f, 1.0f);
                g.setFont (ui (10.0f));
                g.drawText (i == 0 ? "OCT +" : "OCT -", k, juce::Justification::centred, false);
            }
        }
        if (! keysView)
        {
            auto k = resetKey();
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (k, 3.0f);
            g.setColour (mint.withAlpha (0.75f));
            g.drawRoundedRectangle (k.reduced (0.5f), 3.0f, 1.0f);
            g.setFont (ui (10.0f));
            g.drawText (stepsView() ? "PATTERN" : "RESET", k, juce::Justification::centred, false);
            auto rk = stepsView() ? autoKey() : fxRandomKey();
            const bool autoOn = stepsView() && processor.flAuto.load();
            if (autoOn) { g.setColour (mint.withAlpha (0.25f)); g.fillRoundedRectangle (rk, 3.0f); }
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (rk, 3.0f);
            g.setColour (bone.withAlpha (0.7f));
            g.drawRoundedRectangle (rk.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (autoOn ? mint : bone.withAlpha (0.7f));
            g.drawText (stepsView() ? "AUTO" : "FX RND", rk, juce::Justification::centred, false);
        }
    }

    //==============================================================================
    // STRING FX: the effect each string plays with. Colours of hardware LEDs, one per effect.
    namespace stringfx
    {
        constexpr int count = 10;
        const char* const names[count] = { "", "CHOP", "STOP", "HALF", "REV", "STUT", "SCRT", "CRSH", "OCT", "DEAD" };
        const char* const longNames[count] = { "Off", "Chop: gated in 1/16", "Stop: tape slows to a halt", "Half: one octave down",
                                               "Rev: swells in like a reversed note", "Stutter: fires 4x at 1/32", "Scratch: rocks under a hand",
                                               "Crush: fewer bits, fewer samples", "Oct: one octave up", "Dead: muted, short and dark" };
        const juce::uint32 colours[count] = { 0xff3a3a38, 0xffe8a13a, 0xffd9442e, 0xff3c78c8, 0xff52ad4f, 0xffe3cf3b,
                                              0xffe2692a, 0xffcc5670, 0xff38b0bd, 0xffc9c7bd };
    }

    juce::Rectangle<float> ConsoleView::fxPad (int s) const
    {
        auto b = bed();
        const float step = (b.getWidth() - 28.0f) / (float) (Hamamatsu108Processor::numStrings - 1);
        return juce::Rectangle<float> (step - 6.0f, 16.0f).withCentre ({ stringX (s), b.getY() - 12.0f });
    }
    int ConsoleView::fxPadAt (juce::Point<float> p) const
    {
        if (keysView || bedLayer == 2) return -1;
        for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
            if (fxPad (s).expanded (3.0f, 2.0f).contains (p)) return s;
        return -1;
    }
    juce::Rectangle<float> ConsoleView::fxRandomKey() const { return resetKey().translated (0.0f, -24.0f); }

    void ConsoleView::paintFx (juce::Graphics& g)
    {
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)).withExtraKerningFactor (0.06f));
        for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
        {
            const int f = juce::jlimit (0, stringfx::count - 1, processor.plFx[(size_t) s].load());
            auto r = fxPad (s);
            // the rubber pad
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r.translated (1.0f, 2.0f), 2.5f);
            g.setColour (juce::Colour (0xff1d1d1c));
            g.fillRoundedRectangle (r, 2.5f);
            if (f == 0)
            {
                g.setColour (juce::Colour (0xff2c2c2a));
                g.fillRoundedRectangle (r.reduced (2.0f), 1.5f);
                continue;
            }
            // the lit window: flat colour, brighter while the string sounds
            const auto col = juce::Colour (stringfx::colours[f]);
            const float v = glow[(size_t) s];
            g.setColour (col.withAlpha (0.18f + 0.22f * v));
            g.fillRoundedRectangle (r.expanded (2.0f), 3.5f);
            g.setColour (col.withMultipliedBrightness (0.82f + 0.18f * v));
            g.fillRoundedRectangle (r.reduced (2.0f), 1.5f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRect (r.reduced (3.0f).withHeight (1.5f));
            g.setColour (juce::Colour (0xff141414));
            g.drawText (stringfx::names[f], r, juce::Justification::centred, false);
        }
    }

    void ConsoleView::fxRandom()
    {
        juce::Random rnd;
        for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
            processor.plFx[(size_t) s].store (rnd.nextFloat() < 0.4f ? 1 + rnd.nextInt (stringfx::count - 1) : 0);
        repaint();
    }

    void ConsoleView::fxMenu (int s)
    {
        juce::PopupMenu m;
        const int cur = s >= 0 ? processor.plFx[(size_t) s].load() : -1;
        if (s >= 0)
        {
            m.addSectionHeader ("STRING " + juce::String (s + 1));
            for (int f = 0; f < stringfx::count; ++f) m.addItem (1 + f, stringfx::longNames[f], true, f == cur);
            m.addSeparator();
            m.addItem (40, "Set this effect on every string");
        }
        m.addItem (41, "Random effects");
        m.addItem (42, "Clear every string");
        juce::Component::SafePointer<ConsoleView> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, s, cur] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            auto& fx = safe->processor.plFx;
            if (r >= 1 && r <= stringfx::count && s >= 0) fx[(size_t) s].store (r - 1);
            if (r == 40) for (auto& f : fx) f.store (cur);
            if (r == 41) safe->fxRandom();
            if (r == 42) for (auto& f : fx) f.store (0);
            safe->repaint();
        });
    }

    juce::Rectangle<float> ConsoleView::resetKey() const
    {
        for (auto& c : controls)
            if (juce::String (c->def->label) == "Tone | Bend") return { c->r.getX() - 2.0f, c->r.getY() - 34.0f, 64.0f, 18.0f };
        return { 1316.0f, 760.0f, 64.0f, 18.0f };
    }

    void ConsoleView::resetMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, bedLayer == 0 ? "Reset TONE and BEND on every string" : "Reset TIME and LEVEL on every string");
        m.addItem (2, "Reset every string (all handles, all strings on)");
        m.addItem (3, "All strings on");
        m.addItem (4, "Clear STRING FX");
        juce::Component::SafePointer<ConsoleView> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            auto& pr = safe->processor;
            for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
            {
                if (r == 1 && safe->bedLayer == 0) { pr.plTone[(size_t) s].store (0.35f); pr.plBend[(size_t) s].store (0.0f); }
                if (r == 1 && safe->bedLayer == 1) { pr.plTime[(size_t) s].store (0.0f); pr.plLevel[(size_t) s].store (0.0f); }
            }
            if (r == 2) pr.resetPlate();
            if (r == 3) pr.plMute.store (0);
            if (r == 4) for (auto& f : pr.plFx) f.store (0);
            safe->repaint();
        });
    }

    //==============================================================================
    // STRINGS: 26 metal strings, each with a handle at the top and at the bottom
    juce::Rectangle<float> ConsoleView::bed() const
    {
        for (auto& c : controls) if (juce::String (c->def->kind) == "strings") return c->r.withTrimmedTop (keysView || bedLayer == 2 ? 0.0f : 24.0f).withTrimmedBottom (14.0f).withWidth (1170.0f);
        return { 115.0f, 752.0f, 1180.0f, 126.0f };
    }
    float ConsoleView::stringX (int s) const
    {
        auto b = bed();
        return b.getX() + 14.0f + (b.getWidth() - 28.0f) * (float) s / (float) (Hamamatsu108Processor::numStrings - 1);
    }
    std::atomic<float>& ConsoleView::handleRef (int s, bool top) const
    {
        auto& pr = processor;
        if (bedLayer == 0) return top ? pr.plBend[(size_t) s] : pr.plTone[(size_t) s];
        return top ? pr.plLevel[(size_t) s] : pr.plTime[(size_t) s];
    }
    float ConsoleView::handleY (int s, bool top) const
    {
        auto b = bed();
        const float travel = b.getHeight() * 0.42f, v = handleRef (s, top).load();
        return top ? b.getY() + 8.0f + v * travel : b.getBottom() - 8.0f - v * travel;
    }
    int ConsoleView::stringAt (float x) const
    {
        int best = -1; float bd = 1.0e9f;
        for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
            if (const float d = std::abs (stringX (s) - x); d < bd) { bd = d; best = s; }
        return bd < 12.0f ? best : -1;
    }
    int ConsoleView::handleAt (juce::Point<float> p) const
    {
        const int s = stringAt (p.x);
        if (s < 0 || ((processor.plMute.load() >> (unsigned) s) & 1u)) return -1;
        if (std::abs (p.y - handleY (s, true)) < 9.0f) return s * 2 + 1;
        if (std::abs (p.y - handleY (s, false)) < 9.0f) return s * 2;
        return -1;
    }

    void ConsoleView::paintStrings (juce::Graphics& g)
    {
        auto b = bed();
        const int n = Hamamatsu108Processor::numStrings;
        for (int s = 0; s < n; ++s)
        {
            const float x = stringX (s), v = glow[(size_t) s];
            const bool muted = (processor.plMute.load() >> (unsigned) s) & 1u;
            const float th = 2.2f + 2.2f * (1.0f - (float) s / (float) (n - 1));   // low strings are thicker
            const float yt = handleY (s, true), yb = handleY (s, false);
            // the steel: a lit cylinder
            juce::ColourGradient steel (juce::Colour (muted ? 0xff2a2a29 : 0xff6c6b67), x - th, 0.0f, juce::Colour (muted ? 0xff141414 : 0xff3a3a38), x + th, 0.0f, false);
            steel.addColour (0.38, juce::Colour (muted ? 0xff3c3c3a : 0xffeeeeea));
            // a soft double shadow on the bed, the string stands off it
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.fillRect (x - th * 0.5f + 5.0f, b.getY() + 2.0f, th + 2.0f, b.getHeight() - 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (x - th * 0.5f + 2.5f, b.getY() + 1.0f, th, b.getHeight() - 1.0f);
            g.setGradientFill (steel);
            g.fillRect (x - th * 0.5f, b.getY(), th, b.getHeight());
            if (s < 10)   // the low strings are wound: a fine winding over the core
            {
                g.setColour (juce::Colours::black.withAlpha (muted ? 0.15f : 0.28f));
                for (float y = b.getY() + 1.0f; y < b.getBottom(); y += 2.0f)
                    g.fillRect (x - th * 0.5f, y, th, 0.7f);
            }
            g.setColour (juce::Colours::white.withAlpha (muted ? 0.12f : 0.55f));   // the specular line
            g.fillRect (x - th * 0.22f, b.getY(), juce::jmax (0.7f, th * 0.18f), b.getHeight());
            // the pins that hold the string, top and bottom
            for (float py : { b.getY() + 4.0f, b.getBottom() - 4.0f })
            {
                auto pin = juce::Rectangle<float> (th + 5.0f, th + 5.0f).withCentre ({ x, py });
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillEllipse (pin.translated (1.5f, 2.0f));
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e6dc), pin.getX() + pin.getWidth() * 0.35f, pin.getY() + pin.getHeight() * 0.3f,
                                                         juce::Colour (0xff3d3c38), pin.getRight(), pin.getBottom(), true));
                g.fillEllipse (pin);
            }
            // the part between the handles sounds: it lights up when plucked and shivers
            if (! muted)
            {
                const float amp = v * v * 2.5f;
                if (v > 0.02f)
                {
                    juce::Path p;
                    p.startNewSubPath (x, yt);
                    for (float y = yt; y <= yb; y += 3.0f)
                    {
                        const float u = (y - yt) / juce::jmax (1.0f, yb - yt);
                        p.lineTo (x + amp * std::sin (u * juce::MathConstants<float>::pi) * std::sin (y * 0.35f + phase * (3.0f + (float) (s % 5))), y);
                    }
                    g.setColour (mint.withAlpha (0.25f * v));
                    g.strokePath (p, juce::PathStrokeType (th + 6.0f));
                    g.setColour (mint.interpolatedWith (juce::Colours::white, 0.4f).withAlpha (0.35f + 0.65f * v));
                    g.strokePath (p, juce::PathStrokeType (th * 0.8f));
                }
                // handles: dark cross pieces with a light index, mint while the string sounds
                for (int t = 0; t < 2; ++t)
                {
                    const float y = t ? yt : yb;
                    const bool hot = dragHandle == s * 2 + t;
                    auto cap = juce::Rectangle<float> (22.0f, 12.0f).withCentre ({ x, y });
                    g.setColour (juce::Colours::black.withAlpha (0.6f));
                    g.fillRoundedRectangle (cap.translated (2.0f, 3.0f), 3.0f);
                    juce::ColourGradient cg (juce::Colour (0xff4a4946), 0.0f, cap.getY(), juce::Colour (0xff0c0c0c), 0.0f, cap.getBottom(), false);
                    g.setGradientFill (cg);
                    g.fillRoundedRectangle (cap, 3.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.25f));
                    g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 3.0f, cap.getRight() - 3.0f);
                    g.setColour (hot ? mint : (v > 0.05f ? mint.withAlpha (0.8f) : bone.withAlpha (0.75f)));
                    g.fillRect (cap.getX() + 4.0f, y - 1.0f, cap.getWidth() - 8.0f, 2.0f);
                }
            }
            // note name under the string
        }
        // what the held handle does
        if (dragHandle >= 0)
        {
            const int s = dragHandle / 2; const bool top = dragHandle & 1;
            const float v = handleRef (s, top).load();
            juce::String what, val;
            if (bedLayer == 0 && ! top) { what = "TONE"; val = v < 0.2f ? "bright, near the end" : v < 0.5f ? "classic" : "round, near the middle"; }
            else if (bedLayer == 0) { what = "BEND"; val = v < 0.01f ? "in tune" : juce::String (-2.0f * v, 2) + " st slide"; }
            else if (! top) { what = "TIME"; val = juce::String (juce::roundToInt (v * 120.0f)) + " ms later"; }
            else { what = "LEVEL"; val = juce::String (juce::roundToInt ((1.0f - 0.85f * v) * 100.0f)) + " %"; }
            g.setColour (mint);
            g.setFont (ui (11.0f));
            g.drawText ("STRING " + juce::String (s + 1) + "  " + what + "  " + val, b.withHeight (14.0f).translated (0.0f, -16.0f), juce::Justification::centred, false);
        }
    }

    // KEYBOARD: five octaves and a C (61 keys) in the bed, shifted by OCT - / OCT +
    namespace { constexpr int numKeys = 61; bool isBlack (int n) { const int k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; } }
    juce::Rectangle<float> ConsoleView::keyRect (int n) const
    {
        auto a = bed().withTrimmedTop (12.0f).reduced (6.0f, 0.0f);
        const int lo = keyBase, hi = keyBase + numKeys - 1;
        int whites = 0; for (int k = lo; k <= hi; ++k) if (! isBlack (k)) ++whites;
        const float w = a.getWidth() / (float) whites;
        int wi = 0; for (int k = lo; k < n; ++k) if (! isBlack (k)) ++wi;
        if (! isBlack (n)) return { a.getX() + (float) wi * w, a.getY(), w, a.getHeight() };
        // black keys sit a little off centre like on a real keyboard (C# and F# lean left, A# and D# right)
        const int pc = n % 12;
        const float lean = (pc == 1 || pc == 6) ? -0.1f : (pc == 3 || pc == 10) ? 0.1f : 0.0f;
        const float bw = w * 0.58f;
        return { a.getX() + (float) wi * w - bw * 0.5f + lean * w, a.getY(), bw, a.getHeight() * 0.62f };
    }
    int ConsoleView::keyAt (juce::Point<float> p) const
    {
        const int lo = keyBase, hi = keyBase + numKeys - 1;
        for (int n = lo; n <= hi; ++n) if (isBlack (n) && keyRect (n).contains (p)) return n;
        for (int n = lo; n <= hi; ++n) if (! isBlack (n) && keyRect (n).contains (p)) return n;
        return -1;
    }
    void ConsoleView::paintKeys (juce::Graphics& g)
    {
        auto& ks = processor.keyboardState;
        const int lo = keyBase, hi = keyBase + numKeys - 1;
        const auto b = bed();
        // the key bed and the red felt the keys rest under
        auto well = b.withTrimmedTop (2.0f).reduced (2.0f, 0.0f);
        g.setColour (juce::Colour (0xff080808));
        g.fillRoundedRectangle (well, 3.0f);
        auto felt = juce::Rectangle<float> (well.getX() + 4.0f, well.getY() + 3.0f, well.getWidth() - 8.0f, 8.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b1a1f), 0.0f, felt.getY(), juce::Colour (0xff3a0c10), 0.0f, felt.getBottom(), false));
        g.fillRect (felt);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRect (felt.withY (felt.getBottom()).withHeight (2.0f));

        // white keys: ivory, a bevel on both sides, a front lip that goes down when the key is pressed
        for (int n = lo; n <= hi; ++n)
        {
            if (isBlack (n)) continue;
            const bool on = ks.isNoteOnForChannels (0xffff, n);
            auto k = keyRect (n).reduced (0.75f, 0.0f);
            const float lip = on ? 2.0f : 7.0f;
            auto top = k.withTrimmedBottom (lip);
            g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffd9d6cd) : juce::Colour (0xfff6f3ea), 0.0f, top.getY(),
                                                     on ? juce::Colour (0xffc4c0b6) : juce::Colour (0xffe2ded3), 0.0f, top.getBottom(), false));
            g.fillRect (top);
            // the shadow of the felt rail and, when pressed, a deeper key
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (on ? 0.45f : 0.3f), 0.0f, top.getY(), juce::Colours::transparentBlack, 0.0f, top.getY() + (on ? 22.0f : 12.0f), false));
            g.fillRect (top.withHeight (on ? 22.0f : 12.0f));
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.fillRect (top.getX(), top.getY(), 1.0f, top.getHeight());
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRect (top.getRight() - 1.0f, top.getY(), 1.0f, top.getHeight());
            // the front lip
            auto front = k.withTop (top.getBottom());
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfcbc0), 0.0f, front.getY(), juce::Colour (0xff8f8b82), 0.0f, front.getBottom(), false));
            g.fillRoundedRectangle (front, 2.0f);
            if (on)
            {
                g.setColour (mint.withAlpha (0.85f));
                g.fillRect (top.withTop (top.getBottom() - 3.0f).reduced (2.0f, 0.0f));
                g.setColour (mint.withAlpha (0.12f));
                g.fillRect (top);
            }
            if (n % 12 == 0)
            {
                g.setFont (ui (9.0f, n == 60));   // C5 = MIDI 60 (FL Studio naming, the same as the flourish editor)
                g.setColour (juce::Colour (0xff55534e));
                g.drawText ("C" + juce::String (n / 12), top.withTrimmedTop (top.getHeight() - 18.0f), juce::Justification::centred, false);
            }
        }
        // black keys: a soft shadow on the ivory, a body, a lighter top face, a short front slope
        for (int n = lo; n <= hi; ++n)
        {
            if (! isBlack (n)) continue;
            const bool on = ks.isNoteOnForChannels (0xffff, n);
            auto k = keyRect (n);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (k.translated (2.5f, 0.0f).withTrimmedTop (4.0f).expanded (1.0f, 3.0f), 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.fillRoundedRectangle (k.translated (4.5f, 0.0f).withTrimmedTop (8.0f).expanded (2.0f, 6.0f), 4.0f);
            g.setColour (juce::Colour (0xff050505));
            g.fillRoundedRectangle (k, 2.0f);
            const float slope = on ? 4.0f : 11.0f;
            auto face = k.reduced (2.2f, 0.0f).withTrimmedBottom (slope).withTrimmedTop (1.0f);
            g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff1c1c1c) : juce::Colour (0xff2d2d2e), 0.0f, face.getY(),
                                                     on ? juce::Colour (0xff141414) : juce::Colour (0xff1a1a1b), 0.0f, face.getBottom(), false));
            g.fillRoundedRectangle (face, 1.5f);
            auto front = k.reduced (1.0f, 0.0f).withTop (face.getBottom()).withTrimmedBottom (1.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4b), 0.0f, front.getY(), juce::Colour (0xff111111), 0.0f, front.getBottom(), false));
            g.fillRoundedRectangle (front, 1.5f);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.05f : 0.16f));   // the gloss along the top face
            g.fillRect (face.getX() + 1.0f, face.getY() + 2.0f, 1.2f, face.getHeight() - 6.0f);
            if (on)
            {
                g.setColour (mint.withAlpha (0.9f));
                g.fillRect (face.withTop (face.getBottom() - 3.0f).reduced (1.0f, 0.0f));
            }
        }
        // the range of the keyboard
        g.setColour (mint.withAlpha (0.7f));
        g.setFont (ui (9.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (lo, true, true, 5) + " - " + juce::MidiMessage::getMidiNoteName (hi, true, true, 5),
                    b.withHeight (12.0f).translated (0.0f, -14.0f), juce::Justification::centredRight, false);
    }

    //==============================================================================
    //==============================================================================
    // OUTPUT METER: two LED columns by the volume fader (mint, amber, red at the top)
    void ConsoleView::paintMeter (juce::Graphics& g)
    {
        const auto m = meterRect();
        g.setColour (juce::Colour (0xff070807));
        g.fillRoundedRectangle (m, 2.0f);
        constexpr int segs = 16;
        const float segH = (m.getHeight() - 4.0f) / (float) segs;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float lvl = ch == 0 ? meterL : meterR;
            const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
            const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 51.0f) * (float) segs);
            const float x = m.getX() + 2.5f + (float) ch * 6.0f;
            for (int k = 0; k < segs; ++k)
            {
                const juce::Colour c = k >= segs - 2 ? cs::ledRed : k >= segs - 5 ? juce::Colour (0xffe8a13a) : mint;
                const float y = m.getBottom() - 2.0f - (float) (k + 1) * segH;
                g.setColour (k < lit ? c : c.withAlpha (0.1f));
                g.fillRect (x, y + 0.6f, 5.0f, segH - 1.2f);
            }
        }
    }

    // the value of the fader or knob in hand, on a little piece of LCD above it
    void ConsoleView::paintValueBubble (juce::Graphics& g)
    {
        if (dragging < 0 || dragging >= (int) controls.size()) return;
        auto& c = *controls[(size_t) dragging];
        if (juce::String (c.def->label) == "Drag MIDI") return;
        juce::String text;
        if (c.param.isNotEmpty()) { if (auto* q = P (processor, c.param.toRawUTF8())) text = q->getCurrentValueAsText(); }
        else if (c.spare >= 0) text = juce::String (juce::roundToInt (valueOf (processor, c) * 100.0f)) + " %";
        if (text.isEmpty()) return;
        text = juce::String (c.def->label).toUpperCase() + "  " + text;
        const auto f = juce::Font (juce::FontOptions (11.0f, juce::Font::bold));
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (f, text) + 18.0f;
        auto r = juce::Rectangle<float> (w, 20.0f).withCentre ({ c.r.getCentreX(), c.r.getY() - 14.0f });
        r = r.withX (juce::jlimit (4.0f, designW - w - 4.0f, r.getX())).withY (juce::jmax (4.0f, r.getY()));
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 3.0f);
        cs::lcd (g, r);
        g.setFont (f);
        cs::lcdText (g, text, r, juce::Justification::centred);
    }

    //==============================================================================
    // HARP TYPES: six instruments, each with its own strum, flourish and touch
    namespace harptypes
    {
        const char* const tips[Hamamatsu108Processor::numHarpTypes] = {
            "Concert harp: wide even glissando, round touch",
            "Paraguayan harp: fast nails, the bass drops under every stroke",
            "Kora: thumb and finger take turns left and right, buzzing bridge",
            "Chord zither: one fast even sweep over the chord bars, short and dry",
            "Wire harp: slow strokes, metal strings that ring and ring",
            "Laser harp: beams locked to the tempo grid, every note the same",
            "Celtic harp: warm gut strings, a gentle lilting run",
            "Cross harp: all twelve notes on the strings, a fast chromatic glissando",
            "Thunder: the bass strings an octave down, rattling like a storm" };
    }

    juce::Rectangle<float> ConsoleView::harpTypeKey (int t) const { return { 1487.0f, 745.0f + 14.0f * (float) t, 64.0f, 13.0f }; }
    juce::Rectangle<float> ConsoleView::flourishKey() const { return { 1487.0f, 873.0f, 62.0f, 20.0f }; }
    juce::Rectangle<float> ConsoleView::autoKey() const { return fxRandomKey(); }

    void ConsoleView::paintHarpTypes (juce::Graphics& g)
    {
        const int cur = processor.harpType();
        g.setColour (bone.withAlpha (0.55f));
        g.setFont (ui (9.0f));
        g.drawText ("HARP TYPE", juce::Rectangle<float> (1487.0f, 730.0f, 64.0f, 13.0f), juce::Justification::centredLeft, false);
        for (int t = 0; t < Hamamatsu108Processor::numHarpTypes; ++t)
        {
            auto r = harpTypeKey (t);
            const bool on = t == cur;
            auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 6.0f, r.getCentreY() });
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillEllipse (led.expanded (1.5f));
            if (on) { g.setColour (ledRed.withAlpha (0.28f)); g.fillEllipse (led.expanded (4.0f)); }
            g.setColour (on ? ledRed : juce::Colour (0xff4a1d1f));
            g.fillEllipse (led);
            g.setColour (on ? bone : bone.withAlpha (0.5f));
            g.setFont (ui (8.0f, on));
            g.drawText (Hamamatsu108Processor::harpTypeName (t), r.withTrimmedLeft (13.0f), juce::Justification::centredLeft, false);
        }
        auto k = flourishKey();
        const bool lit = stepsView();
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (k, 3.0f);
        if (lit) { g.setColour (mint.withAlpha (0.22f)); g.fillRoundedRectangle (k, 3.0f); }
        g.setColour (lit ? mint : bone.withAlpha (0.7f));
        g.drawRoundedRectangle (k.reduced (0.5f), 3.0f, 1.0f);
        g.setFont (ui (9.0f));
        g.drawText ("FLOURISH", k, juce::Justification::centred, false);
    }

    void ConsoleView::selectHarpType (int t, bool withPattern)
    {
        if (auto* q = P (processor, "harpType"))
        {
            q->beginChangeGesture();
            q->setValueNotifyingHost (q->convertTo0to1 ((float) t));
            q->endChangeGesture();
        }
        if (withPattern) processor.loadFlourishDefault (t);
        processor.lastHarpType = t;   // the audio thread must not swap the steps again
        if (onStatus) onStatus (Hamamatsu108Processor::harpTypeName (t), harptypes::tips[t]);
        repaint();
    }

    // FLOURISH STEPS: 16 columns over the bed, note height up and down, level as brightness
    juce::Rectangle<float> ConsoleView::stepColumn (int i) const
    {
        auto b = bed().withTrimmedLeft (34.0f).withTrimmedRight (8.0f);
        const float w = b.getWidth() / (float) Hamamatsu108Processor::maxFlSteps;
        return { b.getX() + w * (float) i, b.getY(), w, b.getHeight() };
    }
    int ConsoleView::stepAt (float x) const
    {
        for (int i = 0; i < Hamamatsu108Processor::maxFlSteps; ++i)
            if (const auto c = stepColumn (i); x >= c.getX() && x < c.getRight()) return i;
        return -1;
    }
    float ConsoleView::semiToY (int semi) const
    {
        const auto b = bed();
        const float top = b.getY() + 28.0f, bottom = b.getBottom() - 22.0f;
        return bottom - (float) (semi + 12) / 48.0f * (bottom - top);
    }
    int ConsoleView::yToSemi (float y) const
    {
        const auto b = bed();
        const float top = b.getY() + 28.0f, bottom = b.getBottom() - 22.0f;
        return juce::jlimit (-12, 36, juce::roundToInt ((bottom - y) / (bottom - top) * 48.0f) - 12);
    }

    void ConsoleView::paintSteps (juce::Graphics& g)
    {
        const auto b = bed();
        const int count = juce::jlimit (1, Hamamatsu108Processor::maxFlSteps, processor.flSteps.load());
        const bool autoOn = processor.flAuto.load();
        const int key = processor.flKey.load();
        const float x0 = stepColumn (0).getX(), x1 = stepColumn (Hamamatsu108Processor::maxFlSteps - 1).getRight();
        // octave guides
        for (int o = -12; o <= 36; o += 12)
        {
            const float y = semiToY (o);
            g.setColour (mint.withAlpha (o == 0 ? 0.25f : 0.1f));
            g.fillRect (x0, y - 0.5f, x1 - x0, 1.0f);
            g.setColour (bone.withAlpha (o == 0 ? 0.6f : 0.35f));
            g.setFont (ui (9.0f, o == 0));
            g.drawText (o == 0 ? juce::String ("KEY") : (o > 0 ? "+" : "") + juce::String (o), juce::Rectangle<float> (b.getX() + 2.0f, y - 6.0f, 28.0f, 12.0f), juce::Justification::centredRight, false);
        }
        for (int i = 0; i < Hamamatsu108Processor::maxFlSteps; ++i)
        {
            const auto c = stepColumn (i);
            const bool inRun = i < count;
            const float vel = i == 0 ? juce::jmax (0.3f, processor.flVel[0].load()) : processor.flVel[(size_t) i].load();
            const int semi = i == 0 ? 0 : processor.flSemi[(size_t) i].load();
            const bool on = inRun && vel > 0.001f;
            const int vis = juce::roundToInt ((count > 1 ? (float) i / (float) (count - 1) : 0.0f) * (float) (Hamamatsu108Processor::numStrings - 1));
            const float hot = inRun ? glow[(size_t) juce::jlimit (0, Hamamatsu108Processor::numStrings - 1, vis)] : 0.0f;
            // the lane
            const auto lane = juce::Rectangle<float> (c.getX() + 3.0f, b.getY() + 22.0f, c.getWidth() - 6.0f, b.getHeight() - 38.0f);
            g.setColour (juce::Colours::black.withAlpha (inRun ? 0.24f : 0.08f));
            g.fillRect (lane);
            if (hot > 0.0f) { g.setColour (mint.withAlpha (0.14f * hot)); g.fillRect (lane); }
            // the note
            auto block = juce::Rectangle<float> (c.getWidth() - 14.0f, 8.0f).withCentre ({ c.getCentreX(), semiToY (semi) });
            if (on)
            {
                const auto col = i == 0 ? bone : mint;
                const float dim = autoOn ? 0.35f : 1.0f;
                g.setColour (col.withAlpha ((0.1f + 0.3f * hot) * dim));
                g.fillRoundedRectangle (block.expanded (3.0f), 3.0f);
                g.setColour (col.withAlpha ((0.35f + 0.65f * vel) * dim));
                g.fillRoundedRectangle (block, 2.0f);
            }
            else if (inRun)
            {
                g.setColour (mint.withAlpha (0.35f));
                g.drawRoundedRectangle (block, 2.0f, 1.0f);
            }
            // the note name on top, the level at the bottom
            g.setColour (inRun ? (i == 0 ? bone : bone.withAlpha (on ? 0.85f : 0.35f)) : bone.withAlpha (0.2f));
            g.setFont (ui (9.5f, i == 0));
            g.drawText (inRun ? juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, key + semi), true, true, 5) : juce::String (i + 1),
                        c.withHeight (18.0f).translated (0.0f, 3.0f), juce::Justification::centred, false);
            if (inRun)
            {
                auto lv = juce::Rectangle<float> (c.getWidth() - 16.0f, 4.0f).withCentre ({ c.getCentreX(), b.getBottom() - 9.0f });
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillRect (lv);
                if (on) { g.setColour (mint.withAlpha (0.8f)); g.fillRect (lv.withWidth (lv.getWidth() * vel)); }
            }
        }
        // the end of the run
        const float endX = stepColumn (count - 1).getRight();
        g.setColour (mint.withAlpha (0.75f));
        g.fillRect (endX - 1.0f, b.getY() + 20.0f, 2.0f, b.getHeight() - 34.0f);
        if (autoOn)
        {
            auto msg = b.withSizeKeepingCentre (560.0f, 24.0f);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (msg, 3.0f);
            g.setColour (bone);
            g.setFont (ui (11.0f));
            g.drawText ("AUTO: the run follows the chord of the key. Switch AUTO off for your steps.", msg, juce::Justification::centred, false);
        }
    }

    void ConsoleView::stepStatus (int i)
    {
        if (! onStatus || i < 0) return;
        const int semi = i == 0 ? 0 : processor.flSemi[(size_t) i].load();
        const float vel = processor.flVel[(size_t) i].load();
        onStatus ("FLOURISH STEP " + juce::String (i + 1),
                  i == 0 ? juce::String ("The key you play, level ") + juce::String (juce::roundToInt (juce::jmax (0.3f, vel) * 100.0f)) + "%" :
                  vel <= 0.001f ? juce::String ("Off") :
                  juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, processor.flKey.load() + semi), true, true, 5)
                      + "  (" + (semi >= 0 ? "+" : "") + juce::String (semi) + ")  level " + juce::String (juce::roundToInt (vel * 100.0f)) + "%");
    }

    void ConsoleView::stepMenu (int i)
    {
        juce::PopupMenu m;
        const int type = processor.harpType();
        if (i >= 0) m.addItem (1, "End the run here (" + juce::String (i + 1) + " steps)");
        m.addItem (2, "Back to the " + juce::String (Hamamatsu108Processor::harpTypeName (type)).toLowerCase() + " pattern");
        m.addItem (3, "Random notes");
        m.addItem (4, "Every step up an octave");
        m.addItem (5, "Every step down an octave");
        m.addItem (6, "Every step on, full level");
        m.addItem (7, "Only the key (clear the steps)");
        m.addSeparator();
        m.addItem (8, "AUTO: the run through the chord of the key", true, processor.flAuto.load());
        juce::Component::SafePointer<ConsoleView> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, i, type] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            auto& p = safe->processor;
            const int n = Hamamatsu108Processor::maxFlSteps;
            if (r == 1) { p.flSteps.store (juce::jmax (1, i + 1)); p.flEdited.store (true); }
            if (r == 2) p.loadFlourishDefault (type);
            if (r == 3)
            {
                static const int pool[] = { -12, -5, 0, 2, 3, 5, 7, 9, 10, 12, 14, 15, 17, 19, 21, 22, 24, 26, 31 };
                juce::Random rnd;
                for (int k = 1; k < n; ++k)
                {
                    p.flSemi[(size_t) k].store (pool[rnd.nextInt ((int) std::size (pool))]);
                    p.flVel[(size_t) k].store (rnd.nextFloat() < 0.15f ? 0.0f : 0.55f + 0.45f * rnd.nextFloat());
                }
            }
            if (r == 4 || r == 5) for (int k = 1; k < n; ++k) p.flSemi[(size_t) k].store (juce::jlimit (-12, 36, p.flSemi[(size_t) k].load() + (r == 4 ? 12 : -12)));
            if (r == 6) for (int k = 0; k < n; ++k) p.flVel[(size_t) k].store (1.0f);
            if (r == 7) for (int k = 1; k < n; ++k) p.flVel[(size_t) k].store (0.0f);
            if (r == 8) p.flAuto.store (! p.flAuto.load());
            else if (r != 1 && r != 2) { p.flAuto.store (false); p.flEdited.store (true); }
            safe->repaint();
        });
    }

    void ConsoleView::mouseDown (const juce::MouseEvent& e)
    {
        const auto d = toDesign (e.position);
        dragging = -1; dragHandle = -1; lastStrumX = -1.0f; stepDrag = -1;
        for (int t = 0; t < Hamamatsu108Processor::numHarpTypes; ++t)
            if (harpTypeKey (t).expanded (0.0f, 0.5f).contains (d))
            {
                if (e.mods.isPopupMenu())
                {
                    juce::PopupMenu m;
                    m.addItem (1, "Switch to " + juce::String (Hamamatsu108Processor::harpTypeName (t)) + " with its flourish");
                    m.addItem (2, "Switch, keep my flourish steps");
                    juce::Component::SafePointer<ConsoleView> safe (this);
                    m.showMenuAsync (juce::PopupMenu::Options(), [safe, t] (int r) { if (safe != nullptr && r > 0) safe->selectHarpType (t, r == 1); });
                }
                else selectHarpType (t, true);
                return;
            }
        if (flourishKey().contains (d))
        {
            if (stepsView()) { keysView = true; bedLayer = 0; }   // FLOURISH again: back to the keyboard
            else { keysView = false; bedLayer = 2; }
            if (onStatus && bedLayer == 2) onStatus ("FLOURISH STEPS", "Step 1 is the key you play. Drag a step up or down, click to switch it off, Shift-drag for its level");
            repaint(); return;
        }
        if (stepsView())
        {
            if (resetKey().contains (d)) { stepMenu (-1); return; }
            if (autoKey().contains (d)) { processor.flAuto.store (! processor.flAuto.load()); repaint(); return; }
            if (bed().contains (d))
            {
                const int i = stepAt (d.x);
                if (i < 0) return;
                if (e.mods.isPopupMenu()) { stepMenu (i); return; }
                const int count = processor.flSteps.load();
                if (i >= count)   // a click past the end makes the run longer
                {
                    processor.flSteps.store (i + 1);
                    for (int k = count; k <= i; ++k) if (processor.flVel[(size_t) k].load() <= 0.001f) processor.flVel[(size_t) k].store (0.8f);
                    processor.flAuto.store (false); processor.flEdited.store (true);
                    stepStatus (i); repaint(); return;
                }
                stepDrag = i; stepMoved = false; stepVelDrag = e.mods.isShiftDown() || i == 0;
                stepStartY = d.y; stepStartVel = processor.flVel[(size_t) i].load();
                stepStatus (i);
                return;
            }
        }

        if (keysView && (resetKey().contains (d) || fxRandomKey().contains (d)))
        {
            for (int n = 0; n < 128; ++n) if (processor.keyboardState.isNoteOn (1, n) && n == heldKey) processor.keyboardState.noteOff (1, n, 0.0f);
            heldKey = -1;
            keyBase = juce::jlimit (12, 60, keyBase + (fxRandomKey().contains (d) ? 12 : -12));
            if (onStatus) onStatus ("KEYBOARD", juce::MidiMessage::getMidiNoteName (keyBase, true, true, 5) + " - " + juce::MidiMessage::getMidiNoteName (keyBase + numKeys - 1, true, true, 5));
            repaint(); return;
        }
        if (! keysView && resetKey().contains (d)) { resetMenu(); return; }
        if (! keysView && fxRandomKey().contains (d)) { if (e.mods.isPopupMenu()) fxMenu (-1); else fxRandom(); return; }
        if (const int s = fxPadAt (d); s >= 0)
        {
            if (e.mods.isPopupMenu()) { fxMenu (s); return; }
            auto& f = processor.plFx[(size_t) s];
            const int next = e.mods.isShiftDown() || e.mods.isAltDown() ? 0 : (f.load() + 1) % stringfx::count;
            f.store (next);
            fxPaint = next;   // drag on: the next pads get the same effect
            if (onStatus) onStatus ("STRING " + juce::String (s + 1) + " FX", stringfx::longNames[next]);
            repaint(); return;
        }
        if (e.mods.isPopupMenu() && bed().contains (d) && ! keysView) { resetMenu(); return; }
        const int i = hitControl (d);
        if (i >= 0)
        {
            auto& c = *controls[(size_t) i];
            const juce::String kind (c.def->kind);
            if ((kind == "fader" || kind == "knob" || kind == "hfader") && (c.param.isNotEmpty() || c.spare >= 0))
            {
                dragging = i; dragStartY = kind == "hfader" ? -d.x : d.y;
                gesture (processor, c, true);
                if (kind == "fader")   // a click on the track jumps the cap there
                {
                    const float travel = c.r.getHeight() - 18.0f;
                    const float cy = c.r.getY() + (1.0f - valueOf (processor, c)) * travel;
                    if (d.y < cy - 2.0f || d.y > cy + 20.0f) setValueOf (processor, c, 1.0f - (d.y - c.r.getY() - 9.0f) / travel);
                }
                dragStartValue = valueOf (processor, c);
                repaint();
                return;
            }
            if (kind == "leds" && c.pick)
            {
                for (int k = 0; k < c.def->numItems; ++k)
                    if (std::abs (d.y - (c.def->items[k].y + c.def->items[k].h * 0.5f)) < 8.0f) { c.pick (k); break; }
                repaint(); return;
            }
            if (juce::String (c.def->label) == "Drag MIDI") { if (e.mods.isPopupMenu()) { if (onTakeMenu) onTakeMenu (this); } else dragging = i; return; }
            if (c.press) { pressedCtl = i; c.press(); repaint(); return; }
            return;
        }
        // the strings / the keys
        if (bed().expanded (0.0f, 6.0f).contains (d))
        {
            if (keysView)
            {
                const int n = keyAt (d);
                if (n >= 0)
                {
                    auto k = keyRect (n);
                    heldVel = juce::jlimit (0.25f, 1.0f, 0.3f + 0.7f * (d.y - k.getY()) / k.getHeight());   // lower on the key = harder
                    processor.keyboardState.noteOn (1, n, heldVel);
                    heldKey = n;
                }
                repaint(); return;
            }
            if (const int h = handleAt (d); h >= 0) { dragHandle = h; repaint(); return; }
            lastStrumX = d.x;
            if (const int s = stringAt (d.x); s >= 0) processor.pluckString (s);
        }
    }

    void ConsoleView::mouseDrag (const juce::MouseEvent& e)
    {
        const auto d = toDesign (e.position);
        if (stepDrag >= 0)
        {
            if (std::abs (d.y - stepStartY) > 2.0f) stepMoved = true;
            if (! stepMoved) return;
            processor.flAuto.store (false); processor.flEdited.store (true);
            if (stepVelDrag)
                processor.flVel[(size_t) stepDrag].store (juce::jlimit (stepDrag == 0 ? 0.3f : 0.05f, 1.0f, stepStartVel + (stepStartY - d.y) / 90.0f));
            else if (processor.flSteps.load() > 1)
            {
                // drag the note; across the steps it draws a line of notes
                const int at = stepAt (d.x);
                const int i = juce::jlimit (1, processor.flSteps.load() - 1, at < 0 ? stepDrag : at);
                processor.flSemi[(size_t) i].store (yToSemi (d.y));
                if (processor.flVel[(size_t) i].load() <= 0.001f) processor.flVel[(size_t) i].store (0.8f);
                stepDrag = i;
            }
            stepStatus (stepDrag);
            repaint();
            return;
        }

        if (fxPaint >= 0)
        {
            if (const int s = stringAt (d.x); s >= 0 && processor.plFx[(size_t) s].load() != fxPaint) { processor.plFx[(size_t) s].store (fxPaint); repaint(); }
            return;
        }
        if (dragging >= 0)
        {
            auto& c = *controls[(size_t) dragging];
            const juce::String kind (c.def->kind);
            if (juce::String (c.def->label) == "Drag MIDI")
            {
                if (e.getDistanceFromDragStart() > 6 && onDragTake) { onDragTake (this); dragging = -1; }
                return;
            }
            {
                const float range = kind == "fader" ? c.r.getHeight() - 18.0f : kind == "hfader" ? c.r.getWidth() - 18.0f : 160.0f;
                const float pos = kind == "hfader" ? -d.x : d.y;
                const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
                setValueOf (processor, c, dragStartValue + (dragStartY - pos) / range * fine);
                if (onStatus) if (auto* q = P (processor, c.param.toRawUTF8())) onStatus (q->getName (40), q->getCurrentValueAsText());
            }
            repaint();
            return;
        }
        if (keysView)
        {
            const int n = keyAt (d);
            if (n >= 0 && n != heldKey)
            {
                if (heldKey >= 0) processor.keyboardState.noteOff (1, heldKey, 0.0f);
                processor.keyboardState.noteOn (1, n, heldVel);   // a glissando keeps the touch
                heldKey = n;
                repaint();
            }
            return;
        }
        if (dragHandle >= 0)
        {
            auto b = bed();
            const bool top = dragHandle & 1;
            const float travel = b.getHeight() * 0.42f;
            const float v = juce::jlimit (0.0f, 1.0f, top ? (d.y - b.getY() - 8.0f) / travel : (b.getBottom() - 8.0f - d.y) / travel);
            for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
                if (e.mods.isShiftDown() || s == dragHandle / 2) handleRef (s, top).store (v);
            repaint();
            return;
        }
        if (lastStrumX >= 0.0f)
        {
            // the hand across the strings: every string crossed sounds
            for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
            {
                const float x = stringX (s);
                if (x > juce::jmin (lastStrumX, d.x) && x <= juce::jmax (lastStrumX, d.x)) processor.pluckString (s);
            }
            lastStrumX = d.x;
        }
    }

    void ConsoleView::mouseUp (const juce::MouseEvent&)
    {
        if (stepDrag > 0 && ! stepMoved)   // a click switches a step off and on
        {
            auto& v = processor.flVel[(size_t) stepDrag];
            v.store (v.load() > 0.001f ? 0.0f : 0.85f);
            processor.flAuto.store (false); processor.flEdited.store (true);
            stepStatus (stepDrag);
        }
        stepDrag = -1;
        pressedCtl = -1;
        if (dragging >= 0) gesture (processor, *controls[(size_t) dragging], false);
        dragging = -1;
        if (heldKey >= 0) { processor.keyboardState.noteOff (1, heldKey, 0.0f); heldKey = -1; }
        dragHandle = -1; lastStrumX = -1.0f; fxPaint = -1;
        repaint();
    }

    void ConsoleView::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const auto d = toDesign (e.position);
        if (const int i = hitControl (d); i >= 0)
        {
            auto& c = *controls[(size_t) i];
            if (c.param.isNotEmpty())
            {
                if (auto* q = P (processor, c.param.toRawUTF8())) { q->beginChangeGesture(); q->setValueNotifyingHost (q->getDefaultValue()); q->endChangeGesture(); }
            }
            else if (c.spare >= 0) processor.consoleSpare[(size_t) c.spare].store (-1.0f);
            repaint(); return;
        }
        if (keysView || stepsView() || ! bed().contains (d)) return;
        if (const int h = handleAt (d); h >= 0) handleRef (h / 2, h & 1).store (bedLayer == 0 && ! (h & 1) ? 0.35f : 0.0f);
        else if (const int s = stringAt (d.x); s >= 0) processor.plMute.fetch_xor (1u << (unsigned) s);
        repaint();
    }

    void ConsoleView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        if (stepsView() && bed().contains (toDesign (e.position)))
        {
            const int i = stepAt (toDesign (e.position).x);
            if (i >= 1 && i < processor.flSteps.load())
            {
                processor.flSemi[(size_t) i].store (juce::jlimit (-12, 36, processor.flSemi[(size_t) i].load() + (w.deltaY > 0 ? 1 : -1)));
                processor.flAuto.store (false); processor.flEdited.store (true);
                stepStatus (i); repaint();
            }
            return;
        }
        if (const int i = hitControl (toDesign (e.position)); i >= 0)
        {
            auto& c = *controls[(size_t) i];
            if (c.param.isNotEmpty() || c.spare >= 0)
            {
                gesture (processor, c, true);
                setValueOf (processor, c, valueOf (processor, c) + w.deltaY * 0.08f);
                gesture (processor, c, false);
                repaint();
            }
        }
    }

    void ConsoleView::mouseMove (const juce::MouseEvent& e)
    {
        const int i = hitControl (toDesign (e.position));
        juce::String tip = i >= 0 ? controls[(size_t) i]->tip : juce::String();
        if (i < 0 && bed().contains (toDesign (e.position)))
            tip = keysView ? "Click and slide over the keys" :
                  stepsView() ? "FLOURISH STEPS: step 1 is the key you play. Drag a step up or down (across the steps to draw), click to switch it off, Shift-drag for its level, wheel for one semitone, right-click for more" : "Drag across the strings to strum. Drag a handle to tune that string (Shift: all strings), double-click a string to switch it off";
        if (tip != getTooltip()) setTooltip (tip);
    }

    void ConsoleView::flash (std::uint32_t bits)
    {
        for (int s = 0; s < Hamamatsu108Processor::numStrings; ++s)
            if (bits & (1u << (unsigned) s)) glow[(size_t) s] = 1.0f;
    }

    void ConsoleView::timerCallback()
    {
        {
            // the output meter: falls slowly, rises at once
            const float l = processor.meterLeft.load(), r = processor.meterRight.load();
            const float nl = juce::jmax (l, meterL * 0.82f), nr = juce::jmax (r, meterR * 0.82f);
            if (std::abs (nl - meterL) > 0.002f || std::abs (nr - meterR) > 0.002f)
            {
                meterL = nl; meterR = nr;
                const float sx = (float) getWidth() / designW, sy = (float) getHeight() / designH;
                auto m = meterRect().expanded (3.0f);
                repaint (juce::Rectangle<float> (m.getX() * sx, m.getY() * sy, m.getWidth() * sx, m.getHeight() * sy).getSmallestIntegerContainer());
            }
        }
        bool any = false;
        for (auto& v : glow) if (v > 0.0f) { v = juce::jmax (0.0f, v - 0.03f); any = true; }
        phase += 0.9f;
        // repaint when anything shown changed (cheap signature of the bound values)
        juce::int64 sig = 1469598103934665603LL;
        auto mix = [&sig] (juce::int64 v) { sig = (sig ^ v) * 1099511628211LL; };
        for (auto& c : controls)
        {
            if (c->param.isNotEmpty() || c->spare >= 0) mix (juce::roundToInt (valueOf (processor, *c) * 1000.0f));
            if (c->lit) mix (c->lit() ? 7 : 3);
            if (c->selected) mix (c->selected());
        }
        mix (processor.presetManager->currentName().hashCode64());
        mix (processor.activeChord.load()); mix (processor.detectedChord.load()); mix (keysView ? 1 : 0); mix (keyBase); mix (bedLayer); mix (processor.harpType()); mix (processor.flAuto.load() ? 1 : 0); mix (processor.flSteps.load());
        mix (processor.stringsMode.load()); mix ((juce::int64) processor.plMute.load());
        if (sig != lastSig) { lastSig = sig; repaint(); }
        else if (any || keysView)
        {
            // only the strings move: repaint just that part
            const float sx = (float) getWidth() / designW, sy = (float) getHeight() / designH;
            auto b = bed().expanded (16.0f, 24.0f);
            repaint (juce::Rectangle<float> (b.getX() * sx, b.getY() * sy, b.getWidth() * sx, b.getHeight() * sy).getSmallestIntegerContainer());
        }
    }

    namespace
    {
        float valueOf (Hamamatsu108Processor& pr, const ConsoleView::Control& c)
        {
            if (c.param.isNotEmpty()) if (auto* q = P (pr, c.param.toRawUTF8())) return q->getValue();
            if (c.spare >= 0) { const float v = pr.consoleSpare[(size_t) c.spare].load(); return v < 0.0f ? 0.5f : v; }
            return 0.5f;
        }
        void setValueOf (Hamamatsu108Processor& pr, ConsoleView::Control& c, float v)
        {
            v = juce::jlimit (0.0f, 1.0f, v);
            if (c.param.isNotEmpty()) { if (auto* q = P (pr, c.param.toRawUTF8())) q->setValueNotifyingHost (v); return; }
            if (c.spare >= 0) pr.consoleSpare[(size_t) c.spare].store (v);
        }
        void gesture (Hamamatsu108Processor& pr, ConsoleView::Control& c, bool begin)
        {
            if (c.param.isNotEmpty()) if (auto* q = P (pr, c.param.toRawUTF8())) { if (begin) q->beginChangeGesture(); else q->endChangeGesture(); }
        }
    }
}
