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

FMMidi::FMMidi (audioMasterCallback audioMaster)
: AudioEffectX (audioMaster, kNumPrograms, kNumParams)
{
	setNumInputs (2);
	setNumOutputs (2);
	setUniqueID ('FMMV');
	setInitialDelay (0);
	canProcessReplacing ();
#if VST_2_4_EXTENSIONS
	canDoubleReplacing ();
#endif
	programsAreChunks ();
	isSynth ();
#if !VST_FORCE_DEPRECATED
	hasVu ();
	hasClip ();
	canMono ();
#endif
	synth = NULL;
	buffer = NULL;
	initializeSettings (false);
	memset(&chunk, 0, sizeof(chunk));
	memset(&hi, 0, sizeof(hi));
	memset(vu, 0, sizeof(vu));
	CPULoad = 0;
	memset(channelPrograms, 0, sizeof(channelPrograms));
	initSynth ();
	initBuffer ();
#ifdef DEMO
	startTime = time(NULL);
	srand(startTime);
#endif
}

FMMidi::~FMMidi ()
{
	clearSynth ();
	clearBuffer ();
}

void FMMidi::open ()
{
	getHostVendorString (hi.VendorString);
	getHostProductString (hi.ProductString);
	hi.VendorVersion = getHostVendorVersion ();
	hi.MasterVersion = getMasterVersion ();
	if (canHostDo ("receiveVstEvents") || canHostDo ("receiveVstMidiEvent"))
	{
		hi.ReceiveEvents = true;
	}
	else
	{
		hi.ReceiveEvents = false;
	}
}

void FMMidi::close ()
{
	memset(&hi, 0, sizeof(hi));
}

void FMMidi::setParameter (VstInt32 index, float value)
{
	lock.acquire();
	switch (index)
	{
	case kVolume:
		Volume = value;
		break;
	case kVolumeDisplay:
		VolumeDisplay = value;
		break;
	case kTranspose:
		Transpose = (value*24.0f)-12.0f;
		if (Transpose > 12)
		{
			Transpose = 12;
		}
		else if (Transpose < -12)
		{
			Transpose = -12;
		}
		suspend ();
		break;
	case kPushMidi:
		PushMidi = value;
		suspend ();
		break;
	}
	lock.release();
}

float FMMidi::getParameter (VstInt32 index)
{
	float value = 0;
	switch (index)
	{
	case kVolume:
		value = Volume;
		break;
	case kVolumeDisplay:
		value = VolumeDisplay;
		break;
	case kTranspose:
		value = (Transpose+12.0f)/24.0f;
		break;
	case kPushMidi:
		value = PushMidi;
		break;
	}
	return value;
}

void FMMidi::getParameterDisplay (VstInt32 index, char* text)
{
	if (!text)
	{
		return;
	}
	switch (index)
	{
	case kVolume:
		if (VolumeDisplay >= 0.5)
		{
			float2string (Volume*100, text, (kVstMaxParamStrLen*2)-1);
		}
		else
		{
			dB2string (Volume, text, (kVstMaxParamStrLen*2)-1);
		}
		break;
	case kVolumeDisplay:
		if (VolumeDisplay >= 0.5)
		{
			vst_strncpy (text, "%", (kVstMaxParamStrLen*2)-1);
		}
		else
		{
			vst_strncpy (text, "dB", (kVstMaxParamStrLen*2)-1);
		}
		break;
	case kTranspose:
		if (Transpose >= 1 || Transpose <= -1)
		{
			int2string ((VstInt32)Transpose, text, (kVstMaxParamStrLen*2)-1);
		}
		else
		{
			vst_strncpy (text, "0", (kVstMaxParamStrLen*2)-1);
		}
		break;
	case kPushMidi:
		if (PushMidi >= 0.5)
		{
			vst_strncpy (text, "ON", (kVstMaxParamStrLen*2)-1);
		}
		else
		{
			vst_strncpy (text, "OFF", (kVstMaxParamStrLen*2)-1);
		}
		break;
	}
}

void FMMidi::getParameterLabel (VstInt32 index, char* label)
{
	if (!label)
	{
		return;
	}
	switch (index)
	{
	case kVolume:
		if (VolumeDisplay >= 0.5)
		{
			vst_strncpy (label, "%", (kVstMaxParamStrLen*2)-1);
		}
		else
		{
			vst_strncpy (label, "dB", (kVstMaxParamStrLen*2)-1);
		}
		break;
	case kTranspose:
		if (floor(Transpose) == 1 || ceil(Transpose) == -1)
		{
			vst_strncpy (label, "Semitone", (kVstMaxParamStrLen*2)-1);
		}
		else
		{
			vst_strncpy (label, "Semitones", (kVstMaxParamStrLen*2)-1);
		}
		break;
	}
}

void FMMidi::getParameterName (VstInt32 index, char* name)
{
	if (!name)
	{
		return;
	}
	switch (index)
	{
	case kVolume:
		vst_strncpy (name, "Volume", (kVstMaxParamStrLen*2)-1);
		break;
	case kVolumeDisplay:
		vst_strncpy (name, "VolumeDisplay", (kVstMaxParamStrLen*2)-1);
		break;
	case kTranspose:
		vst_strncpy (name, "Transpose", (kVstMaxParamStrLen*2)-1);
		break;
	case kPushMidi:
		vst_strncpy (name, "PushMidi", (kVstMaxParamStrLen*2)-1);
		break;
	}
}

VstInt32 FMMidi::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	if (!data)
	{
		return 0;
	}
	FMMidiChunk* chunkData = (FMMidiChunk*)data;
	VstInt32 i;
	if (chunkData->Size != sizeof(FMMidiChunk))
	{
		return 0;
	}
	setProgramName (chunkData->ProgramName);
	for (i = 0; i < kNumParams; i++)
	{
		setParameter (i, chunkData->Parameters[i]);
	}
	setBypass (chunkData->bypassed);
	for (i = 0; i < 16; i++)
	{
		enableChannel (i, chunkData->ChannelEnabled[i]);
	}
	setFreezeMeters (chunkData->FreezeMeters);
	setHideParameters (chunkData->HideParameters);
	return byteSize;
}

VstInt32 FMMidi::getChunk (void** data, bool isPreset)
{
	if (!data)
	{
		return 0;
	}
	VstInt32 i;
	chunk.Size = sizeof(FMMidiChunk);
	getProgramName (chunk.ProgramName);
	for (i = 0; i < kNumParams; i++)
	{
		chunk.Parameters[i] = getParameter (i);
	}
	chunk.bypassed = bypassed;
	for (i = 0; i < 16; i++)
	{
		chunk.ChannelEnabled[i] = ChannelEnabled[i];
	}
	chunk.FreezeMeters = FreezeMeters;
	chunk.HideParameters = HideParameters;
	*data = &chunk;
	return sizeof(FMMidiChunk);
}

void FMMidi::setProgram (VstInt32 program)
{
	if (program >= kNumPrograms || program < 0)
	{
		return;
	}
	lock.acquire();
	curProgram = program;
	lock.release();
}

VstInt32 FMMidi::getProgram ()
{
	return curProgram%kNumPrograms;
}

void FMMidi::setProgramName (char* name)
{
	if (!name)
	{
		return;
	}
	lock.acquire();
	vst_strncpy (ProgramName, name, kVstMaxProgNameLen-1);
	lock.release();
}

void FMMidi::getProgramName (char* name)
{
	if (!name)
	{
		return;
	}
	vst_strncpy (name, ProgramName, kVstMaxProgNameLen-1);
}

bool FMMidi::string2parameter (VstInt32 index, char* text)
{
	if (!text)
	{
		return false;
	}
	float value = (float)atof(text);
	switch (index)
	{
	case kVolume:
		if (VolumeDisplay >= 0.5)
		{
			value = value/100.0f;
		}
		else
		{
			value = (float)pow(10.0, value/20.0);
		}
		break;
	case kTranspose:
		value = (value+12.0f)/24.0f;
		break;
	}
	if (value > 1)
	{
		value = 1;
	}
	else if (value < 0)
	{
		value = 0;
	}
	setParameter (index, value);
	return true;
}

bool FMMidi::getParameterProperties (VstInt32 index, VstParameterProperties* p)
{
	if (!p)
	{
		return false;
	}
	getParameterName (index, p->label);
	memset(p->shortLabel, 0, kVstMaxShortLabelLen);
	p->flags = kVstParameterUsesFloatStep | kVstParameterSupportsDisplayIndex;
	p->stepFloat = 0.01f;
	p->smallStepFloat = 0.001f;
	p->largeStepFloat = 0.1f;
	p->displayIndex = (VstInt16)index;
	switch (index)
	{
	case kVolumeDisplay:
		p->flags |= kVstParameterIsSwitch;
		break;
	case kTranspose:
		p->flags |= (kVstParameterUsesIntegerMinMax | kVstParameterUsesIntStep);
		p->minInteger = -12;
		p->maxInteger = 12;
		p->stepInteger = 1;
		p->largeStepInteger = 2;
		break;
	case kPushMidi:
		p->flags |= kVstParameterIsSwitch;
		break;
	}
	return true;
}

bool FMMidi::getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text)
{
	if (index == 0 && text)
	{
		vst_strncpy (text, ProgramName, kVstMaxProgNameLen-1);
		return true;
	}
	return false;
}

bool FMMidi::getInputProperties (VstInt32 index, VstPinProperties* properties)
{
	if (!properties)
	{
		return false;
	}
	switch (cEffect.numInputs)
	{
	case 1:
		if (index == 0)
		{
			vst_strncpy (properties->label, "Mono Input", kVstMaxLabelLen-1);
			vst_strncpy (properties->shortLabel, "InM", kVstMaxShortLabelLen-1);
			properties->flags = kVstPinIsActive;
			return true;
		}
		break;
	case 2:
		if (index == 0)
		{
			vst_strncpy (properties->label, "Left Input", kVstMaxLabelLen-1);
			vst_strncpy (properties->shortLabel, "InL", kVstMaxShortLabelLen-1);
			properties->flags = kVstPinIsActive | kVstPinIsStereo;
			return true;
		}
		else if (index == 1)
		{
			vst_strncpy (properties->label, "Right Input", kVstMaxLabelLen-1);
			vst_strncpy (properties->shortLabel, "InR", kVstMaxShortLabelLen-1);
			properties->flags = kVstPinIsActive | kVstPinIsStereo;
			return true;
		}
		break;
	}
	return false;
}

bool FMMidi::getOutputProperties (VstInt32 index, VstPinProperties* properties)
{
	if (!properties)
	{
		return false;
	}
	switch (cEffect.numOutputs)
	{
	case 1:
		if (index == 0)
		{
			vst_strncpy (properties->label, "Mono Output", kVstMaxLabelLen-1);
			vst_strncpy (properties->shortLabel, "OutM", kVstMaxShortLabelLen-1);
			properties->flags = kVstPinIsActive;
			return true;
		}
		break;
	case 2:
		if (index == 0)
		{
			vst_strncpy (properties->label, "Left Output", kVstMaxLabelLen-1);
			vst_strncpy (properties->shortLabel, "OutL", kVstMaxShortLabelLen-1);
			properties->flags = kVstPinIsActive | kVstPinIsStereo;
			return true;
		}
		else if (index == 1)
		{
			vst_strncpy (properties->label, "Right Output", kVstMaxLabelLen-1);
			vst_strncpy (properties->shortLabel, "OutR", kVstMaxShortLabelLen-1);
			properties->flags = kVstPinIsActive | kVstPinIsStereo;
			return true;
		}
		break;
	}
	return false;
}

bool FMMidi::setBypass (bool onOff)
{
	lock.acquire();
	bypassed = onOff;
	if (bypassed)
	{
		suspend ();
		stopProcess ();
	}
	else
	{
		resume ();
		startProcess ();
	}
	lock.release();
	return true;
}

bool FMMidi::getEffectName (char* name)
{
	if (!name)
	{
		return false;
	}
	vst_strncpy (name, "FMMidi", kVstMaxEffectNameLen-1);
	return true;
}

bool FMMidi::getVendorString (char* text)
{
	if (!text)
	{
		return false;
	}
	vst_strncpy (text, "Datajake", kVstMaxVendorStrLen-1);
	return true;
}

bool FMMidi::getProductString (char* text)
{
	if (!text)
	{
		return false;
	}
	vst_strncpy (text, "FMMidi VSTi", kVstMaxProductStrLen-1);
#ifdef DEMO
	vst_strncat (text, " Demo", kVstMaxProductStrLen-1);
#endif
	return true;
}

VstInt32 FMMidi::getVendorVersion ()
{ 
	return 1000; 
}

VstPlugCategory FMMidi::getPlugCategory ()
{
	return kPlugCategSynth;
}

VstIntPtr FMMidi::vendorSpecific (VstInt32 lArg, VstIntPtr lArg2, void* ptrArg, float floatArg)
{
	switch (lArg)
	{
#if REAPER_EXTENSIONS
	case effGetParamDisplay:
		if (lArg2 >= 0 && lArg2 < kNumParams)
		{
			if (ptrArg)
			{
				if (getParameterDisplayValue ((VstInt32)lArg2, (char*)ptrArg, floatArg))
				{
					return 0xbeef;
				}
			}
		}
		break;
	case effString2Parameter:
		if (lArg2 >= 0 && lArg2 < kNumParams)
		{
			if (ptrArg)
			{
				if (string2parameterReplace ((VstInt32)lArg2, (char*)ptrArg))
				{
					return 0xbeef;
				}
			}
		}
		break;
	case kVstParameterUsesIntStep:
		if (lArg2 >= 0 && lArg2 < kNumParams)
		{
			if (isEnumParameter ((VstInt32)lArg2))
			{
				return 0xbeef;
			}
		}
		break;
	case effCanBeAutomated:
		if (lArg2 >= 0 && lArg2 < kNumParams)
		{
			if (automateParameter ((VstInt32)lArg2, floatArg, (VstInt32)ptrArg))
			{
				return 0xbeef;
			}
		}
		break;
	case 0xdeadbef0:
		if (lArg2 >= 0 && lArg2 < kNumParams)
		{
			if (ptrArg)
			{
				if (parameterRange ((VstInt32)lArg2, (double*)ptrArg))
				{
					return 0xbeef;
				}
			}
		}
		break;
	case effGetChunk:
		if (lArg2 && ptrArg)
		{
			vst_strncpy ((char*)ptrArg, (char*)lArg2, (VstInt32)floatArg-1);
			return 0xf00d;
		}
		break;
	case effSetChunk:
		if (lArg2 && ptrArg)
		{
			return 0xf00d;
		}
		break;
	case effGetEffectName:
		if (lArg2 == 0x50 && ptrArg)
		{
			if (renamePlug ((char**)ptrArg, "FMMidi"))
			{
				return 0xf00d;
			}
		}
		break;
#endif
	}
	return 0;
}

VstInt32 FMMidi::canDo (char* text)
{
	if (!text)
	return 0;
	if (!strcmp (text, "sendVstEvents"))
	return 1;
	if (!strcmp (text, "sendVstMidiEvent"))
	return 1;
	if (!strcmp (text, "receiveVstEvents"))
	return 1;
	if (!strcmp (text, "receiveVstMidiEvent"))
	return 1;
	if (!strcmp (text, "midiProgramNames"))
	return 1;
	if (!strcmp (text, "bypass"))
	return 1;
#if REAPER_EXTENSIONS
	if (!strcmp (text, "hasCockosExtensions"))
	return 0xbeef0000;
	if (!strcmp (text, "hasCockosSampleAccurateAutomation"))
	return 0xbeef0000;
#endif
	return -1;	// explicitly can't do; 0 => don't know
}

#if VST_2_4_EXTENSIONS
VstInt32 FMMidi::getNumMidiInputChannels ()
{
	return 16;
}

VstInt32 FMMidi::getNumMidiOutputChannels ()
{
	if (hi.ReceiveEvents)
	{
		return 16;
	}
	return 0;
}
#endif
