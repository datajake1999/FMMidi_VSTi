# Introduction
This is a quickly thrown together VSTi wrapper around the FMMidi engine. The synthesizer code was taken from [the following repository](https://github.com/supercatexpert/fmmidi)

# Automatable parameters

* Volume: Synth master volume.
* VolumeDisplay: Sets the unit for displaying the aforementioned Volume parameter, either dB or %.
* Transpose: Applies an offset to Incoming MIDI notes.
* PushMidi: Queue's MIDI events Instead of processing them Immediately. Queued events have sample accurate timing, while immediate events can have jittery playback with large audio buffers.

# Extra notes

* This is only a VST2 compatible plug-in. A VST3 version Is not planned for various reasons.
