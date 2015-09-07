/*  			DirectSound
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Rob Riggs
 * Copyright 2000-2002 TransGaming Technologies, Inc.
 * Copyright 2007 Peter Dons Tychsen
 * Copyright 2007 Maarten Lankhorst
 * Copyright 2011 Owen Rudge for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <assert.h>
#include <stdarg.h>
#include <math.h>	/* Insomnia - pow() function */

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "mmsystem.h"
#include "wingdi.h"
#include "mmreg.h"
#include "winternl.h"
#include "wine/debug.h"
#include "dsound.h"
#include "ks.h"
#include "ksmedia.h"
#include "dsound_private.h"
#include "fir.h"

WINE_DEFAULT_DEBUG_CHANNEL(dsound);

void DSOUND_RecalcVolPan(PDSVOLUMEPAN volpan)
{
	double temp;
	TRACE("(%p)\n",volpan);

	TRACE("Vol=%d Pan=%d\n", volpan->lVolume, volpan->lPan);
	/* the AmpFactors are expressed in 16.16 fixed point */

	/* FIXME: use calculated vol and pan ampfactors */
	temp = (double) (volpan->lVolume - (volpan->lPan > 0 ? volpan->lPan : 0));
	volpan->dwTotalAmpFactor[0] = (ULONG) (pow(2.0, temp / 600.0) * 0xffff);
	temp = (double) (volpan->lVolume + (volpan->lPan < 0 ? volpan->lPan : 0));
	volpan->dwTotalAmpFactor[1] = (ULONG) (pow(2.0, temp / 600.0) * 0xffff);

	TRACE("left = %x, right = %x\n", volpan->dwTotalAmpFactor[0], volpan->dwTotalAmpFactor[1]);
}

void DSOUND_AmpFactorToVolPan(PDSVOLUMEPAN volpan)
{
    double left,right;
    TRACE("(%p)\n",volpan);

    TRACE("left=%x, right=%x\n",volpan->dwTotalAmpFactor[0],volpan->dwTotalAmpFactor[1]);
    if (volpan->dwTotalAmpFactor[0]==0)
        left=-10000;
    else
        left=600 * log(((double)volpan->dwTotalAmpFactor[0]) / 0xffff) / log(2);
    if (volpan->dwTotalAmpFactor[1]==0)
        right=-10000;
    else
        right=600 * log(((double)volpan->dwTotalAmpFactor[1]) / 0xffff) / log(2);
    if (left<right)
        volpan->lVolume=right;
    else
        volpan->lVolume=left;
    if (volpan->lVolume < -10000)
        volpan->lVolume=-10000;
    volpan->lPan=right-left;
    if (volpan->lPan < -10000)
        volpan->lPan=-10000;

    TRACE("Vol=%d Pan=%d\n", volpan->lVolume, volpan->lPan);
}

/**
 * Recalculate the size for temporary buffer, and new writelead
 * Should be called when one of the following things occur:
 * - Primary buffer format is changed
 * - This buffer format (frequency) is changed
 */
void DSOUND_RecalcFormat(IDirectSoundBufferImpl *dsb)
{
	DWORD ichannels = dsb->pwfx->nChannels;
	DWORD ochannels = dsb->device->pwfx->nChannels;
	WAVEFORMATEXTENSIBLE *pwfxe;
	BOOL ieee = FALSE;

	TRACE("(%p)\n",dsb);

	pwfxe = (WAVEFORMATEXTENSIBLE *) dsb->pwfx;
	dsb->freqAdjustNum = dsb->freq;
	dsb->freqAdjustDen = dsb->device->pwfx->nSamplesPerSec;

	if ((pwfxe->Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) || ((pwfxe->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	    && (IsEqualGUID(&pwfxe->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))))
		ieee = TRUE;

	/**
	 * Recalculate FIR step and gain.
	 *
	 * firstep says how many points of the FIR exist per one
	 * sample in the secondary buffer. firgain specifies what
	 * to multiply the FIR output by in order to attenuate it correctly.
	 */
	if (dsb->freqAdjustNum / dsb->freqAdjustDen > 0) {
		/**
		 * Yes, round it a bit to make sure that the
		 * linear interpolation factor never changes.
		 */
		dsb->firstep = fir_step * dsb->freqAdjustDen / dsb->freqAdjustNum;
	} else {
		dsb->firstep = fir_step;
	}
	dsb->firgain = (float)dsb->firstep / fir_step;

	/* calculate the 10ms write lead */
	dsb->writelead = (dsb->freq / 100) * dsb->pwfx->nBlockAlign;

	dsb->freqAccNum = 0;

	dsb->get_aux = ieee ? getbpp[4] : getbpp[dsb->pwfx->wBitsPerSample/8 - 1];
	dsb->put_aux = putieee32;

	dsb->get = dsb->get_aux;
	dsb->put = dsb->put_aux;

	if (ichannels == ochannels)
	{
		dsb->mix_channels = ichannels;
		if (ichannels > 32) {
			FIXME("Copying %u channels is unsupported, limiting to first 32\n", ichannels);
			dsb->mix_channels = 32;
		}
	}
	else if (ichannels == 1)
	{
		dsb->mix_channels = 1;

		if (ochannels == 2)
			dsb->put = put_mono2stereo;
		else if (ochannels == 4)
			dsb->put = put_mono2quad;
		else if (ochannels == 6)
			dsb->put = put_mono2surround51;
	}
	else if (ochannels == 1)
	{
		dsb->mix_channels = 1;
		dsb->get = get_mono;
	}
	else if (ichannels == 2 && ochannels == 4)
	{
		dsb->mix_channels = 2;
		dsb->put = put_stereo2quad;
	}
	else if (ichannels == 2 && ochannels == 6)
	{
		dsb->mix_channels = 2;
		dsb->put = put_stereo2surround51;
	}
	else
	{
		if (ichannels > 2)
			FIXME("Conversion from %u to %u channels is not implemented, falling back to stereo\n", ichannels, ochannels);
		dsb->mix_channels = 2;
	}
}

/**
 * Check for application callback requests for when the play position
 * reaches certain points.
 *
 * The offsets that will be triggered will be those between the recorded
 * "last played" position for the buffer (i.e. dsb->playpos) and "len" bytes
 * beyond that position.
 */
void DSOUND_CheckEvent(const IDirectSoundBufferImpl *dsb, DWORD playpos, int len)
{
    int first, left, right, check;

    if(dsb->nrofnotifies == 0)
        return;

    if(dsb->state == STATE_STOPPED){
        TRACE("Stopped...\n");
        /* DSBPN_OFFSETSTOP notifies are always at the start of the sorted array */
        for(left = 0; left < dsb->nrofnotifies; ++left){
            if(dsb->notifies[left].dwOffset != DSBPN_OFFSETSTOP)
                break;

            TRACE("Signalling %p\n", dsb->notifies[left].hEventNotify);
            SetEvent(dsb->notifies[left].hEventNotify);
        }
        return;
    }

    for(first = 0; first < dsb->nrofnotifies && dsb->notifies[first].dwOffset == DSBPN_OFFSETSTOP; ++first)
        ;

    if(first == dsb->nrofnotifies)
        return;

    check = left = first;
    right = dsb->nrofnotifies - 1;

    /* find leftmost notify that is greater than playpos */
    while(left != right){
        check = left + (right - left) / 2;
        if(dsb->notifies[check].dwOffset < playpos)
            left = check + 1;
        else if(dsb->notifies[check].dwOffset > playpos)
            right = check;
        else{
            left = check;
            break;
        }
    }

    TRACE("Not stopped: first notify: %u (%u), left notify: %u (%u), range: [%u,%u)\n",
            first, dsb->notifies[first].dwOffset,
            left, dsb->notifies[left].dwOffset,
            playpos, (playpos + len) % dsb->buflen);

    /* send notifications in range */
    if(dsb->notifies[left].dwOffset >= playpos){
        for(check = left; check < dsb->nrofnotifies; ++check){
            if(dsb->notifies[check].dwOffset >= playpos + len)
                break;

            TRACE("Signalling %p (%u)\n", dsb->notifies[check].hEventNotify, dsb->notifies[check].dwOffset);
            SetEvent(dsb->notifies[check].hEventNotify);
        }
    }

    if(playpos + len > dsb->buflen){
        for(check = first; check < left; ++check){
            if(dsb->notifies[check].dwOffset >= (playpos + len) % dsb->buflen)
                break;

            TRACE("Signalling %p (%u)\n", dsb->notifies[check].hEventNotify, dsb->notifies[check].dwOffset);
            SetEvent(dsb->notifies[check].hEventNotify);
        }
    }
}

static inline float get_current_sample(const IDirectSoundBufferImpl *dsb,
        DWORD mixpos, DWORD channel)
{
    if (mixpos >= dsb->buflen && !(dsb->playflags & DSBPLAY_LOOPING))
        return 0.0f;
    return dsb->get(dsb, mixpos % dsb->buflen, channel);
}

static UINT cp_fields_noresample(IDirectSoundBufferImpl *dsb, bitsputfunc put, UINT ostride, UINT count)
{
    UINT istride = dsb->pwfx->nBlockAlign;
    DWORD channel, i;
    for (i = 0; i < count; i++)
        for (channel = 0; channel < dsb->mix_channels; channel++)
            put(dsb, i * ostride, channel, get_current_sample(dsb,
                dsb->sec_mixpos + i * istride, channel));
    return count;
}

static UINT cp_fields_resample_lq(IDirectSoundBufferImpl *dsb, bitsputfunc put,
                                  UINT ostride, UINT count, LONG64 *freqAccNum)
{
    UINT i, channel;
    UINT istride = dsb->pwfx->nBlockAlign;
    UINT channels = dsb->mix_channels;

    LONG64 freqAcc_start = *freqAccNum;
    LONG64 freqAcc_end = freqAcc_start + count * dsb->freqAdjustNum;
    UINT max_ipos = freqAcc_end / dsb->freqAdjustDen;

    for (i = 0; i < count; ++i) {
        float cur_freqAcc = (freqAcc_start + i * dsb->freqAdjustNum) / (float)dsb->freqAdjustDen;
        float cur_freqAcc2;
        UINT ipos = cur_freqAcc;
        UINT idx = dsb->sec_mixpos + ipos * istride;
        cur_freqAcc -= (int)cur_freqAcc;
        cur_freqAcc2 = 1.0f - cur_freqAcc;
        for (channel = 0; channel < channels; channel++) {
            /**
             * Generally we cannot cache the result of get_current_sample().
             * Consider the case of resampling from 192000 Hz to 44100 Hz -
             * none of the values will get reused for the next value of i.
             * OTOH, for resampling from 44100 Hz to 192000 Hz both values
             * will likely be reused.
             *
             * So far, this possibility of saving calls to
             * get_current_sample() is ignored.
             */
            float s1 = get_current_sample(dsb, idx, channel);
            float s2 = get_current_sample(dsb, idx + istride, channel);
            float result = s1 * cur_freqAcc2 + s2 * cur_freqAcc;
            put(dsb, i * ostride, channel, result);
        }
    }

    *freqAccNum = freqAcc_end % dsb->freqAdjustDen;
    return max_ipos;
}

static UINT cp_fields_resample_hq(IDirectSoundBufferImpl *dsb, bitsputfunc put,
                                  UINT ostride, UINT count, LONG64 *freqAccNum)
{
    UINT i, channel;
    UINT istride = dsb->pwfx->nBlockAlign;

    LONG64 freqAcc_start = *freqAccNum;
    LONG64 freqAcc_end = freqAcc_start + count * dsb->freqAdjustNum;
    UINT dsbfirstep = dsb->firstep;
    UINT channels = dsb->mix_channels;
    UINT max_ipos = (freqAcc_start + count * dsb->freqAdjustNum) / dsb->freqAdjustDen;

    UINT fir_cachesize = (fir_len + dsbfirstep - 2) / dsbfirstep;
    UINT required_input = max_ipos + fir_cachesize;

    float* intermediate = HeapAlloc(GetProcessHeap(), 0,
            sizeof(float) * required_input * channels);

    float* fir_copy = HeapAlloc(GetProcessHeap(), 0,
            sizeof(float) * fir_cachesize);

    /* Important: this buffer MUST be non-interleaved
     * if you want -msse3 to have any effect.
     * This is good for CPU cache effects, too.
     */
    float* itmp = intermediate;
    for (channel = 0; channel < channels; channel++)
        for (i = 0; i < required_input; i++)
            *(itmp++) = get_current_sample(dsb,
                    dsb->sec_mixpos + i * istride, channel);

    for(i = 0; i < count; ++i) {
        UINT int_fir_steps = (freqAcc_start + i * dsb->freqAdjustNum) * dsbfirstep / dsb->freqAdjustDen;
        float total_fir_steps = (freqAcc_start + i * dsb->freqAdjustNum) * dsbfirstep / (float)dsb->freqAdjustDen;
        UINT ipos = int_fir_steps / dsbfirstep;

        UINT idx = (ipos + 1) * dsbfirstep - int_fir_steps - 1;
        float rem = int_fir_steps + 1.0 - total_fir_steps;

        int fir_used = 0;
        while (idx < fir_len - 1) {
            fir_copy[fir_used++] = fir[idx] * (1.0 - rem) + fir[idx + 1] * rem;
            idx += dsb->firstep;
        }

        assert(fir_used <= fir_cachesize);
        assert(ipos + fir_used <= required_input);

        for (channel = 0; channel < dsb->mix_channels; channel++) {
            int j;
            float sum = 0.0;
            float* cache = &intermediate[channel * required_input + ipos];
            for (j = 0; j < fir_used; j++)
                sum += fir_copy[j] * cache[j];
            put(dsb, i * ostride, channel, sum * dsb->firgain);
        }
    }

    *freqAccNum = freqAcc_end % dsb->freqAdjustDen;

    HeapFree(GetProcessHeap(), 0, fir_copy);
    HeapFree(GetProcessHeap(), 0, intermediate);

    return max_ipos;
}

static void cp_fields(IDirectSoundBufferImpl *dsb, bitsputfunc put,
                      UINT ostride, UINT count, LONG64 *freqAccNum)
{
    DWORD ipos, adv;

    if (dsb->freqAdjustNum == dsb->freqAdjustDen)
        adv = cp_fields_noresample(dsb, put, ostride, count); /* *freqAcc is unmodified */
    else if (dsb->device->nrofbuffers > ds_hq_buffers_max)
        adv = cp_fields_resample_lq(dsb, put, ostride, count, freqAccNum);
    else
        adv = cp_fields_resample_hq(dsb, put, ostride, count, freqAccNum);

    ipos = dsb->sec_mixpos + adv * dsb->pwfx->nBlockAlign;
    if (ipos >= dsb->buflen) {
        if (dsb->playflags & DSBPLAY_LOOPING)
            ipos %= dsb->buflen;
        else {
            ipos = 0;
            dsb->state = STATE_STOPPED;
        }
    }

    dsb->sec_mixpos = ipos;
}

/**
 * Calculate the distance between two buffer offsets, taking wraparound
 * into account.
 */
static inline DWORD DSOUND_BufPtrDiff(DWORD buflen, DWORD ptr1, DWORD ptr2)
{
/* If these asserts fail, the problem is not here, but in the underlying code */
	assert(ptr1 < buflen);
	assert(ptr2 < buflen);
	if (ptr1 >= ptr2) {
		return ptr1 - ptr2;
	} else {
		return buflen + ptr1 - ptr2;
	}
}

static float getieee32_dsp(const IDirectSoundBufferImpl *dsb, DWORD pos, DWORD channel)
{
    const BYTE *buf = (BYTE *)dsb->device->dsp_buffer;
    const float *fbuf = (const float*)(buf + pos + sizeof(float) * channel);
    return *fbuf;
}

static void putieee32_dsp(const IDirectSoundBufferImpl *dsb, DWORD pos, DWORD channel, float value)
{
    BYTE *buf = (BYTE *)dsb->device->dsp_buffer;
    float *fbuf = (float*)(buf + pos + sizeof(float) * channel);
    *fbuf = value;
}

/**
 * Mix at most the given amount of data into the allocated temporary buffer
 * of the given secondary buffer, starting from the dsb's first currently
 * unsampled frame (writepos), translating frequency (pitch), stereo/mono
 * and bits-per-sample so that it is ideal for the primary buffer.
 * Doesn't perform any mixing - this is a straight copy/convert operation.
 *
 * dsb = the secondary buffer
 * writepos = Starting position of changed buffer
 * len = number of bytes to resample from writepos
 *
 * NOTE: writepos + len <= buflen. When called by mixer, MixOne makes sure of this.
 */
static void DSOUND_MixToTemporary(IDirectSoundBufferImpl *dsb, DWORD frames)
{
    BOOL using_filters = dsb->num_filters > 0 || dsb->device->eax.using_eax;
    UINT istride, ostride, size_bytes;
    DWORD channel, i;
    bitsputfunc put;
	HRESULT hr;

    put = dsb->put;
    ostride = dsb->device->pwfx->nChannels * sizeof(float);
    size_bytes = frames * ostride;

    if (dsb->device->tmp_buffer_len < size_bytes || !dsb->device->tmp_buffer) {
		if (dsb->device->tmp_buffer)
			dsb->device->tmp_buffer = HeapReAlloc(GetProcessHeap(), 0, dsb->device->tmp_buffer, size_bytes);
		else
			dsb->device->tmp_buffer = HeapAlloc(GetProcessHeap(), 0, size_bytes);
        dsb->device->tmp_buffer_len = size_bytes;
	}

    if (using_filters) {
        put = putieee32_dsp;
        ostride = dsb->mix_channels * sizeof(float);
        size_bytes = frames * ostride;

        if (dsb->device->dsp_buffer_len < size_bytes || !dsb->device->dsp_buffer) {
            if (dsb->device->dsp_buffer)
                dsb->device->dsp_buffer = HeapReAlloc(GetProcessHeap(), 0, dsb->device->dsp_buffer, size_bytes);
            else
                dsb->device->dsp_buffer = HeapAlloc(GetProcessHeap(), 0, size_bytes);
            dsb->device->dsp_buffer_len = size_bytes;
        }
    }

    cp_fields(dsb, put, ostride, frames, &dsb->freqAccNum);

    if (using_filters) {
        if (frames > 0) {
            for (i = 0; i < dsb->num_filters; i++) {
                if (dsb->filters[i].inplace) {
                    hr = IMediaObjectInPlace_Process(dsb->filters[i].inplace, frames * sizeof(float) * dsb->mix_channels,
                                                     (BYTE *)dsb->device->dsp_buffer, 0, DMO_INPLACE_NORMAL);
                    if (FAILED(hr))
                        WARN("IMediaObjectInPlace_Process failed for filter %u\n", i);
                } else
                    WARN("filter %u has no inplace object - unsupported\n", i);
            }
        }

        if (dsb->device->eax.using_eax)
            process_eax_buffer(dsb, dsb->device->dsp_buffer, frames * dsb->mix_channels);

        istride = ostride;
        ostride = dsb->device->pwfx->nChannels * sizeof(float);
        for (i = 0; i < frames; i++) {
            for (channel = 0; channel < dsb->mix_channels; channel++) {
                dsb->put(dsb, i * ostride, channel, getieee32_dsp(dsb, i * istride, channel));
            }
        }
    }
}

static void DSOUND_MixerVol(const IDirectSoundBufferImpl *dsb, INT frames)
{
	INT	i;
	float vols[DS_MAX_CHANNELS];
	UINT channels = dsb->device->pwfx->nChannels, chan;

	TRACE("(%p,%d)\n",dsb,frames);
	TRACE("left = %x, right = %x\n", dsb->volpan.dwTotalAmpFactor[0],
		dsb->volpan.dwTotalAmpFactor[1]);

	if ((!(dsb->dsbd.dwFlags & DSBCAPS_CTRLPAN) || (dsb->volpan.lPan == 0)) &&
	    (!(dsb->dsbd.dwFlags & DSBCAPS_CTRLVOLUME) || (dsb->volpan.lVolume == 0)) &&
	     !(dsb->dsbd.dwFlags & DSBCAPS_CTRL3D))
		return; /* Nothing to do */

	if (channels > DS_MAX_CHANNELS)
	{
		FIXME("There is no support for %u channels\n", channels);
		return;
	}

	for (i = 0; i < channels; ++i)
		vols[i] = dsb->volpan.dwTotalAmpFactor[i] / ((float)0xFFFF);

	for(i = 0; i < frames; ++i){
		for(chan = 0; chan < channels; ++chan){
			dsb->device->tmp_buffer[i * channels + chan] *= vols[chan];
		}
	}
}

/**
 * Mix (at most) the given number of bytes into the given position of the
 * device buffer, from the secondary buffer "dsb" (starting at the current
 * mix position for that buffer).
 *
 * Returns the number of bytes actually mixed into the device buffer. This
 * will match fraglen unless the end of the secondary buffer is reached
 * (and it is not looping).
 *
 * dsb  = the secondary buffer to mix from
 * writepos = position (offset) in device buffer to write at
 * fraglen = number of bytes to mix
 */
static DWORD DSOUND_MixInBuffer(IDirectSoundBufferImpl *dsb, DWORD writepos, DWORD fraglen)
{
	INT len = fraglen;
	float *ibuf;
	DWORD oldpos;
	UINT frames = fraglen / dsb->device->pwfx->nBlockAlign;

	TRACE("sec_mixpos=%d/%d\n", dsb->sec_mixpos, dsb->buflen);
	TRACE("(%p,%d,%d)\n",dsb,writepos,fraglen);

	if (len % dsb->device->pwfx->nBlockAlign) {
		INT nBlockAlign = dsb->device->pwfx->nBlockAlign;
		ERR("length not a multiple of block size, len = %d, block size = %d\n", len, nBlockAlign);
		len -= len % nBlockAlign; /* data alignment */
	}

	/* Resample buffer to temporary buffer specifically allocated for this purpose, if needed */
	oldpos = dsb->sec_mixpos;

	DSOUND_MixToTemporary(dsb, frames);
	ibuf = dsb->device->tmp_buffer;

	/* Apply volume if needed */
	DSOUND_MixerVol(dsb, frames);

	mixieee32(ibuf, dsb->device->mix_buffer, frames * dsb->device->pwfx->nChannels);

	/* check for notification positions */
	if (dsb->dsbd.dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY &&
	    dsb->state != STATE_STARTING) {
		INT ilen = DSOUND_BufPtrDiff(dsb->buflen, dsb->sec_mixpos, oldpos);
		DSOUND_CheckEvent(dsb, oldpos, ilen);
	}

	return len;
}

/**
 * Mix some frames from the given secondary buffer "dsb" into the device
 * primary buffer.
 *
 * dsb = the secondary buffer
 * playpos = the current play position in the device buffer (primary buffer)
 * writepos = the current safe-to-write position in the device buffer
 * mixlen = the maximum number of bytes in the primary buffer to mix, from the
 *          current writepos.
 *
 * Returns: the number of bytes beyond the writepos that were mixed.
 */
static DWORD DSOUND_MixOne(IDirectSoundBufferImpl *dsb, DWORD writepos, DWORD mixlen)
{
	DWORD primary_done = 0;

	TRACE("(%p,%d,%d)\n",dsb,writepos,mixlen);
	TRACE("writepos=%d, mixlen=%d\n", writepos, mixlen);
	TRACE("looping=%d, leadin=%d\n", dsb->playflags, dsb->leadin);

	/* If leading in, only mix about 20 ms, and 'skip' mixing the rest, for more fluid pointer advancement */
	/* FIXME: Is this needed? */
	if (dsb->leadin && dsb->state == STATE_STARTING) {
		if (mixlen > 2 * dsb->device->fraglen) {
			primary_done = mixlen - 2 * dsb->device->fraglen;
			mixlen = 2 * dsb->device->fraglen;
			writepos += primary_done;
			dsb->sec_mixpos += (primary_done / dsb->device->pwfx->nBlockAlign) *
				dsb->pwfx->nBlockAlign * dsb->freqAdjustNum / dsb->freqAdjustDen;
		}
	}

	dsb->leadin = FALSE;

	TRACE("mixlen (primary) = %i\n", mixlen);

	/* First try to mix to the end of the buffer if possible
	 * Theoretically it would allow for better optimization
	*/
	primary_done += DSOUND_MixInBuffer(dsb, writepos, mixlen);

	TRACE("total mixed data=%d\n", primary_done);

	/* Report back the total prebuffered amount for this buffer */
	return primary_done;
}

/**
 * For a DirectSoundDevice, go through all the currently playing buffers and
 * mix them in to the device buffer.
 *
 * writepos = the current safe-to-write position in the primary buffer
 * mixlen = the maximum amount to mix into the primary buffer
 *          (beyond the current writepos)
 * recover = true if the sound device may have been reset and the write
 *           position in the device buffer changed
 * all_stopped = reports back if all buffers have stopped
 *
 * Returns:  the length beyond the writepos that was mixed to.
 */

static void DSOUND_MixToPrimary(const DirectSoundDevice *device, DWORD writepos, DWORD mixlen, BOOL recover, BOOL *all_stopped)
{
	INT i;
	IDirectSoundBufferImpl	*dsb;

	/* unless we find a running buffer, all have stopped */
	*all_stopped = TRUE;

	TRACE("(%d,%d,%d)\n", writepos, mixlen, recover);
	for (i = 0; i < device->nrofbuffers; i++) {
		dsb = device->buffers[i];

		TRACE("MixToPrimary for %p, state=%d\n", dsb, dsb->state);

		if (dsb->buflen && dsb->state) {
			TRACE("Checking %p, mixlen=%d\n", dsb, mixlen);
			RtlAcquireResourceShared(&dsb->lock, TRUE);
			/* if buffer is stopping it is stopped now */
			if (dsb->state == STATE_STOPPING) {
				dsb->state = STATE_STOPPED;
				DSOUND_CheckEvent(dsb, 0, 0);
			} else if (dsb->state != STATE_STOPPED) {

				/* if the buffer was starting, it must be playing now */
				if (dsb->state == STATE_STARTING)
					dsb->state = STATE_PLAYING;

				/* mix next buffer into the main buffer */
				DSOUND_MixOne(dsb, writepos, mixlen);

				*all_stopped = FALSE;
			}
			RtlReleaseResource(&dsb->lock);
		}
	}
}

/**
 * Add buffers to the emulated wave device system.
 *
 * device = The current dsound playback device
 * force = If TRUE, the function will buffer up as many frags as possible,
 *         even though and will ignore the actual state of the primary buffer.
 *
 * Returns:  None
 */

static void DSOUND_WaveQueue(DirectSoundDevice *device, BOOL force)
{
	DWORD prebuf_frames, prebuf_bytes, read_offs_bytes;
	BYTE *buffer;
	HRESULT hr;

	TRACE("(%p)\n", device);

	read_offs_bytes = (device->playing_offs_bytes + device->in_mmdev_bytes) % device->buflen;

	TRACE("read_offs_bytes = %u, playing_offs_bytes = %u, in_mmdev_bytes: %u, prebuf = %u\n",
		read_offs_bytes, device->playing_offs_bytes, device->in_mmdev_bytes, device->prebuf);

	if (!force)
	{
		if(device->mixpos < device->playing_offs_bytes)
			prebuf_bytes = device->mixpos + device->buflen - device->playing_offs_bytes;
		else
			prebuf_bytes = device->mixpos - device->playing_offs_bytes;
	}
	else
		/* buffer the maximum amount of frags */
		prebuf_bytes = device->prebuf * device->fraglen;

	/* limit to the queue we have left */
	if(device->in_mmdev_bytes + prebuf_bytes > device->prebuf * device->fraglen)
		prebuf_bytes = device->prebuf * device->fraglen - device->in_mmdev_bytes;

	TRACE("prebuf_bytes = %u\n", prebuf_bytes);

	if(!prebuf_bytes)
		return;

	if(prebuf_bytes + read_offs_bytes > device->buflen){
		DWORD chunk_bytes = device->buflen - read_offs_bytes;
		prebuf_frames = chunk_bytes / device->pwfx->nBlockAlign;
		prebuf_bytes -= chunk_bytes;
	}else{
		prebuf_frames = prebuf_bytes / device->pwfx->nBlockAlign;
		prebuf_bytes = 0;
	}

	hr = IAudioRenderClient_GetBuffer(device->render, prebuf_frames, &buffer);
	if(FAILED(hr)){
		WARN("GetBuffer failed: %08x\n", hr);
		return;
	}

	memcpy(buffer, device->buffer + read_offs_bytes,
			prebuf_frames * device->pwfx->nBlockAlign);

	hr = IAudioRenderClient_ReleaseBuffer(device->render, prebuf_frames, 0);
	if(FAILED(hr)){
		WARN("ReleaseBuffer failed: %08x\n", hr);
		return;
	}

	device->in_mmdev_bytes += prebuf_frames * device->pwfx->nBlockAlign;

	/* check if anything wrapped */
	if(prebuf_bytes > 0){
		prebuf_frames = prebuf_bytes / device->pwfx->nBlockAlign;

		hr = IAudioRenderClient_GetBuffer(device->render, prebuf_frames, &buffer);
		if(FAILED(hr)){
			WARN("GetBuffer failed: %08x\n", hr);
			return;
		}

		memcpy(buffer, device->buffer, prebuf_frames * device->pwfx->nBlockAlign);

		hr = IAudioRenderClient_ReleaseBuffer(device->render, prebuf_frames, 0);
		if(FAILED(hr)){
			WARN("ReleaseBuffer failed: %08x\n", hr);
			return;
		}
		device->in_mmdev_bytes += prebuf_frames * device->pwfx->nBlockAlign;
	}

	TRACE("in_mmdev_bytes now = %i\n", device->in_mmdev_bytes);
}

/**
 * Perform mixing for a Direct Sound device. That is, go through all the
 * secondary buffers (the sound bites currently playing) and mix them in
 * to the primary buffer (the device buffer).
 *
 * The mixing procedure goes:
 *
 * secondary->buffer (secondary format)
 *   =[Resample]=> device->tmp_buffer (float format)
 *   =[Volume]=> device->tmp_buffer (float format)
 *   =[Mix]=> device->mix_buffer (float format)
 *   =[Reformat]=> device->buffer (device format)
 */
static void DSOUND_PerformMix(DirectSoundDevice *device)
{
	UINT32 pad, to_mix_frags, to_mix_bytes;
	HRESULT hr;

	TRACE("(%p)\n", device);

	/* **** */
	EnterCriticalSection(&device->mixlock);

	hr = IAudioClient_GetCurrentPadding(device->client, &pad);
	if(FAILED(hr)){
		WARN("GetCurrentPadding failed: %08x\n", hr);
		LeaveCriticalSection(&device->mixlock);
		return;
	}

	to_mix_frags = device->prebuf - (pad * device->pwfx->nBlockAlign + device->fraglen - 1) / device->fraglen;

	to_mix_bytes = to_mix_frags * device->fraglen;

	if(device->in_mmdev_bytes > 0){
		DWORD delta_bytes = min(to_mix_bytes, device->in_mmdev_bytes);
		device->in_mmdev_bytes -= delta_bytes;
		device->playing_offs_bytes += delta_bytes;
		device->playing_offs_bytes %= device->buflen;
	}

	if (device->priolevel != DSSCL_WRITEPRIMARY) {
		BOOL recover = FALSE, all_stopped = FALSE;
		DWORD playpos, writepos, writelead, maxq, prebuff_max, prebuff_left, size1, size2;
		LPVOID buf1, buf2;
		int nfiller;

		/* the sound of silence */
		nfiller = device->pwfx->wBitsPerSample == 8 ? 128 : 0;

		/* get the position in the primary buffer */
		if (DSOUND_PrimaryGetPosition(device, &playpos, &writepos) != 0){
			LeaveCriticalSection(&(device->mixlock));
			return;
		}

		TRACE("primary playpos=%d, writepos=%d, clrpos=%d, mixpos=%d, buflen=%d\n",
			playpos,writepos,device->playpos,device->mixpos,device->buflen);
		assert(device->playpos < device->buflen);

		/* calc maximum prebuff */
		prebuff_max = (device->prebuf * device->fraglen);

		/* check how close we are to an underrun. It occurs when the writepos overtakes the mixpos */
		prebuff_left = DSOUND_BufPtrDiff(device->buflen, device->mixpos, playpos);
		writelead = DSOUND_BufPtrDiff(device->buflen, writepos, playpos);

		/* check for underrun. underrun occurs when the write position passes the mix position
		 * also wipe out just-played sound data */
		if((prebuff_left > prebuff_max) || (device->state == STATE_STOPPED) || (device->state == STATE_STARTING)){
			if (device->state == STATE_STOPPING || device->state == STATE_PLAYING)
				WARN("Probable buffer underrun\n");
			else TRACE("Buffer starting or buffer underrun\n");

			/* recover mixing for all buffers */
			recover = TRUE;

			/* reset mix position to write position */
			device->mixpos = writepos;

			ZeroMemory(device->buffer, device->buflen);
		} else if (playpos < device->playpos) {
			buf1 = device->buffer + device->playpos;
			buf2 = device->buffer;
			size1 = device->buflen - device->playpos;
			size2 = playpos;
			FillMemory(buf1, size1, nfiller);
			if (playpos && (!buf2 || !size2))
				FIXME("%d: (%d, %d)=>(%d, %d) There should be an additional buffer here!!\n", __LINE__, device->playpos, device->mixpos, playpos, writepos);
			FillMemory(buf2, size2, nfiller);
		} else {
			buf1 = device->buffer + device->playpos;
			buf2 = NULL;
			size1 = playpos - device->playpos;
			size2 = 0;
			FillMemory(buf1, size1, nfiller);
		}
		device->playpos = playpos;

		/* find the maximum we can prebuffer from current write position */
		maxq = (writelead < prebuff_max) ? (prebuff_max - writelead) : 0;

		TRACE("prebuff_left = %d, prebuff_max = %dx%d=%d, writelead=%d\n",
			prebuff_left, device->prebuf, device->fraglen, prebuff_max, writelead);

		ZeroMemory(device->mix_buffer, device->mix_buffer_len);

		/* do the mixing */
		DSOUND_MixToPrimary(device, writepos, maxq, recover, &all_stopped);

		if (maxq + writepos > device->buflen)
		{
			DWORD todo = device->buflen - writepos;
			DWORD offs_float = (todo / device->pwfx->nBlockAlign) * device->pwfx->nChannels;
			device->normfunction(device->mix_buffer, device->buffer + writepos, todo);
			device->normfunction(device->mix_buffer + offs_float, device->buffer, maxq - todo);
		}
		else
			device->normfunction(device->mix_buffer, device->buffer + writepos, maxq);

		/* update the mix position, taking wrap-around into account */
		device->mixpos = writepos + maxq;
		device->mixpos %= device->buflen;

		/* update prebuff left */
		prebuff_left = DSOUND_BufPtrDiff(device->buflen, device->mixpos, playpos);

		/* check if have a whole fragment */
		if (prebuff_left >= device->fraglen){

			/* update the wave queue */
			DSOUND_WaveQueue(device, FALSE);

			/* buffers are full. start playing if applicable */
			if(device->state == STATE_STARTING){
				TRACE("started primary buffer\n");
				if(DSOUND_PrimaryPlay(device) != DS_OK){
					WARN("DSOUND_PrimaryPlay failed\n");
				}
				else{
					/* we are playing now */
					device->state = STATE_PLAYING;
				}
			}

			/* buffers are full. start stopping if applicable */
			if(device->state == STATE_STOPPED){
				TRACE("restarting primary buffer\n");
				if(DSOUND_PrimaryPlay(device) != DS_OK){
					WARN("DSOUND_PrimaryPlay failed\n");
				}
				else{
					/* start stopping again. as soon as there is no more data, it will stop */
					device->state = STATE_STOPPING;
				}
			}
		}

		/* if device was stopping, its for sure stopped when all buffers have stopped */
		else if (all_stopped && (device->state == STATE_STOPPING)) {
			TRACE("All buffers have stopped. Stopping primary buffer\n");
			device->state = STATE_STOPPED;

			/* stop the primary buffer now */
			DSOUND_PrimaryStop(device);
		}

	} else if (device->state != STATE_STOPPED) {

		DSOUND_WaveQueue(device, TRUE);

		/* in the DSSCL_WRITEPRIMARY mode, the app is totally in charge... */
		if (device->state == STATE_STARTING) {
			if (DSOUND_PrimaryPlay(device) != DS_OK)
				WARN("DSOUND_PrimaryPlay failed\n");
			else
				device->state = STATE_PLAYING;
		}
		else if (device->state == STATE_STOPPING) {
			if (DSOUND_PrimaryStop(device) != DS_OK)
				WARN("DSOUND_PrimaryStop failed\n");
			else
				device->state = STATE_STOPPED;
		}
	}

	LeaveCriticalSection(&(device->mixlock));
	/* **** */
}

DWORD CALLBACK DSOUND_mixthread(void *p)
{
	DirectSoundDevice *dev = p;
	TRACE("(%p)\n", dev);

	while (dev->ref) {
		DWORD ret;

		/*
		 * Some audio drivers are retarded and won't fire after being
		 * stopped, add a timeout to handle this.
		 */
		ret = WaitForSingleObject(dev->sleepev, dev->sleeptime);
		if (ret == WAIT_FAILED)
			WARN("wait returned error %u %08x!\n", GetLastError(), GetLastError());
		else if (ret != WAIT_OBJECT_0)
			WARN("wait returned %08x!\n", ret);
		if (!dev->ref)
			break;

		RtlAcquireResourceShared(&(dev->buffer_list_lock), TRUE);
		DSOUND_PerformMix(dev);
		RtlReleaseResource(&(dev->buffer_list_lock));
	}
	SetEvent(dev->thread_finished);
	return 0;
}
