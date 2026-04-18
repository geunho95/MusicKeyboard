# Sample Drop Folder

Host preview looks for a source WAV file in one of these places:

- `samples/source.wav`
- `samples/recording.wav`
- `samples/po33.wav`

If those exact names are not present, it will scan the `samples` folder and pick the first visible WAV file.

Ignored during scan:

- `.DS_Store`
- `._anything.wav`
- any other hidden dotfile

You can also override the path with:

```bash
MK_SAMPLE_WAV=/absolute/path/to/your-file.wav ./firmware/build/musickeyboard_sampler
```

Expected input for the current loader:

- PCM WAV
- `8-bit` or `16-bit`
- mono or stereo

Stereo files are mixed down to mono for now.

The current prototype maps the same source sample to four button chords:

- blue 1: `Cmaj`
- blue 2: `Am`
- blue 3: `Fmaj`
- blue 4: `Gmaj`
