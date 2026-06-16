#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace na
{
    // ------------------------------------------------------------------
    // AtiNAMatiC-Air — cab feel enhancer (placed AFTER cab/IR).
    //
    // The single AIR macro (0..1) controls together:
    //   - high "air" shelf (~10 kHz)  : 0..+5 dB
    //   - presence peak (~5 kHz)      : 0..+2 dB (gentle)
    //   - M/S sides gain              : 1.0..~1.6  (subtle widening)
    //   - mode-dependent body / depth amount
    //
    // Modes shape the colour & depth, NOT the macro:
    //   Studio : tight, controlled. Smaller widening, no low-mid peak.
    //   Room   : bigger air, +small ~80 Hz speaker resonance peak,
    //            +short side Haas delay for "depth" feel.
    //
    // All filtering done with TPT/IIR. Signal stays mono-sum-safe (M/S
    // widening only scales the sides, never inverts polarity or delays
    // mid).
    // ------------------------------------------------------------------
    class AirDSP
    {
    public:
        enum Mode { Studio = 0, Room = 1 };

        // ---- Public meters (read by editor on a UI timer) ---------------
        std::atomic<float> inputDb     { -90.0f };
        std::atomic<float> outputDb    { -90.0f };
        std::atomic<float> airAmount   {   0.0f };  // 0..1 — sides/mid energy

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);

            const juce::dsp::ProcessSpec spec {
                sr, (juce::uint32) juce::jmax (blockSize, 32), (juce::uint32) channels
            };

            // Per-channel air shelf + presence peak + body peak (Room only).
            airCoefs      = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1000.0f, 0.7f, 1.0f);
            presenceCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1000.0f, 0.7f, 1.0f);
            bodyCoefs     = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1000.0f, 0.7f, 1.0f);

            for (int c = 0; c < (int) airF.size(); ++c)
            {
                airF[c].reset();      airF[c].coefficients      = airCoefs;
                presenceF[c].reset(); presenceF[c].coefficients = presenceCoefs;
                bodyF[c].reset();     bodyF[c].coefficients     = bodyCoefs;
            }

            // Side Haas delay buffer (short, ~10 ms max). Used only in Room
            // mode to add depth without smearing transients on mid.
            const int maxDelay = (int) std::ceil (sr * 0.012);
            sideDelay.assign ((size_t) juce::jmax (1, maxDelay) + 4, 0.0f);
            sideWritePos = 0;

            envIn  = 0.0f;
            envOut = 0.0f;
            airE   = 0.0f;

            currentMode = -1;
            updateAir (0.0f, Studio);
        }

        void process (juce::AudioBuffer<float>& buffer,
                      float air, int mode, float mix, float outGainDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            const Mode m = (Mode) juce::jlimit (0, 1, mode);

            // --- Macro mapping -------------------------------------------
            // Shelf / presence / body amounts are computed inside updateAir;
            // here we only need the values used directly by the per-sample
            // mixer (widening + Haas).
            const float sidesGain     = 1.0f + air * (m == Room ? 0.7f : 0.4f);
            const int   sideDelaySmps = (m == Room)
                ? (int) std::round (sampleRate * (0.0008 + 0.0035 * air)) // ~0.8..4.3 ms
                : 0;

            // Update shelves cheaply per block. Filters share coefficient
            // objects — write once, all per-channel filters see the change.
            if (m != currentMode || std::abs (lastAir - air) > 1.0e-4f)
            {
                updateAir (air, m);
                currentMode = m;
                lastAir     = air;
            }

            const float outGain = juce::Decibels::decibelsToGain (outGainDb);
            const float dryMul  = 1.0f - mix;
            const float wetMul  = mix;

            const int chClamp = juce::jmin (numCh, 8);
            float* writePtrs[8] = { nullptr };
            for (int c = 0; c < chClamp; ++c)
                writePtrs[c] = buffer.getWritePointer (c);

            const float envA = (float) (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));
            float pkIn = 0.0f, pkOut = 0.0f;

            const int delayBufSize = (int) sideDelay.size();

            // Stereo path: M/S processing for widening + per-channel EQ.
            if (chClamp >= 2)
            {
                const float invSqrt2 = 0.70710678f;

                for (int i = 0; i < n; ++i)
                {
                    const float L = writePtrs[0][i];
                    const float R = writePtrs[1][i];

                    // EQ each side first (both channels share coefficients).
                    const float Lf = bodyF[0].processSample (
                                        presenceF[0].processSample (
                                            airF[0].processSample (L)));
                    const float Rf = bodyF[1].processSample (
                                        presenceF[1].processSample (
                                            airF[1].processSample (R)));

                    // Mid / Side (energy-preserving 1/sqrt(2) form).
                    const float Mid  = (Lf + Rf) * invSqrt2;
                    float       Side = (Lf - Rf) * invSqrt2;

                    // Optional Haas-ish side delay for depth (Room).
                    sideDelay[(size_t) sideWritePos] = Side;
                    if (sideDelaySmps > 0)
                    {
                        int rp = sideWritePos - sideDelaySmps;
                        if (rp < 0) rp += delayBufSize;
                        Side = sideDelay[(size_t) rp];
                    }
                    if (++sideWritePos >= delayBufSize) sideWritePos = 0;

                    // Widen.
                    Side *= sidesGain;

                    const float wetL = (Mid + Side) * invSqrt2;
                    const float wetR = (Mid - Side) * invSqrt2;

                    const float yL = wetL * outGain * wetMul + L * dryMul;
                    const float yR = wetR * outGain * wetMul + R * dryMul;

                    writePtrs[0][i] = yL;
                    writePtrs[1][i] = yR;

                    const float aIn  = juce::jmax (std::abs (L),  std::abs (R));
                    const float aOut = juce::jmax (std::abs (yL), std::abs (yR));
                    if (aIn  > pkIn)  pkIn  = aIn;
                    if (aOut > pkOut) pkOut = aOut;
                }
            }
            else
            {
                // Mono fallback: no widening (no sides), just EQ + makeup.
                for (int i = 0; i < n; ++i)
                {
                    const float x = writePtrs[0][i];
                    const float f = bodyF[0].processSample (
                                       presenceF[0].processSample (
                                           airF[0].processSample (x)));
                    const float y = f * outGain * wetMul + x * dryMul;
                    writePtrs[0][i] = y;

                    const float aIn  = std::abs (x);
                    const float aOut = std::abs (y);
                    if (aIn  > pkIn)  pkIn  = aIn;
                    if (aOut > pkOut) pkOut = aOut;
                }
            }

            envIn  += (pkIn  - envIn)  * envA;
            envOut += (pkOut - envOut) * envA;

            // air activity = knob position scaled by input signal level.
            // Works correctly for mono sources (guitar -> NAM -> here):
            // DRY when knob=0 or no signal, LIFT/OPEN/WIDE as air and level rise.
            const float inDb      = juce::Decibels::gainToDecibels (envIn, -90.0f);
            const float inNorm    = juce::jlimit (0.0f, 1.0f, (inDb + 60.0f) / 60.0f);
            const float airTarget = air * inNorm;
            airE += (airTarget - airE) * envA;

            inputDb.store   (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store  (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            airAmount.store (juce::jlimit (0.0f, 1.0f, airE),                 std::memory_order_relaxed);
        }

    private:
        void updateAir (float air, Mode m)
        {
            const float airShelfDb = air * 5.0f;
            const float presenceDb = air * 2.0f;
            const float bodyDb     = (m == Room ? air * 2.5f : 0.0f);

            *airCoefs      = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                                sampleRate, 10000.0f, 0.5f,
                                juce::Decibels::decibelsToGain (airShelfDb));

            *presenceCoefs = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                                sampleRate, 5000.0f, 0.8f,
                                juce::Decibels::decibelsToGain (presenceDb));

            // Room mode adds a gentle ~80 Hz speaker-cab resonance peak.
            // Studio mode = unity (no body peak).
            *bodyCoefs     = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                                sampleRate, 80.0f, 1.2f,
                                juce::Decibels::decibelsToGain (bodyDb));
        }

        double sampleRate { 48000.0 };
        int    channels   { 2 };

        juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>
            airCoefs, presenceCoefs, bodyCoefs;
        std::array<juce::dsp::IIR::Filter<float>, 8> airF, presenceF, bodyF;

        std::vector<float> sideDelay;
        int   sideWritePos { 0 };

        int   currentMode  { -1 };
        float lastAir      { -1.0f };

        float envIn  { 0.0f };
        float envOut { 0.0f };
        float airE   { 0.0f };
    };
}
