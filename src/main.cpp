#include <M5Unified.h>
#include <SamplerBase.h>
#include <SamplerOptimized.h>
#include <MidiMessage.h>
#include <vector>

#define ENABLE_PRINTING false

extern const MidiMessage simple_song[];
extern const MidiMessage neko_song[];
extern const MidiMessage threepiece_song[];
extern const MidiMessage future_song[];
extern const MidiMessage stresstest_song[];
extern const int16_t piano_data[24000];
extern const int16_t bass_data[24000];
extern const int16_t kick_data[12000];
extern const int16_t hihat_data[3200];
extern const int16_t snare_data[12000];
extern const int16_t crash_data[38879];
extern const int16_t supersaw_data[30000];

static struct Sample pianoSample = Sample{
    piano_data, 24000, 60,
    21608, 21975,
    true, 1.0f, 0.998000f, 0.1f, 0.985000f};
static struct Sample bassSample = Sample{
    bass_data, 24000, 36,
    21714, 22448,
    true, 1.0f, 0.999000f, 0.25f, 0.970000f};
static struct Sample kickSample = Sample{
    kick_data, 12000, 36,
    0, 0,
    false, 0, 0, 0, 0};
static struct Sample hihatSample = Sample{
    hihat_data, 3200, 42,
    0, 0,
    false, 0, 0, 0, 0};
static struct Sample snareSample = Sample{
    snare_data, 12000, 38,
    0, 0,
    false, 0, 0, 0, 0};
static struct Sample crashSample = Sample{
    crash_data, 38800, 49,
    0, 0,
    false, 0, 0, 0, 0};
static struct Sample supersawSample = Sample{
    supersaw_data, 30000, 60,
    23979, 25263,
    true, 1.0f, 0.982f, 0, 0.5f};

static Timbre piano = Timbre({{&pianoSample, 0, 127, 0, 127}});
static Timbre bass = Timbre({{&bassSample, 0, 127, 0, 127}});
static Timbre drumset = Timbre({
  {&kickSample, 36, 36, 0, 127},
  {&snareSample, 38, 38, 0, 127},
  {&hihatSample, 42, 42, 0, 127},
  {&crashSample, 49, 49, 0, 127}
});
static Timbre supersaw = Timbre({{&supersawSample, 0, 127, 0, 127}});

struct song_table_entry_t
{
  const MidiMessage *song;
  const char *name;
};

static constexpr const song_table_entry_t song_table[] = {
  { simple_song,     "simple" },
  { neko_song,       "neko" },
  { threepiece_song, "threepiece" },
  { future_song,     "future" },
  { stresstest_song, "stresstest" },
};
static constexpr const size_t song_table_count = sizeof(song_table) / sizeof(song_table[0]);

static constexpr const uint8_t SPK_CH = 1;

#if defined ( M5UNIFIED_PC_BUILD )
static int getCpuFrequencyMhz() { return 1; }
#define PRO_CPU_NUM 0
#endif

uint32_t process(SamplerBase *sampler, int16_t *output)
{
  uint32_t cycle_begin, cycle_end;
#if defined (M5UNIFIED_PC_BUILD)
  cycle_begin = M5.micros();
#else
  __asm__ __volatile("rsr %0, ccount" : "=r"(cycle_begin)); // 処理前のCPUサイクル値を取得
#endif
  sampler->Process(output);
#if defined (M5UNIFIED_PC_BUILD)
  cycle_end = M5.micros();
#else
  __asm__ __volatile("rsr %0, ccount" : "=r"(cycle_end)); // 処理後のCPUサイクル値を取得
#endif
  uint32_t cycle = cycle_end - cycle_begin;
#if ENABLE_PRINTING
  for (uint_fast16_t i; i < SAMPLE_BUFFER_SIZE; i++)
  {
    Serial.printf("%d,", output[i]);
  }
  delay(1);
#else
  // 生成した音を鳴らす
  M5.Speaker.playRaw(output, SAMPLE_BUFFER_SIZE, SAMPLE_RATE, false, 1, SPK_CH);

  static int dw = M5.Display.width();
  static int dh = M5.Display.height();
  // 波形を表示
  static int16_t prev_y[320];
  {
    static int x = 0;
    for (uint_fast16_t i = 0; i < SAMPLE_BUFFER_SIZE; i++)
    {
      int y = (dh >> 1) - (output[i] >> 7);
      int py = prev_y[x];
      if (py != y)
      {
        M5.Display.writeFastVLine(x, py, 2, TFT_BLACK);
        M5.Display.writeFastVLine(x, y, 2, TFT_WHITE);
        prev_y[x] = y;
      }
      if (++x >= M5.Display.width()) { x = 0; }
    }
  }
  // オーディオ負荷を表示
  static uint32_t prev_width = 0;
  static uint32_t realtime_cycle = SAMPLE_BUFFER_SIZE * 1000000 / SAMPLE_RATE * getCpuFrequencyMhz(); // リアルタイム処理が可能な限界値 = 負荷100%時のcycle
  {
    uint32_t width = dw * cycle / realtime_cycle;
    int32_t delta = width - prev_width;
    if (delta > 0)
      M5.Display.fillRect(prev_width, dh, delta, -10, TFT_WHITE);
    else if (delta < 0)
      M5.Display.fillRect(prev_width, dh, delta, -10, TFT_BLACK);
    prev_width = width;
  }
#endif
  // 波形合成処理に掛かったCPUサイクル数を返す
  return cycle;
}

uint32_t benchmark(SamplerBase *sampler, const MidiMessage *song)
{
  // M5.Speakerに渡すバッファとして4つ用意する。(3個あればよいが循環処理をしやすくするため4個とした)
  int16_t output[4][SAMPLE_BUFFER_SIZE] = {0};
  uint8_t buf_idx = 0;

  uint32_t cycle_count = 0;

  sampler->SetTimbre(0, &piano);
  sampler->SetTimbre(1, &bass);
  sampler->SetTimbre(2, &supersaw);
  sampler->SetTimbre(9, &drumset);

  // 最初に無音を再生しておくことで先頭のノイズを抑える
  M5.Speaker.playRaw(output[buf_idx], SAMPLE_BUFFER_SIZE, SAMPLE_RATE, false, 16, SPK_CH);
  buf_idx = (buf_idx + 1) & 3;

  uint32_t processedSamples = 0;         // 処理済みのサンプル数
  const MidiMessage *nextMessage = song; // 次に処理するべきMIDIメッセージ
  uint32_t nextGoal = nextMessage->time;
  bool hasReachedEndOfSong = false;  // 多重ループを抜けるために使用
  while (processedSamples < 2880000) // 長すぎる曲は途中で打ち切る
  {
    // MIDIメッセージを処理する
    while (processedSamples >= nextGoal)
    {
      if ((nextMessage->status & 0xF0) == 0x90)
      {
        sampler->NoteOn(nextMessage->data1, nextMessage->data2, nextMessage->status & 0x0F);
      }
      else if ((nextMessage->status & 0xF0) == 0x80)
      {
        sampler->NoteOff(nextMessage->data1, nextMessage->data2, nextMessage->status & 0x0F);
      }
      else if ((nextMessage->status & 0xF0) == 0xE0)
      {
        uint_fast16_t rawValue = (nextMessage->data2 & 0b01111111) << 7 | (nextMessage->data1 & 0b01111111);
        int16_t value = rawValue - 8192;
        sampler->PitchBend(value, nextMessage->status & 0x0F);
      }
      if (nextMessage->status == 0xFF && nextMessage->data1 == 0x2F && nextMessage->data2 == 0x00)
      {
        // 0xFF, 0x2F, 0x00 は曲の終わりを意味する
        hasReachedEndOfSong = true;
        break;
      }
      nextMessage++;
      nextGoal = nextMessage->time;
    }
    if (hasReachedEndOfSong)
      break;

    // 音声処理を進める
    while (processedSamples < nextGoal)
    {
      cycle_count += process(sampler, output[buf_idx]);
      buf_idx = (buf_idx + 1) & 3;
      processedSamples += SAMPLE_BUFFER_SIZE;
    }
  }

  while (M5.Speaker.isPlaying())
  {
    M5.delay(1);
  }

  // CPUサイクル数をマイクロ秒に変換して返す
  return cycle_count / getCpuFrequencyMhz();
}

void setup()
{
  M5.begin();
  {
    // 無駄がないようにサンプルレートとバッファ長をSamplerの処理と揃えておく
    auto spk_cfg = M5.Speaker.config();
    spk_cfg.sample_rate = SAMPLE_RATE;
    spk_cfg.task_pinned_core = PRO_CPU_NUM;
    spk_cfg.dma_buf_len = SAMPLE_BUFFER_SIZE;
    spk_cfg.dma_buf_count = 16;

    M5.Speaker.config(spk_cfg);
    M5.Speaker.setVolume(192);
  }
  M5.Display.startWrite();
  M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  M5.Display.setTextSize(2);
  M5.Display.println("Hello World!");
  M5.Display.println("");
}

void loop()
{
  M5.update();
  auto touch = M5.Touch.getDetail();

  static int32_t prev_song_index = -1;
  static int32_t song_index = 0;

  // フリックを終えたとき song_indexを更新する
  if (touch.wasFlicked()) {
    song_index = prev_song_index;
  }
  // フリック操作による曲選択
  int32_t selecting_song_index = song_index;
  if (touch.isFlicking()) {
    int32_t advance = touch.distanceY() >> 5;
    selecting_song_index += advance + (song_table_count << 8);
    selecting_song_index %= song_table_count;
  }
  // 表示を更新
  if (prev_song_index != selecting_song_index) {
    prev_song_index = selecting_song_index;
    int y = 120;
    M5.Display.fillRect(0, y, M5.Display.width(), 32, TFT_BLACK);
    M5.Display.setCursor(0, y);
    M5.Display.print(song_table[selecting_song_index].name);
  }

  if (touch.wasClicked() && touch.base_y < M5.Display.height())
  {
    prev_song_index = -1;
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
    M5.Display.println("Processing...");

    SamplerOptimized sampler = SamplerOptimized();
    time_t elapsedTime = benchmark(&sampler, song_table[song_index].song);
    
#if ENABLE_PRINTING
    M5.Display.println("Processed.");
#else
    M5.Display.printf("Elapsed time: %ld us\n", elapsedTime);
    M5.Log.printf("Elapsed time: %ld us\n", elapsedTime);
#endif
  }
  M5.delay(16);
}
