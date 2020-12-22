#include <opentok.h>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <signal.h>

#include "config.h"
#include "otk_thread.h"

static std::atomic<bool> g_is_connected(false);

FILE *audio_out;

struct audio_device {
  otc_audio_device_callbacks audio_device_callbacks;
  otk_thread_t renderer_thread;
  std::atomic<bool> renderer_thread_exit;
};

static otk_thread_func_return_type renderer_thread_start_function(void *arg) {
  struct audio_device *device = static_cast<struct audio_device *>(arg);
  if (device == nullptr) {
    otk_thread_func_return_value;
  }

  while (device->renderer_thread_exit.load() == false) {
  	int16_t samples[160];
	size_t actual = otc_audio_device_read_render_data(samples,160);
	if(actual > 0){
		fwrite(samples, sizeof(int16_t),actual,audio_out);
	}
	usleep(10*1000);
  }

  otk_thread_func_return_value;
}

static otc_bool audio_device_destroy_renderer(const otc_audio_device *audio_device,
                                              void *user_data) {
  struct audio_device *device = static_cast<struct audio_device *>(user_data);
  if (device == nullptr) {
    return OTC_FALSE;
  }

  device->renderer_thread_exit = true;
  otk_thread_join(device->renderer_thread);

  return OTC_TRUE;
}

static otc_bool audio_device_start_renderer(const otc_audio_device *audio_device,
                                            void *user_data) {
  struct audio_device *device = static_cast<struct audio_device *>(user_data);
  printf("Starting audio renderer\n");
  if (device == nullptr) {
    return OTC_FALSE;
  }

  device->renderer_thread_exit = false;
  if (otk_thread_create(&(device->renderer_thread), &renderer_thread_start_function, (void *)device) != 0) {
    return OTC_FALSE;
  }
  printf("Started audio renderer\n");

  return OTC_TRUE;
}

static otc_bool audio_device_get_render_settings(const otc_audio_device *audio_device,
                                                  void *user_data,
                                                  struct otc_audio_device_settings *settings) {
  if (settings == nullptr) {
    return OTC_FALSE;
  }

  settings->number_of_channels = 1;
  settings->sampling_rate = 16000;
  return OTC_TRUE;
}

static void on_subscriber_connected(otc_subscriber *subscriber,
                                    void *user_data,
                                    const otc_stream *stream) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
  otc_subscriber_set_subscribe_to_video(subscriber,0);
}

static void on_subscriber_render_frame(otc_subscriber *subscriber,
                                       void *user_data,
                                       const otc_video_frame *frame) {
}

static void on_subscriber_error(otc_subscriber* subscriber,
                                void *user_data,
                                const char* error_string,
                                enum otc_subscriber_error_code error) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
  std::cout << "Subscriber error. Error code: " << error_string << std::endl;
}

static void on_session_connected(otc_session *session, void *user_data) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;

  g_is_connected = true;

}

static void on_session_connection_created(otc_session *session,
                                          void *user_data,
                                          const otc_connection *connection) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
}

static void on_session_connection_dropped(otc_session *session,
                                          void *user_data,
                                          const otc_connection *connection) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
}

static void on_session_stream_received(otc_session *session,
                                       void *user_data,
                                       const otc_stream *stream) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
  struct otc_subscriber_callbacks subscriber_callbacks = {0};
  subscriber_callbacks.user_data = user_data;
  subscriber_callbacks.on_connected = on_subscriber_connected;
  subscriber_callbacks.on_render_frame = on_subscriber_render_frame;
  subscriber_callbacks.on_error = on_subscriber_error;

  otc_subscriber *subscriber = otc_subscriber_new(stream,&subscriber_callbacks);
  otc_subscriber_set_subscribe_to_video(subscriber,0);
 
  if (otc_session_subscribe(session, subscriber) == OTC_SUCCESS) {
    printf("subscribed successfully\n");
    return;
  }
  else{
    printf("Error during subscribe\n");
  }
}

static void on_session_stream_dropped(otc_session *session,
                                      void *user_data,
                                      const otc_stream *stream) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
}

static void on_session_disconnected(otc_session *session, void *user_data) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
}

static void on_session_error(otc_session *session,
                             void *user_data,
                             const char *error_string,
                             enum otc_session_error_code error) {
  std::cout << __FUNCTION__ << " callback function" << std::endl;
  std::cout << "Session error. Error : " << error_string << std::endl;
}

static void on_otc_log_message(const char* message) {
  std::cout <<  __FUNCTION__ << ":" << message << std::endl;
}

void sigfun(int sig)
{
        printf("You have presses Ctrl-C , please press again to exit\n");
	(void) signal(SIGINT, SIG_DFL);
}

int main(int argc, char** argv) {
  if (otc_init(nullptr) != OTC_SUCCESS) {
    std::cout << "Could not init OpenTok library" << std::endl;
    return EXIT_FAILURE;
  }
 (void) signal(SIGINT, sigfun);
#ifdef CONSOLE_LOGGING
  otc_log_set_logger_callback(on_otc_log_message);
  otc_log_enable(OTC_LOG_LEVEL_ALL);
#endif

  if((audio_out = fopen("audio_out.pcm","wb"))==NULL){
	printf("Error opening audio output file\n");
	exit(1);
  } 
  struct audio_device *device = (struct audio_device *)malloc(sizeof(struct audio_device));
  device->audio_device_callbacks = {0};
  device->audio_device_callbacks.user_data = static_cast<void *>(device);
  device->audio_device_callbacks.destroy_renderer = audio_device_destroy_renderer;
  device->audio_device_callbacks.start_renderer = audio_device_start_renderer;
  device->audio_device_callbacks.get_render_settings = audio_device_get_render_settings;
  otc_set_audio_device(&(device->audio_device_callbacks));
  
  struct otc_session_callbacks session_callbacks = {0};
  session_callbacks.on_connected = on_session_connected;
  session_callbacks.on_connection_created = on_session_connection_created;
  session_callbacks.on_connection_dropped = on_session_connection_dropped;
  session_callbacks.on_stream_received = on_session_stream_received;
  session_callbacks.on_stream_dropped = on_session_stream_dropped;
  session_callbacks.on_disconnected = on_session_disconnected;
  session_callbacks.on_error = on_session_error;

  otc_session *session = nullptr;
  session = otc_session_new(API_KEY, SESSION_ID, &session_callbacks);

  if (session == nullptr) {
    std::cout << "Could not create OpenTok session successfully" << std::endl;
    return EXIT_FAILURE;
  }

  otc_session_connect(session, TOKEN);

  while(1){
	  sleep(1);
  }

  if ((session != nullptr) && g_is_connected.load()) {
    otc_session_disconnect(session);
  }

  if (session != nullptr) {
    otc_session_delete(session);
  }

  if (device != nullptr) {
    free(device);
  }

  otc_destroy();

  return EXIT_SUCCESS;
}
