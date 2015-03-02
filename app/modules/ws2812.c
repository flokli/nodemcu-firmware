#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
/**
 * All this code is mostly from http://www.esp8266.com/viewtopic.php?f=21&t=1143&sid=a620a377672cfe9f666d672398415fcb
 * from user Markus Gritsch.
 * I just put this code into its own module and pushed into a forked repo,
 * to easily create a pull request. Thanks to Markus Gritsch for the code.
 *
 */

// This must be compiled with -O2 to get the timing right.
static void __attribute__((optimize("O2"))) delay_nops(uint32_t count) {
  uint32_t i;
  for(i = 0; i < count; i++)
    __asm__ __volatile__ ("nop");
}


// ----------------------------------------------------------------------------
// -- http://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/
// The ICACHE_FLASH_ATTR is there to trick the compiler and get the very first pulse width correct.
static void ICACHE_FLASH_ATTR send_ws_0(uint8_t gpio) {
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
  delay_nops(7); // ~ 430ns
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
  delay_nops(9); // ~ 780ns
}

static void ICACHE_FLASH_ATTR send_ws_1(uint8_t gpio) {
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
  delay_nops(11); // ~ 680ns
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
  delay_nops(8);  // ~ 690ns
}

// Lua: ws2812.write(pin, "string")
// Byte triples in the string are interpreted as R G B values
// and sent to the hardware as G R B.
// ws2812.write(4, string.char(255, 0, 0)) uses GPIO2 and sets the first LED red.
// ws2812.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.write(4, string.char(0, 255, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_write(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *buffer = luaL_checklstring(L, 2, &length);

  //ignore incomplete Byte triples at the end of buffer
  length -= length % 3;

  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 0);
  os_delay_us(10);

  os_intr_lock();
  const char * const end = buffer + length;

  size_t i = 1;
  while (buffer + i <= end) {

    uint8_t mask = 0x80;
    while (mask) {
      (buffer[i] & mask) ? send_ws_1(pin_num[pin]) : send_ws_0(pin_num[pin]);
      mask >>= 1;
    }
    i += ((i % 3) == 1) ? -1 : 2;
  }
  os_intr_unlock();

  return 0;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "write" ), LFUNCVAL( ws2812_write )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  LREGISTER(L, "ws2812", ws2812_map);
  return 1;
}

// ----------------------------------------------------------------------------

