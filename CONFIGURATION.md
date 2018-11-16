# FlexASIO Configuration

FlexASIO does not provide a graphical interface ([GUI][]) to adjust its
settings. This is because developing a GUI typically requires a
significant amount of developer time that FlexASIO, sadly, doesn't have.
This explains why nothing happens when you click on the ASIO driver
"configure" or "settings" button in your application.

Instead, FlexASIO settings can be specified using a
[configuration file][]. FlexASIO will search for a file named
`FlexASIO.toml` directly inside your Windows user profile folder; for
example: `C:\Users\Your Name\FlexASIO.toml`.

If the file is missing, this is equivalent to supplying an empty file,
and as a result FlexASIO will use default values for everything.

The configuration file is a text file that can be edited using any text editor,
such as Notepad. The file follows the [TOML][] syntax, which is very similar to
the syntax used for [INI files][]. Every feature described in the [official TOML documentation] should be supported.

FlexASIO will silently ignore attempts to set options that don't exist,
so beware of typos. However, if an existing option is set to an invalid
value (which includes using the wrong type or missing quotes), FlexASIO
will *fail to initialize*. The FlexASIO log will contain details about what went
wrong.

## Example configuration file

```toml
# Use WASAPI as the PortAudio host API backend.
backend = "Windows WASAPI"

[input]
# Disable the input. It is strongly recommended to do this if you only want to
# stream audio in one direction.
device = ""

[output]
# Select the output device. The name comes from the output of the
# PortAudioDevices program.
device = "Speakers (Realtek High Definition Audio)"

# Open the hardware output device with 6 channels. This is only required if you
# are unhappy with the default channel count.
channels = 6

# Set the output to WASAPI Exclusive Mode.
wasapiExclusiveMode = true
```

Experimentally, the following set of options have been shown to be a good
starting point for low latency operation:

```toml
backend = "Windows WASAPI"

[input]
wasapiExclusiveMode = true
bufferSizeSamples = 480
suggestedLatencySeconds = 0.0

[output]
wasapiExclusiveMode = true
bufferSizeSamples = 480
suggestedLatencySeconds = 0.0
```

## Options reference

### Global section

These options are outside of any section ("table" in TOML parlance), and affect
both input and output streams.

#### Option `backend`

*String*-typed option that determines which audio backend FlexASIO will attempt
to use. In PortAudio parlance, this is called the *host API*. FlexASIO uses the
term "backend" to avoid potential confusion with the term "ASIO host".

This is by far the most important option in FlexASIO. Changing the backend can
have wide-ranging consequences on the operation of the entire audio pipeline.
For more information, see [BACKENDS][].

The value of the option is matched against PortAudio host API names, as
shown in the output of the `PortAudioDevices` program. If the specified name
doesn't match any host API, FlexASIO will fail to initialize.

In practice, PortAudio will recognize the following names: `MME`,
`Windows DirectSound`, `Windows WASAPI` and `Windows WDM-KS`.

Example:

```toml
backend = "Windows WASAPI"
```

The default behaviour is to use DirectSound.

#### Option `bufferSizeSamples`

*Integer*-typed option that determines which ASIO buffer size (in samples)
FlexASIO will suggest to the ASIO Host application.

This option can have a major impact on reliability and latency. Smaller buffers
will reduce latency but will increase the likelihood of glitches/discontinuities
(buffer overflow/underrun) if the audio pipeline is not fast enough.

Note that some host applications might already provide a user-controlled buffer
size setting; in this case, there should be no need to use this option. It is
useful only when the application does not provide a way to customize the buffer
size.

The ASIO buffer size is also used as the PortAudio buffer size, as FlexASIO
bridges the two. Note that, for various technical reasons and depending on the
backend and settings used (especially the `suggestedLatencySeconds` option),
there are many scenarios where additional buffers will be inserted in the audio
pipeline (either by PortAudio or by Windows itself), *in addition* to the ASIO
buffer. This can result in overall latency being higher than what the ASIO
buffer size alone would suggest.

Example:

```toml
bufferSizeSamples = 480 # 10 ms at 48 kHz
```

The default behaviour is to advertise minimum, preferred and maximum buffer
sizes of 1 ms, 20 ms and 1 s, respectively. The resulting sizes in samples are
computed based on whatever sample rate the driver is set to when the application
enquires.

### `[input]` and `[output]` sections

Options in this section only apply to the *input* (capture, recording) audio
stream or to the *output* (rendering, playback) audio stream, respectively.

#### Option `device`

*String*-typed option that determines which hardware audio device FlexASIO will
attempt to use.

The value of the option is the *full name* of the device. The list of available
device names is shown by the `PortAudioDevices` command line program which can
be found in the FlexASIO installation folder. The value of this option must
exactly match the "Device name" shown by `PortAudioDevices`, including any
text in parentheses.

**Note:** only devices that match the selected *backend* (see above) will be
considered. In other words, the "Host API name" as shown in the output of
`PortAudioDevices` has to match the value of the `backend` option. Beware that a
given hardware device will not necessarily have the same name under different
backends.

If the specified name doesn't match any device under the selected backend,
FlexASIO will fail to initialize.

If the option is set to the empty string (`""`), no device will be used; that
is, the input or output side of the stream will be disabled, and all other
options in the section will be ignored. Note that making your ASIO Host
Application unselect all input channels or all output channels will achieve the
same result.

Example:

```toml
[input]
device = ""

[output]
device = "Speakers (Realtek High Definition Audio)"
```

The default behaviour is to use the default device for the selected backend.
`PortAudioDevices` will show which device that is. Typically, this would be the
device set as default in the Windows audio control panel.

#### Option `channels`

*Integer*-typed option that determines how many channels FlexASIO will open the
hardware audio device with. This is the number of channels the ASIO Host
Application will see.

**Note:** even if the ASIO Host Application only decides to use a subset of the
available channels, the hardware audio device will still be opened with the
number of channels configured here. In other words, the host application has no
control over the hardware channel configuration. The only exception is if the
application does not request any input channels, or any output channels; in this
case the input or output device (respectively) won't be opened at all.

If the requested channel count doesn't match what the audio device is configured
for, the resulting behaviour depends on the backend. Some backends will accept
any channel count, upmixing or downmixing as necessary. Other backends might
refuse to initialize.

The value of this option must be strictly positive. To completely disable the
input or output, set the `device` option to the empty string (see above).

**Note:** with the WASAPI backend, setting this option has the side effect of
disabling channel masks. This means channel names will not be shown, and the
backend might behave differently with regard to channel routing.

Example:

```toml
[input]
channels = 2

[output]
channels = 6
```

The default behaviour is to use the maximum channel count for the selected
device as reported by PortAudio. This information is shown in the output of the
`PortAudioDevice` program. Sadly, PortAudio often gets the channel count wrong,
so setting this option explicitly might be necessary for correct operation.

#### Option `suggestedLatencySeconds`

*Floating-point*-typed option that determines the amount of audio latency (in
seconds) that FlexASIO will "suggest" to PortAudio. In some cases this can
influence the amount of additional buffering that will be introduced in the
audio pipeline in addition to the ASIO buffer itself (see also the
`bufferSizeSamples` option). As a result, this option can have a major impact
on reliability and latency.

Note that this is only a hint; the resulting latency can be very different from
the value of this option. PortAudio backends interpret this setting in
complicated and confusing ways, so it is recommended to experiment with various
values. For example, the WASAPI backend will interpret the value `0.0`
specially, optimizing for low latency operation.

**Note:** the TOML parser that FlexASIO uses require all floating point values
to have a decimal point. So, for example, `1` will not work, but `1.0` will.

Example:

```toml
[output]
suggestedLatencySeconds = 0.01
```

The default value is the ASIO buffer size divided by the sample rate; that is,
20 ms if using the default preferred ASIO buffer size.

#### Option `wasapiExclusiveMode`

*Boolean*-typed option that determines if the stream should be opened in
*WASAPI Shared* or in *WASAPI Exclusive* mode. For more information, see
[BACKENDS][].

This option is ignored if the backend is not WASAPI. See the `backend` option,
above.

Example:

```toml
backend = "Windows WASAPI"

[input]
wasapiExclusiveMode = true
```

The default behaviour is to open the stream in *shared* mode.

[BACKENDS]: BACKENDS.md
[configuration file]: https://en.wikipedia.org/wiki/Configuration_file
[GUI]: https://en.wikipedia.org/wiki/Graphical_user_interface
[INI files]: https://en.wikipedia.org/wiki/INI_file
[official TOML documentation]: https://github.com/toml-lang/toml#toml
[TOML]: https://en.wikipedia.org/wiki/TOML
