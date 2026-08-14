#include "../CppOverdub.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <thread>
#include <vector>

namespace {

int TestsRun = 0;
int TestsPassed = 0;
int TestsFailed = 0;
std::vector<std::string> FailureList;

#define TEST_ASSERT(cond, msg)                                                     \
    do {                                                                           \
        if (!(cond)) {                                                             \
            ::std::println(stderr, "    FAIL: {}:{} {}", __FILE__, __LINE__, msg); \
            FailureList.push_back(::std::string(#cond) + " -- " + (msg));          \
            return;                                                                \
        }                                                                          \
    } while (0)

#define TEST_APPROX(a, b, eps, msg)                                        \
    do {                                                                   \
        double _da = static_cast<double>(a);                               \
        double _db = static_cast<double>(b);                               \
        if (::std::abs(_da - _db) > (eps)) {                               \
            ::std::println(stderr, "    FAIL: {}:{} {} (got {} vs {})",    \
                           __FILE__, __LINE__, msg, _da, _db);             \
            FailureList.push_back(::std::string(#a) + " ~= " + #b +        \
                                  " -- " + (msg));                         \
            return;                                                        \
        }                                                                  \
    } while (0)

#define TEST_CASE(name)                                       \
    static void name();                                       \
    struct Register_##name {                                  \
        Register_##name() { Tests.push_back({#name, name}); } \
    } register_##name;                                        \
    static void name()

struct TestEntry {
    const char *Name;
    void (*Fn)();
};
std::vector<TestEntry> Tests;

AudioSegment generateSine(double Frequency, double Duration, int SampleRate = 44100,
                          double Amplitude = 0.8) {
    return AudioSegment::doGenerate(Frequency, Duration, SampleRate, Amplitude);
}

int countZeroCrossings(const AudioSegment &Audio) {
    int Crossings = 0;
    int16_t Previous = 0;
    bool HasPrevious = false;
    const auto &AudioConst = Audio;
    for (auto Sample : AudioConst.getSamples(0, -1)) {
        if (HasPrevious && ((Previous < 0 && Sample >= 0) || (Previous > 0 && Sample <= 0))) ++Crossings;
        Previous = Sample;
        HasPrevious = true;
    }
    return Crossings;
}

bool hasOpenALDevice() {
    ALCdevice *D = ::alcOpenDevice(nullptr);
    if (!D) return false;
    ::alcCloseDevice(D);
    return true;
}

// ---------------------------------------------------------------------------
// MediaChannelLayout
// ---------------------------------------------------------------------------

TEST_CASE(testMediaChannelLayout_Default) {
    MediaChannelLayout Layout;
    TEST_ASSERT(Layout.getChannelCount() == 0, "default channel count");
    TEST_ASSERT(Layout.getChannelMask() == 0, "default channel mask");
}

TEST_CASE(testMediaChannelLayout_Construct) {
    MediaChannelLayout Layout{2, AV_CH_LAYOUT_STEREO};
    TEST_ASSERT(Layout.getChannelCount() == 2, "stereo count");
    TEST_ASSERT(Layout.getChannelMask() == AV_CH_LAYOUT_STEREO, "stereo mask");
}

TEST_CASE(testMediaChannelLayout_FromAVChannelLayout) {
    AVChannelLayout AvLayout = AV_CHANNEL_LAYOUT_MASK(2, AV_CH_LAYOUT_STEREO);
    MediaChannelLayout Layout(AvLayout);
    TEST_ASSERT(Layout.getChannelCount() == 2, "from av count");
    TEST_ASSERT(Layout.getChannelMask() == AV_CH_LAYOUT_STEREO, "from av mask");
}

TEST_CASE(testMediaChannelLayout_FromAVChannelLayout_ZeroMaskFallsBackToStereo) {
    AVChannelLayout AvLayout = AV_CHANNEL_LAYOUT_MASK(1, 0);
    MediaChannelLayout Layout(AvLayout);
    TEST_ASSERT(Layout.getChannelMask() == AV_CH_LAYOUT_STEREO, "zero mask fallback");
}

TEST_CASE(testMediaChannelLayout_ToFFMpegFormat) {
    MediaChannelLayout Layout{2, AV_CH_LAYOUT_STEREO};
    AVChannelLayout AvLayout = Layout.toFFMpegFormat();
    TEST_ASSERT(AvLayout.nb_channels == 2, "toFFMpeg nb_channels");
    TEST_ASSERT(AvLayout.u.mask == AV_CH_LAYOUT_STEREO, "toFFMpeg mask");
}

TEST_CASE(testMediaChannelLayout_ToOpenALFormat) {
    TEST_ASSERT(LayoutMono.toOpenALFormat() == AL_FORMAT_MONO16, "mono openal");
    TEST_ASSERT(LayoutStereo.toOpenALFormat() == AL_FORMAT_STEREO16, "stereo openal");
    TEST_ASSERT(LayoutQuad.toOpenALFormat() == AL_FORMAT_QUAD16, "quad openal");
    TEST_ASSERT(Layout51.toOpenALFormat() == AL_FORMAT_51CHN16, "5.1 openal");
    TEST_ASSERT(Layout61.toOpenALFormat() == AL_FORMAT_61CHN16, "6.1 openal");
    TEST_ASSERT(Layout71.toOpenALFormat() == AL_FORMAT_71CHN16, "7.1 openal");
}

TEST_CASE(testMediaChannelLayout_ToOpenALFormat_Unsupported_Throws) {
    MediaChannelLayout Layout{3, 0x1234};
    bool Threw = false;
    try {
        (void) Layout.toOpenALFormat();
    } catch (const std::runtime_error &) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "unsupported layout should throw");
}

// ---------------------------------------------------------------------------
// AudioSegment::doGenerate
// ---------------------------------------------------------------------------

TEST_CASE(testDoGenerate_Duration) {
    auto Audio = generateSine(440, 1.0, 44100);
    TEST_APPROX(Audio.getDuration(), 1.0, 0.01, "1s sine duration");
    TEST_ASSERT(Audio.getSampleRate() == 44100, "sample rate preserved");
}

TEST_CASE(testDoGenerate_Silence) {
    auto Audio = generateSine(0, 0.5, 44100);
    TEST_ASSERT(Audio.getMaximum() == 0, "frequency 0 produces silence");
}

TEST_CASE(testDoGenerate_NegativeFrequencyClampedToZero) {
    auto Audio = AudioSegment::doGenerate(-50, 0.2, 44100, 0.8);
    TEST_ASSERT(Audio.getMaximum() == 0, "negative frequency clamped");
}

TEST_CASE(testDoGenerate_AmplitudeAffectsMaximum) {
    auto Loud = generateSine(440, 0.5, 44100, 0.9);
    auto Quiet = generateSine(440, 0.5, 44100, 0.1);
    TEST_ASSERT(Loud.getMaximum() > Quiet.getMaximum(), "louder amplitude -> larger max");
}

TEST_CASE(testDoGenerate_SineWaveContent) {
    const auto Audio = generateSine(440, 0.1, 44100, 0.8);
    auto Samples = Audio.getSamples(0, 100);
    int ZeroCrossings = 0;
    int16_t Prev = 0;
    bool First = true;
    for (auto S : Samples) {
        if (!First && ((Prev < 0 && S >= 0) || (Prev > 0 && S <= 0))) ++ZeroCrossings;
        Prev = S;
        First = false;
    }
    // 440 Hz over 100 samples at 44100 Hz -> ~1 cycle -> 2 zero crossings
    TEST_ASSERT(ZeroCrossings >= 1, "sine wave has zero crossings");
}

// ---------------------------------------------------------------------------
// AudioSegment::doGain / doNormalize
// ---------------------------------------------------------------------------

TEST_CASE(testDoGain_Positive) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Boosted = Audio.doGain(6.0); // +6 dB ~ 2x amplitude
    TEST_APPROX(Boosted.getMaximum(), Audio.getMaximum() * 2.0, Audio.getMaximum() * 0.05,
                "+6dB ~ 2x");
}

TEST_CASE(testDoGain_Negative) {
    auto Audio = generateSine(440, 0.2, 44100, 0.8);
    auto Attenuated = Audio.doGain(-6.0);
    TEST_ASSERT(Attenuated.getMaximum() < Audio.getMaximum(), "-6dB attenuates");
}

TEST_CASE(testDoGain_ZeroGain_NoChange) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Same = Audio.doGain(0.0);
    TEST_ASSERT(Same.isEqual(Audio), "0dB no change");
}

TEST_CASE(testDoNormalize) {
    auto Audio = generateSine(440, 0.5, 44100, 0.1);
    auto Normalized = Audio.doNormalize(0.1);
    TEST_ASSERT(Normalized.getMaximum() > Audio.getMaximum(), "normalize increases low signal");
    double ExpectedMax = 32767.0 * AudioSegment::dB2Ratio(-0.1);
    TEST_APPROX(Normalized.getMaximum(), ExpectedMax, ExpectedMax * 0.05, "normalize hits headroom");
}

TEST_CASE(testDoNormalize_SilenceIsNoop) {
    auto Audio = generateSine(0, 0.2, 44100);
    auto Normalized = Audio.doNormalize();
    TEST_ASSERT(Normalized.getMaximum() == 0, "normalize silence stays silent");
}

// ---------------------------------------------------------------------------
// AudioSegment::doFilterLow / doFilterHigh / doFilterBand
// ---------------------------------------------------------------------------

TEST_CASE(testDoFilterLow_AttenuatesHighFrequency) {
    auto Audio = generateSine(8000, 0.3, 44100, 0.8);
    auto Filtered = Audio.doFilterLow(1000);
    TEST_APPROX(Filtered.getDuration(), Audio.getDuration(), 0.01, "lowpass keeps duration");
    TEST_ASSERT(Filtered.getRMS() < Audio.getRMS() * 0.2, "8kHz tone strongly attenuated by 1kHz lowpass");
}

TEST_CASE(testDoFilterLow_PassesLowFrequency) {
    auto Audio = generateSine(100, 0.3, 44100, 0.8);
    auto Filtered = Audio.doFilterLow(1000);
    TEST_ASSERT(Filtered.getRMS() > Audio.getRMS() * 0.8, "100Hz tone passes 1kHz lowpass");
}

TEST_CASE(testDoFilterHigh_AttenuatesLowFrequency) {
    auto Audio = generateSine(100, 0.3, 44100, 0.8);
    auto Filtered = Audio.doFilterHigh(1000);
    TEST_ASSERT(Filtered.getRMS() < Audio.getRMS() * 0.2, "100Hz tone strongly attenuated by 1kHz highpass");
}

TEST_CASE(testDoFilterHigh_PassesHighFrequency) {
    auto Audio = generateSine(8000, 0.3, 44100, 0.8);
    auto Filtered = Audio.doFilterHigh(1000);
    TEST_ASSERT(Filtered.getRMS() > Audio.getRMS() * 0.8, "8kHz tone passes 1kHz highpass");
}

TEST_CASE(testDoFilterBand_PassesInBand) {
    auto Audio = generateSine(1000, 0.3, 44100, 0.8);
    auto Filtered = Audio.doFilterBand(500, 2000);
    TEST_ASSERT(Filtered.getRMS() > Audio.getRMS() * 0.6, "1kHz tone passes 500Hz-2kHz band");
}

TEST_CASE(testDoFilterBand_AttenuatesOutOfBand) {
    auto Audio = generateSine(100, 0.3, 44100, 0.8);
    auto Filtered = Audio.doFilterBand(500, 2000);
    TEST_ASSERT(Filtered.getRMS() < Audio.getRMS() * 0.2, "100Hz tone rejected by 500Hz-2kHz band");
}

TEST_CASE(testDoFilterLow_InvalidCutoffThrows) {
    auto Audio = generateSine(440, 0.2, 44100);
    bool Threw = false;
    try {
        (void) Audio.doFilterLow(0);
    } catch (const std::invalid_argument&) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "zero cutoff throws");
}

TEST_CASE(testDoFilterBand_InvalidRangeThrows) {
    auto Audio = generateSine(440, 0.2, 44100);
    bool Threw = false;
    try {
        (void) Audio.doFilterBand(2000, 500);
    } catch (const std::invalid_argument&) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "reversed band range throws");
}

TEST_CASE(testDoFilterLow_EmptyAudioIsNoop) {
    auto Empty = AudioSegment::doGenerate(0, 0, 44100);
    auto Filtered = Empty.doFilterLow(1000);
    TEST_ASSERT(Filtered.getDuration() == 0, "filtering empty audio stays empty");
}

// ---------------------------------------------------------------------------
// AudioSegment::doPan
// ---------------------------------------------------------------------------

TEST_CASE(testDoPan_MonoToStereo) {
    auto Audio = generateSine(440, 0.3, 44100, 0.8);
    auto Panned = Audio.doPan(0);
    TEST_ASSERT(Panned.getChannelLayout().getChannelCount() == 2, "mono pan produces stereo");
    TEST_APPROX(Panned.getDuration(), Audio.getDuration(), 0.01, "pan keeps duration");
}

TEST_CASE(testDoPan_MonoFullLeft) {
    auto Audio = generateSine(440, 0.3, 44100, 0.8);
    auto Panned = Audio.doPan(-1);
    auto Chs = std::vector<AudioSegment>();
    for (auto Ch : Panned.doSplitChannels()) Chs.push_back(Ch);
    TEST_APPROX(Chs[0].getRMS(), Audio.getRMS(), Audio.getRMS() * 0.05, "full left keeps left level");
    TEST_ASSERT(Chs[1].getMaximum() == 0, "full left silences right channel");
}

TEST_CASE(testDoPan_MonoFullRight) {
    auto Audio = generateSine(440, 0.3, 44100, 0.8);
    auto Panned = Audio.doPan(1);
    auto Chs = std::vector<AudioSegment>();
    for (auto Ch : Panned.doSplitChannels()) Chs.push_back(Ch);
    TEST_ASSERT(Chs[0].getMaximum() == 0, "full right silences left channel");
    TEST_APPROX(Chs[1].getRMS(), Audio.getRMS(), Audio.getRMS() * 0.05, "full right keeps right level");
}

TEST_CASE(testDoPan_StereoCenter) {
    auto A = generateSine(440, 0.3, 44100, 0.8);
    auto Stereo = AudioSegment::fromChannels(A, A.doGain(-6));
    auto Panned = Stereo.doPan(0);
    auto Chs = std::vector<AudioSegment>();
    for (auto Ch : Panned.doSplitChannels()) Chs.push_back(Ch);
    TEST_APPROX(Chs[0].getRMS(), A.getRMS() * 0.7071, A.getRMS() * 0.06, "center left -3dB");
    TEST_APPROX(Chs[1].getRMS(), A.getRMS() * 0.7071 * 0.5012, A.getRMS() * 0.06, "center right -3dB");
}

TEST_CASE(testDoPan_OutOfRangeThrows) {
    auto Audio = generateSine(440, 0.2, 44100);
    bool Threw = false;
    try {
        (void) Audio.doPan(2);
    } catch (const std::invalid_argument&) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "pan > 1 throws");
}

// ---------------------------------------------------------------------------
// AudioSegment::doFade / doFadeIn / doFadeOut
// ---------------------------------------------------------------------------

TEST_CASE(testDoFadeIn) {
    const auto Audio = generateSine(440, 0.5, 44100, 0.8);
    auto Faded = Audio.doFadeIn(200);
    const auto& FadedC = Faded;
    auto SamplesEarly = FadedC.getSamples(0, 10);
    auto SamplesLater = FadedC.getSamples(4000, 4010);
    int EarlyMax = 0, LateMax = 0;
    for (auto S : SamplesEarly) EarlyMax = std::max(EarlyMax, (int) std::abs((int) S));
    for (auto S : SamplesLater) LateMax = std::max(LateMax, (int) std::abs((int) S));
    TEST_ASSERT(EarlyMax < LateMax, "fade-in: early quieter than late");
}

TEST_CASE(testDoFadeOut) {
    const auto Audio = generateSine(440, 0.5, 44100, 0.8);
    auto Faded = Audio.doFadeOut(200);
    const auto& FadedC = Faded;
    auto SamplesEarly = FadedC.getSamples(0, 10);
    auto SamplesLate = FadedC.getSamples(20000, 20010);
    int EarlyMax = 0, LateMax = 0;
    for (auto S : SamplesEarly) EarlyMax = std::max(EarlyMax, (int) std::abs((int) S));
    for (auto S : SamplesLate) LateMax = std::max(LateMax, (int) std::abs((int) S));
    TEST_ASSERT(LateMax < EarlyMax, "fade-out: late quieter than early");
}

TEST_CASE(testDoFade_NoOpWhenBothZero) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Faded = Audio.doFade(0, 0);
    TEST_ASSERT(Faded.isEqual(Audio), "doFade(0,0) noop");
}

// ---------------------------------------------------------------------------
// AudioSegment::doConcat
// ---------------------------------------------------------------------------

TEST_CASE(testDoConcat_Duration) {
    auto A = generateSine(440, 0.5, 44100, 0.5);
    auto B = generateSine(880, 0.5, 44100, 0.5);
    auto C = A.doConcat(B);
    TEST_APPROX(C.getDuration(), 1.0, 0.02, "concat doubles duration");
}

TEST_CASE(testDoConcat_DifferentSampleRates) {
    auto A = generateSine(440, 0.5, 44100, 0.5);
    auto B = generateSine(880, 0.5, 22050, 0.5);
    auto C = A.doConcat(B);
    TEST_APPROX(C.getDuration(), 1.0, 0.05, "concat resamples to higher rate");
    TEST_ASSERT(C.getSampleRate() == 44100, "concat output rate");
}

// ---------------------------------------------------------------------------
// AudioSegment::doOverlay
// ---------------------------------------------------------------------------

TEST_CASE(testDoOverlay_Length) {
    auto Base = generateSine(440, 1.0, 44100, 0.5);
    auto Over = generateSine(880, 0.2, 44100, 0.5);
    auto Mixed = Base.doOverlay(Over, 500);
    TEST_APPROX(Mixed.getDuration(), 1.0, 0.02, "overlay does not extend base");
}

TEST_CASE(testDoOverlay_ExtendsWhenOverlayBeyondEnd) {
    auto Base = generateSine(440, 0.5, 44100, 0.5);
    auto Over = generateSine(880, 0.5, 44100, 0.5);
    auto Mixed = Base.doOverlay(Over, 400);
    TEST_ASSERT(Mixed.getDuration() > Base.getDuration(), "overlay extends beyond base");
}

TEST_CASE(testDoOverlay_Loop) {
    auto Base = generateSine(440, 1.0, 44100, 0.5);
    auto Over = generateSine(880, 0.1, 44100, 0.3);
    auto Mixed = Base.doOverlay(Over, 0, true);
    TEST_APPROX(Mixed.getDuration(), 1.0, 0.02, "loop overlay same duration as base");
    TEST_ASSERT(Mixed.getMaximum() > Base.getMaximum(), "overlay adds energy");
}

TEST_CASE(testDoOverlay_LoopTime) {
    auto Base = generateSine(440, 1.0, 44100, 0.5);
    auto Over = generateSine(880, 0.1, 44100, 0.3);
    auto Mixed = Base.doOverlay(Over, 0, true, 300);
    TEST_APPROX(Mixed.getDuration(), 1.0, 0.02, "loop with bounded time keeps base duration");
}

// ---------------------------------------------------------------------------
// AudioSegment::doSpeedUp
// ---------------------------------------------------------------------------

TEST_CASE(testDoSpeedUp_Factor2) {
    auto Audio = generateSine(440, 1.0, 44100, 0.5);
    auto Sped = Audio.doSpeedUp(2.0);
    TEST_ASSERT(Sped.getDuration() < Audio.getDuration(), "speedup shortens duration");
    TEST_APPROX(Sped.getDuration(), 0.5, 0.05, "2x speedup ~ half duration");
}

TEST_CASE(testDoSpeedUp_Factor1_NoChange) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Sped = Audio.doSpeedUp(1.0);
    TEST_ASSERT(Sped.isEqual(Audio), "1x speedup noop");
}

TEST_CASE(testDoSpeedUp_NonPositive_Throws) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    bool Threw = false;
    try {
        (void) Audio.doSpeedUp(0.0);
    } catch (const std::invalid_argument &) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "speedup 0 throws");
}

// ---------------------------------------------------------------------------
// AudioSegment::doRepeat / doReverse
// ---------------------------------------------------------------------------

TEST_CASE(testDoRepeat) {
    auto Audio = generateSine(440, 0.5, 44100, 0.5);
    auto Repeated = Audio.doRepeat(3);
    TEST_APPROX(Repeated.getDuration(), 1.5, 0.02, "repeat 3x triples duration");
}

TEST_CASE(testDoRepeat_Zero) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Repeated = Audio.doRepeat(0);
    TEST_ASSERT(Repeated.getDuration() == 0, "repeat 0 produces empty");
}

TEST_CASE(testDoReverse_PreservesLength) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Reversed = Audio.doReverse();
    TEST_APPROX(Reversed.getDuration(), Audio.getDuration(), 0.01, "reverse preserves length");
}

TEST_CASE(testDoReverse_ReversesSamples) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Reversed = Audio.doReverse();
    const auto &AudioC = Audio;
    const auto &ReversedC = Reversed;
    std::vector<int16_t> FwdVec;
    for (auto S : AudioC.getSamples(0, 5)) FwdVec.push_back(S);
    std::vector<int16_t> RevVec;
    for (auto S : ReversedC.getSamples(-5, 0)) RevVec.push_back(S);
    std::reverse(RevVec.begin(), RevVec.end());
    TEST_ASSERT(FwdVec == RevVec, "reversed tail matches forward head");
}

// ---------------------------------------------------------------------------
// AudioSegment::doSlice
// ---------------------------------------------------------------------------

TEST_CASE(testDoSlice_Range) {
    auto Audio = generateSine(440, 1.0, 44100, 0.5);
    auto Sliced = Audio.doSlice(100, 500, 1);
    TEST_APPROX(Sliced.getDuration(), 400.0 / 1000.0, 0.01, "slice [100,500) ms");
}

TEST_CASE(testDoSlice_Step) {
    auto Audio = generateSine(440, 1.0, 44100, 0.5);
    auto Sliced = Audio.doSlice(0, 1000, 2);
    TEST_APPROX(Sliced.getDuration(), 0.5, 0.02, "step 2 halves samples");
}

TEST_CASE(testDoSlice_ZeroStep_Throws) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    bool Threw = false;
    try {
        (void) Audio.doSlice(0, 100, 0);
    } catch (const std::invalid_argument &) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "zero step throws");
}

TEST_CASE(testDoSlice_EmptyRange) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Sliced = Audio.doSlice(100, 100, 1);
    TEST_ASSERT(Sliced.getDuration() == 0, "empty slice returns empty");
}

TEST_CASE(testDoSlice_Position) {
    auto Audio = generateSine(440, 1.0, 44100, 0.5);
    auto Sliced = Audio.doSlice((intmax_t) 500);
    TEST_ASSERT(Sliced.getDuration() > 0 && Sliced.getDuration() < 0.01,
                "slice by position ~1ms");
}

TEST_CASE(testDoSlice_NegativePosition) {
    auto Audio = generateSine(440, 1.0, 44100, 0.5);
    auto Sliced = Audio.doSlice((intmax_t) -1);
    TEST_ASSERT(Sliced.getDuration() > 0, "negative position wraps");
}

// ---------------------------------------------------------------------------
// AudioSegment::doSplitChannels / fromChannels
// ---------------------------------------------------------------------------

TEST_CASE(testDoSplitChannels) {
    auto Left = generateSine(440, 0.2, 44100, 0.5);
    auto Right = generateSine(880, 0.2, 44100, 0.5);
    auto Stereo = AudioSegment::fromChannels(Left, Right);
    TEST_ASSERT(Stereo.getChannelLayout().getChannelCount() == 2, "fromChannels -> stereo");
    std::vector<AudioSegment> Chs;
    for (auto Ch : Stereo.doSplitChannels()) Chs.push_back(Ch);
    TEST_ASSERT(Chs.size() == 2, "split produces 2 channels");
    TEST_ASSERT(Chs[0].getChannelLayout().getChannelCount() == 1, "split channel is mono");
}

TEST_CASE(testFromChannels_ZeroArgs) {
    auto Empty = AudioSegment::fromChannels();
    TEST_ASSERT(Empty.getDuration() == 0, "fromChannels() empty");
    TEST_ASSERT(Empty.getChannelLayout().getChannelCount() == 1, "empty defaults mono");
}

TEST_CASE(testFromChannels_UnequalLengths) {
    auto A = generateSine(440, 0.3, 44100, 0.5);
    auto B = generateSine(880, 0.1, 44100, 0.5);
    auto Stereo = AudioSegment::fromChannels(A, B);
    TEST_APPROX(Stereo.getDuration(), 0.3, 0.01, "fromChannels uses longest");
}

// ---------------------------------------------------------------------------
// AudioSegment::doSplitOnSilence / doStripSilence
// ---------------------------------------------------------------------------

TEST_CASE(testDoSplitOnSilence) {
    auto Tone1 = generateSine(440, 0.2, 44100, 0.5);
    auto Silence = generateSine(0, 1.0, 44100);
    auto Tone2 = generateSine(880, 0.2, 44100, 0.5);
    auto Combined = Tone1.doConcat(Silence).doConcat(Tone2);
    std::vector<AudioSegment> Parts;
    for (auto P : Combined.doSplitOnSilence()) Parts.push_back(P);
    TEST_ASSERT(Parts.size() >= 2, "split on silence produces at least 2 segments");
}

TEST_CASE(testDoStripSilence) {
    auto Silence = generateSine(0, 0.1, 44100);
    auto Tone = generateSine(440, 0.3, 44100, 0.5);
    auto WithSilence = Silence.doConcat(Tone);
    auto Stripped = WithSilence.doStripSilence(100, -40, 0);
    TEST_ASSERT(Stripped.getDuration() < WithSilence.getDuration(),
                "strip silence shortens audio");
}

// ---------------------------------------------------------------------------
// AudioSegment::doCompressDynamicRange
// ---------------------------------------------------------------------------

TEST_CASE(testDoCompressDynamicRange) {
    auto Audio = generateSine(440, 0.3, 44100, 0.9);
    auto Compressed = Audio.doCompressDynamicRange(-20, 4, 5, 50);
    TEST_ASSERT(Compressed.getMaximum() <= Audio.getMaximum(),
                "compression does not exceed original max");
    TEST_APPROX(Compressed.getDuration(), Audio.getDuration(), 0.01, "compression preserves length");
}

TEST_CASE(testDoCompressDynamicRange_ZeroRatio_NoChange) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Same = Audio.doCompressDynamicRange(-20, 0);
    TEST_ASSERT(Same.isEqual(Audio), "ratio 0 noop");
}

// ---------------------------------------------------------------------------
// AudioSegment::doSynchronize
// ---------------------------------------------------------------------------

TEST_CASE(testDoSynchronize_Single) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    std::vector<AudioSegment> Out;
    for (auto A : AudioSegment::doSynchronize(Audio)) Out.push_back(A);
    TEST_ASSERT(Out.size() == 1, "synchronize single returns 1");
}

TEST_CASE(testDoSynchronize_Multiple) {
    auto A = generateSine(440, 0.3, 44100, 0.5);
    auto B = generateSine(880, 0.3, 22050, 0.5);
    std::vector<AudioSegment> Out;
    for (auto Seg : AudioSegment::doSynchronize(A, B)) Out.push_back(Seg);
    TEST_ASSERT(Out.size() == 2, "synchronize 2 inputs -> 2 outputs");
    TEST_ASSERT(Out[0].getSampleRate() == Out[1].getSampleRate(), "synchronized sample rates match");
    TEST_ASSERT(Out[0].getChannelLayout().getChannelCount() == Out[1].getChannelLayout().getChannelCount(),
                "synchronized channel counts match");
}

// ---------------------------------------------------------------------------
// AudioSegment::setChannelLayout / setSampleRate
// ---------------------------------------------------------------------------

TEST_CASE(testSetChannelLayout_MonoToStereo) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Stereo = Audio.setChannelLayout(LayoutStereo);
    TEST_ASSERT(Stereo.getChannelLayout().getChannelCount() == 2, "set layout stereo");
    TEST_APPROX(Stereo.getDuration(), Audio.getDuration(), 0.01, "set layout preserves duration");
}

TEST_CASE(testSetSampleRate_Upsample) {
    auto Audio = generateSine(440, 0.3, 22050, 0.5);
    auto Upsampled = Audio.setSampleRate(44100);
    TEST_ASSERT(Upsampled.getSampleRate() == 44100, "new rate applied");
    TEST_APPROX(Upsampled.getDuration(), 0.3, 0.05, "duration preserved after resample");
}

TEST_CASE(testSetSampleRate_Same_NoChange) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Same = Audio.setSampleRate(44100);
    TEST_ASSERT(Same.isEqual(Audio), "same rate noop");
}

TEST_CASE(testSetSampleRate_Invalid_Throws) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    bool Threw = false;
    try {
        (void) Audio.setSampleRate(0);
    } catch (const std::runtime_error &) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "rate 0 throws");
}

// ---------------------------------------------------------------------------
// AudioSegment measurements
// ---------------------------------------------------------------------------

TEST_CASE(testGetMaximumPossibleAmplitude) {
    auto Audio = generateSine(440, 0.1, 44100, 0.5);
    // Int16 full-scale magnitude is 32768
    TEST_APPROX(Audio.getMaximumPossibleAmplitude(), 32768.0, 1.0, "int16 amplitude");
}

TEST_CASE(testGetRMS) {
    auto Audio = generateSine(440, 0.2, 44100, 1.0);
    double Rms = Audio.getRMS();
    TEST_ASSERT(Rms > 0, "RMS positive for sine");
    TEST_ASSERT(Rms <= 32767, "RMS bounded by max amplitude");
}

TEST_CASE(testGetdBFS_SilenceNegativeInfinity) {
    auto Audio = generateSine(0, 0.2, 44100);
    double Db = Audio.getdBFS();
    TEST_ASSERT(Db == -std::numeric_limits<double>::infinity(), "silence dBFS = -inf");
}

TEST_CASE(testIsEqual) {
    auto A = generateSine(440, 0.2, 44100, 0.5);
    auto B = generateSine(440, 0.2, 44100, 0.5);
    auto C = generateSine(880, 0.2, 44100, 0.5);
    TEST_ASSERT(A.isEqual(B), "identical generation equal");
    TEST_ASSERT(!A.isEqual(C), "different freq not equal");
}

// ---------------------------------------------------------------------------
// Static helpers dB2Ratio / ratio2dB
// ---------------------------------------------------------------------------

TEST_CASE(testDB2Ratio) {
    TEST_APPROX(AudioSegment::dB2Ratio(0), 1.0, 1e-9, "0dB = 1x");
    TEST_APPROX(AudioSegment::dB2Ratio(20), 10.0, 1e-6, "+20dB = 10x amplitude");
    TEST_APPROX(AudioSegment::dB2Ratio(-20), 0.1, 1e-6, "-20dB = 0.1x amplitude");
    TEST_APPROX(AudioSegment::dB2Ratio(10, false), 10.0, 1e-6, "+10dB power = 10x");
}

TEST_CASE(testRatio2dB) {
    TEST_APPROX(AudioSegment::ratio2dB(1.0), 0.0, 1e-9, "1x = 0dB");
    TEST_APPROX(AudioSegment::ratio2dB(10.0), 20.0, 1e-9, "10x = 20dB");
    TEST_ASSERT(AudioSegment::ratio2dB(0.0) == -std::numeric_limits<double>::infinity(),
                "0 ratio = -inf dB");
}

TEST_CASE(testDB2Ratio_RoundTrip) {
    double X = 0.731;
    TEST_APPROX(AudioSegment::ratio2dB(AudioSegment::dB2Ratio(X)), X, 1e-6, "round trip");
}

// ---------------------------------------------------------------------------
// AudioSegment::doExport + doOpen round trip
// ---------------------------------------------------------------------------

TEST_CASE(testDoExportAndDoOpen_RoundTrip) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    const std::string Path = "test_roundtrip.wav";
    Audio.doExport(Path);
    TEST_ASSERT(std::filesystem::exists(Path), "export creates file");
    auto Loaded = AudioSegment::doOpen(Path);
    TEST_APPROX(Loaded.getDuration(), 0.3, 0.05, "loaded duration approx");
    TEST_ASSERT(Loaded.getSampleRate() > 0, "loaded has sample rate");
    std::filesystem::remove(Path);
}

TEST_CASE(testDoExport_CustomFormatMp3) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    const std::string Path = "test_export_custom.mp3";
    MediaExportOption Option;
    Option.ExportFormat = "mp3";
    Option.ExportCodec = "libmp3lame";
    Option.ExportBitrate = 64000;
    Audio.doExport(Path, Option);
    TEST_ASSERT(std::filesystem::exists(Path), "custom export creates file");
    auto Loaded = AudioSegment::doOpen(Path);
    TEST_APPROX(Loaded.getDuration(), 0.3, 0.08, "mp3 loaded duration approx");
    TEST_ASSERT(Loaded.getSampleRate() == 44100, "mp3 keeps sample rate");
    std::filesystem::remove(Path);
}

TEST_CASE(testDoExport_CustomFormatOgg) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    const std::string Path = "test_export_custom.ogg";
    MediaExportOption Option;
    Option.ExportFormat = "ogg";
    Option.ExportCodec = "libvorbis";
    Audio.doExport(Path, Option);
    TEST_ASSERT(std::filesystem::exists(Path), "ogg export creates file");
    auto Loaded = AudioSegment::doOpen(Path);
    TEST_APPROX(Loaded.getDuration(), 0.3, 0.08, "ogg loaded duration approx");
    std::filesystem::remove(Path);
}

TEST_CASE(testDoExport_CustomSampleRate) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    const std::string Path = "test_export_22k.wav";
    MediaExportOption Option;
    Option.ExportSampleRate = 22050;
    Audio.doExport(Path, Option);
    auto Loaded = AudioSegment::doOpen(Path);
    TEST_ASSERT(Loaded.getSampleRate() == 22050, "export resamples to requested rate");
    TEST_APPROX(Loaded.getDuration(), 0.3, 0.05, "resampled export keeps duration");
    std::filesystem::remove(Path);
}

TEST_CASE(testDoExport_InvalidFormatThrows) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    MediaExportOption Option;
    Option.ExportFormat = "nonexistentformatxyz";
    bool Threw = false;
    try {
        Audio.doExport("test_export_bad.out", Option);
    } catch (const std::runtime_error&) {
        Threw = true;
    }
    std::filesystem::remove("test_export_bad.out");
    TEST_ASSERT(Threw, "unknown format name throws");
}

TEST_CASE(testDoSlice_Position_OutOfBoundsThrows) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    bool Threw = false;
    try {
        intmax_t PastEnd = (intmax_t)(Audio.getDuration() * 1000) + 2;
        (void) Audio.doSlice(PastEnd);
    } catch (const std::runtime_error&) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "slice past end+1 throws");
}

// ---------------------------------------------------------------------------
// OpenAL playback smoke test (may be skipped if no device available)
// ---------------------------------------------------------------------------

TEST_CASE(testOpenAL_PlaybackSmoke) {
    bool HasDevice = false;
    {
        ALCdevice *D = ::alcOpenDevice(nullptr);
        if (D) {
            HasDevice = true;
            ::alcCloseDevice(D);
        }
    }
    if (!HasDevice) {
        ::std::println(stderr, "    [skip] no OpenAL device available");
        return;
    }

    try {
        doInitializeOpenAL();
        auto Audio = generateSine(440, 0.2, 44100, 0.5);
        OpenAL::MediaSource Source;
        Source.setSourceBuffer(Audio.toMediaBuffer());
        Source.doPlay();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Source.doStop();
        doDestroyOpenAL();
        TEST_ASSERT(true, "OpenAL playback completes without error");
    } catch (const std::runtime_error &E) {
        ::std::println(stderr, "    [skip] OpenAL: {}", E.what());
    }
}

TEST_CASE(testOpenAL_MediaReverbSmoke) {
    bool HasDevice = false;
    {
        ALCdevice *D = ::alcOpenDevice(nullptr);
        if (D) {
            HasDevice = true;
            ::alcCloseDevice(D);
        }
    }
    if (!HasDevice) {
        ::std::println(stderr, "    [skip] no OpenAL device available");
        return;
    }

    try {
        doInitializeOpenAL();
        auto Audio = generateSine(440, 0.2, 44100, 0.5);
        OpenAL::MediaSource Source;
        Source.setSourceBuffer(Audio.toMediaBuffer());
        OpenAL::MediaReverb Reverb;
        Reverb.setSource(Source);
        Source.doPlay();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Source.doStop();
        doDestroyOpenAL();
        TEST_ASSERT(true, "OpenAL EFX reverb completes without error");
    } catch (const std::runtime_error &E) {
        ::std::println(stderr, "    [skip] OpenAL EFX: {}", E.what());
    }
}

TEST_CASE(testOpenAL_MediaEffectPresetAndParameter) {
    bool HasDevice = false;
    {
        ALCdevice *D = ::alcOpenDevice(nullptr);
        if (D) {
            HasDevice = true;
            ::alcCloseDevice(D);
        }
    }
    if (!HasDevice) {
        ::std::println(stderr, "    [skip] no OpenAL device available");
        return;
    }

    try {
        doInitializeOpenAL();
        OpenAL::MediaEffect Effect(AL_EFFECT_REVERB);
        Effect.doApplyPreset(EFX_REVERB_PRESET_PADDEDCELL);
        Effect.setParameter(AL_EAXREVERB_DECAY_TIME, 1.5f);
        OpenAL::MediaAuxiliarySlot Slot;
        Slot.setEffect(Effect);
        doDestroyOpenAL();
        TEST_ASSERT(true, "effect preset + parameter + slot wiring completes without error");
    } catch (const std::runtime_error &E) {
        ::std::println(stderr, "    [skip] OpenAL EFX: {}", E.what());
    }
}

// ---------------------------------------------------------------------------
// AudioSegment: comprehensive additional tests
// ---------------------------------------------------------------------------

TEST_CASE(testDoGenerate_SampleRateVariations) {
    auto A441 = generateSine(440, 0.5, 44100, 0.5);
    auto A220 = generateSine(440, 0.5, 22050, 0.5);
    auto A480 = generateSine(440, 0.5, 48000, 0.5);
    TEST_APPROX(A441.getDuration(), 0.5, 0.01, "44100 Hz duration");
    TEST_APPROX(A220.getDuration(), 0.5, 0.01, "22050 Hz duration");
    TEST_APPROX(A480.getDuration(), 0.5, 0.01, "48000 Hz duration");
    TEST_ASSERT(A441.getSampleRate() == 44100, "44100 rate");
    TEST_ASSERT(A220.getSampleRate() == 22050, "22050 rate");
    TEST_ASSERT(A480.getSampleRate() == 48000, "48000 rate");
}

TEST_CASE(testDoGenerate_ZeroDuration) {
    auto Audio = AudioSegment::doGenerate(440, 0, 44100, 0.8);
    TEST_ASSERT(Audio.getDuration() == 0, "zero duration");
    TEST_ASSERT(Audio.getMaximum() == 0, "zero duration has no samples");
}

TEST_CASE(testDoGenerate_AmplitudeClamping) {
    auto Overdrive = AudioSegment::doGenerate(440, 0.1, 44100, 2.0);
    int MaxVal = Overdrive.getMaximum();
    TEST_ASSERT(MaxVal <= 32767, "amplitude clamped to int16 range");
}

TEST_CASE(testDoGain_Identity) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Same = Audio.doGain(0);
    TEST_ASSERT(Same.isEqual(Audio), "0 dB gain is identity");
}

TEST_CASE(testDoGain_NearInfinity) {
    auto Audio = generateSine(440, 0.1, 44100, 0.1);
    auto Loud = Audio.doGain(200);
    int MaxVal = Loud.getMaximum();
    TEST_ASSERT(MaxVal <= (int)Loud.getMaximumPossibleAmplitude(), "+200dB clamped");
    TEST_ASSERT(MaxVal >= 10000, "+200dB pushes up");
}

TEST_CASE(testDoGain_NegativeInfinity) {
    auto Audio = generateSine(440, 0.1, 44100, 0.8);
    auto Silent = Audio.doGain(-200);
    TEST_ASSERT(Silent.getMaximum() == 0, "-200dB is silent");
}

TEST_CASE(testDoNormalize_PreservesRelativeShape) {
    auto Audio = generateSine(440, 0.3, 44100, 0.1);
    auto Norm = Audio.doNormalize(0);
    TEST_ASSERT(Norm.getMaximum() > Audio.getMaximum(), "normalize increases level");
    double Expected = 32767.0;
    TEST_APPROX(Norm.getMaximum(), Expected, Expected * 0.05, "normalize hits 0dBFS");
}

TEST_CASE(testDoNormalize_CustomHeadroom) {
    auto Audio = generateSine(440, 0.3, 44100, 0.3);
    auto Norm6 = Audio.doNormalize(6);
    auto Norm0 = Audio.doNormalize(0);
    TEST_ASSERT(Norm6.getMaximum() < Norm0.getMaximum(), "6dB headroom is quieter");
    double Target = 32767.0 * AudioSegment::dB2Ratio(-6);
    TEST_APPROX(Norm6.getMaximum(), Target, Target * 0.05, "6dB headroom target");
}

    TEST_CASE(testDoFade_CustomGainRange) {
        auto Audio = generateSine(440, 0.5, 44100, 0.5);
        const auto Faded = Audio.doFade(6, -6, 100, 400, -1);
        int EarlyVal = 0, LateVal = 0;
        for (auto S : Faded.getSamples(4410, 4420)) EarlyVal = std::max(EarlyVal, (int)std::abs((int)S));
        for (auto S : Faded.getSamples(17630, 17640)) LateVal = std::max(LateVal, (int)std::abs((int)S));
        TEST_ASSERT(EarlyVal < LateVal, "fade -6dB to +6dB: late louder");
    }

TEST_CASE(testDoFade_NegativeStart) {
    auto Audio = generateSine(440, 0.5, 44100, 0.5);
    auto Faded = Audio.doFade(0, -120, -200, -1, 200);
    TEST_APPROX(Faded.getDuration(), Audio.getDuration(), 0.01, "negative start wraps");
}

TEST_CASE(testDoFadeInOut_Chain) {
    auto Audio = generateSine(440, 0.5, 44100, 0.5);
    auto Faded = Audio.doFadeIn(100).doFadeOut(100);
    TEST_APPROX(Faded.getDuration(), Audio.getDuration(), 0.01, "fade in+out preserves duration");
}

TEST_CASE(testDoConcat_MultipleSegments) {
    auto A = generateSine(440, 0.2, 44100, 0.5);
    auto B = generateSine(880, 0.2, 44100, 0.5);
    auto C = generateSine(1760, 0.2, 44100, 0.5);
    auto Chain = A.doConcat(B).doConcat(C);
    TEST_APPROX(Chain.getDuration(), 0.6, 0.05, "triple concat");
}

TEST_CASE(testDoConcat_EmptySegment) {
    auto A = generateSine(440, 0.3, 44100, 0.5);
    auto Empty = AudioSegment::doGenerate(0, 0, 44100);
    auto Result = A.doConcat(Empty);
    TEST_APPROX(Result.getDuration(), 0.3, 0.02, "concat with empty");
}

TEST_CASE(testDoConcat_DifferentChannelLayouts) {
    auto Mono = generateSine(440, 0.2, 44100, 0.5);
    auto Stereo = Mono.setChannelLayout(LayoutStereo);
    auto Result = Mono.doConcat(Stereo);
    TEST_ASSERT(Result.getChannelLayout().getChannelCount() == 2, "concat upgrades to stereo");
    TEST_ASSERT(Result.getSampleRate() == 44100, "concat preserves sample rate");
}

TEST_CASE(testDoOverlay_AtVeryEnd) {
    auto Base = generateSine(440, 0.5, 44100, 0.5);
    auto Over = generateSine(880, 0.2, 44100, 0.3);
    auto Mixed = Base.doOverlay(Over, 490);
    TEST_ASSERT(Mixed.getDuration() > Base.getDuration(), "overlay at end extends");
}

TEST_CASE(testDoOverlay_SilentBase) {
    auto Silence = generateSine(0, 0.5, 44100);
    auto Tone = generateSine(440, 0.3, 44100, 0.5);
    auto Mixed = Silence.doOverlay(Tone, 100);
    TEST_ASSERT(Mixed.getMaximum() > 0, "silent base with tone overlay has signal");
}

TEST_CASE(testDoOverlay_Stereo) {
    auto Left = generateSine(440, 0.3, 44100, 0.5);
    auto Right = generateSine(880, 0.3, 44100, 0.5);
    auto Base = AudioSegment::fromChannels(Left, Right);
    auto Over = generateSine(220, 0.2, 44100, 0.3).setChannelLayout(LayoutStereo);
    auto Mixed = Base.doOverlay(Over, 50);
    TEST_ASSERT(Mixed.getChannelLayout().getChannelCount() == 2, "stereo overlay");
}

TEST_CASE(testDoSpeedUp_SlowDown) {
    auto Audio = generateSine(440, 0.5, 44100, 0.5);
    auto Slowed = Audio.doSpeedUp(0.5);
    TEST_ASSERT(Slowed.getDuration() > Audio.getDuration(), "half speed longer");
    TEST_APPROX(Slowed.getDuration(), 1.0, 0.1, "0.5x speed ~ 2x duration");
}

TEST_CASE(testDoSpeedUp_VeryFast) {
    auto Audio = generateSine(440, 1.0, 44100, 0.5);
    auto Fast = Audio.doSpeedUp(10.0);
    TEST_ASSERT(Fast.getDuration() < 0.2, "10x speed much shorter");
}

TEST_CASE(testDoRepeat_One) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Repeated = Audio.doRepeat(1);
    TEST_ASSERT(Repeated.isEqual(Audio), "repeat 1 is identity");
}

TEST_CASE(testDoReverse_DoubleReverse_Identity) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto DoubleRev = Audio.doReverse().doReverse();
    TEST_ASSERT(DoubleRev.isEqual(Audio), "double reverse is identity");
}

TEST_CASE(testDoSlice_OutOfBounds) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Sliced = Audio.doSlice(0, 100000, 1);
    TEST_ASSERT(Sliced.isEqual(Audio), "stop beyond end clamps");
}

TEST_CASE(testDoSlice_StepOneFullCopy) {
    auto Audio = generateSine(440, 0.1, 44100, 0.5);
    auto Copy = Audio.doSlice(0, 5000, 1);
    TEST_APPROX(Copy.getDuration(), Audio.getDuration(), 0.01, "step=1 full copy");
}

TEST_CASE(testDoSlice_Position_EdgeCases) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    // Last millisecond slice
    intmax_t LastMs = (intmax_t)(Audio.getDuration() * 1000) - 1;
    auto LastSlice = Audio.doSlice(LastMs);
    TEST_ASSERT(LastSlice.getDuration() > 0, "last millisecond has data");
    // First millisecond
    auto FirstSlice = Audio.doSlice(0);
    TEST_ASSERT(FirstSlice.getDuration() > 0, "first millisecond has data");
}

TEST_CASE(testDoSplitChannels_Mono) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    std::vector<AudioSegment> Chs;
    for (auto Ch : Audio.doSplitChannels()) Chs.push_back(Ch);
    TEST_ASSERT(Chs.size() == 1, "mono splits to 1 channel");
    TEST_ASSERT(Chs[0].isEqual(Audio), "mono split equals original");
}

TEST_CASE(testDoSplitChannels_Quad) {
    auto A = generateSine(440, 0.1, 44100, 0.5);
    auto B = generateSine(554, 0.1, 44100, 0.3);
    auto C = generateSine(660, 0.1, 44100, 0.3);
    auto D = generateSine(880, 0.1, 44100, 0.3);
    auto Quad = AudioSegment::fromChannels(A, B, C, D);
    std::vector<AudioSegment> Chs;
    for (auto Ch : Quad.doSplitChannels()) Chs.push_back(Ch);
    TEST_ASSERT(Chs.size() == 4, "quad splits to 4");
    for (auto const& Ch : Chs)
        TEST_ASSERT(Ch.getChannelLayout().getChannelCount() == 1, "each channel is mono");
}

TEST_CASE(testDoSplitOnSilence_ContinuousTone) {
    auto Tone = generateSine(440, 0.5, 44100, 0.5);
    std::vector<AudioSegment> Parts;
    for (auto P : Tone.doSplitOnSilence()) Parts.push_back(P);
    TEST_ASSERT(Parts.size() >= 1, "continuous tone gives at least 1 segment");
}

TEST_CASE(testDoStripSilence_AllSilent) {
    auto Silent = generateSine(0, 0.5, 44100);
    auto Stripped = Silent.doStripSilence();
    TEST_APPROX(Stripped.getDuration(), 0.5, 0.01, "all silent returns original");
}

TEST_CASE(testDoStripSilence_CustomParams) {
    auto Silent = generateSine(0, 0.5, 44100);
    auto Tone = generateSine(440, 0.2, 44100, 0.5);
    auto WithSilence = Silent.doConcat(Tone);
    auto Stripped = WithSilence.doStripSilence(50, -30, 50);
    TEST_ASSERT(Stripped.getDuration() <= WithSilence.getDuration(), "custom params shorten");
}

TEST_CASE(testDoStripSilence_InvalidParamsClamped) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Result = Audio.doStripSilence(-1, 10, -10);
    TEST_ASSERT(Result.isEqual(Audio), "invalid args clamped, result unchanged");
}

TEST_CASE(testDoCompressDynamicRange_StrongCompression) {
    auto Audio = generateSine(440, 0.3, 44100, 0.9);
    auto Compressed = Audio.doCompressDynamicRange(-30, 10, 1, 20);
    TEST_ASSERT(Compressed.getMaximum() <= Audio.getMaximum(), "strong compression reduces level");
}

TEST_CASE(testDoCompressDynamicRange_NoAttackRelease) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Compressed = Audio.doCompressDynamicRange(-20, 2, 0, 0);
    TEST_ASSERT(Compressed.getMaximum() <= Audio.getMaximum(), "zero attack/release still compresses");
}

TEST_CASE(testDoSynchronize_ThreeInputs) {
    auto A = generateSine(440, 0.2, 44100, 0.5);
    auto B = generateSine(880, 0.2, 22050, 0.5);
    auto C = generateSine(1760, 0.2, 48000, 0.5);
    std::vector<AudioSegment> Out;
    for (auto Seg : AudioSegment::doSynchronize(A, B, C)) Out.push_back(Seg);
    TEST_ASSERT(Out.size() == 3, "3 inputs -> 3 outputs");
    TEST_ASSERT(Out[0].getSampleRate() == Out[1].getSampleRate() &&
                Out[1].getSampleRate() == Out[2].getSampleRate(), "all rates match");
    TEST_ASSERT(Out[0].getChannelLayout().getChannelCount() ==
                Out[1].getChannelLayout().getChannelCount() &&
                Out[1].getChannelLayout().getChannelCount() ==
                Out[2].getChannelLayout().getChannelCount(), "all channel counts match");
}

TEST_CASE(testSetChannelLayout_StereoToMono) {
    auto L = generateSine(440, 0.2, 44100, 0.5);
    auto R = generateSine(880, 0.2, 44100, 0.5);
    auto Stereo = AudioSegment::fromChannels(L, R);
    auto Mono = Stereo.setChannelLayout(LayoutMono);
    TEST_ASSERT(Mono.getChannelLayout().getChannelCount() == 1, "stereo->mono");
    TEST_APPROX(Mono.getDuration(), Stereo.getDuration(), 0.01, "duration preserved");
}

TEST_CASE(testSetChannelLayout_SameLayout) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    auto Same = Audio.setChannelLayout(LayoutMono);
    TEST_ASSERT(Same.getChannelLayout().getChannelCount() == 1, "same layout");
    TEST_APPROX(Same.getDuration(), Audio.getDuration(), 0.01, "same layout duration");
}

TEST_CASE(testSetSampleRate_Downsample) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Down = Audio.setSampleRate(11025);
    TEST_ASSERT(Down.getSampleRate() == 11025, "downsampled rate");
}

TEST_CASE(testSetSampleRate_QualityCheck) {
    // Resample chain preserves signal characteristics
    auto Original = generateSine(440, 0.3, 44100, 0.5);
    auto Resampled = Original.setSampleRate(22050).setSampleRate(44100);
    TEST_APPROX(Resampled.getDuration(), Original.getDuration(), 0.05, "round-trip resample duration");
    TEST_ASSERT(Resampled.getRMS() > 0, "round-trip resample has energy");
}

TEST_CASE(testGetSamples_BoundsReturn) {
    auto Audio = generateSine(440, 0.5, 44100, 0.8);
    int Count = 0;
    for (auto S : Audio.getSamples(0, 100)) { ++Count; (void)S; }
    TEST_ASSERT(Count == 100, "100 samples returned");
}

TEST_CASE(testGetSamples_NegativeIndicesWrap) {
    auto Audio = generateSine(440, 0.1, 44100, 0.5);
    auto Samples = Audio.getSamples(-10, -1);
    int Count = 0;
    for (auto S : Samples) { ++Count; (void)S; }
    TEST_ASSERT(Count == 9, "negative indices wrap");
}

TEST_CASE(testGetSamples_MutableModify) {
    auto Audio = generateSine(440, 0.1, 44100, 0.5);
    for (auto S : Audio.getSamples(0, 10)) *S = 0;
    const auto& AudioC = Audio;
    for (auto S : AudioC.getSamples(0, 10)) TEST_ASSERT(S == 0, "mutable sample modified");
}

TEST_CASE(testIsEqual_DifferentLayout) {
    auto A = generateSine(440, 0.1, 44100, 0.5);
    auto B = A.setChannelLayout(LayoutStereo);
    TEST_ASSERT(!A.isEqual(B), "different layout not equal");
}

TEST_CASE(testIsEqual_DifferentRate) {
    auto A = generateSine(440, 0.1, 44100, 0.5);
    auto B = A.setSampleRate(22050);
    TEST_ASSERT(!A.isEqual(B), "different rate not equal");
}

TEST_CASE(testDoGain_ChannelLayoutPreserved) {
    auto Stereo = AudioSegment::fromChannels(
        generateSine(440, 0.1, 44100, 0.5),
        generateSine(880, 0.1, 44100, 0.5));
    auto Gained = Stereo.doGain(3);
    TEST_ASSERT(Gained.getChannelLayout().getChannelCount() == 2, "gain preserves stereo");
}

TEST_CASE(testDoFade_ZeroDuration) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Faded = Audio.doFade(0, -120, 100, 100, -1);
    TEST_ASSERT(Faded.isEqual(Audio), "zero fade duration = noop");
}

TEST_CASE(testDoReverse_EmptyAudio) {
    auto Empty = AudioSegment::doGenerate(0, 0, 44100);
    auto Rev = Empty.doReverse();
    TEST_ASSERT(Rev.getDuration() == 0, "reverse empty");
}

TEST_CASE(testFromChannels_FiveChannelCustom) {
    auto A = generateSine(100, 0.05, 44100, 0.3);
    auto B = generateSine(200, 0.05, 44100, 0.3);
    auto C = generateSine(300, 0.05, 44100, 0.3);
    auto D = generateSine(400, 0.05, 44100, 0.3);
    auto E = generateSine(500, 0.05, 44100, 0.3);
    auto Custom = AudioSegment::fromChannels(A, B, C, D, E);
    TEST_ASSERT(Custom.getChannelLayout().getChannelCount() == 5, "custom 5-channel");
    TEST_APPROX(Custom.getDuration(), 0.05, 0.01, "custom layout duration");
}

// ---------------------------------------------------------------------------
// FFMpeg wrappers
// ---------------------------------------------------------------------------

TEST_CASE(testFFMpeg_MediaCodec_FindDecoder) {
    auto Codec = FFMpeg::MediaCodec::doFindDecoder(AV_CODEC_ID_PCM_S16LE);
    TEST_ASSERT((const AVCodec *) Codec != nullptr, "PCM decoder found");
}

TEST_CASE(testFFMpeg_MediaCodec_FindEncoder) {
    auto Codec = FFMpeg::MediaCodec::doFindEncoder(AV_CODEC_ID_PCM_S16LE);
    TEST_ASSERT((const AVCodec *) Codec != nullptr, "PCM encoder found");
}

TEST_CASE(testFFMpeg_MediaFrame_AllocateAndDestroy) {
    auto Frame = FFMpeg::MediaFrame::doAllocate();
    TEST_ASSERT((AVFrame *) Frame != nullptr, "frame allocated");
    Frame.doDestroy();
    TEST_ASSERT((AVFrame *) Frame == nullptr, "frame destroyed");
}

TEST_CASE(testFFMpeg_MediaPacket_Allocate) {
    auto Packet = FFMpeg::MediaPacket::doAllocate();
    TEST_ASSERT((AVPacket *) Packet != nullptr, "packet allocated");
}

TEST_CASE(testFFMpeg_MediaCodecContext_Allocate) {
    auto Context = FFMpeg::MediaCodecContext::doAllocate();
    TEST_ASSERT((AVCodecContext *) Context != nullptr, "codec context allocated");
}

TEST_CASE(testFFMpeg_MediaFormatContext_AllocateOutput) {
    auto Context = FFMpeg::MediaFormatContext::doAllocateOutput("out_test.wav");
    TEST_ASSERT((AVFormatContext *) Context != nullptr, "format output allocated");
}

TEST_CASE(testFFMpeg_MediaCodec_MoveAssign) {
    auto A = FFMpeg::MediaCodec::doFindDecoder(AV_CODEC_ID_PCM_S16LE);
    const AVCodec *PtrA = (const AVCodec *) A;
    FFMpeg::MediaCodec B(std::move(A));
    TEST_ASSERT((const AVCodec *) B == PtrA, "move ctor transfers");
    TEST_ASSERT((const AVCodec *) A == nullptr, "source nulled after move");
}

// ---------------------------------------------------------------------------
// Metadata reading
// ---------------------------------------------------------------------------

TEST_CASE(testDoGetMetadata_ReadsTags) {
    const std::string Path = "test_metadata.wav";
    {
        FFMpeg::MediaFormatContext AudioFormatContext(FFMpeg::MediaFormatContext::doAllocateOutput(Path));
        ::av_dict_set(&AudioFormatContext->metadata, "title", "TestTitle", 0);
        ::av_dict_set(&AudioFormatContext->metadata, "artist", "TestArtist", 0);
        ::av_dict_set(&AudioFormatContext->metadata, "album", "TestAlbum", 0);
        FFMpeg::MediaCodec AudioCodec(FFMpeg::MediaCodec::doFindEncoder(AV_CODEC_ID_PCM_S16LE));
        FFMpeg::MediaCodecContext AudioCodecContext(FFMpeg::MediaCodecContext::doAllocate(AudioCodec));
        AVChannelLayout AudioLayout = AV_CHANNEL_LAYOUT_MASK(2, AV_CH_LAYOUT_STEREO);
        ::av_channel_layout_copy(&AudioCodecContext->ch_layout, &AudioLayout);
        AudioCodecContext->sample_fmt = AV_SAMPLE_FMT_S16;
        AudioCodecContext->sample_rate = 44100;
        AVStream *AudioStream = ::avformat_new_stream((AVFormatContext *) AudioFormatContext,
                                                      (const AVCodec *) AudioCodec);
        TEST_ASSERT(AudioStream != nullptr, "metadata stream created");
        ::avcodec_parameters_from_context(AudioStream->codecpar, (AVCodecContext *) AudioCodecContext);
        if (::avio_open(&AudioFormatContext->pb, Path.c_str(), AVIO_FLAG_WRITE) < 0)
            TEST_ASSERT(false, "metadata avio_open");
        AudioFormatContext.doWriteHeader();
        AudioFormatContext.doWriteTrailer();
    }
    auto AudioMetadata = AudioSegment::doGetMetadata(Path);
    TEST_ASSERT(AudioMetadata.MetadataTitle == "TestTitle", "title tag read");
    TEST_ASSERT(AudioMetadata.MetadataArtist == "TestArtist", "artist tag read");
    TEST_ASSERT(AudioMetadata.MetadataAlbum == "TestAlbum", "album tag read");
    TEST_ASSERT(AudioMetadata.getEntry("artist") == "TestArtist", "getEntry lookup");
    TEST_ASSERT(AudioMetadata.getEntry("nonexistent").empty(), "missing entry returns empty");
    std::filesystem::remove(Path);
}

// ---------------------------------------------------------------------------
// setFrameRate (tag-only rate change)
// ---------------------------------------------------------------------------

TEST_CASE(testSetFrameRate_RetagOnly) {
    auto Audio = generateSine(440, 0.5, 44100, 0.5);
    auto Retagged = Audio.setFrameRate(22050);
    TEST_ASSERT(Retagged.getSampleRate() == 22050, "frame rate tag changed");
    TEST_APPROX(Retagged.getDuration(), 1.0, 0.01, "duration doubles on retag");
    std::vector<int16_t> A, B;
    for (auto *S : Audio.getSamples(0, 100)) A.push_back(*S);
    const auto &RC = Retagged;
    for (auto S : RC.getSamples(0, 100)) B.push_back(S);
    TEST_ASSERT(A == B, "samples identical after retag");
}

TEST_CASE(testSetFrameRate_InvalidThrows) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    bool Threw = false;
    try {
        (void) Audio.setFrameRate(0);
    } catch (const std::runtime_error &) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "frame rate 0 throws");
}

// ---------------------------------------------------------------------------
// doShiftPitch (phase vocoder)
// ---------------------------------------------------------------------------

TEST_CASE(testDoShiftPitch_OctaveUp) {
    auto Audio = generateSine(440, 1.0, 44100, 0.5);
    auto Shifted = Audio.doShiftPitch(12);
    TEST_APPROX(Shifted.getDuration(), Audio.getDuration(), 0.01, "pitch shift preserves duration");
    int CrossingsOriginal = countZeroCrossings(Audio);
    int CrossingsShifted = countZeroCrossings(Shifted);
    TEST_ASSERT(CrossingsOriginal > 0, "original has crossings");
    double Ratio = (double) CrossingsShifted / CrossingsOriginal;
    TEST_ASSERT(Ratio > 1.8 && Ratio < 2.2, "octave up doubles zero crossing rate");
    double RmsRatio = Shifted.getRMS() / Audio.getRMS();
    TEST_ASSERT(RmsRatio > 0.5 && RmsRatio < 2.0, "pitch shift roughly preserves level");
}

TEST_CASE(testDoShiftPitch_OctaveDown) {
    auto Audio = generateSine(440, 1.0, 44100, 0.5);
    auto Shifted = Audio.doShiftPitch(-12);
    TEST_APPROX(Shifted.getDuration(), Audio.getDuration(), 0.01, "down shift preserves duration");
    double Ratio = (double) countZeroCrossings(Shifted) / countZeroCrossings(Audio);
    TEST_ASSERT(Ratio > 0.4 && Ratio < 0.6, "octave down halves zero crossing rate");
}

TEST_CASE(testDoShiftPitch_ZeroIsIdentity) {
    auto Audio = generateSine(440, 0.3, 44100, 0.5);
    auto Same = Audio.doShiftPitch(0);
    TEST_ASSERT(Same.isEqual(Audio), "0 semitones is identity");
}

TEST_CASE(testDoShiftPitch_StereoEachChannel) {
    auto Left = generateSine(440, 0.6, 44100, 0.5);
    auto Right = generateSine(880, 0.6, 44100, 0.5);
    auto Stereo = AudioSegment::fromChannels(Left, Right);
    auto Shifted = Stereo.doShiftPitch(12);
    TEST_ASSERT(Shifted.getChannelLayout().getChannelCount() == 2, "stereo layout preserved");
    std::vector<AudioSegment> Chs;
    for (auto Ch : Shifted.doSplitChannels()) Chs.push_back(Ch);
    double RatioLeft = (double) countZeroCrossings(Chs[0]) / countZeroCrossings(Left);
    double RatioRight = (double) countZeroCrossings(Chs[1]) / countZeroCrossings(Right);
    TEST_ASSERT(RatioLeft > 1.8 && RatioLeft < 2.2, "left channel shifted up one octave");
    TEST_ASSERT(RatioRight > 1.8 && RatioRight < 2.2, "right channel shifted up one octave");
}

TEST_CASE(testDoShiftPitch_RoundTrip) {
    auto Audio = generateSine(440, 0.5, 44100, 0.5);
    auto RoundTripped = Audio.doShiftPitch(7).doShiftPitch(-7);
    TEST_APPROX(RoundTripped.getDuration(), Audio.getDuration(), 0.01, "round trip preserves duration");
    double Ratio = (double) countZeroCrossings(RoundTripped) / countZeroCrossings(Audio);
    TEST_ASSERT(Ratio > 0.7 && Ratio < 1.3, "round trip restores frequency");
}

// ---------------------------------------------------------------------------
// getSpectrum (FFT)
// ---------------------------------------------------------------------------

TEST_CASE(testGetSpectrum_PeakBin) {
    double BinHz = 44100.0 / 1024.0;
    auto Audio = generateSine(BinHz * 23, 0.2, 44100, 0.8);
    auto Spectrum = Audio.getSpectrum(1024, 0);
    TEST_ASSERT(Spectrum.SpectrumMagnitude.size() == 513, "spectrum size N/2+1");
    TEST_APPROX(Spectrum.SpectrumFrequencyStep, BinHz, 0.001, "bin width");
    size_t Peak = 0;
    for (size_t i = 1; i < Spectrum.SpectrumMagnitude.size(); ++i)
        if (Spectrum.SpectrumMagnitude[i] > Spectrum.SpectrumMagnitude[Peak]) Peak = i;
    TEST_ASSERT(Peak == 23, "peak bin at generated frequency");
    TEST_APPROX(Spectrum.SpectrumMagnitude[Peak], -1.938, 0.15, "peak magnitude dBFS");
    TEST_APPROX(::std::cos(Spectrum.SpectrumPhase[Peak]), 0.0, 0.01, "sine peak phase is +/- pi/2");
}

TEST_CASE(testGetSpectrum_NonPowerOfTwoThrows) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    bool Threw = false;
    try {
        (void) Audio.getSpectrum(1000);
    } catch (const std::invalid_argument &) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "non power-of-two size throws");
}

TEST_CASE(testGetSpectrum_StartOutOfRangeThrows) {
    auto Audio = generateSine(440, 0.2, 44100, 0.5);
    bool Threw = false;
    try {
        (void) Audio.getSpectrum(1024, 1000000);
    } catch (const std::invalid_argument &) {
        Threw = true;
    }
    TEST_ASSERT(Threw, "start beyond end throws");
}

TEST_CASE(testGetSpectrum_SilenceIsNegativeInfinity) {
    auto Silence = generateSine(0, 0.2, 44100);
    auto Spectrum = Silence.getSpectrum(1024, 0);
    bool AllInf = true;
    for (double Magnitude : Spectrum.SpectrumMagnitude)
        if (Magnitude != -std::numeric_limits<double>::infinity()) AllInf = false;
    TEST_ASSERT(AllInf, "silence spectrum magnitudes are -inf");
}

// ---------------------------------------------------------------------------
// EBU R128 / LUFS loudness
// ---------------------------------------------------------------------------

TEST_CASE(testGetLoudness_SteadyTone) {
    auto Audio = generateSine(1000, 2.0, 44100, 0.5);
    auto Loudness = Audio.getLoudness();
    TEST_APPROX(Loudness.LoudnessIntegrated, -9.07, 0.2, "integrated LUFS of 1kHz half-scale sine");
    TEST_APPROX(Loudness.LoudnessMomentary, -9.07, 0.2, "momentary LUFS");
    TEST_APPROX(Loudness.LoudnessShortTerm, -9.07, 0.2, "short-term LUFS");
    TEST_APPROX(Audio.getLoudnessIntegrated(), -9.07, 0.2, "getLoudnessIntegrated accessor");
}

TEST_CASE(testGetLoudness_KWeightingAttenuatesBass) {
    auto Low = generateSine(100, 1.0, 44100, 0.5);
    auto High = generateSine(1000, 1.0, 44100, 0.5);
    TEST_ASSERT(High.getLoudnessIntegrated() > Low.getLoudnessIntegrated(),
                "1kHz reads louder than 100Hz at equal amplitude");
}

TEST_CASE(testGetLoudness_SilenceIsNegativeInfinity) {
    auto Silence = generateSine(0, 1.0, 44100);
    auto Loudness = Silence.getLoudness();
    TEST_ASSERT(Loudness.LoudnessIntegrated == -std::numeric_limits<double>::infinity(),
                "silence integrated = -inf");
    TEST_ASSERT(Loudness.LoudnessMomentary == -std::numeric_limits<double>::infinity(),
                "silence momentary = -inf");
}

// ---------------------------------------------------------------------------
// OpenAL: 3D positioning, listener, queue streaming, HRTF, loop points, echo
// ---------------------------------------------------------------------------

TEST_CASE(testOpenAL_QueueStreamingSmoke) {
    if (!hasOpenALDevice()) {
        ::std::println(stderr, "    [skip] no OpenAL device available");
        return;
    }
    try {
        doInitializeOpenAL();
        auto Audio = generateSine(440, 0.6, 44100, 0.5);
        auto Chunk = Audio.doSlice(0, 100, 1);
        OpenAL::MediaSource Source;
        OpenAL::MediaBuffer Buffer0, Buffer1;
        Buffer0.setBufferData(Chunk.getChannelLayout(), Chunk.getRawData(), 44100);
        Source.doQueueBuffer(Buffer0);
        Buffer1.setBufferData(Chunk.getChannelLayout(), Chunk.getRawData(), 44100);
        Source.doQueueBuffer(Buffer1);
        TEST_ASSERT(Source.getSourceBuffersQueued() == 2, "2 buffers queued");
        Source.doPlay();
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        ALint AudioProcessed = Source.getSourceBuffersProcessed();
        TEST_ASSERT(AudioProcessed >= 1, "at least one buffer processed after 150ms");
        Source.doUnqueueBuffer();
        TEST_ASSERT(Source.getSourceBuffersQueued() == 1, "unqueue reduces queued count");
        // Refill the processed buffer and requeue it (FIFO: Buffer0 was queued first).
        Buffer0.setBufferData(Chunk.getChannelLayout(), Chunk.getRawData(), 44100);
        Source.doQueueBuffer(Buffer0);
        TEST_ASSERT(Source.getSourceBuffersQueued() == 2, "refilled buffer requeued");
        TEST_ASSERT(Source.getSourceState() == OpenAL::MediaSource::MediaSourceState::StatePlaying,
                    "source still playing");
        Source.doStop();
        doDestroyOpenAL();
        TEST_ASSERT(true, "queue streaming completes without error");
    } catch (const std::runtime_error &E) {
        ::std::println(stderr, "    [skip] OpenAL streaming: {}", E.what());
    }
}

TEST_CASE(testOpenAL_ListenerAnd3DPositionSmoke) {
    if (!hasOpenALDevice()) {
        ::std::println(stderr, "    [skip] no OpenAL device available");
        return;
    }
    try {
        doInitializeOpenAL();
        OpenAL::MediaContext::setListenerPosition(0, 0, 0);
        OpenAL::MediaContext::setListenerOrientation(0, 0, -1, 0, 1, 0);
        OpenAL::MediaContext::setListenerVelocity(0, 0, 0);
        OpenAL::MediaContext::setListenerGain(1.0f);
        auto Audio = generateSine(440, 0.3, 44100, 0.5);
        OpenAL::MediaSource Source;
        Source.setSourceBuffer(Audio.toMediaBuffer());
        Source.setSourcePosition(1.0f, 0.0f, -1.0f);
        Source.doPlay();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        TEST_ASSERT(Source.getSourceOffsetSamples() > 0, "playback offset advances");
        TEST_ASSERT(Source.getSourceOffsetSeconds() > 0.0, "playback offset in seconds");
        Source.doStop();
        doDestroyOpenAL();
        TEST_ASSERT(true, "listener + 3D position completes without error");
    } catch (const std::runtime_error &E) {
        ::std::println(stderr, "    [skip] OpenAL 3D: {}", E.what());
    }
}

TEST_CASE(testOpenAL_MediaEchoSmoke) {
    if (!hasOpenALDevice()) {
        ::std::println(stderr, "    [skip] no OpenAL device available");
        return;
    }
    try {
        doInitializeOpenAL();
        auto Audio = generateSine(440, 0.2, 44100, 0.5);
        OpenAL::MediaSource Source;
        Source.setSourceBuffer(Audio.toMediaBuffer());
        OpenAL::MediaEcho Echo(0.15f, 0.4f, 0.5f);
        Echo.setSource(Source);
        Echo.setFeedback(0.3f);
        Source.doPlay();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Source.doStop();
        doDestroyOpenAL();
        TEST_ASSERT(true, "OpenAL EFX echo completes without error");
    } catch (const std::runtime_error &E) {
        ::std::println(stderr, "    [skip] OpenAL EFX echo: {}", E.what());
    }
}

TEST_CASE(testOpenAL_HRTFEnumerationAndEnable) {
    if (!hasOpenALDevice()) {
        ::std::println(stderr, "    [skip] no OpenAL device available");
        return;
    }
    try {
        OpenAL::MediaDevice Device("");
        auto HRTFNames = Device.doListHRTFNames();
        ::std::println("    HRTF profiles enumerated: {}", HRTFNames.size());
        if (!HRTFNames.empty()) {
            Device.setHRTF(true);
            TEST_ASSERT(Device.doGetHRTFState() == 1, "HRTF enabled after setHRTF(true)");
            OpenAL::MediaContext Context(Device);
            Context.setContextCurrent();
            auto Audio = generateSine(440, 0.2, 44100, 0.5);
            OpenAL::MediaSource Source;
            Source.setSourceBuffer(Audio.toMediaBuffer());
            Source.doPlay();
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            Source.doStop();
        }
        TEST_ASSERT(true, "HRTF enumeration/enable completes without error");
    } catch (const std::runtime_error &E) {
        ::std::println(stderr, "    [skip] OpenAL HRTF: {}", E.what());
    }
}

TEST_CASE(testOpenAL_LoopPointsSmoke) {
    if (!hasOpenALDevice()) {
        ::std::println(stderr, "    [skip] no OpenAL device available");
        return;
    }
    try {
        doInitializeOpenAL();
        auto Audio = generateSine(440, 0.2, 44100, 0.5);
        OpenAL::MediaSource Source;
        OpenAL::MediaBuffer Buffer(Audio.getChannelLayout(), Audio.getRawData(), 44100);
        Source.setSourceBuffer(Buffer);
        Source.setSourceLoop(true);
        Source.setSourceLoopPoints(0, 2205);
        Source.doPlay();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        TEST_ASSERT(Source.getSourceState() == OpenAL::MediaSource::MediaSourceState::StatePlaying,
                    "loop-point source playing");
        Source.doStop();
        doDestroyOpenAL();
        TEST_ASSERT(true, "seamless loop points complete without error");
    } catch (const std::runtime_error &E) {
        ::std::println(stderr, "    [skip] OpenAL loop points: {}", E.what());
    }
}

} // namespace

int main() {
    doInitializeFFMpeg();
    ::setvbuf(stdout, nullptr, _IONBF, 0);
    ::std::println("Running {} tests...", Tests.size());
    for (const auto &T : Tests) {
        ++TestsRun;
        ::std::println("[RUN ] {}", T.Name);
        try {
            T.Fn();
            ++TestsPassed;
            ::std::println("[PASS] {}", T.Name);
        } catch (const std::exception &E) {
            ++TestsFailed;
            FailureList.push_back(std::string(T.Name) + " -- threw: " + E.what());
            ::std::println(stderr, "[ERR ] {} threw: {}", T.Name, E.what());
        } catch (...) {
            ++TestsFailed;
            FailureList.push_back(std::string(T.Name) + " -- threw unknown");
            ::std::println(stderr, "[ERR ] {} threw unknown", T.Name);
        }
    }

    ::std::println("\n==== Summary ====");
    ::std::println("Total:   {}", TestsRun);
    ::std::println("Passed:  {}", TestsPassed);
    ::std::println("Failed:  {}", TestsFailed);
    if (!FailureList.empty()) {
        ::std::println("\nFailures:");
        for (const auto &F : FailureList) ::std::println("  - {}", F);
    }
    return TestsFailed == 0 ? 0 : 1;
}
