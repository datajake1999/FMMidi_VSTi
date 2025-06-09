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

void FMMidi::initializeSettings (bool resetSynth)
{
	lock.acquire();
	vst_strncpy (ProgramName, "Default", kVstMaxProgNameLen-1);
	Volume = 1;
	VolumeDisplay = 0;
	Transpose = 0;
	PushMidi = 1;
	bypassed = false;
	for (VstInt32 i = 0; i < 16; i++)
	{
		ChannelEnabled[i] = true;
	}
	if (resetSynth)
	{
		suspend ();
	}
	lock.release();
}

bool FMMidi::getBypass ()
{
	return bypassed;
}

void FMMidi::enableChannel (VstInt32 channel, bool enable)
{
	lock.acquire();
	channel = channel & 0x0f;
	if (ChannelEnabled[channel] && !enable)
	{
		char data[3];
		data[0] = 0xb0 + (char)channel;
		data[1] = 0x40;
		data[2] = 0;
		sendMidi (data);
		data[1] = 0x7b;
		sendMidi (data);
		data[1] = 0x79;
		sendMidi (data);
		data[0] = 0xe0 + (char)channel;
		data[1] = 0;
		data[2] = 0x40;
		sendMidi (data);
	}
	ChannelEnabled[channel] = enable;
	lock.release();
}

bool FMMidi::isChannelEnabled (VstInt32 channel)
{
	channel = channel & 0x0f;
	return ChannelEnabled[channel];
}

void FMMidi::hardReset ()
{
	lock.acquire();
	clearSynth ();
	clearBuffer ();
	initSynth ();
	initBuffer ();
	lock.release();
}

VstInt32 FMMidi::getActiveVoices ()
{
	return 0;
}

HostInfo* FMMidi::getHostInfo ()
{
	return &hi;
}

double FMMidi::getCPULoad ()
{
	return CPULoad;
}
