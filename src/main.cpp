#include <M5Unified.h>
#include <SamplerBase.h>
#include <SamplerLegacy.h>
#include <piano.h>

#define ENABLE_PRINTING true

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

inline void process(SamplerBase *sampler, int16_t *output)
{
  sampler->Process(output);
#if ENABLE_PRINTING
  for (uint8_t i; i < SAMPLE_BUFFER_SIZE; i++)
  {
    Serial.printf("%d,", output[i]);
  }
#endif
}

time_t benchmark(SamplerBase *sampler)
{
  unsigned long startTime = micros();

  sampler->SetSample(0, &piano);
  int16_t output[SAMPLE_BUFFER_SIZE] = {0};
  uint32_t processedSamples = 0; // 処理済みのサンプル数

  // 0秒時点の処理
  sampler->NoteOn(60, 127, 0); // ド
  sampler->NoteOn(64, 127, 0); // ミ
  sampler->NoteOn(67, 127, 0); // ソ
  uint32_t nextGoal = SAMPLE_RATE * 1;
  while (processedSamples < nextGoal)
  {
    process(sampler, output);
    processedSamples += SAMPLE_BUFFER_SIZE;
  }

  // 1秒時点の処理
  sampler->NoteOff(60, 0, 0);
  sampler->NoteOff(64, 0, 0);
  sampler->NoteOff(67, 0, 0);
  nextGoal = SAMPLE_RATE * 2;
  while (processedSamples < nextGoal)
  {
    process(sampler, output);
    processedSamples += SAMPLE_BUFFER_SIZE;
  }

  // 2秒で終了

  unsigned long endTime = micros();
  return endTime - startTime;
}

void setup()
{
  M5.begin();
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
    M5.Display.println("Processing...");
    SamplerLegacy samplerLegacy = SamplerLegacy();
    time_t elapsedTime = benchmark(&samplerLegacy);
#if ENABLE_PRINTING
    M5.Display.println("Processed.");
#else
    M5.Display.printf("Elapsed time: %ld us\n", elapsedTime);
#endif
  }
  delay(100);
}
