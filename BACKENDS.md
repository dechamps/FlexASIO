# FlexASIO backends

This document provides background information about what FlexASIO *backends*
are, and the differences between them. It is recommended reading for anyone
wanting to optimize their audio pipeline. In particular, this document provides
the necessary information to understand what the [`backend`][backend] and
[`wasapiExclusiveMode`][wasapiExclusiveMode] FlexASIO configuration options do.

**Note:** this document is a work in progress, and does not go into much detail
regarding the specific differences between the backends. For now, you will have
to experiment for yourself. Comments and suggestions are welcome.

A `backend` is just another term for what PortAudio calls the *host API*.
FlexASIO uses the term "backend" to avoid potential confusion with the term
"ASIO host".

The following architecture diagram shows the entire audio path, from the ASIO
host application (e.g. Cubase) to the hardware audio device (arrows represent
the direction of audio streaming in the playback direction):

[![FlexASIO architecture diagram][diagram]][diagram]

The backends are shown in red on the diagram. They lead to various paths that
the audio can take through the PortAudio library, and then through the Windows
audio subsystem, before reaching the audio hardware driver and device.

Roughly speaking, each backend maps to a single audio API that Windows makes
available for applications: *MME*, *DirectSound*, *WASAPI* and *Kernel
Streaming*. The main difference between the backends is therefore which Windows
API they use to play and record audio.

In terms of code paths, there are large differences between the various
backends, both on the Windows side and on the PortAudio side. Therefore, the
choice of backend can have a large impact on the behaviour of the overall audio
pipeline. In particular, choice of backend can affect:

- **Reliability:** some backends can be more likely to work than others, because
  of the quality and maturity of the code involved.
- **Ease of use:** Some backends are more "opinionated" than others about when
  it comes to which audio formats (i.e. sample rate, number of channels) they
  will accept. They might refuse to work if the audio format is not exactly the
  one they expect.
- **Latency:** depending on the backend, the audio pipeline might include
  additional layers of buffering that incur additional latency.
- **Latency reporting:** some backends are better than others at reporting the
  end-to-end latency of the entire audio pipeline. Some backends underestimate
  their own total latency, resulting in FlexASIO reporting misleading latency
  numbers.
- **Exclusivity:** some backends will seize complete control of the hardware
  device, preventing any other application from using it.
- **Audio processing:** some backends will automatically apply additional
  processing on audio data. Specifically:
  - *Sample rate conversion:* some backends will accept any sample rate and then
    convert to whatever sample rate Windows is configured to use.
  - *Sample type conversion:* some backends will accept any sample type (16-bit
    integer, 32-bit float, etc.) and then convert to whatever sample type
    Windows is configured to use.
  - *Downmixing:* some backends will accept any channel count and then downmix
  (or maybe even upmix) to whatever channel count Windows is configured to use.
  - *Mixing:* if the backend is not exclusive (see above), audio might be mixed
    with the streams from other applications.
  - *APOs:* Hardware audio drivers can come bundled with so-called [Audio
    Processing Objects][] (APOs), which can apply arbitrary pre-mixing (SFX) or
    post-mixing (MFX/EFX) processing on audio data. Some backends will bypass
    this processing, some won't.
- **Feature set:** some FlexASIO features and options might not work with all
  backends.

**Note:** the internal buffer size of the shared Windows audio pipeline has been
observed to be 20 ms. This means that only exclusive backends (i.e. WASAPI
Exclusive, WDM-KS) can achieve an actual latency below 20 ms.

**Note:** In addition to APOs, hardware devices can also implement additional
audio processing at a low level in the audio driver, or baked into the hardware
itself ([DSP][]). Choice of backend cannot affect such processing.

**Note:** none of the backends seem to be capable of taking the hardware itself
into account when reporting latency. So, for example, when playing audio over
Bluetooth, the reported latency will not take the Bluetooth stack into account,
and will therefore be grossly underestimated.

## MME backend

[Multimedia Extensions][] is the first audio API that was introduced in Windows,
going all the way back to 1991. Despite its age, it is still widely used because
of its ubiquity and simplicity. In PortAudio (but not FlexASIO), it is the
default host API used on Windows. It should therefore be expected to be highly
mature and extremely reliable; if it doesn't work, it is unlikely any other
backend will.

MME goes through the entire Windows audio pipeline, including sample rate
conversion, mixing, and APOs. As a result it is extremely permissive when it
comes to audio formats. It should be expected to behave just like a typical
Windows application would. Its latency should be expected to be mediocre at
best, as MME was never designed for low-latency operation. This is compounded by
the fact that MME appears to [behave very poorly][issue30] with small (<40 ms)
buffer sizes.

Latency numbers reported by MME do not seem to take the Windows audio pipeline
into account. This means the reported latency is underestimated by at least 20
ms, if not more.

**Note:** the channel count exposed by FlexASIO when using MME can be a bit odd.
For example, it might expose 8 channels for a 5.1 output, downmixing the rear
channels pairs.

Modern versions of Windows implement the MME API by using WASAPI internally,
making this backend a "second-class citizen" compared to WASAPI and WDM-KS.

## DirectSound backend

[DirectSound][] was introduced in 1995. It is mainly designed for use in video
games.

In practice, DirectSound should be expected to behave somewhat similarly to MME.
It will accept most reasonable audio formats. Audio goes through the entire
Windows pipeline, converting as necessary.

One would expect latency to be somewhat better than MME, though it's not clear
if that's really the case in practice. The DirectSound backend has been observed
to [behave very poorly][issue29] with small (<=30 ms) buffer sizes on the input
side, making it a poor choice for low-latency capture use cases.

Modern versions of Windows implement the DirectSound API by using WASAPI
internally, making this backend a "second-class citizen" compared to WASAPI and
WDM-KS.

## WASAPI backend

[Windows Audio Session API][] (WASAPI), first introduced in Windows Vista, is
the most modern Windows audio API. Microsoft actively supports WASAPI and
regularly releases new features for it. PortAudio supports most of these
features and makes them available through various options (though, sadly,
FlexASIO doesn't provide ways to leverage all these options yet).

WASAPI is the only entry point to the Windows audio engine. In modern versions
of Windows, DirectSound and MME merely forward to WASAPI internally. This means
that, in theory, one cannot achieve better performance than WASAPI when using
MME or DirectSound. In practice however, the PortAudio side of the WASAPI
backend could conceivably have limitations or bugs that these other backends
don't have.

WASAPI can be used in two different modes: *shared* or *exclusive*. In FlexASIO,
the [`wasapiExclusiveMode` option][wasapiExclusiveMode] determines which mode is
used. The two modes behave very differently; in fact, they should probably be
seen as two separate backends entirely.

In *shared* mode, WASAPI behaves similarly to MME and DirectSound, in that the
audio goes through most of the normal Windows audio pipeline. One important
limitation of this mode is that there is no sample rate conversion. Therefore,
initialization will fail if the application sample rate is different from the
sample rate of the input or output devices, as configured in the Windows sound
settings. (Corollary: if the input and output devices are configured with
different sample rates in Windows, WASAPI Shared won't work, period.) There is
also no support for upmixing nor downmixing; the channel counts must match
exactly. These limitations [are inherent to WASAPI itself][wasapisr]. It is
reasonable to assume that this mode will provide the best possible latency for
a shared backend.

In *exclusive* mode, WASAPI behaves completely differently and bypasses the
entirety of the Windows audio pipeline, including mixing and APOs. As a result,
PortAudio has a direct path to the audio hardware driver, which makes for an
ideal setup for low latency operation - latencies in the single-digit
milliseconds have been observed. The lack of APOs in the signal path can
also be helpful in applications where fidelity to the original signal is of the
utmost importance, and in fact, this mode can be used for "bit-perfect"
operation. However, since mixing is bypassed, other applications cannot use the
audio device at the same time.

In both modes, the latency numbers reported by WASAPI appear to be more reliable
than other backends.

**Note:** FlexASIO will show channel names (e.g. "Front left") when the WASAPI
backend is used. Channel names are not shown when using other backends due to
PortAudio limitations.

The alternative [ASIO2WASAPI][] universal ASIO driver uses WASAPI.

## WDM-KS backend

[Kernel Streaming][] (WDM stands for [Windows Driver Model][]) is an API
introduced in Windows 98 that enables streaming of audio data directly to the
audio hardware driver, bypassing the entire Windows audio pipeline. In that
sense it is very similar to WASAPI Exclusive, and should behave fairly
similarly.

Compared to WASAPI Exclusive, Kernel Streaming is a much older API that is
perceived as highly complex and less reliable. WASAPI Exclusive should be
expected to provide better results in most cases. That said, it seems that
Kernel Streaming, contrary to MME and DirectSound, is not implemented using
WASAPI in current versions of Windows; therefore, it could conceivably behave
differently in potentially interesting ways.

In terms of latency, WDM-KS has been observed to perform slightly worse than
WASAPI Exclusive, bottoming out at around 10 ms.

**Note:** typically, WDM-KS will fail to initialize if the audio device is
currently opened by any other application, even if no sound is playing.
Consequently, it is very likely to fail when used on the default device. This
issue can be worked around by pointing FlexASIO to another, idle device using
the [`device` option][device].

The alternative [ASIO4ALL][] and [ASIO2KS][] universal ASIO drivers use Kernel
Streaming.

[ASIO2WASAPI]: https://github.com/levmin/ASIO2WASAPI
[ASIO2KS]: http://www.asio2ks.de/
[ASIO4ALL]: http://www.asio4all.org/
[Audio Processing Objects]: https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture
[backend]: CONFIGURATION.md#option-backend
[device]: CONFIGURATION.md#option-device
[DirectSound]: https://en.wikipedia.org/wiki/DirectSound
[DSP]: https://en.wikipedia.org/wiki/Digital_signal_processor
[issue29]: https://github.com/dechamps/FlexASIO/issues/29
[issue30]: https://github.com/dechamps/FlexASIO/issues/30
[Kernel Streaming]: https://en.wikipedia.org/wiki/Windows_legacy_audio_components#Kernel_Streaming
[Multimedia Extensions]: https://en.wikipedia.org/wiki/Windows_legacy_audio_components#Multimedia_Extensions_(MME)
[portaudio]: http://www.portaudio.com/
[wasapiExclusiveMode]: CONFIGURATION.md#option-wasapiExclusiveMode
[Windows Audio Session API]: https://docs.microsoft.com/en-us/windows/desktop/coreaudio/wasapi
[Windows Driver Model]: https://en.wikipedia.org/wiki/Windows_Driver_Model
[wasapisr]: https://docs.microsoft.com/windows/desktop/CoreAudio/device-formats
[WDM-KS issue]: https://github.com/dechamps/FlexASIO/issues/21

<!-- Use the converter at http://http://gravizo.com/ to recover the source code
of this graph. -->
[diagram]: https://g.gravizo.com/svg?digraph%20G%20%7B%0A%09rankdir%3D%22LR%22%0A%09style%3D%22dashed%22%0A%09fontname%3D%22sans-serif%22%0A%09node%5Bfontname%3D%22sans-serif%22%5D%0A%0A%09subgraph%20clusterApplicationProcess%20%7B%0A%09%09label%3D%22Application%20process%22%0A%0A%09%09Host%5Blabel%3D%22ASIO%20host%20application%22%5D%0A%0A%09%09subgraph%20clusterFlexASIO%20%7B%0A%09%09%09label%3D%22FlexASIO%22%0A%09%09%09FlexASIO%5Blabel%3D%22ASIO%20driver%22%5D%0A%0A%09%09%09subgraph%20clusterPortAudio%20%7B%0A%09%09%09%09label%3D%22PortAudio%22%0A%0A%09%09%09%09PortAudio%5Blabel%20%3D%20%22Frontend%22%5D%0A%09%09%09%09subgraph%20%7B%0A%09%09%09%09%09rank%3D%22same%22%0A%09%09%09%09%09node%20%5Bcolor%3D%22red%22%3B%20penwidth%3D3%5D%0A%0A%09%09%09%09%09PortAudioMME%5Blabel%3D%22MME%22%5D%0A%09%09%09%09%09PortAudioDirectSound%5Blabel%3D%22DirectSound%22%5D%0A%09%09%09%09%09PortAudioWASAPI%5Blabel%3D%22WASAPI%22%5D%0A%09%09%09%09%09PortAudioWDMKS%5Blabel%3D%22WDM-KS%22%5D%0A%09%09%09%09%7D%0A%09%09%09%7D%0A%09%09%7D%0A%09%7D%0A%0A%09subgraph%20clusterWindows%20%7B%0A%09%09label%3D%22Windows%20audio%20subsystem%22%0A%09%09subgraph%20%7B%0A%09%09%09rank%3D%22same%22%0A%09%09%09MME%0A%09%09%09DirectSound%0A%09%09%09WASAPIShared%5Blabel%3D%22WASAPI%20(shared)%22%5D%0A%09%09%09WASAPIExclusive%5Blabel%3D%22WASAPI%20(exclusive)%22%5D%0A%09%09%09WDMKS%5Blabel%3D%22Kernel%20Streaming%22%5D%0A%09%09%7D%0A%0A%09%09SampleRateConversion%5Blabel%3D%22Sample%20rate%20conversion%22%5D%0A%09%09PreMix%5Blabel%3D%22Pre-mix%20APOs%22%5D%0A%09%09Mix%5Blabel%3D%22Mixing%22%5D%0A%09%09PostMix%5Blabel%3D%22Post-mix%20APOs%22%5D%0A%09%7D%0A%0A%09subgraph%20clusterHardware%20%7B%0A%09%09label%3D%22Audio%20hardware%22%0A%09%09HardwareDriver%5Blabel%3D%22Driver%22%5D%0A%09%09HardwareDevice%5Blabel%3D%22Device%22%5D%0A%09%7D%0A%0A%09Host-%3EFlexASIO%0A%09FlexASIO-%3EPortAudio%0A%0A%09PortAudio-%3E%7B%0A%09%09PortAudioMME%0A%09%09PortAudioDirectSound%0A%09%09PortAudioWASAPI%0A%09%09PortAudioWDMKS%0A%09%7D%0A%0A%09PortAudioMME-%3EMME%0A%09PortAudioDirectSound-%3EDirectSound%0A%09PortAudioWASAPI-%3EWASAPIShared%0A%09PortAudioWASAPI-%3EWASAPIExclusive%0A%09PortAudioWDMKS-%3EWDMKS%0A%0A%09MME-%3ESampleRateConversion%0A%09DirectSound-%3ESampleRateConversion%0A%09SampleRateConversion-%3EWASAPIShared%0A%09%0A%09WASAPIShared-%3EPreMix%0A%09WASAPIExclusive-%3EHardwareDriver%0A%09PreMix-%3EMix%0A%09Mix-%3EPostMix%0A%09PostMix-%3EHardwareDriver%0A%09%0A%09WDMKS-%3EHardwareDriver%0A%09%0A%09HardwareDriver-%3EHardwareDevice%0A%7D%0A
