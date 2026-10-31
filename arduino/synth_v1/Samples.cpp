#include "SampleSet.h"
#include "SampleData.h"  // generated; included only here so flash holds one copy

namespace samples {

uint32_t rate() {
  return kSampleDataRate;
}

uint8_t setCount() {
  return sizeof(kSampleSets) / sizeof(kSampleSets[0]);
}

const SampleZone &zoneFor(uint8_t set, uint8_t note) {
  const SampleSet &s = kSampleSets[set % setCount()];
  for (uint8_t i = 0; i < s.zoneCount; ++i) {
    if (note >= s.zones[i].lowKey && note <= s.zones[i].highKey) {
      return s.zones[i];
    }
  }
  return note < s.zones[0].lowKey ? s.zones[0] : s.zones[s.zoneCount - 1];
}

}  // namespace samples
