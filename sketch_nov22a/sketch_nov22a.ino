#include <SDHCI.h>
#include <MediaRecorder.h>
#include <MediaPlayer.h>
#include <OutputMixer.h>
#include <MemoryUtil.h>

SDClass theSD;
MediaRecorder *theRecorder;
MediaPlayer *thePlayer;
OutputMixer *theMixer;
File playFile;
File recFile;
static bool ErrEnd = false;
bool bRecStart = false;
bool bRecording = false;

static const uint32_t recoding_bitrate = 192000 * 8 * 2; //  Set 48000 or 192000
static const uint32_t recoding_sampling_rate = 192000;//  Set 16000 or 48000, 192000
static const int32_t  recoding_size = (recoding_bitrate / 8) * 4;
static const uint32_t frame_size  = (1152 * (recoding_bitrate / 8)) / recoding_sampling_rate;//  Calculated with 1152 samples per frame.
static const uint32_t buffer_size = (frame_size + 511) & ~511;//  Buffer size：  Align in 512byte units based on frame size.
static uint8_t        s_buffer[buffer_size];
static int gCounter = 0;
static char filename[12] = {0};

static void changeState() {
  bRecStart = bRecStart ? false : true;
}

static void attention_cb(const ErrorAttentionParam *atprm) {
  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING) {
    ErrEnd = true;
  }
}

static void mediarecorder_attention_cb(const ErrorAttentionParam *atprm) {
  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING) {
    ErrEnd = true;
  }
}

static void outputmixer_done_callback(MsgQueId requester_dtq, MsgType reply_of, AsOutputMixDoneParam *done_param) {
  return;
}
static void outmixer_send_callback(int32_t identifier, bool is_end) {
  AsRequestNextParam next;
  next.type = (!is_end) ? AsNextNormalRequest : AsNextStopResRequest;
  AS_RequestNextPlayerProcess(AS_PLAYER_ID_0, &next);
  return;
}
static bool mediaplayer_done_callback(AsPlayerEvent event, uint32_t result, uint32_t sub_result) {
  return true; //printf("mp cb %x %x %x\n", event, result, sub_result);
}
static void mediaplayer_decode_callback(AsPcmDataParam pcm_param) {
  theMixer->sendData(OutputMixer0, outmixer_send_callback, pcm_param);
}
static bool mediarecorder_done_callback(AsRecorderEvent event, uint32_t result, uint32_t sub_result) {
  return true;
}

void setup() {
  attachInterrupt(digitalPinToInterrupt(4) , changeState , FALLING);
  initMemoryPools();
  createStaticPools(MEM_LAYOUT_RECORDINGPLAYER);
}

void audio() {
  while (!theSD.begin()) {
    puts("Insert SD card.");
  }
  theRecorder = MediaRecorder::getInstance();
  thePlayer = MediaPlayer::getInstance();
  theMixer = OutputMixer::getInstance();
  theMixer->setRenderingClkMode(OUTPUTMIXER_RNDCLK_NORMAL);

  thePlayer->begin();
  theMixer->begin();
  theRecorder->begin(mediarecorder_attention_cb);

  thePlayer->create(MediaPlayer::Player0, attention_cb);
  theMixer->create(attention_cb);
  thePlayer->activate(MediaPlayer::Player0, mediaplayer_done_callback, 512 * 2 * 10);
  theMixer->activate(OutputMixer0, HPOutputDevice, outputmixer_done_callback);//  Set I2SOutputDevice or HPOutputDevice
  theRecorder->activate(AS_SETRECDR_STS_INPUTDEVICE_MIC, mediarecorder_done_callback, (768 * 2 * 8) * 10);
  usleep(100 * 1000);

  thePlayer->init(MediaPlayer::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_48000, AS_CHANNEL_MONO);
  theRecorder->init(AS_CODECTYPE_WAV, 1, recoding_sampling_rate, 16,  recoding_bitrate, "/mnt/sd0/BIN");
  //theRecorder->init(AS_CODECTYPE_WAV, recoding_cannel_number(Set 1, 2, or 4.), recoding_sampling_rate, recoding_bit_length(Set 16 or 24.),  recoding_bitrate, "/mnt/sd0/BIN");

  playFile = theSD.open("AUDIO/Sound.mp3");
  sprintf(filename, "REC%03d.wav", gCounter);
  recFile = theSD.open(filename , FILE_WRITE);
  /* Send first frames to be decoded */
  err_t err = thePlayer->writeFrames(MediaPlayer::Player0, playFile);
  if ((err != MEDIAPLAYER_ECODE_OK ) || !playFile || !recFile) {
    printf("File Read Error! =%d\n", err);
    playFile.close();
    stop_audio();
  }
  /* Main volume set to -16.0 dB, Main player and sub player set to 0 dB */
  theMixer->setVolume(-40, 0, 0);
  theRecorder->setMicGain(0);//ANALOG_MIC_GAIN
  thePlayer->start(MediaPlayer::Player0, mediaplayer_decode_callback);
  theRecorder->start();
  ++gCounter;
}

err_t execute_aframe(uint32_t* size) {
  err_t err = theRecorder->readFrames(s_buffer, buffer_size, size);
  if (!(((err == MEDIARECORDER_ECODE_OK) || (err == MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)) && (*size > 0))) {
    return err;
  }
  if (recFile.write((uint8_t*)&s_buffer, *size) < 0) {
    //puts("File write error.");
    err = MEDIARECORDER_ECODE_FILEACCESS_ERROR;
  }
  return err;
}

void execute_frames() {
  uint32_t read_size = 0;
  do {
    err_t err = execute_aframe(&read_size);
    if ((err != MEDIARECORDER_ECODE_OK) && (err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)) {
      break;
    }
  }
  while (read_size > 0);
}

void loop() {
  static int32_t total_size = 0;
  uint32_t read_size = 0;
  err_t err;

  if (bRecStart && !bRecording) {
    audio();
    usleep(10000);
    stop_audio();
    return;
  } else if (read_size > 0)  {
    total_size += read_size;
  } else {
    err = execute_aframe(&read_size);
    if (((err != MEDIARECORDER_ECODE_OK && err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA) || (total_size > recoding_size)) || (bRecStart && bRecording)) {
      //puts("Recording Error!");
      stop_audio();
    }
  }

  err = thePlayer->writeFrames(MediaPlayer::Player0, playFile);
  if (err == MEDIAPLAYER_ECODE_FILEEND) {
    puts("Main player File End!\n");
    return;
  }
  if (err || ErrEnd) {
    printf("Main player error code: %d\n", err);
    stop_audio();
  }
}

void stop_audio() {
  theRecorder->stop();
  thePlayer->stop(MediaPlayer::Player0);
  execute_frames();

  sleep(1); /* For data pipline stop */
  theRecorder->writeWavHeader(recFile);
  puts("Update Header!");
  playFile.close();
  recFile.close();
  usleep(100 * 1000);
  thePlayer->deactivate(MediaPlayer::Player0);
  theRecorder->deactivate();
  theMixer->end();
  thePlayer->end();
  theRecorder->end();
  //  AS_DeletePlayer(AS_PLAYER_ID_0);
  //  AS_DeleteOutputMix();
  //  AS_DeleteRenderer();
  //  AS_DeleteMediaRecorder();
  puts("End Recording");
}
