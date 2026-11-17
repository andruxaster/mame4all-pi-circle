/* sound.cpp - MAME4ALL sound output for the Circle bare-metal port.
   MAME produces one frame of samples per emulated frame; they go straight to the Circle
   sound device queue (HDMI or PWM), see circle_glue.cpp. */
#include "driver.h"
#include "minimal.h"

int soundcard, usestereo;
int master_volume = 100;
int attenuation = 0;

static INT16 *stream_cache_data;
static int stream_cache_channels;
static int samples_per_frame;
static int sound_running;

int msdos_init_sound(void)
{
	if (soundcard == -1) soundcard = 1;
	if (soundcard == 0)
	{
		Machine->sample_rate = 0;	/* sound disabled */
		return 0;
	}
	stream_cache_data = 0;
	stream_cache_channels = 0;
	return 0;
}

void msdos_shutdown_sound(void)
{
}

int osd_start_audio_stream(int stereo)
{
	if (stereo) stereo = 1;
	stream_cache_channels = stereo ? 2 : 1;
	if (options.force_stereo) stream_cache_channels = 2;

	if (Machine->drv->frames_per_second > 60)
		samples_per_frame = Machine->sample_rate / (int) Machine->drv->frames_per_second;
	else
		samples_per_frame = Machine->sample_rate / Machine->drv->frames_per_second;

	if (Machine->sample_rate == 0) return samples_per_frame;

	cg_sound_start(Machine->sample_rate, stream_cache_channels == 2);
	sound_running = 1;
	return samples_per_frame;
}

void osd_stop_audio_stream(void)
{
	if (sound_running)
	{
		cg_sound_stop();
		sound_running = 0;
	}
}

int osd_update_audio_stream(INT16 *buffer)
{
	if (Machine->sample_rate == 0) return samples_per_frame;
	stream_cache_data = buffer;
	cg_sound_write(buffer, samples_per_frame);
	return samples_per_frame;
}

int msdos_update_audio(void)
{
	return 0;
}

void osd_set_mastervolume(int _attenuation)
{
	attenuation = _attenuation;
}

int osd_get_mastervolume(void)
{
	return attenuation;
}

void osd_sound_enable(int enable_it)
{
}

void osd_opl_control(int chip, int reg)
{
}

void osd_opl_write(int chip, int data)
{
}
