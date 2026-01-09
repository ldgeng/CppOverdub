#include "../CppOverdub.hpp"
#include <print>
#include <thread>

int main() {
    doInitializeFFMpeg();
    doInitializeOpenAL();
    AudioSegment SegmentObject(AudioSegment::doOpen("bgm01.wav"));
    OpenAL::MediaSource SegmentSource;
    SegmentSource.setSourceBuffer(SegmentObject.toMediaBuffer());
    SegmentSource.doPlay();
    std::this_thread::sleep_for(std::chrono::seconds(100L));
    doDestroyOpenAL();
    return 0;
}