#include <string.h>
#include <SDHCI.h>
#include <MediaRecorder.h>
#include <FrontEnd.h>
#include <MediaPlayer.h>
#include <OutputMixer.h>
#include <MemoryUtil.h>
#include <GNSS.h>

#define RECORD_FILE_NAME "Recorde.wav"
#define ANALOG_MIC_GAIN  0 /* +0dB */
#define STRING_BUFFER_SIZE  128       /**< %Buffer size for GNSS */
#define RESTART_CYCLE       (60 * 5)  /**< positioning test term */
SDClass theSD;

MediaRecorder *theRecorder;
MediaPlayer *thePlayer;
OutputMixer *theMixer;
FrontEnd *theFrontEnd;

File playFile;
File recFile;

bool ErrEnd = false;
bool AudioEnd = false;
bool skip_stoporder = false;

static const uint32_t recoding_sampling_rate = 48000;
static const uint8_t  recoding_cannel_number = 1;
static const uint8_t  recoding_bit_length = 16;
static const uint32_t recoding_time = 1;
static const int32_t recoding_byte_per_second = recoding_sampling_rate *
                                                recoding_cannel_number *
                                                recoding_bit_length / 8;
static const int32_t recoding_size = recoding_byte_per_second * recoding_time;
static const uint32_t frame_size  = 768 * recoding_cannel_number * (recoding_bit_length / 8);
static const uint32_t buffer_size = (frame_size + 511) & ~511;
static uint8_t        s_buffer[2 * buffer_size];

static SpGnss Gnss;

static void attention_cb(const ErrorAttentionParam *atprm)
{
  puts("Attention!");

  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING)
  {
    ErrEnd = true;
  }
}

static void mediarecorder_attention_cb(const ErrorAttentionParam *atprm)
{
  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING)
  {
    ErrEnd = true;
  }
}

static void outputmixer_done_callback(MsgQueId requester_dtq,
                                      MsgType reply_of,
                                      AsOutputMixDoneParam *done_param)
{
  return;
}

static void outmixer_send_callback(int32_t identifier, bool is_end)
{
  AsRequestNextParam next;
  next.type = (!is_end) ? AsNextNormalRequest : AsNextStopResRequest;
  AS_RequestNextPlayerProcess(AS_PLAYER_ID_0, &next);
  return;
}

static bool mediaplayer_done_callback(AsPlayerEvent event, uint32_t result, uint32_t sub_result)
{
  return true;
}

void mediaplayer_decode_callback(AsPcmDataParam pcm_param)
{
  theMixer->sendData(OutputMixer0,
                     outmixer_send_callback,
                     pcm_param);
}

static bool mediarecorder_done_callback(AsRecorderEvent event, uint32_t result, uint32_t sub_result)
{
  UNUSED(event);
  UNUSED(result);
  UNUSED(sub_result);
  return true;
}


void setup()
{
  int error_flag = 0;
  Serial.begin(115200);
  while (!theSD.begin()) {};
  Gnss.setDebugMode(PrintInfo);

  /* start GNSS system */
  int result;
  result = Gnss.begin();
  if (result != 0) {
    puts("Gnss begin error!!");
    error_flag = 1;
  } else {
    Gnss.select(GPS);
    Gnss.select(QZ_L1CA);
    /* Start positioning */
    result = Gnss.start(COLD_START);
    if (result != 0) {
      puts("Gnss start error!!"); error_flag = 1;
    } else {
      puts("Gnss setup OK");
    }
  }
  if (error_flag == 1) {
    exit(0);
  }
  
  initMemoryPools();
  createStaticPools(MEM_LAYOUT_RECORDINGPLAYER);

  /* start audio system */
  thePlayer = MediaPlayer::getInstance();
  theMixer  = OutputMixer::getInstance();
  theRecorder = MediaRecorder::getInstance();

  thePlayer->begin();
  theMixer->begin();
  theRecorder->begin(mediarecorder_attention_cb);

  puts("initialization Audio Library");

  thePlayer->create(MediaPlayer::Player0, attention_cb);
  theMixer->create(attention_cb);

  /* Set rendering clock */
  theMixer->setRenderingClkMode(OUTPUTMIXER_RNDCLK_NORMAL);
  theRecorder->setCapturingClkMode(MEDIARECORDER_CAPCLK_NORMAL);

  /* Activate Player Object */
  thePlayer->activate(MediaPlayer::Player0, mediaplayer_done_callback);
  theMixer->activate(OutputMixer0, HPOutputDevice, outputmixer_done_callback);
  theRecorder->activate(AS_SETRECDR_STS_INPUTDEVICE_MIC, mediarecorder_done_callback);

  usleep(100 * 1000);

  thePlayer->init(MediaPlayer::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_44100, AS_CHANNEL_STEREO);
  theRecorder->init(AS_CODECTYPE_WAV,
                    recoding_cannel_number,
                    recoding_sampling_rate,
                    recoding_bit_length,
                    AS_BITRATE_8000,
                    "/mnt/sd0/BIN");

  while (!theSD.begin()) {
    puts("Insert SD card.");
  }
  if (theSD.exists(RECORD_FILE_NAME)){
      printf("Remove existing file [%s].\n", RECORD_FILE_NAME);
      theSD.remove(RECORD_FILE_NAME);
  }
  playFile = theSD.open("AUDIO/Sound.mp3");
  recFile  = theSD.open(RECORD_FILE_NAME, FILE_WRITE);

  /* Verify file open */
  if (!playFile)
  {
    puts("File open error\n");
    exit(1);
  }
  puts("Open!  recFile\n");

  /* Verify file open */
  if (!recFile)
  {
    puts("File open error\n");
    exit(1);
  }
  puts("Open! recFile\n");

  /* Send first frames to be decoded */
  err_t err = thePlayer->writeFrames(MediaPlayer::Player0, playFile);

  if (err != MEDIAPLAYER_ECODE_OK)
  {
    puts("File Read Error! = ");  Serial.println(String(err));
    playFile.close();
    exit(1);
  }

  puts("Play!");

  usleep(1000);
  /* Main volume set to -16.0 dB, Main player and sub player set to 0 dB */
  theMixer->setVolume(-100, 0, 0);

  theRecorder->writeWavHeader(recFile);
  puts("Update Header!");

  // Start Player
  thePlayer->start(MediaPlayer::Player0, mediaplayer_decode_callback);

  /* Set Gain */
  theRecorder->setMicGain(ANALOG_MIC_GAIN);

  /* Start Recorder */
  theRecorder->start();
}

err_t execute_aframe(uint32_t* size)
{
  err_t err = theRecorder->readFrames(s_buffer, buffer_size, size);

  if (((err == MEDIARECORDER_ECODE_OK) || (err == MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)) && (*size > 0))
  {
  } else {
    return err;
  }
  int ret = recFile.write((uint8_t*)&s_buffer, *size);
  if (ret < 0)
  {
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
    if ((err != MEDIARECORDER_ECODE_OK)
        && (err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA))
    {
      break;
    }
  }
  while (read_size > 0);
}

static void print_pos(SpNavData *pNavData)
{
  char StringBuffer[STRING_BUFFER_SIZE];

  /* print time */
  snprintf(StringBuffer, STRING_BUFFER_SIZE, "%04d/%02d/%02d ", pNavData->time.year, pNavData->time.month, pNavData->time.day);
  Serial.print(StringBuffer);

  snprintf(StringBuffer, STRING_BUFFER_SIZE, "%02d:%02d:%02d.%06ld, ", pNavData->time.hour, pNavData->time.minute, pNavData->time.sec, pNavData->time.usec);
  Serial.print(StringBuffer);

  /* print satellites count */
  snprintf(StringBuffer, STRING_BUFFER_SIZE, "numSat:%2d, ", pNavData->numSatellites);
  Serial.print(StringBuffer);

  /* print position data */
  if (pNavData->posFixMode == FixInvalid) {
    puts("No-Fix, ");
  } else {
    puts("Fix, ");
  }
  if (pNavData->posDataExist == 0) {
    puts("No Position");
  } else {
    puts("Lat=");
    Serial.print(pNavData->latitude, 6);
    puts(", Lon=");
    Serial.print(pNavData->longitude, 6);
  }
}

static void print_condition(SpNavData *pNavData)
{
  char StringBuffer[STRING_BUFFER_SIZE];
  unsigned long cnt;

  /* Print satellite count. */
  snprintf(StringBuffer, STRING_BUFFER_SIZE, "numSatellites:%2d\n", pNavData->numSatellites);
  Serial.print(StringBuffer);

  for (cnt = 0; cnt < pNavData->numSatellites; cnt++) {
    const char *pType = "---";
    SpSatelliteType sattype = pNavData->getSatelliteType(cnt);
    switch (sattype) {
      case GPS:
        pType = "GPS";
        break;
      case QZ_L1CA:
        pType = "QCA";
        break;
      default:
        pType = "UKN";
        break;
    }

    /* Get print conditions. */
    unsigned long Id  = pNavData->getSatelliteId(cnt);
    unsigned long Elv = pNavData->getSatelliteElevation(cnt);
    unsigned long Azm = pNavData->getSatelliteAzimuth(cnt);
    float sigLevel = pNavData->getSatelliteSignalLevel(cnt);

    /* Print satellite condition. */
    snprintf(StringBuffer, STRING_BUFFER_SIZE, "[%2ld] Type:%s, Id:%2ld, Elv:%2ld, Azm:%3ld, CN0:", cnt, pType, Id, Elv, Azm );
    Serial.print(StringBuffer);
    Serial.println(sigLevel, 6);
  }
}

void loop()
{
  static int32_t total_size = 0;
  uint32_t read_size = 0;
  err_t err;

  if(AudioEnd){
    skip_stoporder = true;
    goto stop_player;
  }

#if 1
  /* Execute audio data */
  if(!AudioEnd){
    err = execute_aframe(&read_size);
    if (err != MEDIARECORDER_ECODE_OK && err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)
    {
      puts("Recording Error!");
      AudioEnd = true;
      goto stop_player;
    }
    else if (read_size > 0)
    {
      total_size += read_size;
    }
  
    /* Stop Recording */
    if (total_size > recoding_size)
    {
      AudioEnd = true;
      goto stop_player;
    }
  }
#endif

  /* Send new frames to decode in a loop until file ends */
  if(!AudioEnd){
    err = thePlayer->writeFrames(MediaPlayer::Player0, playFile);
  }
#if 0
  if(!AudioEnd){
    /*  Tell when player file ends */
    if (err == MEDIAPLAYER_ECODE_FILEEND)
    {
      puts("Main player File End!\n");
    }
  
    /* Show error code from player and stop */
    if (err)
    {
      puts("Main player error code: ");  Serial.println(String(err));
      AudioEnd = true;
      goto stop_player;
      
    }
  
    if (ErrEnd)
    {
      puts("Error End\n");
      AudioEnd = true;
      goto stop_player;
    }
  }
#endif

  /* Don't go further and continue play */
  return;

stop_player:
  if(!skip_stoporder){
    theRecorder->stop();
    thePlayer->stop(MediaPlayer::Player0);
    execute_frames();
    theRecorder->writeWavHeader(recFile);
    puts("Update Header!");
  
    /* Get ramaining data(flushing) */
    sleep(1); /* For data pipline stop */
    playFile.close();
    recFile.close();
  
    thePlayer->deactivate(MediaPlayer::Player0);
    theRecorder->deactivate();
    thePlayer->end();
    theRecorder->end();
    puts("End Recording");
  }

  static int LastPrintMin = 0;

  /* Check update. */
  if (Gnss.waitUpdate(-1))
  {
    /* Get NaviData. */
    SpNavData NavData;
    Gnss.getNavData(&NavData);

    /* Set posfix LED. */
    bool LedSet = (NavData.posDataExist && (NavData.posFixMode != FixInvalid));
    /* Print satellite information every minute. */
    if (NavData.time.minute != LastPrintMin){
      print_condition(&NavData);
      LastPrintMin = NavData.time.minute;
    }
    print_pos(&NavData);
  }else{
    puts("data not update");
  }

}
