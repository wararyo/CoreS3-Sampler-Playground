#include <M5Unified.h>
#include <SamplerBase.h>
#include <SamplerOptimized.h>

#include <piano.h>
#include <bass.h>

// 再生したい曲によってどちらか一方をインポートする
// #include <song/simple.h>
#include <song/neko.h>

#define ENABLE_PRINTING false

static struct Sample piano = Sample{
    piano_sample, 24000, 60,
    21608, 21975,
    true, 1.0f, 0.998000f, 0.1f, 0.985000f};
static struct Sample bass = Sample{
    bass_sample, 24000, 36,
    21714, 22448,
    true, 1.0f, 0.999000f, 0.25f, 0.970000f};

static constexpr const uint8_t SPK_CH = 1;

uint32_t process(SamplerBase *sampler, int16_t *output)
{
  uint32_t cycle_begin, cycle_end;
  __asm__ __volatile("rsr %0, ccount" : "=r"(cycle_begin) ); // 処理前のCPUサイクル値を取得
  sampler->Process(output);
  __asm__ __volatile("rsr %0, ccount" : "=r"(cycle_end) ); // 処理後のCPUサイクル値を取得
  uint32_t cycle = cycle_end - cycle_begin;
#if ENABLE_PRINTING
  for (uint_fast16_t i; i < SAMPLE_BUFFER_SIZE; i++)
  {
    Serial.printf("%d,", output[i]);
  }
#else
  M5.Speaker.playRaw(output, SAMPLE_BUFFER_SIZE, SAMPLE_RATE, false, 1, SPK_CH);
  static int16_t prev_y[320];
  {
    static int x = 0;
    int dh = M5.Display.height();
    for (uint_fast16_t i = 0; i < SAMPLE_BUFFER_SIZE; i++)
    {
      int y = (dh >> 1) - (output[i] >> 7);
      int py = prev_y[x];
      if (py != y) {
        M5.Display.writeFastVLine(x, py, 2, TFT_BLACK);
        M5.Display.writeFastVLine(x, y, 2, TFT_WHITE);
        prev_y[x] = y;
      }
      if (++x >= M5.Display.width()) { x = 0; }
    }
  }
#endif
  // 波形合成処理に掛かったCPUサイクル数を返す
  return cycle;
}

uint32_t benchmark(SamplerBase *sampler)
{
  // M5.Speakerに渡すバッファとして4つ用意する。(3個あればよいが循環処理をしやすくするため4個とした)
  int16_t output[4][SAMPLE_BUFFER_SIZE] = {0};
  uint8_t buf_idx = 0;

  uint32_t cycle_count = 0;

  sampler->SetSample(0, &piano);
  sampler->SetSample(1, &bass);

  // 最初に無音を再生しておくことで先頭のノイズを抑える
  M5.Speaker.playRaw(output[buf_idx], SAMPLE_BUFFER_SIZE, SAMPLE_RATE, false, 16, SPK_CH);
  buf_idx = (buf_idx + 1) & 3;

  uint32_t processedSamples = 0;   // 処理済みのサンプル数
  const MidiMessage *nextMessage = song; // 次に処理するべきMIDIメッセージ
  uint32_t nextGoal = nextMessage->time;
  bool hasReachedEndOfSong = false; // 多重ループを抜けるために使用
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

  while (M5.Speaker.isPlaying()) {
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
  if (touch.wasClicked() && touch.base_y < M5.Display.height())
  {
    M5.Display.clear();
    M5.Display.setCursor(0,0);
    M5.Display.println("Processing...");

    SamplerOptimized sampler = SamplerOptimized();
    time_t elapsedTime = benchmark(&sampler);
    
#if ENABLE_PRINTING
    M5.Display.println("Processed.");
#else
    M5.Display.printf("Elapsed time: %ld us\n", elapsedTime);
    M5.Log.printf("Elapsed time: %ld us\n", elapsedTime);
#endif
  }
  delay(100);
}
