/*
FMMidi VSTi
Copyright (C) 2021-2025  Datajake

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "FMMidi.h"

void FMMidi::setSampleRate (float sampleRate)
{
	lock.acquire();
	AudioEffectX::setSampleRate (sampleRate);
	lock.release();
}

void FMMidi::setBlockSize (VstInt32 blockSize)
{
	lock.acquire();
	clearBuffer ();
	AudioEffectX::setBlockSize (blockSize);
	initBuffer ();
	lock.release();
}

void FMMidi::setBlockSizeAndSampleRate (VstInt32 blockSize, float sampleRate)
{
	setBlockSize (blockSize);
	setSampleRate (sampleRate);
}

void FMMidi::initSynth ()
{
	synth = new fmOut();
}

void FMMidi::initBuffer ()
{
	buffer = new short[2*blockSize];
	if (buffer)
	{
		memset(buffer, 0, (2*blockSize)*sizeof(short));
	}
}

void FMMidi::clearSynth ()
{
	if (synth)
	{
		delete synth;
		synth = NULL;
	}
}

void FMMidi::clearBuffer ()
{
	if (buffer)
	{
		memset(buffer, 0, (2*blockSize)*sizeof(short));
		delete[] buffer;
		buffer = NULL;
	}
}

bool FMMidi::getErrorText (char* text)
{
	if (!text)
	{
		return false;
	}
	if (!synth)
	{
		sprintf(text, "Error initializing synth.\n");
		return true;
	}
	if (!buffer)
	{
		sprintf(text, "Error initializing buffer.\n");
		return true;
	}
	return false;
}

void FMMidi::suspend ()
{
	lock.acquire();
	MidiQueue.Flush(true);
#if REAPER_EXTENSIONS
	ParameterQueue.Flush(true);
#endif
	if (synth)
	{
		synth->panic();
	}
	lock.release();
}

void FMMidi::resume ()
{
	lock.acquire();
	AudioEffectX::resume ();
	if (synth)
	{
		synth->reset();
	}
	lock.release();
}

float FMMidi::getVu ()
{
	if (bypassed)
	{
		return 0;
	}
	return (float)fabs((vu[0] + vu[1]) / 2);
}

#if !VST_FORCE_DEPRECATED
void FMMidi::process (float** inputs, float** outputs, VstInt32 sampleFrames)
{
	processTemplate (inputs, outputs, sampleFrames);
}

#endif
void FMMidi::processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames)
{
	processTemplate (inputs, outputs, sampleFrames);
}

#if VST_2_4_EXTENSIONS
void FMMidi::processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames)
{
	processTemplate (inputs, outputs, sampleFrames);
}

#endif
template <class sampletype>
void FMMidi::processTemplate (sampletype** inputs, sampletype** outputs, VstInt32 sampleFrames)
{
	sampletype* in1 = inputs[0];
	sampletype* in2 = inputs[1];
	sampletype* out1 = outputs[0];
	sampletype* out2 = outputs[1];
	double begin;
	double end;

	if (bypassed || !buffer || !lock.tryAcquire())
	{
		begin = GetCPUTime();
		if (out1)
		{
			if (in1)
			{
				memcpy(out1, in1, sampleFrames*sizeof(sampletype));
			}
			else
			{
				memset(out1, 0, sampleFrames*sizeof(sampletype));
			}
		}
		if (out2)
		{
			if (in2)
			{
				memcpy(out2, in2, sampleFrames*sizeof(sampletype));
			}
			else
			{
				memset(out2, 0, sampleFrames*sizeof(sampletype));
			}
		}
		end = GetCPUTime();
		calculateCPULoad (begin, end, sampleFrames);
		lock.release();
		return;
	}

	if (sampleFrames > blockSize)
	{
		sampleFrames = blockSize;
	}

	begin = GetCPUTime();
	VstInt32 totalFrames = sampleFrames;
	VstInt32 renderedFrames = 0;
	short* bufferPointer = buffer;
	while (totalFrames > 0)
	{
		while (MidiQueue.HasEvents() && MidiQueue.GetEventTime() <= renderedFrames)
		{
			processEvent (MidiQueue.GetNextEvent());
		}
		VstInt32 currentFrames = MidiQueue.GetEventTime() - renderedFrames;
		if (currentFrames > totalFrames || currentFrames <= 0)
		{
			currentFrames = totalFrames;
		}
		if (synth)
		{
			synth->synthesize(bufferPointer, currentFrames, sampleRate);
		}
		bufferPointer += 2*currentFrames;
		renderedFrames += currentFrames;
		totalFrames -= currentFrames;
	}
	while (MidiQueue.HasEvents())
	{
		processEvent (MidiQueue.GetNextEvent());
	}
	for (VstInt32 i = 0; i < sampleFrames; i++)
	{
#if REAPER_EXTENSIONS
		while (ParameterQueue.HasEvents() && ParameterQueue.GetEventTime() <= i)
		{
			processEvent (ParameterQueue.GetNextEvent());
		}
#endif
		if (out1)
		{
			out1[i] = buffer[i*2+0] / (sampletype)32768;
			out1[i] = out1[i] * Volume;
#ifdef DEMO
			if (time(NULL) >= startTime + 600)
			{
				out1[i] += ((rand() / (sampletype)RAND_MAX) / (sampletype)256);
			}
#endif
			if (in1)
			{
				out1[i] += in1[i];
			}
			vu[0] = out1[i];
		}
		if (out2)
		{
			out2[i] = buffer[i*2+1] / (sampletype)32768;
			out2[i] = out2[i] * Volume;
#ifdef DEMO
			if (time(NULL) >= startTime + 600)
			{
				out2[i] += ((rand() / (sampletype)RAND_MAX) / (sampletype)256);
			}
#endif
			if (in2)
			{
				out2[i] += in2[i];
			}
			vu[1] = out2[i];
		}
	}
#if REAPER_EXTENSIONS
	while (ParameterQueue.HasEvents())
	{
		processEvent (ParameterQueue.GetNextEvent());
	}
#endif
	end = GetCPUTime();
	calculateCPULoad (begin, end, sampleFrames);
	lock.release();
}

void FMMidi::calculateCPULoad (double begin, double end, VstInt32 numsamples)
{
	double GenerateDuration = (end - begin) / GetCPUFrequency();
	double BufferDuration = numsamples * (1.0 / sampleRate);
	CPULoad = (GenerateDuration / BufferDuration) * 100.0;
}

VstInt32 FMMidi::processEvents (VstEvents* ev)
{
	lock.acquire();
	if (bypassed || !ev)
	{
		if (ev && hi.ReceiveEvents)
		{
			sendVstEventsToHost (ev);
		}
		lock.release();
		return 0;
	}

	if (PushMidi >= 0.5)
	{
		VstInt32 eventCount = ev->numEvents;
		if (eventCount > EVBUFSIZE)
		{
			eventCount = EVBUFSIZE;
		}
		for (VstInt32 i = 0; i < eventCount; i++)
		{
			if (ev->events[i]->type == kVstMidiType)
			{
				if (!MidiQueue.EnqueueEvent(ev->events[i]))
				{
					break;
				}
			}
#if VST_2_4_EXTENSIONS
			else if (ev->events[i]->type == kVstSysExType)
			{
				processEvent (ev->events[i]);
			}
#endif
		}
	}
	else
	{
		for (VstInt32 i = 0; i < ev->numEvents; i++)
		{
			processEvent (ev->events[i]);
		}
	}
	if (hi.ReceiveEvents)
	{
		sendVstEventsToHost (ev);
	}
	lock.release();
	return 1;
}

void FMMidi::processEvent (VstEvent* ev)
{
	if (!ev)
	{
		return;
	}
	if (ev->type == kVstMidiType)
	{
		VstMidiEvent* event = (VstMidiEvent*)ev;
		char* midiData = event->midiData;
		sendMidi (midiData);
	}
#if VST_2_4_EXTENSIONS
	else if (ev->type == kVstSysExType)
	{
		VstMidiSysexEvent* event = (VstMidiSysexEvent*)ev;
		if (synth)
		{
			synth->sysex_message(0, event->sysexDump, event->dumpBytes);
		}
	}
#endif
#if REAPER_EXTENSIONS
	else if (ev->type == kVstParameterType)
	{
		VstParameterEvent* event = (VstParameterEvent*)ev;
		setParameter (event->index, event->value);
	}
#endif
}

void FMMidi::sendMidi (char* data)
{
	if (!data)
	{
		return;
	}
	unsigned char byte1 = data[0] & 0xff;
	unsigned char byte2 = data[1] & 0x7f;
	unsigned char byte3 = data[2] & 0x7f;
	unsigned char type = byte1 & 0xf0;
	unsigned char channel = byte1 & 0x0f;
	if (!ChannelEnabled[channel])
	{
		return;
	}
	if (Transpose >= 1 || Transpose <= -1)
	{
		if (type == 0x80 || type == 0x90 || type == 0xa0)
		{
			if (channel != 9)
			{
				VstInt32 note = byte2 + (VstInt32)Transpose;
				if (note > 127)
				{
					note = 127;
				}
				else if (note < 0)
				{
					note = 0;
				}
				byte2 = (unsigned char)note;
			}
		}
	}
	if (synth)
	{
		synth->midi_message(0, (byte3 << 16) | (byte2 << 8) | byte1);
	}
}

VstInt32 FMMidi::startProcess ()
{
	lock.acquire();
	if (buffer)
	{
		memset(buffer, 0, (2*blockSize)*sizeof(short));
		lock.release();
		return 1;
	}
	lock.release();
	return 0;
}

VstInt32 FMMidi::stopProcess ()
{
	lock.acquire();
	if (buffer)
	{
		memset(buffer, 0, (2*blockSize)*sizeof(short));
		lock.release();
		return 1;
	}
	lock.release();
	return 0;
}
