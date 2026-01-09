#ifndef CPPOVERDUB_LIBRARY_H
#define CPPOVERDUB_LIBRARY_H

#include <algorithm>
#include <cmath>
#include <generator>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

extern "C" {
#include "AL/alext.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavfilter/avfilter.h"
#include "libswresample/swresample.h"
};

class NonConstructable {
public:
    constexpr NonConstructable() noexcept = delete;
};

class NonCopyAssignable {
protected:
    constexpr NonCopyAssignable() noexcept = default;

public:
    NonCopyAssignable &operator=(const NonCopyAssignable &) noexcept = delete;
};

class NonCopyConstructable {
protected:
    constexpr NonCopyConstructable() noexcept = default;

public:
    constexpr NonCopyConstructable(const NonCopyConstructable &) noexcept = delete;
};

class NonCopyable : public NonCopyAssignable, public NonCopyConstructable {
};

#define doEnableCopyAssignConstruct(ClassName) ClassName(const ClassName &ObjectSource) {doAssign(ObjectSource);}ClassName &operator=(const ClassName &ObjectSource) {doAssign(ObjectSource);return *this;}
#define doEnableCopyAssignParameterConstruct(ClassName, ParameterName) ClassName(const ParameterName &ObjectSource) {doAssign(ObjectSource);}ClassName &operator=(const ParameterName &ObjectSource) {doAssign(ObjectSource);return *this;}
#define doEnableMoveAssignConstruct(ClassName) ClassName(ClassName &&ObjectSource) noexcept {doAssign(::std::move(ObjectSource));}ClassName &operator=(ClassName &&ObjectSource) noexcept {doAssign(::std::move(ObjectSource));return *this;}
#define doEnableMoveAssignParameterConstruct(ClassName, ParameterName) ClassName(ParameterName &&ObjectSource) noexcept {doAssign(Objects::doMove(ObjectSource));}ClassName &operator=(ParameterName &&ObjectSource) noexcept {doAssign(::std::move(ObjectSource));return *this;}
#define doEnableValueAssignParameterConstruct(ClassName, ParameterName) ClassName(ParameterName ObjectSource) {doAssign(ObjectSource);}ClassName &operator=(ParameterName ObjectSource) {doAssign(ObjectSource);return *this;}
#define doThrowChecked(ExceptionType, ...) throw ExceptionType(__VA_ARGS__)
#define doThrowUnchecked(...) throw __VA_ARGS__

class MediaChannelLayout final {
private:
    uint8_t LayoutChannelCount;
    uint64_t LayoutChannelMask;

public:
    constexpr MediaChannelLayout() noexcept : LayoutChannelCount(0), LayoutChannelMask(0) {
    }

    constexpr
    MediaChannelLayout(uint8_t LayoutChannelCountSource,
                       uint64_t LayoutChannelMaskSource) noexcept : LayoutChannelCount(LayoutChannelCountSource),
                                                                    LayoutChannelMask(LayoutChannelMaskSource) {
    }

    constexpr MediaChannelLayout(const AVChannelLayout &LayoutSource) noexcept : LayoutChannelCount(
        (uint8_t) LayoutSource.nb_channels), LayoutChannelMask(LayoutSource.u.mask) {
        if (!LayoutChannelMask) LayoutChannelMask = AV_CH_LAYOUT_STEREO;
    }

    uint8_t getChannelCount() const noexcept {
        return LayoutChannelCount;
    }

    uint64_t getChannelMask() const noexcept {
        return LayoutChannelMask;
    }

    AVChannelLayout toFFMpegFormat() const noexcept {
        return AV_CHANNEL_LAYOUT_MASK(LayoutChannelCount, LayoutChannelMask);
    }

    auto toOpenALFormat() const {
        switch (LayoutChannelMask) {
            case AV_CH_LAYOUT_5POINT1:
                return AL_FORMAT_51CHN16;
            case AV_CH_LAYOUT_6POINT1:
                return AL_FORMAT_61CHN16;
            case AV_CH_LAYOUT_7POINT1:
                return AL_FORMAT_71CHN16;
            case AV_CH_LAYOUT_MONO:
                return AL_FORMAT_MONO16;
            case AV_CH_LAYOUT_QUAD:
                return AL_FORMAT_QUAD16;
            case AV_CH_LAYOUT_STEREO:
                return AL_FORMAT_STEREO16;
        }
        doThrowChecked(::std::runtime_error, "MediaChannelLayout::toOpenALFormat() Unsupported channel layout");
    }
};

static const MediaChannelLayout Layout51{6, AV_CH_LAYOUT_5POINT1};
static const MediaChannelLayout Layout61{7, AV_CH_LAYOUT_6POINT1};
static const MediaChannelLayout Layout71{8, AV_CH_LAYOUT_7POINT1};
static const MediaChannelLayout LayoutMono{1, AV_CH_LAYOUT_MONO};
static const MediaChannelLayout LayoutQuad{4, AV_CH_LAYOUT_QUAD};
static const MediaChannelLayout LayoutStereo{2, AV_CH_LAYOUT_STEREO};

namespace FFMpeg {
    class MediaCodec final : public NonCopyable {
    private:
        const AVCodec *CodecObject = nullptr;

        constexpr MediaCodec(const AVCodec *CodecSource) noexcept: CodecObject(CodecSource) {
        }

        friend class MediaCodecContext;

    public:
        doEnableMoveAssignConstruct(MediaCodec)

        ~MediaCodec() noexcept {
            if (CodecObject) CodecObject = nullptr;
        }

        void doAssign(MediaCodec &&CodecSource) noexcept {
            if (::std::addressof(CodecSource) == this) return;
            CodecObject = CodecSource.CodecObject;
            CodecSource.CodecObject = nullptr;
        }

        static MediaCodec doFindDecoder(const AVCodecID CodecID) {
            const AVCodec *CodecObject = ::avcodec_find_decoder(CodecID);
            if (!CodecObject)
                doThrowChecked(::std::runtime_error, "MediaCodec::doFindDecoder(const AVCodecID) ::avcodec_find_decoder");
            return {CodecObject};
        }

        static MediaCodec doFindEncoder(const AVCodecID CodecID) {
            const AVCodec *CodecObject = ::avcodec_find_encoder(CodecID);
            if (!CodecObject)
                doThrowChecked(::std::runtime_error, "MediaCodec::doFindEncoder(const AVCodecID) ::avcodec_find_encoder");
            return {CodecObject};
        }

        explicit operator const AVCodec *() const noexcept {
            return CodecObject;
        }

        const AVCodec *operator->() noexcept {
            return CodecObject;
        }
    };

    class MediaFrame final : public NonCopyable {
    private:
        AVFrame *FrameObject = nullptr;

        constexpr MediaFrame(AVFrame *FrameSource) noexcept: FrameObject(FrameSource) {
        }

        friend class MediaCodecContext;

    public:
        doEnableMoveAssignConstruct(MediaFrame)

        ~MediaFrame() noexcept {
            doDestroy();
        }

        static MediaFrame doAllocate() {
            AVFrame *FrameObject = ::av_frame_alloc();
            if (!FrameObject)
                doThrowChecked(::std::runtime_error, "MediaFrame::doAllocate() ::av_frame_alloc");
            return {FrameObject};
        }

        void doAssign(MediaFrame &&FrameSource) noexcept {
            if (::std::addressof(FrameSource) == this) return;
            FrameObject = FrameSource.FrameObject;
            FrameSource.FrameObject = nullptr;
        }

        void doDestroy() noexcept {
            if (FrameObject) {
                ::av_frame_free(&FrameObject);
                FrameObject = nullptr;
            }
        }

        void getFrameBuffer() const {
            if (::av_frame_get_buffer(FrameObject, 0))
                doThrowChecked(::std::runtime_error, "MediaFrame::getFrameBuffer() ::av_frame_get_buffer");
        }

        explicit operator AVFrame *() const noexcept {
            return FrameObject;
        }

        AVFrame *operator->() noexcept {
            return FrameObject;
        }
    };

    class MediaPacket final : public NonCopyable {
    private:
        AVPacket *PacketObject = nullptr;

        constexpr MediaPacket(AVPacket *PacketSource) noexcept: PacketObject(PacketSource) {
        }

        friend class MediaCodecContext;
        friend class MediaFormatContext;

    public:
        doEnableMoveAssignConstruct(MediaPacket)

        ~MediaPacket() noexcept {
            if (PacketObject) {
                ::av_packet_free(&PacketObject);
                PacketObject = nullptr;
            }
        }

        static MediaPacket doAllocate() {
            AVPacket *PacketObject = ::av_packet_alloc();
            if (!PacketObject)
                doThrowChecked(::std::runtime_error, "MediaPacket::doAllocate() ::av_packet_alloc");
            return {PacketObject};
        }

        void doAssign(MediaPacket &&PacketSource) noexcept {
            if (::std::addressof(PacketSource) == this) return;
            PacketObject = PacketSource.PacketObject;
            PacketSource.PacketObject = nullptr;
        }

        explicit operator AVPacket *() const noexcept {
            return PacketObject;
        }

        AVPacket *operator->() noexcept {
            return PacketObject;
        }
    };

    class MediaSWRContext final : public NonCopyable {
    private:
        SwrContext *ContextObject = nullptr;

        constexpr MediaSWRContext(SwrContext *ContextSource) noexcept: ContextObject(ContextSource) {
        }

    public:
        doEnableMoveAssignConstruct(MediaSWRContext)

        ~MediaSWRContext() noexcept {
            if (ContextObject) {
                swr_free(&ContextObject);
                ContextObject = nullptr;
            }
        }

        static MediaSWRContext doAllocate(AVChannelLayout *ContextChannelLayoutInput,
                                          AVChannelLayout *ContextChannelLayoutOutput,
                                          AVSampleFormat ContextSampleFormatInput,
                                          AVSampleFormat ContextSampleFormatOutput, int ContextSampleRateInput,
                                          int ContextSampleRateOutput) {
            SwrContext *ContextObject = nullptr;
            if (::swr_alloc_set_opts2(&ContextObject, ContextChannelLayoutOutput, ContextSampleFormatOutput,
                                    ContextSampleRateOutput, ContextChannelLayoutInput, ContextSampleFormatInput,
                                    ContextSampleRateInput, 0, nullptr))
                doThrowChecked(::std::runtime_error,
                           "MediaSWRContext::doAllocate(AVChannelLayout*, AVChannelLayout, enum AVSampleFormat, enum AVSampleFormat, int, int) ::swr_alloc_set_opts2");
            return {ContextObject};
        }

        void doAssign(MediaSWRContext &&ContextSource) noexcept {
            if (::std::addressof(ContextSource) == this) return;
            ContextObject = ContextSource.ContextObject;
            ContextSource.ContextObject = nullptr;
        }

        void doConvert(const uint8_t **ContextInput, int ContextInputSize, uint8_t **ContextOutput,
                       int ContextOutputSize) {
            if (::swr_convert(ContextObject, ContextOutput, ContextOutputSize, ContextInput, ContextInputSize) < 0)
                doThrowChecked(::std::runtime_error,
                           "MediaSWRContext::doConvert(const uint8_t**, int, uint8_t**, int) ::swr_convert");
        }

        void doInitialize() {
            if (::swr_init(ContextObject))
                doThrowChecked(::std::runtime_error, "MediaSWRContext::doInitialize() ::swr_init");
        }

        explicit operator SwrContext *() const noexcept {
            return ContextObject;
        }
    };

    class MediaCodecContext final : public NonCopyable {
    private:
        AVCodecContext *ContextObject = nullptr;

        constexpr MediaCodecContext(AVCodecContext *ContextSource) noexcept: ContextObject(ContextSource) {
        }

    public:
        doEnableMoveAssignConstruct(MediaCodecContext)

        ~MediaCodecContext() noexcept {
            if (ContextObject) {
                avcodec_close(ContextObject);
                avcodec_free_context(&ContextObject);
                ContextObject = nullptr;
            }
        }

        static MediaCodecContext doAllocate() {
            AVCodecContext *CodecContextObject = ::avcodec_alloc_context3(nullptr);
            if (!CodecContextObject)
                doThrowChecked(::std::runtime_error, "MediaCodecContext::doAllocate() ::avcodec_alloc_context3");
            return {CodecContextObject};
        }

        static MediaCodecContext doAllocate(const MediaCodec &ContextCodecSource) {
            AVCodecContext *CodecContextObject = ::avcodec_alloc_context3(ContextCodecSource.CodecObject);
            if (!CodecContextObject)
                doThrowChecked(::std::runtime_error,
                           "MediaCodecContext::doAllocate(const MediaCodec&) ::avcodec_alloc_context3");
            return {CodecContextObject};
        }

        void doAssign(MediaCodecContext &&ContextSource) noexcept {
            if (::std::addressof(ContextSource) == this) return;
            ContextObject = ContextSource.ContextObject;
            ContextSource.ContextObject = nullptr;
        }

        void doOpen(const MediaCodec &ContextCodecSource) {
            if (::avcodec_open2(ContextObject, ContextCodecSource.CodecObject, nullptr))
                doThrowChecked(::std::runtime_error, "MediaCodecContext::doOpen(const MediaCodec&) ::avcodec_open2");
        }

        void doSendFrame(const MediaFrame &ContextFrameSource) {
            if (::avcodec_send_frame(ContextObject, ContextFrameSource.FrameObject))
                doThrowChecked(::std::runtime_error, "MediaCodecContext::doSendFrame(const MediaFrame&) ::avcodec_send_frame");
        }

        void doSendPacket(const MediaPacket &ContextPacketSource) {
            if (::avcodec_send_packet(ContextObject, ContextPacketSource.PacketObject))
                doThrowChecked(::std::runtime_error,
                           "MediaCodecContext::doSendPacket(const MediaPacket&) ::avcodec_send_packet");
        }

        explicit operator AVCodecContext *() const noexcept {
            return ContextObject;
        }

        AVCodecContext *operator->() noexcept {
            return ContextObject;
        }

        void setParameter(AVCodecParameters *ContextParameterSource) {
            if (::avcodec_parameters_to_context(ContextObject, ContextParameterSource))
                doThrowChecked(::std::runtime_error,
                           "MediaCodecContext::setParameter(AVCodecParameters*) ::avcodec_parameters_to_context");
        }
    };

    class MediaFormatContext final : public NonCopyable {
    private:
        AVFormatContext *ContextObject = nullptr;

        constexpr MediaFormatContext(AVFormatContext *ContextSource) noexcept: ContextObject(ContextSource) {
        }

    public:
        doEnableMoveAssignConstruct(MediaFormatContext)

        ~MediaFormatContext() noexcept {
            if (ContextObject) {
                ::avformat_close_input(&ContextObject);
                ContextObject = nullptr;
            }
        }

        static MediaFormatContext doAllocate() {
            AVFormatContext *ContextObject = ::avformat_alloc_context();
            if (!ContextObject)
                doThrowChecked(::std::runtime_error, "MediaFormatContext::doAllocate() ::avformat_alloc_context");
            return {ContextObject};
        }

        static MediaFormatContext doAllocateOutput(const ::std::string &MediaPath) {
            AVFormatContext *ContextObject = nullptr;
            if (::avformat_alloc_output_context2(&ContextObject, nullptr, nullptr, MediaPath.c_str()) < 0)
                doThrowChecked(::std::runtime_error,
                           "MediaFormatContext::doAllocateOutput(const ::std::string&) ::avformat_alloc_output_context2");
            return {ContextObject};
        }

        void doAssign(MediaFormatContext &&ContextSource) noexcept {
            if (::std::addressof(ContextSource) == this) return;
            ContextObject = ContextSource.ContextObject;
            ContextSource.ContextObject = nullptr;
        }

        int doFindBestStream(const AVMediaType StreamType) const {
            int MediaStreamIndex = ::av_find_best_stream(ContextObject, StreamType, -1, -1, nullptr, 0);
            if (MediaStreamIndex < 0)
                doThrowChecked(::std::runtime_error, "MediaFormatContext::doFindBestStream(const AVMediaType) ::av_find_best_stream");
            return MediaStreamIndex;
        }

        void doFindStreamInformation() const {
            if (::avformat_find_stream_info(ContextObject, nullptr) < 0)
                doThrowChecked(::std::runtime_error,
                           "MediaFormatContext::doFindStreamInformation() ::avformat_find_stream_info");
        }

        static MediaFormatContext doOpen(const ::std::string &MediaSource) {
            AVFormatContext *ContextObject = nullptr;
            if (::avformat_open_input(&ContextObject, MediaSource.c_str(), nullptr, nullptr))
                doThrowChecked(::std::runtime_error, "MediaFormatContext::doOpen(const ::std::string&) ::avformat_open_input");
            return {ContextObject};
        }

        void doWriteFrame(const MediaPacket &ContextPacketSource) const {
            if (::av_write_frame(ContextObject, ContextPacketSource.PacketObject) < 0)
                doThrowChecked(::std::runtime_error, "MediaFormatContext::doWriteFrame(const MediaPacket&) ::av_write_frame");
        }

        void doWriteHeader() const {
            if (::avformat_write_header(ContextObject, nullptr) < 0)
                doThrowChecked(::std::runtime_error, "MediaFormatContext::doWriteHeader() ::avformat_write_header");
        }

        void doWriteTrailer() const {
            if (::av_write_trailer(ContextObject) < 0)
                doThrowChecked(::std::runtime_error, "MediaFormatContext::doWriteTrailer() ::av_write_trailer");
        }

        explicit operator AVFormatContext *() const noexcept {
            return ContextObject;
        }

        AVFormatContext *operator->() noexcept {
            return ContextObject;
        }

        void setOutputFormat(const AVOutputFormat *FormatSource) {
            if (!FormatSource)
                doThrowChecked(::std::runtime_error,
                           "MediaFormatContext::setOutputFormat(const AVOutputFormat*) FormatSource");
            ContextObject->oformat = FormatSource;
        }
    };
#define doInitializeFFMpeg() av_log_set_level(AV_LOG_ERROR);
}

namespace OpenAL {
    class MediaBuffer final : public NonCopyable {
    private:
        ALuint BufferIndex;
        ::std::vector<uint8_t> BufferObject;

        friend class MediaSource;
    public:
        MediaBuffer() {
            ::alGenBuffers(1, &BufferIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaBuffer::MediaBuffer() ::alGenBuffers");
        }

        MediaBuffer(const MediaChannelLayout &AudioBufferLayout, const ::std::vector<uint8_t> &AudioBuffer,
                    ALsizei AudioSampleRate) : BufferObject(AudioBuffer) {
            ::alGenBuffers(1, &BufferIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error,
                           "MediaSource::MediaSource(const MediaChannelLayout&, const IO::ByteBuffer&, ALsizei) ::alGenBuffers");
            ::alBufferData(BufferIndex, AudioBufferLayout.toOpenALFormat(), BufferObject.data(), AudioBuffer.size(),
                         AudioSampleRate);
        }

        ~MediaBuffer() noexcept {
            if (alIsBuffer(BufferIndex)) alDeleteBuffers(1, &BufferIndex);
        }
    };

    class MediaDevice final : public NonCopyable {
    private:
        ALCdevice *DeviceObject;

        friend class MediaContext;

    public:
        MediaDevice(const ::std::string &DeviceName) {
            DeviceObject = ::alcOpenDevice(DeviceName.empty() ? nullptr : DeviceName.c_str());
            if (!DeviceObject)
                doThrowChecked(::std::runtime_error, "MediaDevice::MediaDevice(const ::std::string&) ::alcOpenDevice");
        }

        ~MediaDevice() noexcept {
            if (DeviceObject) doClose();
        }

        void doClose() {
            if (!DeviceObject)
                doThrowChecked(::std::runtime_error, "MediaDevice::doClose() DeviceObject");
            alcCloseDevice(DeviceObject);
            DeviceObject = nullptr;
        }
    };

    class MediaContext final : public NonCopyable {
    private:
        ALCcontext *ContextObject;

    public:
        enum class MediaDistanceModel : ALenum {
            ModelExponent = AL_EXPONENT_DISTANCE,
            ModelExponentClamped = AL_EXPONENT_DISTANCE_CLAMPED,
            ModelInverse = AL_INVERSE_DISTANCE,
            ModelInverseClamped = AL_INVERSE_DISTANCE_CLAMPED,
            ModelLinear = AL_LINEAR_DISTANCE,
            ModelLinearClamped = AL_LINEAR_DISTANCE_CLAMPED,
            ModelNone = AL_NONE
        };

        MediaContext(const MediaDevice &ContextDevice) {
            ContextObject = ::alcCreateContext(ContextDevice.DeviceObject, nullptr);
            if (!ContextObject)
                doThrowChecked(::std::runtime_error, "MediaContext::MediaContext(const MediaDevice&) ::alcCreateContext");
        }

        ~MediaContext() noexcept {
            if (ContextObject) doDestroy();
        }

        void doDestroy() {
            if (!ContextObject)
                doThrowChecked(::std::runtime_error, "MediaContext::doDestroy() ::ContextObject");
            ::alcDestroyContext(ContextObject);
            ContextObject = nullptr;
        }

        void setContextCurrent() const {
            ::alcMakeContextCurrent(ContextObject);
        }

        static void setContextCurrentNull() noexcept {
            ::alcMakeContextCurrent(nullptr);
        }

        static void setDistanceModel(MediaDistanceModel ModelType) noexcept {
            ::alDistanceModel((ALenum) ModelType);
        }

        static void setDopplerFactor(float FactorValue) noexcept {
            ::alDopplerFactor(FactorValue);
        }

        static void setSoundVelocity(float VelocityValue) noexcept {
            ::alSpeedOfSound(VelocityValue);
        }
    };

    class MediaSource final : public NonCopyable {
    private:
        ALuint SourceIndex = -1;
    public:
        MediaSource() {
            ::alGenSources(1, &SourceIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::MediaSource() ::alGenSources");
        }

        ~MediaSource() noexcept {
            ::alDeleteSources(1, &SourceIndex);
        }

        void doPause() const {
            ::alSourcePause(SourceIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::doPause() ::alSourcePause");
        }

        void doPlay() const {
            ::alSourcePlay(SourceIndex);
        }

        void doRewind() const {
            ::alSourceRewind(SourceIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::doRewind() ::alSourceRewind");
        }

        void doStop() const {
            ::alSourceStop(SourceIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::doStop() ::alSourceStop");
        }

        void setSourceBuffer(const MediaBuffer &SourceBufferSource) const {
            ::alSourcei(SourceIndex, AL_BUFFER, (ALint) SourceBufferSource.BufferIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceBuffer(const MediaBuffer &) ::alGenSources");
        }

        void setSourceDirection(float DirectionX, float DirectionY, float DirectionZ) const {
            ::alSource3f(SourceIndex, AL_DIRECTION, DirectionX, DirectionY, DirectionZ);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceDirection(float, float, float) ::alSource3f");
        }

        void setSourceDistanceMaximum(float OptionValue) const {
            ::alSourcef(SourceIndex, AL_MAX_DISTANCE, OptionValue);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceDistanceMaximum(float) ::alSourcef");
        }

        void setSourceDistanceReference(float OptionValue) const {
            ::alSourcef(SourceIndex, AL_REFERENCE_DISTANCE, OptionValue);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceDistanceReference(float) ::alSourcef");
        }

        void setSourceGain(float OptionValue) const {
            ::alSourcef(SourceIndex, AL_GAIN, OptionValue);
            if (alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceGain(float) ::alSourcef");
        }

        void setSourceGainMaximum(float OptionValue) const {
            ::alSourcef(SourceIndex, AL_MAX_GAIN, OptionValue);
            if (alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceGainMaximum(float) ::alSourcef");
        }

        void setSourceGainMinimum(float OptionValue) const {
            ::alSourcef(SourceIndex, AL_MIN_GAIN, OptionValue);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceGainMinimum(float) ::alSourcef");
        }

        void setSourceLoop(bool OptionValue) const {
            ::alSourcei(SourceIndex, AL_LOOPING, OptionValue);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceLoop(bool) ::alSourcei");
        }

        void setSourcePitch(float OptionValue) const {
            ::alSourcef(SourceIndex, AL_PITCH, OptionValue);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourcePitch(float) ::alSourcef");
        }

        void setSourceRelative(bool OptionValue) const {
            ::alSourcei(SourceIndex, AL_SOURCE_RELATIVE, OptionValue);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceRelative(bool) ::alSourcei");
        }

        void setSourceRolloffFactor(float OptionValue) const {
            ::alSourcef(SourceIndex, AL_ROLLOFF_FACTOR, OptionValue);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceRolloffFactor(float) ::alSourcef");
        }

        void setSourceVelocity(float VelocityX, float VelocityY, float VelocityZ) const {
            ::alSource3f(SourceIndex, AL_VELOCITY, VelocityX, VelocityY, VelocityZ);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceVelocity(float, float, float) ::alSource3f");
        }
    };

#define doDestroyOpenAL() ::OpenAL::MediaContext::setContextCurrentNull();
#define doInitializeOpenAL() ::OpenAL::MediaDevice MediaDeviceObject("");::OpenAL::MediaContext MediaContextObject(MediaDeviceObject);MediaContextObject.setContextCurrent();
}

class AudioSegment {
private:
    MediaChannelLayout AudioChannelLayout;
    ::std::vector<uint8_t> AudioData;
    intmax_t AudioSampleCount = 0; // in per channel
    int AudioSampleRate = 0;
    int AudioSampleWidth = 2;

    AudioSegment(const ::std::vector<uint8_t> &AudioDataSource,
                 const MediaChannelLayout &AudioChannelLayoutSource,
                 int AudioSampleRateSource, int AudioSampleWidthSource) : AudioChannelLayout(AudioChannelLayoutSource), AudioData(AudioDataSource), AudioSampleCount(AudioDataSource.size() / AudioChannelLayoutSource.getChannelCount() / AudioSampleWidthSource), AudioSampleRate(AudioSampleRateSource), AudioSampleWidth(AudioSampleWidthSource) {}

    static MediaChannelLayout getCommonChannelLayout(const AudioSegment &Audio0) {
        return Audio0.getChannelLayout();
    }

    static MediaChannelLayout getCommonChannelLayout(const AudioSegment &Audio1, const AudioSegment &Audio2) {
        if (Audio1.getChannelLayout().getChannelCount() > Audio2.getChannelLayout().getChannelCount()) {
            return Audio1.getChannelLayout();
        }
        if (Audio1.getChannelLayout().getChannelCount() < Audio2.getChannelLayout().getChannelCount()) {
            return Audio2.getChannelLayout();
        }
        return Audio1.getChannelLayout();
    }

    template<typename ...AudioTypes>
    static MediaChannelLayout getCommonChannelLayout(const AudioSegment &Audio1, const AudioSegment &Audio2, AudioTypes ...AudioSources) {
        if (Audio1.getChannelLayout().getChannelCount() > Audio2.getChannelLayout().getChannelCount()) {
            return getCommonChannelLayout(Audio1, AudioSources...);
        }
        if (Audio1.getChannelLayout().getChannelCount() < Audio2.getChannelLayout().getChannelCount()) {
            return getCommonChannelLayout(Audio2, AudioSources...);
        }
        return Audio1.getChannelLayout();
    }

    static int getCommonSampleRate(const AudioSegment &Audio0) {
        return Audio0.getSampleRate();
    }

    static int getCommonSampleRate(const AudioSegment &Audio1, const AudioSegment &Audio2) {
        return std::max(Audio1.getSampleRate(), Audio2.getSampleRate());
    }

    template<typename ...AudioTypes>
    static int getCommonSampleRate(const AudioSegment &Audio1, const AudioSegment &Audio2, AudioTypes ...AudioSources) {
        return std::max(std::max(Audio1.getSampleRate(), Audio2.getSampleRate()), getCommonSampleRate(AudioSources...));
    }

    static std::generator<AudioSegment> doSynchronize0(MediaChannelLayout AudioLayout, int AudioRate, const AudioSegment &AudioCurrent) {
        co_yield AudioCurrent.setChannelLayout(AudioLayout).setSampleRate(AudioRate);
    }

    template<typename ...AudioTypes>
    static std::generator<AudioSegment> doSynchronize0(MediaChannelLayout AudioLayout, int AudioRate, const AudioSegment &AudioCurrent, AudioTypes ...AudioSources) {
        co_yield AudioCurrent.setChannelLayout(AudioLayout).setSampleRate(AudioRate);
        co_yield std::ranges::elements_of(doSynchronize0(AudioLayout, AudioRate, AudioSources...));
    }
public:
    doEnableCopyAssignConstruct(AudioSegment)
    doEnableMoveAssignConstruct(AudioSegment)

    static double dB2Ratio(double dBSource, bool dBAmplitude = true) noexcept {
        return dBAmplitude ? std::pow(10, dBSource / 20) : std::pow(10, dBSource / 10);
    }

    void doAssign(const AudioSegment &AudioSource) noexcept {
        if (::std::addressof(AudioSource) == this) return;
        AudioChannelLayout = AudioSource.AudioChannelLayout;
        AudioData = AudioSource.AudioData;
        AudioSampleCount = AudioSource.AudioSampleCount;
        AudioSampleRate = AudioSource.AudioSampleRate;
        AudioSampleWidth = AudioSource.AudioSampleWidth;
    }

    void doAssign(AudioSegment &&AudioSource) noexcept {
        if (::std::addressof(AudioSource) == this) return;
        AudioChannelLayout = AudioSource.AudioChannelLayout;
        AudioData = ::std::move(AudioSource.AudioData);
        AudioSampleCount = AudioSource.AudioSampleCount;
        AudioSampleRate = AudioSource.AudioSampleRate;
        AudioSampleWidth = AudioSource.AudioSampleWidth;
    }

    AudioSegment doCompressDynamicRange(double, double, double, double) const noexcept;

    AudioSegment doConcat(const AudioSegment &AudioSource) const {
        auto AudioIterator(doSynchronize(*this, AudioSource).begin());
        AudioSegment Audio1(*AudioIterator);
        AudioSegment Audio2(*AudioIterator);
        std::vector<uint8_t> AudioVector(Audio1.AudioData);
        AudioVector.insert(AudioVector.end(), Audio2.AudioData.begin(), Audio2.AudioData.end());
        return {AudioVector, AudioChannelLayout, AudioSampleRate, AudioSampleWidth};
    }

    void doExport(const ::std::string &AudioPath) const {
        FFMpeg::MediaFormatContext AudioFormatContext(FFMpeg::MediaFormatContext::doAllocateOutput(AudioPath));
        if (::avio_open(&AudioFormatContext->pb, AudioPath.c_str(), AVIO_FLAG_WRITE) < 0)
            doThrowChecked(::std::runtime_error, "AudioSegment::doExport(const ::std::string&) ::avio_open");
        FFMpeg::MediaCodec AudioCodec(FFMpeg::MediaCodec::doFindEncoder(AudioFormatContext->oformat->audio_codec));
        FFMpeg::MediaCodecContext AudioCodecContext(FFMpeg::MediaCodecContext::doAllocate(AudioCodec));
        AVChannelLayout AudioChannelLayoutSource(AudioChannelLayout.toFFMpegFormat());
        ::av_channel_layout_copy(&AudioCodecContext->ch_layout, &AudioChannelLayoutSource);
        AudioCodecContext->sample_fmt = AudioCodec->sample_fmts[0];
        AudioCodecContext->sample_rate = (int) AudioSampleRate;
        AVStream *AudioStreamObject;
        AudioCodecContext.doOpen(AudioCodec);
        if (!(AudioStreamObject = ::avformat_new_stream((AVFormatContext *) AudioFormatContext,
                                                      (const AVCodec *) AudioCodec)))
            doThrowChecked(::std::runtime_error, "AudioSegment::doExport(const ::std::string&) ::avformat_new_stream");
        if (::avcodec_parameters_from_context(AudioStreamObject->codecpar, (AVCodecContext *) AudioCodecContext))
            doThrowChecked(::std::runtime_error,
                       "AudioSegment::doExport(const ::std::string&) ::avcodec_parameters_from_context");
        AudioFormatContext.doWriteHeader();
        FFMpeg::MediaSWRContext AudioSWRContext(FFMpeg::MediaSWRContext::doAllocate(
            &AudioCodecContext->ch_layout, &AudioCodecContext->ch_layout, AV_SAMPLE_FMT_S16,
            AudioCodecContext->sample_fmt, AudioCodecContext->sample_rate, AudioCodecContext->sample_rate));
        AudioSWRContext.doInitialize();
        FFMpeg::MediaFrame AudioFrame(FFMpeg::MediaFrame::doAllocate());
        if (AudioCodecContext->frame_size <= 0) AudioCodecContext->frame_size = 2048;
        ::av_channel_layout_copy(&AudioFrame->ch_layout, &AudioCodecContext->ch_layout);
        AudioFrame->format = AudioCodecContext->sample_fmt;
        AudioFrame->nb_samples = AudioCodecContext->frame_size;
        AudioFrame->sample_rate = AudioCodecContext->sample_rate;
        AudioFrame.getFrameBuffer();
        FFMpeg::MediaPacket AudioPacket(FFMpeg::MediaPacket::doAllocate());
        uint32_t AudioSampleCurrent = 0;
        auto *AudioDataSample = new uint8_t[AudioChannelLayout.getChannelCount() * AudioCodecContext->frame_size * AudioSampleWidth];
        for (;;) {
            if (AudioSampleCurrent < AudioSampleCount) {
                int AudioFrameSize = ::std::min(AudioCodecContext->frame_size, int(AudioSampleCount - AudioSampleCurrent));
                AudioFrame->nb_samples = AudioFrameSize;
                AudioFrame->pts = AudioSampleCurrent;
                std::copy(AudioData.begin() + AudioSampleCurrent * AudioSampleWidth * AudioChannelLayout.getChannelCount(), AudioData.begin() + (AudioSampleCurrent + AudioFrameSize) * AudioSampleWidth * AudioChannelLayout.getChannelCount(), AudioDataSample);
                AudioSampleCurrent += AudioFrameSize;
                AudioSWRContext.doConvert((const uint8_t **) &AudioDataSample, AudioFrameSize,
                                          AudioFrame->extended_data, AudioFrameSize);
            } else AudioFrame.doDestroy();
            AudioCodecContext.doSendFrame(AudioFrame);
            int AudioStatus;
            while (!(AudioStatus = ::avcodec_receive_packet((AVCodecContext *) AudioCodecContext,
                                                          (AVPacket *) AudioPacket)))
                AudioFormatContext.doWriteFrame(AudioPacket);
            if (AudioStatus == AVERROR_EOF) break;
            if (AudioStatus != AVERROR(EAGAIN))
                doThrowChecked(::std::runtime_error, "AudioSegment::doExport(const ::std::string&) ::avcodec_receive_packet");
        }
        delete[] AudioDataSample;
        AudioFormatContext.doWriteTrailer();
    }

    AudioSegment doFade(double AudioGainTo = 0, double AudioGainFrom = 0, intmax_t AudioStart = 0, intmax_t AudioStop = 0, intmax_t AudioDuration = 0) const {
        if (AudioGainTo == 0 && AudioGainFrom == 0) return *this;
        doThrowChecked(std::invalid_argument, "AudioSegment::doFade(double, double, intmax_t, intmax_t, intmax_t) Unimplemented");
    }

    AudioSegment doFadeIn(intmax_t AudioDuration) const noexcept {
        return doFade(0, -120., 0, -1, AudioDuration);
    }

    AudioSegment doFadeOut(intmax_t AudioDuration) const noexcept {
        return doFade(-120., 0, -1, (intmax_t) getDuration() * 1000., AudioDuration);
    }

    AudioSegment doFilterLow(intmax_t) const noexcept;

    AudioSegment doFilterHigh(intmax_t) const noexcept;

    AudioSegment doGain(double) const noexcept;

    AudioSegment doNormalize(double) const noexcept;

    static AudioSegment doOpen(const ::std::string &AudioPath) {
        FFMpeg::MediaFormatContext AudioFormatContext(FFMpeg::MediaFormatContext::doOpen(AudioPath));
        AudioFormatContext.doFindStreamInformation();
        int AudioStreamIndex = AudioFormatContext.doFindBestStream(AVMEDIA_TYPE_AUDIO);
        FFMpeg::MediaCodecContext AudioCodecContext(FFMpeg::MediaCodecContext::doAllocate());
        AudioCodecContext.setParameter(AudioFormatContext->streams[AudioStreamIndex]->codecpar);
        FFMpeg::MediaCodec AudioCodec(FFMpeg::MediaCodec::doFindDecoder(AudioCodecContext->codec_id));
        AudioCodecContext.doOpen(AudioCodec);
        if (AudioCodecContext->sample_rate <= 0) [[unlikely]]
                doThrowChecked(::std::runtime_error,
                               "AudioSegment::doOpen(const ::std::string&) AudioCodecContext->sample_rate");
        FFMpeg::MediaSWRContext AudioSWRContext(FFMpeg::MediaSWRContext::doAllocate(
            &AudioCodecContext->ch_layout, &AudioCodecContext->ch_layout, AudioCodecContext->sample_fmt,
            AV_SAMPLE_FMT_S16, AudioCodecContext->sample_rate, AudioCodecContext->sample_rate));
        AudioSWRContext.doInitialize();
        int AudioSampleWidthOutput = 2;
        FFMpeg::MediaFrame AudioFrame(FFMpeg::MediaFrame::doAllocate());
        FFMpeg::MediaPacket AudioPacket(FFMpeg::MediaPacket::doAllocate());
        ::std::vector<uint8_t> AudioDataOutput;
        for (;;) {
            int AudioStatus = ::av_read_frame((AVFormatContext *) AudioFormatContext, (AVPacket *) AudioPacket);
            if (AudioStatus == AVERROR_EOF) break;
            if (AudioStatus < 0)
                doThrowChecked(::std::runtime_error, "AudioSegment::doOpen(const ::std::string&) ::av_read_frame");
            if (AudioPacket->stream_index != AudioStreamIndex) continue;
            AudioCodecContext.doSendPacket(AudioPacket);
            while (!(AudioStatus =
                     ::avcodec_receive_frame((AVCodecContext *) AudioCodecContext, (AVFrame *) AudioFrame))) {
                auto *AudioDataBuffer = new uint8_t[AudioCodecContext->ch_layout.nb_channels * AudioFrame->nb_samples * AudioSampleWidthOutput];
                AudioSWRContext.doConvert((const uint8_t **) AudioFrame->extended_data, AudioFrame->nb_samples,
                                          (uint8_t **) &AudioDataBuffer, AudioFrame->nb_samples);
                AudioDataOutput.insert(AudioDataOutput.end(), AudioDataBuffer, AudioDataBuffer + AudioFrame->nb_samples * AudioCodecContext->ch_layout.nb_channels * AudioSampleWidthOutput);
                delete[] AudioDataBuffer;
            }
            if (AudioStatus != AVERROR(EAGAIN))
                doThrowChecked(::std::runtime_error, "AudioSegment::doOpen(const ::std::string&) ::avcodec_receive_frame");
        }
        return {AudioDataOutput, AudioCodecContext->ch_layout, AudioCodecContext->sample_rate, 2};
    }

    AudioSegment doOverlay(const AudioSegment &AudioSource, uintmax_t, bool) const;

    AudioSegment doSpeedUp(double) const;

    static std::generator<AudioSegment> doSynchronize(const AudioSegment &AudioCurrent) {
        co_yield AudioCurrent;
    }

    template<typename ...AudioTypes>
    static std::generator<AudioSegment> doSynchronize(const AudioSegment &AudioCurrent, AudioTypes ...AudioSources) {
        co_yield std::ranges::elements_of(doSynchronize0(getCommonChannelLayout(AudioCurrent, AudioSources...), getCommonSampleRate(AudioCurrent, AudioSources...), AudioCurrent, AudioSources...));
    }

    AudioSegment doRepeat(uintmax_t AudioCount) const {
        std::vector<uint8_t> AudioDataNew;
        for (uintmax_t AudioIndex = 0;AudioIndex < AudioCount;++AudioIndex)
            AudioDataNew.insert(AudioDataNew.end(), AudioData.begin(), AudioData.end());
        return {AudioDataNew, AudioChannelLayout, AudioSampleRate, AudioSampleWidth};
    }

    AudioSegment doReverse() const noexcept {
        return {{AudioData.rbegin(), AudioData.rend()}, AudioChannelLayout, AudioSampleRate, AudioSampleWidth};
    }

    AudioSegment doSlice(uintmax_t AudioStart, uintmax_t AudioStop, uintmax_t AudioStep) const;

    AudioSegment doSlice(intmax_t AudioPosition) const {
        if (AudioPosition < 0) AudioPosition += getDuration() * 1000;
        if (AudioPosition < 0 || AudioPosition > intmax_t(getDuration() * 1000) + 1) doThrowChecked(::std::runtime_error, "AudioSegment::doSlice(intmax_t) Index out of bounds");
        return {{AudioData.begin() + AudioPosition * (AudioSampleRate / 1000.0) * AudioChannelLayout.getChannelCount() * AudioSampleWidth, AudioPosition != int(getDuration() * 1000) + 1 ? AudioData.begin() + (AudioPosition + 1) * (AudioSampleRate / 1000.0) * AudioChannelLayout.getChannelCount() * AudioSampleWidth : AudioData.end()}, AudioChannelLayout, AudioSampleRate, AudioSampleWidth};
    }

    std::generator<AudioSegment> doSplitChannels() const noexcept;

    std::generator<AudioSegment> doSplitOnSilence() const noexcept;

    AudioSegment doStripSilence(intmax_t, intmax_t, intmax_t) const;

    template<typename ...AudioSegments>
    static AudioSegment fromChannels(AudioSegments...);

    MediaChannelLayout getChannelLayout() const noexcept {
        return AudioChannelLayout;
    }

    double getdBFS() const noexcept {
        return ratio2dB(getRMS() * 1.0 / getMaximumPossibleAmplitude());
    }

    double getdBFSMaximum() const noexcept {
        return ratio2dB(getMaximum() * 1.0 / getMaximumPossibleAmplitude());
    }

    double getDuration() const noexcept {
        return AudioSampleCount * 1. / AudioSampleRate;
    }

    int getMaximum() const noexcept {
        int AudioMaximum = 0;
        auto AudioDataPointer(reinterpret_cast<const int16_t*>(AudioData.data()));
        for (uintmax_t AudioSample = 0;AudioSample < AudioSampleCount * AudioChannelLayout.getChannelCount();++AudioSample) {
            AudioMaximum = std::max(abs(AudioDataPointer[AudioSample]), AudioMaximum);
        }
        return AudioMaximum;
    }

    double getMaximumPossibleAmplitude() const noexcept {
        return (2 << (AudioSampleWidth << 3)) >> 2;
    }

    double getRMS() const noexcept {
        double AudioSum = 0;
        auto AudioDataPointer(reinterpret_cast<const int16_t*>(AudioData.data()));
        for (uintmax_t AudioSample = 0;AudioSample < AudioSampleCount * AudioChannelLayout.getChannelCount();++AudioSample) {
            AudioSum += (double) AudioDataPointer[AudioSample] * AudioDataPointer[AudioSample];
        }
        return sqrt(AudioSum / (AudioSampleCount * AudioChannelLayout.getChannelCount()));
    }

    int getSampleRate() const noexcept {
        return AudioSampleRate;
    }

    std::generator<int16_t*> getSamples(intmax_t SampleStart, intmax_t SampleStop, intmax_t SampleStep = 1) {
        auto AudioDataPointer(reinterpret_cast<int16_t*>(AudioData.data()));
        for (uintmax_t AudioSample = SampleStart * AudioChannelLayout.getChannelCount();AudioSample < SampleStop * AudioChannelLayout.getChannelCount();AudioSample += SampleStep)
            co_yield AudioDataPointer + AudioSample;
    }

    std::generator<int16_t> getSamples(intmax_t SampleStart, intmax_t SampleStop, intmax_t SampleStep = 1) const {
        if (SampleStart < 0) SampleStart += AudioSampleCount;
        if (SampleStop < 0) SampleStop += AudioSampleCount;
        auto AudioDataPointer(reinterpret_cast<const int16_t*>(AudioData.data()));
        for (intmax_t AudioSample = SampleStart * AudioChannelLayout.getChannelCount();AudioSample < SampleStop * AudioChannelLayout.getChannelCount();AudioSample += SampleStep)
            co_yield AudioDataPointer[AudioSample];
    }

    bool isEqual(const AudioSegment &AudioSource) const noexcept {
        return AudioData == AudioSource.AudioData && AudioChannelLayout.getChannelMask() == AudioSource.AudioChannelLayout.getChannelMask() && AudioSampleRate == AudioSource.AudioSampleRate && AudioSampleWidth == AudioSource.AudioSampleWidth;
    }

    static double ratio2dB(double RatioSource, bool RatioAmplitude=true) noexcept {
        if (!RatioSource) return -std::numeric_limits<double>::infinity();
        return RatioAmplitude ? 20 * log10(RatioSource) : 10 * log10(RatioSource);
    }

    AudioSegment setChannelLayout(MediaChannelLayout AudioChannelLayoutSource) const {
        AVChannelLayout AudioChannelLayoutInput(AudioChannelLayout.toFFMpegFormat());
        AVChannelLayout AudioChannelLayoutOutput(AudioChannelLayoutSource.toFFMpegFormat());
        FFMpeg::MediaSWRContext AudioSWRContext(FFMpeg::MediaSWRContext::doAllocate(
            &AudioChannelLayoutInput, &AudioChannelLayoutOutput, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16, AudioSampleRate,
            AudioSampleRate));
        AudioSWRContext.doInitialize();
        uint8_t *AudioDataPointer(const_cast<uint8_t*>(AudioData.data()));
        ::std::vector<uint8_t> AudioDataOutput(AudioSampleCount * AudioChannelLayoutSource.getChannelCount() * AudioSampleWidth, 0);
        auto AudioDataOutputPointer(AudioDataOutput.data());
        AudioSWRContext.doConvert((const uint8_t **) &AudioDataPointer, AudioSampleCount, &AudioDataOutputPointer, AudioSampleCount);
        return {AudioDataOutput, AudioChannelLayoutSource, AudioSampleRate, AudioSampleWidth};
    }

    AudioSegment setSampleRate(int AudioSampleRateSource) const {
        if (AudioSampleRateSource <= 0) doThrowChecked(::std::runtime_error, "AudioSegment::setSampleRate(int) Invalid sample rate");
        if (AudioSampleRateSource == AudioSampleRate) return *this;
        AVChannelLayout AudioChannelLayoutSource(AudioChannelLayout.toFFMpegFormat());
        FFMpeg::MediaSWRContext AudioSWRContext(FFMpeg::MediaSWRContext::doAllocate(
            &AudioChannelLayoutSource, &AudioChannelLayoutSource, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16, AudioSampleRate,
            AudioSampleRateSource));
        AudioSWRContext.doInitialize();
        uint8_t *AudioDataPointer(const_cast<uint8_t*>(AudioData.data()));
        auto AudioDataOutputSample = av_rescale_rnd(swr_get_delay((SwrContext*) AudioSWRContext, AudioSampleRate) + AudioSampleCount, AudioSampleRateSource, AudioSampleRate, AV_ROUND_UP);
        ::std::vector<uint8_t> AudioDataOutput(AudioDataOutputSample * AudioChannelLayout.getChannelCount() * AudioSampleWidth, 0);
        auto AudioDataOutputPointer(AudioDataOutput.data());
        AudioSWRContext.doConvert((const uint8_t **) &AudioDataPointer, AudioSampleCount, &AudioDataOutputPointer, AudioDataOutputSample);
        return {AudioDataOutput, AudioChannelLayout, AudioSampleRateSource, AudioSampleWidth};
    }

    OpenAL::MediaBuffer toMediaBuffer() const {
        return {AudioChannelLayout, AudioData, (ALsizei) AudioSampleRate};
    }
};

#endif
