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
# Set the input to WASAPI Exclusive Mode.
wasapiExclusiveMode = true

[output]
# Set the output to WASAPI Shared Mode.
# Note that this is already the default, so this doesn't actually do
# anything.
wasapiExclusiveMode = false
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

### `[input]` and `[output]` sections

Options in this section only apply to the *input* (capture, recording) audio
stream or to the *output* (rendering, playback) audio stream, respectively.

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
