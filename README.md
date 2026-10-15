# BMApple2

BMApple2 is a bare metal Apple IIgs and Apple IIe emulator for the Raspberry
Pi 2, 3 and 4. It runs Kent Dickey's [KEGS](https://kegs.sourceforge.net/)
emulator directly on the hardware, with no Linux underneath, the same way
[BMC64](https://github.com/randyrossi/bmc64) does for the Commodore 64,
[BMA800XL](https://github.com/aminch/bma800xl) for the Atari 800XL and
[BMCoco](https://github.com/jhonnaker1/bmcoco) for the Tandy Color Computer.

BMApple2 is based on [BMCoco](https://github.com/jhonnaker1/bmcoco), which is
based on [BMA800XL](https://github.com/aminch/bma800xl) by Adam Minchinton,
which is based on [BMC64](https://github.com/randyrossi/bmc64) by Randy
Rossi. The emulator is replaced by KEGS; the kernel, display, sound, network
and web UI come from those projects.

- Emulates the Apple IIgs (ROM 01 or ROM 3), and the Apple IIe with a IIe ROM,
  chosen in the menu
- Boots GS/OS, ProDOS and DOS 3.3 disks: hard disk images in slot 7, 3.5"
  disks in slot 5, 5.25" disks in slot 6 (`.po`, `.2mg`, `.hdv`, `.dsk`,
  `.do`, `.nib`, `.woz`)
- Frames locked to the display's refresh for perfectly smooth scrolling
  (the IIgs runs at 59.92 Hz), with the sound kept in step: the IIgs's
  Ensoniq, the speaker and, in KEGS's settings, a Mockingboard
- USB keyboards and mice; USB gamepads as the Apple II's joystick (an analog
  stick gives the full paddle range); real joysticks wired to the GPIO header
- No shutdown needed: just switch it off (disk images are written as the
  emulated machine writes them, and pushed out to the card every second)
- KEGS's own settings (F4) for disks, speed and the rest, and a menu (F10):
  the machine, a colour or monochrome monitor (white, green or amber), the
  volume, video, sound and network
- Optional Ethernet or Wi-Fi with BMC64's web UI: manage the SD card from a
  browser and start disk images

## ROMs

The Apple IIgs and IIe ROMs are copyrighted by Apple and are not included.
Copy your own into the `roms` folder of the SD card: `ROM.03` (or `ROM.01`)
for a IIgs, `APPLE2E.ROM` for a IIe. See [docs/ROMS.txt](docs/ROMS.txt).

A MAME ROM set works too (such as the one in
[Ample](https://github.com/ksherlock/ample) on the Mac): copy `341-0728` and
`341-0748` out of `apple2gs.zip` into `roms`, and BMApple2 makes `ROM.03` from
them the first time it starts (MAME keeps the ROM chips, with the second's
halves the other way round). The same works for the IIe from `apple2e.zip`
and `a2diskiing.zip`; see [docs/ROMS.txt](docs/ROMS.txt).

## Supported hardware

Raspberry Pi 3 Model B, B+ and A+, and the Raspberry Pi Zero 2 W
(`kernel8-32.img`), and the Raspberry Pi 2 Model B (`kernel7.img`). The card
has a kernel for each and each Pi starts its own. Video and sound go out over
HDMI; sound can also go to the 3.5mm jack.

The Raspberry Pi 4 Model B and Pi 400 (`kernel8-rpi4.img`, 64-bit) are
supported too, with the display run at the IIgs's exact refresh rate on either
HDMI port, and sound over HDMI or the Pi 4 B's 3.5mm jack. See
[docs/PI4.md](docs/PI4.md).

## Getting started

1. Format an SD card as FAT32.
2. Copy the contents of a release zip (or of `build/sdcard/` after building)
   to the root of the card.
3. Copy your ROM into `roms`, and your disk images to the card (the `disks`
   folder is there for them, but any folder works).
4. Boot the Pi with a USB keyboard (and a mouse) connected. With no disks
   yet, the IIgs says *Check startup device!*. Press **F4**, choose *Disk
   Configuration*, and put a disk image in a drive (slot 7 drive 1 for a
   GS/OS hard disk image), then *Restart* in the **F10** menu.

## Keys

| Key                       | Does                                            |
|---------------------------|-------------------------------------------------|
| F10                       | BMApple2's menu                                 |
| F4                        | KEGS's settings: disks, ROM, speed, joystick    |
| Ctrl+F12                  | Reset                                           |
| Windows / Command, or F1  | Open Apple (Command)                            |
| Alt, or F2                | Option (Solid Apple)                            |
| F3                        | Escape                                          |
| F6                        | Speed (1 MHz, 2.8 MHz, 8 MHz, unlimited)        |
| F5                        | KEGS's status lines                             |

To restart the machine (as switching it off and on does), choose *Restart*
in the F10 menu.

Gamepads are the joystick: the left stick or D-pad moves, **A** and **B** are
buttons 0 and 1, **X** types Space and **Y** Return. **Home**, or
**Select+Start** together, opens the menu, where the D-pad moves, **A**
chooses and **B** goes back.

See the [User Guide](docs/USER_GUIDE.md) for all the details, including the
options in `cmdline.txt` and the GPIO joystick wiring.

## Building

```
./build_sdcard.sh
```

builds everything (the kernels for all three boards; `./build_sdcard.sh pi3`,
`pi2` or `pi4` builds one) and stages the SD card files in `build/sdcard/`.
It works on Linux and macOS. See [docs/BUILDING.md](docs/BUILDING.md).

## How it fits together

```
src/main.cpp, kernel.cpp   Circle kernel: USB, GPIO, SD card; runs KEGS on core 1
src/fbl.cpp                Display (dispmanx layer, optional CRT shader), from BMC64
src/vchiqsound*.cpp        Sound over HDMI or the headphone jack, from BMC64
src/network.cpp, webui/    Ethernet / Wi-Fi and the web UI, from BMC64
src/host.h                 What the kernel offers the emulator
src/port/platform.c        KEGS's front end: picture, keyboard, mouse, ROMs; main loop
src/port/snddrv.c          KEGS's sound output
src/port/joystick.c        Gamepads and GPIO joysticks as the Apple II's joystick
src/port/keymap.c          USB keys to Apple Desktop Bus key codes
src/port/menu.c, osd.c     The menu, drawn in the Apple II's font
src/port/kegs.mk           KEGS's source list
third_party/kegs           KEGS (submodule)
third_party/circle-stdlib  Circle and newlib (submodule)
src/pi4/                   Pi 4 display, smooth scaling / CRT shader, HDMI sound
src/pi4kms/                Pi 4 direct display driver (docs/PI4.md)
tools/hostsim              Runs the emulator port on a PC for testing
tools/pi4scaler            Tests the Pi 4's smooth scaling and CRT shader on a PC
tools/pi4kms               Tests the Pi 4 display driver's calculations on a PC
```

KEGS is used unmodified: everything specific to BMApple2 is in `src/`.

## Licence

BMApple2 is free software under the GNU General Public License version 3; see
[LICENSE](LICENSE). It includes software from other projects under their own
licences; see [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).
