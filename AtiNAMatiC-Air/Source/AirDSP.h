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
    //   Studio : tight, controlled. Smaller widening, a few very short
    //            reflections (2-12 ms), no low-mid peak.
    //   Room   : bigger air, +small ~80 Hz speaker resonance peak, and a
    //            longer early-reflection pattern (5-29 ms) for "standing in
    //            front of the cab" depth.
    //
    // Depth on mono sources: a guitar out of a mono amp sim has no side
    // signal at all, so plain M/S widening does nothing. Air builds its
    // depth from a short early-reflection pattern of the MID signal that is
    // written ONLY into the side channel (high-passed at 150 Hz so the lows
    // stay centred, darkened above 6 kHz like a real room). L+R cancels it
    // exactly, so the mono sum is untouched. No tail -- it is not a reverb.
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

            // Mid history for the early-reflection generator (~40 ms).
            midHist.assign ((size_t) juce::nextPowerOfTwo ((int) std::ceil (sr * 0.040) + 4), 0.0f);
            midMask = (int) midHist.size() - 1;
            midPos  = 0;
            for (int k = 0; k < kStudioTaps; ++k) studioDelay[k] = (int) std::round (sr * 0.001 * kStudioMs[k]);
            for (int k = 0; k < kRoomTaps;   ++k) roomDelay[k]   = (int) std::round (sr * 0.001 * kRoomMs[k]);
            erHpA = (float) std::exp (-juce::MathConstants<double>::twoPi * 150.0 / sr);
            erLpA = (float) std::exp (-juce::MathConstants<double>::twoPi * 6000.0 / sr);
            erHpX = erHpY = erLp = 0.0f;
            gStudio = gRoom = 0.0f;
            erSmA = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
            sideMs = midMs = 0.0f;

            envIn  = 0.0f;
            envOut = 0.0f;
            airE   = 0.0f;

            currentMode = -1;
            smAir = 0.0f;
            updateAir (0.0f, Studio);

            // ~25 ms block-rate smoothing on the AIR macro -- avoids the
            // filter-coefficient jump click on fast knob moves/automation.
            smCoef = std::exp (-float (juce::jmax (blockSize, 32)) / float (0.025 * sr));

            // Per-sample ramps for output trim and dry/wet mix.
            outGainSm.reset (sr, 0.02);
            mixSm.reset (sr, 0.02);
            outGainSm.setCurrentAndTargetValue (1.0f);
            mixSm.setCurrentAndTargetValue (1.0f);
        }

        void process (juce::AudioBuffer<float>& buffer,
                      float air, int mode, float mix, float outGainDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            const Mode m = (Mode) juce::jlimit (0, 1, mode);

            // Smooth the AIR macro at block rate so the filter-coefficient
            // update below doesn't snap on a fast knob move/automation.
            smAir = smCoef * smAir + (1.0f - smCoef) * air;

            // --- Macro mapping -------------------------------------------
            // Shelf / presence / body amounts are computed inside updateAir;
            // here we only need the values used directly by the per-sample
            // mixer (widening + early reflections).
            const float sidesGain = 1.0f + smAir * (m == Room ? 0.7f : 0.4f);
            const float tgtStudio = (m == Studio ? smAir * 0.30f : 0.0f);
            const float tgtRoom   = (m == Room   ? smAir * 0.45f : 0.0f);

            // Update shelves cheaply per block. Filters share coefficient
            // objects — write once, all per-channel filters see the change.
            if (m != currentMode || std::abs (lastAir - smAir) > 1.0e-4f)
            {
                updateAir (smAir, m);
                currentMode = m;
                lastAir     = smAir;
            }

            outGainSm.setTargetValue (juce::Decibels::decibelsToGain (outGainDb));
            mixSm.setTargetValue (mix);

            const int chClamp = juce::jmin (numCh, 8);
            float* writePtrs[8] = { nullptr };
            for (int c = 0; c < chClamp; ++c)
                writePtrs[c] = buffer.getWritePointer (c);

            const float envA = (float) (1.0 - std::exp (-1.0 / (0.020 * sampleRate)));
            float pkIn = 0.0f, pkOut = 0.0f;

            // Stereo path: M/S processing for widening + per-channel EQ.
            if (chClamp >= 2)
            {
                const float invSqrt2 = 0.70710678f;

                for (int i = 0; i < n; ++i)
                {
                    const float L = writePtrs[0][i];
                    const float R = writePtrs[1][i];

                    // EQ each side first (both channels share coefficients).
                    const float Lf = runChain (0, L);
                    const float Rf = runChain (1, R);

                    // Mid / Side (energy-preserving 1/sqrt(2) form).
                    const float Mid  = (Lf + Rf) * invSqrt2;
                    float       Side = (Lf - Rf) * invSqrt2 * sidesGain;

                    // Early reflections of the mid, into the side only.
                    midHist[(size_t) midPos] = Mid;
                    gStudio += (tgtStudio - gStudio) * erSmA;
                    gRoom   += (tgtRoom   - gRoom)   * erSmA;
                    float er = 0.0f;
                    if (gStudio > 1.0e-5f)
                    {
                        float acc = 0.0f;
                        for (int k = 0; k < kStudioTaps; ++k)
                            acc += kStudioGain[k] * midHist[(size_t) ((midPos - studioDelay[k]) & midMask)];
                        er += acc * gStudio;
                    }
                    if (gRoom > 1.0e-5f)
                    {
                        float acc = 0.0f;
                        for (int k = 0; k < kRoomTaps; ++k)
                            acc += kRoomGain[k] * midHist[(size_t) ((midPos - roomDelay[k]) & midMask)];
                        er += acc * gRoom;
                    }
                    midPos = (midPos + 1) & midMask;

                    // Keep the lows centred, darken the top like a real room.
                    const float hp = er - erHpX + erHpA * erHpY;
                    erHpX = er; erHpY = hp;
                    erLp = hp + erLpA * (erLp - hp);
                    Side += erLp;

                    sideMs += (Side * Side - sideMs) * envA;
                    midMs  += (Mid  * Mid  - midMs)  * envA;

                    const float wetL = (Mid + Side) * invSqrt2;
                    const float wetR = (Mid - Side) * invSqrt2;

                    const float outGainNow = outGainSm.getNextValue();
                    const float mixNow     = mixSm.getNextValue();
                    const float yL = wetL * outGainNow * mixNow + L * (1.0f - mixNow);
                    const float yR = wetR * outGainNow * mixNow + R * (1.0f - mixNow);

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
                    const float f = runChain (0, x);
                    const float outGainNow = outGainSm.getNextValue();
                    const float mixNow     = mixSm.getNextValue();
                    const float y = f * outGainNow * mixNow + x * (1.0f - mixNow);
                    writePtrs[0][i] = y;

                    const float aIn  = std::abs (x);
                    const float aOut = std::abs (y);
                    if (aIn  > pkIn)  pkIn  = aIn;
                    if (aOut > pkOut) pkOut = aOut;
                }
            }

            envIn  += (pkIn  - envIn)  * envA;
            envOut += (pkOut - envOut) * envA;

            // Air activity = how much side/depth the output actually carries
            // (side-to-mid energy, -30 dB -> 0, -6 dB -> 1), or the knob
            // position for mono layouts where there is no side.
            float airTarget;
            if (chClamp >= 2 && midMs > 1.0e-9f)
            {
                const float sideToMidDb = 10.0f * std::log10 (juce::jmax (sideMs, 1.0e-12f) / midMs);
                airTarget = juce::jlimit (0.0f, 1.0f, (sideToMidDb + 30.0f) / 24.0f);
            }
            else
            {
                const float inDb = juce::Decibels::gainToDecibels (envIn, -90.0f);
                airTarget = air * juce::jlimit (0.0f, 1.0f, (inDb + 60.0f) / 60.0f);
            }
            airE += (airTarget - airE) * juce::jmin (1.0f, envA * (float) n);

            inputDb.store   (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store  (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            airAmount.store (juce::jlimit (0.0f, 1.0f, airE),                 std::memory_order_relaxed);
        }

    private:
        // Shared per-channel EQ chain (air shelf -> presence peak -> body peak).
        inline float runChain (int c, float x)
        {
            return bodyF[c].processSample (presenceF[c].processSample (airF[c].processSample (x)));
        }

        void updateAir (float air, Mode m)
        {
            const float airShelfDb = air * 5.0f;
            const float presenceDb = air * 2.0f;
            const float bodyDb     = (m == Room ? air * 2.5f : 0.0f);

            *airCoefs      = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
                                sampleRate, 10000.0f, 0.5f,
                                juce::Decibels::decibelsToGain (airShelfDb));

            *presenceCoefs = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
                                sampleRate, 5000.0f, 0.8f,
                                juce::Decibels::decibelsToGain (presenceDb));

            // Room mode adds a gentle ~80 Hz speaker-cab resonance peak.
            // Studio mode = unity (no body peak).
            *bodyCoefs     = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (
                                sampleRate, 80.0f, 1.2f,
                                juce::Decibels::decibelsToGain (bodyDb));
        }

        double sampleRate { 48000.0 };
        int    channels   { 2 };

        juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>
            airCoefs, presenceCoefs, bodyCoefs;
        std::array<juce::dsp::IIR::Filter<float>, 8> airF, presenceF, bodyF;

        // Early-reflection patterns (ms, gain incl. polarity). Alternating
        // polarity keeps the pattern from colouring the side like a comb.
        static constexpr int   kStudioTaps = 4;
        static constexpr float kStudioMs[kStudioTaps]   { 2.1f, 4.7f, 7.3f, 11.9f };
        static constexpr float kStudioGain[kStudioTaps] { 0.55f, -0.42f, 0.33f, -0.22f };
        static constexpr int   kRoomTaps = 6;
        static constexpr float kRoomMs[kRoomTaps]       { 5.3f, 9.1f, 13.7f, 17.9f, 23.3f, 29.0f };
        static constexpr float kRoomGain[kRoomTaps]     { 0.60f, -0.48f, 0.40f, -0.30f, 0.22f, -0.15f };

        std::vector<float> midHist;
        int   midMask { 0 }, midPos { 0 };
        int   studioDelay[kStudioTaps] {}, roomDelay[kRoomTaps] {};
        float erHpA { 0.0f }, erLpA { 0.0f }, erHpX { 0.0f }, erHpY { 0.0f }, erLp { 0.0f };
        float gStudio { 0.0f }, gRoom { 0.0f }, erSmA { 0.0f };
        float sideMs { 0.0f }, midMs { 0.0f };

        int   currentMode  { -1 };
        float lastAir      { -1.0f };

        // Block-rate smoothed AIR macro + per-sample output/mix ramps.
        float smAir  { 0.0f };
        float smCoef { 0.0f };
        juce::SmoothedValue<float> outGainSm, mixSm;

        float envIn  { 0.0f };
        float envOut { 0.0f };
        float airE   { 0.0f };
    };
}
