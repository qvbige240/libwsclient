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
#include <alsa/asoundlib.h>

int size;
snd_pcm_uframes_t frames;
snd_pcm_t *playback_handle;//PCM设备句柄pcm.h
static void alsa_release(int exitcode)
{

	usleep(200000);
	printf("recv: SIGINT\n");
	// 关闭PCM设备句柄
	snd_pcm_drain(playback_handle);
	snd_pcm_close(playback_handle);

	//if (fp != NULL)
	//	fclose(fp);
}
static void alsa_pcm_setting_8000_1()
{
	int ret;
	//int buf[128];
	unsigned int val;
	unsigned int rate = 8000, channel = 1;
	int dir=0;
	//char *buffer;
	//int size;
	snd_pcm_uframes_t periodsize;
	//snd_pcm_t *playback_handle;//PCM设备句柄pcm.h
	snd_pcm_hw_params_t *hw_params;//硬件信息和PCM流配置
	//if (argc != 2) {
	//	printf("error: alsa_play_test [music name]\n");
	//	exit(1);
	//}
	//printf("play song %s by wolf\n", argv[1]);
	//FILE *fp = fopen(argv[1], "rb");
	//fp = fopen(argv[1], "rb");
	//if(fp == NULL)
	//	return 0;
	//fseek(fp, 100, SEEK_SET);

	//1. 打开PCM，最后一个参数为0意味着标准配置
	ret = snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (ret < 0) {
		perror("snd_pcm_open");
		exit(1);
	}

	//2. 分配snd_pcm_hw_params_t结构体
	ret = snd_pcm_hw_params_malloc(&hw_params);
	if (ret < 0) {
		perror("snd_pcm_hw_params_malloc");
		exit(1);
	}
	//3. 初始化hw_params
	ret = snd_pcm_hw_params_any(playback_handle, hw_params);
	if (ret < 0) {
		perror("snd_pcm_hw_params_any");
		exit(1);
	}
	//4. 初始化访问权限
	ret = snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret < 0) {
		perror("snd_pcm_hw_params_set_access");
		exit(1);
	}
	//5. 初始化采样格式SND_PCM_FORMAT_U8,8位
	ret = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE);
	if (ret < 0) {
		perror("snd_pcm_hw_params_set_format");
		exit(1);
	}
	//6. 设置采样率，如果硬件不支持我们设置的采样率，将使用最接近的
	//val = 44100,有些录音采样频率固定为8KHz


	//val = 8000;
	//val = 44100;
	ret = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, &dir);
	if (ret < 0) {
		perror("snd_pcm_hw_params_set_rate_near");
		exit(1);
	}
	//7. 设置通道数量
	ret = snd_pcm_hw_params_set_channels(playback_handle, hw_params, channel);
	if (ret < 0) {
		perror("snd_pcm_hw_params_set_channels");
		exit(1);
	}
	printf("sample rate: %d, channel: %d\n ", rate, channel);

	//frames = 160;
	frames = rate * 20 / 1000;
	frames = frames / channel;
    frames = 400;
	ret = snd_pcm_hw_params_set_period_size_near(playback_handle, hw_params, &frames, 0);
	if (ret < 0) 
	{
		printf("Unable to set period size %li : %s\n", frames,  snd_strerror(ret));
	}
	printf("set frames: %d\n ", frames);

	//periodsize = 800;
	periodsize = (rate / 1000) * 100;
	//periodsize = periodsize * 2;
	ret = snd_pcm_hw_params_set_buffer_size_near(playback_handle, hw_params, &periodsize);
	if (ret < 0) 
	{
		printf("Unable to set buffer size %li : %s\n", periodsize, snd_strerror(ret));

	}
	printf("set periodsize: %d\n ", periodsize);
	///* Set period size to 32 frames. */
	//frames = 32;
	//periodsize = frames;
	////periodsize = val * 2;//frames;
	//ret = snd_pcm_hw_params_set_buffer_size_near(playback_handle, hw_params, &periodsize);
	//if (ret < 0) 
	//{
	//	printf("Unable to set buffer size %li : %s\n", frames * 2, snd_strerror(ret));

	//}
	//printf("set buffer, periodsize: %d\n ", periodsize);
	//periodsize /= 2;

	//ret = snd_pcm_hw_params_set_period_size_near(playback_handle, hw_params, &periodsize, 0);
	//if (ret < 0) 
	//{
	//	printf("Unable to set period size %li : %s\n", periodsize,  snd_strerror(ret));
	//}

	//8. 设置hw_params
	ret = snd_pcm_hw_params(playback_handle, hw_params);
	if (ret < 0) {
		perror("snd_pcm_hw_params");
		exit(1);
	}

	/* Use a buffer large enough to hold one period */
	snd_pcm_hw_params_get_period_size(hw_params, &frames, &dir);

	size = frames * 2; /* 2 bytes/sample, 2 channels */
	//buffer = (char *) malloc(size + 1);
	//memset(buffer, 0x0, size+1);
	fprintf(stderr, "size = %d\n", size);

	signal(SIGINT, alsa_release);
	signal(SIGTERM, alsa_release);

}

static void alsa_pcm_play_8000_1(char *buffer)
{
	int ret;

	//9. 写音频数据到PCM设备
	//while(ret = snd_pcm_writei(playback_handle, buffer, frames*2)<0)

	//ret = snd_pcm_writei(playback_handle, buffer, frames);

	//while(ret = snd_pcm_writei(playback_handle, buffer, 320)<0)
	while(ret = snd_pcm_writei(playback_handle, buffer, frames)<0)
	{
		usleep(2000);
		printf("writei failed \n");
		snd_pcm_prepare(playback_handle);
		if (ret == -EPIPE)
		{
			/* EPIPE means underrun */
			fprintf(stderr, "underrun occurred\n");
			//完成硬件参数设置，使设备准备好
			snd_pcm_prepare(playback_handle);
		} 
		else if (ret < 0) 
		{
			fprintf(stderr,
				"error from writei: %s\n",
				snd_strerror(ret));
		}  
	}
}

static char pcm_data[9632] = {0};

int onmessage(wsclient *c, wsclient_message *msg) {
	fprintf(stderr, "onmessage: (%llu): %s\n", msg->payload_len, msg->payload);

	char *data_buf = msg->payload + 30;
	int data_size = msg->payload_len - 30;
	//if (strcmp(argv[2], "alaw_pcm") == 0) 
	{
		alaw_pcm16_tableinit();

		alaw_to_pcm16(data_size, data_buf, pcm_data);
	} 

	//alsa_pcm_play_8000_1(msg->payload+30);
	alsa_pcm_play_8000_1(pcm_data);

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

	alsa_pcm_setting_8000_1();

	return 0;
}

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

