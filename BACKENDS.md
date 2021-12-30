# FlexASIO backends

This document provides background information about what FlexASIO *backends*
are, and the differences between them. It is recommended reading for anyone
wanting to optimize their audio pipeline. In particular, this document provides
the necessary information to understand what the [`backend`][backend] and
[`wasapiExclusiveMode`][wasapiExclusiveMode] FlexASIO configuration options do.

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
  of the quality and maturity of the code involved. Some might be more likely
  to introduce audio discontinuities (glitches).
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
    post-mixing (MFX/EFX/GFX) processing on audio data. Some backends will
    bypass this processing, some won't.
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
the fact that MME appears to [behave very poorly][issue30] with small buffer
sizes.

Latency numbers reported by MME do not seem to take the Windows audio pipeline
into account. This means the reported latency is underestimated by at least 20
ms, if not more.

**Note:** the channel count exposed by FlexASIO when using MME can be a bit odd.
For example, it might expose 8 channels for a 5.1 output, downmixing the rear
channels pairs.

Modern versions of Windows implement the MME API by using WASAPI Shared
internally, making this backend a "second-class citizen" compared to WASAPI and
WDM-KS.

## DirectSound backend

[DirectSound][] was introduced in 1995. It is mainly designed for use in video
games.

In practice, DirectSound should be expected to behave somewhat similarly to MME.
It will accept most reasonable audio formats. Audio goes through the entire
Windows pipeline, converting as necessary.

One would expect latency to be somewhat better than MME, though it's not clear
if that's really the case in practice. The DirectSound backend has been observed
to [behave very poorly][issue29] with small buffer sizes on the input side,
making it a poor choice for low-latency capture use cases.

Modern versions of Windows implement the DirectSound API by using WASAPI Shared
internally, making this backend a "second-class citizen" compared to WASAPI and
WDM-KS.

## WASAPI backend

[Windows Audio Session API][] (WASAPI), first introduced in Windows Vista, is
the most modern Windows audio API. Microsoft actively supports WASAPI and
regularly releases new features for it. PortAudio supports most of these
features and makes them available through various options (though, sadly,
FlexASIO doesn't provide ways to leverage all these options yet).

WASAPI is the only entry point to the shared Windows audio processing pipeline,
which runs in the *Windows Audio* service (`audiosrv`). In modern versions of
Windows, DirectSound and MME merely forward to WASAPI internally. This means
that, in theory, one cannot achieve better performance than WASAPI when usin
MME or DirectSound. In practice however, the PortAudio side of the WASAPI
backend could conceivably have limitations or bugs that these other backends
don't have.

WASAPI can be used in two different modes: *shared* or *exclusive*. In FlexASIO,
the [`wasapiExclusiveMode` option][wasapiExclusiveMode] determines which mode is
used. The two modes behave very differently; in fact, they should probably be
seen as two separate backends entirely.

In *shared* mode, WASAPI behaves similarly to MME and DirectSound, in that the
audio goes through the normal Windows audio processing pipeline, including
mixing and APOs. Indeed, in modern versions of Windows, MME and DirectSound are
just thin wrappers implemented on top of WASAPI Shared. For this reason it is
reasonable to assume that this mode will provide the best possible latency for a
shared backend.

In *exclusive* mode, WASAPI behaves completely differently and bypasses the
entirety of the Windows audio pipeline. As a result, PortAudio has a nearly
direct path to the audio hardware driver, which makes for an ideal setup for low
latency operation - latencies in the single-digit milliseconds have been
observed. The lack of APOs in the signal path can also be helpful in
applications where fidelity to the original signal is of the utmost importance,
and in fact, this mode can be used for "bit-perfect" operation. However, since
mixing is bypassed, other applications cannot use the audio device at the same
time.

In both modes, the latency numbers reported by WASAPI appear to be more reliable
than other backends.

**Note:** FlexASIO will show channel names (e.g. "Front left") when the WASAPI
backend is used. Channel names are not shown when using other backends due to
PortAudio limitations.

Windows implements WASAPI using Kernel Streaming internally.

The alternative [ASIO2WASAPI][] universal ASIO driver uses WASAPI.

## WDM-KS backend

[Kernel Streaming][] was first introduced in Windows 98 and is the interface
between Windows audio device drivers (also known as WDM, which stands for
[Windows Driver Model][]), running in kernel mode, and processes running in
user mode. Note that, in this context, "Windows audio device driver" refers to
Windows hardware drivers, not ASIO drivers - the two concepts are not directly
related to each other.

The Windows audio engine itself (WASAPI) uses Kernel Streaming internally to
communicate with audio device drivers. It logically follows that any device that
behaves as a normal Windows audio device *de facto* comes with a WDM driver that
implements Kernel Streaming (usually not directly but through a [PortCls
miniport driver][]). Calls made through any of the standard Windows audio APIs
(MME, DirectSound, WASAPI) eventually become Kernel Streaming calls as they
cross the boundary into kernel mode and enter the Windows audio device driver.

In the typical case, the only client of the Windows audio device driver is the
Windows audio engine. It is, however, technically possible, albeit highly
atypical, for an application to issue Kernel Streaming requests directly,
bypassing the Windows audio engine and talking to the Windows kernel directly.
This is what the WDM-KS PortAudio backend does.

Given the above, Kernel Streaming offers the most direct path to the audio
device among all PortAudio backends, but comes with some downsides:

- Many audio outputs only handle a single stream at a time, because many audio
  devices do not support hardware mixing. (In Kernel Streaming terms, their
  *pins* only support one *instance* at a time.) These devices can therefore
  only be used by one KS client at a time, making Kernel Streaming an
  *exclusive* backend in this case. Because the Windows audio engine is itself a
  KS client, it is usually not possible to access an audio device using KS if
  the Windows audio engine is already using it. Use the [`device`
  option][device] to select a device that the Windows audio engine is not
  currently using.
- Kernel Streaming is a very flexible API, which also makes it quite
  complicated. Even just enumerating audio devices involves quite a bit of
  complex, error-prone logic to be implemented in the application (here, in the
  PortAudio KS backend). Different device drivers implement KS calls in
  different ways, report different [topologies][], and even [different ways of
  handling audio buffers][wdmwave]. This presents a lot of opportunities for
  things to go wrong in a variety of different ways depending on the specific
  audio device used. Presumably this is the reason why most applications do not
  attempt to use KS directly, and the reason why Microsoft does not recommend
  this approach.

Note that the list of devices that the PortAudio WDM-KS backend exposes might
look a bit different from the list of devices shown in the Windows audio
settings. This is because the Windows audio engine [generates its own list of
devices][AudioEndpointBuilder] (or, more specifically, [audio endpoint
devices][]) by interpreting information returned by Kernel Streaming. When using
KS directly this logic is bypassed, and the PortAudio WDM-KS backend uses its
own logic to discover devices. Furthermore, the concept of a device "name" is
specific to the Windows audio engine and does not apply to KS, which explains
why PortAudio WDM-KS device names do not necessarily match the Windows audio
settings.

In principle, similar results should be obtained when using WASAPI Exclusive
and Kernel Streaming, since they both offer exclusive access to the hardware.
WASAPI is simpler and less likely to cause problems, but Kernel Streaming is
more direct and more flexible. Furthermore, their internal implementation in
PortAudio are very different. Therefore, the WASAPI Exclusive and WDM-KS
PortAudio backends might behave somewhat differently depending on the situation.

The alternative [ASIO4ALL][] and [ASIO2KS][] universal ASIO drivers use Kernel
Streaming.

---

*ASIO is a trademark and software of Steinberg Media Technologies GmbH*

[ASIO2WASAPI]: https://github.com/levmin/ASIO2WASAPI
[ASIO2KS]: http://www.asio2ks.de/
[ASIO4ALL]: http://www.asio4all.org/
[AudioEndpointBuilder]: https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/audio-endpoint-builder-algorithm
[audio endpoint devices]: https://docs.microsoft.com/en-us/windows/win32/coreaudio/audio-endpoint-devices
[Audio Processing Objects]: https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture
[backend]: CONFIGURATION.md#option-backend
[device]: CONFIGURATION.md#option-device
[DirectSound]: https://en.wikipedia.org/wiki/DirectSound
[DSP]: https://en.wikipedia.org/wiki/Digital_signal_processor
[issue29]: https://github.com/dechamps/FlexASIO/issues/29
[issue30]: https://github.com/dechamps/FlexASIO/issues/30
[Kernel Streaming]: https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/kernel-streaming
[Multimedia Extensions]: https://en.wikipedia.org/wiki/Windows_legacy_audio_components#Multimedia_Extensions_(MME)
[portaudio]: http://www.portaudio.com/
[PortCls miniport driver]: https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/audio-miniport-drivers
[topologies]: https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/specifying-the-topology
[wasapiExclusiveMode]: CONFIGURATION.md#option-wasapiExclusiveMode
[Windows Audio Session API]: https://docs.microsoft.com/en-us/windows/desktop/coreaudio/wasapi
[Windows Driver Model]: https://en.wikipedia.org/wiki/Windows_Driver_Model
[wdmwave]: https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/wave-filters

<!-- Use the converter at http://gravizo.com/ to recover the source code of this
graph. -->
[diagram]: https://g.gravizo.com/svg?digraph%20G%20%7B%0A%09rankdir%3D%22LR%22%0A%09style%3D%22dashed%22%0A%09fontname%3D%22sans-serif%22%0A%09node%5Bfontname%3D%22sans-serif%22%5D%0A%0A%09subgraph%20clusterUserMode%20%7B%0A%09%09label%3D%22Windows%20user%20mode%22%0A%0A%09%09subgraph%20clusterApplicationProcess%20%7B%0A%09%09%09label%3D%22Application%20process%22%0A%0A%09%09%09Host%5Blabel%3D%22ASIO%20host%5Cnapplication%22%5D%0A%0A%09%09%09subgraph%20clusterFlexASIO%20%7B%0A%09%09%09%09label%3D%22FlexASIO%22%0A%09%09%09%09FlexASIO%5Blabel%3D%22ASIO%5Cndriver%22%5D%0A%0A%09%09%09%09subgraph%20clusterPortAudio%20%7B%0A%09%09%09%09%09label%3D%22PortAudio%22%0A%0A%09%09%09%09%09PortAudio%5Blabel%20%3D%20%22Frontend%22%5D%0A%09%09%09%09%09subgraph%20%7B%0A%09%09%09%09%09%09rank%3D%22same%22%0A%09%09%09%09%09%09node%20%5Bcolor%3D%22red%22%3B%20penwidth%3D3%5D%0A%0A%09%09%09%09%09%09PortAudioMME%5Blabel%3D%22MME%22%5D%0A%09%09%09%09%09%09PortAudioDirectSound%5Blabel%3D%22DirectSound%22%5D%0A%09%09%09%09%09%09PortAudioWASAPI%5Blabel%3D%22WASAPI%22%5D%0A%09%09%09%09%09%09PortAudioWDMKS%5Blabel%3D%22WDM-KS%22%5D%0A%09%09%09%09%09%7D%0A%09%09%09%09%7D%0A%09%09%09%7D%0A%09%09%7D%0A%0A%09%09subgraph%20%7B%0A%09%09%09rank%3D%22same%22%0A%09%09%09MME%5Blabel%3D%22MME%5Cnlibrary%22%5D%0A%09%09%09DirectSound%5Blabel%3D%22DirectSound%5Cnlibrary%22%5D%0A%09%09%09WASAPI%5Blabel%3D%22WASAPI%5Cnlibrary%22%5D%0A%09%09%7D%0A%0A%09%09subgraph%20%7B%0A%09%09%09rank%3D%22same%22%0A%09%09%09WASAPIShared%5Blabel%3D%22WASAPI%5Cn(shared)%22%5D%0A%09%09%09WASAPIExclusive%5Blabel%3D%22WASAPI%5Cn(exclusive)%22%5D%0A%09%09%7D%0A%0A%09%09subgraph%20clusterWindows%20%7B%0A%09%09%09label%3D%22Windows%20Audio%20Service%20(audiosrv%2C%20audiodg)%22%0A%09%09%09%0A%09%09%09PreMix%5Blabel%3D%22Pre-mix%5CnAPOs%22%5D%0A%09%09%09Mix%5Blabel%3D%22Mixing%22%5D%0A%09%09%09PostMix%5Blabel%3D%22Post-mix%5CnAPOs%22%5D%0A%09%09%7D%0A%0A%09%09KS%5Blabel%3D%22Kernel%20Streaming%5Cnlibrary%22%5D%0A%09%7D%0A%0A%09WDM%5Blabel%3D%22Windows%20audio%5Cndevice%20driver%5Cn(WDM%2C%20kernel%20mode)%22%5D%0A%09Hardware%5Blabel%3D%22Hardware%5Cndevice%22%5D%0A%0A%09Host-%3EFlexASIO%0A%09FlexASIO-%3EPortAudio%0A%0A%09PortAudio-%3E%7B%0A%09%09PortAudioMME%0A%09%09PortAudioDirectSound%0A%09%09PortAudioWASAPI%0A%09%09PortAudioWDMKS%0A%09%7D%0A%0A%09PortAudioMME-%3EMME%0A%09PortAudioDirectSound-%3EDirectSound%0A%09PortAudioWASAPI-%3EWASAPI%0A%09PortAudioWDMKS-%3EKS%0A%0A%09MME-%3EWASAPIShared%0A%09DirectSound-%3EWASAPIShared%0A%09WASAPI-%3EWASAPIShared%0A%09WASAPI-%3EWASAPIExclusive%0A%09%0A%09WASAPIShared-%3EPreMix%0A%09PreMix-%3EMix%0A%09Mix-%3EPostMix%0A%09PostMix-%3EKS%0A%0A%09WASAPIExclusive-%3EKS%0A%09%0A%09KS-%3EWDM%0A%09WDM-%3EHardware%0A%7D%0A
