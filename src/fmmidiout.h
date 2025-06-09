#ifndef FMMIDIOUT_H
#define FMMIDIOUT_H

#include "midisynth.hpp"

class fmOut
{
private:
	midisynth::synthesizer *synth;
	midisynth::fm_note_factory *note_factory;
	midisynth::DRUMPARAMETER p;
	void load_programs();
public:
	fmOut();
	void midi_message(int port, uint_least32_t message);
	void sysex_message(int port, const void* data, std::size_t size);
	int synthesize(int_least16_t* output, std::size_t samples, float rate);
	void set_mode(midisynth::system_mode_t mode);
	void reset();
	void panic();
	int get_program(int ch);
};

#endif
