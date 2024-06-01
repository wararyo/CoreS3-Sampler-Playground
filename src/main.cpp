#include <M5Unified.h>
#include <SamplerBase.h>
#include <SamplerOptimized.h>
#include <piano.h>
#include <sine.h>

#define ENABLE_PRINTING false

int16_t *piano_sample_psram1;
int16_t *piano_sample_psram2;
int16_t *piano_sample_psram3;
int16_t *sine_sample_sram;
struct Sample piano1;
struct Sample piano2;
struct Sample piano3;
struct Sample sine;

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
  static int16_t prev_yh[320][2];
  {
    static int max_y = 0;
    static int min_y = 1024;
    static int x = 0;
    static int y1 = 0;
    int dh = M5.Display.height();
    for (uint_fast16_t i = 0; i < SAMPLE_BUFFER_SIZE; i++)
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
  // M5.Speakerに渡すバッファとして4つ用意する。(3個あればよいが循環処理をしやすくするため4個とした)
  int16_t output[4][SAMPLE_BUFFER_SIZE] = {0};
  uint8_t buf_idx = 0;

  uint32_t cycle_count = 0;

  piano1 = Sample{
      piano_sample_psram1, 32000, 60,
      26253, 26436,
      true, 1.0f, 0.998887f, 0.1f, 0.988885f};
  piano2 = Sample{
      piano_sample_psram1, 32000, 60,
      26253, 26436,
      true, 1.0f, 0.998887f, 0.1f, 0.988885f};
  piano3 = Sample{
      piano_sample_psram1, 32000, 60,
      26253, 26436,
      true, 1.0f, 0.998887f, 0.1f, 0.988885f};
  sine = Sample{
      sine_sample_sram, 3000, 60,
      1368, 1453,
      true, 1.0f, 0.998887f, 0.1f, 0.988885f};
  M5.Log.printf("piano1.sample  : %4x\n", piano1.sample);
  M5.Log.printf("piano2.sample  : %4x\n", piano2.sample);
  M5.Log.printf("piano3.sample  : %4x\n", piano3.sample);
  M5.Log.printf("sine.sample  : %4x\n", sine.sample);
  sampler->SetSample(0, &piano1);
  sampler->SetSample(1, &piano2);
  sampler->SetSample(2, &piano3);
  uint32_t processedSamples = 0; // 処理済みのサンプル数

  // 最初に無音を再生しておくことで先頭のノイズを抑える
  M5.Speaker.playRaw(output[buf_idx], SAMPLE_BUFFER_SIZE, SAMPLE_RATE, false, 16, SPK_CH);
  buf_idx = (buf_idx + 1) & 3;
  // 0秒時点の処理
  sampler->NoteOn(60, 127, 0); // ド
  sampler->NoteOn(64, 127, 1); // ミ
  sampler->NoteOn(67, 127, 2); // ソ
  uint32_t nextGoal = SAMPLE_RATE * 1;
  while (processedSamples < nextGoal)
  {
    cycle_count += process(sampler, output[buf_idx]);
    buf_idx = (buf_idx + 1) & 3;
    processedSamples += SAMPLE_BUFFER_SIZE;
  }

  // 1秒時点の処理
  sampler->NoteOff(60, 0, 0);
  sampler->NoteOff(64, 0, 1);
  sampler->NoteOff(67, 0, 2);
  nextGoal = SAMPLE_RATE * 2;
  while (processedSamples < nextGoal)
  {
    cycle_count += process(sampler, output[buf_idx]);
    buf_idx = (buf_idx + 1) & 3;
    processedSamples += SAMPLE_BUFFER_SIZE;
  }

  // 2秒で終了

  while (M5.Speaker.isPlaying()) {
    M5.delay(1);
  }

  // CPUサイクル数をマイクロ秒に変換して返す
  return cycle_count / getCpuFrequencyMhz();
}

void printMem()
{
  M5.Log.printf("heap_caps_get_free_size(MALLOC_CAP_SPIRAM)            : %6d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  M5.Log.printf("heap_caps_get_free_size(MALLOC_CAP_INTERNAL)          : %6d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  M5.Log.printf("heap_caps_get_free_size(MALLOC_CAP_DEFAULT)           : %6d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
  M5.Log.printf("heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)   : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  M5.Log.printf("heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  M5.Log.printf("heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)  : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
  M5.Log.println();
}

void setup()
{
  M5.begin();
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);
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

  // ピアノサンプルをPSRAMにコピー
  piano_sample_psram1 = (int16_t *)heap_caps_malloc(64000, MALLOC_CAP_SPIRAM);
  memcpy(piano_sample_psram1, piano_sample, 64000);
  piano_sample_psram2 = (int16_t *)heap_caps_malloc(64000, MALLOC_CAP_SPIRAM);
  memcpy(piano_sample_psram2, piano_sample, 64000);
  piano_sample_psram3 = (int16_t *)heap_caps_malloc(64000, MALLOC_CAP_SPIRAM);
  memcpy(piano_sample_psram3, piano_sample, 64000);

  // サイン派サンプルをPSRAMにコピー
  sine_sample_sram = (int16_t *)heap_caps_malloc(6000, MALLOC_CAP_INTERNAL);
  memcpy(sine_sample_sram, sine_sample, 6000);

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
    printMem();
    M5.Log.printf("piano_sample  : %4x\n", piano_sample);
    M5.Log.printf("piano_sample_psram1  : %4x\n", piano_sample_psram1);
    M5.Log.printf("piano_sample_psram1[0]  : %2x\n", piano_sample_psram1[0]);
    M5.Log.printf("sine_sample  : %4x\n", sine_sample);
    M5.Log.printf("sine_sample_sram  : %4x\n", sine_sample_sram);
    M5.Log.printf("sine_sample_sram[0]  : %2x\n", sine_sample_sram[0]);
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
