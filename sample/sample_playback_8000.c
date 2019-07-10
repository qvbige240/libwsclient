#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <wsclient/wsclient.h>

int onclose(wsclient *c) {
	fprintf(stderr, "onclose called: %d\n", c->sockfd);
	return 0;
}

int onerror(wsclient *c, wsclient_error *err) {
	fprintf(stderr, "onerror: (%d): %s\n", err->code, err->str);
	if(err->extra_code) {
		errno = err->extra_code;
		perror("recv");
	}
	return 0;
}

#include <signal.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#define LOG_D printf
#define PORT 9002

int jt1078_package(char *buffer, const char *data, unsigned short len)
{
	int i = 0;

	char *p = buffer;
	p[i++] = 0x30;
	p[i++] = 0x31;
	p[i++] = 0x63;
	p[i++] = 0x64;

	p[8] = 0x01;
	p[9] = 0x36;
	p[10] = 0x84;
	p[11] = 0x09;
	p[12] = 0x11;
	p[13] = 0x97;

	p[15] = 0x30;
	p[28] = (len >> 8) & 0xff;
	p[29] = len & 0xff;

	memcpy(p+30, data, len);

	return len+30;
}


snd_pcm_t *playback_handle, *capture_handle;

static unsigned int rate = 8000;
static unsigned int format = SND_PCM_FORMAT_S16_LE;
static unsigned int channel = 1;

unsigned int buffer_time = 300000;
unsigned int period_time = 100000;
snd_pcm_sframes_t buffer_size;
snd_pcm_sframes_t period_size;

static int open_stream(snd_pcm_t **handle, const char *name, int dir)
{
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	const char *dirname = (dir == SND_PCM_STREAM_PLAYBACK) ? "PLAYBACK" : "CAPTURE";
	int err;

	if ((err = snd_pcm_open(handle, name, dir, 0)) < 0) {
		fprintf(stderr, "%s (%s): cannot open audio device (%s)\n", 
			name, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		fprintf(stderr, "%s (%s): cannot allocate hardware parameter structure(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_any(*handle, hw_params)) < 0) {
		fprintf(stderr, "%s (%s): cannot initialize hardware parameter structure(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_set_access(*handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "%s (%s): cannot set access type(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_set_format(*handle, hw_params, format)) < 0) {
		fprintf(stderr, "%s (%s): cannot set sample format(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_set_rate_near(*handle, hw_params, &rate, NULL)) < 0) {
		fprintf(stderr, "%s (%s): cannot set sample rate(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_hw_params_set_channels(*handle, hw_params, channel)) < 0) {
		fprintf(stderr, "%s (%s): cannot set channel count(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

	snd_pcm_hw_params_set_buffer_time_near(*handle, hw_params, &buffer_time, &dir);
	snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size);

	snd_pcm_hw_params_set_period_time_near(*handle, hw_params, &period_time, &dir);
	snd_pcm_hw_params_get_period_size(hw_params, &period_size, &dir);

	if ((err = snd_pcm_hw_params(*handle, hw_params)) < 0) {
		fprintf(stderr, "%s (%s): cannot set parameters(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
		fprintf(stderr, "%s (%s): cannot allocate software parameters structure(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}
	if ((err = snd_pcm_sw_params_current(*handle, sw_params)) < 0) {
		fprintf(stderr, "%s (%s): cannot initialize software parameters structure(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

#if 1 
	/* start the transfer when the buffer is almost full: */
	/* (buffer_size / avail_min) * avail_min */
	snd_pcm_sw_params_set_start_threshold(*handle, sw_params, (buffer_size / period_size) * period_size);
	snd_pcm_sw_params_set_avail_min(*handle, sw_params, period_size);
	printf("period_size = %d, buffer_size = %d, dir = %d\n", period_size, buffer_size, dir);

#else

	if ((err = snd_pcm_sw_params_set_avail_min(*handle, sw_params, BUFSIZE)) < 0) {
		fprintf(stderr, "%s (%s): cannot set minimum available count(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_sw_params_set_start_threshold(*handle, sw_params, BUFSIZE)) < 0) {
		fprintf(stderr, "%s (%s): cannot set start mode(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}
#endif

	if ((err = snd_pcm_sw_params(*handle, sw_params)) < 0) {
		fprintf(stderr, "%s (%s): cannot set software parameters(%s)\n",
			name, dirname, snd_strerror(err));
		return err;
	}

	return 0;
}

pthread_mutex_t		voice_mutex;
pthread_cond_t		voice_cond;
static int alsa_pcm_init()
{
	int err;

	if ((err = open_stream(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK)) < 0)
		return err;

	if ((err = open_stream(&capture_handle, "default", SND_PCM_STREAM_CAPTURE)) < 0)
		return err;

	if ((err = snd_pcm_prepare(playback_handle)) < 0) {
		fprintf(stderr, "cannot prepare audio interface for use(%s)\n",
			snd_strerror(err));
		return err;
	}

	if ((err = snd_pcm_start(capture_handle)) < 0) {
		fprintf(stderr, "cannot prepare audio interface for use(%s)\n",
			snd_strerror(err));
		return err;
	}

	pthread_mutex_init(&voice_mutex, NULL);
	pthread_cond_init(&voice_cond, NULL);

	return 0;
}
static int alsa_pcm_write(char *buffer)
{
	int wait_time = 1000;
	int ret = 0, avail, err;

write_wait:
	if ((err = snd_pcm_wait(playback_handle, wait_time)) < 0) {
		fprintf(stderr, "poll failed(%s)\n", strerror(errno));
		return -1;
	}

	avail = snd_pcm_avail_update(playback_handle);
	if (avail > 0) {
		if (avail < period_size) {
			//pthread_mutex_unlock(&voice_mutex);
			printf("============= writei snd_pcm_wait = %d ============\n", wait_time);
			goto write_wait;
		}
		if (avail > period_size)
			avail = period_size;
		ret = snd_pcm_writei(playback_handle, buffer, avail);
	}

	return ret;
}
static int alsa_pcm_play_8000_1(char *buffer, int len)
{
	int ret;

	//while((ret=snd_pcm_writei(playback_handle, buffer, period_size)) < 0)
	while((ret=alsa_pcm_write(buffer)) < 0)
	{
		printf("writei failed ret = %d, playback_handle = %p\n", ret, playback_handle);
		snd_pcm_prepare(playback_handle);
		if (ret == -EPIPE) {
			/* EPIPE means underrun */
			fprintf(stderr, "underrun occurred\n");
			snd_pcm_prepare(playback_handle);
		} 
		else if (ret < 0) {
			fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
		}  
	}
}

static void audio_init()
{
	pcm16_alaw_tableinit();
	alaw_pcm16_tableinit();

	alsa_pcm_init();
}

static char pcm_data[9632] = {0};

int onmessage(wsclient *c, wsclient_message *msg) {
	fprintf(stderr, "onmessage: (%llu): %s\n", msg->payload_len, msg->payload);

	char *data_buf = msg->payload + 30;
	int data_size = msg->payload_len - 30;

	alaw_to_pcm16(data_size, data_buf, pcm_data);
	//alsa_pcm_play_8000_1(msg->payload+30);
	alsa_pcm_play_8000_1(pcm_data, data_size * 2);

	return 0;
}

static char *audio = NULL;

int onopen(wsclient *c) {
    audio = malloc(1324);
    memset(audio, 0x48, 1324);
    memcpy(audio, "Hello King.", 11);
    char *str = "end";
    //strcpy(audio+1024-4, "end");
    memcpy(audio+1324-3, "end", 3);
	fprintf(stderr, "onopen called: %d\n", c->sockfd);
	libwsclient_send(c, "Hello onopen");
	//libwsclient_send(c, audio);

	audio_init();

	return 0;
}

/* ./play_test wss://172.17.13.222:9002/13684091197  */
int main(int argc, char **argv) {
	//Initialize new wsclient * using specified URI
    char *str = NULL;
    if (argc > 1)
    str = argv[1];
    else {
        printf("args url \n");
		fprintf(stderr, "Unable to initialize new WS client, with url null.\n");
        exit(1);
    }
	//wsclient *client = libwsclient_new("ws://echo.websocket.org");
	//wsclient *client = libwsclient_new("ws://localhost:9001");
	wsclient *client = libwsclient_new(str);
	if(!client) {
		fprintf(stderr, "Unable to initialize new WS client.\n");
		exit(1);
	}
	//set callback functions for this client
	libwsclient_onopen(client, &onopen);
	libwsclient_onmessage(client, &onmessage);
	libwsclient_onerror(client, &onerror);
	libwsclient_onclose(client, &onclose);
	//bind helper UNIX socket to "test.sock"
	//One can then use netcat (nc) to send data to the websocket server end on behalf of the client, like so:
	// $> echo -n "some data that will be echoed by echo.websocket.org" | nc -U test.sock
	libwsclient_helper_socket(client, "test.sock");
	//starts run thread.
	libwsclient_run(client);
	//blocks until run thread for client is done.
	libwsclient_finish(client);
	return 0;
}

