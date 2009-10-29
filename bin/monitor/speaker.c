/* globals */

#include "types.h"
#include "emu.h"

static unsigned int port61 = 0x0e;

/* reads/writes to the speaker control port (0x61)
 */
unsigned char spkr_io_read(unsigned int port)
{
   if (port==0x61)  {
      if (config.speaker == SPKR_NATIVE)
         return port_safe_inb(0x61);
      else
         return port61;
   }
   return 0xff;
}

void spkr_io_write(unsigned int port, unsigned char value) {
   if (port==0x61) {
      switch (config.speaker) {
       case SPKR_NATIVE:
          port_safe_outb(0x61, value & 0x03);
          break;

       case SPKR_EMULATED:
          port61 = value & 0x0f;
          do_sound(pit[2].write_latch & 0xffff);
          break;

       case SPKR_OFF:
          port61 = value & 0x0f;
          break;
      }
      port61 = value & 0x0f;
   }
}
