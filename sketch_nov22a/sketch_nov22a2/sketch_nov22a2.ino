#include <SDHCI.h>
#include <MediaRecorder.h>
#include <MediaPlayer.h>
#include <OutputMixer.h>
#include <MemoryUtil.h>
#include <stdio.h>

SDClass theSD;
MediaRecorder *theRecorder;
MediaPlayer *thePlayer;
OutputMixer *theMixer;
File playFile;
File recFile;

//#define RECORD_FILE_NAME "Sound.WAV"
#define ANALOG_MIC_GAIN  0 /* +0dB */
bool ErrEnd = false;

static const uint32_t recoding_bitrate = 192000 * 8 * 2; //  Set 48000 or 192000
static const uint32_t recoding_sampling_rate = 192000;//  Set 16000 or 48000, 192000
static const int32_t  recoding_size = (recoding_bitrate / 8) * 4;
static const uint32_t frame_size  = (1152 * (recoding_bitrate / 8)) / recoding_sampling_rate;//  Calculated with 1152 samples per frame.
static const uint32_t buffer_size = (frame_size + 511) & ~511;//  Buffer size：  Align in 512byte units based on frame size.
static uint8_t        s_buffer[buffer_size];
int gCounter = 0;
int intPin = 4;
static char filename[12] = {0};

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
  printf("mp cb %x %x %x\n", event, result, sub_result); return true;
}
void mediaplayer_decode_callback(AsPcmDataParam pcm_param) {
  theMixer->sendData(OutputMixer0, outmixer_send_callback, pcm_param);
}
static bool mediarecorder_done_callback(AsRecorderEvent event, uint32_t result, uint32_t sub_result) {
  return true;
}

void setup()
{
  while (!theSD.begin())
  {
    puts("Insert SD card.");
  }

  initMemoryPools();
  createStaticPools(MEM_LAYOUT_RECORDINGPLAYER);
  thePlayer = MediaPlayer::getInstance();
  theMixer  = OutputMixer::getInstance();
  theRecorder = MediaRecorder::getInstance();
  thePlayer->begin();
  theMixer->begin();
  theRecorder->begin(mediarecorder_attention_cb);
  puts("initialization Audio Library");

  thePlayer->create(MediaPlayer::Player0, attention_cb);
  theMixer->create(attention_cb);
  theMixer->setRenderingClkMode(OUTPUTMIXER_RNDCLK_NORMAL);
  thePlayer->activate(MediaPlayer::Player0, mediaplayer_done_callback, 1024 * 2 * 20);
  theMixer->activate(OutputMixer0, HPOutputDevice, outputmixer_done_callback);
  theRecorder->activate(AS_SETRECDR_STS_INPUTDEVICE_MIC, mediarecorder_done_callback, (384 * 2 * 2) * 4);

  usleep(100 * 1000);

  thePlayer->init(MediaPlayer::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_48000, AS_CHANNEL_MONO);
  theRecorder->init(AS_CODECTYPE_WAV,1,recoding_sampling_rate,
                    16,recoding_bitrate,"/mnt/sd0/BIN");

  playFile = theSD.open("AUDIO/Sound.mp3");
  sprintf(filename, "REC%03d.wav", gCounter);
  recFile  = theSD.open(filename, FILE_WRITE);

  if (!playFile)
  {
    puts("File open error\n");
    exit(1);
  }
  puts("Open! playFile\n");
  if (!recFile)
  {
    puts("File open error\n");
    exit(1);
  }
  puts("Open! recFile\n");
  err_t err = thePlayer->writeFrames(MediaPlayer::Player0, playFile);
  if (err != MEDIAPLAYER_ECODE_OK)
  {
    printf("File Read Error! =%d\n", err);
    playFile.close();
    exit(1);
  }
  puts("Play!");
  theMixer->setVolume(-160, 0, 0);
  theRecorder->setMicGain(ANALOG_MIC_GAIN);
  theRecorder->start();
  thePlayer->start(MediaPlayer::Player0, mediaplayer_decode_callback);
}

err_t execute_aframe(uint32_t* size)
{
  err_t err = theRecorder->readFrames(s_buffer, buffer_size, size);
  if (((err == MEDIARECORDER_ECODE_OK) || (err == MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)) && (*size > 0)){
 } else {
    return err;
  }
  int ret = recFile.write((uint8_t*)&s_buffer, *size);
  if (ret < 0){
    puts("File write error.");
    err = MEDIARECORDER_ECODE_FILEACCESS_ERROR;
  }
  return err;
}

void execute_frames()
{
  uint32_t read_size = 0;
  do
  {
    err_t err = execute_aframe(&read_size);
    if ((err != MEDIARECORDER_ECODE_OK) && (err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA))
    {
      break;
    }
  }
  while (read_size > 0);
}

void loop()
{
  static int32_t total_size = 0;
  uint32_t read_size = 0;
  err_t err;

#if 1
  err = execute_aframe(&read_size);
  if (err != MEDIARECORDER_ECODE_OK && err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)
  {
    puts("Recording Error!");
    goto stop_player;
  }
  else if (read_size > 0)
  {
    total_size += read_size;
  }

  if (total_size > recoding_size)
  {
    goto stop_player;
  }
#endif

#if 1
  err = thePlayer->writeFrames(MediaPlayer::Player0, playFile);
  if (err == MEDIAPLAYER_ECODE_FILEEND)
  {
    puts("Main player File End!\n");
  }

  if (err)
  {
    printf("Main player error code: %d\n", err);
    goto stop_player;
  }

  if (ErrEnd)
  {
    puts("Error End\n");
    goto stop_player;
  }
#endif
  return;

stop_player:
  theRecorder->stop();
  thePlayer->stop(MediaPlayer::Player0);
  execute_frames();

  sleep(1); /* For data pipline stop */
  theRecorder->writeWavHeader(recFile);
  puts("Update Header!");
  playFile.close();
  recFile.close();

  thePlayer->deactivate(MediaPlayer::Player0);
  theRecorder->deactivate();
  thePlayer->end();
  theRecorder->end();
  puts("End Recording");
  exit(1);
}
