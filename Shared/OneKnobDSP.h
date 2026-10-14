#pragma once

// Small DSP helpers shared by the ONE KNOB series.
// Header-only, allocation-free after prepare(), safe to call from the audio thread.

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace ok
{
    // One-pole DC blocker (~10 Hz). Asymmetric saturation adds a DC offset
    // that follows the signal envelope; left in, it thumps on every note.
    struct DcBlocker
    {
        float r { 0.999f }, x1 { 0.0f }, y1 { 0.0f };

        void prepare (double sr, float hz = 10.0f)
        {
            r = (float) std::exp (-juce::MathConstants<double>::twoPi * hz / sr);
            reset();
        }
        void reset() { x1 = y1 = 0.0f; }

        inline float process (float x)
        {
            const float y = x - x1 + r * y1;
            x1 = x; y1 = y;
            return y;
        }
    };

    // Static loudness compensation for a waveshaper: runs a reference
    // "pluck" through the curve -- a sine at eight levels 3 dB apart, from
    // refPeak down 21 dB, so it has a guitar-like spread of levels -- and
    // returns the gain that puts its RMS back where it started. Being
    // static it behaves like a pedal's level knob: no breathing or pumping,
    // unlike an adaptive auto-gain. Cost: 128 curve evaluations per call.
    template <typename Curve>
    inline float referenceCompensation (Curve&& curve, float refPeak = 0.5f)
    {
        constexpr int levels = 8, N = 16;
        double in = 0.0, out = 0.0;
        float a = refPeak;
        for (int k = 0; k < levels; ++k, a *= 0.7079f)
            for (int i = 0; i < N; ++i)
            {
                const float x = a * std::sin (juce::MathConstants<float>::twoPi * ((float) i + 0.5f) / (float) N);
                const float y = curve (x);
                in  += (double) x * x;
                out += (double) y * y;
            }
        return out > 1.0e-12 ? (float) std::sqrt (in / out) : 1.0f;
    }

    // Integer-sample delay line used to keep a dry path aligned with a
    // processed path that has latency (oversampling filters).
    class DelayLine
    {
    public:
        void prepare (int numChannels, int maxDelay)
        {
            size = juce::nextPowerOfTwo (juce::jmax (2, maxDelay + 1));
            buf.setSize (juce::jmax (1, numChannels), size, false, true, false);
            buf.clear();
            pos = 0;
        }
        void reset() { buf.clear(); pos = 0; }

        // Writes x and returns the sample from `delay` samples ago.
        inline float process (int ch, float x, int delay)
        {
            auto* d = buf.getWritePointer (ch);
            d[pos] = x;
            return d[(pos - delay) & (size - 1)];
        }
        inline void advance() { pos = (pos + 1) & (size - 1); }

    private:
        juce::AudioBuffer<float> buf;
        int size { 2 }, pos { 0 };
    };
}
