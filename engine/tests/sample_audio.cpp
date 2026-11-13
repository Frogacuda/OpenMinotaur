#include "neon/sample_audio.h"
#include "neon/resource_catalog.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
using namespace neon::audio;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + ":" + std::to_string(__LINE__));            \
    } while (false)
Sample sample(int16_t value, int instances = 1, int priority = 1) {
    Sample s;
    s.pcm.assign(16, value);
    s.maxInstances = instances;
    s.priority = priority;
    return s;
}
void allocation() {
    for (std::size_t cap : {1, 16, 32, 48}) {
        NativeSampleMixer mixer({cap});
        std::vector<Sample> samples;
        for (std::size_t i = 0; i <= cap; ++i)
            samples.push_back(sample(100, 1, i == cap ? 2 : 5));
        mixer.setSamples(samples);
        for (std::size_t i = 0; i < cap; ++i)
            mixer.play(int(i), 4096, 0);
        CHECK(mixer.voices().size() == cap);
        mixer.play(int(cap), 8192, 1);
        CHECK(mixer.voices()[0].sample == int(cap) && mixer.samples()[0].active == 0 &&
              mixer.samples()[cap].active == 1);
        CHECK(mixer.voices()[0].volume == 8192 && mixer.voices()[0].loop == 1);
        mixer.stopAll();
        for (auto &v : mixer.voices())
            CHECK(v.sample == -1);
        for (auto &s : mixer.samples())
            CHECK(s.active == 0);
    }
    NativeSampleMixer mixer({2});
    mixer.setSamples({sample(1, 1, 1), sample(1, 1, 9), sample(1, 1, 2)});
    mixer.play(0, 4096, 0);
    mixer.play(1, 4096, 0);
    mixer.play(2, 4096, 0);
    CHECK(mixer.voices()[0].sample == 0 &&
          mixer.voices()[1].sample == 1); // do not search past first nonnegative slot
    auto negative = sample(1, 1, -1);
    mixer.setSamples({negative, sample(1, 1, 9), sample(1, 1, 2)});
    mixer.play(0, 1, 0);
    mixer.play(1, 1, 0);
    mixer.play(2, 2, 0);
    CHECK(mixer.voices()[0].sample == 0 && mixer.voices()[1].sample == 2);
    mixer.setSamples({negative, negative, sample(1, 1, 2)});
    mixer.play(0, 1, 0);
    mixer.play(1, 1, 0);
    mixer.play(2, 1, 0);
    CHECK(mixer.voices()[0].sample == 0 &&
          mixer.voices()[1].sample == 1); // bounded rejection of unsafe source index
    mixer.play(-1, 0, 0);
    mixer.play(99, 0, 0);
    mixer.stop(-1);
    mixer.stop(99);
}
void retriggerAndLoops() {
    for (auto rule : {RetriggerRule::RewindFirst, RetriggerRule::RequireSampleRestart})
        for (bool restart : {false, true}) {
            auto s = sample(123);
            s.restart = restart;
            NativeSampleMixer mixer({16, MixArithmetic::ReverseQ16Saturating, rule});
            mixer.setSamples({s});
            mixer.play(0, 8192, 1000);
            int16_t out[3];
            mixer.mixMono(out, 3);
            mixer.play(0, 16384, 0);
            const auto &v = mixer.voices()[0];
            CHECK(v.position == (rule == RetriggerRule::RewindFirst || restart ? 0u : 3u));
            CHECK(v.volume == 8192 && v.loop == 1000 && mixer.samples()[0].active == 1);
        }
    for (auto arithmetic :
         {MixArithmetic::ReverseQ16Saturating, MixArithmetic::ForwardFloatToInteger}) {
        for (int loop : {0, 1, 1000, -1}) {
            auto s = sample(32767);
            s.pcm.resize(8);
            NativeSampleMixer mixer({2, arithmetic});
            mixer.setSamples({s});
            mixer.play(0, 16384, loop);
            int16_t out[8];
            mixer.mixMono(out, 8);
            CHECK(mixer.samples()[0].active ==
                  (loop != 0 || arithmetic == MixArithmetic::ForwardFloatToInteger ? 1 : 0));
            int16_t more[80];
            mixer.mixMono(more, 80);
            if (loop) {
                CHECK(mixer.samples()[0].active == 1 && mixer.voices()[0].loop == loop);
                CHECK(std::all_of(more, more + 80, [](int16_t n) { return n > 0; }));
            } else {
                CHECK(mixer.samples()[0].active == 0 &&
                      std::all_of(more, more + 80, [](int16_t n) { return n == 0; }));
            }
            mixer.stop(0);
            mixer.mixMono(out, 8);
            CHECK(std::all_of(out, out + 8, [](int16_t n) { return n == 0; }));
        }
    }
    // stopSample removes only the FIRST matching slot and resets the sample
    // counter, even with multiple instances. Preserve that source quirk.
    NativeSampleMixer m;
    auto s = sample(1, 3);
    m.setSamples({s});
    m.play(0, 1, 0);
    m.play(0, 1, 0);
    m.play(0, 1, 0);
    m.stop(0);
    CHECK(m.voices()[0].sample == -1 && m.voices()[1].sample == 0 && m.samples()[0].active == 0);
    auto grouped = sample(1);
    grouped.group = 1;
    m.setSamples({s, grouped});
    m.play(0, 1, 0);
    m.play(1, 1, 0);
    m.setGroup(1);
    CHECK(m.voices()[0].sample == -1 && m.voices()[1].sample == 1);
    m.play(0, 1, 0);
    CHECK(m.samples()[0].active == 0);
}
void arithmetic() {
    // Distinguish ARM SMULW arithmetic shift from truncation/float gain.
    NativeSampleMixer early({32});
    early.setSamples({sample(-1)});
    early.play(0, 4096, 0);
    int16_t x;
    early.mixMono(&x, 1);
    CHECK(x == -1);
    early.setSamples({sample(-30000), sample(30000), sample(30000)});
    for (int n = 0; n < 3; ++n)
        early.play(n, 65536, 0);
    early.mixMono(&x, 1);
    CHECK(x == 2767); // reverse slots, saturate EACH addition; a final clamp yields 30000
    NativeSampleMixer late({32, MixArithmetic::ForwardFloatToInteger});
    late.setSamples({sample(-1)});
    late.play(0, 4096, 0);
    late.mixMono(&x, 1);
    CHECK(x == 0);
    late.setSamples({sample(-30000), sample(30000), sample(30000)});
    for (int n = 0; n < 3; ++n)
        late.play(n, 32768, 0);
    late.mixMono(&x, 1);
    CHECK(x == 30000);
    late.setSamples({sample(-32768)});
    late.play(0, 32768, 0);
    late.mixMono(&x, 1);
    CHECK(x == -32767);
    early.setSamples({sample(16384)});
    early.play(0, 16384, 0);
    early.mixMono(&x, 1);
    CHECK(x == 4096);
    late.setSamples({sample(16384)});
    late.play(0, 16384, 0);
    late.mixMono(&x, 1);
    CHECK(x == 8192); // original later gain, not legacy port's half gain
    // Test opposite-signed contribution truncation after adding to an integer
    // accumulator: truncating each contribution independently gives 100 here.
    late.setSamples({sample(100), sample(-1)});
    late.play(0, 32768, 0);
    late.play(1, 4096, 0);
    late.mixMono(&x, 1);
    CHECK(x == 99);
}
void arithmeticFixture(const char *path) {
    std::ifstream file(path);
    CHECK(file.good());
    std::string profile;
    int count = 0;
    while (file >> profile) {
        int volume[3], value[3], expected;
        for (int i = 0; i < 3; ++i)
            CHECK(bool(file >> volume[i] >> value[i]));
        CHECK(bool(file >> expected));
        NativeSampleMixer mixer({3, profile == "early" ? MixArithmetic::ReverseQ16Saturating
                                                       : MixArithmetic::ForwardFloatToInteger});
        mixer.setSamples(
            {sample(int16_t(value[0])), sample(int16_t(value[1])), sample(int16_t(value[2]))});
        for (int i = 0; i < 3; ++i)
            mixer.play(i, volume[i], 0);
        int16_t actual;
        mixer.mixMono(&actual, 1);
        CHECK(actual == expected);
        ++count;
    }
    CHECK(count == 4096);
    std::cout << count << " independent instruction-derived arithmetic vectors passed\n";
}
void hostStream() {
    {
        const MixerRules rules{32, MixArithmetic::ForwardFloatToInteger};
        SampleMixer original(rules, 44100), normalized(rules, 44100, .5f);
        auto s = sample(32767);
        original.setSamples({s});
        normalized.setSamples({s});
        original.play(0, 28672, 1);
        normalized.play(0, 28672, 1);
        float full[128], half[128];
        original.mix(full, 64);
        normalized.mix(half, 64);
        for (int i = 0; i < 128; ++i)
            CHECK(half[i] == full[i] * .5f); // deliberate host gain, native arithmetic unchanged
    }
    for (int rate : {11025, 44100, 48000}) {
        SampleMixer whole({}, rate), split({}, rate);
        auto s = sample(0);
        for (int i = 0; i < 16; ++i)
            s.pcm[i] = int16_t((i - 8) * 3000);
        whole.setSamples({s});
        split.setSamples({s});
        whole.play(0, 28672, 1);
        split.play(0, 28672, 1);
        std::vector<float> a(514), b(514);
        whole.mix(a.data(), 257);
        int offset = 0;
        for (int count : {1, 3, 17, 29, 100, 107}) {
            split.mix(b.data() + offset * 2, count);
            offset += count;
        }
        CHECK(a == b);
        for (int i = 0; i < 257; ++i)
            CHECK(a[2 * i] == a[2 * i + 1] && std::isfinite(a[2 * i]) && std::fabs(a[2 * i]) <= 1);
        whole.stopAll();
        whole.mix(a.data(), 257);
        for (float v : a)
            CHECK(v == 0);
    }
    // Native and reconstructed host stream agree at each native sample time,
    // allowing the documented one-native-frame causal reconstruction delay.
    NativeSampleMixer native;
    SampleMixer host;
    auto s = sample(8000);
    native.setSamples({s});
    host.setSamples({s});
    native.play(0, 32768, 1);
    host.play(0, 32768, 1);
    int16_t mono[16];
    float stereo[128];
    native.mixMono(mono, 16);
    host.mix(stereo, 64);
    for (int i = 1; i < 16; ++i)
        CHECK(stereo[8 * i] == float(mono[i - 1]) / 32768.f);
    host.stop(0);
    host.mix(stereo, 64);
    for (int i = 8; i < 128; ++i)
        CHECK(stereo[i] == 0); // at most one native reconstruction frame remains
}
void cleanPlayback() {
    const MixerRules rules{32, MixArithmetic::ForwardFloatToInteger};
    // Two voices sum to .8 at the chosen listening gain. Post-clamp gain
    // flattens this to .5; clean playback must retain the .8 waveform.
    for (int sign : {-1, 1}) {
        auto s = sample(int16_t(sign * 26214), 2);
        SampleMixer original(rules, 44100, .5f), clean(rules, 44100, .5f, PlaybackMix::CleanFloat);
        original.setSamples({s});
        clean.setSamples({s});
        for (int i = 0; i < 2; ++i) {
            original.play(0, 32768, 1);
            clean.play(0, 32768, 1);
        }
        float a[64], b[64];
        original.mix(a, 32);
        clean.mix(b, 32);
        CHECK(std::fabs(a[32]) <= .5f && std::fabs(b[32] - sign * .8f) < .0001f);
    }
    // Same commands and native boundaries must produce identical voice state.
    for (auto arithmetic :
         {MixArithmetic::ReverseQ16Saturating, MixArithmetic::ForwardFloatToInteger}) {
        NativeSampleMixer a({3, arithmetic}), b({3, arithmetic});
        auto s = sample(30000, 2);
        s.pcm.resize(8);
        a.setSamples({s, s});
        b.setSamples({s, s});
        for (int tick = 0; tick < 64; ++tick) {
            if (tick % 7 == 0) {
                a.play(0, 32768, 0);
                b.play(0, 32768, 0);
            }
            if (tick % 11 == 0) {
                a.play(1, 65536, 1);
                b.play(1, 65536, 1);
            }
            if (tick % 13 == 0) {
                a.stop(1);
                b.stop(1);
            }
            int16_t x[3];
            float y[3];
            a.mixMono(x, 3);
            b.mixFloat(y, 3);
            for (int i = 0; i < 3; ++i) {
                const auto &u = a.voices()[i];
                const auto &v = b.voices()[i];
                CHECK(u.sample == v.sample && u.position == v.position && u.volume == v.volume &&
                      u.loop == v.loop);
            }
            for (int i = 0; i < 2; ++i)
                CHECK(a.samples()[i].active == b.samples()[i].active);
        }
    }
    for (int rate : {11025, 44100, 48000}) {
        SampleMixer a(rules, rate, .5f, PlaybackMix::CleanFloat),
            b(rules, rate, .5f, PlaybackMix::CleanFloat);
        auto s = sample(32767, 3);
        a.setSamples({s});
        b.setSamples({s});
        for (int i = 0; i < 3; ++i) {
            a.play(0, 32768, 1);
            b.play(0, 32768, 1);
        }
        std::vector<float> x(514), y(514);
        a.mix(x.data(), 257);
        int at = 0;
        for (int n : {1, 3, 17, 29, 100, 107}) {
            b.mix(y.data() + at * 2, n);
            at += n;
        }
        CHECK(x == y && x[64] == 1.f);
        for (float v : x)
            CHECK(std::isfinite(v) && std::fabs(v) <= 1);
        a.stopAll();
        a.mix(x.data(), 257);
        for (float v : x)
            CHECK(v == 0);
    }
}
void wavDecoder() {
    std::vector<uint8_t> bytes;
    auto word = [&](uint32_t x, int n) {
        for (int k = 0; k < n; ++k)
            bytes.push_back(uint8_t(x >> (8 * k)));
    };
    auto four = [&](const char *text) {
        for (int i = 0; i < 4; ++i)
            bytes.push_back(uint8_t(text[i]));
    };
    four("RIFF");
    word(72, 4);
    four("WAVE");
    four("fmt ");
    word(16, 4);
    word(1, 2);
    word(2, 2);
    word(11025, 4);
    word(44100, 4);
    word(4, 2);
    word(16, 2);
    four("data");
    word(36, 4);
    for (int i = 0; i < 9; ++i) {
        word(6000, 2);
        word(uint16_t(-2000), 2);
    }
    Sample s;
    s.priority = 7;
    s.maxInstances = 3;
    s.restart = true;
    CHECK(parseWav(bytes, s));
    CHECK(s.pcm.size() == 8 && s.pcm[0] == 2000 && s.rate == 11025);
    CHECK(s.priority == 7 && s.maxInstances == 3 && s.restart);
    auto old = s.pcm;
    bytes.pop_back();
    CHECK(!parseWav(bytes, s) && s.pcm == old);
    CHECK(!parseWav({}, s) && s.pcm == old);
    neon::ResourceCatalog resources;
    CHECK(resources.load(
        "<NeonResources><Literals><Literal>x</Literal></Literals><Spritemaps><Spritemap entries='1'/></Spritemaps><Sounds><Sample priority='7'>a</Sample><Sample priority='0'>b</Sample></Sounds></NeonResources>"));
    CHECK(resources.samples()[0].priority == 7 && resources.samples()[1].priority == 1);
}
void imported(const char *directory) {
    std::size_t count = 0;
    for (const char *game : {"minotron", "minorescue", "gridrunner"}) {
        uint64_t hash = 14695981039346656037ULL;
        std::size_t gameCount = 0;
        auto byte = [&](uint8_t x) {
            hash ^= x;
            hash *= 1099511628211ULL;
        };
        const std::string root = std::string(directory) + "/" + game + "/";
        std::ifstream xml(root + "NeonResources.xml");
        CHECK(xml.good());
        const std::string text((std::istreambuf_iterator<char>(xml)), {});
        neon::ResourceCatalog resources;
        CHECK(resources.load(text));
        for (const auto &entry : resources.samples()) {
            std::ifstream wav(root + entry.file, std::ios::binary);
            CHECK(wav.good());
            const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(wav)), {});
            Sample s;
            CHECK(parseWav(bytes, s));
            CHECK(s.rate == 11025 && s.pcm.size() > 0 && s.pcm.size() % 8 == 0);
            ++count;
            ++gameCount;
            for (int k = 0; k < 8; ++k)
                byte(uint8_t(uint64_t(s.pcm.size()) >> (8 * k)));
            for (int16_t value : s.pcm) {
                byte(uint8_t(uint16_t(value)));
                byte(uint8_t(uint16_t(value) >> 8));
            }
        }
        std::cout << game << " " << gameCount << " " << std::hex << hash << std::dec << "\n";
    }
    std::cout << count << " imported native PCM samples decoded and aligned\n";
}
int main(int argc, char **argv) {
    try {
        allocation();
        retriggerAndLoops();
        arithmetic();
        hostStream();
        cleanPlayback();
        wavDecoder();
        for (int i = 1; i + 1 < argc; i += 2) {
            if (std::string(argv[i]) == "--imports")
                imported(argv[i + 1]);
            else if (std::string(argv[i]) == "--fixture")
                arithmeticFixture(argv[i + 1]);
            else
                CHECK(false);
        }
        std::cout
            << "Original audio allocation, restart/stop/group, non-counted loops, arithmetic, boundaries, decoder and host stream passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
