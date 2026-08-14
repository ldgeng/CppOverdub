# CppOverdub 功能提案（FEATURES）

> 本文档梳理 CppOverdub 现状与 pydub 0.25.1 的功能差异，并列出可增加的功能提案。
> 提案中的 API 草图沿用库现有命名风格（PascalCase 变量、`doXxx` 动作方法、`getXxx` 取值方法、`setXxx` 配置方法、`doThrowChecked` 抛错）。

## 现状盘点（已实现）

| 分类 | 已实现 |
| --- | --- |
| 输入输出 | `doOpen`(文件解码)、`doExport`(按扩展名推断格式)、`toMediaBuffer` |
| 编辑 | `doConcat`、`doOverlay`、`doSlice`×2、`doRepeat`、`doReverse`、`doFade`/`doFadeIn`/`doFadeOut` |
| 变换 | `doGain`、`doNormalize`、`doCompressDynamicRange`、`doSpeedUp`、`doShiftPitch`(相位声码器变调不变速)、`setSampleRate`(重采样)、`setFrameRate`(只改标签)、`setChannelLayout`(声道重排) |
| 声道/静音 | `fromChannels`、`doSplitChannels`、`doSplitOnSilence`、`doStripSilence` |
| 生成 | `doGenerate`(正弦/静音) |
| 测量 | `getDuration`、`getMaximum`、`getRMS`、`getdBFS`、`getdBFSMaximum`、`getSamples`×2(可写/只读生成器)、`isEqual`、`getSpectrum`(FFT 频谱)、`getLoudness`/`getLoudnessIntegrated`/`getLoudnessShortTerm`/`getLoudnessMomentary`(EBU R128 / LUFS)、`getRawData`(原始字节只读视图) |
| 元数据 | `doGetMetadata`(标题/艺术家/专辑 + 全部标签键值对) |
| 同步 | `doSynchronize`(多段对齐采样率/声道) |
| OpenAL | `MediaDevice`/`MediaContext`/`MediaSource`/`MediaBuffer`，含 3D 方向/位置、监听者(位置/朝向/速度/增益)、增益、循环、pitch、队列流式播放(`doQueueBuffer`/`doUnqueueBuffer`)、播放状态/进度查询、HRTF(`doListHRTFNames`/`setHRTF`)、无缝循环点(`setSourceLoopPoints`) |
| OpenAL EFX | `MediaEffect`/`MediaAuxiliarySlot`/`MediaReverb`(预设)/`MediaEcho`(延迟/反馈/阻尼/扩散) |
| FFmpeg 封装 | `MediaCodec`/`MediaFrame`/`MediaPacket`/`MediaSWRContext`/`MediaCodecContext`/`MediaFormatContext` |

## 与 pydub 0.25.1 的差异（缺口清单）

pydub 有、CppOverdub 尚无的功能（按价值排序）：

| pydub 功能 | 现状 | 价值 |
| --- | --- | --- |
| `low_pass_filter` / `high_pass_filter` | 无（README 曾列出后删除） | 高 |
| `invert_phase` | 无 | 高 |
| `pan` / `apply_gain_stereo` | 无 | 高 |
| `export(format/codec/bitrate/parameters/tags)` | 仅按扩展名推断 | 高 |
| `silent(duration)` / `empty()` | 无 | 高 |
| 生成器 `Sine/Square/Sawtooth/Triangle/Pulse/WhiteNoise` | 仅正弦 | 高 |
| `set_frame_rate`(只改播放速率标签，不重采样) | `setSampleRate` 会重采样，无"只改标签"版 | 中 |
| `set_sample_width`(8/16/24/32 bit) | 固定 int16 | 中 |
| `detect_silence` / `detect_nonsilent`(返回区间而非切分) | 无 | 中 |
| `get_dc_offset` / `remove_dc_offset` | 无 | 中 |
| `overlay(gain_during_overlay=...)` | `doOverlay` 无增益参数 | 中 |
| `get_array_of_samples` / `get_frame` / `frame_count` / `channels` / `sample_width` / `raw_data` | 部分有(getSamples)，缺原始访问器 | 中 |
| `__add__` / `__mul__` / `__getitem__` 运算符 | 无运算符重载 | 低 |
| `from_raw`(裸 PCM) / 内存字节解码 | `doOpen` 仅支持路径 | 中 |
| `apply_mono_filter_to_each_channel` | 无 | 低 |
| `from_flv` / `from_mp3` / `from_ogg` 便捷函数 | 无(FFmpeg 解码已通用覆盖，价值低) | 低 |
| scipy 系特效(需 scipy 依赖) | 无(不建议引入重量级依赖) | 低 |

---

## 功能提案

### P0 —— pydub 对齐的高频刚需

#### 1. 滤波器三件套：`doFilterLow` / `doFilterHigh` / `doFilterBand`  ✅ 已实现（2026-08，纯 C++ 二阶 biquad，命名按最终 API）
- 动机：pydub 使用率最高的缺失功能；用途广（电话音效、去齿音、低音增强）。
- 实现路径（二选一）：
  - a) **libavfilter 滤镜图**（推荐）：已封装 FFmpeg，加一层 `MediaFilterGraph` 即可，同时顺带解锁 P1 的通用 `doFilter`；
  - b) 纯手写二阶 IIR biquad（零依赖、延迟小，但精度与 pydub 行为略有差异）。
- API 草案：
  ```cpp
  AudioSegment doLowPass(double AudioCutoff) const;                          // Hz
  AudioSegment doHighPass(double AudioCutoff) const;                         // Hz
  AudioSegment doBandPass(double AudioCutoffLow, double AudioCutoffHigh) const; // Hz
  ```
- 依赖：libavfilter(已在 3rd_party)、或纯 C++ DSP。
- 复杂度：中。

#### 2. `doInvertPhase`
- 动机：消人声/卡拉 OK 的前置操作（与 `doOverlay` 反相叠加），实现仅逐样本取负。
- API 草案：`AudioSegment doInvertPhase() const noexcept;`
- 复杂度：低（纯内存循环）。

#### 3. `doPan` / `doApplyGainStereo`（`doPan` ✅ 已实现：等功率声像，单声道自动转立体声）
- 动机：立体声平衡是基础混音操作；pydub 语义：pan∈[-1,1]，-1 全左、0 居中、+1 全右（等功率定律）。
- API 草案：
  ```cpp
  AudioSegment doPan(double AudioPan) const;                      // -1..1
  AudioSegment doApplyGainStereo(double AudioGainLeft, double AudioGainRight) const;
  ```
- 复杂度：低（仅双声道；单声道输入可提示或先 `fromChannels` 复制为立体声）。

#### 4. `doExport` 参数化（码率/编码器/容器参数）  ✅ 已实现：`MediaExportOption{ExportFormat, ExportCodec, ExportBitrate, ExportSampleRate}`，原签名默认参数保持兼容
- 动机：目前只能按扩展名默认参数导出（如 MP3 用默认码率），无法控制质量。
- API 草案（沿用风格，用命名结构体而不是一长串默认参数）：
  ```cpp
  struct MediaExportOption final {
      ::std::string ExportFormat;   // "mp3"、"wav"、"ogg"… 为空则按扩展名推断
      intmax_t ExportBitrate = 0;   // bps，0=编码器默认
      int ExportSampleFormat = 0;   // AV_SAMPLE_FMT_*，0=沿用 S16
  };
  void doExport(const ::std::string &AudioPath, const MediaExportOption &AudioOption = {}) const;
  ```
- 复杂度：低（现有 `doExport` 内部已有全部基础设施，仅加参数透传）。

#### 5. `doSilent` / `doEmpty`（静态生成）
- 动机：pydub 的 `silent()`/`empty()` 是测试与拼接的常用基座；目前只能用 `doGenerate(0, dur)` 绕。
- API 草案：
  ```cpp
  static AudioSegment doSilent(intmax_t AudioDuration, int AudioSampleRateSource = 44100) noexcept;
  static AudioSegment doEmpty() noexcept;
  ```
- 复杂度：低。

#### 6. 波形生成器家族
- 动机：`doGenerate` 目前只有正弦；测试信号（方波/锯齿/白噪）与音效合成都需要。
- API 草案（扩展枚举而非新增一堆静态方法）：
  ```cpp
  enum class MediaWaveType : uint8_t { WaveSine, WaveSquare, WaveSawtooth, WaveTriangle, WavePulse, WaveWhiteNoise };
  static AudioSegment doGenerate(MediaWaveType WaveType = MediaWaveType::WaveSine,
                                 double AudioFrequency = 440, double AudioDuration = 1.0,
                                 int AudioSampleRateSource = 44100, double AudioAmplitude = 0.8);
  // 保留旧签名做转发，或加 AudioDutyCycle 参数支持 Pulse 占空比
  ```
- 复杂度：低（白噪声用 `::std::mt19937`，其余均为逐样本公式）。

#### 7. `doDetectSilence` / `doDetectNonSilent`（返回区间）
- 动机：`doSplitOnSilence` 直接切分，但很多场景要先"知道"静音区间（如做波形图、挑段落）。
- API 草案（返回区间结构体向量，区间单位毫秒，与 pydub 一致）：
  ```cpp
  struct MediaSilenceRange final { intmax_t RangeStart; intmax_t RangeStop; };
  ::std::vector<MediaSilenceRange> doDetectSilence(intmax_t AudioSilenceLen = 1000, double AudioSilenceThresh = -40) const noexcept;
  ::std::vector<MediaSilenceRange> doDetectNonSilent(intmax_t AudioSilenceLen = 1000, double AudioSilenceThresh = -40) const noexcept;
  ```
- 复杂度：低（与 `doSplitOnSilence` 同一套检测逻辑，抽出公共部分即可）。

### P1 —— 高价值扩展

#### 8. 通用 FFmpeg 滤镜：`doFilter`
- 动机：一旦有滤镜图封装，`lowpass/highpass/aecho/equalizer/volume/atempo` 全部免费获得，是性价比最高的一条路。
- API 草案：
  ```cpp
  AudioSegment doFilter(const ::std::string &FilterDescription) const; // 如 "lowpass=f=300,aecho=0.8:0.9:1000|1800:0.3|0.25"
  ```
- 依赖：新增 `FFMpeg::MediaFilterGraph` 封装（`avfilter_graph_alloc/parse_ptr` + `buffersrc`/`buffersink`）。
- 复杂度：中高（封装滤镜图是主要工作量，之后各滤镜是零成本）。

#### 9. 内存/流式 I/O：`doOpenBytes`、`doExportBytes`
- 动机：网络音频、资源包内音频、避免落盘的临时处理都需要字节流 I/O。
- 实现路径：自定义 `AVIOContext`（`avio_alloc_context` + 内存读写回调）。
- API 草案：
  ```cpp
  static AudioSegment doOpenBytes(const ::std::vector<uint8_t> &AudioData);
  ::std::vector<uint8_t> doExportBytes(const MediaExportOption &AudioOption = {}) const;
  ```
- 复杂度：中（需自定义 AVIO 读写回调，是一次性基础建设）。

#### 10. 元数据读取：`doGetMetadata`  ✅ 已实现（`MediaMetadata{MetadataTitle, MetadataArtist, MetadataAlbum, MetadataEntries}` + `getEntry(key)`，`av_dict_iterate` 遍历容器标签）
- 动机：读标题/艺术家/专辑封面是播放器常见需求；FFmpeg 字典 API 现成。
- API 草案：
  ```cpp
  struct MediaMetadata final { ::std::string MetadataTitle; ::std::string MetadataArtist; ::std::string MetadataAlbum; };
  static MediaMetadata doGetMetadata(const ::std::string &AudioPath);
  ```
- 复杂度：低（`av_dict_get` 遍历）。

#### 11. OpenAL 播放增强（3D 定位 / 监听者 / 流式队列）  ✅ 已实现：`setSourcePosition`、`setListenerPosition/Velocity/Orientation/Gain`、`doQueueBuffer`/`doUnqueueBuffer`/`getSourceBuffersQueued`/`getSourceBuffersProcessed`、`MediaSourceState getSourceState`、`getSourceOffsetSamples/Seconds`、`MediaBuffer::setBufferData`(流式重填)；析构已加 `alcGetCurrentContext` 保护（无当前上下文时不调 AL，兼容会 abort 的 OpenAL Soft 构建）
- 动机：当前 `MediaSource` 缺 `setSourcePosition`，`MediaContext` 缺监听者位置/朝向，无法做真正的 3D 声场；`MediaBuffer` 整段缓冲对长音频不友好。
- API 草案：
  ```cpp
  void setSourcePosition(float PositionX, float PositionY, float PositionZ) const; // MediaSource
  static void setListenerPosition(float PositionX, float PositionY, float PositionZ) noexcept; // MediaContext
  static void setListenerOrientation(float AtX, float AtY, float AtZ, float UpX, float UpY, float UpZ) noexcept;
  void doQueueBuffer(const MediaBuffer &);   // 流式：多缓冲排队(MediaSource)
  void doUnqueueBuffer();                    // alSourceUnqueueBuffers 语义
  ALenum getSourceState() const noexcept;    // 播放状态查询(枚举包装可选)
  ```
- 复杂度：低（全部是 `alSource3f`/`alListener3f`/`alSourceQueueBuffers` 单行封装，与现有 `setSourceDirection` 完全同构）。

#### 12. EFX 音效（混响/回声）  ✅ 已实现：`MediaEffect`/`MediaAuxiliarySlot`/`MediaReverb`（含 `EFX_REVERB_PRESET_*` 预设，见 efx-presets.h）/`MediaEcho`（`setDelay/setFeedback/setDamping/setSpread/setLeftRightDelay`）；`doApplyPreset` 末尾排空错误队列（部分实现仅支持标准 `AL_REVERB_*` 子集，会拒绝 EAX4 参数）
- 动机：`3rd_party/AL` 已含 `efx.h`/`efx-presets.h`（ALC_EXT_EFX），头文件现成，只差封装；游戏/语音场景刚需。
- API 草案（沿用"先建对象再 set"风格）：
  ```cpp
  class MediaEffect final : public NonCopyable {        // OpenAL 命名空间
  public:
      MediaEffect(ALenum EffectType);                   // AL_EFFECT_REVERB / AL_EFFECT_ECHO
      void setParameter(ALenum Parameter, float OptionValue); // 或模板化
      ~MediaEffect() noexcept;
  };
  class MediaAuxiliarySlot final : public NonCopyable { /* AL_AUXILIARY_SENDSLOT */ };
  void setSourceAuxiliary(const MediaAuxiliarySlot &);  // MediaSource
  ```
- 复杂度：中（对象生命周期管理 + 常量表映射，模式与现有类一致）。

#### 13. `setFrameRate`（只改标签不重采样）+ `setSampleWidth`  ✅ `setFrameRate` 已实现（只改标签，时长/音高随播放速率变化）；`setSampleWidth`(8/24/32bit) 未做
- 动机：pydub 语义区分"改播放速率(不重采样)"与"真重采样"；目前只有重采样版。24/32bit 输出与整型宽度转换是音频库基本盘。
- API 草案：
  ```cpp
  AudioSegment setFrameRate(int AudioFrameRateSource) const noexcept;  // 仅改 AudioSampleRate 标签
  AudioSegment setSampleWidth(int AudioSampleWidthSource) const;        // 1/2/3/4 字节，内部转 S16/24/32
  int getSampleWidth() const noexcept;                                  // 返回 2
  intmax_t getFrameCount() const noexcept;                              // == AudioSampleCount
  ```
- 复杂度：低-中（标签版一行；宽度转换需 swr 或手工移位，且 `AudioData` 假设 int16 处需审计）。

#### 14. 直流偏置：`getDCOffset` / `doRemoveDCOffset`
- 动机：录音设备直流偏置常见，pydub 同名功能直译；实现为均值/去均值循环。
- API 草案：
  ```cpp
  double getDCOffset() const noexcept;                    // 每声道平均，单位采样值
  AudioSegment doRemoveDCOffset() const noexcept;
  ```
- 复杂度：低。

#### 15. 交叉淡化：`doCrossfade`
- 动机：pydub 无内置但社区配方极常见；实现=尾渐出+头渐入+叠加，全部可复用 `doFade`+`doOverlay`。
- API 草案：`AudioSegment doCrossfade(const AudioSegment &AudioSource, intmax_t AudioDuration) const;`
- 复杂度：低（组合已有原语，亦可提供 `doFadeIn/doFadeOut` 内部转发）。

#### 16. 峰值归一化到目标电平
- 动机：现有 `doNormalize(headroom)` 是"留余量"语义；合成/响度对齐场景常要"精确到 -1 dBFS"。
- API 草案：`AudioSegment doNormalizePeak(double AudioTargetdBFS = -1.0) const noexcept;`
- 复杂度：低（复用 `getMaximum`+`doGain`）。

### P2 —— 体验与工程化

#### 17. 运算符重载（`operator+`/`operator*`/`operator[]`/`operator==`）
- 动机：现代 C++ 易用性；与 pydub `+`(拼接)、`*`(重复)、切片、`==` 对齐。
- 草案：`operator+` → `doConcat`、`operator*` → `doRepeat`、`operator==` → `isEqual`；切片返回 `AudioSegment` 与 `doSlice` 对齐（`operator[]` 取 1ms 帧，兼容 pydub `seg[i]` 语义）。
- 复杂度：低（纯转发），注意与 `NonCopyable` 无冲突。

#### 18. 原始数据访问：`getRawData` / `getData`（span 视图）
- 动机：互操作与零拷贝采样访问；C++20 `std::span` 零成本。
- API 草案：
  ```cpp
  ::std::span<const uint8_t> getRawData() const noexcept;   // 字节视图
  ::std::span<int16_t> getSampleData() noexcept;            // 采样视图(多声道交错)
  ```
- 复杂度：低；若提供可写视图，需保证 `AudioSampleCount` 一致性（文档说明"修改长度需重建"）。

#### 19. 逐块流式解码：`doOpenStream`
- 动机：当前 `doOpen` 全量载入内存；长音频(几小时)内存翻倍问题靠流式 chunk 解决。
- API 草案：
  ```cpp
  static std::generator<AudioSegment> doOpenStream(const ::std::string &AudioPath, intmax_t ChunkMilliseconds = 1000);
  ```
- 复杂度：中（`doOpen` 循环体改 `co_yield`，需处理跨块状态/尾部 flush）。

#### 20. 便捷构造与选项结构体统一
- 动机：`doSplitOnSilence` 目前阈值硬编码为 -40dB/1000ms，pydub 可调；把散落的参数统一成结构体，避免函数签名继续膨胀。
- 草案：`MediaSilenceOption { SilenceLenMs; SilenceThreshdB; KeepSilenceMs; }` 供 `doSplitOnSilence`/`doStripSilence`/`doDetectSilence` 共用。
- 复杂度：低（重构已有私有逻辑，行为保持默认值不变）。

#### 21. 设备枚举：`doListDevices`
- 动机：OpenAL 设备选择是播放器基本需求；`alcGetString(NULL, ALC_DEVICE_SPECIFIER)` 现成。
- API 草案：`static ::std::vector<::std::string> doListDevices();`（`OpenAL::MediaDevice`）
- 复杂度：低。

#### 22. 属性标注：`[[nodiscard]]`
- 动机：所有 `doXxx` 返回新 `AudioSegment`，漏接返回值即静默丢结果；`[[nodiscard]]` 零成本防错（g++/MSVC 均支持，C++23 可用）。
- 复杂度：低（机械标注），与风格不冲突。

#### 23. 工程化：install 规则 / 包配置 / CI / 文档
- 动机：作为"库"目前无法被外部工程干净消费。
- 提案：
  - 根 `CMakeLists.txt` 加 `install()` 规则 + `CppOverdubConfig.cmake` 包配置（INTERFACE 库）；
  - GitHub Actions 双平台 CI（Windows MinGW + Linux 用系统 FFmpeg），测试 `CppOverdubTest`；
  - `ctest` 注册测试；Doxygen 注释；`examples/` 目录（打开→滤波→导出→播放 的最小示例）；
  - vcpkg/conan 端口（可选）。
- 复杂度：中（一次性）。

### P3 —— 进阶/可选

#### 24. 频域能力：`getSpectrum`（FFT）与频谱图导出  ✅ `getSpectrum` 已实现（自带 radix-2 FFT，Hann 窗，相干增益校正，`MediaSpectrum{SpectrumMagnitude(dBFS), SpectrumPhase, SpectrumFrequencyStep}`；FFT 大小需 2 的幂，起始毫秒偏移可负）；频谱图导出未做
- 动机：可视化、音高检测、均衡器 UI 的基石。
- 实现：自带小 FFT（如 radix-2 原地 FFT，百行内）或接 kissfft。
- 复杂度：中高。

#### 25. 响度：EBU R128 / LUFS  ✅ 已实现：`getLoudness()`/`getLoudnessIntegrated()`/`getLoudnessShortTerm()`/`getLoudnessMomentary()`，返回 `MediaLoudness{LoudnessMomentary, LoudnessShortTerm, LoudnessIntegrated}`(LUFS)。手工实现 BS.1770 K 加权（精确增益表系数，任意采样率：高架 f0=1681.97Hz G=4dB Q=0.7072 + 高通 f0=38.135Hz Q=0.5003）+ 400ms 块/100ms 跳 + -70 LUFS 绝对门限 + 相对门限；静音返回 -inf。校验：满幅 1kHz 正弦 ≈ -3.05 LUFS（与 libebur128 系数吻合到 1e-15）
- 动机：流媒体响度标准化（-14 LUFS）刚需。
- 实现：libavfilter `loudnorm`/`ebur128` 或手工 K 加权滤波。
- 复杂度：高（算法细节多，建议用 FFmpeg 滤镜绕）。

#### 26. 变调不变速：`doShiftPitch`  ✅ 已实现：`doShiftPitch(double AudioSemitone)`（12 = 升八度）。相位声码器（2048 点 FFT / 512 跳 / Hann 窗重叠相加，瞬时频率相位传播），逐声道处理，复用 `getSpectrum` 的 FFT 核；0 半音为恒等变换
- 动机：pydub 也没有，但混音/卡拉 OK 常用；可"重采样+变速"组合实现或 `rubberband` 库。
- 复杂度：中高。

#### 27. 内部浮点管线 / SIMD 加速  ❌ 评估后未做：按本文"勿与功能项混做、远期独立里程碑"的约束跳过——`AudioData` 存储格式变更影响面过大（所有 `reinterpret_cast<int16_t*>` 假设需审计）。现有逐样本热循环（`doGain`/`doOverlay`/滤波）已可被编译器在 -O2 下自动向量化，暂不手写 SIMD
- 动机：`doGain`/`doOverlay` 等逐样本循环可用 `std::execution::par_unseq` 或手写 AVX2；浮点内部表示可减少多次处理的量化噪声。
- 约束：涉及 `AudioData` 存储格式变更，影响面大，建议作为远期独立里程碑，勿与功能项混做。
- 复杂度：高。

#### 28. 更多 OpenAL 细节  ✅ 部分已实现：HRTF（`MediaDevice::doListHRTFNames`/`doGetHRTFState`/`setHRTF(bool|name)`，`ALC_SOFT_HRTF` 经 `alcGetProcAddress` 运行时解析，兼容未导出扩展的导入库）与无缝循环点（`MediaSource::setSourceLoopPoints`，`AL_SOFT_loop_points`）；播放进度查询见 #11（`getSourceOffsetSamples/Seconds`）。注意：随库分发的 OpenAL32.dll 声明支持但实际拒绝 `AL_LOOP_POINTS_SOFT`（对 `alSourceiv` 返回 AL_INVALID_ENUM），对真正的 OpenAL Soft 构建正常
- HRTF 设置（`ALC_HRTF_SOFT`）、`AL_SOFT_source_length`/`AL_SOFT_loop_points` 无缝循环点（配 `doSlice` 做鼓组 loop）、`MediaSource` 播放进度回调/`getPlaybackOffset`。
- 复杂度：低-中（均为既有扩展常量）。

---

## 明确不做 / 低优先级（附理由）

| 项 | 理由 |
| --- | --- |
| `from_flv`/`from_mp3`/`from_ogg` 等按格式拆分入口 | `doOpen` 已由 FFmpeg 统一解码，拆分入口是纯样板代码 |
| scipy 系特效 | 引入重量级数值依赖，违背 header-only 定位；用 FFmpeg 滤镜替代 |
| 手动 WAV 解析/写出 | FFmpeg 已覆盖，自写解析是无谓维护负担 |
| 子进程调 ffmpeg.exe 播放/导出 | OpenAL 直放 + libav 直导已是更优方案 |
| 视频处理 | 超出音频库定位 |
| ASIO/WASAPI 直通 | OpenAL 抽象已够用，底层后端切换是 OpenAL 配置问题 |

## 建议推进顺序

1. **P0 全部**（滤波三件套、反相、声像、导出参数、静音/空、波形生成、静音检测区间）——补齐 pydub 高频缺口；
2. **P1 的 `doFilter` 通用滤镜图** —— 一次封装解锁后续所有 FFmpeg 滤镜；
3. **P1 OpenAL 定位 + 队列流式** ✅ 已随本批完成；
4. **P2 运算符/span/`[[nodiscard]]`/选项结构体** —— API 打磨，可随任意里程碑顺手完成（`getRawData` 原始字节视图已随流式播放先行落地）；
5. **P2 工程化** —— CI + install，独立推进不阻塞功能；
6. **P3 按需取用** ✅ 本批已完成：元数据(#10)、OpenAL 3D/监听者/队列(#11)、EFX 回声(#12)、`setFrameRate`(#13 标签版)、FFT `getSpectrum`(#24)、EBU R128/LUFS(#25)、`doShiftPitch` 相位声码器(#26)、HRTF + 无缝循环点(#28)；#27 浮点管线/SIMD 按约束跳过（见上文）。
