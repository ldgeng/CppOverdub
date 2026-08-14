#ifndef CPPOVERDUB_LIBRARY_H
#define CPPOVERDUB_LIBRARY_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <generator>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#define AL_ALEXT_PROTOTYPES
#include "AL/alext.h"
#include "AL/efx-presets.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavfilter/avfilter.h"
#include "libswresample/swresample.h"
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
#define doEnableMoveAssignConstruct(ClassName) ClassName(ClassName &&ObjectSource) noexcept {doAssign(::std::move(ObjectSource));}ClassName &operator=(ClassName &&ObjectSource) noexcept {doAssign(::std::move(ObjectSource));return *this;}
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

        ~MediaCodec() noexcept = default;

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

        static MediaCodec doFindEncoderByName(const ::std::string &CodecName) {
            const AVCodec *CodecObject = ::avcodec_find_encoder_by_name(CodecName.c_str());
            if (!CodecObject)
                doThrowChecked(::std::runtime_error, "MediaCodec::doFindEncoderByName(const ::std::string&) ::avcodec_find_encoder_by_name");
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
            doDestroy();
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
            doDestroy();
        }

        static MediaPacket doAllocate() {
            AVPacket *PacketObject = ::av_packet_alloc();
            if (!PacketObject)
                doThrowChecked(::std::runtime_error, "MediaPacket::doAllocate() ::av_packet_alloc");
            return {PacketObject};
        }

        void doAssign(MediaPacket &&PacketSource) noexcept {
            if (::std::addressof(PacketSource) == this) return;
            doDestroy();
            PacketObject = PacketSource.PacketObject;
            PacketSource.PacketObject = nullptr;
        }

        void doDestroy() noexcept {
            if (PacketObject) {
                ::av_packet_free(&PacketObject);
                PacketObject = nullptr;
            }
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
            doDestroy();
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
                           "MediaSWRContext::doAllocate(AVChannelLayout*, AVChannelLayout*, enum AVSampleFormat, enum AVSampleFormat, int, int) ::swr_alloc_set_opts2");
            return {ContextObject};
        }

        void doAssign(MediaSWRContext &&ContextSource) noexcept {
            if (::std::addressof(ContextSource) == this) return;
            doDestroy();
            ContextObject = ContextSource.ContextObject;
            ContextSource.ContextObject = nullptr;
        }

        void doDestroy() noexcept {
            if (ContextObject) {
                swr_free(&ContextObject);
                ContextObject = nullptr;
            }
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
            doDestroy();
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
            doDestroy();
            ContextObject = ContextSource.ContextObject;
            ContextSource.ContextObject = nullptr;
        }

        void doDestroy() noexcept {
            if (ContextObject) {
                avcodec_free_context(&ContextObject);
                ContextObject = nullptr;
            }
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
            doDestroy();
        }

        static MediaFormatContext doAllocateOutput(const ::std::string &MediaPath) {
            AVFormatContext *ContextObject = nullptr;
            if (::avformat_alloc_output_context2(&ContextObject, nullptr, nullptr, MediaPath.c_str()) < 0)
                doThrowChecked(::std::runtime_error,
                           "MediaFormatContext::doAllocateOutput(const ::std::string&) ::avformat_alloc_output_context2");
            return {ContextObject};
        }

        static MediaFormatContext doAllocateOutput(const ::std::string &MediaPath, const ::std::string &MediaFormatName) {
            AVFormatContext *ContextObject = nullptr;
            const AVOutputFormat *FormatObject = ::av_guess_format(MediaFormatName.c_str(), MediaPath.c_str(), nullptr);
            if (!FormatObject)
                doThrowChecked(::std::runtime_error,
                           "MediaFormatContext::doAllocateOutput(const ::std::string&, const ::std::string&) ::av_guess_format");
            if (::avformat_alloc_output_context2(&ContextObject, FormatObject, MediaFormatName.c_str(), MediaPath.c_str()) < 0)
                doThrowChecked(::std::runtime_error,
                           "MediaFormatContext::doAllocateOutput(const ::std::string&, const ::std::string&) ::avformat_alloc_output_context2");
            return {ContextObject};
        }

        void doAssign(MediaFormatContext &&ContextSource) noexcept {
            if (::std::addressof(ContextSource) == this) return;
            doDestroy();
            ContextObject = ContextSource.ContextObject;
            ContextSource.ContextObject = nullptr;
        }

        void doDestroy() noexcept {
            if (ContextObject) {
                ::avformat_close_input(&ContextObject);
                ContextObject = nullptr;
            }
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
    // ALC_SOFT_HRTF entry points resolved at runtime (some import libs omit the extension exports).
    static LPALCGETSTRINGISOFT doGetHRTFStringFunction(ALCdevice *DeviceSource) noexcept {
        static LPALCGETSTRINGISOFT FunctionObject =
            (LPALCGETSTRINGISOFT) ::alcGetProcAddress(DeviceSource, "alcGetStringiSOFT");
        return FunctionObject;
    }

    static LPALCRESETDEVICESOFT doGetHRTFResetFunction(ALCdevice *DeviceSource) noexcept {
        static LPALCRESETDEVICESOFT FunctionObject =
            (LPALCRESETDEVICESOFT) ::alcGetProcAddress(DeviceSource, "alcResetDeviceSOFT");
        return FunctionObject;
    }

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
                           "MediaBuffer::MediaBuffer(const MediaChannelLayout&, const ::std::vector<uint8_t>&, ALsizei) ::alGenBuffers");
            ::alBufferData(BufferIndex, AudioBufferLayout.toOpenALFormat(), BufferObject.data(), AudioBuffer.size(),
                         AudioSampleRate);
        }

        ~MediaBuffer() noexcept {
            // This build of OpenAL Soft aborts on AL calls without a current context, so skip
            // deletion entirely when no context is current (the device reclaims the name on close).
            if (!::alcGetCurrentContext()) return;
            if (alIsBuffer(BufferIndex)) {
                // Deleting a buffer still attached to a source defers the delete but may queue an
                // AL error; drain it so the next checked AL call does not see a stale error.
                alDeleteBuffers(1, &BufferIndex);
                while (::alGetError() != AL_NO_ERROR);
            }
        }

        // Refills this buffer with new PCM data (queue streaming: refill then doQueueBuffer again).
        void setBufferData(const MediaChannelLayout &AudioBufferLayout, const ::std::vector<uint8_t> &AudioBuffer,
                           ALsizei AudioSampleRate) {
            BufferObject = AudioBuffer;
            ::alBufferData(BufferIndex, AudioBufferLayout.toOpenALFormat(), BufferObject.data(),
                           (ALsizei) BufferObject.size(), AudioSampleRate);
            ALenum AudioError = ::alGetError();
            if (AudioError != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error,
                           "MediaBuffer::setBufferData(const MediaChannelLayout&, const ::std::vector<uint8_t>&, ALsizei) ::alBufferData error=0x"
                           + std::to_string((unsigned) AudioError));
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

        // ALC_SOFT_HRTF: lists the HRTF profile names of this device (OpenAL Soft).
        ::std::vector<::std::string> doListHRTFNames() const {
            ::std::vector<::std::string> HRTFNames;
            if (!::alcIsExtensionPresent(DeviceObject, "ALC_SOFT_HRTF")) return HRTFNames;
            for (ALCint HRTFIndex = 0;; ++HRTFIndex) {
                const ALCchar *HRTFName = doGetHRTFStringFunction(DeviceObject)(DeviceObject, ALC_HRTF_SPECIFIER_SOFT, HRTFIndex);
                if (!HRTFName) break;
                HRTFNames.emplace_back(HRTFName);
            }
            return HRTFNames;
        }

        // ALC_SOFT_HRTF: 1 = enabled, 0 = disabled, -1 = extension unsupported.
        int doGetHRTFState() const {
            if (!::alcIsExtensionPresent(DeviceObject, "ALC_SOFT_HRTF")) return -1;
            ALCint HRTFState = 0;
            ::alcGetIntegerv(DeviceObject, ALC_HRTF_SOFT, 1, &HRTFState);
            return (int) HRTFState;
        }

        // ALC_SOFT_HRTF: enable/disable HRTF. Resets the device: call before creating MediaContext.
        void setHRTF(bool OptionValue) {
            if (!::alcIsExtensionPresent(DeviceObject, "ALC_SOFT_HRTF"))
                doThrowChecked(::std::runtime_error, "MediaDevice::setHRTF(bool) ALC_SOFT_HRTF");
            const ALCint HRTFAttributes[] = {ALC_HRTF_SOFT, OptionValue ? ALC_TRUE : ALC_FALSE, 0};
            if (!doGetHRTFResetFunction(DeviceObject)(DeviceObject, HRTFAttributes))
                doThrowChecked(::std::runtime_error, "MediaDevice::setHRTF(bool) ::alcResetDeviceSOFT");
        }

        // ALC_SOFT_HRTF: select a profile by name (see doListHRTFNames). Resets the device.
        void setHRTF(const ::std::string &HRTFName) {
            if (!::alcIsExtensionPresent(DeviceObject, "ALC_SOFT_HRTF"))
                doThrowChecked(::std::runtime_error, "MediaDevice::setHRTF(const ::std::string&) ALC_SOFT_HRTF");
            ALint HRTFIndex = -1;
            for (ALCint HRTFIndexSource = 0;; ++HRTFIndexSource) {
                const ALCchar *HRTFNameSource = doGetHRTFStringFunction(DeviceObject)(DeviceObject, ALC_HRTF_SPECIFIER_SOFT, HRTFIndexSource);
                if (!HRTFNameSource) break;
                if (::std::string(HRTFNameSource) == HRTFName) {
                    HRTFIndex = HRTFIndexSource;
                    break;
                }
            }
            if (HRTFIndex < 0)
                doThrowChecked(::std::invalid_argument, "MediaDevice::setHRTF(const ::std::string&) Unknown HRTF name");
            const ALCint HRTFAttributes[] = {ALC_HRTF_ID_SOFT, HRTFIndex, ALC_HRTF_SOFT, ALC_TRUE, 0};
            if (!doGetHRTFResetFunction(DeviceObject)(DeviceObject, HRTFAttributes))
                doThrowChecked(::std::runtime_error, "MediaDevice::setHRTF(const ::std::string&) ::alcResetDeviceSOFT");
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

        static void setListenerPosition(float PositionX, float PositionY, float PositionZ) noexcept {
            ::alListener3f(AL_POSITION, PositionX, PositionY, PositionZ);
        }

        static void setListenerVelocity(float VelocityX, float VelocityY, float VelocityZ) noexcept {
            ::alListener3f(AL_VELOCITY, VelocityX, VelocityY, VelocityZ);
        }

        static void setListenerOrientation(float AtX, float AtY, float AtZ, float UpX, float UpY,
                                           float UpZ) noexcept {
            const ALfloat AudioOrientation[] = {AtX, AtY, AtZ, UpX, UpY, UpZ};
            ::alListenerfv(AL_ORIENTATION, AudioOrientation);
        }

        static void setListenerGain(float OptionValue) noexcept {
            ::alListenerf(AL_GAIN, OptionValue);
        }
    };

    class MediaEffect final : public NonCopyable {
    private:
        ALuint EffectIndex = 0;

        friend class MediaAuxiliarySlot;

    public:
        explicit MediaEffect(ALenum EffectType) {
            ::alGenEffects(1, &EffectIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaEffect::MediaEffect(ALenum) ::alGenEffects");
            ::alEffecti(EffectIndex, AL_EFFECT_TYPE, EffectType);
        }

        ~MediaEffect() noexcept {
            if (!EffectIndex || !::alcGetCurrentContext()) return;
            ::alDeleteEffects(1, &EffectIndex);
        }

        void doApplyPreset(const EFXEAXREVERBPROPERTIES &PresetSource) {
            ::alEffectf(EffectIndex, AL_EAXREVERB_DENSITY, PresetSource.flDensity);
            ::alEffectf(EffectIndex, AL_EAXREVERB_DIFFUSION, PresetSource.flDiffusion);
            ::alEffectf(EffectIndex, AL_EAXREVERB_GAIN, PresetSource.flGain);
            ::alEffectf(EffectIndex, AL_EAXREVERB_GAINHF, PresetSource.flGainHF);
            ::alEffectf(EffectIndex, AL_EAXREVERB_GAINLF, PresetSource.flGainLF);
            ::alEffectf(EffectIndex, AL_EAXREVERB_DECAY_TIME, PresetSource.flDecayTime);
            ::alEffectf(EffectIndex, AL_EAXREVERB_DECAY_HFRATIO, PresetSource.flDecayHFRatio);
            ::alEffectf(EffectIndex, AL_EAXREVERB_DECAY_LFRATIO, PresetSource.flDecayLFRatio);
            ::alEffectf(EffectIndex, AL_EAXREVERB_REFLECTIONS_GAIN, PresetSource.flReflectionsGain);
            ::alEffectf(EffectIndex, AL_EAXREVERB_REFLECTIONS_DELAY, PresetSource.flReflectionsDelay);
            ::alEffectfv(EffectIndex, AL_EAXREVERB_REFLECTIONS_PAN, PresetSource.flReflectionsPan);
            ::alEffectf(EffectIndex, AL_EAXREVERB_LATE_REVERB_GAIN, PresetSource.flLateReverbGain);
            ::alEffectf(EffectIndex, AL_EAXREVERB_LATE_REVERB_DELAY, PresetSource.flLateReverbDelay);
            ::alEffectfv(EffectIndex, AL_EAXREVERB_LATE_REVERB_PAN, PresetSource.flLateReverbPan);
            ::alEffectf(EffectIndex, AL_EAXREVERB_ECHO_TIME, PresetSource.flEchoTime);
            ::alEffectf(EffectIndex, AL_EAXREVERB_ECHO_DEPTH, PresetSource.flEchoDepth);
            ::alEffectf(EffectIndex, AL_EAXREVERB_MODULATION_TIME, PresetSource.flModulationTime);
            ::alEffectf(EffectIndex, AL_EAXREVERB_MODULATION_DEPTH, PresetSource.flModulationDepth);
            ::alEffectf(EffectIndex, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, PresetSource.flAirAbsorptionGainHF);
            ::alEffectf(EffectIndex, AL_EAXREVERB_HFREFERENCE, PresetSource.flHFReference);
            ::alEffectf(EffectIndex, AL_EAXREVERB_LFREFERENCE, PresetSource.flLFReference);
            ::alEffectf(EffectIndex, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, PresetSource.flRoomRolloffFactor);
            ::alEffecti(EffectIndex, AL_EAXREVERB_DECAY_HFLIMIT, PresetSource.iDecayHFLimit);
            // Some implementations only support the standard AL_REVERB_* subset and reject the
            // EAX4-only parameters above; drain any queued errors so the next checked call is clean.
            while (::alGetError() != AL_NO_ERROR);
        }

        void setParameter(ALenum Parameter, float OptionValue) const {
            ::alEffectf(EffectIndex, Parameter, OptionValue);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaEffect::setParameter(ALenum, float) ::alEffectf");
        }
    };

    class MediaAuxiliarySlot final : public NonCopyable {
    private:
        ALuint SlotIndex = 0;

        friend class MediaSource;

    public:
        MediaAuxiliarySlot() {
            ::alGenAuxiliaryEffectSlots(1, &SlotIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaAuxiliarySlot::MediaAuxiliarySlot() ::alGenAuxiliaryEffectSlots");
        }

        ~MediaAuxiliarySlot() noexcept {
            if (!SlotIndex || !::alcGetCurrentContext()) return;
            ::alDeleteAuxiliaryEffectSlots(1, &SlotIndex);
        }

        void setEffect(const MediaEffect &EffectSource) const {
            ::alAuxiliaryEffectSloti(SlotIndex, AL_EFFECTSLOT_EFFECT, (ALint) EffectSource.EffectIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaAuxiliarySlot::setEffect(const MediaEffect &) ::alAuxiliaryEffectSloti");
        }
    };

    class MediaSource final : public NonCopyable {
    private:
        ALuint SourceIndex = -1;
    public:
        enum class MediaSourceState : ALint {
            StateInitial = AL_INITIAL,
            StatePlaying = AL_PLAYING,
            StatePaused = AL_PAUSED,
            StateStopped = AL_STOPPED
        };

        MediaSource() {
            ::alGenSources(1, &SourceIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::MediaSource() ::alGenSources");
        }

        ~MediaSource() noexcept {
            if (!::alcGetCurrentContext()) return;
            ::alDeleteSources(1, &SourceIndex);
        }

        void doPause() const {
            ::alSourcePause(SourceIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::doPause() ::alSourcePause");
        }

        void doPlay() const {
            ::alSourcePlay(SourceIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::doPlay() ::alSourcePlay");
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
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceBuffer(const MediaBuffer &) ::alSourcei");
        }

        // Queue streaming: enqueue one buffer. The source must not have a static AL_BUFFER attached.
        void doQueueBuffer(const MediaBuffer &SourceBufferSource) const {
            ::alSourceQueueBuffers(SourceIndex, 1, &SourceBufferSource.BufferIndex);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::doQueueBuffer(const MediaBuffer &) ::alSourceQueueBuffers");
        }

        // Queue streaming: unqueues one processed buffer so it can be refilled and queued again.
        void doUnqueueBuffer() const {
            ALuint AudioBuffer = 0;
            ::alSourceUnqueueBuffers(SourceIndex, 1, &AudioBuffer);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::doUnqueueBuffer() ::alSourceUnqueueBuffers");
        }

        ALint getSourceBuffersProcessed() const noexcept {
            ALint AudioProcessed = 0;
            ::alGetSourcei(SourceIndex, AL_BUFFERS_PROCESSED, &AudioProcessed);
            return AudioProcessed;
        }

        ALint getSourceBuffersQueued() const noexcept {
            ALint AudioQueued = 0;
            ::alGetSourcei(SourceIndex, AL_BUFFERS_QUEUED, &AudioQueued);
            return AudioQueued;
        }

        MediaSourceState getSourceState() const noexcept {
            ALint AudioState = 0;
            ::alGetSourcei(SourceIndex, AL_SOURCE_STATE, &AudioState);
            return (MediaSourceState) AudioState;
        }

        ALint getSourceOffsetSamples() const noexcept {
            ALint AudioOffset = 0;
            ::alGetSourcei(SourceIndex, AL_SAMPLE_OFFSET, &AudioOffset);
            return AudioOffset;
        }

        double getSourceOffsetSeconds() const noexcept {
            ALfloat AudioOffset = 0;
            ::alGetSourcef(SourceIndex, AL_SEC_OFFSET, &AudioOffset);
            return (double) AudioOffset;
        }

        void setSourceAuxiliary(const MediaAuxiliarySlot &AuxiliarySlotSource) const {
            ::alSource3i(SourceIndex, AL_AUXILIARY_SEND_FILTER, (ALint) AuxiliarySlotSource.SlotIndex, 0, AL_FILTER_NULL);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceAuxiliary(const MediaAuxiliarySlot &) ::alSource3i");
        }

        void setSourceDirection(float DirectionX, float DirectionY, float DirectionZ) const {
            ::alSource3f(SourceIndex, AL_DIRECTION, DirectionX, DirectionY, DirectionZ);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceDirection(float, float, float) ::alSource3f");
        }

        void setSourcePosition(float PositionX, float PositionY, float PositionZ) const {
            ::alSource3f(SourceIndex, AL_POSITION, PositionX, PositionY, PositionZ);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourcePosition(float, float, float) ::alSource3f");
        }

        // AL_SOFT_loop_points: seamless loop between two sample offsets; requires setSourceLoop(true).
        void setSourceLoopPoints(ALint AudioLoopStart, ALint AudioLoopEnd) const {
            if (!::alIsExtensionPresent("AL_SOFT_loop_points"))
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceLoopPoints(ALint, ALint) AL_SOFT_loop_points");
            const ALint AudioLoopPoints[] = {AudioLoopStart, AudioLoopEnd};
            ::alSourceiv(SourceIndex, AL_LOOP_POINTS_SOFT, AudioLoopPoints);
            if (::alGetError() != AL_NO_ERROR)
                doThrowChecked(::std::runtime_error, "MediaSource::setSourceLoopPoints(ALint, ALint) ::alSourceiv");
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
            if (::alGetError() != AL_NO_ERROR)
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

    class MediaReverb final : public NonCopyable {
    private:
        MediaEffect EffectObject;
        MediaAuxiliarySlot SlotObject;

    public:
        MediaReverb() : EffectObject(AL_EFFECT_REVERB), SlotObject() {
            EffectObject.doApplyPreset(EFX_REVERB_PRESET_GENERIC);
            SlotObject.setEffect(EffectObject);
        }

        explicit MediaReverb(const EFXEAXREVERBPROPERTIES &PresetSource) : EffectObject(AL_EFFECT_REVERB), SlotObject() {
            EffectObject.doApplyPreset(PresetSource);
            SlotObject.setEffect(EffectObject);
        }

        void setSource(const MediaSource &SourceTarget) const {
            SourceTarget.setSourceAuxiliary(SlotObject);
        }
    };

    class MediaEcho final : public NonCopyable {
    private:
        MediaEffect EffectObject;
        MediaAuxiliarySlot SlotObject;

    public:
        MediaEcho() : EffectObject(AL_EFFECT_ECHO), SlotObject() {
            EffectObject.setParameter(AL_ECHO_DELAY, AL_ECHO_DEFAULT_DELAY);
            EffectObject.setParameter(AL_ECHO_LRDELAY, AL_ECHO_DEFAULT_LRDELAY);
            EffectObject.setParameter(AL_ECHO_DAMPING, AL_ECHO_DEFAULT_DAMPING);
            EffectObject.setParameter(AL_ECHO_FEEDBACK, AL_ECHO_DEFAULT_FEEDBACK);
            EffectObject.setParameter(AL_ECHO_SPREAD, AL_ECHO_DEFAULT_SPREAD);
            SlotObject.setEffect(EffectObject);
        }

        explicit MediaEcho(float EchoDelay, float EchoFeedback, float EchoDamping,
                           float EchoSpread = AL_ECHO_DEFAULT_SPREAD) : EffectObject(AL_EFFECT_ECHO), SlotObject() {
            setDelay(EchoDelay);
            setFeedback(EchoFeedback);
            setDamping(EchoDamping);
            setSpread(EchoSpread);
            SlotObject.setEffect(EffectObject);
        }

        void setDelay(float OptionValue) const {
            EffectObject.setParameter(AL_ECHO_DELAY, ::std::clamp(OptionValue, AL_ECHO_MIN_DELAY, AL_ECHO_MAX_DELAY));
        }

        void setLeftRightDelay(float OptionValue) const {
            EffectObject.setParameter(AL_ECHO_LRDELAY,
                                      ::std::clamp(OptionValue, AL_ECHO_MIN_LRDELAY, AL_ECHO_MAX_LRDELAY));
        }

        void setDamping(float OptionValue) const {
            EffectObject.setParameter(AL_ECHO_DAMPING,
                                      ::std::clamp(OptionValue, AL_ECHO_MIN_DAMPING, AL_ECHO_MAX_DAMPING));
        }

        void setFeedback(float OptionValue) const {
            EffectObject.setParameter(AL_ECHO_FEEDBACK,
                                      ::std::clamp(OptionValue, AL_ECHO_MIN_FEEDBACK, AL_ECHO_MAX_FEEDBACK));
        }

        void setSpread(float OptionValue) const {
            EffectObject.setParameter(AL_ECHO_SPREAD, ::std::clamp(OptionValue, AL_ECHO_MIN_SPREAD, AL_ECHO_MAX_SPREAD));
        }

        void setSource(const MediaSource &SourceTarget) const {
            SourceTarget.setSourceAuxiliary(SlotObject);
        }
    };

#define doDestroyOpenAL() ::OpenAL::MediaContext::setContextCurrentNull();
#define doInitializeOpenAL() ::OpenAL::MediaDevice MediaDeviceObject("");::OpenAL::MediaContext MediaContextObject(MediaDeviceObject);MediaContextObject.setContextCurrent();
}

struct MediaExportOption final {
    ::std::string ExportFormat;    // container format name ("mp3"/"wav"/"ogg"...), empty = infer from path extension
    ::std::string ExportCodec;     // encoder name ("libmp3lame"/"aac"/"pcm_s16le"...), empty = container default
    intmax_t ExportBitrate = 0;    // target bitrate in bps, 0 = encoder default
    int ExportSampleRate = 0;      // target sample rate in Hz, 0 = keep original
};

struct MediaMetadata final {
    ::std::string MetadataTitle;   // "title" tag, empty when absent
    ::std::string MetadataArtist;  // "artist" tag, empty when absent
    ::std::string MetadataAlbum;   // "album" tag, empty when absent
    ::std::vector<::std::pair<::std::string, ::std::string>> MetadataEntries; // all tags as key/value pairs

    ::std::string getEntry(const ::std::string &MetadataKey) const noexcept {
        for (const auto &MetadataEntry : MetadataEntries)
            if (MetadataEntry.first == MetadataKey) return MetadataEntry.second;
        return {};
    }
};

struct MediaSpectrum final {
    ::std::vector<double> SpectrumMagnitude;  // per-bin magnitude in dBFS, Hann window, coherent-gain corrected
    ::std::vector<double> SpectrumPhase;      // per-bin phase in radians, [-pi, pi]
    double SpectrumFrequencyStep = 0;         // Hz between bins, SpectrumMagnitude[k] sits at k * SpectrumFrequencyStep
};

struct MediaLoudness final {
    double LoudnessMomentary = -::std::numeric_limits<double>::infinity();  // LUFS, last 400 ms block
    double LoudnessShortTerm = -::std::numeric_limits<double>::infinity();  // LUFS, last 3 s window
    double LoudnessIntegrated = -::std::numeric_limits<double>::infinity(); // LUFS, EBU R128 gated
};

class AudioSegment {
private:
    MediaChannelLayout AudioChannelLayout;
    ::std::vector<uint8_t> AudioData;
    intmax_t AudioSampleCount = 0; // in per channel
    int AudioSampleRate = 0;
    AudioSegment(const ::std::vector<uint8_t> &AudioDataSource,
                 const MediaChannelLayout &AudioChannelLayoutSource,
                 int AudioSampleRateSource) : AudioChannelLayout(AudioChannelLayoutSource), AudioData(AudioDataSource), AudioSampleCount(AudioDataSource.size() / AudioChannelLayoutSource.getChannelCount() / 2), AudioSampleRate(AudioSampleRateSource) {}

    AudioSegment doResample(const MediaChannelLayout &TargetLayout, int TargetRate) const {
        if (AudioChannelLayout.getChannelCount() == TargetLayout.getChannelCount() && AudioSampleRate == TargetRate)
            return *this;
        AVChannelLayout LayoutIn(AudioChannelLayout.toFFMpegFormat());
        AVChannelLayout LayoutOut(TargetLayout.toFFMpegFormat());
        FFMpeg::MediaSWRContext Ctx(FFMpeg::MediaSWRContext::doAllocate(
            &LayoutIn, &LayoutOut, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16, AudioSampleRate, TargetRate));
        Ctx.doInitialize();
        uint8_t *SrcPtr(const_cast<uint8_t*>(AudioData.data()));
        auto OutSamples = ::av_rescale_rnd(
            ::swr_get_delay((SwrContext*)Ctx, AudioSampleRate) + AudioSampleCount, TargetRate, AudioSampleRate, AV_ROUND_UP);
        ::std::vector<uint8_t> OutData(OutSamples * TargetLayout.getChannelCount() * 2, 0);
        auto DstPtr(OutData.data());
        Ctx.doConvert((const uint8_t**)&SrcPtr, AudioSampleCount, &DstPtr, OutSamples);
        return {OutData, TargetLayout, TargetRate};
    }

    static MediaChannelLayout getCommonChannelLayout(const AudioSegment &Audio0) {
        return Audio0.getChannelLayout();
    }

    static MediaChannelLayout getCommonChannelLayout(const AudioSegment &Audio1, const AudioSegment &Audio2) {
        return Audio1.getChannelLayout().getChannelCount() >= Audio2.getChannelLayout().getChannelCount()
                   ? Audio1.getChannelLayout() : Audio2.getChannelLayout();
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
        return ::std::max(Audio1.getSampleRate(), Audio2.getSampleRate());
    }

    template<typename ...AudioTypes>
    static int getCommonSampleRate(const AudioSegment &Audio1, const AudioSegment &Audio2, AudioTypes ...AudioSources) {
        return ::std::max({Audio1.getSampleRate(), Audio2.getSampleRate(), AudioSources.getSampleRate()...});
    }

    static std::generator<AudioSegment> doSynchronize0(MediaChannelLayout AudioLayout, int AudioRate, const AudioSegment &AudioCurrent) {
        co_yield AudioCurrent.setChannelLayout(AudioLayout).setSampleRate(AudioRate);
    }

    template<typename ...AudioTypes>
    static std::generator<AudioSegment> doSynchronize0(MediaChannelLayout AudioLayout, int AudioRate, const AudioSegment &AudioCurrent, AudioTypes ...AudioSources) {
        co_yield AudioCurrent.setChannelLayout(AudioLayout).setSampleRate(AudioRate);
        co_yield std::ranges::elements_of(doSynchronize0(AudioLayout, AudioRate, AudioSources...));
    }

    AudioSegment doFilterBiquad(bool AudioHighPass, double AudioCutoff) const noexcept {
        uint8_t AudioChannels = AudioChannelLayout.getChannelCount();
        ::std::vector<uint8_t> AudioDataNew(AudioData);
        if (!AudioChannels || !AudioSampleCount || AudioCutoff <= 0 || AudioCutoff >= AudioSampleRate * 0.5)
            return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
        auto AudioDataOutput(reinterpret_cast<int16_t *>(AudioDataNew.data()));
        double AudioOmega = 2 * 3.14159265358979323846 * AudioCutoff / AudioSampleRate;
        double AudioSin = ::std::sin(AudioOmega);
        double AudioCos = ::std::cos(AudioOmega);
        double AudioAlpha = AudioSin * 0.70710678118654752440; // sin(w) / (2Q), Q = 1/sqrt(2)
        double AudioA0 = 1 + AudioAlpha;
        double AudioA1 = -2 * AudioCos;
        double AudioA2 = 1 - AudioAlpha;
        double AudioB0 = AudioHighPass ? (1 + AudioCos) * 0.5 : (1 - AudioCos) * 0.5;
        double AudioB1 = AudioHighPass ? -(1 + AudioCos) : (1 - AudioCos);
        double AudioB2 = AudioB0;
        AudioB0 /= AudioA0;
        AudioB1 /= AudioA0;
        AudioB2 /= AudioA0;
        AudioA1 /= AudioA0;
        AudioA2 /= AudioA0;
        ::std::vector<double> AudioStateX1(AudioChannels, 0);
        ::std::vector<double> AudioStateX2(AudioChannels, 0);
        ::std::vector<double> AudioStateY1(AudioChannels, 0);
        ::std::vector<double> AudioStateY2(AudioChannels, 0);
        for (intmax_t AudioSample = 0; AudioSample < AudioSampleCount; ++AudioSample) {
            for (uint8_t AudioChannel = 0; AudioChannel < AudioChannels; ++AudioChannel) {
                double AudioInput = AudioDataOutput[AudioSample * AudioChannels + AudioChannel];
                double AudioOutput = AudioB0 * AudioInput + AudioB1 * AudioStateX1[AudioChannel]
                                     + AudioB2 * AudioStateX2[AudioChannel]
                                     - AudioA1 * AudioStateY1[AudioChannel] - AudioA2 * AudioStateY2[AudioChannel];
                AudioStateX2[AudioChannel] = AudioStateX1[AudioChannel];
                AudioStateX1[AudioChannel] = AudioInput;
                AudioStateY2[AudioChannel] = AudioStateY1[AudioChannel];
                AudioStateY1[AudioChannel] = AudioOutput;
                AudioDataOutput[AudioSample * AudioChannels + AudioChannel] = static_cast<int16_t>(
                    ::std::clamp(AudioOutput, -32768.0, 32767.0));
            }
        }
        return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
    }

    // Radix-2 decimation-in-time FFT, in place. AudioInverse computes the inverse transform (1/N scaled).
    static void doFFT(::std::vector<double> &AudioDataReal, ::std::vector<double> &AudioDataImag,
                      bool AudioInverse = false) noexcept {
        size_t AudioSize = AudioDataReal.size();
        if (!AudioSize || (AudioSize & (AudioSize - 1))) return;
        for (size_t AudioIndex = 1, AudioReverse = 0; AudioIndex < AudioSize; ++AudioIndex) {
            size_t AudioBit = AudioSize >> 1;
            for (; AudioReverse & AudioBit; AudioBit >>= 1) AudioReverse ^= AudioBit;
            AudioReverse ^= AudioBit;
            if (AudioIndex < AudioReverse) {
                ::std::swap(AudioDataReal[AudioIndex], AudioDataReal[AudioReverse]);
                ::std::swap(AudioDataImag[AudioIndex], AudioDataImag[AudioReverse]);
            }
        }
        for (size_t AudioLength = 2; AudioLength <= AudioSize; AudioLength <<= 1) {
            double AudioAngle = (AudioInverse ? 2 : -2) * 3.14159265358979323846 / AudioLength;
            double AudioStepReal = ::std::cos(AudioAngle);
            double AudioStepImag = ::std::sin(AudioAngle);
            size_t AudioHalf = AudioLength >> 1;
            for (size_t AudioStart = 0; AudioStart < AudioSize; AudioStart += AudioLength) {
                double AudioTwiddleReal = 1;
                double AudioTwiddleImag = 0;
                for (size_t AudioOffset = 0; AudioOffset < AudioHalf; ++AudioOffset) {
                    size_t AudioEvenIndex = AudioStart + AudioOffset;
                    size_t AudioOddIndex = AudioEvenIndex + AudioHalf;
                    double AudioOddReal = AudioTwiddleReal * AudioDataReal[AudioOddIndex]
                                          - AudioTwiddleImag * AudioDataImag[AudioOddIndex];
                    double AudioOddImag = AudioTwiddleReal * AudioDataImag[AudioOddIndex]
                                          + AudioTwiddleImag * AudioDataReal[AudioOddIndex];
                    AudioDataReal[AudioOddIndex] = AudioDataReal[AudioEvenIndex] - AudioOddReal;
                    AudioDataImag[AudioOddIndex] = AudioDataImag[AudioEvenIndex] - AudioOddImag;
                    AudioDataReal[AudioEvenIndex] += AudioOddReal;
                    AudioDataImag[AudioEvenIndex] += AudioOddImag;
                    double AudioTwiddleRealNext = AudioTwiddleReal * AudioStepReal
                                                  - AudioTwiddleImag * AudioStepImag;
                    AudioTwiddleImag = AudioTwiddleReal * AudioStepImag + AudioTwiddleImag * AudioStepReal;
                    AudioTwiddleReal = AudioTwiddleRealNext;
                }
            }
        }
        if (AudioInverse)
            for (double &AudioValue : AudioDataReal) {
                AudioValue /= (double) AudioSize;
            }
        if (AudioInverse)
            for (double &AudioValue : AudioDataImag) {
                AudioValue /= (double) AudioSize;
            }
    }

    // Phase vocoder: pitch-shifts one interleaved channel by AudioFactor, preserving length.
    static void doShiftPitchChannel(const int16_t *AudioDataInput, int16_t *AudioDataOutput,
                                    intmax_t AudioSampleCount, intmax_t AudioSampleStride, double AudioFactor) noexcept {
        constexpr size_t AudioFFTSize = 2048;
        constexpr size_t AudioHop = AudioFFTSize / 4;
        size_t AudioPad = AudioFFTSize - AudioHop;
        size_t AudioPaddedSize = (size_t) AudioSampleCount + 2 * AudioPad;
        ::std::vector<double> AudioAccum(AudioPaddedSize, 0);
        ::std::vector<double> AudioWindow(AudioFFTSize);
        for (size_t AudioIndex = 0; AudioIndex < AudioFFTSize; ++AudioIndex)
            AudioWindow[AudioIndex] = 0.5 - 0.5 * ::std::cos(2 * 3.14159265358979323846 * AudioIndex / AudioFFTSize);
        ::std::vector<double> AudioReal(AudioFFTSize), AudioImag(AudioFFTSize);
        ::std::vector<double> AudioPhasePrevious(AudioFFTSize / 2 + 1, 0);
        ::std::vector<double> AudioPhaseSynthesis(AudioFFTSize / 2 + 1, 0);
        ::std::vector<double> AudioMagnitudeBins(AudioFFTSize / 2 + 1, 0);
        ::std::vector<double> AudioFrequencyBins(AudioFFTSize / 2 + 1, 0);
        for (size_t AudioFrameStart = 0; AudioFrameStart + AudioFFTSize <= AudioPaddedSize;
             AudioFrameStart += AudioHop) {
            for (size_t AudioIndex = 0; AudioIndex < AudioFFTSize; ++AudioIndex) {
                intmax_t AudioSourceIndex = (intmax_t) (AudioFrameStart + AudioIndex) - (intmax_t) AudioPad;
                AudioReal[AudioIndex] = (AudioSourceIndex >= 0 && AudioSourceIndex < AudioSampleCount
                                             ? AudioDataInput[AudioSourceIndex * AudioSampleStride] : 0.0)
                                        * AudioWindow[AudioIndex];
                AudioImag[AudioIndex] = 0;
            }
            doFFT(AudioReal, AudioImag);
            for (size_t AudioBin = 0; AudioBin <= AudioFFTSize / 2; ++AudioBin) {
                AudioMagnitudeBins[AudioBin] = ::std::hypot(AudioReal[AudioBin], AudioImag[AudioBin]);
                double AudioPhase = ::std::atan2(AudioImag[AudioBin], AudioReal[AudioBin]);
                double AudioDeviation = AudioPhase - AudioPhasePrevious[AudioBin]
                                        - 2 * 3.14159265358979323846 * AudioBin * AudioHop / AudioFFTSize;
                AudioDeviation -= 2 * 3.14159265358979323846
                                  * ::std::round(AudioDeviation / (2 * 3.14159265358979323846));
                AudioFrequencyBins[AudioBin] = 2 * 3.14159265358979323846 * AudioBin / AudioFFTSize
                                               + AudioDeviation / AudioHop;
                AudioPhasePrevious[AudioBin] = AudioPhase;
            }
            ::std::fill(AudioReal.begin(), AudioReal.end(), 0.0);
            ::std::fill(AudioImag.begin(), AudioImag.end(), 0.0);
            for (size_t AudioBin = 0; AudioBin <= AudioFFTSize / 2; ++AudioBin) {
                size_t AudioBinTarget = (size_t) (AudioBin * AudioFactor + 0.5);
                if (AudioBinTarget > AudioFFTSize / 2) break;
                AudioPhaseSynthesis[AudioBinTarget] += AudioHop * AudioFrequencyBins[AudioBin];
                double AudioAmplitude = 2 * AudioMagnitudeBins[AudioBin]; // undo Hann gain, IFFT 1/N scaling
                double AudioBinReal = AudioAmplitude * ::std::cos(AudioPhaseSynthesis[AudioBinTarget]);
                double AudioBinImag = AudioAmplitude * ::std::sin(AudioPhaseSynthesis[AudioBinTarget]);
                AudioReal[AudioBinTarget] += AudioBinReal;
                AudioImag[AudioBinTarget] += AudioBinImag;
                if (AudioBinTarget && AudioBinTarget < AudioFFTSize / 2) {
                    AudioReal[AudioFFTSize - AudioBinTarget] += AudioBinReal;
                    AudioImag[AudioFFTSize - AudioBinTarget] -= AudioBinImag;
                }
            }
            doFFT(AudioReal, AudioImag, true);
            for (size_t AudioIndex = 0; AudioIndex < AudioFFTSize; ++AudioIndex)
                AudioAccum[AudioFrameStart + AudioIndex] += AudioReal[AudioIndex] * AudioWindow[AudioIndex];
        }
        // Periodic Hann with hop N/4 sums to 1.5 under overlap-add.
        for (intmax_t AudioSample = 0; AudioSample < AudioSampleCount; ++AudioSample)
            AudioDataOutput[AudioSample * AudioSampleStride] = static_cast<int16_t>(
                ::std::clamp(AudioAccum[AudioSample + AudioPad] / 1.5, -32768.0, 32767.0));
    }

    // EBU R128: per-block mean-square values of K-weighted samples (400 ms blocks, 100 ms hop).
    ::std::vector<double> doLoudnessBlocks() const {
        if (!AudioSampleCount) return {0.0};
        double AudioRate = (double) AudioSampleRate;
        double AudioK1 = ::std::tan(3.14159265358979323846 * 1681.974450955533 / AudioRate);
        double AudioVh = ::std::pow(10.0, 3.999843853973347 / 20.0);
        double AudioVb = ::std::pow(AudioVh, 0.4996667741545416);
        double AudioQ1 = 0.7071752369554196;
        double AudioA0S = 1 + AudioK1 / AudioQ1 + AudioK1 * AudioK1;
        double AudioB0S = (AudioVh + AudioVb * AudioK1 / AudioQ1 + AudioK1 * AudioK1) / AudioA0S;
        double AudioB1S = 2 * (AudioK1 * AudioK1 - AudioVh) / AudioA0S;
        double AudioB2S = (AudioVh - AudioVb * AudioK1 / AudioQ1 + AudioK1 * AudioK1) / AudioA0S;
        double AudioA1S = 2 * (AudioK1 * AudioK1 - 1) / AudioA0S;
        double AudioA2S = (1 - AudioK1 / AudioQ1 + AudioK1 * AudioK1) / AudioA0S;
        double AudioK2 = ::std::tan(3.14159265358979323846 * 38.13547087602444 / AudioRate);
        double AudioQ2 = 0.5003270373238773;
        double AudioA0H = 1 + AudioK2 / AudioQ2 + AudioK2 * AudioK2;
        double AudioB0H = 1 / AudioA0H;
        double AudioB1H = -2 / AudioA0H;
        double AudioB2H = AudioB0H;
        double AudioA1H = 2 * (AudioK2 * AudioK2 - 1) / AudioA0H;
        double AudioA2H = (1 - AudioK2 / AudioQ2 + AudioK2 * AudioK2) / AudioA0H;
        uint8_t AudioChannels = AudioChannelLayout.getChannelCount();
        auto AudioDataPointer(reinterpret_cast<const int16_t *>(AudioData.data()));
        ::std::vector<double> AudioWeighted((uintmax_t) AudioSampleCount, 0);
        ::std::vector<double> AudioStateX1S(AudioChannels, 0), AudioStateX2S(AudioChannels, 0);
        ::std::vector<double> AudioStateY1S(AudioChannels, 0), AudioStateY2S(AudioChannels, 0);
        ::std::vector<double> AudioStateX1H(AudioChannels, 0), AudioStateX2H(AudioChannels, 0);
        ::std::vector<double> AudioStateY1H(AudioChannels, 0), AudioStateY2H(AudioChannels, 0);
        for (intmax_t AudioSample = 0; AudioSample < AudioSampleCount; ++AudioSample) {
            double AudioSampleSum = 0;
            for (uint8_t AudioChannel = 0; AudioChannel < AudioChannels; ++AudioChannel) {
                // Normalize to full scale: EBU R128 works on -1..1 (z of a full-scale sine is 0.5).
                double AudioInput = AudioDataPointer[AudioSample * AudioChannels + AudioChannel] / 32768.0;
                double AudioShelf = AudioB0S * AudioInput + AudioB1S * AudioStateX1S[AudioChannel]
                                    + AudioB2S * AudioStateX2S[AudioChannel]
                                    - AudioA1S * AudioStateY1S[AudioChannel] - AudioA2S * AudioStateY2S[AudioChannel];
                AudioStateX2S[AudioChannel] = AudioStateX1S[AudioChannel];
                AudioStateX1S[AudioChannel] = AudioInput;
                AudioStateY2S[AudioChannel] = AudioStateY1S[AudioChannel];
                AudioStateY1S[AudioChannel] = AudioShelf;
                double AudioOutput = AudioB0H * AudioShelf + AudioB1H * AudioStateX1H[AudioChannel]
                                     + AudioB2H * AudioStateX2H[AudioChannel]
                                     - AudioA1H * AudioStateY1H[AudioChannel] - AudioA2H * AudioStateY2H[AudioChannel];
                AudioStateX2H[AudioChannel] = AudioStateX1H[AudioChannel];
                AudioStateX1H[AudioChannel] = AudioShelf;
                AudioStateY2H[AudioChannel] = AudioStateY1H[AudioChannel];
                AudioStateY1H[AudioChannel] = AudioOutput;
                AudioSampleSum += AudioOutput * AudioOutput;
            }
            AudioWeighted[AudioSample] = AudioSampleSum;
        }
        intmax_t AudioBlockLength = ::std::max((intmax_t) 1, (intmax_t) (AudioSampleRate * 0.4));
        intmax_t AudioBlockHop = ::std::max((intmax_t) 1, (intmax_t) (AudioSampleRate * 0.1));
        ::std::vector<double> AudioBlocks;
        auto doLoudnessBlockSum = [&](intmax_t AudioBlockStart, intmax_t AudioBlockStop) {
            double AudioBlockSum = 0;
            for (intmax_t AudioSample = AudioBlockStart; AudioSample < AudioBlockStop; ++AudioSample)
                AudioBlockSum += AudioWeighted[AudioSample];
            AudioBlocks.push_back(AudioBlockSum / ((double) AudioChannels * (AudioBlockStop - AudioBlockStart)));
        };
        if (AudioSampleCount <= AudioBlockLength) {
            doLoudnessBlockSum(0, AudioSampleCount);
            return AudioBlocks;
        }
        for (intmax_t AudioBlockStart = 0; AudioBlockStart + AudioBlockLength <= AudioSampleCount;
             AudioBlockStart += AudioBlockHop)
            doLoudnessBlockSum(AudioBlockStart, AudioBlockStart + AudioBlockLength);
        doLoudnessBlockSum(AudioSampleCount - AudioBlockLength, AudioSampleCount);
        return AudioBlocks;
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
    }

    void doAssign(AudioSegment &&AudioSource) noexcept {
        if (::std::addressof(AudioSource) == this) return;
        AudioChannelLayout = AudioSource.AudioChannelLayout;
        AudioData = ::std::move(AudioSource.AudioData);
        AudioSampleCount = AudioSource.AudioSampleCount;
        AudioSampleRate = AudioSource.AudioSampleRate;
    }

    AudioSegment doCompressDynamicRange(double AudioThreshold = -20, double AudioRatio = 4, double AudioAttack = 5,
                                        double AudioRelease = 50) const noexcept {
        if (AudioRatio <= 0) return *this;
        ::std::vector<uint8_t> AudioDataNew(AudioData);
        auto AudioDataOutput(reinterpret_cast<int16_t *>(AudioDataNew.data()));
        double AudioThresholdLinear = dB2Ratio(AudioThreshold) * getMaximumPossibleAmplitude();
        double AudioAttackCoef = AudioAttack > 0 ? ::std::exp(-1.0 / (AudioSampleRate * AudioAttack / 1000.0)) : 0;
        double AudioReleaseCoef = AudioRelease > 0 ? ::std::exp(-1.0 / (AudioSampleRate * AudioRelease / 1000.0)) : 0;
        double AudioRatioReciprocal = 1.0 / AudioRatio;
        double AudioEnvelope = 0;
        uintmax_t AudioTotalSamples = AudioSampleCount * AudioChannelLayout.getChannelCount();
        for (uintmax_t AudioSample = 0; AudioSample < AudioTotalSamples; ++AudioSample) {
            double AudioLevel = ::std::abs((double) AudioDataOutput[AudioSample]);
            double AudioCoef = AudioLevel > AudioEnvelope ? AudioAttackCoef : AudioReleaseCoef;
            AudioEnvelope = AudioCoef * AudioEnvelope + (1 - AudioCoef) * AudioLevel;
            if (AudioEnvelope > AudioThresholdLinear) {
                double AudioGainReduction = AudioThresholdLinear +
                                            (AudioEnvelope - AudioThresholdLinear) * AudioRatioReciprocal;
                double AudioGain = AudioGainReduction / AudioEnvelope;
                AudioDataOutput[AudioSample] = static_cast<int16_t>(
                    ::std::clamp(AudioDataOutput[AudioSample] * AudioGain, -32768.0, 32767.0));
            }
        }
        return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
    }

    AudioSegment doConcat(const AudioSegment &AudioSource) const {
        int AudioCommonRate = getCommonSampleRate(*this, AudioSource);
        MediaChannelLayout AudioCommonLayout = getCommonChannelLayout(*this, AudioSource);
        auto Audio1 = doResample(AudioCommonLayout, AudioCommonRate);
        auto Audio2 = AudioSource.doResample(AudioCommonLayout, AudioCommonRate);
        ::std::vector<uint8_t> AudioVector(Audio1.AudioData);
        AudioVector.reserve(AudioVector.size() + Audio2.AudioData.size());
        AudioVector.insert(AudioVector.end(), Audio2.AudioData.begin(), Audio2.AudioData.end());
        return {AudioVector, AudioCommonLayout, AudioCommonRate};
    }

    void doExport(const ::std::string &AudioPath, const MediaExportOption &AudioOption = MediaExportOption()) const {
        AudioSegment AudioExportSource = AudioOption.ExportSampleRate > 0
            ? setSampleRate(AudioOption.ExportSampleRate) : *this;
        FFMpeg::MediaFormatContext AudioFormatContext(AudioOption.ExportFormat.empty()
            ? FFMpeg::MediaFormatContext::doAllocateOutput(AudioPath)
            : FFMpeg::MediaFormatContext::doAllocateOutput(AudioPath, AudioOption.ExportFormat));
        if (::avio_open(&AudioFormatContext->pb, AudioPath.c_str(), AVIO_FLAG_WRITE) < 0)
            doThrowChecked(::std::runtime_error,
                       "AudioSegment::doExport(const ::std::string&, const MediaExportOption&) ::avio_open");
        FFMpeg::MediaCodec AudioCodec(AudioOption.ExportCodec.empty()
            ? FFMpeg::MediaCodec::doFindEncoder(AudioFormatContext->oformat->audio_codec)
            : FFMpeg::MediaCodec::doFindEncoderByName(AudioOption.ExportCodec));
        FFMpeg::MediaCodecContext AudioCodecContext(FFMpeg::MediaCodecContext::doAllocate(AudioCodec));
        AVChannelLayout AudioChannelLayoutSource(AudioExportSource.AudioChannelLayout.toFFMpegFormat());
        ::av_channel_layout_copy(&AudioCodecContext->ch_layout, &AudioChannelLayoutSource);
        const enum AVSampleFormat *AudioSampleFormats = nullptr;
        if (::avcodec_get_supported_config((AVCodecContext *) AudioCodecContext, (const AVCodec *) AudioCodec,
                                           AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void **) &AudioSampleFormats, nullptr) < 0
            || !AudioSampleFormats)
            doThrowChecked(::std::runtime_error,
                       "AudioSegment::doExport(const ::std::string&, const MediaExportOption&) ::avcodec_get_supported_config");
        AudioCodecContext->sample_fmt = AudioSampleFormats[0];
        AudioCodecContext->sample_rate = (int) AudioExportSource.AudioSampleRate;
        if (AudioOption.ExportBitrate > 0) AudioCodecContext->bit_rate = (int64_t) AudioOption.ExportBitrate;
        AudioCodecContext.doOpen(AudioCodec);
        AVStream *AudioStreamObject = ::avformat_new_stream((AVFormatContext *) AudioFormatContext,
                                                            (const AVCodec *) AudioCodec);
        if (!AudioStreamObject)
            doThrowChecked(::std::runtime_error,
                       "AudioSegment::doExport(const ::std::string&, const MediaExportOption&) ::avformat_new_stream");
        if (::avcodec_parameters_from_context(AudioStreamObject->codecpar, (AVCodecContext *) AudioCodecContext))
            doThrowChecked(::std::runtime_error,
                       "AudioSegment::doExport(const ::std::string&, const MediaExportOption&) ::avcodec_parameters_from_context");
        AudioFormatContext.doWriteHeader();
        FFMpeg::MediaSWRContext AudioSWRContext(FFMpeg::MediaSWRContext::doAllocate(
            &AudioCodecContext->ch_layout, &AudioCodecContext->ch_layout, AV_SAMPLE_FMT_S16,
            AudioCodecContext->sample_fmt, AudioCodecContext->sample_rate, AudioCodecContext->sample_rate));
        AudioSWRContext.doInitialize();
        FFMpeg::MediaFrame AudioFrame(FFMpeg::MediaFrame::doAllocate());
        int AudioFrameSizeMaximum = AudioCodecContext->frame_size > 0 ? AudioCodecContext->frame_size : 2048;
        ::av_channel_layout_copy(&AudioFrame->ch_layout, &AudioCodecContext->ch_layout);
        AudioFrame->format = AudioCodecContext->sample_fmt;
        AudioFrame->nb_samples = AudioFrameSizeMaximum;
        AudioFrame->sample_rate = AudioCodecContext->sample_rate;
        AudioFrame.getFrameBuffer();
        FFMpeg::MediaPacket AudioPacket(FFMpeg::MediaPacket::doAllocate());
        uint32_t AudioSampleCurrent = 0;
        ::std::vector<uint8_t> AudioDataSample(
            AudioExportSource.AudioChannelLayout.getChannelCount() * AudioFrameSizeMaximum * 2);
        for (;;) {
            if (AudioSampleCurrent < AudioExportSource.AudioSampleCount) {
                int AudioFrameSize = ::std::min(AudioFrameSizeMaximum,
                                                int(AudioExportSource.AudioSampleCount - AudioSampleCurrent));
                AudioFrame->nb_samples = AudioFrameSize;
                AudioFrame->pts = AudioSampleCurrent;
                std::copy(AudioExportSource.AudioData.begin() + AudioSampleCurrent * 2 * AudioExportSource.AudioChannelLayout.getChannelCount(), AudioExportSource.AudioData.begin() + (AudioSampleCurrent + AudioFrameSize) * 2 * AudioExportSource.AudioChannelLayout.getChannelCount(), AudioDataSample.begin());
                AudioSampleCurrent += AudioFrameSize;
                auto *AudioDataSamplePtr = AudioDataSample.data();
                AudioSWRContext.doConvert((const uint8_t **) &AudioDataSamplePtr, AudioFrameSize,
                                          AudioFrame->extended_data, AudioFrameSize);
            } else AudioFrame.doDestroy();
            AudioCodecContext.doSendFrame(AudioFrame);
            int AudioStatus;
            while (!(AudioStatus = ::avcodec_receive_packet((AVCodecContext *) AudioCodecContext,
                                                          (AVPacket *) AudioPacket)))
                AudioFormatContext.doWriteFrame(AudioPacket);
            if (AudioStatus == AVERROR_EOF) break;
            if (AudioStatus != AVERROR(EAGAIN))
                doThrowChecked(::std::runtime_error,
                           "AudioSegment::doExport(const ::std::string&, const MediaExportOption&) ::avcodec_receive_packet");
        }
        AudioFormatContext.doWriteTrailer();
    }

    AudioSegment doFade(double AudioGainTo = 0, double AudioGainFrom = 0, intmax_t AudioStart = 0,
                        intmax_t AudioStop = 0, intmax_t AudioDuration = 0) const {
        if (AudioGainTo == 0 && AudioGainFrom == 0) return *this;
        intmax_t AudioDurationTotal = (intmax_t)(getDuration() * 1000);
        if (AudioStart < 0) AudioStart += AudioDurationTotal;
        if (AudioStop <= 0) {
            if (AudioDuration <= 0) AudioStop = AudioDurationTotal;
            else AudioStop = AudioStart + AudioDuration;
        }
        if (AudioStop < 0) AudioStop += AudioDurationTotal;
        if (AudioStop <= AudioStart) return *this;
        double AudioGainDelta = AudioGainTo - AudioGainFrom;
        ::std::vector<uint8_t> AudioDataNew(AudioData);
        auto AudioDataOutput(reinterpret_cast<int16_t *>(AudioDataNew.data()));
        uintmax_t AudioFadeStartSample = (uintmax_t)(AudioStart * AudioSampleRate / 1000);
        uintmax_t AudioFadeStopSample = (uintmax_t)(AudioStop * AudioSampleRate / 1000);
        if (AudioFadeStopSample > (uintmax_t)AudioSampleCount) AudioFadeStopSample = (uintmax_t)AudioSampleCount;
        if (AudioFadeStopSample <= AudioFadeStartSample) return *this;
        uint8_t AudioChannels = AudioChannelLayout.getChannelCount();
        double AudioFadeInvRange = 1.0 / (AudioFadeStopSample - AudioFadeStartSample);
        for (uintmax_t AudioSample = AudioFadeStartSample; AudioSample < AudioFadeStopSample; ++AudioSample) {
            double AudioProgress = (double)(AudioSample - AudioFadeStartSample) * AudioFadeInvRange;
            double AudioGain = dB2Ratio(AudioGainFrom + AudioGainDelta * AudioProgress);
            for (uint8_t AudioChannel = 0; AudioChannel < AudioChannels; ++AudioChannel) {
                AudioDataOutput[AudioSample * AudioChannels + AudioChannel] = static_cast<int16_t>(
                    ::std::clamp(AudioDataOutput[AudioSample * AudioChannels + AudioChannel] * AudioGain, -32768.0, 32767.0));
            }
        }
        return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
    }

    AudioSegment doFadeIn(intmax_t AudioDuration) const noexcept {
        return doFade(0, -120., 0, -1, AudioDuration);
    }

    AudioSegment doFadeOut(intmax_t AudioDuration) const noexcept {
        return doFade(-120., 0, (intmax_t)(getDuration() * 1000.) - AudioDuration, -1, AudioDuration);
    }

    AudioSegment doGain(double AudioGain) const noexcept {
        ::std::vector<uint8_t> AudioDataNew(AudioData);
        auto AudioDataOutput(reinterpret_cast<int16_t *>(AudioDataNew.data()));
        double AudioGainLinear = dB2Ratio(AudioGain);
        uintmax_t AudioTotalSamples = AudioSampleCount * AudioChannelLayout.getChannelCount();
        for (uintmax_t AudioSample = 0; AudioSample < AudioTotalSamples; ++AudioSample) {
            AudioDataOutput[AudioSample] = static_cast<int16_t>(::std::clamp(AudioDataOutput[AudioSample] * AudioGainLinear, -32768.0, 32767.0));
        }
        return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
    }

    AudioSegment doNormalize(double AudioHeadroom = 0.1) const noexcept {
        auto AudioMaximum(getMaximum());
        if (!AudioMaximum) return *this;
        return doGain(ratio2dB(getMaximumPossibleAmplitude() * dB2Ratio(-AudioHeadroom) / AudioMaximum));
    }

    AudioSegment doFilterLow(double AudioCutoff) const {
        if (AudioCutoff <= 0 || AudioCutoff >= AudioSampleRate * 0.5)
            doThrowChecked(::std::invalid_argument, "AudioSegment::doFilterLow(double) Invalid cutoff frequency");
        return doFilterBiquad(false, AudioCutoff);
    }

    AudioSegment doFilterHigh(double AudioCutoff) const {
        if (AudioCutoff <= 0 || AudioCutoff >= AudioSampleRate * 0.5)
            doThrowChecked(::std::invalid_argument, "AudioSegment::doFilterHigh(double) Invalid cutoff frequency");
        return doFilterBiquad(true, AudioCutoff);
    }

    AudioSegment doFilterBand(double AudioCutoffLow, double AudioCutoffHigh) const {
        if (AudioCutoffLow <= 0 || AudioCutoffHigh <= 0 || AudioCutoffLow >= AudioCutoffHigh
            || AudioCutoffHigh >= AudioSampleRate * 0.5)
            doThrowChecked(::std::invalid_argument, "AudioSegment::doFilterBand(double, double) Invalid cutoff range");
        return doFilterHigh(AudioCutoffLow).doFilterLow(AudioCutoffHigh);
    }

    AudioSegment doPan(double AudioPan) const {
        if (AudioPan < -1 || AudioPan > 1)
            doThrowChecked(::std::invalid_argument, "AudioSegment::doPan(double) pan out of range");
        double AudioTheta = (AudioPan + 1) * 3.14159265358979323846 * 0.25;
        double AudioGainLeft = ::std::cos(AudioTheta);
        double AudioGainRight = ::std::sin(AudioTheta);
        uint8_t AudioChannels = AudioChannelLayout.getChannelCount();
        auto AudioDataPointer(reinterpret_cast<const int16_t *>(AudioData.data()));
        if (AudioChannels == 1) {
            ::std::vector<uint8_t> AudioDataNew((uintmax_t) AudioSampleCount * 4, 0);
            auto AudioDataOutput(reinterpret_cast<int16_t *>(AudioDataNew.data()));
            for (intmax_t AudioSample = 0; AudioSample < AudioSampleCount; ++AudioSample) {
                double AudioValue = AudioDataPointer[AudioSample];
                AudioDataOutput[AudioSample * 2] = static_cast<int16_t>(
                    ::std::clamp(AudioValue * AudioGainLeft, -32768.0, 32767.0));
                AudioDataOutput[AudioSample * 2 + 1] = static_cast<int16_t>(
                    ::std::clamp(AudioValue * AudioGainRight, -32768.0, 32767.0));
            }
            return {AudioDataNew, LayoutStereo, AudioSampleRate};
        }
        if (AudioChannels != 2)
            doThrowChecked(::std::invalid_argument, "AudioSegment::doPan(double) only mono or stereo audio");
        ::std::vector<uint8_t> AudioDataNew(AudioData);
        auto AudioDataOutput(reinterpret_cast<int16_t *>(AudioDataNew.data()));
        for (intmax_t AudioSample = 0; AudioSample < AudioSampleCount; ++AudioSample) {
            AudioDataOutput[AudioSample * 2] = static_cast<int16_t>(
                ::std::clamp(AudioDataOutput[AudioSample * 2] * AudioGainLeft, -32768.0, 32767.0));
            AudioDataOutput[AudioSample * 2 + 1] = static_cast<int16_t>(
                ::std::clamp(AudioDataOutput[AudioSample * 2 + 1] * AudioGainRight, -32768.0, 32767.0));
        }
        return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
    }

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
                ::std::vector<uint8_t> AudioDataBuffer(
                    AudioCodecContext->ch_layout.nb_channels * AudioFrame->nb_samples * AudioSampleWidthOutput);
                auto *AudioDataBufferPtr = AudioDataBuffer.data();
                AudioSWRContext.doConvert((const uint8_t **) AudioFrame->extended_data, AudioFrame->nb_samples,
                                          (uint8_t **) &AudioDataBufferPtr, AudioFrame->nb_samples);
                AudioDataOutput.insert(AudioDataOutput.end(), AudioDataBuffer.begin(), AudioDataBuffer.end());
            }
            if (AudioStatus != AVERROR(EAGAIN))
                doThrowChecked(::std::runtime_error, "AudioSegment::doOpen(const ::std::string&) ::avcodec_receive_frame");
        }
        return {AudioDataOutput, AudioCodecContext->ch_layout, AudioCodecContext->sample_rate};
    }

    // Reads the container metadata tags (title/artist/album and any other entries) without decoding audio.
    static MediaMetadata doGetMetadata(const ::std::string &AudioPath) {
        FFMpeg::MediaFormatContext AudioFormatContext(FFMpeg::MediaFormatContext::doOpen(AudioPath));
        AudioFormatContext.doFindStreamInformation();
        MediaMetadata AudioMetadata;
        for (const AVDictionaryEntry *AudioEntry = ::av_dict_iterate(AudioFormatContext->metadata, nullptr);
             AudioEntry; AudioEntry = ::av_dict_iterate(AudioFormatContext->metadata, AudioEntry)) {
            ::std::string AudioKey(AudioEntry->key);
            ::std::string AudioValue(AudioEntry->value);
            AudioMetadata.MetadataEntries.emplace_back(AudioKey, AudioValue);
            if (AudioKey == "title") AudioMetadata.MetadataTitle = AudioValue;
            else if (AudioKey == "artist") AudioMetadata.MetadataArtist = AudioValue;
            else if (AudioKey == "album") AudioMetadata.MetadataAlbum = AudioValue;
        }
        return AudioMetadata;
    }

    static AudioSegment doGenerate(double AudioFrequency = 440, double AudioDuration = 1.0,
                                   int AudioSampleRateSource = 44100, double AudioAmplitude = 0.8) {
        if (AudioFrequency < 0) AudioFrequency = 0;
        uintmax_t AudioSampleCountSource = (uintmax_t)(AudioDuration * AudioSampleRateSource);
        ::std::vector<uint8_t> AudioDataGen(AudioSampleCountSource * 2, 0);
        auto AudioSamples(reinterpret_cast<int16_t *>(AudioDataGen.data()));
        if (AudioFrequency > 0) {
            double AudioPhaseStep = 2 * 3.14159265358979323846 * AudioFrequency / AudioSampleRateSource;
            for (uintmax_t i = 0; i < AudioSampleCountSource; ++i)
                AudioSamples[i] = (int16_t)(AudioAmplitude * 32767.0 *
                                            ::std::sin(AudioPhaseStep * i));
        }
        return {AudioDataGen, LayoutMono, AudioSampleRateSource};
    }

    AudioSegment doOverlay(const AudioSegment &AudioSource, uintmax_t AudioPosition, bool AudioLoop = false,
                           intmax_t AudioLoopTime = -1) const {
        int AudioCommonRate = getCommonSampleRate(*this, AudioSource);
        MediaChannelLayout AudioCommonLayout = getCommonChannelLayout(*this, AudioSource);
        AudioSegment AudioBase = doResample(AudioCommonLayout, AudioCommonRate);
        AudioSegment AudioOverlay = AudioSource.doResample(AudioCommonLayout, AudioCommonRate);
        uintmax_t AudioOverlayStartSample = (uintmax_t)(AudioPosition * AudioBase.AudioSampleRate / 1000);
        uintmax_t AudioOverlayChannels = AudioBase.AudioChannelLayout.getChannelCount();
        intmax_t AudioOverlayDurationSamples;
        if (AudioLoopTime >= 0)
            AudioOverlayDurationSamples = AudioLoopTime * AudioBase.AudioSampleRate / 1000;
        else if (AudioLoop)
            AudioOverlayDurationSamples = (intmax_t)(AudioBase.AudioSampleCount - AudioOverlayStartSample);
        else
            AudioOverlayDurationSamples = (intmax_t)AudioOverlay.AudioSampleCount;
        if (AudioOverlayDurationSamples <= 0 || !AudioOverlay.AudioSampleCount) return *this;
        uintmax_t AudioResultSamples = ::std::max(AudioOverlayStartSample + (uintmax_t)AudioOverlayDurationSamples,
                                                  (uintmax_t)AudioBase.AudioSampleCount);
        ::std::vector<uint8_t> AudioDataNew(AudioResultSamples * AudioOverlayChannels * 2, 0);
        auto AudioDataOutput(reinterpret_cast<int16_t *>(AudioDataNew.data()));
        auto AudioBasePointer(reinterpret_cast<const int16_t *>(AudioBase.AudioData.data()));
        for (uintmax_t i = 0; i < AudioBase.AudioSampleCount * AudioOverlayChannels; ++i)
            AudioDataOutput[i] = AudioBasePointer[i];
        uintmax_t AudioOverlayTotalSamples = (uintmax_t)AudioOverlay.AudioSampleCount * AudioOverlayChannels;
        auto AudioOverlayPointer(reinterpret_cast<const int16_t *>(AudioOverlay.AudioData.data()));
        for (uintmax_t i = 0; i < (uintmax_t)AudioOverlayDurationSamples * AudioOverlayChannels; ++i) {
            uintmax_t AudioOverlayIdx = AudioLoop ? i % AudioOverlayTotalSamples : i;
            AudioDataOutput[AudioOverlayStartSample * AudioOverlayChannels + i] = static_cast<int16_t>(
                ::std::clamp((int32_t)AudioDataOutput[AudioOverlayStartSample * AudioOverlayChannels + i] +
                             AudioOverlayPointer[AudioOverlayIdx], -32768, 32767));
        }
        return {AudioDataNew, AudioBase.AudioChannelLayout, AudioBase.AudioSampleRate};
    }

    AudioSegment doSpeedUp(double AudioFactor) const {
        if (AudioFactor <= 0)
            doThrowChecked(::std::invalid_argument, "AudioSegment::doSpeedUp(double) factor must be positive");
        if (AudioFactor == 1.0) return *this;
        int AudioNewRate = ::std::max(1, (int)(AudioSampleRate / AudioFactor));
        AVChannelLayout AudioChannelLayoutSource(AudioChannelLayout.toFFMpegFormat());
        FFMpeg::MediaSWRContext AudioSWRContext(FFMpeg::MediaSWRContext::doAllocate(
            &AudioChannelLayoutSource, &AudioChannelLayoutSource, AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16, AudioSampleRate,
            AudioNewRate));
        AudioSWRContext.doInitialize();
        uint8_t *AudioDataPointer(const_cast<uint8_t *>(AudioData.data()));
        auto AudioDataOutputSample = ::av_rescale_rnd(
            ::swr_get_delay((SwrContext *) AudioSWRContext, AudioSampleRate) + AudioSampleCount, AudioNewRate,
            AudioSampleRate, AV_ROUND_UP);
        ::std::vector<uint8_t> AudioDataOutput(
            AudioDataOutputSample * AudioChannelLayout.getChannelCount() * 2, 0);
        auto AudioDataOutputPointer(AudioDataOutput.data());
        AudioSWRContext.doConvert((const uint8_t **) &AudioDataPointer, (int)AudioSampleCount,
                                  &AudioDataOutputPointer, AudioDataOutputSample);
        return {AudioDataOutput, AudioChannelLayout, AudioSampleRate};
    }

    // Pitch shift by AudioSemitone semitones (12 = one octave up) without changing the duration.
    // Phase vocoder: overlap-add FFT frames with instantaneous-frequency phase propagation.
    AudioSegment doShiftPitch(double AudioSemitone) const {
        if (AudioSemitone == 0) return *this;
        double AudioFactor = ::std::pow(2.0, AudioSemitone / 12.0);
        uint8_t AudioChannels = AudioChannelLayout.getChannelCount();
        ::std::vector<uint8_t> AudioDataNew(AudioData.size(), 0);
        if (!AudioSampleCount) return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
        auto AudioDataInput(reinterpret_cast<const int16_t *>(AudioData.data()));
        auto AudioDataOutput(reinterpret_cast<int16_t *>(AudioDataNew.data()));
        for (uint8_t AudioChannel = 0; AudioChannel < AudioChannels; ++AudioChannel)
            doShiftPitchChannel(AudioDataInput + AudioChannel, AudioDataOutput + AudioChannel,
                                AudioSampleCount, AudioChannels, AudioFactor);
        return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
    }

    static std::generator<AudioSegment> doSynchronize(const AudioSegment &AudioCurrent) {
        co_yield AudioCurrent;
    }

    template<typename ...AudioTypes>
    static std::generator<AudioSegment> doSynchronize(const AudioSegment &AudioCurrent, AudioTypes ...AudioSources) {
        co_yield std::ranges::elements_of(doSynchronize0(getCommonChannelLayout(AudioCurrent, AudioSources...), getCommonSampleRate(AudioCurrent, AudioSources...), AudioCurrent, AudioSources...));
    }

    AudioSegment doRepeat(uintmax_t AudioCount) const {
        ::std::vector<uint8_t> AudioDataNew;
        AudioDataNew.reserve(AudioData.size() * AudioCount);
        for (uintmax_t AudioIndex = 0; AudioIndex < AudioCount; ++AudioIndex)
            AudioDataNew.insert(AudioDataNew.end(), AudioData.begin(), AudioData.end());
        return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
    }

    AudioSegment doReverse() const noexcept {
        size_t AudioFrameSize = AudioChannelLayout.getChannelCount() * 2;
        size_t AudioFrameCount = AudioData.size() / AudioFrameSize;
        ::std::vector<uint8_t> AudioReversed(AudioData.size());
        for (size_t AudioFrame = 0; AudioFrame < AudioFrameCount; ++AudioFrame) {
            auto AudioSourceBegin = AudioData.begin() + (AudioFrameCount - AudioFrame - 1) * AudioFrameSize;
            ::std::copy(AudioSourceBegin, AudioSourceBegin + AudioFrameSize,
                        AudioReversed.begin() + AudioFrame * AudioFrameSize);
        }
        return {AudioReversed, AudioChannelLayout, AudioSampleRate};
    }

    AudioSegment doSlice(uintmax_t AudioStart, uintmax_t AudioStop, uintmax_t AudioStep) const {
        if (AudioStep == 0)
            doThrowChecked(::std::invalid_argument,
                           "AudioSegment::doSlice(uintmax_t, uintmax_t, uintmax_t) step cannot be zero");
        AudioStart = (uintmax_t)(AudioStart * AudioSampleRate / 1000);
        AudioStop = (uintmax_t)(AudioStop * AudioSampleRate / 1000);
        if (AudioStop > (uintmax_t)AudioSampleCount) AudioStop = (uintmax_t)AudioSampleCount;
        if (AudioStart >= AudioStop)
            return {::std::vector<uint8_t>(), AudioChannelLayout, AudioSampleRate};
        ::std::vector<uint8_t> AudioDataNew;
        AudioDataNew.reserve((AudioStop - AudioStart + AudioStep - 1) / AudioStep *
                             AudioChannelLayout.getChannelCount() * 2);
        for (uintmax_t i = AudioStart; i < AudioStop; i += AudioStep) {
            auto AudioBegin = AudioData.begin() + i * AudioChannelLayout.getChannelCount() * 2;
            auto AudioEnd = AudioBegin + AudioChannelLayout.getChannelCount() * 2;
            AudioDataNew.insert(AudioDataNew.end(), AudioBegin, AudioEnd);
        }
        return {AudioDataNew, AudioChannelLayout, AudioSampleRate};
    }

    AudioSegment doSlice(intmax_t AudioPosition) const {
        intmax_t AudioDurationMilliseconds = (intmax_t)(getDuration() * 1000);
        if (AudioPosition < 0) AudioPosition += AudioDurationMilliseconds;
        if (AudioPosition < 0 || AudioPosition > AudioDurationMilliseconds)
            doThrowChecked(::std::runtime_error, "AudioSegment::doSlice(intmax_t) Index out of bounds");
        intmax_t AudioByteOffset = (intmax_t)(AudioPosition * (AudioSampleRate / 1000.0)) *
                                   AudioChannelLayout.getChannelCount() * 2;
        intmax_t AudioByteOffsetEnd = (intmax_t)((AudioPosition + 1) * (AudioSampleRate / 1000.0)) *
                                      AudioChannelLayout.getChannelCount() * 2;
        intmax_t AudioByteCount = (intmax_t)AudioData.size();
        if (AudioByteOffset > AudioByteCount) AudioByteOffset = AudioByteCount;
        if (AudioByteOffsetEnd > AudioByteCount) AudioByteOffsetEnd = AudioByteCount;
        return {{AudioData.begin() + AudioByteOffset, AudioData.begin() + AudioByteOffsetEnd},
                AudioChannelLayout, AudioSampleRate};
    }

    std::generator<AudioSegment> doSplitChannels() const noexcept {
        uint8_t AudioChannels = AudioChannelLayout.getChannelCount();
        intmax_t AudioFrameSize = 2;
        for (uint8_t AudioChannelIdx = 0; AudioChannelIdx < AudioChannels; ++AudioChannelIdx) {
            ::std::vector<uint8_t> AudioChannelData(AudioSampleCount * 2);
            auto AudioSrcBase = AudioData.data() + (uintmax_t)AudioChannelIdx * 2;
            uintmax_t AudioSrcStride = (uintmax_t)AudioChannels * 2;
            auto AudioDstPtr = AudioChannelData.data();
            for (intmax_t i = 0; i < AudioSampleCount; ++i, AudioDstPtr += AudioFrameSize) {
                auto AudioSrc = AudioSrcBase + (uintmax_t)i * AudioSrcStride;
                ::std::copy_n(AudioSrc, AudioFrameSize, AudioDstPtr);
            }
            co_yield {::std::move(AudioChannelData), LayoutMono, AudioSampleRate};
        }
    }

    std::generator<AudioSegment> doSplitOnSilence() const noexcept {
        intmax_t AudioMinSilenceLen = 1000;
        double AudioSilenceThresh = -40;
        intmax_t AudioKeepSilence = 100;
        intmax_t AudioMinSilenceSamples = AudioMinSilenceLen * AudioSampleRate / 1000;
        double AudioSilenceThreshLinear = dB2Ratio(AudioSilenceThresh) * getMaximumPossibleAmplitude();
        intmax_t AudioKeepSilenceSamples = AudioKeepSilence * AudioSampleRate / 1000;
        intmax_t AudioChannels = AudioChannelLayout.getChannelCount();
        auto AudioDataPointer(reinterpret_cast<const int16_t *>(AudioData.data()));
        intmax_t AudioSegmentStart = 0;
        intmax_t AudioSilenceRun = 0;
        for (intmax_t i = 0; i < (intmax_t)AudioSampleCount; ++i) {
            bool AudioIsSilent = true;
            const int16_t *AudioSamplePtr = AudioDataPointer + i * AudioChannels;
            for (intmax_t ch = 0; ch < AudioChannels && AudioIsSilent; ++ch) {
                if (::std::abs(AudioSamplePtr[ch]) >= AudioSilenceThreshLinear)
                    AudioIsSilent = false;
            }
            if (AudioIsSilent) {
                ++AudioSilenceRun;
            } else {
                if (AudioSilenceRun >= AudioMinSilenceSamples) {
                    intmax_t AudioSegEnd = i - AudioSilenceRun + AudioKeepSilenceSamples;
                    if (AudioSegEnd > AudioSegmentStart) {
                        auto AudioBegin = AudioData.begin() +
                                          AudioSegmentStart * AudioChannels * 2;
                        auto AudioEnd = AudioData.begin() +
                                        (uintmax_t)AudioSegEnd * AudioChannels * 2;
                        co_yield {::std::vector<uint8_t>(AudioBegin, AudioEnd), AudioChannelLayout, AudioSampleRate};
                    }
                    AudioSegmentStart = i - AudioKeepSilenceSamples;
                    if (AudioSegmentStart < 0) AudioSegmentStart = 0;
                }
                AudioSilenceRun = 0;
            }
        }
        if (AudioSegmentStart < (intmax_t)AudioSampleCount) {
            auto AudioBegin = AudioData.begin() + AudioSegmentStart * AudioChannels * 2;
            co_yield {::std::vector<uint8_t>(AudioBegin, AudioData.end()), AudioChannelLayout, AudioSampleRate};
        }
    }

    AudioSegment doStripSilence(intmax_t AudioSilenceLen = 1000, intmax_t AudioSilenceThresh = -40,
                                intmax_t AudioPadding = 100) const {
        if (AudioSilenceLen <= 0) AudioSilenceLen = 1000;
        if (AudioSilenceThresh >= 0) AudioSilenceThresh = -40;
        if (AudioPadding < 0) AudioPadding = 100;
        intmax_t AudioMinSilenceSamples = AudioSilenceLen * AudioSampleRate / 1000;
        double AudioSilenceThreshLinear = dB2Ratio((double)AudioSilenceThresh) * getMaximumPossibleAmplitude();
        intmax_t AudioPaddingSamples = AudioPadding * AudioSampleRate / 1000;
        intmax_t AudioChannels = AudioChannelLayout.getChannelCount();
        auto AudioDataPointer(reinterpret_cast<const int16_t *>(AudioData.data()));
        intmax_t AudioStart = 0;
        intmax_t AudioSilenceRun = 0;
        for (intmax_t i = 0; i < (intmax_t)AudioSampleCount; ++i) {
            bool AudioIsSilent = true;
            const int16_t *AudioSamplePtr = AudioDataPointer + i * AudioChannels;
            for (intmax_t ch = 0; ch < AudioChannels && AudioIsSilent; ++ch) {
                if (::std::abs(AudioSamplePtr[ch]) >= AudioSilenceThreshLinear)
                    AudioIsSilent = false;
            }
            if (AudioIsSilent) {
                ++AudioSilenceRun;
                if (AudioSilenceRun >= AudioMinSilenceSamples) AudioStart = i + 1;
            } else {
                AudioSilenceRun = 0;
            }
        }
        AudioStart = AudioStart > AudioPaddingSamples ? AudioStart - AudioPaddingSamples : 0;
        intmax_t AudioStop = (intmax_t)AudioSampleCount;
        AudioSilenceRun = 0;
        for (intmax_t i = (intmax_t)AudioSampleCount - 1; i >= AudioStart; --i) {
            bool AudioIsSilent = true;
            const int16_t *AudioSamplePtr = AudioDataPointer + i * AudioChannels;
            for (intmax_t ch = 0; ch < AudioChannels && AudioIsSilent; ++ch) {
                if (::std::abs(AudioSamplePtr[ch]) >= AudioSilenceThreshLinear)
                    AudioIsSilent = false;
            }
            if (AudioIsSilent) {
                ++AudioSilenceRun;
                if (AudioSilenceRun >= AudioMinSilenceSamples) AudioStop = i;
            } else {
                AudioSilenceRun = 0;
            }
        }
        AudioStop = AudioStop + AudioPaddingSamples < (intmax_t)AudioSampleCount
                        ? AudioStop + AudioPaddingSamples
                        : (intmax_t)AudioSampleCount;
        if (AudioStart >= AudioStop) return *this;
        auto AudioBegin = AudioData.begin() + (uintmax_t)AudioStart * AudioChannels * 2;
        auto AudioEnd = AudioData.begin() + (uintmax_t)AudioStop * AudioChannels * 2;
        return {::std::vector<uint8_t>(AudioBegin, AudioEnd), AudioChannelLayout, AudioSampleRate};
    }

    template<typename ...AudioSegments>
    static AudioSegment fromChannels(AudioSegments... AudioSources) {
        ::std::vector<AudioSegment> AudioSourceList = {AudioSources...};
        size_t AudioChannelCount = sizeof...(AudioSegments);
        if (AudioChannelCount == 0) return {::std::vector<uint8_t>(), LayoutMono, 44100};
        intmax_t AudioMaxSamples = 0;
        int AudioCommonRate = 0;
        for (const auto &AudioSrc : AudioSourceList) {
            AudioMaxSamples = ::std::max(AudioMaxSamples, (intmax_t) AudioSrc.AudioSampleCount);
            AudioCommonRate = ::std::max(AudioCommonRate, AudioSrc.AudioSampleRate);
        }
        ::std::vector<AudioSegment> AudioSynced;
        AudioSynced.reserve(AudioChannelCount);
        for (const auto &AudioSrc : AudioSourceList)
            AudioSynced.emplace_back(AudioSrc.setChannelLayout(LayoutMono).setSampleRate(AudioCommonRate));
        ::std::vector<uint8_t> AudioDataOut((uintmax_t)AudioMaxSamples * AudioChannelCount * 2, 0);
        auto AudioDataOutPtr(reinterpret_cast<int16_t *>(AudioDataOut.data()));
        for (intmax_t i = 0; i < AudioMaxSamples; ++i) {
            for (size_t ch = 0; ch < AudioChannelCount; ++ch) {
                if (i < AudioSynced[ch].AudioSampleCount) {
                    auto AudioSrcPtr(reinterpret_cast<const int16_t *>(AudioSynced[ch].AudioData.data()));
                    AudioDataOutPtr[i * AudioChannelCount + ch] = AudioSrcPtr[i];
                }
            }
        }
        MediaChannelLayout AudioOutLayout;
        switch (AudioChannelCount) {
            case 1: AudioOutLayout = LayoutMono;
                break;
            case 2: AudioOutLayout = LayoutStereo;
                break;
            case 4: AudioOutLayout = LayoutQuad;
                break;
            case 6: AudioOutLayout = Layout51;
                break;
            case 7: AudioOutLayout = Layout61;
                break;
            case 8: AudioOutLayout = Layout71;
                break;
            default: AudioOutLayout = MediaChannelLayout((uint8_t) AudioChannelCount, 0);
                break;
        }
        return {AudioDataOut, AudioOutLayout, AudioCommonRate};
    }

    MediaChannelLayout getChannelLayout() const noexcept {
        return AudioChannelLayout;
    }

    double getdBFS() const noexcept {
        return ratio2dB(getRMS() / getMaximumPossibleAmplitude());
    }

    double getdBFSMaximum() const noexcept {
        return ratio2dB(getMaximum() / getMaximumPossibleAmplitude());
    }

    double getDuration() const noexcept {
        return AudioSampleCount * 1. / AudioSampleRate;
    }

    int getMaximum() const noexcept {
        int AudioMaximum = 0;
        auto AudioDataPointer(reinterpret_cast<const int16_t *>(AudioData.data()));
        uintmax_t AudioTotalSamples = AudioSampleCount * AudioChannelLayout.getChannelCount();
        for (uintmax_t AudioSample = 0; AudioSample < AudioTotalSamples; ++AudioSample)
            AudioMaximum = ::std::max(::std::abs(AudioDataPointer[AudioSample]), AudioMaximum);
        return AudioMaximum;
    }

    double getMaximumPossibleAmplitude() const noexcept {
        return 32768.0;
    }

    double getRMS() const noexcept {
        double AudioSum = 0;
        auto AudioDataPointer(reinterpret_cast<const int16_t *>(AudioData.data()));
        uintmax_t AudioTotalSamples = AudioSampleCount * AudioChannelLayout.getChannelCount();
        if (AudioTotalSamples == 0) return 0;
        for (uintmax_t AudioSample = 0; AudioSample < AudioTotalSamples; ++AudioSample)
            AudioSum += (double) AudioDataPointer[AudioSample] * AudioDataPointer[AudioSample];
        return ::std::sqrt(AudioSum / AudioTotalSamples);
    }

    // FFT magnitude/phase spectrum of the first channel. AudioFFTSize must be a power of two;
    // AudioSampleStartMilliseconds is a millisecond offset (negative counts from the end).
    // Hann windowed, magnitude in dBFS per bin.
    MediaSpectrum getSpectrum(size_t AudioFFTSize = 4096, intmax_t AudioSampleStartMilliseconds = 0) const {
        if (!AudioFFTSize || (AudioFFTSize & (AudioFFTSize - 1)))
            doThrowChecked(::std::invalid_argument,
                           "AudioSegment::getSpectrum(size_t, intmax_t) FFT size must be a power of two");
        intmax_t AudioDurationMilliseconds = (intmax_t) (getDuration() * 1000);
        if (AudioSampleStartMilliseconds < 0) AudioSampleStartMilliseconds += AudioDurationMilliseconds;
        if (AudioSampleCount
            && (AudioSampleStartMilliseconds < 0 || AudioSampleStartMilliseconds >= AudioDurationMilliseconds))
            doThrowChecked(::std::invalid_argument,
                           "AudioSegment::getSpectrum(size_t, intmax_t) Sample start out of range");
        intmax_t AudioSampleStart = AudioSampleStartMilliseconds * AudioSampleRate / 1000;
        uint8_t AudioChannels = AudioChannelLayout.getChannelCount();
        size_t AudioAvailable = (size_t) AudioSampleCount - (size_t) AudioSampleStart;
        size_t AudioWindowSize = ::std::min(AudioFFTSize, AudioAvailable);
        ::std::vector<double> AudioReal(AudioFFTSize, 0);
        ::std::vector<double> AudioImag(AudioFFTSize, 0);
        auto AudioDataPointer(reinterpret_cast<const int16_t *>(AudioData.data()));
        for (size_t AudioSample = 0; AudioSample < AudioWindowSize; ++AudioSample) {
            double AudioHann = 0.5 - 0.5 * ::std::cos(2 * 3.14159265358979323846 * AudioSample / AudioFFTSize);
            AudioReal[AudioSample] = AudioDataPointer[(AudioSampleStart + AudioSample) * AudioChannels] * AudioHann;
        }
        doFFT(AudioReal, AudioImag);
        MediaSpectrum AudioSpectrum;
        AudioSpectrum.SpectrumMagnitude.reserve(AudioFFTSize / 2 + 1);
        AudioSpectrum.SpectrumPhase.reserve(AudioFFTSize / 2 + 1);
        AudioSpectrum.SpectrumFrequencyStep = (double) AudioSampleRate / AudioFFTSize;
        if (!AudioWindowSize) {
            AudioSpectrum.SpectrumMagnitude.assign(AudioFFTSize / 2 + 1,
                                                   -::std::numeric_limits<double>::infinity());
            AudioSpectrum.SpectrumPhase.assign(AudioFFTSize / 2 + 1, 0.0);
            return AudioSpectrum;
        }
        for (size_t AudioBin = 0; AudioBin <= AudioFFTSize / 2; ++AudioBin) {
            double AudioMagnitude = ::std::hypot(AudioReal[AudioBin], AudioImag[AudioBin]);
            // Hann coherent gain is 0.5; interior bins carry half the real-signal amplitude.
            double AudioFactor = (AudioBin && AudioBin < AudioFFTSize / 2) ? 4.0 : 2.0;
            double AudioAmplitude = AudioFactor * AudioMagnitude / (double) AudioWindowSize;
            AudioSpectrum.SpectrumMagnitude.push_back(ratio2dB(AudioAmplitude / getMaximumPossibleAmplitude()));
            AudioSpectrum.SpectrumPhase.push_back(::std::atan2(AudioImag[AudioBin], AudioReal[AudioBin]));
        }
        return AudioSpectrum;
    }

    // All three EBU R128 / ITU-R BS.1770 loudness values in LUFS at once.
    MediaLoudness getLoudness() const {
        auto AudioBlocks = doLoudnessBlocks();
        MediaLoudness AudioLoudness;
        AudioLoudness.LoudnessMomentary = -0.691 + 10.0 * ::std::log10(AudioBlocks.back());
        size_t AudioWindow = ::std::min<size_t>(30, AudioBlocks.size());
        double AudioSumShort = 0;
        for (size_t AudioIndex = AudioBlocks.size() - AudioWindow; AudioIndex < AudioBlocks.size(); ++AudioIndex)
            AudioSumShort += AudioBlocks[AudioIndex];
        AudioLoudness.LoudnessShortTerm = -0.691 + 10.0 * ::std::log10(AudioSumShort / AudioWindow);
        double AudioGateAbsolute = ::std::pow(10.0, (-70.0 + 0.691) / 10.0);
        double AudioSumGated = 0;
        intmax_t AudioCountGated = 0;
        for (double AudioBlock : AudioBlocks)
            if (AudioBlock >= AudioGateAbsolute) {
                AudioSumGated += AudioBlock;
                ++AudioCountGated;
            }
        if (!AudioCountGated) return AudioLoudness;
        double AudioMeanGated = AudioSumGated / AudioCountGated;
        double AudioGateRelative = AudioMeanGated / 10.0;
        double AudioSumIntegrated = 0;
        intmax_t AudioCountIntegrated = 0;
        for (double AudioBlock : AudioBlocks)
            if (AudioBlock >= AudioGateRelative) {
                AudioSumIntegrated += AudioBlock;
                ++AudioCountIntegrated;
            }
        if (!AudioCountIntegrated) {
            AudioSumIntegrated = AudioSumGated;
            AudioCountIntegrated = AudioCountGated;
        }
        AudioLoudness.LoudnessIntegrated = -0.691 + 10.0 * ::std::log10(AudioSumIntegrated / AudioCountIntegrated);
        return AudioLoudness;
    }

    double getLoudnessIntegrated() const {
        return getLoudness().LoudnessIntegrated;
    }

    double getLoudnessShortTerm() const {
        return getLoudness().LoudnessShortTerm;
    }

    double getLoudnessMomentary() const {
        return getLoudness().LoudnessMomentary;
    }

    int getSampleRate() const noexcept {
        return AudioSampleRate;
    }

    // Raw interleaved S16 little-endian bytes: AudioSampleCount * channels * 2 bytes (read-only view).
    const ::std::vector<uint8_t> &getRawData() const noexcept {
        return AudioData;
    }

    std::generator<int16_t *> getSamples(intmax_t SampleStart, intmax_t SampleStop, intmax_t SampleStep = 1) {
        if (SampleStart < 0) SampleStart += AudioSampleCount;
        if (SampleStop <= 0) SampleStop += AudioSampleCount;
        auto AudioDataPointer(reinterpret_cast<int16_t *>(AudioData.data()));
        intmax_t AudioSampleStart = SampleStart * AudioChannelLayout.getChannelCount();
        intmax_t AudioSampleStop = SampleStop * AudioChannelLayout.getChannelCount();
        for (intmax_t AudioSample = AudioSampleStart; AudioSample < AudioSampleStop; AudioSample += SampleStep)
            co_yield AudioDataPointer + AudioSample;
    }

    std::generator<int16_t> getSamples(intmax_t SampleStart, intmax_t SampleStop, intmax_t SampleStep = 1) const {
        if (SampleStart < 0) SampleStart += AudioSampleCount;
        if (SampleStop <= 0) SampleStop += AudioSampleCount;
        auto AudioDataPointer(reinterpret_cast<const int16_t *>(AudioData.data()));
        intmax_t AudioSampleStart = SampleStart * AudioChannelLayout.getChannelCount();
        intmax_t AudioSampleStop = SampleStop * AudioChannelLayout.getChannelCount();
        for (intmax_t AudioSample = AudioSampleStart; AudioSample < AudioSampleStop; AudioSample += SampleStep)
            co_yield AudioDataPointer[AudioSample];
    }

    bool isEqual(const AudioSegment &AudioSource) const noexcept {
        return AudioData == AudioSource.AudioData && AudioChannelLayout.getChannelMask() == AudioSource.AudioChannelLayout.getChannelMask() && AudioSampleRate == AudioSource.AudioSampleRate;
    }

    static double ratio2dB(double RatioSource, bool RatioAmplitude = true) noexcept {
        if (!RatioSource) return -std::numeric_limits<double>::infinity();
        return RatioAmplitude ? 20 * log10(RatioSource) : 10 * log10(RatioSource);
    }

    AudioSegment setChannelLayout(MediaChannelLayout AudioChannelLayoutSource) const {
        return doResample(AudioChannelLayoutSource, AudioSampleRate);
    }

    AudioSegment setSampleRate(int AudioSampleRateSource) const {
        if (AudioSampleRateSource <= 0)
            doThrowChecked(::std::runtime_error, "AudioSegment::setSampleRate(int) Invalid sample rate");
        return doResample(AudioChannelLayout, AudioSampleRateSource);
    }

    // Changes only the rate tag (pydub set_frame_rate): no resampling, duration changes, pitch changes.
    AudioSegment setFrameRate(int AudioFrameRateSource) const {
        if (AudioFrameRateSource <= 0)
            doThrowChecked(::std::runtime_error, "AudioSegment::setFrameRate(int) Invalid frame rate");
        return {AudioData, AudioChannelLayout, AudioFrameRateSource};
    }

    OpenAL::MediaBuffer toMediaBuffer() const {
        return {AudioChannelLayout, AudioData, (ALsizei) AudioSampleRate};
    }
};

#endif
