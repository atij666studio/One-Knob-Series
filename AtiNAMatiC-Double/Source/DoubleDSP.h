#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <random>

namespace nd
{
    // Convincing studio double-tracking.
    //
    //  - Always produces stereo output (mono in is duplicated).
    //  - Two virtual "performance" voices: each is a fractional-delay tap
    //    of the dry signal, panned to opposite sides.
    //  - Each voice's delay is modulated by a slow band-limited random
    //    walk (NOT a sine LFO) — that gives micro-pitch variation and
    //    timing drift that feels like two takes, never like a chorus.
    //  - The two voices use independent random sources at slightly
    //    different rates so their pitch/time wander is uncorrelated → no
    //    static comb filtering when summed to mono.
    //  - Each voice gets its own gentle tone (A a touch darker, B a touch
    //    leaner in the lows) so they read as separate takes, not copies,
    //    and the mono sum combs less.
    //  - Mono safety: the dry signal is always blended back through the
    //    centre so the mix can't phase-cancel down to nothing. At 0 % the
    //    voices are faded out completely (true mono / bypass-like).
    //
    // Single user-facing macro: WIDTH 0..1 simultaneously controls the
    // base delay spread, the drift magnitude, and the L/R pan amount.
    class DoubleDSP
    {
    public:
        enum Mode { Tight = 0, Loose = 1, Wide = 2 };

        // ---- Public meters (read by editor on a UI timer) ---------------
        std::atomic<float> levelL    { -90.0f };
        std::atomic<float> levelR    { -90.0f };
        std::atomic<float> driftDispA{   0.0f };  // -1..1
        std::atomic<float> driftDispB{   0.0f };  // -1..1
        std::atomic<float> widthDisp {   0.0f };  // 0..1, smoothed

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            juce::ignoreUnused (numChannels);

            // Max delay we ever read at: ~40 ms gives plenty of head room
            // for base delay + drift swing across all modes / widths.
            const int maxDelaySamples =
                juce::jmax (64, (int) std::ceil (sr * 0.045));

            // Power-of-two-sized circular buffer per side simplifies wraps
            // and gives fast bitmask indexing.
            int sz = 1;
            while (sz < maxDelaySamples + blockSize + 8) sz <<= 1;
            bufferSize = sz;
            mask       = sz - 1;

            for (auto& v : { &voiceA, &voiceB })
            {
                v->buffer.assign ((size_t) sz, 0.0f);
                v->writePos = 0;
                v->driftSmoothed = 0.0f;
                v->driftTarget   = 0.0f;
                v->samplesUntilNewTarget = 0;
                v->currentDelay = 0.0f;
            }

            // Random-walk smoothing: very long 3 s tau keeps the pitch
            // derivative well under 5 cents at all times — human-sounding
            // timing wander, not chorus LFO.
            driftSmoothCoeff = 1.0f - std::exp (-1.0f / (float) (sr * 3.0));

            // Output level smoothers (avoid mode/knob clicks)
            widthSmoothCoeff = 1.0f - std::exp (-1.0f / (float) (sr * 0.030));
            mixSmoothCoeff   = widthSmoothCoeff;
            outSmoothCoeff   = widthSmoothCoeff;

            widthSmoothed = 0.0f;
            mixSmoothed   = 1.0f;
            outSmoothed   = 1.0f;
            for (auto& g : gSm) g = 0.0f;
            firstBlock    = true;

            // Per-voice tone: A one-pole LP ~6.5 kHz, B LP ~9 kHz + HP ~110 Hz.
            lpA = (float) std::exp (-juce::MathConstants<double>::twoPi * 6500.0 / sr);
            lpB = (float) std::exp (-juce::MathConstants<double>::twoPi * 9000.0 / sr);
            hpB = (float) std::exp (-juce::MathConstants<double>::twoPi * 110.0  / sr);
            zA = zB = hpBx = hpBy = 0.0f;

            rng.seed (0xA17ED00B);
        }

        // width  : 0..1
        // mode   : Tight / Loose / Wide
        // mix    : 0..1   (0 = dry, 1 = full doubled image)
        // outDb  : output trim in dB
        void process (juce::AudioBuffer<float>& buffer,
                      float width, int mode, float mix, float outDb)
        {
            const int numIn  = buffer.getNumChannels();
            const int n      = buffer.getNumSamples();
            if (numIn <= 0 || n <= 0) return;

            const float outGain = juce::Decibels::decibelsToGain (outDb);

            // Mode shapes the base time / drift envelope (NOT the rate of
            // randomness — that stays slow in every mode so we never enter
            // chorus territory).
            float baseDelayMsA = 8.0f,  baseDelayMsB = 14.0f;
            float driftMs      = 1.2f;
            float maxPan       = 0.85f;
            switch (mode)
            {
                case Loose: baseDelayMsA = 11.0f; baseDelayMsB = 19.0f; driftMs = 2.0f; maxPan = 0.90f; break;
                case Wide:  baseDelayMsA = 14.0f; baseDelayMsB = 26.0f; driftMs = 2.8f; maxPan = 1.00f; break;
                default:    break;
            }

            // Width curve: 0–20% subtle, 20–60% natural, 60–100% wide.
            // S-shape pulls the macro toward the "natural double" zone.
            const float wCurved = juce::jlimit (0.0f, 1.0f,
                                                width * width * (3.0f - 2.0f * width));

            // Base delays scale a touch with width so low widths still
            // sound coherent and high widths feel ambitious.
            const float baseDelayAsamp =
                (float) (sampleRate * 0.001 * baseDelayMsA * (0.55f + 0.55f * wCurved));
            const float baseDelayBsamp =
                (float) (sampleRate * 0.001 * baseDelayMsB * (0.55f + 0.55f * wCurved));

            // Drift swing in samples (peak deviation around base).
            const float driftRange =
                (float) (sampleRate * 0.001 * driftMs) * (0.20f + 0.80f * wCurved);

            // Pan amount per side: voice A → left, voice B → right.
            const float panAmt = maxPan * wCurved;

            // Voice gain rises a touch with width (without pulling the dry
            // image apart) so doubles "lean in" as you open up. Below ~15 %
            // the voices fade out entirely, so 0 % is a clean mono signal
            // instead of a centred, comb-filtered copy.
            const float voiceGain = (0.55f + 0.20f * wCurved) * juce::jmin (1.0f, width / 0.15f);

            // Dry centre always present for mono safety.
            const float centreGain = 1.0f - 0.25f * wCurved;

            const float* inL = buffer.getReadPointer (0);
            const float* inR = (numIn >= 2 ? buffer.getReadPointer (1) : inL);

            // We only ever output stereo, but processBlock guarantees ≥1
            // channel; we wrote inR via fallback above for the mono case.
            float* outL = buffer.getWritePointer (0);
            float* outR = (buffer.getNumChannels() >= 2
                              ? buffer.getWritePointer (1)
                              : nullptr);

            // Equal-power pan helpers ------------------------------------
            // panAmt = 0 → both voices centred; panAmt = 1 → A hard L, B hard R.
            const float thetaA = juce::MathConstants<float>::pi * 0.25f * (1.0f - panAmt);
            const float thetaB = juce::MathConstants<float>::pi * 0.25f * (1.0f + panAmt);
            // Targets for the per-sample smoothed gains (a fast knob move used
            // to step these once per block = zipper noise).
            const float tgt[kNumGains] { std::cos (thetaA) * voiceGain, std::sin (thetaA) * voiceGain,
                                         std::cos (thetaB) * voiceGain, std::sin (thetaB) * voiceGain,
                                         centreGain };
            if (firstBlock)
            {
                for (int k = 0; k < kNumGains; ++k) gSm[k] = tgt[k];
                voiceA.currentDelay = baseDelayAsamp;     // no pitch glide from 0 ms on load
                voiceB.currentDelay = baseDelayBsamp;
                firstBlock = false;
            }

            float runningSqL = 0.0f, runningSqR = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                // Slewed knobs --------------------------------------------
                widthSmoothed += (wCurved - widthSmoothed) * widthSmoothCoeff;
                mixSmoothed   += (mix     - mixSmoothed)   * mixSmoothCoeff;
                outSmoothed   += (outGain - outSmoothed)   * outSmoothCoeff;

                const float src = 0.5f * (inL[i] + inR[i]);

                // Write same source into both voices' delay lines (cheap
                // and lets us read with independent fractional taps).
                voiceA.buffer[(size_t) voiceA.writePos] = src;
                voiceB.buffer[(size_t) voiceB.writePos] = src;

                // ----- Update slow random drift per voice ---------------
                if (--voiceA.samplesUntilNewTarget <= 0)
                {
                    voiceA.driftTarget = nextSignedRandom();
                    voiceA.samplesUntilNewTarget =
                        (int) (sampleRate * (4.0f + 8.0f * nextUnitRandom()));
                }
                if (--voiceB.samplesUntilNewTarget <= 0)
                {
                    voiceB.driftTarget = nextSignedRandom();
                    voiceB.samplesUntilNewTarget =
                        (int) (sampleRate * (5.0f + 9.0f * nextUnitRandom()));
                }
                voiceA.driftSmoothed +=
                    (voiceA.driftTarget - voiceA.driftSmoothed) * driftSmoothCoeff;
                voiceB.driftSmoothed +=
                    (voiceB.driftTarget - voiceB.driftSmoothed) * driftSmoothCoeff;

                // Target delay in samples (always > 1 so reads never alias
                // into the just-written sample).
                const float targA = juce::jmax (1.0f,
                    baseDelayAsamp + voiceA.driftSmoothed * driftRange);
                const float targB = juce::jmax (1.0f,
                    baseDelayBsamp + voiceB.driftSmoothed * driftRange);

                // Slew the actual read-delay ourselves so we never jump
                // (jumps = clicks). This sample-rate-portable slew is the
                // very thing that gives the read-tap its micro-pitch.
                voiceA.currentDelay += (targA - voiceA.currentDelay) * driftSmoothCoeff;
                voiceB.currentDelay += (targB - voiceB.currentDelay) * driftSmoothCoeff;

                for (int k = 0; k < kNumGains; ++k)
                    gSm[k] += (tgt[k] - gSm[k]) * widthSmoothCoeff;

                // Read both voices and give each its own tone.
                const float aRaw = readFrac (voiceA, voiceA.currentDelay);
                const float bRaw = readFrac (voiceB, voiceB.currentDelay);
                zA = aRaw + lpA * (zA - aRaw);
                zB = bRaw + lpB * (zB - bRaw);
                const float bHp = zB - hpBx + hpB * hpBy;
                hpBx = zB; hpBy = bHp;
                const float a = zA, b = bHp;

                // Doubled image: dry centre + panned voices ---------------
                const float doubledL = inL[i] * gSm[4] + a * gSm[0] + b * gSm[2];
                const float doubledR = inR[i] * gSm[4] + a * gSm[1] + b * gSm[3];

                // Wet/dry against the original signal.
                const float yL = (inL[i] * (1.0f - mixSmoothed) + doubledL * mixSmoothed) * outSmoothed;
                const float yR = (inR[i] * (1.0f - mixSmoothed) + doubledR * mixSmoothed) * outSmoothed;

                outL[i] = yL;
                if (outR != nullptr) outR[i] = yR;

                runningSqL += yL * yL;
                runningSqR += yR * yR;

                voiceA.writePos = (voiceA.writePos + 1) & mask;
                voiceB.writePos = (voiceB.writePos + 1) & mask;
            }

            // Publish meters once per block (cheap, smooth enough for UI).
            const float invN = 1.0f / (float) n;
            const float rmsL = std::sqrt (runningSqL * invN + 1.0e-12f);
            const float rmsR = std::sqrt (runningSqR * invN + 1.0e-12f);
            levelL.store     (juce::Decibels::gainToDecibels (rmsL, -90.0f), std::memory_order_relaxed);
            levelR.store     (juce::Decibels::gainToDecibels (rmsR, -90.0f), std::memory_order_relaxed);
            driftDispA.store (juce::jlimit (-1.0f, 1.0f, voiceA.driftSmoothed),
                              std::memory_order_relaxed);
            driftDispB.store (juce::jlimit (-1.0f, 1.0f, voiceB.driftSmoothed),
                              std::memory_order_relaxed);
            widthDisp.store  (juce::jlimit (0.0f, 1.0f, widthSmoothed),
                              std::memory_order_relaxed);
        }

    private:
        struct Voice
        {
            std::vector<float> buffer;
            int   writePos              { 0 };
            float currentDelay          { 0.0f };
            float driftSmoothed         { 0.0f };
            float driftTarget           { 0.0f };
            int   samplesUntilNewTarget { 0 };
        };

        Voice voiceA, voiceB;
        int   bufferSize { 0 };
        int   mask       { 0 };

        double sampleRate { 48000.0 };

        float driftSmoothCoeff { 0.0f };
        float widthSmoothCoeff { 0.0f };
        float mixSmoothCoeff   { 0.0f };
        float outSmoothCoeff   { 0.0f };

        float widthSmoothed { 0.0f };
        float mixSmoothed   { 1.0f };
        float outSmoothed   { 1.0f };

        // Smoothed gains: A->L, A->R, B->L, B->R, centre.
        static constexpr int kNumGains = 5;
        float gSm[kNumGains] {};
        bool  firstBlock { true };

        float lpA { 0.0f }, lpB { 0.0f }, hpB { 0.0f };
        float zA { 0.0f }, zB { 0.0f }, hpBx { 0.0f }, hpBy { 0.0f };

        std::mt19937 rng { 0xA17ED00B };

        float nextUnitRandom()
        {
            return (float) ((rng() & 0xFFFFFF) / (double) 0xFFFFFF);
        }
        float nextSignedRandom() { return nextUnitRandom() * 2.0f - 1.0f; }

        // Linear-interpolated read into a circular buffer.
        float readFrac (const Voice& v, float delaySamples) const
        {
            const float maxD = (float) (bufferSize - 4);
            delaySamples = juce::jlimit (1.0f, maxD, delaySamples);
            const int   d0   = (int) delaySamples;
            const float frac = delaySamples - (float) d0;

            const int p0 = (v.writePos - d0)     & mask;
            const int p1 = (v.writePos - d0 - 1) & mask;
            const float s0 = v.buffer[(size_t) p0];
            const float s1 = v.buffer[(size_t) p1];
            return s0 + (s1 - s0) * frac;
        }
    };
}
