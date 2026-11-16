#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <set>
#include <atomic>

// Factory presets (144, built in), user presets (files) and favourites.
namespace presets
{
    inline const juce::StringArray& categories() { static const juce::StringArray c { "Harp", "Pluck", "Bell", "Keys", "Pad", "Texture" }; return c; }

    struct Preset
    {
        juce::String name, category;
        int model = 1;
        bool factory = true;
        juce::File file;                                   // user presets
        std::vector<std::pair<juce::String, float>> values; // factory: plain values that differ from the defaults
        juce::String key() const { return (factory ? "factory:" : "user:") + name; }
    };

    std::vector<Preset> makeFactory();

    // parameters a preset never changes (performance settings)
    inline bool isPerformanceParam (const juce::String& id)
    {
        return id == "keyRoot" || id == "keyMode" || id == "transpose" || id == "hold" || id == "master";
    }

    class Manager : private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        explicit Manager (juce::AudioProcessorValueTreeState&);
        ~Manager() override;

        const std::vector<Preset>& all() const { return list; }
        void rescanUser();

        int getCurrent() const noexcept { return current; }
        bool isModified() const noexcept { return modified.load(); }
        juce::String currentName() const;
        juce::String currentCategory() const;

        void load (int index);
        void loadNext (int delta, const std::vector<int>& order);
        bool saveUser (const juce::String& name, const juce::String& category, int model);   // creates or overwrites
        bool deleteUser (int index);

        bool isFavourite (int index) const;
        void toggleFavourite (int index);

        // locked parameters are not changed by presets or randomize
        std::set<juce::String> locked;

        static juce::File userFolder();
        std::function<void()> onChange;   // preset loaded / list changed / modified flag changed

        void markClean() { modified.store (false); }
        void restoreCurrent (const juce::String& key, bool wasModified);

    private:
        void parameterChanged (const juce::String&, float) override;
        void saveFavourites() const;

        juce::AudioProcessorValueTreeState& state;
        std::vector<Preset> list;
        std::set<juce::String> favourites;
        int current = -1;
        std::atomic<bool> modified { false };
        bool loading = false;
    };
}
