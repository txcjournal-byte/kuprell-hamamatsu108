#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <vector>

// A MIDI clip you drag out of a Kuprell plugin into the piano roll.
// CLEAN (default): every stroke becomes one block chord on a fine grid (1/24 beat, so 16ths, triplets and dotted notes all fit),
// no overlapping notes, no tiny slivers: easy to read and edit. EXACT: the timing as played (humanized strums), still no overlaps.
// One track, MIDI file type 0, no tempo or time signature: the host keeps its own tempo.
namespace kuprell::midiclip
{
    struct Note { double beat; int pitch; float velocity; double length; };

    inline juce::File write (std::vector<Note> notes, const juce::String& fileName, bool clean, int channel = 16, double clipBeats = 0.0)
    {
        if (notes.empty()) return {};
        constexpr int ppq = 960;
        constexpr double grid = 1.0 / 24.0;
        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.beat < b.beat || (a.beat == b.beat && a.pitch < b.pitch); });

        if (clean)
        {
            // 1. strokes: notes that start within ~1/10 beat of each other (and are different strings) are one stroke
            double clusterStart = -1.0;
            std::vector<int> clusterPitches;
            for (auto& n : notes)
            {
                const bool samePitchAgain = std::find (clusterPitches.begin(), clusterPitches.end(), n.pitch) != clusterPitches.end();
                if (clusterStart < 0.0 || n.beat - clusterStart > 0.1 || samePitchAgain) { clusterStart = n.beat; clusterPitches.clear(); }
                clusterPitches.push_back (n.pitch);
                n.beat = std::round (clusterStart / grid) * grid;
            }
            // 2. one note per pitch and start (the loudest wins)
            std::vector<Note> merged;
            for (const auto& n : notes)
            {
                auto it = std::find_if (merged.begin(), merged.end(), [&] (const Note& m) { return m.pitch == n.pitch && std::abs (m.beat - n.beat) < 1.0e-6; });
                if (it != merged.end()) { it->velocity = std::max (it->velocity, n.velocity); it->length = std::max (it->length, n.length); }
                else merged.push_back (n);
            }
            notes.swap (merged);
            std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.beat < b.beat || (a.beat == b.beat && a.pitch < b.pitch); });
            // 3. lengths on the grid, ending at the next stroke at the latest: tidy blocks, nothing hangs over
            for (size_t i = 0; i < notes.size(); ++i)
            {
                double nextStroke = -1.0;
                for (size_t j = i + 1; j < notes.size(); ++j) if (notes[j].beat > notes[i].beat + 1.0e-6) { nextStroke = notes[j].beat; break; }
                double len = std::max (grid * 2.0, std::round (notes[i].length / grid) * grid);
                if (nextStroke > 0.0) len = std::min (len, nextStroke - notes[i].beat);
                else if (clipBeats > 0.0) len = std::min (len, clipBeats - notes[i].beat);
                notes[i].length = std::max (grid, std::min (4.0, len));
            }
        }

        // never two notes of one pitch on top of each other
        for (size_t i = 0; i < notes.size(); ++i)
            for (size_t j = i + 1; j < notes.size(); ++j)
                if (notes[j].pitch == notes[i].pitch && notes[j].beat > notes[i].beat + 1.0e-6)
                {
                    notes[i].length = std::min (notes[i].length, notes[j].beat - notes[i].beat);
                    break;
                }

        juce::MidiMessageSequence seq;
        for (const auto& n : notes)
        {
            const double on = std::round (n.beat * ppq);
            const double off = std::max (on + 20.0, std::round ((n.beat + n.length) * ppq) - 1.0);
            const auto v = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity * 127.0f));
            seq.addEvent (juce::MidiMessage::noteOn (channel, juce::jlimit (0, 127, n.pitch), v), on);
            seq.addEvent (juce::MidiMessage::noteOff (channel, juce::jlimit (0, 127, n.pitch)), off);
        }
        seq.updateMatchedPairs();
        seq.sort();
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ppq);
        mf.addTrack (seq);
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Kuprell");
        dir.createDirectory();
        auto file = dir.getChildFile (juce::File::createLegalFileName (fileName) + ".mid");
        file.deleteFile();
        juce::FileOutputStream os (file);
        if (! os.openedOk()) return {};
        mf.writeTo (os, 0);
        os.flush();
        return file;
    }
}
