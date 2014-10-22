// Video Motion Detector using intersil EL1883 Sync Separator
//
// 2014 (c) orangkucing
//
#ifdef USE_VIDEOMOTION

// This part of the sketch exclusively uses the following pins. 
// If an other part uses one of these pins please #undef the part.
// D2 (INT0) : Vertical Sync = pin 3 of EL1883.
// D3 (INT1) : Horizontal Sync = pin 7 of EL1883.
// and
// if Teensy 3.x
//   A9/D23 (CMP1_IN0) : Analog comparator negative input. (voltage to compare)
// if GR-KURUMI (RL78/G13 has no built-in analog comparator)
//   D4 : Input from external analog comparator
//   D9 (PWM) : Constant reference PWM voltage for brightness threshold.
// if Arduino Pro Mini
//   D6 (AIN0) : Analog comparator positive input. (fixed voltage)
//   D7 (AIN1) : Analog comparator negative input. (voltage to compare)
//   D9 (PWM)  : Constant reference PWM voltage for brightness threshold.

#if defined(REL_GR_KURUMI) // GR-KURUMI

// Renesas RL78/G13 has 10 external interrupts (INTP0-INTP6, INTP8, INTP9, INTKR)
// but GR-KURUMI made all of these pins unusable except
//   INTP3 on D2 (as Arduino compatible interrupt number 0)
//   INTP5 on D3 (as Arduino compatible interrupt number 1)
//   INTP4 on D4 (not used in standard library).
// Since this code needs three interrupt pins, we assign as
//   D2 : Vertical Sync
//   D3 : Horizontal Sync
// and additional
//   D4 : Input from external analog comparator.
// In order to use D4 as an interrupt pin we need to do it low level. (sigh..)
// "How To Do It" documents about GR-KURUMI should be prepared...

#else // Arduino Pro Mini or Teensy 3.x

// The part of code utilizes the following library. Please download and install.
//   https://github.com/orangkucing/analogComp
//#include "analogComp.h" // *** please comment out this line if GR-KURUMI ***

#endif

// Bulk motion detection:
// volatile variables

volatile uint16_t interlace;
volatile uint16_t currentLine;
volatile uint16_t lastHsyncTime;
volatile boolean nosignal = true;

// Composite video signal:
//   White level = 1.0V (77/255 or 19/63)
//   Black level = 0.3V (23/255 or 5/63)

#if defined(__MK20DX256__) || defined(__MK20DX128__) // Teensy 3.x

const int SCANLINE_OFFSET = 94;
const int MAX_SCANLINES = 100;
typedef uint32_t image_t; // 1 word = 32 bits
#define SIZE_OF_IMAGE_T 4
const int WORD_PER_LINE = 2; // 2^2 words = 128 bits

// set brightnessThreshold between 5: very sensitive and 19: not sensitive.
const int brightnessThreshold = 10; // CHANGE ME!!
const image_t differenceThreshold = 2000L; // CHANGE ME!!

#elif defined(REL_GR_KURUMI) // GR-KURUMI

const int SCANLINE_OFFSET = 94;
const int MAX_SCANLINES = 100;
typedef uint32_t image_t; // 1 word = 32 bits
#define SIZE_OF_IMAGE_T 4
const int WORD_PER_LINE = 2; // 2^2 words = 128 bits

// set brightnessThreshold between 23: very sensitive and 77: not sensitive.
const int brightnessThreshold = 50; // CHANGE ME!!
const image_t differenceThreshold = 1000L; // CHANGE ME!!

#else // Arduino Pro Mini

#if F_CPU == 16000000
const int SCANLINE_OFFSET = 110;
const int MAX_SCANLINES = 70;
typedef uint16_t image_t; // 1 word = 16 bits
#define SIZE_OF_IMAGE_T 2
// overclocked Arduino Pro Mini
// GoPro needs 3.3V logic. Running 16MHz on ATmega 328P is not recommended at 3.3V.
// (Note: ATmega 328P can run 20MHz but it is not supported by Arduino IDE)
const int WORD_PER_LINE = 1; // 2^1 words = 32 bits
#else // F_CPU == 8000000
const int SCANLINE_OFFSET = 94;
const int MAX_SCANLINES = 100;
typedef uint16_t image_t; // 1 word = 16 bits
#define SIZE_OF_IMAGE_T 2
const int WORD_PER_LINE = 0; // 2^0 words = 16 bits
#endif

// set brightnessThreshold between 23: very sensitive and 77: not sensitive.
const int brightnessThreshold = 50; // CHANGE ME!!
const image_t differenceThreshold = 300; // CHANGE ME!!

#endif

volatile image_t diff, tmpdiff;
// buffer for scanning lines located at the center part.
// effective scanning lines in NTSC (PAL) = 240 * 2 (288 * 2).
volatile image_t line[1 << WORD_PER_LINE];
volatile image_t image[(MAX_SCANLINES << 1) * (1 << WORD_PER_LINE)];
volatile image_t imagediff[MAX_SCANLINES * (1 << WORD_PER_LINE)];

int VMDstatus = 0;
unsigned long epoch;

void motionEnd()
{
  VMDstatus = 4;
  Serial.println(F("stop"));
  stopRecording();
  epoch = millis();
}

void motionDetected()
{
  if (!ledState) {
    VMDstatus = 3;
    Serial.println(F("start"));
    startRecording();
  }
  epoch = millis();
}

// interrupt routines

void VSyncHandler()
{
	uint16_t i;
	image_t d;
	
  currentLine = 3; // VSync always occurs in line 4 (or 267) (base 1).
  
  // For detailed info on HSync and VSync timing, this is very good document:
  //   "MAX9568 Component Analog TV Sync Separator datasheet"
#if !defined(REL_GR_KURUMI)
  if ((uint16_t)micros() - lastHsyncTime < 32) {
#else // GR-KURUMI's micros() is too slow so we must directly read decrementing timer counter
  if (lastHsyncTime - TCR05.tcr05 < 32) {
#endif
    // beginning of an even frame
    interlace = 1;
    nosignal = false;
  } else {
    // beginning of an odd frame
    interlace = 0;
  }
  for (i = 0; i < MAX_SCANLINES * (1 << WORD_PER_LINE); i++) {
    d = imagediff[i];
    // nasty but fast bitcount
#if SIZE_OF_IMAGE_T == 4
    d = d - ((d >> 1) & 0x55555555);
    d = (d & 0x33333333) + ((d >> 2) & 0x33333333);
    d = (d + (d >> 4)) & 0x0f0f0f0f;
    d = (d + (d >> 8)) & 0x00ff00ff;
    tmpdiff += (d + (d >> 16)) & 0x0000ffff;
#elif SIZE_OF_IMAGE_T == 2
    d = d - ((d >> 1) & 0x5555);
    d = (d & 0x3333) + ((d >> 2) & 0x3333);
    d = (d + (d >> 4)) & 0x0f0f;
    tmpdiff += (d + (d >> 8)) & 0x00ff;
#else // SIZE_OF_IMAGE_T == 1
    d = d - ((d >> 1) & 0x55);
    d = (d & 0x33) + ((d >> 2) & 0x33);
    tmpdiff += (d + (d >> 4)) & 0x0f;
#endif
  }
  if (interlace == 0) {
      diff = tmpdiff;
      tmpdiff = 0;
  }
}

void HSyncHandler()
{
  uint16_t i, c, d;

#if !defined(REL_GR_KURUMI)
  lastHsyncTime = (uint16_t)micros();
#else // workaround for GR-KURUMI's very slow micros()
  lastHsyncTime = TCR05.tcr05;
#endif
  c = currentLine++ - SCANLINE_OFFSET;
  d = c;
  
  if (c < MAX_SCANLINES) {
    if (interlace) {
      c += MAX_SCANLINES;
    }
    c <<= WORD_PER_LINE; d <<= WORD_PER_LINE;
    for (i = 0; i < (1 << WORD_PER_LINE); i++) {
      imagediff[d + i] = image[c + i] ^ line[i];
      image[c + i] = line[i];
    }
  }
  memset((void *)line, 0, sizeof(line));
}

#if defined(__MK20DX256__) || defined(__MK20DX128__) // Teensy 3.x

void changeHandler()
{
  uint16_t x;
 
  x = (uint16_t)micros() - lastHsyncTime;

  // HSync period is 63.56μs and sync-to-blanking-end 9.48μs
  if (x <= 64 && x >= 10) {
    x -= 10;
    if ((CMP1_SCR & 1)) { // check CMP_SCR_COUT and store 2 bit per interrupt
      // INP > INM
      line[x >> 4] |= (image_t)1 << ((x & B1111) << 1);
    } else {
      // INP < INM
      line[x >> 4] |= (image_t)1 << (((x & B1111) << 1) | 1);
    }
  }
}

void _resetCMP()
{
  // prepare capturing video frames.
  // we use 6bit DAC1 for INP.
  analogComparator1.setOn(7, 0); // set comparator input PSEL=IN7 (6b DAC1 Reference) / MSEL=IN0 (D23)
  // set the reference analog voltage.
  CMP1_DACCR = B11000000 | brightnessThreshold; // enable 6b DAC1 and use VDD as reference
  analogComparator1.enableInterrupt(changeHandler, CHANGE);
}

#elif defined(REL_GR_KURUMI) // GR-KURUMI

void changeHandler()
{
  uint16_t x;

  x = lastHsyncTime - (uint16_t)TCR05.tcr05; // TCR05 counts down every micro seconds

  // HSync period is 63.56μs and sync-to-blanking-end 9.48μs
  if (x <= 64 && x >= 10) {
  	x -= 10;
    // D4 = P31 (port 3, bit 1)
    if ((*((volatile uint8_t*)ADDR_PORT_REG + PORT_3) & B00000010)) { // store 2 bit per interrupt
      // INP > INM
      line[x >> 4] |= (image_t)1 << ((x & B1111) << 1);
    } else {
      // INP < INM
      line[x >> 4] |= (image_t)1 << (((x & B1111) << 1) | 1);
    }
  }
}

void _resetCMP()
{
  // Prepare capturing video frames:
  //   D9 is used as the analog threshold voltage on external analog comparator.
  pinMode(9, OUTPUT);
  //   Comparator's positive reference pin is connected to D9 via a low-pass RC filter
  //   (R = 3.9k and C = 0.1μF). In order to keep the voltage stable, PWM frequency on D9 pin
  //   must be as high as possible: 
  //       cf. gr_common/RLduino78/cores/RLduino78_basic.cpp

  analogWriteFrequency(32000L);
  analogWrite(9, brightnessThreshold);
  
  pinMode(4, INPUT_PULLUP);
  // in order to attach interrupt #2 on D4, modification on RLduino78 files are necessary.
  // (not well documented yet)
  attachInterrupt(4 + INT_NUMBER_OFFSET, changeHandler, CHANGE);
}

#else // Arduino Pro Mini

void changeHandler()
{
  uint16_t x;

  x = (uint16_t)micros() - lastHsyncTime;

  // HSync period is 63.56μs.
  // subtract the amount for sync-to-blanking-end 9.48μs.
  x -= 10;

  // on 8MHz/16MHz Arduinos micros() has a resolution of 8μs/4μs respectively.
  // actual brightness information Arduino Pro Mini 8MHz can capture from each scan line is only 5 (or 6) samples. 
  x >>= 2;
#if F_CPU == 16000000 // overclocked Arduino Pro Mini
  // store two bit per interrupt.
  if ((ACSR & (1 << ACO))) {
      // AIN+ > AIN-
      line[x >> 3] |= (image_t)1 << ((x & B111) << 1);
  } else {
      // AIN+ > AIN-
      line[x >> 3] |= (image_t)1 << (((x & B111) << 1) | 1);
  }
#else // F_CPU == 8000000
  // store two bit per interrupt.
  if ((ACSR & (1 << ACO))) {
    // AIN+ > AIN-
    line[0] |= (1 << x);      // store B01
  } else {
    // AIN+ < AIN-
    line[0] |= (1 << (x | 1));    // store B10
  }
#endif
}

void _resetCMP()
{
  // Prepare capturing video frames:
  //   D9 is used as the analog threshold voltage on Arduino analog comparator.
  //   Note: don't change pin D9 to D5 or D6. because PWM on D5 or D6 depends on Timer 0 so as micros() or delay().
  pinMode(9, OUTPUT);
  //   Comparator's positive reference pin AIN0 is connected to D9 via a low-pass RC filter
  //   (R = 3.9k and C = 0.1μF). In order to keep the voltage stable, PWM frequency on D9 pin
  //   must be as high as possible: Set divisor = 1.
  //       cf. http://playground.arduino.cc/Main/TimerPWMCheatsheet
  TCCR1B = TCCR1B & B11111000 | 0x01;
  analogWrite(9, brightnessThreshold);
  
  analogComparator.setOn(AIN0, AIN1);
  analogComparator.enableInterrupt(changeHandler, CHANGE);
}

#endif

void _resetVMD()
{
  interlace = 0;
  nosignal = true;
  
  pinMode(2, INPUT);
  pinMode(3, INPUT);
  attachInterrupt(2 + INT_NUMBER_OFFSET, VSyncHandler, FALLING); 
  attachInterrupt(3 + INT_NUMBER_OFFSET, HSyncHandler, FALLING);

  _resetCMP(); // reset analog comparator

  VMDstatus = 2;
  Serial.println(F("VMD start"));
}

void resetVMD()
{
  Serial.println(F("VMD will start after 5 seconds."));
  VMDstatus = 1;
  epoch = millis();
}

void checkVMD()
{
  unsigned long n = millis();
  switch (VMDstatus) {
    case 1:
      if (n - epoch > 4500L) { // wait 4.5 sec until starting VMD.
        _resetVMD();
        delay(500); // ignore transition from blank signal to captured signal
      }
      return;
    case 2: // VMD working & no motion detected.
      break;
    case 3: // motion has been detected recently, do nothing for 5 sec.
      if (n - epoch > 5000L) { 
        motionEnd();
        return;
      }
      break;
    case 4: // camera stop command has been sent recently, do nothing for 5 sec.
      if (n - epoch > 5000L) {
        VMDstatus = 2;
      }
      return;
    default:
      break;
  }
  
  if (nosignal) {
    return;
  }

#if 0  // Debug
  for (int c = 0; c < MAX_SCANLINES; c++) {
    for (int i = 0; i < (1 << WORD_PER_LINE); i++) {
      for (int k = 0; k < SIZE_OF_IMAGE_T << 3; k++) {
        Serial.print((image[(c << WORD_PER_LINE) + i] & (1 << k) ? 1 : 0) + (image[(c + MAX_SCANLINES << WORD_PER_LINE) + i] & (1 << k) ? 1 : 0));
      }
    }
    Serial.println();
  }
  Serial.println((uint16_t)diff);
  Serial.println(F("--------"));
#else
  if (diff > differenceThreshold) { // Change Me!
//    Serial.println((uint16_t)diff);
    Serial.println(F("move"));
    motionDetected();
  }
#endif
}

#else

void resetVMD()
{
}

void checkVMD()
{
}

#endif


