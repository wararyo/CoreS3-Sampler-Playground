#include <M5Unified.h>
#include <SamplerBase.h>
#include <SamplerOptimized.h>
#include <piano.h>

#define ENABLE_PRINTING false

static struct Sample piano = Sample{
    piano_sample,
    32000,
    60,
    26253,
    26436,
    true,
    1.0f,
    0.998887f,
    0.1f,
    0.988885f};

static constexpr const uint8_t SPK_CH = 1;

uint32_t process(SamplerBase *sampler, int16_t *output)
{
  uint32_t cycle_begin, cycle_end;
  __asm__ __volatile("rsr %0, ccount" : "=r"(cycle_begin) ); // 処理前のCPUサイクル値を取得
  sampler->Process(output);
  __asm__ __volatile("rsr %0, ccount" : "=r"(cycle_end) ); // 処理後のCPUサイクル値を取得
  uint32_t cycle = cycle_end - cycle_begin;
#if ENABLE_PRINTING
  for (uint8_t i; i < SAMPLE_BUFFER_SIZE; i++)
  {
    Serial.printf("%d,", output[i]);
  }
#else
  M5.Speaker.playRaw(output, SAMPLE_BUFFER_SIZE, SAMPLE_RATE, false, 1, SPK_CH);
  static int16_t prev_yh[320][2];
  {
    static int max_y = 0;
    static int min_y = 1024;
    static int x = 0;
    static int y1 = 0;
    int dh = M5.Display.height();
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
    {
      M5.Display.writeFastVLine(x, prev_yh[x][0], prev_yh[x][1], TFT_BLACK);
      int y0 = (dh >> 1) - (output[i] >> 7);
      if (min_y > y0) { min_y = y0; }
      if (max_y < y0) { max_y = y0; }
      int y = y0 < y1 ? y0 : y1;
      int h = y0 < y1 ? y1 : y0;
      y1 = y0;
      h = h - y + 1;
      M5.Display.writeFastVLine(x, y, h, TFT_WHITE);
      prev_yh[x][0] = y;
      prev_yh[x][1] = h;
      ++x;
      if (x >= M5.Display.width()) { x = 0; }
    }
  }
#endif
  // 波形合成処理に掛かったCPUサイクル数を返す
  return cycle;
}

uint32_t benchmark(SamplerBase *sampler)
{
  int16_t output[2][SAMPLE_BUFFER_SIZE] = {0};
  bool buf_idx = false;

  uint32_t cycle_count = 0;

  sampler->SetSample(0, &piano);
  uint32_t processedSamples = 0; // 処理済みのサンプル数

  M5.Speaker.playRaw(output[buf_idx], SAMPLE_BUFFER_SIZE, SAMPLE_RATE, false, 1, SPK_CH);
  buf_idx = !buf_idx;
  // 0秒時点の処理
  sampler->NoteOn(60, 127, 0); // ド
  sampler->NoteOn(64, 127, 0); // ミ
  sampler->NoteOn(67, 127, 0); // ソ
  uint32_t nextGoal = SAMPLE_RATE * 1;
  while (processedSamples < nextGoal)
  {
    cycle_count += process(sampler, output[buf_idx]);
    buf_idx = !buf_idx;
    processedSamples += SAMPLE_BUFFER_SIZE;
  }

  // 1秒時点の処理
  sampler->NoteOff(60, 0, 0);
  sampler->NoteOff(64, 0, 0);
  sampler->NoteOff(67, 0, 0);
  nextGoal = SAMPLE_RATE * 2;
  while (processedSamples < nextGoal)
  {
    cycle_count += process(sampler, output[buf_idx]);
    buf_idx = !buf_idx;
    processedSamples += SAMPLE_BUFFER_SIZE;
  }

  // 2秒で終了

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
    auto spk_cfg = M5.Speaker.config();
    spk_cfg.sample_rate = 48000;
    spk_cfg.task_pinned_core = PRO_CPU_NUM;
    spk_cfg.dma_buf_len = 64;
    spk_cfg.dma_buf_count = 16;

    M5.Speaker.config(spk_cfg);
    M5.Speaker.setVolume(192);
  }
  M5.Display.startWrite();
  // M5.Display.setRotation(M5.Display.getRotation() ^ 1);
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
