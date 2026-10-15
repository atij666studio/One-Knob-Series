#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>
#include <cmath>

namespace nrm
{
    // Room — a short ambience that stays out of the way of dense mixes.
    // Placed AFTER the amp / cab.
    //
    //   1. Mono feed of the input -> pre-delay -> 4 series all-pass
    //      diffusers (smears the attack into a dense early field).
    //   2. 8-line feedback delay network, Hadamard mixing, per-line decay
    //      set from the Space's RT60, one-pole damping in each loop, and a
    //      slow, tiny modulation on two lines so it never rings metallic.
    //   3. Two decorrelated outputs (different sign patterns) -> wet EQ:
    //      high-pass 180 Hz (the low end stays dry and tight), low-pass 8 kHz.
    //   4. Ducking: the wet level dips while you play (up to 10 dB with DUCK
    //      at 100 %) and blooms back in the gaps, so the room fills the
    //      spaces instead of blurring the notes.
    //   5. Dry passes at unity; wet is added on top by ROOM.
    //
    // Spaces: Booth (tight, 0.35 s), Studio (0.7 s), Hall (1.6 s). ROOM also
    // nudges size and decay up as it rises. Zero latency (the dry path is
    // never delayed).
    class RoomDSP
    {
    public:
        enum Space { Booth = 0, Studio = 1, Hall = 2 };

        std::atomic<float> inputDb  { -90.0f };
        std::atomic<float> roomDb   { -90.0f };   // wet level
        std::atomic<float> duckDb   {   0.0f };   // current ducking, positive dB

        void prepare (double sr, int /*blockSize*/, int /*numChannels*/)
        {
            sampleRate = sr;
            auto alloc = [&] (Line& l, double maxSec)
            {
                l.buf.assign ((size_t) juce::nextPowerOfTwo ((int) (sr * maxSec) + 8), 0.0f);
                l.mask = (int) l.buf.size() - 1; l.pos = 0; l.lp = 0.0f;
            };
            alloc (pre, 0.05);
            for (auto& d : diff) alloc (d, 0.02);
            for (auto& l : fdn)  alloc (l, 0.12);

            using AC = juce::dsp::IIR::ArrayCoefficients<float>;
            for (int c = 0; c < 2; ++c)
            {
                wetHP[c].reset(); wetLP[c].reset();
                *wetHP[c].coefficients = AC::makeHighPass (sr, 180.0f, 0.6f);
                *wetLP[c].coefficients = AC::makeLowPass  (sr, juce::jmin (8000.0f, (float) sr * 0.4f), 0.6f);
            }

            lfoPhase = 0.0;
            duckEnv = 0.0f; duckSm = 0.0f;
            duckAtk = coef (0.005); duckRel = coef (0.090);
            wetSm.reset (sr, 0.05);  wetSm.setCurrentAndTargetValue (0.0f);
            outSm.reset (sr, 0.02);  outSm.setCurrentAndTargetValue (1.0f);
            sizeSm = -1.0f;
            envIn = envWet = 0.0f;
        }

        void reset()
        {
            for (auto* l : { &pre }) std::fill (l->buf.begin(), l->buf.end(), 0.0f);
            for (auto& l : diff) std::fill (l.buf.begin(), l.buf.end(), 0.0f);
            for (auto& l : fdn)  { std::fill (l.buf.begin(), l.buf.end(), 0.0f); l.lp = 0.0f; }
        }

        void process (juce::AudioBuffer<float>& buffer, float room, int spaceIn, float duck, float outDb)
        {
            const int n = buffer.getNumSamples();
            if (n <= 0) return;
            const bool stereo = buffer.getNumChannels() >= 2;
            float* L = buffer.getWritePointer (0);
            float* R = stereo ? buffer.getWritePointer (1) : nullptr;

            room = juce::jlimit (0.0f, 1.0f, room);
            const Space s = (Space) juce::jlimit (0, 2, spaceIn);

            float size = 1.0f, rt60 = 0.7f, preMs = 12.0f, dampHz = 5200.0f;
            switch (s)
            {
                case Booth: size = 0.55f; rt60 = 0.35f; preMs = 4.0f;  dampHz = 6500.0f; break;
                case Hall:  size = 1.7f;  rt60 = 1.6f;  preMs = 24.0f; dampHz = 4200.0f; break;
                default: break;
            }
            size *= 0.85f + 0.3f * room;
            rt60 *= 0.8f + 0.4f * room;

            // Size changes glide (block rate) so moving ROOM never clicks.
            sizeSm = sizeSm < 0.0f ? size : sizeSm + (size - sizeSm) * 0.05f;
            float lens[8], gains[8];
            for (int k = 0; k < 8; ++k)
            {
                lens[k]  = (float) (sampleRate * 0.001 * kBaseMs[k] * sizeSm);
                gains[k] = std::pow (10.0f, -3.0f * lens[k] / (rt60 * (float) sampleRate));
            }
            const float dampA = (float) std::exp (-juce::MathConstants<double>::twoPi * dampHz / sampleRate);
            const float preLen = (float) (sampleRate * 0.001 * preMs);
            float diffLen[4];
            for (int k = 0; k < 4; ++k) diffLen[k] = (float) (sampleRate * 0.001 * kDiffMs[k] * juce::jmin (1.2f, sizeSm));

            // Wet level: ROOM^1.5 so the first half of the knob stays subtle.
            // DUCK lowers the wet under playing; give some of it back
            // overall so the gaps bloom at full size.
            wetSm.setTargetValue (0.9f * std::pow (room, 1.5f) * juce::Decibels::decibelsToGain (4.0f * juce::jlimit (0.0f, 1.0f, duck)));
            outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));
            const float duckMaxDb = 10.0f * juce::jlimit (0.0f, 1.0f, duck);
            const double lfoInc = juce::MathConstants<double>::twoPi * 0.37 / sampleRate;
            const float modDepth = (float) (sampleRate * 0.0004);

            float pkIn = 0.0f, pkWet = 0.0f, duckMax = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float l = L[i], r = stereo ? R[i] : L[i];
                const float x = 0.5f * (l + r);
                const float ax = juce::jmax (std::abs (l), std::abs (r));

                // --- ducking envelope (dry level) ----------------------
                duckEnv += (ax - duckEnv) * (ax > duckEnv ? duckAtk : duckRel);
                const float envDb = juce::Decibels::gainToDecibels (duckEnv, -120.0f);
                const float target = -duckMaxDb * juce::jlimit (0.0f, 1.0f, (envDb + 40.0f) / 30.0f);
                duckSm += (target - duckSm) * 0.002f;

                // --- pre-delay + diffusion ----------------------------
                float v = pre.readWrite (x, preLen);
                for (int k = 0; k < 4; ++k)
                {
                    const float dly = diff[k].read (diffLen[k]);
                    const float in  = v + kDiffG * dly;
                    diff[k].write (in);
                    v = dly - kDiffG * in;
                }

                // --- FDN ------------------------------------------------
                lfoPhase += lfoInc; if (lfoPhase > juce::MathConstants<double>::twoPi) lfoPhase -= juce::MathConstants<double>::twoPi;
                const float mod = (float) std::sin (lfoPhase) * modDepth;
                float o[8];
                for (int k = 0; k < 8; ++k)
                {
                    const float len = lens[k] + (k == 1 ? mod : k == 6 ? -mod : 0.0f);
                    float y = fdn[k].read (len) * gains[k];
                    fdn[k].lp = y + dampA * (fdn[k].lp - y);       // damping
                    o[k] = fdn[k].lp;
                }
                // Hadamard 8x8 (normalised)
                float h[8];
                hadamard8 (o, h);
                for (int k = 0; k < 8; ++k)
                    fdn[k].write (h[k] + v * kInGain[k]);

                float wl = (o[0] - o[1] + o[2] - o[3] + o[4] - o[5] + o[6] - o[7]) * 0.35f;
                float wr = (o[0] + o[1] - o[2] - o[3] + o[4] + o[5] - o[6] - o[7]) * 0.35f;
                wl = wetLP[0].processSample (wetHP[0].processSample (wl));
                wr = wetLP[1].processSample (wetHP[1].processSample (wr));

                const float wg = wetSm.getNextValue() * juce::Decibels::decibelsToGain (duckSm);
                const float og = outSm.getNextValue();
                L[i] = (l + wl * wg) * og;
                if (stereo) R[i] = (r + wr * wg) * og;

                pkIn    = juce::jmax (pkIn, ax);
                pkWet   = juce::jmax (pkWet, juce::jmax (std::abs (wl * wg), std::abs (wr * wg)));
                duckMax = juce::jmin (duckMax, duckSm);
            }

            const float k = (float) (1.0 - std::exp (-(double) n / (0.08 * sampleRate)));
            envIn  += (pkIn  - envIn)  * k;
            envWet += (pkWet - envWet) * k;
            inputDb.store (juce::Decibels::gainToDecibels (envIn,  -90.0f), std::memory_order_relaxed);
            roomDb.store  (juce::Decibels::gainToDecibels (envWet, -90.0f), std::memory_order_relaxed);
            duckDb.store  (-duckMax, std::memory_order_relaxed);
        }

    private:
        struct Line
        {
            std::vector<float> buf;
            int mask { 0 }, pos { 0 };
            float lp { 0.0f };

            float read (float delay) const
            {
                const float rp = (float) pos - delay;
                const int   i0 = (int) std::floor (rp);
                const float fr = rp - (float) i0;
                const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
                return a + (b - a) * fr;
            }
            void write (float x) { buf[(size_t) pos] = x; pos = (pos + 1) & mask; }
            float readWrite (float x, float delay) { const float y = read (delay); write (x); return y; }
        };

        static void hadamard8 (const float* in, float* out)
        {
            float a[8];
            for (int k = 0; k < 8; ++k) a[k] = in[k];
            for (int len = 1; len < 8; len <<= 1)
                for (int i = 0; i < 8; i += len << 1)
                    for (int j = i; j < i + len; ++j)
                    {
                        const float u = a[j], w = a[j + len];
                        a[j] = u + w; a[j + len] = u - w;
                    }
            constexpr float norm = 0.35355339f;   // 1/sqrt(8)
            for (int k = 0; k < 8; ++k) out[k] = a[k] * norm;
        }

        float coef (double seconds) const { return (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate))); }

        static constexpr float kBaseMs[8] { 17.3f, 21.1f, 25.7f, 29.3f, 33.1f, 37.9f, 41.3f, 47.9f };
        static constexpr float kDiffMs[4] { 3.1f, 4.7f, 7.3f, 11.3f };
        static constexpr float kDiffG     = 0.62f;
        static constexpr float kInGain[8] { 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, -0.5f, 0.5f, -0.5f };

        double sampleRate { 48000.0 };
        Line pre;
        std::array<Line, 4> diff;
        std::array<Line, 8> fdn;
        std::array<juce::dsp::IIR::Filter<float>, 2> wetHP, wetLP;
        double lfoPhase { 0.0 };
        float duckEnv { 0 }, duckSm { 0 }, duckAtk { 0 }, duckRel { 0 };
        float sizeSm { -1.0f };
        juce::SmoothedValue<float> wetSm, outSm;
        float envIn { 0 }, envWet { 0 };
    };
}
