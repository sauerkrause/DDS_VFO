#include <EEPROM.h>

void setup() {
  // put your setup code here, to run once:
  unsigned long band_edges[] = {
    1800000, 2000000,
    3500000, 4000000,
    5330500, 5403500,
    7000000, 7300000,
    10100000, 10150000,
    14000000, 14350000,
    18068000, 18168000,
    21000000, 21450000,
    24890000, 24990000,
    28000000, 29700000
  };
  for(size_t i = 0; i < (sizeof(band_edges) >> 2); ++i) {
    EEPROM.put(i << 2, band_edges[i]);
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
