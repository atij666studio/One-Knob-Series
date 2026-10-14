#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace ng
{
    // Pro-grade musical gate for guitar.
    //
    //  - Sidechain HPF (~90 Hz) so sub rumble can't false-trigger
    //  - Dual-time-constant detector (fast peak + slow RMS)
    //  - Hysteresis (open ↔ close threshold)
    //  - Adaptive noise-floor tracking (Adaptive mode lifts threshold above
    //    measured floor with a 10 dB margin)
    //  - Hold time after detector falls below close threshold (no chatter)
    //  - Asymmetric attack/release with cubic ease for click-free transitions
    //  - Audio lookahead (~3 ms) so gate opens *before* the transient and
    //    never chops pick attacks
    //  - Soft attenuation floor (-65 dB), never a hard mute
    //
    // Single user-facing macro: gateAmt 0..1 (Open ↔ Tight) drives threshold,
    // hold and aggressiveness simultaneously.
    // Detector levels below this are never real playing, so the adaptive
    // floor may learn from them even while the gate is open.
    constexpr float kLearnCeiling = 0.00316f;   // -50 dBFS

    class GateDSP
    {
    public:
        enum Mode { Adaptive = 0, Fast = 1, Natural = 2 };

        // ---- Public meters (read by editor on a UI timer) ---------------
        std::atomic<float> inputDb       { -90.0f };
        std::atomic<float> openThreshDb  { -60.0f };
        std::atomic<float> closeThreshDb { -65.0f };
        std::atomic<float> noiseFloorDb  { -90.0f };
        std::atomic<float> gateGainDb    {   0.0f };  // 0 dB = fully open
        std::atomic<int>   stateAtomic   {   0   };   // 0 closed, 1 transition, 2 open

        int getLookaheadSamples() const { return lookaheadSamples; }

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);

            // 3 ms lookahead — short enough to feel zero-latency, long enough
            // to fully open the gate on a hard pick attack.
            lookaheadSamples = juce::jmax (1, (int) std::round (sr * 0.003));

            delayLine.setSize (channels,
                               lookaheadSamples + juce::jmax (blockSize, 1024),
                               false, true, true);
            delayLine.clear();
            delayWritePos = 0;

            rmsAlpha          = oneMinusExp (0.005);
            peakReleaseAlpha  = oneMinusExp (0.040);
            noiseTrackUpAlpha = oneMinusExp (3.000);
            noiseTrackDnAlpha = oneMinusExp (0.300);

            scHPF.reset();
            scHPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 90.0f);
            juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (blockSize, 32), 1u };
            scHPF.prepare (spec);

            rmsState     = 0.0f;
            peakState    = 0.0f;
            envFloor     = 1.0e-7f;
            currentGain  = 1.0f;
            gateOpen     = false;
            holdSamples  = 0;

            outGainSm.reset (sr, 0.02);
            mixSm.reset (sr, 0.02);
            outGainSm.setCurrentAndTargetValue (1.0f);
            mixSm.setCurrentAndTargetValue (1.0f);
        }

        void process (juce::AudioBuffer<float>& buffer,
                      float gateAmt, int mode, float mix, float outGainDb)
        {
            const int numCh = juce::jmin (channels, buffer.getNumChannels());
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            // Map macro knob to DSP behaviour.
            const float openThrDb  = juce::jmap (gateAmt, 0.0f, 1.0f, -78.0f, -22.0f);
            const float hystDb     = juce::jmap (gateAmt, 0.0f, 1.0f,   4.0f,   8.0f);
            const float closeThrDb = openThrDb - hystDb;

            float attackMs   = 1.5f;
            float releaseMs  = 90.0f;
            float baseHoldMs = 60.0f;
            switch (mode)
            {
                case Fast:    attackMs = 0.4f; releaseMs = 25.0f;  baseHoldMs = 20.0f;  break;
                case Natural: attackMs = 2.5f; releaseMs = 260.0f; baseHoldMs = 120.0f; break;
                default:      break;
            }
            const float holdMs    = baseHoldMs * (1.7f - gateAmt);
            const float relScaled = releaseMs   * (1.6f - 0.8f * gateAmt);

            const float attackAlpha   = oneMinusExpMs (attackMs);
            const float releaseAlpha  = oneMinusExpMs (relScaled);
            const int   holdSamplesMax = (int) std::round (holdMs * 0.001f * (float) sampleRate);

            const float floorGain = juce::Decibels::decibelsToGain (-65.0f);
            outGainSm.setTargetValue (juce::Decibels::decibelsToGain (outGainDb));
            mixSm.setTargetValue (mix);

            float lastOpenDb  = openThrDb;
            float lastCloseDb = closeThrDb;

            const int chClamp  = juce::jmin (numCh, 8);
            const int delayCap = delayLine.getNumSamples();

            float* writePtrs[8] = { nullptr };
            float* delayPtrs[8] = { nullptr };
            for (int c = 0; c < chClamp; ++c)
            {
                writePtrs[c] = buffer.getWritePointer (c);
                delayPtrs[c] = delayLine.getWritePointer (c);
            }

            for (int i = 0; i < n; ++i)
            {
                float mono = 0.0f;
                for (int c = 0; c < chClamp; ++c)
                    mono += writePtrs[c][i];
                mono /= (float) chClamp;

                const float sc = scHPF.processSample (mono);

                // RMS² (one-pole on x²).
                const float sq = sc * sc;
                rmsState += (sq - rmsState) * rmsAlpha;
                const float rmsLevel = std::sqrt (juce::jmax (rmsState, 1.0e-12f));

                // Peak: instant attack, slow release.
                const float absSc = std::abs (sc);
                if (absSc > peakState) peakState = absSc;
                else                   peakState += (absSc - peakState) * peakReleaseAlpha;

                // Combined detector: max(RMS, peak·0.6).
                const float detector = juce::jmax (rmsLevel, peakState * 0.6f);

                // Adaptive noise-floor tracking. Learns upward while the gate
                // is closed OR from anything quieter than real playing
                // (< -50 dBFS) -- otherwise noise that held the gate open
                // from the start was never learned, so Adaptive never
                // adapted in exactly the case it exists for.
                if (detector < envFloor)
                    envFloor += (detector - envFloor) * noiseTrackDnAlpha;
                else if (currentGain < 0.10f || detector < kLearnCeiling)
                    envFloor += (detector - envFloor) * noiseTrackUpAlpha;
                envFloor = juce::jmax (envFloor, 1.0e-7f);

                float openDb  = openThrDb;
                float closeDb = closeThrDb;
                if (mode == Adaptive)
                {
                    const float floorDb = juce::Decibels::gainToDecibels (envFloor, -120.0f);
                    const float minOpen = floorDb + 10.0f;
                    openDb  = juce::jmax (openDb,  minOpen);
                    closeDb = juce::jmax (closeDb, minOpen - hystDb);
                }
                lastOpenDb  = openDb;
                lastCloseDb = closeDb;

                const float openThrLin  = juce::Decibels::decibelsToGain (openDb);
                const float closeThrLin = juce::Decibels::decibelsToGain (closeDb);

                // Hysteresis + hold.
                if (! gateOpen)
                {
                    if (detector > openThrLin)
                    {
                        gateOpen    = true;
                        holdSamples = holdSamplesMax;
                    }
                }
                else
                {
                    if (detector > closeThrLin)
                        holdSamples = holdSamplesMax;
                    else if (holdSamples > 0)
                        --holdSamples;
                    else
                        gateOpen = false;
                }

                // Asymmetric one-pole gain smoother.
                const float targetGain = gateOpen ? 1.0f : floorGain;
                const float a = (targetGain > currentGain) ? attackAlpha : releaseAlpha;
                currentGain += (targetGain - currentGain) * a;

                // Cubic ease (smoothstep) on the residual range floor..1.
                const float lin = juce::jlimit (0.0f, 1.0f,
                                                (currentGain - floorGain) / (1.0f - floorGain));
                const float eased = lin * lin * (3.0f - 2.0f * lin);
                const float shapedGain = floorGain + eased * (1.0f - floorGain);

                int readPos = delayWritePos - lookaheadSamples;
                if (readPos < 0) readPos += delayCap;

                const float mixNow     = mixSm.getNextValue();
                const float outGainNow = outGainSm.getNextValue();
                const float gateMul  = shapedGain * mixNow + (1.0f - mixNow);
                const float finalMul = gateMul * outGainNow;

                for (int c = 0; c < chClamp; ++c)
                {
                    const float in  = writePtrs[c][i];
                    const float out = delayPtrs[c][readPos] * finalMul;
                    delayPtrs[c][delayWritePos] = in;
                    writePtrs[c][i]             = out;
                }

                if (++delayWritePos >= delayCap) delayWritePos = 0;

                if ((meterCounter++ & 63) == 0)
                {
                    inputDb.store      (juce::Decibels::gainToDecibels (detector,    -90.0f),  std::memory_order_relaxed);
                    gateGainDb.store   (juce::Decibels::gainToDecibels (shapedGain,  -90.0f),  std::memory_order_relaxed);
                    noiseFloorDb.store (juce::Decibels::gainToDecibels (envFloor,    -120.0f), std::memory_order_relaxed);
                }
            }

            openThreshDb.store  (lastOpenDb,  std::memory_order_relaxed);
            closeThreshDb.store (lastCloseDb, std::memory_order_relaxed);

            int s = 1;
            if      (currentGain > 0.92f) s = 2;
            else if (currentGain < 0.08f) s = 0;
            stateAtomic.store (s, std::memory_order_relaxed);
        }

    private:
        double sampleRate { 48000.0 };
        int    channels   { 2 };

        juce::AudioBuffer<float> delayLine;
        int    lookaheadSamples { 0 };
        int    delayWritePos    { 0 };

        float  rmsState   { 0.0f };
        float  peakState  { 0.0f };
        float  envFloor   { 1.0e-7f };

        float  currentGain { 1.0f };
        bool   gateOpen    { false };
        int    holdSamples { 0 };

        float  rmsAlpha          { 0.0f };
        float  peakReleaseAlpha  { 0.0f };
        float  noiseTrackUpAlpha { 0.0f };
        float  noiseTrackDnAlpha { 0.0f };

        juce::dsp::IIR::Filter<float> scHPF;

        juce::SmoothedValue<float> outGainSm, mixSm;

        unsigned meterCounter { 0 };

        float oneMinusExp (double tauSeconds) const
        {
            return (float) (1.0 - std::exp (-1.0 / (tauSeconds * sampleRate)));
        }
        float oneMinusExpMs (float ms) const
        {
            return oneMinusExp ((double) ms * 0.001);
        }
    };
}
