static uint32_t analyzeStartHz   = 0;
static uint32_t analyzeEndHz     = 0;
static uint32_t analyzeStepHz    = 0;
static uint32_t analyzePrevFreq  = 433000000;
static uint32_t lastAnalyzeFrame = 0;
static bool     analyzeNeedsClear = false;
static bool     analyzePeakHold = true;
static int16_t  analyzeThresholdDbm = -95;
static int16_t  analyzeHoldPeakRssi = -200;
static uint32_t analyzeHoldPeakFreq = 0;
static const uint16_t ANALYZE_MAX_BINS = 96;

bool applyAnalyzePreset(const char* preset, uint32_t& startHz, uint32_t& endHz, uint32_t& stepHz) 
{
  if (strcasecmp(preset, "433") == 0) 
  {
    startHz = 430000000UL; 
    endHz = 440000000UL; 
    stepHz = 50000UL; 
    return true;
  }
  if (strcasecmp(preset, "868") == 0) 
  {
    startHz = 863000000UL; 
    endHz = 870000000UL; 
    stepHz = 50000UL; 
    return true;  
  }
  if (strcasecmp(preset, "915") == 0) 
  {
    startHz = 902000000UL; 
    endHz = 928000000UL; 
    stepHz = 200000UL; 
    return true;  
  }
  return false;
}

bool parseAnalyzeArgs(const char* arg, uint32_t& startHz, uint32_t& endHz, uint32_t& stepHz) 
{
  if (*arg == '\0') 
  {
    uint32_t center = radio.getConfig().frequencyHz;
    startHz = (center > 1000000UL) ? (center - 1000000UL) : 150000000UL;
    endHz = center + 1000000UL;
    if (startHz < 150000000UL) startHz = 150000000UL;
    if (endHz > 960000000UL) endHz = 960000000UL;
    stepHz = 25000UL;
    return true;
  }
  if (applyAnalyzePreset(arg, startHz, endHz, stepHz)) return true;
  char* p1 = nullptr;
  float startMHz = strtof(arg, &p1);
  while (*p1 == ' ') p1++;
  char* p2 = nullptr;
  float endMHz = strtof(p1, &p2);
  while (*p2 == ' ') p2++;
  float stepKHz = 25.0f;
  if (*p2 != '\0') stepKHz = strtof(p2, nullptr);
  if (startMHz < 150.0f || endMHz > 960.0f || startMHz >= endMHz || stepKHz < 1.0f) return false;
  startHz = (uint32_t)(startMHz * 1e6f);
  endHz = (uint32_t)(endMHz * 1e6f);
  stepHz = (uint32_t)(stepKHz * 1e3f);
  return (stepHz > 0);
}

void renderAnalyzeFrame(void) 
{
  const uint16_t maxBins = ANALYZE_MAX_BINS;
  if (analyzeEndHz <= analyzeStartHz || analyzeStepHz == 0) return;
  uint32_t spanHz = analyzeEndHz - analyzeStartHz;
  uint32_t requestedBins = (spanHz / analyzeStepHz) + 1;
  uint32_t effectiveStepHz = analyzeStepHz;
  uint16_t bins = (requestedBins > maxBins) ? maxBins : (uint16_t)requestedBins;
  if (requestedBins > maxBins) 
  {
    effectiveStepHz = spanHz / (maxBins - 1);
    if (effectiveStepHz == 0) effectiveStepHz = 1;
  }
  static int16_t rssiBins[ANALYZE_MAX_BINS];
  static uint32_t freqBins[ANALYZE_MAX_BINS];
  int16_t peakRssi = -200;
  uint32_t peakFreq = analyzeStartHz;
  uint16_t hits = 0;
  for (uint16_t i = 0; i < bins; i++) 
  {
    uint32_t freq = analyzeStartHz + (uint32_t)i * effectiveStepHz;
    if (freq > analyzeEndHz) freq = analyzeEndHz;

    radio.setFrequency(freq);
    radio.startReceive();
    delayMicroseconds(800);

    int16_t rssi = radio.readRssi();
    radio.goStandby();

    rssiBins[i] = rssi;
    freqBins[i] = freq;
    if (rssi > peakRssi) 
    {
        peakRssi = rssi;
        peakFreq = freq;
    }
    if (rssi >= analyzeThresholdDbm) hits++;
  }
  if (analyzePeakHold) 
  {
    if (peakRssi > analyzeHoldPeakRssi) 
    {
      analyzeHoldPeakRssi = peakRssi;
      analyzeHoldPeakFreq = peakFreq;
    }
  } 
  else 
  {
    analyzeHoldPeakRssi = peakRssi;
    analyzeHoldPeakFreq = peakFreq;
  }
  if (analyzeNeedsClear) 
  {
    Serial1.print("\033[2J\033[H");
    analyzeNeedsClear = false;
  } 
  else Serial1.print("\033[H");
  Serial1.print("analyze ");
  Serial1.print(analyzeStartHz / 1e6f, 3);
  Serial1.print("-");
  Serial1.print(analyzeEndHz / 1e6f, 3);
  Serial1.print("MHz step ");
  Serial1.print(effectiveStepHz / 1e3f, 1);
  Serial1.print("k thr ");
  Serial1.print(analyzeThresholdDbm);
  Serial1.print(" p ");
  Serial1.print(peakFreq / 1e6f, 3);
  Serial1.print("@");
  Serial1.print(peakRssi);
  Serial1.print(" h ");
  Serial1.print(analyzeHoldPeakFreq / 1e6f, 3);
  Serial1.print("@");
  Serial1.print(analyzeHoldPeakRssi);
  Serial1.print(" hits ");
  Serial1.println(hits);
  for (uint16_t i = 0; i < bins; i++) 
  {
    Serial1.print(i);
    Serial1.print(": ");
    Serial1.print(freqBins[i] / 1e6f, 3);
    Serial1.print(" ");
    if (rssiBins[i] > -100) Serial1.print(' ');
    if (rssiBins[i] > -10) Serial1.print(' ');
    Serial1.print(rssiBins[i]);
    Serial1.print(" ");
    if (rssiBins[i] >= analyzeThresholdDbm) Serial1.print("*");
    if (freqBins[i] == peakFreq) Serial1.print("P");
    if (freqBins[i] == analyzeHoldPeakFreq) Serial1.print("H");
    Serial1.println();
  }
}
