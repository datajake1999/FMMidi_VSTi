#include "fmmidiout.h"

void fmOut::load_programs()
{
	#include "program.h"
}

fmOut::fmOut()
{
	note_factory = new midisynth::fm_note_factory;
	synth = new midisynth::synthesizer(note_factory);
	
	load_programs();
}

void fmOut::midi_message(int port, uint_least32_t message)
{
	synth->midi_event(message);
}

void fmOut::sysex_message(int port, const void* data, std::size_t size)
{
	synth->sysex_message(data, size);
}

int fmOut::synthesize(int_least16_t* output, std::size_t samples, float rate)
{
	return synth->synthesize(output, samples, rate);
}

void fmOut::set_mode(midisynth::system_mode_t mode)
{
	synth->set_system_mode(mode);
}

void fmOut::reset()
{
	synth->reset();
}

void fmOut::panic()
{
	synth->all_note_off();
}

int fmOut::get_program(int ch)
{
	return synth->get_channel(ch)->get_program();
}
