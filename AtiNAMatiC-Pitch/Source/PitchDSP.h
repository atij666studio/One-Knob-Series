#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>
#include <cmath>

namespace npi
{
    // Granular delay-line pitch shifter: two read taps sweep through a short
    // window at the rate the pitch ratio needs, each faded with a Hann
    // window (a tap is silent at the moment it jumps back -- no clicks).
    //
    // Pitch-synchronous: the caller offers a window that is an even number
    // of the note periods, so a restart lands in phase with the waveform and
    // the two grains add up instead of partly cancelling (with a fixed window
    // the octave-down voice of a steady note mostly turns into sidebands).
    struct Shifter
    {
        std::vector<float> buf;
        int mask { 0 }, pos { 0 };
        float phase { 0.0f };
        float win { 2048.0f };

        void prepare (double sr)
        {
            buf.assign ((size_t) juce::nextPowerOfTwo ((int) (sr * 0.25) + 8), 0.0f);
            mask = (int) buf.size() - 1; pos = 0; phase = 0.0f;
            win = (float) (sr * 0.04);
        }

        inline float read (float delay) const
        {
            const float rp = (float) pos - delay;
            const int   i1 = (int) std::floor (rp);
            const float t  = rp - (float) i1;
            const float y0 = buf[(size_t) ((i1 - 1) & mask)], y1 = buf[(size_t) (i1 & mask)];
            const float y2 = buf[(size_t) ((i1 + 1) & mask)], y3 = buf[(size_t) ((i1 + 2) & mask)];
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * t + c2) * t + c1) * t + y1;
        }

        // ratio = output pitch / input pitch; window in samples (latched
        // whenever either tap restarts); minDelay in samples. One shared
        // phase keeps the taps exactly half a cycle apart, so the two Hann
        // windows always sum to 1. When the offered window is an even number
        // of periods, half of it is a whole number of periods, so the tap
        // that keeps playing through a latch jumps by whole periods: seamless.
        inline float process (float x, float ratio, float window, float minDelay)
        {
            buf[(size_t) pos] = x;
            const float before = phase;
            phase += (1.0f - ratio) / win;
            const bool wrapped = phase >= 1.0f || phase < 0.0f;
            phase -= std::floor (phase);
            const bool crossedHalf = (before < 0.5f) != (phase < 0.5f) && ! wrapped;
            if (wrapped || crossedHalf) win = window;

            const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
            const float w1 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
            const float y  = read (minDelay + phase * win) * w1 + read (minDelay + p2 * win) * (1.0f - w1);
            pos = (pos + 1) & mask;
            return y;
        }
    };

    // Light pitch tracker: normalised autocorrelation on a 4x-decimated mono
    // signal, 50-400 Hz, every 512 samples. Returns the period in samples
    // (at the full rate) or 0 when there is no clear single pitch.
    struct PeriodTracker
    {
        static constexpr int kLen = 512;          // analysis length (decimated)
        std::array<float, 2048> ring {};
        int wpos { 0 }, decim { 0 }, count { 0 };
        float acc { 0.0f }, period { 0.0f };
        double sr { 48000.0 };
        int lagMin { 30 }, lagMax { 240 };

        void prepare (double sampleRate)
        {
            sr = sampleRate; ring.fill (0.0f); wpos = decim = count = 0; acc = 0; period = 0;
            lagMin = juce::jmax (4, (int) (sr / 4.0 / 400.0));
            lagMax = juce::jmin (kLen - 1, (int) (sr / 4.0 / 50.0));
        }

        float push (float x)
        {
            acc += x;
            if (++decim < 4) return period;
            decim = 0;
            ring[(size_t) wpos] = acc * 0.25f; acc = 0.0f;
            wpos = (wpos + 1) & 2047;
            if (++count < 128) return period;
            count = 0;

            auto at = [this] (int back) { return ring[(size_t) ((wpos - 1 - back) & 2047)]; };
            float e0 = 0.0f;
            for (int i = 0; i < kLen; ++i) e0 += at (i) * at (i);
            if (e0 < 1.0e-6f) { period = 0.0f; return period; }
            auto corr = [&] (int lag)
            {
                float c = 0.0f, e1 = 0.0f;
                for (int i = 0; i < kLen - lag; ++i) { c += at (i) * at (i + lag); e1 += at (i + lag) * at (i + lag); }
                return c / std::sqrt (e0 * e1 + 1.0e-12f);
            };
            float best = 0.0f; int bestLag = 0;
            for (int lag = lagMin; lag <= lagMax; ++lag)
            {
                const float r = corr (lag);
                if (r > best) { best = r; bestLag = lag; }
            }
            if (best <= 0.8f) { period = 0.0f; return period; }
            // Sub-sample refinement (parabola through the peak): the 4x
            // decimation alone would quantise the period to 4 samples, enough
            // to drift the grain phase on high notes.
            float frac = 0.0f;
            if (bestLag > lagMin && bestLag < lagMax)
            {
                const float rm = corr (bestLag - 1), rp = corr (bestLag + 1);
                const float den = rm - 2.0f * best + rp;
                if (std::abs (den) > 1.0e-9f) frac = juce::jlimit (-0.5f, 0.5f, 0.5f * (rm - rp) / den);
            }
            period = ((float) bestLag + frac) * 4.0f;
            return period;
        }
    };

    // Window that is an even number of periods close to the target, or the
    // target itself when there is no clear pitch.
    inline float syncWindow (float target, float period)
    {
        if (period <= 0.0f) return target;
        const float k = juce::jmax (2.0f, 2.0f * std::round (target / period * 0.5f));
        return k * period;
    }

    // Pitch — drop tuning, detune, octave.
    //
    //   Drop   : PITCH 0..100 % -> 0..-7 semitones, in whole semitones (so a
    //            setting is always in tune). 100 % wet unless MIX is lowered.
    //   Detune : PITCH -> 0..25 cents; the left voice goes up and the right
    //            down (plus 7 / 11 ms offsets), blended with the dry centre:
    //            a stereo "micro-shift" thickener.
    //   Octave : PITCH -> level of an octave-below voice (low-passed so it
    //            stays smooth), added under the dry signal.
    //
    // Honest limits: like any real-time shifter, chords at large shifts
    // warble a little; single notes and small shifts are cleanest. The grain
    // window adds ~20-30 ms of delay in Drop and Octave.
    class PitchDSP
    {
    public:
        enum Mode { Drop = 0, Detune = 1, Octave = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> outputDb { -90.0f };
        std::atomic<float> shiftSt  {   0.0f };   // current shift in semitones (Drop / Octave) or cents/100 (Detune)

        void prepare (double sr, int /*blockSize*/, int /*numChannels*/)
        {
            sampleRate = sr;
            for (auto& s : sh) s.prepare (sr);
            tracker.prepare (sr); period = 0.0f;
            octLP[0].reset(); octLP[1].reset();
            const auto lp = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (sr, 1500.0f, 0.7f);
            *octLP[0].coefficients = lp; *octLP[1].coefficients = lp;
            ratioSm = 1.0f; detSm = 0.0f; octSm = 0.0f;
            mixSm.reset (sr, 0.03); mixSm.setCurrentAndTargetValue (1.0f);
            outSm.reset (sr, 0.02); outSm.setCurrentAndTargetValue (1.0f);
            envIn = envOut = 0.0f;
        }

        void process (juce::AudioBuffer<float>& buffer, float amount, int modeIn, float mix, float outDb)
        {
            const int n = buffer.getNumSamples();
            if (n <= 0) return;
            const bool stereo = buffer.getNumChannels() >= 2;
            float* L = buffer.getWritePointer (0);
            float* R = stereo ? buffer.getWritePointer (1) : nullptr;
            amount = juce::jlimit (0.0f, 1.0f, amount);
            const Mode m = (Mode) juce::jlimit (0, 2, modeIn);

            const float semis    = -std::round (amount * 7.0f);                 // Drop
            const float cents    = amount * 25.0f;                              // Detune
            const float targetR  = std::pow (2.0f, semis / 12.0f);
            const float sr       = (float) sampleRate;

            mixSm.setTargetValue (mix);
            outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));

            float pkIn = 0.0f, pkOut = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float l = L[i], r = stereo ? R[i] : L[i];
                period = tracker.push (0.5f * (l + r));
                const float mx = mixSm.getNextValue();
                const float og = outSm.getNextValue();
                float yl = l, yr = r;

                switch (m)
                {
                    case Drop:
                    {
                        ratioSm += (targetR - ratioSm) * 0.0005f;          // glide between semitones
                        const float w45 = syncWindow (0.045f * sr, period);
                        const float wl = sh[0].process (l, ratioSm, w45, 2.0f);
                        const float wr = stereo ? sh[1].process (r, ratioSm, w45, 2.0f) : wl;
                        // At (or gliding into) 0 st the taps stand still and would
                        // comb; fade the shifted signal out there.
                        const float w = mx * juce::jmin (1.0f, std::abs (1.0f - ratioSm) * 40.0f);
                        yl = wl * w + l * (1.0f - w);
                        yr = wr * w + r * (1.0f - w);
                        break;
                    }
                    case Detune:
                    {
                        detSm += (cents - detSm) * 0.0005f;
                        const float up   = std::pow (2.0f,  detSm / 1200.0f);
                        const float down = std::pow (2.0f, -detSm / 1200.0f);
                        const float mono = 0.5f * (l + r);
                        const float vl = sh[0].process (mono, up,   0.025f * sr, 0.007f * sr);
                        const float vr = sh[1].process (mono, down, 0.025f * sr, 0.011f * sr);
                        const float wet = juce::jmin (1.0f, detSm / 6.0f) * 0.7f;   // fades in from 0 cents
                        yl = l + (vl * wet) * mx;
                        yr = r + (vr * wet) * mx;
                        const float norm = 1.0f / (1.0f + 0.35f * wet * mx);
                        yl *= norm; yr *= norm;
                        break;
                    }
                    case Octave:
                    default:
                    {
                        octSm += (amount - octSm) * 0.0005f;
                        const float mono = 0.5f * (l + r);
                        float o = sh[0].process (mono, 0.5f, syncWindow (0.060f * sr, period), 2.0f);
                        o = octLP[1].processSample (octLP[0].processSample (o));
                        const float lvl = octSm * 1.2f * mx;
                        yl = l + o * lvl;
                        yr = r + o * lvl;
                        break;
                    }
                }

                yl *= og; yr *= og;
                L[i] = yl;
                if (stereo) R[i] = yr;
                pkIn  = juce::jmax (pkIn,  juce::jmax (std::abs (l), std::abs (r)));
                pkOut = juce::jmax (pkOut, juce::jmax (std::abs (yl), std::abs (yr)));
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.06 * sampleRate)));
            envIn  += (pkIn  - envIn)  * k;
            envOut += (pkOut - envOut) * k;
            inputDb.store  (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            outputDb.store (juce::Decibels::gainToDecibels (envOut, -90.0f), std::memory_order_relaxed);
            shiftSt.store  (m == Drop ? semis : m == Detune ? cents : -12.0f * (amount > 0.0f ? 1.0f : 0.0f),
                            std::memory_order_relaxed);
        }

    private:
        double sampleRate { 48000.0 };
        std::array<Shifter, 2> sh;
        PeriodTracker tracker;
        float period { 0.0f };
        std::array<juce::dsp::IIR::Filter<float>, 2> octLP;
        float ratioSm { 1.0f }, detSm { 0.0f }, octSm { 0.0f };
        juce::SmoothedValue<float> mixSm, outSm;
        float envIn { 0 }, envOut { 0 };
    };
}
