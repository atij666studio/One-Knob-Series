#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace nt
{
    enum class Mode { Classic = 0, Modern = 1, Extreme = 2 };

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

    // Stereo-linked transient shaper using fast / slow envelopes.
    //
    // Both envelopes share the same release, so on a sustained note they
    // decay together between waveform peaks and their difference stays ~0
    // (separate releases made the "transient" signal ripple at the note's
    // own frequency, which modulated the gain and fuzzed up low notes).
    // Only the attack time differs: fast ~1 ms, slow ~30 ms, so a pick
    // attack opens a gap for a few tens of ms and nothing else does.
    struct TransientShaper
    {
        float fast { 0.0f }, slow { 0.0f }, gainDb { 0.0f };
        float aFast { 0.0f }, aSlow { 0.0f }, rel { 0.0f }, gSm { 0.0f };

        void prepare (double sr)
        {
            aFast = 1.0f - std::exp (-1.0f / float (0.001 * sr));
            aSlow = 1.0f - std::exp (-1.0f / float (0.030 * sr));
            rel   = 1.0f - std::exp (-1.0f / float (0.120 * sr));
            gSm   = 1.0f - std::exp (-1.0f / float (0.0007 * sr));
            reset();
        }

        void reset() { fast = slow = gainDb = 0.0f; }

        // detector: linked level (max |x| across channels).
        // amount in [0..1+]: 0 => unity, 1 => up to ~+6 dB on attacks with
        // a gentle -1.5 dB on the sustain.
        inline float processSample (float detector, float amount)
        {
            fast += (detector - fast) * (detector > fast ? aFast : rel);
            slow += (detector - slow) * (detector > slow ? aSlow : rel);

            const float norm   = juce::jmax (0.0f, fast - slow) / juce::jmax (slow, 1.0e-5f);
            const float attack = norm / (1.0f + norm);            // soft 0..1
            const float steady = 1.0f - juce::jmin (1.0f, norm * 2.0f);

            const float target = amount * (6.0f * attack - 1.5f * steady);
            gainDb += (target - gainDb) * gSm;
            return dbToGain (gainDb);
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

            shaper.prepare (sr);

            shapeAmtSm.reset (sr, 0.030);
            shapeAmtSm.setCurrentAndTargetValue (0.0f);

            // Sized once here to the negotiated block size; process() clamps to
            // this capacity instead of resizing, so there's never an allocation
            // on the audio thread even if a host violates the block-size contract.
            maxBlockSize = juce::jmax (1, blockSize);
            scratchDet.setSize (1, maxBlockSize, false, false, true);

            lowEnv = 0.0f;
            lowEnvCoef = std::exp (-1.0f / float (0.080 * sr));
        }

        void reset()
        {
            hpf1.reset(); hpf2.reset();
            lowMidDip.reset(); presence.reset();
            lowBand.reset();
            shaper.reset();
            lowEnv = 0.0f;
        }

        // In-place tightening: HPF → low-mid dip → presence shelf → transient shaper.
        // Updates `lowEnergyDb` for the meter from the *input* signal.
        void process (juce::AudioBuffer<float>& buffer)
        {
            // A host that sends more than it negotiated gets processed in
            // chunks of the reserved size -- no allocation, no dry tail.
            const int total = buffer.getNumSamples();
            for (int pos = 0; pos < total; pos += maxBlockSize)
            {
                const int len = juce::jmin (maxBlockSize, total - pos);
                juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), pos, len);
                processChunk (view);
            }
        }

    private:
        void processChunk (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = juce::jmin (buffer.getNumChannels(), channels);
            const int n     = buffer.getNumSamples();
            if (numCh <= 0 || n <= 0 || n > maxBlockSize) return;

            const float t = juce::jlimit (0.0f, 1.0f, tight.load());
            const Mode  m = (Mode) juce::jlimit (0, 2, modeIx.load());

            float hpfHz, dipDb, presenceDb, shapeAmt;
            mapKnob (t, m, hpfHz, dipDb, presenceDb, shapeAmt);

            // Smooth filter params at block rate — no zipper noise when sweeping TIGHT
            smHpfHz  = smCoef * smHpfHz  + (1.0f - smCoef) * hpfHz;
            smDipDb  = smCoef * smDipDb  + (1.0f - smCoef) * dipDb;
            smPresDb = smCoef * smPresDb + (1.0f - smCoef) * presenceDb;
            updateFilters (smHpfHz, smDipDb, smPresDb);
            shapeAmtSm.setTargetValue (shapeAmt);

            // ---- Low-energy meter ----
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

            // ---- Transient shaping (one linked detector keeps the image steady) ----
            for (int i = 0; i < n; ++i)
            {
                float det = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                    det = juce::jmax (det, std::abs (buffer.getReadPointer (ch)[i]));
                const float g = shaper.processSample (det, shapeAmtSm.getNextValue());
                for (int ch = 0; ch < numCh; ++ch)
                    buffer.getWritePointer (ch)[i] *= g;
            }
        }

        // Maps the master TIGHT knob through the Mode curve to per-stage params.
        static void mapKnob (float t, Mode m,
                             float& hpfHz, float& dipDb, float& presenceDb,
                             float& shapeAmt)
        {
            // Base curves (Modern):
            //  HPF:      60 Hz @ 0   ..  150 Hz @ 1
            //  Dip:       0 dB @ 0   .. -5.5 dB @ 1   centered around 280 Hz (Q ~1.0)
            //  Presence: +0 dB @ 0   .. +3.0 dB @ 1   high-shelf @ 2.5 kHz
            //  Shaper:    0   @ 0   ..   0.85 @ 1
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

        double sampleRate { 44100.0 };
        int    channels   { 2 };
        int    maxBlockSize { 0 };

        juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                       juce::dsp::IIR::Coefficients<float>>
            hpf1, hpf2, lowMidDip, presence, lowBand;

        juce::LinearSmoothedValue<float> shapeAmtSm;

        TransientShaper shaper;

        juce::AudioBuffer<float> scratchDet;

        float lowEnv { 0.0f };
        float lowEnvCoef { 0.99f };

        // Block-rate smoothed filter targets (eliminates coefficient-jump clicks)
        float smHpfHz  { 60.0f };
        float smDipDb  {  0.0f };
        float smPresDb {  0.0f };
        float smCoef   {  0.0f };
    };
}
