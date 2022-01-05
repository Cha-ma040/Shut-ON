#include <string.h>
#include <SDHCI.h>
#include <MediaRecorder.h>
#include <FrontEnd.h>
#include <MediaPlayer.h>
#include <OutputMixer.h>
#include <MemoryUtil.h>
//#include <GNSS.h>

#define RECORD_FILE_NAME "Recorde.wav"
#define ANALOG_MIC_GAIN  0 /* +0dB */
SDClass theSD;

MediaRecorder *theRecorder;
MediaPlayer *thePlayer;
OutputMixer *theMixer;
FrontEnd *theFrontEnd;

File playFile;
File recFile;

bool ErrEnd = false;

//static const uint32_t recoding_bitrate = 48000 * 8;
//static const uint32_t recoding_sampling_rate = 48000;
//static const uint8_t  recoding_cannel_number = 1;
//static const uint8_t  recoding_bit_length = 16;
//static const uint32_t recoding_time = 1;
//static const int32_t  recoding_byte_per_second = (recoding_bitrate / 8);
//static const int32_t  recoding_size = recoding_byte_per_second * recoding_time;
//static const uint32_t frame_size  = 768 * recoding_cannel_number * (recoding_bit_length / 8);
//static const uint32_t buffer_size = (frame_size + 511) & ~511;
//static uint8_t        s_buffer[buffer_size];

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
static uint8_t        s_buffer[buffer_size];

//追加プログラム(GPS)
//static SpGnss Gnss;
//struct pos {
//  int posDataExist;//位置情報が取得できたかどうか
//  double latitude;//緯度
//  double longitude;//経度
//} position;

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

/**
   @brief Mixer data send callback procedure

   @param [in] identifier   Device identifier
   @param [in] is_end       For normal request give false, for stop request give true
*/
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
  Serial.begin(115200);
  while (!theSD.begin()) {};

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
  if (theSD.exists(RECORD_FILE_NAME))
    {
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

  //追加プログラム
//  int result;
//  result = Gnss.begin();
//  if (result != 0)
//  {
//    puts("Gnss begin error!!");
//    exit(1);
//  }

  //利用する衛星を選択(日本では以下の３つが利用できる)
//  Gnss.select(GPS);
//  Gnss.select(QZ_L1CA);
//  Gnss.select(QZ_L1S);
//
//  //GPSによる位置情報の取得を開始
//  result = Gnss.start(COLD_START);
//  if (result != 0)
//  {
//    puts("Gnss start error!!");
//    exit(1);
//  }
//  else
//  {
//    puts("Gnss setup OK");
//  }

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

/**
   @brief Execute one frame
*/
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

/**
   @brief Execute frames for FIFO empty
*/
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

/**
   @brief Play audio frames until file ends
*/
void loop()
{
  static int32_t total_size = 0;
  uint32_t read_size = 0;
  err_t err;

#if 1
  /* Execute audio data */
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

  /* Stop Recording */
  if (total_size > recoding_size)
  {
    goto stop_player;
  }
#endif


  /* Send new frames to decode in a loop until file ends */
  err = thePlayer->writeFrames(MediaPlayer::Player0, playFile);
#if 0
  /*  Tell when player file ends */
  if (err == MEDIAPLAYER_ECODE_FILEEND)
  {
    puts("Main player File End!\n");
  }

  /* Show error code from player and stop */
  if (err)
  {
    puts("Main player error code: ");  Serial.println(String(err));
    goto stop_player;
  }

  if (ErrEnd)
  {
    puts("Error End\n");
    goto stop_player;
  }
#endif

 //GPSのデータが更新されていたら
//  if (Gnss.isUpdate() == 1)
//  {
//    //GPSデータを取得
//    SpNavData NavData;
//    Gnss.getNavData(&NavData);
//
//    //取得した位置情報を構造体変数positionにコピー
//    position.posDataExist = NavData.posDataExist;  //位置情報が取得できたか
//    position.latitude = NavData.latitude;          //緯度
//    position.longitude = NavData.longitude;        //経度
//
//    //位置情報が取得できていたらサブコアに通知＆デバグ用に出力
//    if (position.posDataExist != 0)
//    {
//      //MP.Send(2, (void*)&position, 1);    //メッセージIDは２
//      Serial.print("pos:    ");
//      Serial.print(String(position.latitude));//%3.5f
//      Serial.print(",");
//      Serial.println(String(position.longitude));//%3.5f
//    }
//  }

  /* Don't go further and continue play */
  return;

stop_player:
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
  exit(1);

}
