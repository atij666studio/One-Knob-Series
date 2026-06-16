#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace nt
{
    enum class Mode { Classic = 0, Modern = 1, Extreme = 2 };

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

    // Per-channel transient shaper using fast / slow envelopes.
    // Attack gain is applied where fast > slow (transient onset).
    struct TransientShaper
    {
        float fast { 0.0f }, slow { 0.0f };
        float aFast { 0.0f }, aSlow { 0.0f };
        float rFast { 0.0f }, rSlow { 0.0f };

        void prepare (double sr)
        {
            // Time constants: fast ~5 ms attack / 20 ms release,
            // slow ~30 ms attack / 150 ms release.
            aFast = std::exp (-1.0f / float (0.005 * sr));
            rFast = std::exp (-1.0f / float (0.020 * sr));
            aSlow = std::exp (-1.0f / float (0.030 * sr));
            rSlow = std::exp (-1.0f / float (0.150 * sr));
            fast = slow = 0.0f;
        }

        void reset() { fast = slow = 0.0f; }

        // Returns linear gain to multiply the sample by.
        // amount in [0..1]: 0 => bypass (gain==1), 1 => max ~+9 dB on transients,
        // sustain reduced by ~ -3 dB.
        inline float processSample (float x, float amount)
        {
            const float a = std::abs (x);

            const float cF = (a > fast) ? aFast : rFast;
            fast = cF * fast + (1.0f - cF) * a;
            const float cS = (a > slow) ? aSlow : rSlow;
            slow = cS * slow + (1.0f - cS) * a;

            // Attack signal in [0..1+]
            const float diff = juce::jmax (0.0f, fast - slow);
            const float norm = diff / juce::jmax (slow, 1.0e-5f);
            const float attack = juce::jmin (1.0f, norm);

            // Sustain signal: how "steady" we are (near 1 when fast≈slow).
            const float steady = 1.0f - juce::jmin (1.0f, norm * 2.0f);

            // Boost transients, attenuate sustain a touch.
            const float attackDb  = +9.0f * amount * attack;
            const float sustainDb = -3.0f * amount * steady;
            return dbToGain (attackDb + sustainDb);
        }
    };

    class TightDSP
    {
    public:
        // Live parameters (set each block by the processor)
        std::atomic<float> tight   { 0.5f };  // 0..1 master knob
        std::atomic<int>   modeIx  { (int) Mode::Modern };

        // Output (read by editor)
        std::atomic<float> lowEnergyDb { -60.0f }; // ~80 Hz band energy of input

        void prepare (double sr, int blockSize, int numChannels)
        {
            sampleRate = sr;
            channels   = juce::jmax (1, numChannels);

            juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockSize, (juce::uint32) channels };

            hpf1.prepare (spec); hpf2.prepare (spec);
            lowMidDip.prepare (spec);
            presence.prepare (spec);

            hpf1.reset(); hpf2.reset();
            lowMidDip.reset(); presence.reset();

            updateFilters (60.0f, 0.0f, 0.0f);
            smHpfHz = 60.0f; smDipDb = 0.0f; smPresDb = 0.0f;
            // ~40 ms time-constant at block rate — eliminates zipper noise on knob sweep
            smCoef = std::exp (-float (blockSize) / float (0.040 * sr));

            juce::dsp::ProcessSpec mSpec { sr, (juce::uint32) blockSize, 1u };
            lowBand.prepare (mSpec);
            lowBand.reset();
            *lowBand.state = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sr, 80.0, 0.5);

            for (auto& ts : shapers) ts.prepare (sr);

            shapeAmtSm.reset (sr, 0.030);
            shapeAmtSm.setCurrentAndTargetValue (0.0f);

            scratchDet.setSize (1, blockSize, false, false, true);
            shapeBuf.assign ((size_t) blockSize, 0.0f);

            lowEnv = 0.0f;
            lowEnvCoef = std::exp (-1.0f / float (0.080 * sr));
        }

        void reset()
        {
            hpf1.reset(); hpf2.reset();
            lowMidDip.reset(); presence.reset();
            lowBand.reset();
            for (auto& ts : shapers) ts.reset();
            lowEnv = 0.0f;
        }

        // In-place tightening: HPF → low-mid dip → presence shelf → transient shaper.
        // Updates `lowEnergyDb` for the meter from the *input* signal.
        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = juce::jmin (buffer.getNumChannels(), channels);
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0) return;

            const float t = juce::jlimit (0.0f, 1.0f, tight.load());
            const Mode  m = (Mode) juce::jlimit (0, 2, modeIx.load());

            float hpfHz, dipDb, presenceDb, shapeAmt, inputTrimDb;
            mapKnob (t, m, hpfHz, dipDb, presenceDb, shapeAmt, inputTrimDb);

            // Smooth filter params at block rate — no zipper noise when sweeping TIGHT
            smHpfHz  = smCoef * smHpfHz  + (1.0f - smCoef) * hpfHz;
            smDipDb  = smCoef * smDipDb  + (1.0f - smCoef) * dipDb;
            smPresDb = smCoef * smPresDb + (1.0f - smCoef) * presenceDb;
            updateFilters (smHpfHz, smDipDb, smPresDb);
            shapeAmtSm.setTargetValue (shapeAmt);

            // ---- Low-energy meter ----
            if (scratchDet.getNumSamples() < n) scratchDet.setSize (1, n, false, false, true);
            scratchDet.clear (0, 0, n);
            const float invCh = 1.0f / float (numCh);
            for (int ch = 0; ch < numCh; ++ch)
                scratchDet.addFrom (0, 0, buffer, ch, 0, n, invCh);
            {
                juce::dsp::AudioBlock<float> blk (scratchDet.getArrayOfWritePointers(), 1, 0, (size_t) n);
                juce::dsp::ProcessContextReplacing<float> ctx (blk);
                lowBand.process (ctx);
            }
            const float* d = scratchDet.getReadPointer (0);
            for (int i = 0; i < n; ++i)
            {
                const float a = std::abs (d[i]);
                if (a > lowEnv) lowEnv = a;
                else            lowEnv = lowEnvCoef * lowEnv + (1.0f - lowEnvCoef) * a;
            }
            lowEnergyDb.store (20.0f * std::log10 (juce::jmax (lowEnv, 1.0e-7f)));

            juce::ignoreUnused (inputTrimDb);

            // ---- Filters ----
            {
                juce::dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(),
                                                 (size_t) numCh, 0, (size_t) n);
                juce::dsp::ProcessContextReplacing<float> ctx (blk);
                hpf1.process (ctx);
                hpf2.process (ctx);
                lowMidDip.process (ctx);
                presence.process (ctx);
            }

            // ---- Transient shaping ----
            shapeBuf.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
                shapeBuf[(size_t) i] = shapeAmtSm.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* wp = buffer.getWritePointer (ch);
                auto& ts = shapers[(size_t) juce::jmin (ch, (int) shapers.size() - 1)];
                for (int i = 0; i < n; ++i)
                {
                    const float g = ts.processSample (wp[i], shapeBuf[(size_t) i]);
                    wp[i] *= g;
                }
            }
        }

    private:
        // Maps the master TIGHT knob through the Mode curve to per-stage params.
        static void mapKnob (float t, Mode m,
                             float& hpfHz, float& dipDb, float& presenceDb,
                             float& shapeAmt, float& inputTrimDb)
        {
            // Base curves (Modern):
            //  HPF:      60 Hz @ 0   ..  150 Hz @ 1
            //  Dip:       0 dB @ 0   .. -5.5 dB @ 1   centered around 280 Hz (Q ~1.0)
            //  Presence: +0 dB @ 0   .. +3.0 dB @ 1   high-shelf @ 2.5 kHz
            //  Shaper:    0   @ 0   ..   0.85 @ 1
            //  Trim:      0 dB                   (input level kept neutral)
            float hpfMin  = 55.0f, hpfMax  = 150.0f;
            float dipMax  = -5.5f;
            float presMax =  3.0f;
            float shpMax  =  0.85f;

            switch (m)
            {
                case Mode::Classic:
                    hpfMin  = 45.0f;  hpfMax = 110.0f;
                    dipMax  = -3.0f;
                    presMax =  1.5f;
                    shpMax  =  0.45f;
                    break;
                case Mode::Modern:
                    break; // defaults above
                case Mode::Extreme:
                    hpfMin  = 70.0f;  hpfMax = 220.0f;
                    dipMax  = -8.0f;
                    presMax =  4.5f;
                    shpMax  =  1.10f;
                    break;
            }

            // Slight ease-in so the lower half is gentler.
            const float te = t * t * (3.0f - 2.0f * t); // smoothstep

            hpfHz       = juce::jmap (te, 0.0f, 1.0f, hpfMin, hpfMax);
            dipDb       = juce::jmap (te, 0.0f, 1.0f, 0.0f,   dipMax);
            presenceDb  = juce::jmap (te, 0.0f, 1.0f, 0.0f,   presMax);
            shapeAmt    = juce::jmap (te, 0.0f, 1.0f, 0.0f,   shpMax);
            inputTrimDb = 0.0f;
        }

        void updateFilters (float hpfHz, float dipDb, float presDb)
        {
            *hpf1.state     = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass
                                 (sampleRate, hpfHz, 0.707f);
            *hpf2.state     = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass
                                 (sampleRate, hpfHz, 0.707f);
            *lowMidDip.state = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter
                                 (sampleRate, 280.0, 1.0f, dbToGain (dipDb));
            *presence.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf
                                 (sampleRate, 2500.0, 0.7f, dbToGain (presDb));
        }

        // Helper removed — shape amount now pre-computed into `shapeBuf`.

        double sampleRate { 44100.0 };
        int    channels   { 2 };

        juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                       juce::dsp::IIR::Coefficients<float>>
            hpf1, hpf2, lowMidDip, presence, lowBand;

        juce::LinearSmoothedValue<float> shapeAmtSm;

        std::array<TransientShaper, 8> shapers;

        juce::AudioBuffer<float> scratchDet;
        std::vector<float>       shapeBuf;

        float lowEnv { 0.0f };
        float lowEnvCoef { 0.99f };

        // Block-rate smoothed filter targets (eliminates coefficient-jump clicks)
        float smHpfHz  { 60.0f };
        float smDipDb  {  0.0f };
        float smPresDb {  0.0f };
        float smCoef   {  0.0f };
    };
}
