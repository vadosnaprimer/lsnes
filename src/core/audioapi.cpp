#include "core/audioapi.hpp"
#include "core/framerate.hpp"
#include "library/minmax.hpp"
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/time.h>
#ifdef NATIVE_THREADS
#include <thread>
#include <condition_variable>
#include <mutex>
typedef std::thread thread_class;
typedef std::condition_variable cv_class;
typedef std::mutex mutex_class;
typedef std::unique_lock<std::mutex> umutex_class;
#else
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
typedef boost::thread thread_class;
typedef boost::condition_variable cv_class;
typedef boost::mutex mutex_class;
typedef boost::unique_lock<boost::mutex> umutex_class;
#endif

//3 music buffers is not enough due to huge blocksizes used by SDL.
#define MUSIC_BUFFERS 8
#define MAX_VOICE_ADJUST 200

namespace
{
	const unsigned voicep_bufsize = 65536;
	const unsigned voicer_bufsize = 65536;
	const unsigned music_bufsize = 8192;
	float voicep_buffer[voicep_bufsize];
	float voicer_buffer[voicer_bufsize];
	int16_t music_buffer[MUSIC_BUFFERS * music_bufsize];
	volatile bool music_stereo[MUSIC_BUFFERS];
	volatile double music_rate[MUSIC_BUFFERS];
	volatile size_t music_size[MUSIC_BUFFERS];
	unsigned music_ptr;
	unsigned last_complete_music_seen = MUSIC_BUFFERS + 1;
	volatile unsigned last_complete_music = MUSIC_BUFFERS;
	volatile unsigned voicep_get = 0;
	volatile unsigned voicep_put = 0;
	volatile unsigned voicer_get = 0;
	volatile unsigned voicer_put = 0;
	volatile unsigned voice_rate = 40000;
	volatile unsigned orig_voice_rate = 40000;
	volatile bool dummy_cb_active = false;
	volatile bool dummy_cb_quit = false;
	volatile float music_volume = 1;
	volatile float voicep_volume = 32767.0;
	volatile float voicer_volume = 1.0/32768;

	struct dummy_cb_proc
	{
		int operator()()
		{
			float buf[16384];
			uint64_t last_ts = get_utime();
			while(!dummy_cb_quit) {
				uint64_t cur_ts = get_utime();
				uint64_t dt = cur_ts - last_ts;
				unsigned samples = dt / 25;
				if(samples > 16384)
					samples = 16384;	//Don't get crazy.
				if(dummy_cb_active) {
					audioapi_get_voice(buf, samples);
					audioapi_put_voice(NULL, samples);
				}
				usleep(10000);
			}
		}
	};

	dummy_cb_proc* dummy_cb_proc_obj;
	thread_class* dummy_cb_thread;

	
//  | -1  1 -1  1 | 1  0  0  0 |
//  |  0  0  0  1 | 0  1  0  0 |
//  |  1  1  1  1 | 0  0  1  0 |
//  |  8  4  2  1 | 0  0  0  1 |


//  |  6  0  0  0 |-1  4 -3  1 |           |-1  3 -3  1|
//  |  0  6  0  0 | 3 -6  3  0 |      1/6  | 3 -6  3  0|
//  |  0  0  6  0 |-2 -4  6 -1 |           |-2 -3  6 -1|
//  |  0  0  0  6 | 0  6  0  0 |           | 0  6  0  0|



	void cubicitr_solve(double v1, double v2, double v3, double v4, double& A, double& B, double& C, double& D)
	{
		A = (-v1 + 3 * v2 - 3 * v3 + v4) / 6;
		B = (v1 - 2 * v2 + v3) / 2;
		C = (-2 * v1 - 3 * v2 + 6 * v3 - v4) / 6;
		D = v2;
	}

	void linintr_solve(double v1, double v2, double& A, double& B)
	{
		A = (v2 - v1);
		B = v1;
	}
}

audioapi_resampler::audioapi_resampler()
{
	position = 0;
	vAl = vBl = vCl = vDl = 0;
	vAr = vBr = vCr = vDr = 0;
}

void audioapi_resampler::resample(float*& in, size_t& insize, float*& out, size_t& outsize, double ratio, bool stereo)
{
	double iratio = 1 / ratio;
	while(outsize) {
		double newpos = position + iratio;
		while(newpos >= 1) {
			//Gotta load a new sample.
			if(!insize)
				goto exit;
			vAl = vBl; vBl = vCl; vCl = vDl; vDl = in[0];
			vAr = vBr; vBr = vCr; vCr = vDr; vDr = in[stereo ? 1 : 0];
			--insize;
			in += (stereo ? 2 : 1);
			newpos = newpos - 1;
		}
		position = newpos;
		double A, B, C, D;
		cubicitr_solve(vAl, vBl, vCl, vDl, A, B, C, D);
		*(out++) = ((A * position + B) * position + C) * position + D;
		if(stereo) {
			cubicitr_solve(vAr, vBr, vCr, vDr, A, B, C, D);
			*(out++) = ((A * position + B) * position + C) * position + D;
		}
		--outsize;
	}
exit:
	;
}


unsigned audioapi_voice_rate()
{
	return voice_rate;
}

void audioapi_voice_rate(unsigned rate)
{
	orig_voice_rate = voice_rate = rate;
}

unsigned audioapi_voice_p_status()
{
	unsigned p = voicep_put;
	unsigned g = voicep_get;
	if(g > p)
		return g - p - 1;
	else
		return voicep_bufsize - (p - g) - 1;
}

unsigned audioapi_voice_p_status2()
{
	unsigned p = voicep_put;
	unsigned g = voicep_get;
	if(g > p)
		return voicep_bufsize - (g - p);
	else
		return (p - g);
}

unsigned audioapi_voice_r_status()
{
	unsigned p = voicer_put;
	unsigned g = voicer_get;
	if(g > p)
		return voicer_bufsize - (g - p);
	else
		return (p - g);
	
}

void audioapi_play_voice(float* samples, size_t count)
{
	unsigned ptr = voicep_put;
	for(size_t i = 0; i < count; i++) {
		voicep_buffer[ptr++] = samples[i];
		if(ptr == voicep_bufsize)
			ptr = 0;
	}
	voicep_put = ptr;
}

void audioapi_record_voice(float* samples, size_t count)
{
	unsigned ptr = voicer_get;
	for(size_t i = 0; i < count; i++) {
		samples[i] = voicer_buffer[ptr++];
		if(ptr == voicer_bufsize)
			ptr = 0;
	}
	voicer_get = ptr;
}

void audioapi_submit_buffer(int16_t* samples, size_t count, bool stereo, double rate)
{
	//Limit buffers to avoid overrunning.
	if(count > music_bufsize / (stereo ? 2 : 1))
		count = music_bufsize / (stereo ? 2 : 1);
	unsigned bidx = last_complete_music;
	bidx = (bidx > (MUSIC_BUFFERS - 2)) ? 0 : bidx + 1;
	memcpy(music_buffer + bidx * music_bufsize, samples, count * (stereo ? 2 : 1) * sizeof(int16_t));
	music_stereo[bidx] = stereo;
	music_rate[bidx] = rate;
	music_size[bidx] = count;
	last_complete_music = bidx;
}

struct audioapi_buffer audioapi_get_music(size_t played)
{
	static bool last_adjust = false;	//Adjusting consequtively is too hard.
	unsigned midx = last_complete_music_seen;
	unsigned midx2 = last_complete_music;
	if(midx2 >= MUSIC_BUFFERS) {
		//Special case: No buffer.
		struct audioapi_buffer out;
		out.samples = NULL;
		out.pointer = 0;
		//The rest are arbitrary.
		out.total = 64;
		out.stereo = false;
		out.rate = 48000;
		return out;
	}
	//Handle ACK. If the current buffer is too old, we want to ignore the ACK.
	if(midx >= MUSIC_BUFFERS) {
		//Load initial buffer.
		midx = last_complete_music_seen = 0;
		music_ptr = 0;
	} else {
		music_ptr += played;
		//Otherwise, check if current buffer is not next on the line to be overwritten.
		if((midx2 + 1) % MUSIC_BUFFERS == midx) {
			//It is, bump buffer by one.
			if(!last_adjust && voice_rate > orig_voice_rate - MAX_VOICE_ADJUST)
				voice_rate--;
			last_adjust = true;
			midx = last_complete_music_seen = (midx + 1) % MUSIC_BUFFERS;
			music_ptr = 0;
		} else if(music_ptr >= music_size[midx] && midx != midx2) {
			//It isn't, but current buffer is finished.
			midx = last_complete_music_seen = (midx + 1) % MUSIC_BUFFERS;
			music_ptr = 0;
			last_adjust = false;
		} else if(music_ptr >= music_size[midx] && midx == midx2) {
			if(!last_adjust && voice_rate < orig_voice_rate + MAX_VOICE_ADJUST)
				voice_rate++;
			last_adjust = true;
			//Current buffer is finished, but there is no new buffer.
			//Send silence.
		} else {
			last_adjust = false;
			//Can continue.
		}
	}
	//Fill the structure.
	struct audioapi_buffer out;
	if(music_ptr < music_size[midx]) {
		out.samples = music_buffer + midx * music_bufsize;
		out.pointer = music_ptr;
		out.total = music_size[midx];
		out.stereo = music_stereo[midx];
		out.rate = music_rate[midx];
	} else {
		//Run out of buffers to play.
		out.samples = NULL;
		out.pointer = 0;
		out.total = 64;		//Arbitrary.
		out.stereo = music_stereo[midx];
		out.rate = music_rate[midx];
		if(out.rate < 100)
			out.rate = 48000;	//Apparently there are buffers with zero rate.
	}
	return out;
}

void audioapi_get_voice(float* samples, size_t count)
{
	unsigned g = voicep_get;
	unsigned p = voicep_put;
	for(size_t i = 0; i < count; i++) {
		if(g != p)
			samples[i] = voicep_volume * voicep_buffer[g++];
		else
			samples[i] = 0.0;
		if(g == voicep_bufsize)
			g = 0;
	}
	voicep_get = g;
}

void audioapi_put_voice(float* samples, size_t count)
{
	unsigned ptr = voicer_put;
	for(size_t i = 0; i < count; i++) {
		voicer_buffer[ptr++] = samples ? voicer_volume * samples[i] : 0.0;
		if(ptr == voicer_bufsize)
			ptr = 0;
	}
	voicer_put = ptr;
}

void audioapi_set_dummy_cb(bool enable)
{
	if(enable)
		voice_rate = 40000;
	dummy_cb_active = enable;
}


void audioapi_init()
{
	voicep_get = 0;
	voicep_put = 0;
	voicer_get = 0;
	voicer_put = 0;
	last_complete_music = 3;
	last_complete_music_seen = 4;
	dummy_cb_active = true;
	dummy_cb_quit = false;
	dummy_cb_proc_obj = new dummy_cb_proc;
	dummy_cb_thread = new thread_class(*dummy_cb_proc_obj);
}

void audioapi_quit()
{
	dummy_cb_quit = true;
	dummy_cb_thread->join();
	delete dummy_cb_proc_obj;
}

void audioapi_music_volume(float volume)
{
	music_volume = volume;
}

float audioapi_music_volume()
{
	return music_volume;
}

void audioapi_voicep_volume(float volume)
{
	voicep_volume = volume * 32767;
}

float audioapi_voicep_volume()
{
	return voicep_volume;
}

void audioapi_voicer_volume(float volume)
{
	voicer_volume = volume / 32768;
}

float audioapi_voicer_volume()
{
	return voicer_volume;
}

void audioapi_get_mixed(int16_t* samples, size_t count, bool stereo)
{
	static audioapi_resampler music_resampler;
	const size_t intbuf_size = 256;
	float intbuf[intbuf_size];
	float intbuf2[intbuf_size];
	while(count > 0) {
		audioapi_buffer b = audioapi_get_music(0);
		float* in = intbuf;
		float* out = intbuf2;
		size_t outdata_used;
		if(b.stereo) {
			size_t indata = min(b.total - b.pointer, intbuf_size / 2);
			size_t outdata = min(intbuf_size / 2, count);
			size_t indata_used = indata;
			outdata_used = outdata;
			if(b.samples)
				for(size_t i = 0; i < 2 * indata; i++)
					intbuf[i] = music_volume * b.samples[i + 2 * b.pointer];
			else
				for(size_t i = 0; i < 2 * indata; i++)
					intbuf[i] = 0;
			music_resampler.resample(in, indata, out, outdata, voice_rate / b.rate, true);
			indata_used -= indata;
			outdata_used -= outdata;
			audioapi_get_music(indata_used);
			audioapi_get_voice(intbuf, outdata_used);
			for(size_t i = 0; i < outdata_used * (stereo ? 2 : 1); i++)
				intbuf2[i] = max(min(intbuf2[i] + intbuf[i / 2], 32766.0f), -32767.0f);
			if(stereo)
				for(size_t i = 0; i < outdata_used * 2; i++)
					samples[i] = intbuf2[i];
			else
				for(size_t i = 0; i < outdata_used; i++)
					samples[i] = (intbuf2[2 * i + 0] + intbuf2[2 * i + 1]) / 2;
		} else {
			size_t indata = min(b.total - b.pointer, intbuf_size);
			size_t outdata = min(intbuf_size, count);
			size_t indata_used = indata;
			outdata_used = outdata;
			if(b.samples)
				for(size_t i = 0; i < indata; i++)
					intbuf[i] = music_volume * b.samples[i + b.pointer];
			else
				for(size_t i = 0; i < indata; i++)
					intbuf[i] = 0;
			music_resampler.resample(in, indata, out, outdata, voice_rate / b.rate, false);
			indata_used -= indata;
			outdata_used -= outdata;
			audioapi_get_music(indata_used);
			audioapi_get_voice(intbuf, outdata_used);
			for(size_t i = 0; i < outdata_used; i++)
				intbuf2[i] = max(min(intbuf2[i] + intbuf[i], 32766.0f), -32767.0f);
			if(stereo)
				for(size_t i = 0; i < outdata_used; i++) {
					samples[2 * i + 0] = intbuf2[i];
					samples[2 * i + 1] = intbuf2[i];
				}
			else
				for(size_t i = 0; i < outdata_used; i++)
					samples[i] = intbuf2[i];
		}
		samples += (stereo ? 2 : 1) * outdata_used;
		count -= outdata_used;
	}
}