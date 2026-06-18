# vgm-to-midi-converter

This command-line program takes .vgm or .vgz files as input, and converts them into midi.

vgm files can be downloaded from [vgmrips.net](https://vgmrips.net/packs/).

**This program is a work in progress, it supports only a small group of soundchips.**

Samples in the vgm file are output as .sf2 files.

The output midi files can be played back with [Thaumoc](https://github.com/Thysbelon/Thaumoc). Some tracks of the output midi will require an SF2 player, I recommend [FluidSynthPlugin](https://github.com/prof-spock/FluidSynthPlugin).

[Please do not attempt to use the output midi files in FL Studio.](https://gist.github.com/Thysbelon/a69da7038e65023a29168d9ef449acda)

## How to Download
Downloads are in the Releases section of this page.

## Supported Soundchips:

### Required Virtual Instrument: Thaumoc
- YM2151

### Required Virtual Instrument: an SF2 Player
- MSM6258, A.K.A. OKIM6258

## Supported Consoles:
- Sharp X68000

## Credits
- [libsmf by loveemu](https://github.com/loveemu/loveemu-lab/tree/master/nds/sseq2mid/src)
- [adpcm library from mdxtools by vampirefrog](https://github.com/vampirefrog/mdxtools)
- [sf2cute by gocha](https://github.com/gocha/sf2cute)
