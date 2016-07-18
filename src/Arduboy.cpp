/** 
 * @file Arduboy.cpp
 * \brief Implementation of the Arduboy class.
 *
 */

#include "Arduboy.h"
#include "ab_logo.c"
#include "glcdfont.c"

uint8_t ArduboyBase::sBuffer[];

ArduboyBase::ArduboyBase()
{
  // Constructor for ArduboyBase

  // Initialize frame management
  setFrameRate(60);    // set default frame rate
  frameCount = 0;      // set frame count to 0
  nextFrameStart = 0;  // first start point set to 0
  post_render = false; // don't perform post render on first frame
}

void ArduboyBase::start()
{
  begin();
}

// functions called here should be public so users can create their
// own init functions if they need different behavior than `begin`
// provides by default
void ArduboyBase::begin()
{
  boot();       // raw hardware
  blank();      // blank the display
  flashlight(); // start the flashlight if the UP button is held
  systemButtons(); // check for the presence of any held system buttons
  bootLogo();      // display the boot logo
  audio.begin();   // start the audio
}

void ArduboyBase::flashlight()
{
  if(!pressed(UP_BUTTON))
    return;

  // turn all pixels on
  sendLCDCommand(OLED_ALL_PIXELS_ON);
  // turn red, green and blue LEDS on for white light
  digitalWriteRGB(RGB_ON, RGB_ON, RGB_ON);

  // until the down button is pressed, stay in flashlight mode.
  while (!pressed(DOWN_BUTTON))
    idle();

  digitalWriteRGB(RGB_OFF, RGB_OFF, RGB_OFF);
  sendLCDCommand(OLED_PIXELS_FROM_RAM);
}

void ArduboyBase::systemButtons()
{
  while (pressed(B_BUTTON))
  {
    digitalWrite(BLUE_LED, RGB_ON);
    sysCtrlSound(UP_BUTTON + B_BUTTON, GREEN_LED, 0xff);
    sysCtrlSound(DOWN_BUTTON + B_BUTTON, RED_LED, 0);
    delay(200);
  }
}

void ArduboyBase::sysCtrlSound(uint8_t buttons, uint8_t led, uint8_t eeVal)
{
  if (pressed(buttons))
  {
    digitalWrite(BLUE_LED, RGB_OFF); // turn off blue LED
    delay(200);
    digitalWrite(led, RGB_ON); // turn on "acknowledge" LED
    EEPROM.update(EEPROM_AUDIO_ON_OFF, eeVal);
    delay(500);
    digitalWrite(led, RGB_OFF); // turn off "acknowledge" LED

    while (pressed(buttons)) {} // Wait for button release
  }
}

void ArduboyBase::bootLogo()
{
  digitalWrite(RED_LED, RGB_ON);

  for (int8_t y = -18; y <= 24; y++)
  {
    if (y == -4)
      digitalWriteRGB(RGB_OFF, RGB_ON, RGB_OFF);

    if (y == -4)
      digitalWriteRGB(RGB_OFF, RGB_ON, RGB_OFF);

    clear();
    drawBitmap(20, y, arduboy_logo, 88, 16, WHITE);
    display();
    delay(27);
    // longer delay post boot, we put it inside the loop to
    // save the flash calling clear/delay again outside the loop
    if (y == -16)
      delay(250);
  }

  delay(750);
  digitalWrite(BLUE_LED, RGB_OFF);
}

// This function is deprecated.
// It is retained for backwards compatibility.
// New code should use boot() as a base.
void ArduboyBase::beginNoLogo()
{
  boot();       // raw hardware
  blank();      // blank the display
  flashlight(); // start the flashlight if the UP button is held
  audio.begin();   // start the audio
}

/* Frame management */

void ArduboyBase::setFrameRate(uint8_t rate)
{
  frameRate = rate;
  eachFrameMillis = 1000/rate;
}

bool ArduboyBase::everyXFrames(uint8_t frames)
{
  return frameCount % frames == 0;
}

bool ArduboyBase::newFrame()
{
  long now = millis();
  uint8_t remaining;

  // post render
  if (post_render)
  {
    lastFrameDurationMs = now - lastFrameStart;
    frameCount++;
    post_render = false;
  }

  // if it's not time for the next frame yet
  if (now < nextFrameStart)
  {
    remaining = nextFrameStart - now;
    // if we have more than 1ms to spare, lets sleep
    // we should be woken up by timer0 every 1ms, so this should be ok
    if (remaining > 1)
      idle();
    return false;
  }

  // pre-render

  // next frame should start from last frame start + frame duration
  nextFrameStart = lastFrameStart + eachFrameMillis;
  // If we're running CPU at 100%+ (too slow to complete each loop within
  // the frame duration) then it's possible that we get "behind"... Say we
  // took 5ms too long, resulting in nextFrameStart being 5ms in the PAST.
  // In that case we simply schedule the next frame to start immediately.
  //
  // If we were to let the nextFrameStart slide further and further into
  // the past AND eventually the CPU usage dropped then frame management
  // would try to "catch up" (by speeding up the game) to make up for all
  // that lost time.  That would not be good.  We allow frames to take too
  // long (what choice do we have?), but we do not allow super-fast frames
  // to make up for slow frames in the past.
  if (nextFrameStart < now)
    nextFrameStart = now;

  lastFrameStart = now;

  post_render = true;
  return post_render;
}

// This function is deprecated.
// It should remain as is for backwards compatibility.
// New code should use newFrame().
bool ArduboyBase::nextFrame()
{
  long now = millis();
  uint8_t remaining;

  if (post_render) {
    lastFrameDurationMs = now - lastFrameStart;
    frameCount++;
    post_render = false;
  }

  if (now < nextFrameStart) {
    remaining = nextFrameStart - now;
    if (remaining > 1)
      idle();
    return false;
  }

  nextFrameStart = now + eachFrameMillis;
  lastFrameStart = now;
  post_render = true;
  return post_render;
}

int ArduboyBase::cpuLoad()
{
  return lastFrameDurationMs*100 / eachFrameMillis;
}

void ArduboyBase::initRandomSeed()
{
  power_adc_enable();  // ADC on
  randomSeed(~rawADC(ADC_TEMP) * ~rawADC(ADC_VOLTAGE) * ~micros() + micros());
  power_adc_disable(); // ADC off
}

uint16_t ArduboyBase::rawADC(uint8_t adc_bits)
{
  ADMUX = adc_bits;
  // we also need MUX5 for temperature check
  if (adc_bits == ADC_TEMP)
    ADCSRB = _BV(MUX5);

  delay(2);                        // Wait for ADMUX setting to settle
  ADCSRA |= _BV(ADSC);             // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  return ADC;
}

/* Graphics */

void ArduboyBase::clear()
{
  fillScreen(BLACK);
}

void ArduboyBase::clearDisplay() // deprecated
{
  clear();
}

uint8_t ArduboyBase::draw(void (*f)())
{
  // pause render until it's time for the next frame
  if (!(newFrame()))
    return 1;

  // clear the buffer
  clear();

  // call the function passed as paramter to draw
  (*f)();

  // draw the buffer
  display();

  return 0;
}

void ArduboyBase::drawPixel(int x, int y, uint8_t color)
{
  #ifdef PIXEL_SAFE_MODE
  if (x < 0 || x > (WIDTH-1) || y < 0 || y > (HEIGHT-1))
    return;
  #endif

  uint8_t row = (uint8_t)y / 8;
  if (color)
    sBuffer[(row*WIDTH) + (uint8_t)x] |=   _BV((uint8_t)y % 8);
  else
    sBuffer[(row*WIDTH) + (uint8_t)x] &= ~ _BV((uint8_t)y % 8);
}

uint8_t ArduboyBase::getPixel(uint8_t x, uint8_t y)
{
  uint8_t row = y / 8;
  uint8_t bit_position = y % 8;
  return (sBuffer[(row*WIDTH) + x] & _BV(bit_position)) >> bit_position;
}

void ArduboyBase::drawCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  drawPixel(x0, y0+r, color);
  drawPixel(x0, y0-r, color);
  drawPixel(x0+r, y0, color);
  drawPixel(x0-r, y0, color);

  while (x<y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }

    x++;
    ddF_x += 2;
    f += ddF_x;

    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 + x, y0 - y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 - y, y0 - x, color);
  }
}

void ArduboyBase::drawCircleHelper(int16_t x0, int16_t y0, uint8_t r, 
                                   uint8_t cornername, uint8_t color)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x<y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }

    x++;
    ddF_x += 2;
    f += ddF_x;

    if (cornername & 0x4)
    {
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 + y, y0 + x, color);
    }
    if (cornername & 0x2)
    {
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8)
    {
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1)
    {
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 - x, y0 - y, color);
    }
  }
}

void ArduboyBase::fillCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color)
{
  drawFastVLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

void ArduboyBase::fillCircleHelper(
    int16_t x0,
    int16_t y0,
    uint8_t r,
    uint8_t cornername,
    int16_t delta,
    uint8_t color)
{
  // used to do circles and roundrects!
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x < y)
  {
    if (f >= 0)
    {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }

    x++;
    ddF_x += 2;
    f += ddF_x;

    if (cornername & 0x1)
    {
      drawFastVLine(x0+x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0+y, y0-x, 2*x+1+delta, color);
    }

    if (cornername & 0x2)
    {
      drawFastVLine(x0-x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0-y, y0-x, 2*x+1+delta, color);
    }
  }
}

void ArduboyBase::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           uint8_t color)
{
  // bresenham's algorithm - thx wikpedia
  bool steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep)
  {
    swap(x0, y0);
    swap(x1, y1);
  }

  if (x0 > x1)
  {
    swap(x0, x1);
    swap(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int8_t ystep;

  if (y0 < y1)
    ystep = 1;
  else
    ystep = -1;

  for (; x0 <= x1; x0++)
  {
    if (steep)
      drawPixel(y0, x0, color);
    else
      drawPixel(x0, y0, color);

    err -= dy;
    if (err < 0)
    {
      y0 += ystep;
      err += dx;
    }
  }
}

void ArduboyBase::drawRect(int16_t x, int16_t y, uint8_t w, uint8_t h,
                           uint8_t color)
{
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y+h-1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x+w-1, y, h, color);
}

void ArduboyBase::drawFastVLine(int16_t x, int16_t y, uint8_t h, uint8_t color)
{
  int end = y+h;
  for (int a = max(0,y); a < min(end,HEIGHT); a++)
    drawPixel(x,a,color);
}

void ArduboyBase::drawFastHLine(int16_t x, int16_t y, uint8_t w, uint8_t color)
{
  // Do bounds/limit checks
  if (y < 0 || y >= HEIGHT)
    return;

  // make sure we don't try to draw below 0
  if (x < 0)
  {
    w += x;
    x = 0;
  }

  // make sure we don't go off the edge of the display
  if ((x + w) > WIDTH)
    w = (WIDTH - x);

  // if our width is now negative, punt
  if (w <= 0)
    return;

  // buffer pointer plus row offset + x offset
  register uint8_t *pBuf = sBuffer + ((y/8) * WIDTH) + x;

  // pixel mask
  register uint8_t mask = 1 << (y&7);

  switch (color)
  {
    case WHITE:
      while (w--) {
        *pBuf++ |= mask;
      };
      break;

    case BLACK:
      mask = ~mask;
      while (w--) {
        *pBuf++ &= mask;
      };
      break;
  }
}

void ArduboyBase::fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h,
                           uint8_t color)
{
  // least efficient version; update in subclasses if desired!
  for (int16_t i=x; i<x+w; i++)
    drawFastVLine(i, y, h, color);
}

void ArduboyBase::fillScreen(uint8_t color)
{
  // C version :
  //
  // if (color) color = 0xFF;  //change any nonzero argument to b11111111 and insert into screen array.
  // for(int16_t i=0; i<1024; i++)  { sBuffer[i] = color; }  //sBuffer = (128*64) = 8192/8 = 1024 bytes.

  asm volatile
  (
    // load color value into r27
    "mov r27, %1 \n\t"
    // if value is zero, skip assigning to 0xff
    "cpse r27, __zero_reg__ \n\t"
    "ldi r27, 0xff \n\t"
    // load sBuffer pointer into Z
    "movw  r30, %0\n\t"
    // counter = 0
    "clr __tmp_reg__ \n\t"
    "loopto:   \n\t"
    // (4x) push zero into screen buffer,
    // then increment buffer position
    "st Z+, r27 \n\t"
    "st Z+, r27 \n\t"
    "st Z+, r27 \n\t"
    "st Z+, r27 \n\t"
    // increase counter
    "inc __tmp_reg__ \n\t"
    // repeat for 256 loops
    // (until counter rolls over back to 0)
    "brne loopto \n\t"
    // input: sBuffer, color
    // modified: Z (r30, r31), r27
    :
    : "r" (sBuffer), "r" (color)
    : "r30", "r31", "r27"
  );
}

void ArduboyBase::drawRoundRect(int16_t x, int16_t y, uint8_t w, uint8_t h,
                                uint8_t r, uint8_t color)
{
  // smarter version
  drawFastHLine(x+r, y, w-2*r, color); // Top
  drawFastHLine(x+r, y+h-1, w-2*r, color); // Bottom
  drawFastVLine(x, y+r, h-2*r, color); // Left
  drawFastVLine(x+w-1, y+r, h-2*r, color); // Right
  // draw four corners
  drawCircleHelper(x+r, y+r, r, 1, color);
  drawCircleHelper(x+w-r-1, y+r, r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r, y+h-r-1, r, 8, color);
}

void ArduboyBase::fillRoundRect(int16_t x, int16_t y, uint8_t w, uint8_t h,
                                uint8_t r, uint8_t color)
{
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r, y+r, r, 2, h-2*r-1, color);
}

void ArduboyBase::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               int16_t x2, int16_t y2, uint8_t color)
{
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

void ArduboyBase::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, 
                               int16_t x2, int16_t y2, uint8_t color)
{

  int16_t a, b, y, last;
  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1)
    swap(y0, y1); swap(x0, x1);
  if (y1 > y2)
    swap(y2, y1); swap(x2, x1);
  if (y0 > y1)
    swap(y0, y1); swap(x0, x1);

  if (y0 == y2)
  { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if (x1 < a)
      a = x1;
    else if (x1 > b)
      b = x1;
    if (x2 < a)
      a = x2;
    else if (x2 > b)
      b = x2;
    drawFastHLine(a, y0, b-a+1, color);
    return;
  }

  int16_t dx01 = x1 - x0,
      dy01 = y1 - y0,
      dx02 = x2 - x0,
      dy02 = y2 - y0,
      dx12 = x2 - x1,
      dy12 = y2 - y1,
      sa = 0,
      sb = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if (y1 == y2)
    last = y1;   // Include y1 scanline
  else
    last = y1-1; // Skip it


  for (y = y0; y <= last; y++)
  {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;

    if (a > b)
      swap(a,b);

    drawFastHLine(a, y, b-a+1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);

  for (; y <= y2; y++)
  {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;

    if (a > b)
      swap(a,b);

    drawFastHLine(a, y, b-a+1, color);
  }
}

void ArduboyBase::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                             uint8_t w, uint8_t h, uint8_t color)
{
  // no need to dar at all of we're offscreen
  if (x+w < 0 || x > WIDTH-1 || y+h < 0 || y > HEIGHT-1)
    return;

  int yOffset = abs(y) % 8;
  int sRow = y / 8;
  if (y < 0) {
    sRow--;
    yOffset = 8 - yOffset;
  }

  int rows = h/8;
  if (h % 8 != 0) rows++;
  for (int a = 0; a < rows; a++) {
    int bRow = sRow + a;
    if (bRow > (HEIGHT / 8) - 1) break;
    if (bRow > -2)
    {
      for (int iCol = 0; iCol < w; iCol++)
      {
        if ((iCol + x) > (WIDTH-1)) break;
        if ((iCol + x) >= 0)
        {
          if (bRow >= 0)
          {
            if (color == WHITE)
            {
              this->sBuffer[ (bRow*WIDTH) + x + iCol ] |=
                pgm_read_byte(bitmap + (a * w) + iCol) << yOffset;
            }
            else if (color == BLACK)
            {
              this->sBuffer[ (bRow*WIDTH) + x + iCol ] &=
                ~(pgm_read_byte(bitmap + (a * w) + iCol) << yOffset);
            }
            else
            {
              this->sBuffer[ (bRow*WIDTH) + x + iCol ] ^=
                pgm_read_byte(bitmap + (a * w) + iCol) << yOffset;
            }
          }
          if (yOffset && (bRow < ((HEIGHT / 8) - 1)) && (bRow > -2)) 
          {
            if (color == WHITE) 
            {
              this->sBuffer[ ((bRow+1)*WIDTH) + x + iCol ] |= 
                pgm_read_byte(bitmap + (a * w) + iCol) >> (8 - yOffset);
            }
            else if (color == BLACK) 
            {
              this->sBuffer[ ((bRow+1)*WIDTH) + x + iCol ] &=
                ~(pgm_read_byte(bitmap + (a * w) + iCol) >> (8 - yOffset));
            }
            else
            {
              this->sBuffer[ ((bRow+1)*WIDTH) + x + iCol ] ^=
                pgm_read_byte(bitmap + (a * w) + iCol) >> (8 - yOffset);
            }
          }
        }
      }
    }
  }
}


void ArduboyBase::drawSlowXYBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                               uint8_t w, uint8_t h, uint8_t color)
{
  // no need to dar at all of we're offscreen
  if (x+w < 0 || x > WIDTH-1 || y+h < 0 || y > HEIGHT-1)
    return;

  int16_t xi, yi, byteWidth = (w + 7) / 8;
  for(yi = 0; yi < h; yi++)
  {
    for(xi = 0; xi < w; xi++ )
    {
      if(pgm_read_byte(bitmap + yi * byteWidth + xi / 8) & (128 >> (xi & 7)))
        drawPixel(x + xi, y + yi, color);
    }
  }
}


void ArduboyBase::drawChar
(int16_t x, int16_t y, uint8_t c, uint8_t color, uint8_t bg, uint8_t size)
{
  bool draw_background = bg != color;

  if ((x >= WIDTH) ||            // Clip right
     (y >= HEIGHT) ||            // Clip bottom
     ((x + 5 * size - 1) < 0) || // Clip left
     ((y + 8 * size - 1) < 0)    // Clip top
  ) return;

  for (int8_t i=0; i<6; i++)
  {
    uint8_t line;
    if (i == 5)
      line = 0x0;
    else
      line = pgm_read_byte(font + (5 * c) + i);

    for (int8_t j = 0; j < 8; j++)
    {
      uint8_t draw_color = (line & 0x1) ? color : bg;

      if (draw_color || draw_background)
      {
        for (uint8_t a = 0; a < size; a++ )
        {
          for (uint8_t b = 0; b < size; b++ )
            drawPixel(x + (i * size) + a, y + (j * size) + b, draw_color);
        }
      }
      line >>= 1;
    }
  }
}

void ArduboyBase::display()
{
  // copy data to draw to buffer
  this->paintScreen(sBuffer);
}

uint8_t* ArduboyBase::getBuffer()
{
  return sBuffer;
}

bool ArduboyBase::pressed(uint8_t buttons)
{
  return (buttonsState() & buttons) == buttons;
}

bool ArduboyBase::notPressed(uint8_t buttons)
{
  return (buttonsState() & buttons) == 0;
}

void ArduboyBase::swap(int16_t& a, int16_t& b)
{
  int temp = a;
  a = b;
  b = temp;
}

/*
 * Arduboy Class
 */

Arduboy::Arduboy()
{
  cursor_x = 0;
  cursor_y = 0;
  textColor = 1;
  textBackground = 0;
  textSize = 1;
  textWrap = 0;
}

size_t Arduboy::write(uint8_t c)
{
  if (c == '\n')
  {
    cursor_y += textSize * 8;
    cursor_x = 0;
  }
  else if (c == '\r') ; // skip carriage returns
  else
  {
    drawChar(cursor_x, cursor_y, c, textColor, textBackground, textSize);
    cursor_x += textSize * 6;
    if (textWrap && (cursor_x > (WIDTH - textSize * 6)))
    {
      // calling ourselves recursively for 'newline' is 
      // 12 bytes smaller than doing the same math here
      write('\n');
    }
  }
}

void Arduboy::setCursor(int16_t x, int16_t y)
{
  cursor_x = x;
  cursor_y = y;
}

uint16_t Arduboy::getCursorX()
{
  return cursor_x;
}

uint16_t Arduboy::getCursorY()
{
  return cursor_y;
}

void Arduboy::setTextColor(uint8_t color)
{
  textColor = color;
}

void Arduboy::setTextBackground(uint8_t bg)
{
  textBackground = bg;
}

void Arduboy::setTextSize(uint8_t s)
{
  // size must always be 1 or higher
  textSize = max(1, s);
}

void Arduboy::setTextWrap(bool w)
{
  textWrap = w;
}

void Arduboy::clear()
{
  ArduboyBase::clear();
  cursor_x = cursor_y = 0;
}
