// Headless engine tests for G0G. Build and run:
//   cmake --build cmake-build --target G0GTests
//   cmake-build/G0GTests_artefacts/Release/G0GTests
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "GlideUnit.h"

namespace {

int failures = 0;

void check(bool ok, const juce::String& what) {
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (!ok) ++failures;
}

void setParam(G0GAudioProcessor& p, const char* id, float realValue) {
    auto* prm = p.apvts.getParameter(id);
    jassert(prm != nullptr);
    prm->setValueNotifyingHost(prm->convertTo0to1(realValue));
}

// Renders `totalSamples` of audio in blocks of `blockSize`, delivering the
// given {samplePosition, message} events at their absolute positions.
struct Event { int pos; juce::MidiMessage msg; };

std::vector<float> render(G0GAudioProcessor& p, int totalSamples, int blockSize,
                          const std::vector<Event>& events) {
    std::vector<float> out;
    out.reserve(static_cast<size_t>(totalSamples));
    juce::AudioBuffer<float> buf(2, blockSize);
    for (int start = 0; start < totalSamples; start += blockSize) {
        const int n = std::min(blockSize, totalSamples - start);
        juce::MidiBuffer midi;
        for (const auto& e : events)
            if (e.pos >= start && e.pos < start + n)
                midi.addEvent(e.msg, e.pos - start);
        buf.setSize(2, n, false, false, true);
        p.processBlock(buf, midi);
        for (int i = 0; i < n; ++i) out.push_back(buf.getSample(0, i));
    }
    return out;
}

std::unique_ptr<G0GAudioProcessor> makeProcessor(int blockSize, double sr = 48000.0) {
    auto p = std::make_unique<G0GAudioProcessor>();
    p->setPlayConfigDetails(0, 2, sr, blockSize);
    p->prepareToPlay(sr, blockSize);
    return p;
}

// A deterministic, bright test patch: square-ish sub, no click, no drive.
void brightPatch(G0GAudioProcessor& p) {
    setParam(p, "osc_morph", 2.0f);       // square
    setParam(p, "pitch_depth", 0.0f);
    setParam(p, "click_type", 0.0f);
    setParam(p, "drive_mode", 0.0f);
    setParam(p, "glide_time", 0.0f);
    setParam(p, "ahd_hold", 500.0f);
    setParam(p, "ahd_decay", 2000.0f);
    setParam(p, "master_volume", 0.0f);
}

double highFreqEnergy(const std::vector<float>& x, int from, int to) {
    double e = 0.0;
    for (int i = from + 1; i < to; ++i) {
        const double d = x[(size_t) i] - x[(size_t) (i - 1)];
        e += d * d;
    }
    return e / std::max(1, to - from);
}

int zeroCrossings(const std::vector<float>& x, int from, int to) {
    int n = 0;
    for (int i = from + 1; i < to; ++i)
        if ((x[(size_t) i - 1] < 0.0f) != (x[(size_t) i] < 0.0f)) ++n;
    return n;
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr double sr = 48000.0;

    std::cout << "G0G engine tests\n";

    {
        std::cout << "[1] Output is finite and non-silent\n";
        auto p = makeProcessor(512);
        brightPatch(*p);
        auto out = render(*p, (int) sr, 512, { { 0, juce::MidiMessage::noteOn(1, 36, 1.0f) } });
        bool finite = true; float peak = 0.0f;
        for (float v : out) { finite &= std::isfinite(v); peak = std::max(peak, std::abs(v)); }
        check(finite, "all samples finite");
        check(peak > 0.05f && peak <= 1.0f, "audible and within full scale (peak " + juce::String(peak, 3) + ")");
    }

    {
        std::cout << "[2] MIDI is sample-accurate inside a block\n";
        auto p = makeProcessor(512);
        brightPatch(*p);
        auto out = render(*p, 512, 512, { { 300, juce::MidiMessage::noteOn(1, 40, 1.0f) } });
        double before = 0.0, after = 0.0;
        for (int i = 0; i < 300; ++i) before = std::max(before, (double) std::abs(out[(size_t) i]));
        for (int i = 330; i < 512; ++i) after = std::max(after, (double) std::abs(out[(size_t) i]));
        check(before == 0.0, "silent before the note position (max " + juce::String(before, 6) + ")");
        check(after > 0.05, "sound after it (max " + juce::String(after, 3) + ")");
    }

    {
        std::cout << "[3] Filter envelope follows its stated decay time\n";
        // Low cutoff, big envelope, 100 ms decay. At 10 ms the filter is wide
        // open; by 400 ms it must have closed again to the base cutoff.
        auto p = makeProcessor(512);
        brightPatch(*p);
        setParam(*p, "cutoff", 120.0f);
        setParam(*p, "filter_env_depth", 5000.0f);
        setParam(*p, "filter_env_decay", 100.0f);
        auto out = render(*p, (int) sr, 512, { { 0, juce::MidiMessage::noteOn(1, 45, 1.0f) } });
        const int a = (int) (0.005 * sr), b = (int) (0.025 * sr);
        const int c = (int) (0.40 * sr), d = (int) (0.42 * sr);
        const double early = highFreqEnergy(out, a, b);
        const double late = highFreqEnergy(out, c, d);
        check(early > late * 5.0,
              "bright early / dark late (ratio " + juce::String(early / std::max(late, 1e-12), 1) + ")");
    }

    {
        std::cout << "[4] Block size does not change the sound\n";
        std::vector<Event> ev = { { 100, juce::MidiMessage::noteOn(1, 36, 0.9f) },
                                  { 9000, juce::MidiMessage::noteOn(1, 43, 0.8f) },
                                  { 20000, juce::MidiMessage::noteOff(1, 43) } };
        auto make = [&](int block) {
            auto p = makeProcessor(block);
            setParam(*p, "drive_mode", 2.0f);
            setParam(*p, "drive_gain", 4.0f);
            setParam(*p, "click_type", 1.0f);
            setParam(*p, "glide_time", 120.0f);
            return render(*p, 30000, block, ev);
        };
        auto a = make(64), b = make(480), c = make(2048);
        // The click uses a random generator; compare with a loose tolerance
        // on the structure (envelope) rather than bit-exact noise.
        auto rms = [](const std::vector<float>& x, int from, int to) {
            double s = 0; for (int i = from; i < to; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
            return std::sqrt(s / (to - from));
        };
        const double ra = rms(a, 2000, 8000), rb = rms(b, 2000, 8000), rc = rms(c, 2000, 8000);
        check(std::abs(ra - rb) < 0.03 * ra && std::abs(ra - rc) < 0.03 * ra,
              "level matches across 64/480/2048 (" + juce::String(ra, 4) + ", " + juce::String(rb, 4)
                  + ", " + juce::String(rc, 4) + ")");
    }

    {
        std::cout << "[5] Pitch wheel bends pitch\n";
        auto base = makeProcessor(512);
        brightPatch(*base);
        setParam(*base, "osc_morph", 0.0f); // clean sine for counting crossings
        auto ref = render(*base, (int) sr, 512, { { 0, juce::MidiMessage::noteOn(1, 48, 1.0f) } });

        auto bent = makeProcessor(512);
        brightPatch(*bent);
        setParam(*bent, "osc_morph", 0.0f);
        auto out = render(*bent, (int) sr, 512,
                          { { 0, juce::MidiMessage::pitchWheel(1, 16383) },
                            { 0, juce::MidiMessage::noteOn(1, 48, 1.0f) } });
        const int from = (int) (0.4 * sr), to = (int) (0.9 * sr);
        const double ratio = (double) zeroCrossings(out, from, to) / std::max(1, zeroCrossings(ref, from, to));
        check(std::abs(ratio - 1.1225) < 0.03, "+2 semitones => x1.122 frequency (got x" + juce::String(ratio, 3) + ")");
    }

    {
        std::cout << "[6] Mod wheel opens the filter\n";
        auto run = [&](bool wheel) {
            auto p = makeProcessor(512);
            brightPatch(*p);
            setParam(*p, "cutoff", 150.0f);
            setParam(*p, "filter_env_depth", 0.0f);
            std::vector<Event> ev = { { 0, juce::MidiMessage::noteOn(1, 45, 1.0f) } };
            if (wheel) ev.insert(ev.begin(), { 0, juce::MidiMessage::controllerEvent(1, 1, 127) });
            return render(*p, (int) sr, 512, ev);
        };
        auto closed = run(false), open = run(true);
        const int from = (int) (0.3 * sr), to = (int) (0.6 * sr);
        check(highFreqEnergy(open, from, to) > highFreqEnergy(closed, from, to) * 3.0,
              "wheel up is brighter than wheel down");
    }

    {
        std::cout << "[7] Presets load and produce sound\n";
        auto p = makeProcessor(512);
        bool all = true;
        for (int i = 0; i < p->getNumPrograms(); ++i) {
            p->setCurrentProgram(i);
            auto out = render(*p, 24000, 512, { { 0, juce::MidiMessage::noteOn(1, 36, 1.0f) },
                                               { 12000, juce::MidiMessage::noteOff(1, 36) } });
            float peak = 0.0f; bool fin = true;
            for (float v : out) { fin &= std::isfinite(v); peak = std::max(peak, std::abs(v)); }
            const bool ok = fin && peak > 0.02f && peak <= 1.0f;
            all &= ok;
            std::cout << "        " << p->getProgramName(i) << ": peak " << peak << (ok ? "" : "  <-- BAD") << "\n";
            p->processBlock(*std::make_unique<juce::AudioBuffer<float>>(2, 64), *std::make_unique<juce::MidiBuffer>());
        }
        check(all, "every factory preset is finite, audible and never above full scale");
    }

    {
        std::cout << "[9] Glide moves evenly in pitch, not in Hz\n";
        GlideUnit g;
        g.prepare(sr);
        g.setGlideTime(200.0f);
        g.setGlideCurve(0.0f);           // linear
        g.noteOn(36, 1.0f);              // 65.41 Hz
        g.getNextFrequency();
        g.noteOn(48, 1.0f);              // 130.81 Hz, one octave up
        float f = 0.0f, prev = 0.0f; bool monotonic = true;
        const int half = (int) (0.1 * sr);
        for (int i = 0; i < half; ++i) { f = g.getNextFrequency(); monotonic &= (f >= prev - 1e-3f); prev = f; }
        const double semitones = 12.0 * std::log2(f / 65.406);
        check(std::abs(semitones - 6.0) < 0.15,
              "halfway through a 1-octave slide is +6.0 semitones (got +" + juce::String(semitones, 2)
                  + "; a Hz-linear slide would be +6.9)");
        check(monotonic, "slide rises monotonically");
        float last = f;
        for (int i = 0; i < (int) (0.15 * sr); ++i) last = g.getNextFrequency();
        check(std::abs(last - 130.813f) < 0.01f, "lands exactly on the target (" + juce::String(last, 3) + " Hz)");

        // Exponential curve must also land exactly (it used to stop 0.7% short).
        GlideUnit e;
        e.prepare(sr); e.setGlideTime(100.0f); e.setGlideCurve(1.0f);
        e.noteOn(40, 1.0f); e.getNextFrequency(); e.noteOn(28, 1.0f);
        float el = 0.0f;
        for (int i = 0; i < (int) (0.2 * sr); ++i) el = e.getNextFrequency();
        check(std::abs(el - 41.203f) < 0.01f, "exponential curve lands exactly (" + juce::String(el, 3) + " Hz)");
    }

    {
        std::cout << "[10] 808 behaviour: gate, DC, attack, tail, legato, tune\n";
        auto rmsOf = [](const std::vector<float>& x, int a, int b) {
            double e = 0; for (int i = a; i < b; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
            return std::sqrt(e / std::max(1, b - a));
        };
        auto cleanSine = [&](G0GAudioProcessor& p) {
            setParam(p, "osc_morph", 0.0f); setParam(p, "pitch_depth", 0.0f); setParam(p, "click_type", 0.0f);
            setParam(p, "drive_mode", 0.0f); setParam(p, "cutoff", 8000.0f); setParam(p, "master_volume", 0.0f);
            setParam(p, "filter_env_depth", 0.0f);
        };

        // Gate: note length decides the tail.
        {
            auto run = [&](float gate) {
                auto p = makeProcessor(512);
                cleanSine(*p);
                setParam(*p, "ahd_hold", 20.0f); setParam(*p, "ahd_decay", 3000.0f);
                setParam(*p, "gate_mode", gate); setParam(*p, "release", 80.0f);
                return render(*p, (int) (1.5 * sr), 512, { { 0, juce::MidiMessage::noteOn(1, 36, 1.0f) },
                                                           { (int) (0.2 * sr), juce::MidiMessage::noteOff(1, 36) } });
            };
            auto gated = run(1.0f), oneShot = run(0.0f);
            const double ref = rmsOf(gated, 4800, 5280);
            check(rmsOf(gated, 19200, 19680) < ref * 0.01, "gate: silent 100 ms after a short note ends");
            check(rmsOf(oneShot, 19200, 19680) > ref * 0.3, "one-shot: tail still rings after note-off");
        }

        // DC offset from the asymmetric tube drive is removed.
        {
            auto p = makeProcessor(512);
            cleanSine(*p);
            setParam(*p, "drive_mode", 1.0f); setParam(*p, "drive_gain", 6.0f);
            setParam(*p, "ahd_hold", 900.0f); setParam(*p, "ahd_decay", 2000.0f);
            auto out = render(*p, (int) sr, 512, { { 0, juce::MidiMessage::noteOn(1, 36, 1.0f) } });
            double mean = 0; for (int i = 9600; i < 40000; ++i) mean += out[(size_t) i];
            mean /= (40000 - 9600);
            check(std::abs(mean) < 0.01, "tube drive has no DC offset (mean " + juce::String(mean, 4) + ")");
        }

        // Every hit has the same attack (phase reset on trigger).
        {
            auto p = makeProcessor(512);
            cleanSine(*p);
            setParam(*p, "legato", 0.0f);
            setParam(*p, "ahd_hold", 20.0f); setParam(*p, "ahd_decay", 100.0f);
            auto out = render(*p, (int) sr, 512, { { 0, juce::MidiMessage::noteOn(1, 40, 1.0f) },
                                                    { 12000, juce::MidiMessage::noteOff(1, 40) },
                                                    { 24000 + 137, juce::MidiMessage::noteOn(1, 40, 1.0f) } });
            const float a = out[20], b = out[(size_t) (24000 + 137 + 20)];
            check(std::abs(a - b) < 0.02f && a > 0.0f && b > 0.0f,
                  "retrigger starts at the same phase (" + juce::String(a, 3) + " vs " + juce::String(b, 3) + ")");
        }

        // Tail ends with no step.
        {
            auto p = makeProcessor(512);
            cleanSine(*p);
            setParam(*p, "ahd_hold", 5.0f); setParam(*p, "ahd_decay", 300.0f);
            setParam(*p, "gate_mode", 0.0f);
            auto out = render(*p, (int) (0.6 * sr), 512, { { 0, juce::MidiMessage::noteOn(1, 60, 1.0f) } });
            float maxStepLate = 0;
            for (int i = (int) (0.28 * sr); i < (int) (0.40 * sr); ++i)
                maxStepLate = std::max(maxStepLate, std::abs(out[(size_t) i] - out[(size_t) i - 1]));
            check(maxStepLate < 0.01f, "no click at the end of the tail (max step " + juce::String(maxStepLate, 5) + ")");
        }

        // Legato: an overlapping note does not retrigger the envelope.
        {
            auto run = [&](float legato) {
                auto p = makeProcessor(512);
                cleanSine(*p);
                setParam(*p, "legato", legato);
                setParam(*p, "ahd_hold", 10.0f); setParam(*p, "ahd_decay", 2000.0f);
                setParam(*p, "glide_time", 50.0f);
                return render(*p, (int) sr, 512, { { 0, juce::MidiMessage::noteOn(1, 36, 1.0f) },
                                                    { 24000, juce::MidiMessage::noteOn(1, 43, 1.0f) } });
            };
            auto legato = run(1.0f), retrig = run(0.0f);
            const double before = rmsOf(legato, 23000, 23900);
            const double afterLegato = rmsOf(legato, 28000, 28900);
            const double afterRetrig = rmsOf(retrig, 28000, 28900);
            check(afterLegato < before * 1.05, "legato: slide keeps the decaying level (" + juce::String(afterLegato / before, 2) + "x)");
            check(afterRetrig > before * 1.5, "retrigger: second note punches again (" + juce::String(afterRetrig / before, 2) + "x)");
        }

        // Tune and fine tune.
        {
            auto crossings = [&](float tune, float fine) {
                auto p = makeProcessor(512);
                cleanSine(*p);
                setParam(*p, "tune", tune); setParam(*p, "fine_tune", fine);
                setParam(*p, "ahd_hold", 900.0f); setParam(*p, "ahd_decay", 2000.0f);
                auto out = render(*p, (int) sr, 512, { { 0, juce::MidiMessage::noteOn(1, 48, 1.0f) } });
                return (double) zeroCrossings(out, 12000, 36000);
            };
            const double base = crossings(0.0f, 0.0f);
            check(std::abs(crossings(12.0f, 0.0f) / base - 2.0) < 0.03, "+12 semitones doubles the frequency");
            check(std::abs(crossings(0.0f, 100.0f) / base - 1.0595) < 0.02, "+100 cents = +1 semitone");
        }
    }

    {
        std::cout << "[8] CPU cost (informational, one core, this machine)\n";
        auto measure = [&](float mode, float gain, const char* label) {
            auto p = makeProcessor(256);
            brightPatch(*p);
            setParam(*p, "drive_mode", mode);
            setParam(*p, "drive_gain", gain);
            setParam(*p, "click_type", 1.0f);
            const int secs = 20, total = (int) sr * secs;
            std::vector<Event> ev;
            for (int t = 0; t < total; t += 12000)
                ev.push_back({ t, juce::MidiMessage::noteOn(1, 36 + (t / 12000) % 12, 1.0f) });
            const auto t0 = juce::Time::getHighResolutionTicks();
            render(*p, total, 256, ev);
            const double sec = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0);
            const double pct = 100.0 * sec / secs;
            std::cout << "        " << label << ": " << juce::String(pct, 2) << " % of real time\n";
            return pct;
        };
        const double off = measure(0.0f, 1.0f, "drive bypassed       ");
        const double on = measure(3.0f, 8.0f, "foldback, 2x oversampled");
        check(on < 25.0, "drive stays cheap (" + juce::String(on, 2) + " % vs " + juce::String(off, 2) + " % bypassed)");
    }

    std::cout << (failures == 0 ? "\nALL PASSED\n" : "\nFAILURES: " + std::to_string(failures) + "\n");
    return failures == 0 ? 0 : 1;
}
