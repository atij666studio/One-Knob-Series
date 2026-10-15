#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>

namespace nco
{
    // Cohere — placed AFTER the amp / cab.
    //
    // Amp sims (NAM captures especially) often carry a harsh 3-4 kHz bite and
    // a 6-9 kHz fizz that jump out on some notes and not others. A static EQ
    // cut dulls every note to fix the few. Cohere cuts those two bands only
    // while they stick out relative to the rest of the signal, then glues the
    // result with a gentle compressor and a touch of tape-style saturation.
    //
    //   1. Dynamic "bite" cut  (3.5 kHz) and "fizz" cut (7.5 kHz), detected
    //      in narrow bands (Q 2.5 / 3) and cut with smoother ones (Q 1.4 / 1.2):
    //      each band's share of the energy is compared with its own 1.5 s
    //      average share; whatever sticks out above that (by a margin that
    //      shrinks as COHERE rises) is cut, up to 9 / 12 dB. The guitar's
    //      overall tone is kept; only the harsh moments are caught.
    //      Linked detection, per-channel filtering.
    //   2. Glue: soft-knee RMS compressor, slow attack / release.
    //   3. Saturation: gentle, unity-slope tanh, blended in by COHERE.
    //   4. Calibrated static make-up, MIX (phase-exact), OUTPUT.
    //
    // Modes: Smooth (mostly taming), Glue (balanced), Tape (more glue and
    // saturation, slightly softer top). Zero latency.
    class CohereDSP
    {
    public:
        enum Mode { Smooth = 0, Glue = 1, Tape = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> outputDb { -90.0f };
        std::atomic<float> tameDb   {   0.0f };   // larger of the two band cuts
        std::atomic<float> glueDb   {   0.0f };   // compressor gain reduction

        void prepare (double sr, int /*blockSize*/, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jlimit (1, 8, numChannels);

            // Narrow detectors (so a spike really stands out from the
            // guitar's normal mids), smoother and wider cut filters.
            const float fizzHz = juce::jmin (7500.0f, (float) sr * 0.42f);
            biteBP = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sr, 3500.0f, 1.4f);
            fizzBP = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sr, fizzHz, 1.2f);
            for (int c = 0; c < 8; ++c) { bite[c].reset(); fizz[c].reset(); top[c].reset(); }
            biteDet.reset(); fizzDet.reset();
            *biteDet.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sr, 3500.0f, 2.5f);
            *fizzDet.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sr, fizzHz, 3.0f);
            for (int c = 0; c < 8; ++c) { *bite[c].coefficients = biteBP; *fizz[c].coefficients = fizzBP; }
            updateTop (Glue);

            msAll = msBite = msFizz = 1.0e-9f;
            gBite = gFizz = 1.0f;
            glueMs = 0.0f; glueGr = 0.0f;
            detAtk = coef (0.003); detRel = coef (0.080);
            avgA   = coef (1.5);
            biteAvg = -12.0f; fizzAvg = -20.0f;
            gSm    = coef (0.002);
            glueRms = coef (0.010);
            curMode = -1;

            outGainSm.reset (sr, 0.02);
            mixSm.reset (sr, 0.02);
            amtSm.reset (sr, 0.03);
            outGainSm.setCurrentAndTargetValue (1.0f);
            mixSm.setCurrentAndTargetValue (1.0f);
            amtSm.setCurrentAndTargetValue (0.0f);
            envIn = envOut = tameEnv = glueEnv = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float amount, int modeIn, float mix, float outDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            const Mode m = (Mode) juce::jlimit (0, 2, modeIn);
            if ((int) m != curMode) { updateTop (m); curMode = (int) m; }

            // Mode character at 100 %: { tame strength, glue ratio-1, glue thresh, sat drive }
            float tameK = 1.0f, glueK = 1.0f, satK = 0.5f;
            switch (m)
            {
                case Smooth: tameK = 1.25f; glueK = 0.5f; satK = 0.25f; break;
                case Tape:   tameK = 0.85f; glueK = 1.4f; satK = 1.0f;  break;
                default: break;
            }

            amtSm.setTargetValue (juce::jlimit (0.0f, 1.0f, amount));
            outGainSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));
            mixSm.setTargetValue (mix);

            const float glueAtk = coef (0.020), glueRel = coef (0.250);

            float* ptr[8] = {};
            for (int c = 0; c < numCh; ++c) ptr[c] = buffer.getWritePointer (c);

            float pkIn = 0.0f, pkOut = 0.0f, tameMax = 0.0f, glueMax = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float a = amtSm.getNextValue();

                // ---- linked detection ---------------------------------
                float mono = 0.0f, inPk = 0.0f;
                for (int c = 0; c < numCh; ++c) { mono += ptr[c][i]; inPk = juce::jmax (inPk, std::abs (ptr[c][i])); }
                mono /= (float) numCh;
                const float hb = biteDet.processSample (mono);
                const float fb = fizzDet.processSample (mono);
                follow (msAll,  mono * mono);
                follow (msBite, hb * hb);
                follow (msFizz, fb * fb);

                // Each band's share of the energy, compared with its own
                // long-term average share: only moments that stick out
                // above the guitar's normal balance get cut. The margin
                // shrinks as COHERE rises (6 dB -> 1 dB).
                const float gate     = msAll > 1.0e-7f ? 1.0f : 0.0f;   // ignore silence
                const float biteRat  = 10.0f * std::log10 ((msBite + 1.0e-12f) / (msAll + 1.0e-12f));
                const float fizzRat  = 10.0f * std::log10 ((msFizz + 1.0e-12f) / (msAll + 1.0e-12f));
                if (gate > 0.0f)
                {
                    biteAvg += (biteRat - biteAvg) * avgA;
                    fizzAvg += (fizzRat - fizzAvg) * avgA;
                }
                const float margin   = 6.0f - 5.0f * a;
                const float biteOver = biteRat - (biteAvg + margin);
                const float fizzOver = fizzRat - (fizzAvg + margin);
                const float biteCut   = gate * juce::jlimit (0.0f, 9.0f, biteOver * tameK) * juce::jmin (1.0f, a * 2.0f);
                const float fizzCut   = gate * juce::jlimit (0.0f, 12.0f, fizzOver * tameK) * juce::jmin (1.0f, a * 2.0f);
                gBite += (juce::Decibels::decibelsToGain (-biteCut) - gBite) * gSm;
                gFizz += (juce::Decibels::decibelsToGain (-fizzCut) - gFizz) * gSm;

                // ---- glue detector (linked) ---------------------------
                glueMs += (mono * mono - glueMs) * glueRms;
                const float lvl    = 10.0f * std::log10 (glueMs + 1.0e-12f);
                const float thresh = -12.0f - 14.0f * a;
                const float ratio  = 1.0f + 1.5f * a * glueK;
                const float over   = lvl - thresh;
                constexpr float knee = 8.0f;
                float grTarget = 0.0f;
                if (over > knee * 0.5f)       grTarget = -over * (1.0f - 1.0f / ratio);
                else if (over > -knee * 0.5f) { const float k = over + knee * 0.5f; grTarget = -(k * k) / (2.0f * knee) * (1.0f - 1.0f / ratio); }
                glueGr += (grTarget - glueGr) * (grTarget < glueGr ? glueAtk : glueRel);
                const float glueG = juce::Decibels::decibelsToGain (glueGr);

                // ---- saturation + make-up -----------------------------
                const float drive  = 1.0f + 1.2f * a * satK;
                const float satMix = juce::jmin (1.0f, a * 1.5f) * satK;
                // Static make-up: the glue's gain reduction at a typical bus
                // level, given back (no adaptive drift).
                const float refOver = juce::jmax (0.0f, kMakeupRefDb - thresh);
                const float makeup  = juce::Decibels::decibelsToGain (refOver * (1.0f - 1.0f / ratio) * kMakeupAmount);
                const float og = outGainSm.getNextValue();
                const float mx = mixSm.getNextValue();

                for (int c = 0; c < numCh; ++c)
                {
                    const float x  = ptr[c][i];
                    float y = x - (1.0f - gBite) * bite[c].processSample (x)
                                - (1.0f - gFizz) * fizz[c].processSample (x);
                    y *= glueG;
                    const float s = std::tanh (drive * y) / drive;
                    y = y + (s - y) * satMix;
                    y = top[c].processSample (y);
                    const float out = (y * makeup * og) * mx + x * (1.0f - mx);
                    ptr[c][i] = out;
                    pkOut = juce::jmax (pkOut, std::abs (out));
                }
                pkIn    = juce::jmax (pkIn, inPk);
                tameMax = juce::jmax (tameMax, juce::jmax (biteCut, fizzCut));
                glueMax = juce::jmin (glueMax, glueGr);
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envIn  += (pkIn  - envIn)  * k;
            envOut += (pkOut - envOut) * k;
            tameEnv = juce::jmax (tameMax, tameEnv * (1.0f - k));
            glueEnv += (-glueMax - glueEnv) * k;
            inputDb.store  (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            tameDb.store   (tameEnv, std::memory_order_relaxed);
            glueDb.store   (juce::jmax (0.0f, glueEnv), std::memory_order_relaxed);
        }

    private:
        static constexpr float kMakeupRefDb  = -18.0f;   // calibrated on amp-sim guitar
        static constexpr float kMakeupAmount = 1.0f;

        void follow (float& ms, float p) const { ms += (p - ms) * (p > ms ? detAtk : detRel); }
        float coef (double seconds) const { return (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate))); }

        // Tape mode rounds the very top a little; other modes are flat.
        void updateTop (Mode m)
        {
            const auto t = m == Tape
                ? juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sampleRate, 9000.0f, 0.7f, juce::Decibels::decibelsToGain (-1.5f))
                : juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sampleRate, 9000.0f, 0.7f, 1.0f);
            for (int c = 0; c < 8; ++c) *top[c].coefficients = t;
        }

        double sampleRate { 48000.0 };
        int    channels   { 2 };
        int    curMode    { -1 };

        std::array<float, 6> biteBP {}, fizzBP {};
        juce::dsp::IIR::Filter<float> biteDet, fizzDet;
        std::array<juce::dsp::IIR::Filter<float>, 8> bite, fizz, top;

        float msAll { 0 }, msBite { 0 }, msFizz { 0 }, gBite { 1 }, gFizz { 1 };
        float glueMs { 0 }, glueGr { 0 };
        float detAtk { 0 }, detRel { 0 }, gSm { 0 }, glueRms { 0 };
        float avgA { 0 }, biteAvg { -12.0f }, fizzAvg { -20.0f };

        juce::SmoothedValue<float> outGainSm, mixSm, amtSm;
        float envIn { 0 }, envOut { 0 }, tameEnv { 0 }, glueEnv { 0 };
    };
}
