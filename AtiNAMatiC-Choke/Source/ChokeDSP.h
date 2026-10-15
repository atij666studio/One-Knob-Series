#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace nch
{
    // Choke — shortens how long notes ring after you play them.
    //
    // A gate only removes noise between notes. Choke works inside the
    // note's own decay: it tracks each note's peak and, once the note has
    // fallen a little below that peak, makes the rest of the decay fall
    // faster (a downward expander relative to the note, not to a fixed
    // threshold). Tight, stopped riffs stay tight; the pick attack itself is
    // never touched, and every new attack restores full level at once.
    //
    //   level detector : peak with ~15 ms hold, 30 ms release (bridges a
    //                    full cycle of a low E, so it never rides the wave)
    //   note peak      : jumps to the level on attacks, otherwise sinks
    //                    slowly (1.5 dB/s) so long notes don't drift off
    //   expansion      : below "start" dB under the peak, each further dB
    //                    of decay becomes RATIO dB (CHOKE sets the ratio)
    //   gain glide     : down in 8 ms, up in 0.5 ms (attacks pass whole)
    //
    // Styles: Tight (starts 9 dB down, up to 3.5:1), Staccato (6 dB, 5:1),
    // Djent (3 dB, 8:1). Zero latency.
    class ChokeDSP
    {
    public:
        enum Style { Tight = 0, Staccato = 1, Djent = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> outputDb { -90.0f };
        std::atomic<float> chokeDb  {   0.0f };   // current cut, positive dB

        void prepare (double sr, int /*blockSize*/, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, 8, numChannels);
            pkHeld = notePkDb = -120.0f; holdLeft = 0; grDb = 0.0f;
            holdLen = (int) (0.015 * sr);
            relA    = (float) std::exp (-1.0 / (0.030 * sr));
            sinkDbPerSample = (float) (1.5 / sr);
            downA = coef (0.008); upA = coef (0.0005);
            mixSm.reset (sr, 0.02); mixSm.setCurrentAndTargetValue (1.0f);
            outSm.reset (sr, 0.02); outSm.setCurrentAndTargetValue (1.0f);
            envIn = envOut = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float amount, int styleIn, float mix, float outDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            amount = juce::jlimit (0.0f, 1.0f, amount);
            const Style s = (Style) juce::jlimit (0, 2, styleIn);
            float startDb = 9.0f, maxRatio = 3.5f;
            switch (s)
            {
                case Staccato: startDb = 6.0f; maxRatio = 5.0f; break;
                case Djent:    startDb = 3.0f; maxRatio = 8.0f; break;
                default: break;
            }
            const float ratio = 1.0f + amount * (maxRatio - 1.0f);
            const float slope = ratio - 1.0f;            // extra dB of cut per dB of decay
            constexpr float maxCutDb = 40.0f;

            mixSm.setTargetValue (mix);
            outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));

            float* p[8] = {};
            for (int c = 0; c < numCh; ++c) p[c] = buffer.getWritePointer (c);

            float pkIn = 0.0f, pkOut = 0.0f, grMax = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float det = 0.0f;
                for (int c = 0; c < numCh; ++c) det = juce::jmax (det, std::abs (p[c][i]));

                // level: peak-hold + release
                if (det >= pkHeld) { pkHeld = det; holdLeft = holdLen; }
                else if (holdLeft > 0) --holdLeft;
                else pkHeld = juce::jmax (det, pkHeld * relA);
                const float lvlDb = juce::Decibels::gainToDecibels (pkHeld, -120.0f);

                // note peak: follows rises instantly, sinks slowly otherwise
                notePkDb = lvlDb > notePkDb ? lvlDb : notePkDb - sinkDbPerSample;

                // expansion below the start point (only for real signal)
                const float fall = notePkDb - lvlDb - startDb;
                float target = 0.0f;
                if (fall > 0.0f && notePkDb > -70.0f)
                    target = -juce::jmin (maxCutDb, fall * slope);
                grDb += (target - grDb) * (target < grDb ? downA : upA);
                const float g = juce::Decibels::decibelsToGain (grDb);

                const float mx = mixSm.getNextValue();
                const float og = outSm.getNextValue();
                for (int c = 0; c < numCh; ++c)
                {
                    const float x = p[c][i];
                    const float y = (x * g * og) * mx + x * (1.0f - mx);
                    p[c][i] = y;
                    pkOut = juce::jmax (pkOut, std::abs (y));
                }
                pkIn  = juce::jmax (pkIn, det);
                grMax = juce::jmin (grMax, grDb);
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envIn  += (pkIn  - envIn)  * k;
            envOut += (pkOut - envOut) * k;
            inputDb.store  (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            chokeDb.store  (-grMax, std::memory_order_relaxed);
        }

    private:
        float coef (double seconds) const { return (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate))); }

        double sampleRate { 48000.0 };
        int    channels   { 2 };
        float  pkHeld { 0 }, notePkDb { -120.0f }, grDb { 0 };
        int    holdLeft { 0 }, holdLen { 0 };
        float  relA { 0 }, sinkDbPerSample { 0 }, downA { 0 }, upA { 0 };
        juce::SmoothedValue<float> mixSm, outSm;
        float  envIn { 0 }, envOut { 0 };
    };
}
