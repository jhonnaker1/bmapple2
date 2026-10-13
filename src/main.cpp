//
// main.cpp
//
// This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
// the Raspberry Pi. See LICENSE for licensing details.
//

#include "kernel.h"
#include <circle/startup.h>

int main(void) {
  // The kernel is large; keep it off the stack.
  static CKernel Kernel;
  if (!Kernel.Initialize()) {
    halt();
    return EXIT_HALT;
  }

  TShutdownMode ShutdownMode = Kernel.Run();

  switch (ShutdownMode) {
  case ShutdownReboot:
    reboot();
    return EXIT_REBOOT;

  case ShutdownHalt:
  default:
    halt();
    return EXIT_HALT;
  }
}
