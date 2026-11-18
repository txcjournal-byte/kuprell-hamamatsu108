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

    std::vector<Preset> makeFactory()
    {
        std::vector<Preset> out;
        for (int c = 0; c < 6; ++c)
            for (int i = 0; i < 24; ++i)
            {
                Builder b;
                b.r.setSeed ((juce::int64) (1081 + c * 977 + i * 31));
                Preset p;
                p.name = names[c][i];
                p.category = categories()[c];
                p.model = i % 6;
                b.set ("model", (float) p.model);

                // musical defaults shared by every preset: clean chorus, a real room, gentle vibrato
                b.set ("fx1Mix", b.range (0.2f, 0.35f));    // chorus
                b.set ("fx1A", b.range (0.2f, 0.35f));
                b.set ("fx1B", b.range (0.3f, 0.5f));
                b.set ("fx2Mix", b.range (0.08f, 0.18f));   // delay
                b.set ("fx2A", (float) b.pick ({ 2, 3, 3, 5 }) / 6.0f + 0.01f);
                b.set ("fx2B", b.range (0.25f, 0.4f));
                b.set ("fx3Mix", b.range (0.25f, 0.4f));    // reverb
                b.set ("fx3A", b.range (0.55f, 0.75f));
                b.set ("fx3B", b.range (0.35f, 0.55f));
                b.set ("vibDepth", b.range (0.03f, 0.1f));
                b.set ("vibRate", b.range (4.5f, 5.8f));

                switch (c)
                {
                    case 0: // Harp: the classic strummed chime, played by a relaxed hand
                        b.set ("harpVoice", (float) b.pick ({ 0, 0, 0, 0, 1, 7 }));
                        b.set ("harpTone", b.range (0.55f, 0.75f));
                        b.set ("harpDecay", b.range (1.8f, 3.0f));
                        b.set ("harpRange", 3.0f);
                        b.set ("strumRate", (float) b.pick ({ 2, 2, 1 }));
                        b.set ("strumFeel", b.range (0.45f, 0.65f));
                        b.set ("strumSpeed", b.range (16.0f, 26.0f));
                        if (b.chance (0.3f)) { b.set ("dual", 1.0f); b.set ("harpVoice2", (float) b.pick ({ 9, 5, 1 })); b.set ("morph", b.range (0.2f, 0.35f)); }
                        if (b.chance (0.3f)) { b.set ("chordType", 2.0f); b.set ("chordLevel", b.range (-24.0f, -18.0f)); b.set ("chordFilter", 0.45f); }
                        break;
                    case 1: // Pluck: short, rhythmic, dry
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
                    case 2: // Bell: long, glassy, spacious
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
                    case 3: // Keys: tines or vibes over a soft organ and a round bass
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
                    case 4: // Pad: slow strums over a warm layer
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
                    default: // Texture: well-loved tape, gentle movement
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
                // modern colour on top of the era sound
                if (p.model == 0 || p.model == 4)   // Warm and Lo-fi are narrow by nature: open them up (mono-safe)
                {
                    b.set ("fx5Type", 20.0f); b.set ("fx5On", 1.0f); b.set ("fx5A", b.range (0.5f, 0.7f)); b.set ("fx5B", b.range (0.35f, 0.6f)); b.set ("fx5Mix", 1.0f);
                }
                if (i % 6 == 3 && (c == 2 || c == 4 || c == 5 || c == 0))   // PRISM: the sound glows on the chord, a slow LFO scans it
                {
                    b.set ("fx4Type", 17.0f); b.set ("fx4On", 1.0f);
                    b.set ("fx4A", b.range (0.3f, 0.7f)); b.set ("fx4B", b.range (0.35f, 0.7f)); b.set ("fx4Mix", b.range (0.35f, 0.6f));
                    b.set ("lfo2QDst", (float) mod::targetIds().indexOf ("fx4A")); b.set ("lfo2QAmt", b.range (0.2f, 0.35f));
                    b.set ("lfo2Rate", b.range (0.05f, 0.25f));
                }
                if (i % 6 == 5 && (c == 1 || c == 2))   // DISPERSE: laser-like attack on plucks and bells
                {
                    b.set ("fx4Type", 18.0f); b.set ("fx4On", 1.0f);
                    b.set ("fx4A", b.range (0.3f, 0.6f)); b.set ("fx4B", b.range (0.3f, 0.6f)); b.set ("fx4Mix", b.range (0.35f, 0.6f));
                }
                if (i % 6 == 1 && (c == 5 || c == 3))   // SHAPE: morphing grit on textures and keys
                {
                    b.set ("fx4Type", 19.0f); b.set ("fx4On", 1.0f);
                    b.set ("fx4A", b.range (0.2f, 0.8f)); b.set ("fx4B", b.range (0.15f, 0.35f)); b.set ("fx4Mix", b.range (0.25f, 0.45f));
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
