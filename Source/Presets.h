#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

//==============================================================================
// Patch handling: factory starters plus user patches stored as small XML
// files. A preset is a list of {parameter id, real (un-normalised) value};
// anything it does not mention is reset to the parameter's default, so patches
// never inherit settings from the previous sound. Master volume is never
// touched by a preset.
//==============================================================================
namespace Presets
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    // gate_mode: 0 one-shot, 1 gate (default).  legato: 0 retrigger, 1 legato (default).
    // click_type: 0 off, 1 white, 2 pink.  drive_mode: 0 bypass, 1 tube, 2 clip, 3 foldback.
    // osc_morph: 0 sine, 1 triangle, 2 square.
    inline const std::vector<Preset>& factory()
    {
        static const std::vector<Preset> presets = {
            { "Classic 808",
              { { "osc_morph", 0.0f }, { "glide_time", 60.0f }, { "glide_curve", 1.0f },
                { "ahd_hold", 20.0f }, { "ahd_decay", 900.0f }, { "decay_curve", 0.0f },
                { "pitch_depth", 24.0f }, { "pitch_decay", 30.0f },
                { "click_type", 1 }, { "click_level", 0.15f },
                { "drive_mode", 1 }, { "drive_gain", 1.5f },
                { "cutoff", 800.0f }, { "filter_env_depth", 800.0f }, { "filter_env_decay", 150.0f }, { "release", 150.0f } } },

            { "Long Boom",
              { { "osc_morph", 0.0f }, { "glide_time", 40.0f },
                { "ahd_hold", 80.0f }, { "ahd_decay", 3200.0f }, { "decay_curve", -0.3f },
                { "pitch_depth", 16.0f }, { "pitch_decay", 60.0f },
                { "click_type", 1 }, { "click_level", 0.08f },
                { "drive_mode", 0 },
                { "cutoff", 400.0f }, { "filter_env_depth", 300.0f }, { "filter_env_decay", 400.0f }, { "release", 450.0f } } },

            { "Distorted 808",
              { { "osc_morph", 0.4f }, { "glide_time", 50.0f },
                { "ahd_hold", 30.0f }, { "ahd_decay", 1200.0f },
                { "pitch_depth", 30.0f }, { "pitch_decay", 25.0f },
                { "click_type", 1 }, { "click_level", 0.2f },
                { "drive_mode", 2 }, { "drive_gain", 5.0f },
                { "cutoff", 1500.0f }, { "filter_env_depth", 2000.0f }, { "filter_env_decay", 120.0f }, { "release", 200.0f } } },

            { "Trap Slide",
              { { "osc_morph", 0.0f }, { "glide_time", 220.0f }, { "glide_curve", 0.6f },
                { "ahd_hold", 40.0f }, { "ahd_decay", 1400.0f },
                { "pitch_depth", 12.0f }, { "pitch_decay", 40.0f },
                { "click_type", 1 }, { "click_level", 0.12f },
                { "drive_mode", 1 }, { "drive_gain", 2.5f },
                { "cutoff", 1000.0f }, { "filter_env_depth", 1000.0f }, { "filter_env_decay", 200.0f }, { "release", 120.0f } } },

            { "Short Kick",
              { { "osc_morph", 0.0f }, { "glide_time", 0.0f },
                { "ahd_hold", 5.0f }, { "ahd_decay", 250.0f }, { "decay_curve", 0.6f },
                { "pitch_depth", 36.0f }, { "pitch_decay", 20.0f },
                { "click_type", 1 }, { "click_level", 0.3f },
                { "drive_mode", 2 }, { "drive_gain", 2.0f },
                { "cutoff", 3000.0f }, { "filter_env_depth", 1500.0f }, { "filter_env_decay", 60.0f }, { "gate_mode", 0 } } },

            { "Foldback Growl",
              { { "osc_morph", 1.0f }, { "glide_time", 70.0f },
                { "ahd_hold", 25.0f }, { "ahd_decay", 1500.0f },
                { "pitch_depth", 24.0f }, { "pitch_decay", 35.0f },
                { "click_type", 2 }, { "click_level", 0.1f },
                { "drive_mode", 3 }, { "drive_gain", 4.0f },
                { "cutoff", 1200.0f }, { "filter_env_depth", 3000.0f }, { "filter_env_decay", 200.0f }, { "release", 180.0f } } },

            { "Pure Sub",
              { { "osc_morph", 0.0f }, { "glide_time", 30.0f },
                { "ahd_hold", 100.0f }, { "ahd_decay", 2500.0f },
                { "pitch_depth", 0.0f },
                { "click_type", 0 },
                { "drive_mode", 0 },
                { "cutoff", 300.0f }, { "filter_env_depth", 0.0f }, { "release", 250.0f } } },
        };
        return presets;
    }

    inline bool isPerformanceParam (const juce::String& id) { return id == "master_volume"; }

    // Reset every sound parameter to its default, then apply the given values.
    inline void applyValues (juce::AudioProcessor& proc,
                             const std::vector<std::pair<juce::String, float>>& values)
    {
        for (auto* p : proc.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (ranged == nullptr || isPerformanceParam (ranged->getParameterID()))
                continue;

            float target = ranged->getDefaultValue(); // normalised
            for (const auto& kv : values)
                if (kv.first == ranged->getParameterID())
                    target = ranged->convertTo0to1 (kv.second);

            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (target);
            ranged->endChangeGesture();
        }
    }

    inline void applyFactory (juce::AudioProcessor& proc, int index)
    {
        const auto& list = factory();
        if (index < 0 || index >= (int) list.size())
            return;
        std::vector<std::pair<juce::String, float>> v;
        for (const auto& kv : list[(size_t) index].values)
            v.emplace_back (kv.first, kv.second);
        applyValues (proc, v);
    }

    //==========================================================================
    inline juce::File userDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("G0G").getChildFile ("Presets");
    }

    inline juce::Array<juce::File> userPresetFiles()
    {
        juce::Array<juce::File> files;
        userDirectory().findChildFiles (files, juce::File::findFiles, false, "*.g0gpreset");
        files.sort();
        return files;
    }

    inline bool saveUser (juce::AudioProcessor& proc, const juce::String& name)
    {
        auto dir = userDirectory();
        if (! dir.createDirectory().wasOk())
            return false;

        juce::XmlElement xml ("G0GPreset");
        xml.setAttribute ("name", name);
        for (auto* p : proc.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! isPerformanceParam (ranged->getParameterID()))
                {
                    auto* e = xml.createNewChildElement ("PARAM");
                    e->setAttribute ("id", ranged->getParameterID());
                    e->setAttribute ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
                }

        return xml.writeTo (dir.getChildFile (juce::File::createLegalFileName (name) + ".g0gpreset"));
    }

    inline bool loadUser (juce::AudioProcessor& proc, const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("G0GPreset"))
            return false;

        std::vector<std::pair<juce::String, float>> values;
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            values.emplace_back (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
        applyValues (proc, values);
        return true;
    }
} // namespace Presets
