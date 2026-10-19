//
// webui_machine.cpp
//
// Modified for BMA800XL: the Atari 800XL's name, settings file and the
// file types atari800 can start, in place of BMC64's machines.
// Changed for BMCoco and BMApple2: the Apple II machine.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "webui.h"

#if BMC64_WEBUI

#include "webui_machine.h"

#include <stdio.h>

// The machine running (src/port/platform.c).
extern "C" int bmapple2_machine(void);
extern "C" const char *bmapple2_machine_name(int machine);

namespace {

// The disk images KEGS can boot (emu_autostart_interrupt() in
// src/port/platform.c puts them in a drive and restarts): 5.25" (DOS 3.3
// and ProDOS order, nibbles, WOZ), 3.5" and hard disks.
const char *const kRunTypes[] = {
    "dsk", "do", "po", "nib", "woz", "2mg", "2img", "hdv", 0,
};

const WebUiMachine kMachine = {
    "/config.kegs", kRunTypes,
};

char Lower(char c) {
  return (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
}

}  // namespace

const WebUiMachine *WebUiMachineGet(void) {
  return &kMachine;
}

const char *WebUiMachineName(void) {
  int machine = bmapple2_machine();
  return machine >= 0 ? bmapple2_machine_name(machine) : "";
}

boolean WebUiMachineCanRun(const char *ext) {
  for (const char *const *type = kMachine.run_types; *type != 0; type++) {
    const char *a = ext;
    const char *b = *type;
    while (*a != '\0' && Lower(*a) == *b) {
      a++;
      b++;
    }
    if (*a == '\0' && *b == '\0') return TRUE;
  }
  return FALSE;
}

int WebUiMachineCapsJson(char *out, unsigned size) {
  char run[128];
  unsigned used = 0;
  for (const char *const *type = kMachine.run_types; *type != 0; type++) {
    int n = snprintf(run + used, sizeof(run) - used, "%s\"%s\"",
                     used == 0 ? "" : ",", *type);
    if (n < 0 || (unsigned) n >= sizeof(run) - used) return -1;
    used += (unsigned) n;
  }
  run[used] = '\0';

  int length = snprintf(out, size,
                        "{\"settings\":\"%s\",\"run\":[%s]}",
                        kMachine.settings, run);
  if (length < 0 || (unsigned) length >= size) return -1;
  return length;
}

#endif  // BMC64_WEBUI
