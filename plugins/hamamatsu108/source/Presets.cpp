#include "Presets.h"
#include "Modulation.h"

namespace presets
{
    namespace
    {
        const char* const names[6][24] = {
            { "Paper Lanterns", "Morning Tram", "Cherry Blossom Rain", "Tatami Light", "Lake Hamana", "Bamboo Wind", "Silk Road Radio", "Shrine Steps",
              "Moon Over Kyoto", "First Snow", "Lantern Festival", "Pocket Sunrise", "Station Chime", "Rice Paper Harp", "Koi Pond", "Garden Bridge",
              "Golden Hour Harp", "Sakura Strum", "Neon Arcade Harp", "Teahouse", "Hilltop Temple", "Plum Wine", "Evening Ferry", "Harbor Lights" },
            { "Little Kalimba", "Rubber Pluck", "Pizz Box", "Dewdrops", "Pebble Game", "Toy Piano Pluck", "Wooden Steps", "Nylon Strum",
              "Raindrop Plinks", "Click Clack", "Mint Candy", "Marimba Market", "Glass Beads", "Pocket Koto", "Tin Roof", "Puddle Jump",
              "Hopscotch", "Plucky Morning", "Bottle Caps", "Woodblock Walk", "Soft Picks", "Cricket Song", "Bubble Pop", "Steel Pan Sunset" },
            { "Joya no Kane", "Temple Bell", "Crystal Garden", "Ice Palace", "Wind Chime Porch", "Glass Harmonica", "Music Box Dream", "Night Train Bell",
              "Celesta Snow", "Silver Spoons", "Midnight Chapel", "Bell Tower", "Star Map", "Frozen Lake", "Aurora Bells", "Mirror Hall",
              "Snow Globe", "Lullaby Box", "Chime Cascade", "Sky Lanterns", "Moonstone", "Cathedral Glass", "Gamelan Haze", "Twelve Strikes" },
            { "Sunday Keys", "Velvet Tines", "Rainy Window Keys", "Cafe Vibes", "Late Night Tines", "Lounge Organ", "Sofa Chords", "Cassette Keys",
              "Soul Kitchen", "Vibraphone Bar", "Elevator Dream", "Mellow Office", "Warm Tines", "Jazz Club Glow", "Rooftop Keys", "City Pop Keys",
              "Dusty Organ", "Neon Diner", "Pastel Keys", "Midnight Lounge", "Gospel Strum", "Soft Focus", "Retro Wagon", "Satin Keys" },
            { "Slow Sunrise", "Floating Garden", "Cloud Choir", "Endless Summer", "Northern Lights", "Dream Machine", "Deep Breath", "Velvet Sky",
              "Ocean Floor", "Haze Strings", "Choir of Lanterns", "Distant City", "Warm Blanket", "Snowfield", "Long Goodbye", "Soft Horizon",
              "Mist Valley", "Slow Motion", "Starlit Pad", "Pillow Fort", "Calm Waters", "Afterglow", "Hush Hour", "Weightless" },
            { "Broken Radio", "Cartridge Dust", "VHS Memories", "Glitch Garden", "Tape Ghosts", "Rusty Swing", "Wobbly Tape", "Lo-Fi Lantern",
              "Dusty Attic", "Robot Lullaby", "Circuit Bent", "Arcade Rain", "Bitcrushed Chimes", "Static Bloom", "Analog Fog", "Haunted Music Box",
              "Warped Vinyl", "Signal Lost", "Pixel Snow", "Rewind", "Old Film", "Shortwave", "Crackle Harp", "Memory Leak" } };

        // added names: 16 more per classic type, 40 for Hybrid and Cinematic
        const std::vector<std::vector<const char*>> extraNames = {
            { "Open Strings", "Wide Strum", "Bright Plate", "Soft Sweep", "Night Strum", "Gold Plate", "Low Sun Strum", "Tape Strum", "Warm Sweep", "Air Harp", "Glass Strum", "Long Exposure", "Full Hand", "Easy Sweep", "Summer Plate", "Last Train Strum" },
            { "Short Story", "Dry Pick", "Hollow Pick", "Tight Pluck", "Pocket Pluck", "Felt Pick", "Nylon Tick", "Quick Step", "Rain Gutter", "Snap Pluck", "Paper Pluck", "Clean Pick", "Muted Pick", "Synth Pluck Air", "FM Pluck", "Analog Pluck" },
            { "Clear Bell", "Wide Bell", "FM Bell Hall", "Digital Chapel", "Ice Bell", "Long Bell", "Bell Air", "Soft Mallet Bell", "Bright Tower", "Copper Bell", "Mint Bell", "Cold Glass", "Bell Pad", "Distant Bell", "Broken Bell", "Tuned Bowl" },
            { "Digital Keys", "FM Rhodes", "Analog Keys", "Wide Keys", "Soft Keys", "Late Keys", "Keys Air", "Dusty Tines", "Polished Keys", "Warm Board", "Studio Keys", "Mono Keys", "Tape Keys", "Night Keys", "Velvet Board", "Glass Keys" },
            { "Analog Pad", "Wavetable Pad", "Air Pad", "Wide Pad", "Slow Pad", "Soft Pad", "Deep Pad", "Bright Pad", "Breath Pad", "Warm Pad", "Glass Pad", "Choir Air", "Long Pad", "Cold Pad", "Gold Pad", "Dark Pad" },
            { "Grain Fog", "Air Noise", "Old Tape Air", "Dust Pad", "Crackle Air", "Bent Bell", "Lo-Fi Keys", "Radio Pad", "Worn Cassette", "Hiss Harp", "Detuned Box", "Faded Strum", "Slow Tape", "Wobble Keys", "Noise Garden", "Static Pad" },
            { "Harp Analog", "Harp Wavetable", "Harp FM", "Harp Air", "Glass Analog", "Celesta Wave", "Pluck FM", "Steel Air", "Chime Saw", "Bell Wave", "Vibes Air", "Marimba FM", "Hybrid Strum", "Hybrid Pluck", "Hybrid Bell", "Hybrid Keys", "Morph Strum", "Morph Bell", "Morph Pluck", "Morph Keys", "Wave Harp", "Saw Harp", "Breath Harp", "Digital Harp", "Analog Chime", "FM Chime", "Air Chime", "Wave Chime", "Split Harp", "Dual Harp", "Layer Strum", "Two Engines", "Octave Harp", "Sub Harp", "High Harp", "Detune Harp", "Wide Hybrid", "Soft Hybrid", "Bright Hybrid", "Dark Hybrid" },
            { "Opening Titles", "Epic Rise", "Slow Reveal", "Trailer Harp", "Score Bed", "Main Theme", "Wide Shot", "Night Scene", "Credits", "Big Sky", "Long Road", "Last Light", "Distant Shore", "Snow Scene", "Final Act", "Prologue", "Wonder", "Discovery", "Low Strings Rise", "Hero Harp", "Tension Bed", "Shimmer Harp", "Ambient Score", "Dream Sequence", "Flashback", "Aerial", "Horizon Line", "Deep Space", "Arrival", "Departure", "Memory Lane", "Silent Film", "Dark Forest", "Rising Tide", "Glacier", "Cathedral", "Ocean Scene", "Desert Wind", "Starfield", "Epilogue" } };

        // TAILS: a shorter release, less room and echo, smaller rooms (level 1 a little, 3 a lot)
        void tame (std::vector<std::pair<juce::String, float>>& v, int level)
        {
            if (level <= 0) return;
            const float mixK = level == 1 ? 0.72f : level == 2 ? 0.5f : 0.33f;
            const float sizeK = level == 1 ? 0.88f : level == 2 ? 0.72f : 0.55f;
            const float rel = level == 1 ? 1.3f : level == 2 ? 0.7f : 0.4f;
            auto last = [&v] (const juce::String& id, float def) { for (auto it = v.rbegin(); it != v.rend(); ++it) if (it->first == id) return it->second; return def; };
            auto has = [&v] (const juce::String& id) { for (auto& p : v) if (p.first == id) return true; return false; };
            v.emplace_back ("harpRelease", juce::jmin (last ("harpRelease", 8.0f), rel));
            if (level >= 3) v.emplace_back ("harpDecay", juce::jmin (last ("harpDecay", 1.8f), 2.5f));
            if (has ("spMix")) v.emplace_back ("spMix", last ("spMix", 0.0f) * mixK);
            if (has ("spSize")) v.emplace_back ("spSize", last ("spSize", 0.5f) * sizeK);
            if (has ("macroDream")) v.emplace_back ("macroDream", last ("macroDream", 0.0f) * mixK);
            static const int defType[8] = { 7, 12, 14, 0, 0, 0, 0, 0 };          // the slot defaults (chorus, delay, reverb)
            static const float defA[8] = { 0.3f, 0.45f, 0.8f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
            static const float defMix[8] = { 0.45f, 0.22f, 0.55f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
            for (int k = 1; k <= 8; ++k)
            {
                const juce::String n (k);
                const int type = juce::roundToInt (last ("fx" + n + "Type", (float) defType[k - 1]));
                if (type < 12 || type > 15) continue;   // delay, ping-pong, reverb, shimmer
                v.emplace_back ("fx" + n + "Mix", last ("fx" + n + "Mix", defMix[k - 1]) * mixK);
                v.emplace_back ("fx" + n + "A", last ("fx" + n + "A", defA[k - 1]) * sizeK);
            }
            if (level >= 2)
            {
                v.emplace_back ("harpSustain", last ("harpSustain", 0.0f) * 0.5f);
                v.emplace_back ("bodyRing", last ("bodyRing", 0.5f) * (level >= 3 ? 0.7f : 0.85f));
            }
        }

        struct Builder
        {
            std::vector<std::pair<juce::String, float>> v;
            juce::Random r;
            void set (const juce::String& id, float value) { v.emplace_back (id, value); }
            float range (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); }
            int pick (std::initializer_list<int> options) { std::vector<int> o (options); return o[(size_t) r.nextInt ((int) o.size())]; }
            bool chance (float p) { return r.nextFloat() < p; }
        };
    }


    // ---- DESIGNED SOUNDS: every category has 8 characters far apart, each in 5 variations ----
    namespace design
    {
        enum Flag : int { arp = 1, gate = 2, ratchet = 4, buzz = 8, harmonic = 16, bow = 32, sweep = 64, vibrato = 128, filterLfo = 256,
                          synthB = 512, bass = 1024, chord = 2048, octDown = 4096, reverse = 8192, hpf = 16384, wood = 32768,
                          bright = 65536, sympath = 131072, mono = 262144, tape = 524288, crush = 1048576, shimmer = 2097152, octUp = 4194304 };
        struct Arch
        {
            const char* names[5];
            int engine, touch, mat;              // engine (14 = HARP STRING), touch, body material
            float tone, decay, shape;            // centre values, varied per preset
            int room; float space;               // SPACE room and mix
            int flags;
            int synth;                           // layer B engine when synthB
        };
        // 7 categories x 8 characters
        static const Arch arches[7][8] = {
            { // PLUCK: short, rhythmic, every one a different object being struck
                { { "Carbon Fiber", "Kevlar Snap", "Graphite Tick", "Composite Pop", "Fiber Burst" }, 14, 2, 2, 0.85f, 0.7f, 0.8f, 1, 0.12f, bright | ratchet, 0 },
                { { "Wet Concrete", "Gravel Drop", "Cement Knock", "Brutal Slab", "Rebar" }, 14, 0, 3, 0.35f, 0.5f, 0.1f, 2, 0.15f, wood | octDown, 0 },
                { { "Kalimba 2099", "Tine Matrix", "Thumb Circuit", "Iron Thumb", "Lamella Code" }, 14, 1, 1, 0.7f, 1.2f, 0.5f, 1, 0.2f, harmonic, 0 },
                { { "Sitar Wire", "Bridge Buzz", "Desert Signal", "Raga Machine", "Copper Drone" }, 14, 2, 0, 0.75f, 1.6f, 0.6f, 0, 0.2f, buzz | sympath, 0 },
                { { "Laser Pluck", "Photon Snap", "Beam Cutter", "Pulse Gun", "Tracer" }, 4, 1, 0, 0.9f, 0.6f, 0.4f, 1, 0.18f, sweep | synthB, 12 },
                { { "Marimba Riot", "Wooden Army", "Log Drum Unit", "Rosewood Fire", "Mallet Squad" }, 6, 0, 0, 0.55f, 0.9f, 0.3f, 2, 0.15f, arp, 0 },
                { { "Glitch Picker", "Error String", "Bit Plucker", "Packet Loss", "Corrupt Pick" }, 14, 2, 1, 0.85f, 0.8f, 0.7f, 3, 0.2f, crush | gate, 0 },
                { { "Muted Nylon", "Palm Mute", "Dead String", "Damped Gut", "Soft Thud" }, 14, 0, 0, 0.3f, 0.35f, 0.0f, 0, 0.12f, mono | wood, 0 } },
            { // BELL: struck metal and glass in different worlds
                { { "Orbital Bell", "Satellite Chime", "Low Orbit", "Station Ring", "Docking Bell" }, 1, 1, 1, 0.8f, 4.5f, 0.4f, 3, 0.35f, shimmer, 0 },
                { { "Temple Steel", "Iron Gong", "Bronze Oath", "Monastery Engine", "Ritual Plate" }, 14, 3, 2, 0.55f, 5.5f, 0.8f, 2, 0.4f, octDown | sympath, 0 },
                { { "Ice Spire", "Frozen Lattice", "Cryo Chime", "Glacier Code", "Polar Glass" }, 3, 1, 1, 0.9f, 4.0f, 0.5f, 1, 0.35f, bright | harmonic, 0 },
                { { "Reverse Bell", "Backwards Hour", "Time Inversion", "Rewind Chime", "Anti Bell" }, 1, 1, 1, 0.7f, 3.5f, 0.3f, 0, 0.55f, reverse, 0 },
                { { "FM Cathedral", "Digital Steeple", "Algorithm Bell", "Carrier Ring", "Sideband Choir" }, 12, 1, 0, 0.7f, 4.0f, 0.6f, 0, 0.35f, synthB | vibrato, 11 },
                { { "Tubular Storm", "Pipe Thunder", "Steel Column", "Chrome Tubes", "Foundry Bell" }, 7, 2, 2, 0.75f, 4.5f, 0.7f, 2, 0.3f, ratchet, 0 },
                { { "Music Box Zero", "Clockwork Ghost", "Tin Lullaby", "Broken Carousel", "Wind-up Star" }, 1, 2, 1, 0.85f, 2.0f, 0.2f, 1, 0.25f, tape | arp, 0 },
                { { "Cosmic Gamelan", "Bronze Nebula", "Gamelan Drift", "Kettle Galaxy", "Gong Signal" }, 6, 1, 2, 0.6f, 3.5f, 0.6f, 3, 0.45f, filterLfo | octUp, 0 } },
            { // KEYS: played instruments with hands and pedals
                { { "Velvet Rhodes", "Night Tines", "Satin Electric", "Lounge Reactor", "Smoke Keys" }, 8, 0, 0, 0.5f, 2.2f, 0.4f, 1, 0.2f, vibrato | bass, 0 },
                { { "Clav Funk", "Rubber Clav", "Snap Board", "Wah Machine", "Pickup Clav" }, 14, 2, 0, 0.85f, 0.6f, 0.9f, 0, 0.1f, buzz | sweep, 0 },
                { { "Harpsichord X", "Baroque Engine", "Quill Matrix", "Gilded Wire", "Court Machine" }, 14, 2, 0, 0.8f, 1.4f, 0.6f, 0, 0.25f, harmonic | bright, 0 },
                { { "Vibes Noir", "Motor Vibes", "Jazz Reactor", "Blue Bars", "Late Show" }, 2, 1, 2, 0.6f, 3.0f, 0.4f, 1, 0.25f, filterLfo, 0 },
                { { "Toy Piano Punk", "Plastic Keys", "Broken Toy", "Mini Grand", "Nursery Rave" }, 1, 2, 1, 0.9f, 1.2f, 0.7f, 0, 0.15f, crush | ratchet, 0 },
                { { "Organ Strum", "Chapel Hands", "Drawbar Harp", "Church Machine", "Pipe Hands" }, 14, 0, 0, 0.55f, 2.0f, 0.2f, 2, 0.3f, chord | bass, 0 },
                { { "FM Keys 84", "Digital Grand", "Algorithm Keys", "Sine Piano", "DX Ghost" }, 12, 1, 0, 0.65f, 2.0f, 0.5f, 1, 0.2f, synthB, 10 },
                { { "Pedal Steel Keys", "Slide Board", "Western Glide", "Lap Steel Unit", "Bend Keys" }, 14, 1, 2, 0.6f, 2.5f, 0.4f, 0, 0.2f, vibrato | sympath, 0 } },
            { // PAD: slow, bowed, breathing
                { { "Bowed Glass Pad", "Crystal Bow", "Frozen Strings", "Glass Choir", "Shard Bow" }, 14, 4, 1, 0.6f, 6.0f, 0.4f, 3, 0.45f, bow | shimmer, 0 },
                { { "Analog Dawn", "Warm Horizon", "Tube Sunrise", "Velvet Ocean", "Amber Field" }, 10, 0, 0, 0.45f, 6.0f, 0.5f, 0, 0.4f, synthB | chord | filterLfo, 11 },
                { { "Choir of Strings", "Silent Mass", "Voices Below", "Cathedral Air", "Requiem Strum" }, 9, 0, 0, 0.5f, 6.0f, 0.3f, 2, 0.45f, chord | vibrato, 0 },
                { { "Wavetable Storm", "Morphing Sky", "Data Cloud", "Spectral Tide", "Table Rider" }, 11, 0, 0, 0.6f, 5.0f, 0.7f, 3, 0.4f, synthB | filterLfo, 13 },
                { { "Breath Machine", "Air Organ", "Lung Pad", "Wind Engine", "Exhale" }, 13, 0, 0, 0.5f, 5.0f, 0.5f, 3, 0.45f, vibrato, 0 },
                { { "Bowed Stone", "Granite Drone", "Earth Bow", "Bedrock", "Deep Bow" }, 14, 4, 3, 0.35f, 6.5f, 0.3f, 2, 0.4f, bow | octDown | bass, 0 },
                { { "Reverse Pad", "Inhale Pad", "Backward Sky", "Tide Reversed", "Return Signal" }, 9, 0, 0, 0.55f, 5.0f, 0.4f, 0, 0.6f, reverse | filterLfo, 0 },
                { { "Steel Aurora", "Northern Bow", "Magnetic Field", "Solar Wind", "Ion Veil" }, 14, 4, 2, 0.65f, 6.0f, 0.6f, 3, 0.45f, bow | shimmer | octUp, 0 } },
            { // TEXTURE: broken, worn, moving
                { { "Dusty Archive", "VHS Memory", "Old Signal", "Found Tape", "Lost Broadcast" }, 0, 0, 0, 0.5f, 2.5f, 0.4f, 1, 0.3f, tape | arp, 0 },
                { { "Rust Belt", "Corroded Wire", "Oxide Field", "Scrap Harp", "Junkyard Strum" }, 14, 2, 2, 0.6f, 2.0f, 0.9f, 2, 0.3f, buzz | crush, 0 },
                { { "Radio Ghosts", "Shortwave Harp", "Number Station", "Static Prayer", "AM Dreams" }, 4, 1, 0, 0.6f, 1.6f, 0.5f, 1, 0.25f, gate | tape, 0 },
                { { "Granular Rain", "Particle Storm", "Grain Cloud", "Dust Engine", "Pollen Code" }, 14, 1, 1, 0.75f, 1.2f, 0.6f, 3, 0.4f, ratchet | arp | filterLfo, 0 },
                { { "Underwater City", "Sunken Strings", "Pressure Zone", "Abyss Signal", "Deep Current" }, 14, 0, 3, 0.3f, 3.0f, 0.3f, 2, 0.5f, filterLfo | octDown, 0 },
                { { "Wire Fence", "Barbed Song", "Wind on Wire", "Border Harp", "Cold Fence" }, 14, 4, 2, 0.7f, 4.0f, 0.8f, 3, 0.35f, bow | buzz, 0 },
                { { "Clock Factory", "Gear Harp", "Mechanism", "Escapement", "Watchmaker" }, 6, 2, 2, 0.7f, 0.8f, 0.5f, 0, 0.2f, ratchet | gate | arp, 0 },
                { { "Haunted Nursery", "Doll Song", "Attic Box", "Faded Lullaby", "Ghost Toy" }, 1, 1, 1, 0.7f, 2.5f, 0.3f, 0, 0.45f, tape | reverse | vibrato, 0 } },
            { // HYBRID: a string engine and a synth fighting it out
                { { "Cyber Harp", "Neural Lyre", "Chrome Synapse", "Android Hymn", "Wired Angel" }, 14, 1, 2, 0.75f, 2.5f, 0.5f, 3, 0.35f, synthB | vibrato, 11 },
                { { "Acid String", "Squelch Harp", "303 Lyre", "Resonant Wire", "Rubber Acid" }, 14, 2, 0, 0.8f, 1.2f, 0.7f, 0, 0.15f, synthB | sweep | arp, 10 },
                { { "FM Harp Wars", "Algorithm Strum", "Operator Six", "Carrier Wave", "Modulator Zero" }, 14, 1, 1, 0.7f, 2.0f, 0.6f, 1, 0.3f, synthB | ratchet, 12 },
                { { "Air Harp", "Breathing Wire", "Ghost Lung", "Whisper Engine", "Vapor Strum" }, 14, 3, 1, 0.6f, 3.5f, 0.4f, 3, 0.45f, synthB | shimmer, 13 },
                { { "Bass Harp Hybrid", "Sub Wire", "Low End Harp", "Floor Shaker", "808 Lyre" }, 14, 0, 3, 0.45f, 2.0f, 0.3f, 2, 0.2f, synthB | bass | octDown, 10 },
                { { "Trance Gate Harp", "Sidechain Strum", "Pump Harp", "Euphoria Wire", "Gate Runner" }, 14, 1, 2, 0.8f, 2.5f, 0.5f, 1, 0.3f, synthB | gate | arp, 11 },
                { { "Dub Harp", "Echo Chamber", "Spring Wire", "Delay Ritual", "Version Strum" }, 4, 0, 0, 0.5f, 1.5f, 0.4f, 1, 0.3f, synthB | tape, 13 },
                { { "Hyperpop Harp", "Sugar Rush", "Pitch Candy", "Glitter Bomb", "Bubble Wire" }, 14, 2, 1, 0.95f, 1.0f, 0.8f, 0, 0.25f, synthB | crush | octUp | ratchet, 12 } },
            { // CINEMATIC: big, slow, dangerous
                { { "Titan Fall", "Colossus Rising", "Giant Steps", "Mountain Hall", "Godzilla Strum" }, 14, 3, 3, 0.4f, 6.0f, 0.5f, 2, 0.5f, octDown | bass | synthB, 10 },
                { { "Interstellar Harp", "Wormhole", "Light Year", "Deep Space Strum", "Gravity Well" }, 14, 4, 1, 0.6f, 6.5f, 0.4f, 3, 0.6f, bow | shimmer | synthB, 11 },
                { { "Tension Wire", "Suspense Unit", "Danger Close", "Countdown", "Tripwire" }, 14, 2, 2, 0.7f, 2.5f, 0.9f, 2, 0.35f, ratchet | gate | buzz, 0 },
                { { "Hero Theme", "Rise of Steel", "Victory Strings", "Last Stand", "Sunrise March" }, 9, 0, 0, 0.6f, 5.0f, 0.5f, 0, 0.45f, chord | bass | synthB, 10 },
                { { "Desert Planet", "Spice Wind", "Dune Strings", "Sand Prophet", "Arrakis Air" }, 14, 1, 0, 0.6f, 4.0f, 0.6f, 3, 0.45f, sympath | buzz | filterLfo, 0 },
                { { "Black Hole Bell", "Event Silence", "Singularity", "Collapse", "Dark Star" }, 12, 1, 2, 0.5f, 6.0f, 0.7f, 3, 0.55f, octDown | reverse | synthB, 13 },
                { { "Elegy", "Fallen Kingdom", "Ashes", "Requiem Wire", "Last Light" }, 14, 0, 0, 0.45f, 6.0f, 0.2f, 2, 0.5f, vibrato | sympath | chord, 0 },
                { { "Alien Contact", "First Signal", "Xeno Harp", "Mothership", "Abduction" }, 14, 2, 1, 0.8f, 4.0f, 0.9f, 3, 0.5f, octUp | filterLfo | crush, 12 } } };

        inline void build (Builder& b, int cat, int i)
        {
            const auto& a = arches[cat - 1][i / 5];
            const int var = i % 5;
            auto jit = [&b] (float v, float amt, float lo, float hi) { return juce::jlimit (lo, hi, v + b.range (-amt, amt)); };
            auto fx = [&b] (int slot, int type, float mix, float x, float y)
            {
                const juce::String n ("fx" + juce::String (slot));
                b.set (n + "Type", (float) type); b.set (n + "On", 1.0f); b.set (n + "Mix", mix); b.set (n + "A", x); b.set (n + "B", y);
            };
            int f = a.flags;
            // the five variations: plain, moving (arp), chopped (gate / ratchet), layered (a synth under it), far away (huge space)
            if (var == 1) f |= arp;
            if (var == 2) f |= (cat % 2 ? gate : ratchet);
            if (var == 3) f |= synthB;
            if (var == 4) f |= (cat % 2 ? reverse : shimmer);
            b.set ("harpVoice", (float) a.engine);
            b.set ("harpTone", jit (a.tone, 0.08f, 0.05f, 1.0f));
            b.set ("harpDecay", jit (a.decay, a.decay * 0.2f, 0.2f, 8.0f));
            b.set ("harpShape", jit (a.shape, 0.12f, 0.0f, 1.0f));
            b.set ("harpRange", cat == 1 ? 2.0f : 3.0f);
            if (a.engine == 14)
            {
                b.set ("strExciter", (float) a.touch);
                b.set ("strHard", jit (a.touch == 2 ? 0.85f : a.touch == 3 ? 0.25f : 0.55f, 0.12f, 0.0f, 1.0f));
                b.set ("strPos", b.range (0.05f, 0.7f));
                b.set ("bodyMat", (float) a.mat);
                b.set ("bodySize", b.range (0.15f, 0.9f));
                b.set ("bodyRing", b.range (0.3f, 0.9f));
                b.set ("bodyMix", b.range (0.3f, 0.65f));
                b.set ("bodyDamp", b.range (0.1f, 0.5f));
            }
            if (f & bow) { b.set ("harpSustain", b.range (0.5f, 0.9f)); b.set ("harpAttack", b.range (0.05f, 0.4f)); }
            if (cat == 4 && ! (f & bow)) { b.set ("harpAttack", b.range (0.15f, 0.6f)); b.set ("harpSustain", b.range (0.3f, 0.6f)); }
            // the hand
            b.set ("strumRate", (float) (cat == 4 || cat == 7 ? b.pick ({ 0, 1 }) : b.pick ({ 2, 4, 2, 1 })));
            b.set ("strumFeel", b.range (0.35f, 0.75f));
            b.set ("strumSpeed", cat == 4 || cat == 7 ? b.range (35.0f, 70.0f) : b.range (10.0f, 28.0f));
            if (f & arp) { b.set ("strumMode", 1.0f); b.set ("arpPattern", (float) b.pick ({ 0, 1, 2, 3, 4, 5, 6, 7 })); b.set ("arpOctaves", (float) b.pick ({ 1, 2, 2, 3 })); b.set ("arpSwing", b.range (0.0f, 0.35f)); }
            if (f & gate) { b.set ("gateOn", 1.0f); b.set ("gateRate", (float) b.pick ({ 0, 1, 1, 4 })); b.set ("gateDepth", b.range (0.6f, 1.0f)); }
            if (f & ratchet) b.set ("ratchet", b.range (0.34f, 0.7f));
            // the string itself
            if (f & buzz) b.set ("strBuzz", b.range (0.35f, 0.8f));
            if (f & harmonic) b.set ("strHarmonic", b.range (0.4f, 0.9f));
            if (f & sympath) b.set ("strSympath", b.range (0.3f, 0.7f));
            if (f & mono) b.set ("mono", 1.0f);
            if (f & octDown) b.set ("harpOctave", -1.0f);
            if (f & octUp) b.set ("harpOctave", 1.0f);
            if (f & sweep)   // the pluck envelope: a filter that snaps shut
            {
                b.set ("pT1", 0.02f); b.set ("pL1", 1.0f); b.set ("pT2", b.range (0.15f, 0.35f)); b.set ("pL2", b.range (0.25f, 0.45f));
                b.set ("pT3", b.range (0.3f, 0.6f)); b.set ("pL3", b.range (0.2f, 0.4f)); b.set ("pT4", 0.5f); b.set ("pTail", b.range (0.15f, 0.35f)); b.set ("pVelo", b.range (0.3f, 0.8f));
            }
            if (f & vibrato) { b.set ("pitchModSrc", 0.0f); b.set ("pitchModAmt", b.range (0.12f, 0.25f)); b.set ("lfo1Rate", b.range (4.5f, 6.0f)); b.set ("lfo1Delay", b.range (0.1f, 0.3f)); b.set ("lfo1Fade", b.range (0.2f, 0.5f)); b.set ("lfo1Trig", 1.0f); }
            if (f & filterLfo) { b.set ("filtLfoSel", 1.0f); b.set ("filtLfoAmt", b.range (0.25f, 0.55f)); b.set ("lfo2Rate", b.range (0.06f, 0.4f)); b.set ("filterCutoff", b.range (2500.0f, 7000.0f)); b.set ("filterReso", b.range (0.15f, 0.45f)); }
            if (f & hpf) { b.set ("filtMode", 0.0f); b.set ("filterCutoff", b.range (300.0f, 900.0f)); }
            if (f & wood) b.set ("morphWood", b.range (0.3f, 0.7f));
            if (f & bright) b.set ("bodyTilt", b.range (0.25f, 0.6f));
            // layers
            if (f & synthB) { b.set ("dual", 1.0f); b.set ("harpVoice2", (float) (a.synth > 0 ? a.synth : b.pick ({ 10, 11, 12, 13 }))); b.set ("bShape", b.range (0.15f, 0.85f)); b.set ("bTone", b.range (0.35f, 0.75f)); b.set ("bDecay", b.range (1.0f, 5.0f)); b.set ("bOctave", (float) b.pick ({ 0, -1, 1, 0 })); b.set ("bDetune", b.range (-6.0f, 6.0f)); b.set ("morph", b.range (0.25f, 0.6f)); }
            if (f & bass) { b.set ("bassType", (float) b.pick ({ 1, 3, 1 })); b.set ("bassLevel", b.range (-16.0f, -10.0f)); b.set ("bassTone", b.range (0.3f, 0.6f)); }
            if (f & chord) { b.set ("chordType", (float) b.pick ({ 1, 2, 3, 4 })); b.set ("chordLevel", b.range (-20.0f, -14.0f)); b.set ("chordFilter", b.range (0.35f, 0.6f)); }
            // colour
            if (f & tape) fx (4, 11, b.range (0.45f, 0.8f), b.range (0.4f, 0.8f), b.range (0.3f, 0.6f));
            if (f & crush) fx (5, 4, b.range (0.3f, 0.55f), b.range (0.35f, 0.7f), 0.5f);
            if (f & shimmer) fx (6, 15, b.range (0.3f, 0.5f), 0.85f, 0.6f);
            // the room
            b.set ("spRoom", (float) a.room);
            b.set ("spSize", b.range (0.35f, 0.95f));
            b.set ("spTone", b.range (0.3f, 0.8f));
            b.set ("spPre", b.range (0.0f, 0.45f));
            b.set ("spMix", juce::jlimit (0.0f, 0.85f, a.space + (var == 4 ? 0.25f : 0.0f) + b.range (-0.05f, 0.05f)));
            b.set ("spMod", b.range (0.1f, 0.6f));
            b.set ("spDuck", cat == 1 || cat == 3 ? b.range (0.2f, 0.6f) : 0.0f);
            if (f & reverse) b.set ("spReverse", 1.0f);
            b.set ("fx3Mix", 0.0f);   // the old reverb steps aside for SPACE
        }
    }

    std::vector<Preset> makeFactory()
    {
        std::vector<Preset> out;
        for (int c = 0; c < 8; ++c)
            for (int i = 0; i < 40; ++i)
            {
                const bool extra = c >= 6 || i >= 24;
                Builder b;
                b.r.setSeed ((juce::int64) (1081 + c * 977 + i * 31));
                Preset p;
                p.name = extra ? juce::String (extraNames[(size_t) c][(size_t) (c >= 6 ? i : i - 24)]) : juce::String (names[c][i]);
                if (c >= 1) p.name = design::arches[c - 1][i / 5].names[i % 5];
                p.category = categories()[c];
                p.model = c == 5 ? 4 : 1;   // presets are not tied to a character: Classic for all, Lo-fi for textures
                b.set ("model", (float) p.model);

                // musical defaults shared by every preset: clean chorus, a real room, gentle vibrato
                b.set ("fx1Mix", b.range (0.15f, 0.28f));   // chorus: width, not wobble
                b.set ("fx1A", b.range (0.15f, 0.3f));
                b.set ("fx1B", b.range (0.15f, 0.3f));
                b.set ("fx2Mix", b.range (0.08f, 0.18f));   // delay
                b.set ("fx2A", (float) b.pick ({ 2, 3, 3, 5 }) / 6.0f + 0.01f);
                b.set ("fx2B", b.range (0.25f, 0.4f));
                b.set ("fx3Mix", b.range (0.25f, 0.4f));    // reverb
                b.set ("fx3A", b.range (0.55f, 0.75f));
                b.set ("fx3B", b.range (0.35f, 0.55f));
                b.set ("vibDepth", b.chance (0.25f) ? b.range (0.01f, 0.03f) : 0.0f);   // clean pitch: vibrato only now and then, and tiny
                b.set ("vibRate", b.range (4.5f, 5.8f));

                switch (c)
                {
                    case 0: // Harp: the HARP STRING engine, eight characters, each one far from the others
                    {
                        static const char* const harpNames[8][5] = {
                            { "Concert Steel", "Titan Harp", "Chrome Lyre", "Silver Citadel", "Iron Choir" },
                            { "Glass Cathedral", "Crystal Throne", "Prism Rain", "Frozen Orbit", "Halo Strings" },
                            { "Razor Wire", "Edge of Steel", "Cutting Light", "Steel Fang", "Chrome Teeth" },
                            { "Obsidian", "Deep Core", "Basalt Hymn", "Tectonic", "Grave Orbit" },
                            { "Bowed Void", "Event Horizon", "Dark Matter Bow", "Ghost Signal", "Infinite Night" },
                            { "Neon Rain", "Night City Harp", "Hologram Rain", "Synthwave Lyre", "Electric Dusk" },
                            { "Monolith", "Ancient Machine", "Stone Engine", "Colossus", "Temple of Rust" },
                            { "Ion Storm", "Plasma Harp", "Circuit Storm", "Fractured Signal", "Static Storm" } };
                        const int arch = i / 5, var = i % 5;
                        p.name = harpNames[arch][var];
                        b.set ("harpVoice", 14.0f);
                        b.set ("harpRange", 3.0f);
                        b.set ("strumRate", (float) b.pick ({ 2, 2, 1, 3 }));
                        b.set ("strumFeel", b.range (0.4f, 0.7f));
                        b.set ("strumSpeed", b.range (14.0f, 30.0f));
                        auto fx = [&b] (int slot, int type, float mix, float a, float bb)
                        {
                            const juce::String n ("fx" + juce::String (slot));
                            b.set (n + "Type", (float) type); b.set (n + "On", 1.0f); b.set (n + "Mix", mix); b.set (n + "A", a); b.set (n + "B", bb);
                        };
                        switch (arch)
                        {
                            case 0: // concert steel: nail on wood, crisp and proud
                                b.set ("strExciter", 1.0f); b.set ("strHard", b.range (0.45f, 0.7f)); b.set ("strPos", b.range (0.15f, 0.35f));
                                b.set ("bodyMat", 0.0f); b.set ("bodySize", b.range (0.4f, 0.7f)); b.set ("bodyRing", b.range (0.3f, 0.5f)); b.set ("bodyMix", b.range (0.3f, 0.45f)); b.set ("bodyDamp", 0.25f);
                                b.set ("harpTone", b.range (0.6f, 0.8f)); b.set ("harpDecay", b.range (2.5f, 4.0f)); b.set ("harpShape", b.range (0.1f, 0.3f));
                                fx (4, 2, 0.6f, 0.4f, 0.4f);
                                break;
                            case 1: // glass cathedral: felt on a glass bar that rings forever
                                b.set ("strExciter", 3.0f); b.set ("strHard", b.range (0.2f, 0.5f)); b.set ("strPos", b.range (0.3f, 0.6f));
                                b.set ("bodyMat", 1.0f); b.set ("bodySize", b.range (0.2f, 0.6f)); b.set ("bodyRing", b.range (0.6f, 0.95f)); b.set ("bodyMix", b.range (0.5f, 0.75f)); b.set ("bodyDamp", 0.1f);
                                b.set ("harpTone", b.range (0.65f, 0.9f)); b.set ("harpDecay", b.range (4.0f, 6.0f)); b.set ("harpShape", b.range (0.2f, 0.5f));
                                fx (4, 15, b.range (0.3f, 0.45f), 0.8f, 0.6f);
                                b.set ("fx3Mix", b.range (0.45f, 0.6f)); b.set ("fx3A", 0.85f);
                                break;
                            case 2: // razor wire: a pick on a steel plate, stiff strings, driven
                                b.set ("strExciter", 2.0f); b.set ("strHard", b.range (0.8f, 1.0f)); b.set ("strPos", b.range (0.02f, 0.15f));
                                b.set ("bodyMat", 2.0f); b.set ("bodySize", b.range (0.3f, 0.7f)); b.set ("bodyRing", b.range (0.3f, 0.6f)); b.set ("bodyMix", b.range (0.35f, 0.55f)); b.set ("bodyDamp", 0.3f);
                                b.set ("harpTone", b.range (0.75f, 0.95f)); b.set ("harpDecay", b.range (1.4f, 2.6f)); b.set ("harpShape", b.range (0.6f, 0.95f));
                                fx (4, 3, b.range (0.35f, 0.6f), b.range (0.4f, 0.7f), 0.6f);
                                fx (5, 13, 0.25f, 0.35f, 0.45f);
                                break;
                            case 3: // obsidian: fingers on stone, low and heavy, close to the ear
                                b.set ("strExciter", 0.0f); b.set ("strHard", b.range (0.1f, 0.35f)); b.set ("strPos", b.range (0.45f, 0.8f));
                                b.set ("bodyMat", 3.0f); b.set ("bodySize", b.range (0.6f, 1.0f)); b.set ("bodyRing", b.range (0.4f, 0.8f)); b.set ("bodyMix", b.range (0.45f, 0.7f)); b.set ("bodyDamp", 0.4f);
                                b.set ("harpTone", b.range (0.3f, 0.5f)); b.set ("harpDecay", b.range (2.0f, 3.5f)); b.set ("harpShape", b.range (0.0f, 0.2f));
                                b.set ("harpOctave", -1.0f);
                                fx (4, 11, 0.5f, 0.5f, 0.2f);
                                break;
                            case 4: // bowed void: the bow keeps the strings singing in an endless space
                                b.set ("strExciter", 4.0f); b.set ("strHard", b.range (0.3f, 0.6f)); b.set ("strPos", b.range (0.2f, 0.5f));
                                b.set ("bodyMat", (float) b.pick ({ 1, 2 })); b.set ("bodySize", b.range (0.3f, 0.8f)); b.set ("bodyRing", b.range (0.5f, 0.9f)); b.set ("bodyMix", b.range (0.3f, 0.5f)); b.set ("bodyDamp", 0.2f);
                                b.set ("harpTone", b.range (0.45f, 0.7f)); b.set ("harpDecay", b.range (5.0f, 7.0f)); b.set ("harpSustain", b.range (0.5f, 0.9f)); b.set ("harpAttack", b.range (0.08f, 0.3f));
                                b.set ("harpShape", b.range (0.2f, 0.5f));
                                fx (4, 15, b.range (0.35f, 0.5f), 0.9f, 0.7f);
                                b.set ("fx3Mix", b.range (0.5f, 0.65f)); b.set ("fx3A", 0.92f);
                                break;
                            case 5: // neon rain: nails on steel through wide chorus and ping-pong echoes
                                b.set ("strExciter", 1.0f); b.set ("strHard", b.range (0.55f, 0.85f)); b.set ("strPos", b.range (0.1f, 0.3f));
                                b.set ("bodyMat", 2.0f); b.set ("bodySize", b.range (0.1f, 0.4f)); b.set ("bodyRing", b.range (0.4f, 0.7f)); b.set ("bodyMix", b.range (0.3f, 0.5f)); b.set ("bodyDamp", 0.2f);
                                b.set ("harpTone", b.range (0.7f, 0.9f)); b.set ("harpDecay", b.range (2.0f, 3.5f)); b.set ("harpShape", b.range (0.3f, 0.6f));
                                fx (4, 13, b.range (0.3f, 0.45f), (float) b.pick ({ 2, 3 }) / 6.0f + 0.01f, 0.5f);
                                fx (5, 20, 0.7f, 0.8f, 0.6f);
                                b.set ("fx1Mix", b.range (0.35f, 0.5f));
                                break;
                            case 6: // monolith: felt on stone and a slow tape, an old machine in a cave
                                b.set ("strExciter", 3.0f); b.set ("strHard", b.range (0.35f, 0.55f)); b.set ("strPos", b.range (0.25f, 0.5f));
                                b.set ("bodyMat", 3.0f); b.set ("bodySize", b.range (0.35f, 0.65f)); b.set ("bodyRing", b.range (0.6f, 1.0f)); b.set ("bodyMix", b.range (0.4f, 0.6f)); b.set ("bodyDamp", 0.15f);
                                b.set ("harpTone", b.range (0.45f, 0.65f)); b.set ("harpDecay", b.range (3.0f, 5.0f)); b.set ("harpShape", b.range (0.4f, 0.7f));
                                fx (4, 11, 0.7f, 0.7f, 0.6f);
                                fx (5, 14, 0.5f, 0.9f, 0.3f);
                                break;
                            default: // ion storm: picks on glass through crushed and torn space
                                b.set ("strExciter", 2.0f); b.set ("strHard", b.range (0.7f, 1.0f)); b.set ("strPos", b.range (0.05f, 0.25f));
                                b.set ("bodyMat", 1.0f); b.set ("bodySize", b.range (0.0f, 0.4f)); b.set ("bodyRing", b.range (0.5f, 0.8f)); b.set ("bodyMix", b.range (0.4f, 0.6f)); b.set ("bodyDamp", 0.2f);
                                b.set ("harpTone", b.range (0.7f, 0.95f)); b.set ("harpDecay", b.range (1.5f, 3.0f)); b.set ("harpShape", b.range (0.5f, 0.9f));
                                fx (4, 4, b.range (0.3f, 0.5f), b.range (0.4f, 0.6f), 0.5f);
                                fx (5, 9, 0.5f, 0.4f, 0.6f);
                                fx (6, 18, 0.4f, 0.5f, 0.5f);
                                break;
                        }
                        if (var == 4) { b.set ("dual", 1.0f); b.set ("harpVoice2", (float) b.pick ({ 9, 13, 11 })); b.set ("morph", b.range (0.2f, 0.35f)); }
                        if (var == 3 && arch % 2 == 0) { b.set ("strumMode", 1.0f); b.set ("arpPattern", (float) b.pick ({ 0, 2, 3, 4 })); }
                        break;
                    }
                    case 1: case 2: case 3: case 4: case 5: case 6: case 7:
                        design::build (b, c, i);
                        break;
                    case 100: // (the older recipes, kept for reference)
                        b.set ("harpVoice", (float) b.pick ({ 4, 4, 4, 6, 7, 2 }));
                        b.set ("harpTone", b.range (0.5f, 0.75f));
                        b.set ("harpDecay", b.range (0.6f, 1.3f));
                        b.set ("harpRange", 2.0f);
                        b.set ("strumRate", (float) b.pick ({ 4, 2, 4 }));
                        b.set ("strumFeel", b.range (0.35f, 0.6f));
                        b.set ("strumSpeed", b.range (10.0f, 18.0f));
                        if (b.chance (0.5f)) { b.set ("strumMode", 1.0f); b.set ("arpPattern", (float) b.pick ({ 0, 2, 3, 4 })); b.set ("arpChance", b.range (0.85f, 1.0f)); }
                        b.set ("fx2Mix", b.range (0.15f, 0.28f));
                        b.set ("fx3Mix", b.range (0.15f, 0.28f));
                        break;
                    case 102: // Bell: long, glassy, spacious
                        b.set ("harpVoice", (float) b.pick ({ 1, 3, 2, 1, 7, 0 }));
                        b.set ("harpTone", b.range (0.6f, 0.8f));
                        b.set ("harpDecay", b.range (3.0f, 5.0f));
                        b.set ("harpRange", (float) b.pick ({ 3, 4 }));
                        b.set ("strumRate", (float) b.pick ({ 1, 0, 1 }));
                        b.set ("strumFeel", b.range (0.35f, 0.55f));
                        b.set ("strumSpeed", b.range (30.0f, 55.0f));
                        if (b.chance (0.5f)) { b.set ("fx4Type", 15.0f); b.set ("fx4Mix", b.range (0.15f, 0.28f)); b.set ("fx4A", 0.7f); b.set ("fx4B", 0.5f); }
                        b.set ("macroDream", b.range (0.08f, 0.22f));
                        if (b.chance (0.6f)) { b.set ("lfo2QAmt", b.range (0.15f, 0.28f)); b.set ("lfo2Rate", b.range (0.12f, 0.35f)); }   // bells drift across the stereo field
                        b.set ("fx3Mix", b.range (0.38f, 0.5f));
                        b.set ("fx3A", b.range (0.75f, 0.88f));
                        break;
                    case 103: // Keys: tines or vibes over a soft organ and a round bass
                        b.set ("harpVoice", (float) b.pick ({ 8, 8, 2, 8, 6, 2 }));
                        b.set ("harpTone", b.range (0.45f, 0.65f));
                        b.set ("harpDecay", b.range (1.4f, 2.4f));
                        b.set ("strumRate", (float) b.pick ({ 2, 1 }));
                        b.set ("strumFeel", b.range (0.35f, 0.55f));
                        b.set ("strumSpeed", b.range (18.0f, 30.0f));
                        b.set ("chordType", (float) b.pick ({ 1, 1, 2 }));
                        b.set ("chordLevel", b.range (-22.0f, -16.0f));
                        b.set ("chordFilter", b.range (0.35f, 0.55f));
                        b.set ("bassType", (float) b.pick ({ 1, 1, 2, 3 }));
                        b.set ("bassMode", (float) b.pick ({ 0, 1, 2 }));
                        b.set ("bassLevel", b.range (-18.0f, -12.0f));
                        b.set ("bassTone", b.range (0.3f, 0.5f));
                        if (b.chance (0.4f)) { b.set ("fx4Type", 11.0f); b.set ("fx4Mix", b.range (0.25f, 0.45f)); b.set ("fx4A", 0.3f); b.set ("fx4B", 0.25f); }
                        if (b.chance (0.45f)) { b.set ("lfo1QDst", 22.0f); b.set ("lfo1QAmt", b.range (0.12f, 0.22f)); b.set ("lfo1Rate", b.range (3.8f, 5.6f)); }   // tine tremolo
                        break;
                    case 104: // Pad: slow strums over a warm layer
                        b.set ("harpVoice", (float) b.pick ({ 9, 5, 9, 1, 0, 9 }));
                        b.set ("harpTone", b.range (0.4f, 0.6f));
                        b.set ("harpDecay", b.range (4.0f, 6.5f));
                        b.set ("chordType", (float) b.pick ({ 2, 3, 4 }));
                        b.set ("chordLevel", b.range (-21.0f, -15.5f));
                        b.set ("chordFilter", b.range (0.4f, 0.6f));
                        b.set ("lfo1QAmt", b.range (0.18f, 0.35f));   // slow filter breathing
                        b.set ("lfo1Rate", b.range (0.08f, 0.25f));
                        if (b.chance (0.5f)) { b.set ("lfo2QAmt", b.range (0.2f, 0.35f)); b.set ("lfo2Rate", b.range (0.1f, 0.2f)); }
                        b.set ("strumRate", (float) b.pick ({ 0, 1 }));
                        b.set ("strumFeel", b.range (0.3f, 0.5f));
                        b.set ("strumSpeed", b.range (40.0f, 70.0f));
                        b.set ("macroDream", b.range (0.15f, 0.35f));
                        b.set ("fx3Mix", b.range (0.42f, 0.55f));
                        b.set ("fx3A", 0.85f);
                        if (b.chance (0.5f)) { b.set ("dual", 1.0f); b.set ("harpVoice2", (float) b.pick ({ 9, 5 })); b.set ("morph", b.range (0.35f, 0.55f)); }
                        break;
                    case 106: // Hybrid: a harp engine on A, a synth engine on B, MORPH between them
                        b.set ("harpVoice", (float) b.pick ({ 0, 0, 1, 3, 4, 7, 2, 6 }));
                        b.set ("harpTone", b.range (0.5f, 0.75f));
                        b.set ("harpDecay", b.range (1.6f, 3.5f));
                        b.set ("harpRange", 3.0f);
                        b.set ("dual", 1.0f);
                        b.set ("harpVoice2", (float) (10 + i % 4));
                        b.set ("bShape", b.range (0.1f, 0.9f));
                        b.set ("bTone", b.range (0.4f, 0.7f));
                        b.set ("bDecay", b.range (1.5f, 4.0f));
                        b.set ("bOctave", (float) b.pick ({ 0, 0, 0, -1, 1 }));
                        b.set ("bDetune", b.range (-3.0f, 3.0f));
                        if (b.chance (0.35f)) b.set ("bAttack", b.range (0.05f, 0.4f));
                        b.set ("morph", b.range (0.3f, 0.6f));
                        b.set ("strumRate", (float) b.pick ({ 2, 1, 4 }));
                        b.set ("strumFeel", b.range (0.4f, 0.6f));
                        b.set ("strumSpeed", b.range (14.0f, 32.0f));
                        if (b.chance (0.5f))   // the layer mix breathes
                        { b.set ("lfo1QDst", (float) mod::targetIds().indexOf ("morph")); b.set ("lfo1QAmt", b.range (0.15f, 0.3f)); b.set ("lfo1Rate", b.range (0.05f, 0.2f)); }
                        else
                        { b.set ("lfo1QDst", (float) mod::targetIds().indexOf ("bShape")); b.set ("lfo1QAmt", b.range (0.15f, 0.35f)); b.set ("lfo1Rate", b.range (0.08f, 0.3f)); }
                        break;
                    case 107: // Cinematic: slow, wide, a low layer and a shimmering tail
                        b.set ("harpVoice", (float) b.pick ({ 0, 3, 1, 9, 7, 4 }));
                        b.set ("harpTone", b.range (0.45f, 0.7f));
                        b.set ("harpDecay", b.range (3.5f, 6.5f));
                        b.set ("harpSustain", b.range (0.1f, 0.35f));
                        b.set ("harpRange", (float) b.pick ({ 3, 4 }));
                        if (b.chance (0.35f)) b.set ("harpAttack", b.range (0.1f, 0.6f));
                        b.set ("dual", 1.0f);
                        b.set ("harpVoice2", (float) b.pick ({ 10, 11, 13, 9, 11 }));
                        b.set ("bOctave", (float) b.pick ({ -1, -1, 0, 1 }));
                        b.set ("bAttack", b.range (0.15f, 0.7f));
                        b.set ("bLevel", b.range (-7.0f, -4.0f));
                        b.set ("bDecay", b.range (3.0f, 5.0f));
                        b.set ("bShape", b.range (0.2f, 0.8f));
                        b.set ("bTone", b.range (0.35f, 0.6f));
                        b.set ("morph", b.range (0.35f, 0.55f));
                        b.set ("strumRate", (float) b.pick ({ 0, 1, 1 }));
                        b.set ("strumFeel", b.range (0.3f, 0.5f));
                        b.set ("strumSpeed", b.range (35.0f, 70.0f));
                        b.set ("fx3Type", 15.0f); b.set ("fx3On", 1.0f); b.set ("fx3A", b.range (0.75f, 0.9f)); b.set ("fx3B", b.range (0.45f, 0.6f)); b.set ("fx3Mix", b.range (0.25f, 0.36f));
                        b.set ("fx5Type", 20.0f); b.set ("fx5On", 1.0f); b.set ("fx5A", b.range (0.6f, 0.8f)); b.set ("fx5B", 0.55f); b.set ("fx5Mix", 1.0f);
                        b.set ("drive", b.range (0.0f, 0.12f));
                        b.set ("macroDream", b.range (0.1f, 0.25f));
                        b.set ("harpTone", b.range (0.4f, 0.6f));
                        b.set ("lfo2QDst", (float) mod::targetIds().indexOf ("filterCutoff")); b.set ("lfo2QAmt", b.range (0.08f, 0.18f)); b.set ("lfo2Rate", b.range (0.04f, 0.12f));
                        break;
                    case 105: // Texture: well-loved tape, gentle movement
                        b.set ("harpVoice", (float) b.pick ({ 0, 1, 2, 3, 8, 4 }));
                        b.set ("harpTone", b.range (0.45f, 0.7f));
                        b.set ("harpDecay", b.range (1.2f, 3.0f));
                        if (b.chance (0.6f)) { b.set ("strumMode", 1.0f); b.set ("arpPattern", (float) b.pick ({ 5, 4, 2 })); b.set ("arpChance", b.range (0.65f, 0.85f)); b.set ("arpRegen", (float) b.pick ({ 1, 2, 3 })); b.set ("strumRate", (float) b.pick ({ 2, 4 })); }
                        b.set ("macroAge", b.range (0.15f, 0.45f));
                        b.set ("macroWobble", b.range (0.1f, 0.3f));
                        if (b.chance (0.6f)) { b.set ("randQAmt", b.range (0.02f, 0.05f)); b.set ("randSmooth", b.range (0.7f, 0.9f)); b.set ("randRate", b.range (0.5f, 1.5f)); }   // tape pitch drift
                        b.set ("fx4Type", (float) b.pick ({ 11, 11, 9, 8, 4 }));
                        b.set ("fx4Mix", b.range (0.25f, 0.45f));
                        b.set ("fx4A", b.range (0.25f, 0.5f));
                        b.set ("fx4B", b.range (0.25f, 0.5f));
                        b.set ("mod1Src", 1.0f); b.set ("mod1Dst", 1.0f); b.set ("mod1Amt", b.range (0.1f, 0.2f));
                        b.set ("lfo1Rate", b.range (0.15f, 0.6f));
                        break;
                }
                // the added presets of the classic types get a synth layer under the harp
                if (c == 0 && i >= 24 && false)
                {
                    static const int engines[6][4] = { { 13, 11, 10, 12 }, { 12, 10, 11, 13 }, { 12, 11, 13, 12 }, { 10, 12, 11, 10 }, { 10, 11, 13, 10 }, { 13, 11, 10, 12 } };
                    b.set ("dual", 1.0f);
                    b.set ("harpVoice2", (float) engines[c][i % 4]);
                    b.set ("bShape", b.range (0.15f, 0.85f));
                    b.set ("bTone", b.range (0.4f, 0.7f));
                    b.set ("bDecay", c == 1 ? b.range (0.6f, 1.4f) : b.range (1.8f, 4.5f));
                    b.set ("bOctave", (float) b.pick ({ 0, 0, -1, 1 }));
                    b.set ("bDetune", b.range (-3.0f, 3.0f));
                    if (c == 4) b.set ("bAttack", b.range (0.15f, 0.8f));
                    b.set ("morph", c == 4 ? b.range (0.45f, 0.7f) : b.range (0.25f, 0.5f));
                }
                // stereo image per type: plucks and keys focused, pads, hybrids and cinematic wide
                {
                    static const float spreadLo[] = { 0.45f, 0.35f, 0.5f, 0.35f, 0.6f, 0.5f, 0.6f, 0.65f };
                    static const float widthLo[]  = { 1.0f, 0.95f, 1.05f, 1.0f, 1.15f, 1.05f, 1.15f, 1.2f };
                    if (c >= 4 || i >= 24 || c == 2)
                    {
                        b.set ("spread", b.range (spreadLo[c], spreadLo[c] + 0.25f));
                        b.set ("width", b.range (widthLo[c], widthLo[c] + 0.2f));
                    }
                }
                // modern colour on top of the era sound
                if (c == 0 && (p.model == 0 || p.model == 4))   // Warm and Lo-fi are narrow by nature: open them up (mono-safe)
                {
                    b.set ("fx5Type", 20.0f); b.set ("fx5On", 1.0f); b.set ("fx5A", b.range (0.5f, 0.7f)); b.set ("fx5B", b.range (0.35f, 0.6f)); b.set ("fx5Mix", 1.0f);
                }
                if (i % 6 == 3 && c == 0)   // PRISM: the sound glows on the chord, a slow LFO scans it
                {
                    b.set ("fx4Type", 17.0f); b.set ("fx4On", 1.0f);
                    b.set ("fx4A", b.range (0.3f, 0.7f)); b.set ("fx4B", b.range (0.35f, 0.7f)); b.set ("fx4Mix", b.range (0.35f, 0.6f));
                    b.set ("lfo2QDst", (float) mod::targetIds().indexOf ("fx4A")); b.set ("lfo2QAmt", b.range (0.2f, 0.35f));
                    b.set ("lfo2Rate", b.range (0.05f, 0.25f));
                }
                if (false && i % 6 == 5 && (c == 1 || c == 2))   // DISPERSE: laser-like attack on plucks and bells
                {
                    b.set ("fx4Type", 18.0f); b.set ("fx4On", 1.0f);
                    b.set ("fx4A", b.range (0.3f, 0.6f)); b.set ("fx4B", b.range (0.3f, 0.6f)); b.set ("fx4Mix", b.range (0.35f, 0.6f));
                }
                if (false && i % 6 == 1 && (c == 5 || c == 3))   // SHAPE: morphing grit on textures and keys
                {
                    b.set ("fx4Type", 19.0f); b.set ("fx4On", 1.0f);
                    b.set ("fx4A", b.range (0.2f, 0.8f)); b.set ("fx4B", b.range (0.15f, 0.35f)); b.set ("fx4Mix", b.range (0.25f, 0.45f));
                }
                // harp bent into strings and into bass: the same instrument, a different job
                if (false && c == 4 && i < 4)
                {
                    static const char* sn[] = { "String Harp", "String Harp Wide", "String Harp Warm", "String Harp Glow" };
                    p.name = sn[i];
                    b.set ("harpVoice", 9.0f); b.set ("harpAttack", b.range (0.12f, 0.35f)); b.set ("harpSustain", b.range (0.5f, 0.8f));
                    b.set ("harpRelease", b.range (0.8f, 1.6f)); b.set ("drive", b.range (0.2f, 0.4f)); b.set ("filterCutoff", b.range (6000.0f, 12000.0f));
                    b.set ("fx5Type", 20.0f); b.set ("fx5On", 1.0f); b.set ("fx5A", 0.65f); b.set ("fx5B", 0.5f); b.set ("fx5Mix", 1.0f);
                }
                if (false && c == 1 && i >= 20 && i < 24)
                {
                    // EPIC HARP: big modern cinematic harp, wide, glowing on the chord, shimmering tail
                    static const char* en[] = { "Epic Harp", "Epic Harp Rise", "Cinematic Harp", "Epic Harp Low" };
                    p.name = en[i - 20];
                    b.set ("harpVoice", (float) b.pick ({ 3, 1, 7 })); b.set ("dual", 1.0f); b.set ("harpVoice2", 9.0f); b.set ("morph", b.range (0.3f, 0.45f));
                    b.set ("harpRange", 3.0f); b.set ("harpDecay", b.range (3.0f, 5.0f)); b.set ("harpSustain", 0.6f);
                    if (i == 21) b.set ("harpAttack", 0.35f);                       // the rise: a swelling attack
                    if (i == 23) { b.set ("harpOctave", -1.0f); b.set ("drive", 0.35f); }
                    else b.set ("drive", b.range (0.12f, 0.22f));
                    b.set ("fx3Type", 15.0f); b.set ("fx3On", 1.0f); b.set ("fx3A", 0.8f); b.set ("fx3B", 0.55f); b.set ("fx3Mix", 0.4f);   // shimmer
                    b.set ("fx4Type", 17.0f); b.set ("fx4On", 1.0f); b.set ("fx4A", 0.55f); b.set ("fx4B", 0.5f); b.set ("fx4Mix", 0.35f);   // prism
                    b.set ("lfo2QDst", (float) mod::targetIds().indexOf ("fx4A")); b.set ("lfo2QAmt", 0.3f); b.set ("lfo2Rate", 0.08f);
                    b.set ("fx5Type", 20.0f); b.set ("fx5On", 1.0f); b.set ("fx5A", 0.75f); b.set ("fx5B", 0.55f); b.set ("fx5Mix", 1.0f);     // wide
                }
                {
                    // measured: every preset as loud as the others (a C major chord, 100 velocity)
                    static const float trims[320] = { -0.9f, 1.9f, 2.0f, 9.4f, -0.2f, -5.3f, 1.3f, 0.5f, 1.1f, -2.6f, 6.1f, 0.9f, 1.1f, 9.5f, -2.3f, -3.9f, -2.0f, -5.4f, -5.3f, -6.7f, 2.5f, -6.0f, -0.7f, 3.4f, 5.0f, 0.5f, -2.6f, -2.2f, -2.8f, -0.2f, 8.0f, 24.0f, 22.3f, 9.7f, 17.6f, 12.5f, 1.1f, 17.1f, 16.7f, -3.0f, 9.3f, -3.1f, 7.9f, 6.1f, 3.8f, -2.7f, -3.1f, -4.9f, -4.5f, 0.6f, -5.3f, 1.5f, -2.9f, 0.3f, -4.9f, 14.1f, 18.0f, 18.0f, 5.9f, 12.8f, 9.2f, 4.4f, 4.2f, 7.8f, 10.9f, 11.0f, 11.2f, 12.1f, 5.6f, 12.7f, 1.2f, 3.8f, 8.8f, 5.5f, -3.0f, -3.2f, 2.4f, 2.5f, -0.9f, -1.7f, 8.4f, 4.8f, 4.3f, 5.8f, 7.9f, -12.0f, -12.0f, -12.0f, -10.4f, -8.4f, 0.2f, 6.6f, 5.8f, 6.5f, 11.0f, 6.7f, 7.4f, 4.9f, 4.2f, 8.7f, 3.1f, 2.8f, 1.4f, 4.1f, 4.4f, 0.5f, 2.1f, 3.2f, 1.8f, 4.4f, 4.1f, 2.1f, -0.1f, 0.3f, 0.4f, 11.3f, 9.6f, 10.7f, 4.9f, 14.6f, 4.8f, 4.8f, 5.3f, 5.9f, 6.7f, 13.5f, 10.8f, 15.7f, 9.8f, 14.5f, 17.5f, 14.8f, 18.0f, 10.5f, 18.4f, 6.8f, 5.6f, 6.4f, 2.5f, 6.5f, 8.8f, 7.1f, 6.1f, 0.3f, 7.0f, -7.0f, -4.9f, 4.8f, -0.4f, -3.5f, 4.0f, 5.3f, 4.5f, 4.8f, 4.8f, -1.6f, -1.8f, -12.0f, 0.1f, -4.0f, -6.6f, -0.9f, -1.6f, -2.3f, 0.3f, -2.8f, -3.6f, -3.0f, -4.4f, -2.0f, -5.2f, -5.1f, -4.9f, -3.8f, -3.0f, -4.0f, -4.7f, -4.6f, -3.9f, -2.7f, -6.6f, -6.1f, -7.7f, -7.2f, -5.6f, 1.7f, 3.0f, 4.7f, 21.7f, 13.0f, 1.3f, 2.3f, -1.8f, -2.1f, 2.6f, 6.1f, 3.8f, 6.4f, -1.7f, -4.4f, -1.0f, 11.8f, 13.2f, 16.5f, 12.0f, 10.8f, 0.2f, -4.7f, 5.0f, 3.3f, 5.8f, 6.0f, 9.3f, -1.6f, 4.5f, -7.7f, -4.0f, -3.0f, -3.8f, -4.1f, -5.0f, -4.8f, -6.3f, -4.7f, -6.9f, -6.8f, -12.0f, -12.0f, -12.0f, -7.0f, -6.0f, 9.5f, 10.0f, 2.1f, 11.6f, 0.8f, -0.2f, 1.5f, 0.2f, 1.9f, -7.5f, -7.4f, -4.4f, -5.5f, 4.4f, 6.4f, 8.5f, 4.4f, 9.4f, 11.4f, -5.0f, -3.3f, -5.2f, -6.2f, -1.2f, -3.5f, -2.0f, -6.2f, -4.9f, -6.8f, -4.6f, -3.9f, -4.7f, -6.8f, 0.7f, -2.8f, -4.0f, -7.4f, -5.6f, -2.6f, -4.7f, -1.7f, 0.4f, 0.4f, 0.5f, 3.9f, 9.0f, 3.8f, 4.5f, 7.6f, -4.2f, -2.0f, -5.7f, -2.1f, -0.4f, 4.1f, 1.7f, -3.9f, -2.6f, -4.3f, -1.0f, -2.4f, 11.1f, -3.8f, 6.0f, 5.1f, 1.9f, 1.1f, 0.6f, 5.5f, -2.8f, -5.5f, 5.1f, 3.5f, -2.9f, 2.8f, 2.1f, 3.8f, 2.9f, 1.7f, -5.7f, -6.8f, -1.1f, -6.7f, -5.6f, -7.4f, -5.3f, 9.6f, 4.5f, 3.9f };
                    b.set ("presetTrim", trims[c * 40 + i]);
                    // HARP TYPE: every preset plays like one instrument (its strum, its flourish, its touch)
                    // harp archetypes: steel, glass, razor, obsidian, bowed, neon, monolith, ion
                    static const int harpMain[8] = { 0, 6, 1, 8, 4, 5, 2, 7 }, harpAlt[8] = { 6, 4, 3, 8, 0, 7, 8, 5 };
                    b.set ("harpType", (float) (c == 0 ? ((i % 5) < 3 ? harpMain[i / 5] : harpAlt[i / 5]) : (i * 7 + c * 4) % 9));
                }
                {
                    // TAILS: measured after the key goes up; every preset stops in a time that suits its kind
                    // (plucks 1.5 s, keys 2 s, harps 2.5 s, bells 3 s, pads and textures 3.5 s, cinematic 4 s)
                    static const int tailLevel[320] = { 0, 0, 0, 0, 0, 3, 3, 2, 3, 3, 0, 0, 0, 0, 0, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 1, 1, 2, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 1, 0, 0, 0, 3, 0, 0, 1, 0, 3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 2, 1, 0, 0, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 2, 1, 2, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 2, 1, 1, 0, 1, 0, 1, 2, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 3, 2, 3, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 };
                    tame (b.v, tailLevel[c * 40 + i]);
                    // seven presets that sounded like a neighbour: each gets its own colour
                    switch (c * 40 + i)
                    {
                        case 11:  b.set ("harpOctave", 1.0f); b.set ("harpTone", 0.9f); b.set ("harpType", 7.0f); break;     // Edge of Steel: high, bright, chromatic
                        case 18:  b.set ("harpOctave", -1.0f); b.set ("drive", 0.45f); b.set ("harpType", 8.0f); break;    // Tectonic: thunder low and driven
                        case 133: b.set ("harpVoice", 7.0f); b.set ("harpTone", 0.82f); b.set ("strBuzz", 0.35f); break;    // Gilded Wire: steel that buzzes
                        case 172: b.set ("harpOctave", -1.0f); b.set ("harpTone", 0.3f); break;                             // Voices Below: low and dark
                        case 191: b.set ("harpAttack", 0.6f); b.set ("filtLfoAmt", 0.55f); break;                           // Inhale Pad: breathes in, filter moves
                        case 262: b.set ("harpType", 8.0f); b.set ("strBuzz", 0.55f); b.set ("harpTone", 0.6f); break;       // Low End Harp: rattling thunder
                        case 314: b.set ("harpOctave", 1.0f); b.set ("harpAttack", 0.8f); b.set ("harpTone", 0.85f); break;  // Last Light: high, slow, glowing
                        default: break;
                    }
                }
                p.values = std::move (b.v);
                out.push_back (std::move (p));
            }
        return out;
    }

    //==========================================================================
    Manager::Manager (juce::AudioProcessorValueTreeState& s) : state (s)
    {
        rescanUser();
        const auto fav = userFolder().getParentDirectory().getChildFile ("favourites.txt");
        if (fav.existsAsFile())
            for (auto& line : juce::StringArray::fromLines (fav.loadFileAsString()))
                if (line.trim().isNotEmpty()) favourites.insert (line.trim());
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                state.addParameterListener (rp->getParameterID(), this);
    }

    Manager::~Manager()
    {
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                state.removeParameterListener (rp->getParameterID(), this);
    }

    juce::File Manager::userFolder()
    {
        const auto kuprell = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Kuprell");
        const auto dir = kuprell.getChildFile ("Hamamatsu 144"), old = kuprell.getChildFile ("Hamamatsu 108");
        if (! dir.exists() && old.isDirectory()) old.copyDirectoryTo (dir);   // renamed from Hamamatsu 108: keep user presets
        return dir.getChildFile ("Presets");
    }

    void Manager::rescanUser()
    {
        const auto keep = current >= 0 && current < (int) list.size() ? list[(size_t) current].key() : juce::String();
        list = makeFactory();
        auto dir = userFolder();
        for (auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.h108preset"))
            if (auto xml = juce::parseXML (f))
            {
                Preset p;
                p.factory = false;
                p.file = f;
                p.bank = f.getParentDirectory() == dir ? xml->getStringAttribute ("bank") : f.getParentDirectory().getFileName();
                p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
                p.category = xml->getStringAttribute ("category", "Harp");
                p.model = xml->getIntAttribute ("model", 1);
                list.push_back (std::move (p));
            }
        current = -1;
        for (size_t i = 0; i < list.size(); ++i) if (list[i].key() == keep) current = (int) i;
        if (onChange) onChange();
    }

    juce::String Manager::currentName() const     { return current >= 0 ? list[(size_t) current].name : juce::String ("Init"); }
    juce::String Manager::currentCategory() const { return current >= 0 ? list[(size_t) current].category : juce::String ("Init"); }

    void Manager::load (int index)
    {
        if (index < 0 || index >= (int) list.size()) return;
        const auto& p = list[(size_t) index];
        if (onLoadStart) onLoadStart();
        loading = true;

        auto apply = [this] (const juce::String& id, float plainValue)
        {
            if (presets::isPerformanceParam (id) || locked.count (id) > 0) return;
            if (auto* prm = state.getParameter (id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
                prm->endChangeGesture();
            }
        };

        // everything back to its default first
        for (auto* prm : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (! presets::isPerformanceParam (rp->getParameterID()) && locked.count (rp->getParameterID()) == 0)
                {
                    rp->beginChangeGesture();
                    rp->setValueNotifyingHost (rp->getDefaultValue());
                    rp->endChangeGesture();
                }

        if (p.factory)
            for (auto& [id, value] : p.values) apply (id, value);
        else if (auto xml = juce::parseXML (p.file))
            for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
                apply (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));

        current = index;
        loading = false;
        modified.store (false);
        if (onChange) onChange();
    }

    void Manager::loadNext (int delta, const std::vector<int>& order)
    {
        if (order.empty()) return;
        auto it = std::find (order.begin(), order.end(), current);
        int pos = it == order.end() ? (delta > 0 ? -1 : 0) : (int) (it - order.begin());
        pos = (pos + delta + (int) order.size()) % (int) order.size();
        load (order[(size_t) pos]);
    }

    juce::StringArray Manager::userBanks() const
    {
        juce::StringArray b;
        for (auto& p : list) if (! p.factory && p.bank.isNotEmpty()) b.addIfNotAlreadyThere (p.bank);
        for (auto& d : userFolder().findChildFiles (juce::File::findDirectories, false)) b.addIfNotAlreadyThere (d.getFileName());
        b.sortNatural();
        return b;
    }

    juce::String Manager::currentBank() const { return current >= 0 ? list[(size_t) current].bank : juce::String(); }

    bool Manager::renameUser (int index, const juce::String& newName, const juce::String& newBank)
    {
        if (index < 0 || index >= (int) list.size() || list[(size_t) index].factory) return false;
        const auto old = list[(size_t) index];
        auto xml = juce::parseXML (old.file);
        if (xml == nullptr) return false;
        auto dir = userFolder();
        const auto bankName = juce::File::createLegalFileName (newBank.trim());
        if (bankName.isNotEmpty()) dir = dir.getChildFile (bankName);
        if (! dir.createDirectory()) return false;
        const auto safeName = juce::File::createLegalFileName (newName.trim());
        if (safeName.isEmpty()) return false;
        xml->setAttribute ("name", newName.trim());
        xml->setAttribute ("bank", bankName);
        auto file = dir.getChildFile (safeName + ".h108preset");
        if (! xml->writeTo (file)) return false;
        if (file != old.file) old.file.deleteFile();
        rescanUser();
        for (size_t i = 0; i < list.size(); ++i) if (! list[i].factory && list[i].file == file) current = (int) i;
        if (onChange) onChange();
        return true;
    }

    bool Manager::saveUser (const juce::String& name, const juce::String& category, int model, const juce::String& bank)
    {
        auto dir = userFolder();
        const auto bankName = juce::File::createLegalFileName (bank.trim());
        if (bankName.isNotEmpty()) dir = dir.getChildFile (bankName);
        if (! dir.createDirectory()) return false;
        const auto safeName = juce::File::createLegalFileName (name.trim());
        if (safeName.isEmpty()) return false;
        auto file = dir.getChildFile (safeName + ".h108preset");

        juce::XmlElement xml ("HAMAMATSU108PRESET");
        xml.setAttribute ("name", name.trim());
        xml.setAttribute ("category", category);
        xml.setAttribute ("bank", bankName);
        xml.setAttribute ("model", model);
        for (auto* prm : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (! presets::isPerformanceParam (rp->getParameterID()))
                {
                    auto* e = xml.createNewChildElement ("PARAM");
                    e->setAttribute ("id", rp->getParameterID());
                    e->setAttribute ("value", rp->convertFrom0to1 (rp->getValue()));
                }
        if (! xml.writeTo (file)) return false;

        rescanUser();
        for (size_t i = 0; i < list.size(); ++i)
            if (! list[i].factory && list[i].file == file) current = (int) i;
        modified.store (false);
        if (onChange) onChange();
        return true;
    }

    bool Manager::deleteUser (int index)
    {
        if (index < 0 || index >= (int) list.size() || list[(size_t) index].factory) return false;
        const bool ok = list[(size_t) index].file.deleteFile();
        if (index == current) current = -1;
        rescanUser();
        return ok;
    }

    bool Manager::isFavourite (int index) const
    {
        return index >= 0 && index < (int) list.size() && favourites.count (list[(size_t) index].key()) > 0;
    }

    void Manager::toggleFavourite (int index)
    {
        if (index < 0 || index >= (int) list.size()) return;
        const auto k = list[(size_t) index].key();
        if (favourites.count (k)) favourites.erase (k); else favourites.insert (k);
        saveFavourites();
        if (onChange) onChange();
    }

    void Manager::saveFavourites() const
    {
        auto f = userFolder().getParentDirectory().getChildFile ("favourites.txt");
        f.getParentDirectory().createDirectory();
        juce::StringArray lines;
        for (auto& k : favourites) lines.add (k);
        f.replaceWithText (lines.joinIntoString ("\n"));
    }

    void Manager::restoreCurrent (const juce::String& key, bool wasModified)
    {
        current = -1;
        for (size_t i = 0; i < list.size(); ++i) if (list[i].key() == key) current = (int) i;
        modified.store (wasModified);
    }

    void Manager::parameterChanged (const juce::String& id, float)
    {
        if (! loading && ! presets::isPerformanceParam (id))
            modified.store (true);
    }
}
