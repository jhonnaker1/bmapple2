# Installation

## What you need

- A Raspberry Pi 4 Model B or Pi 400, a Raspberry Pi 3 Model B, B+ or A+, a
  Raspberry Pi Zero 2 W, or a Raspberry Pi 2 Model B
- A microSD card (BMApple2 itself needs only a few MB; disk images take
  what they take, a GS/OS hard disk image often 32 MB)
- An HDMI display, ideally one that accepts 720p at 60 Hz
- Your own Apple IIgs or Apple IIe ROM image (see below)
- A USB keyboard, and a USB mouse for the IIgs desktop; a USB gamepad if you
  like

## Prepare the SD card

1. Download `bmapple2-v<version>.zip` from the releases page, or build it
   yourself (see [BUILDING.md](BUILDING.md)) and use `build/sdcard/`.
2. Format the SD card as FAT32.
3. Extract the zip to the root of the card, so that the kernels
   (`kernel8-32.img`, `kernel7.img`, `kernel8-rpi4.img`) and `config.txt` are
   at the top level, not in a folder.
4. Copy your ROM into the `roms` folder (see below).
5. Copy your disk images to the card. The `disks` folder is there for them,
   but any folder works.
6. Put the card in the Pi, connect the display, keyboard and mouse, and
   switch on.

There is no image to write and nothing to install on the card beyond
copying the files.

## ROM files

The Apple IIgs and IIe ROMs are copyrighted by Apple, so BMApple2 can't
include them. Copy an image of your own into the `roms` folder:

| File | ROM |
| --- | --- |
| `ROM.03` | Apple IIgs ROM 3 (256K) |
| `ROM.01` | Apple IIgs ROM 01 (128K) |
| `APPLE2E.ROM` | Apple IIe, enhanced (32K, the form other emulators use) |

The names other emulators use work too (`APPLE2GS.ROM2`, `APPLE2GS.ROM`,
`xgs.rom`, ...). With a MAME ROM set (Ample's on the Mac is one), copy
`341-0728` and `341-0748` out of `apple2gs.zip` into `roms` and BMApple2 makes
`ROM.03` from them. See [ROMS.txt](ROMS.txt) for the IIe and the details.

## First boot

With no disks yet the IIgs says *Check startup device!*. Press **F4** for
KEGS's settings, choose *Disk Configuration*, and put a disk image in a
drive: a GS/OS or ProDOS hard disk image in slot 7 drive 1, an 800K disk in
slot 5, a 5.25" disk in slot 6. Leave with F4 and choose *Restart* in the
**F10** menu. The disks are remembered for next time.

USB keyboards, mice and gamepads can be plugged in and out while it runs.

The display is set to 720p at 60 Hz. If you get no picture, see
[Troubleshooting](USER_GUIDE.md#troubleshooting) in the User Guide.

## Networking (optional)

To manage the card's files from a browser, and start disk images from it,
turn on Ethernet or Wi-Fi under *Network* in the F10 menu, then reboot; the
web UI's address is shown at the bottom of the screen. For Wi-Fi, *Scan
for Wi-Fi Networks* lists the networks in range and asks for the password.
See [Networking and the web UI](USER_GUIDE.md#networking-and-the-web-ui).

## Upgrading

Copy the new release's files over the old ones, except for files you have
changed: `config.txt` and `cmdline.txt` if you edited them. Your settings
(`bmapple2.cfg`, `config.kegs`, `wpa_supplicant.conf`), ROMs and disk images
are not in the release zip and stay as they are.

With networking on, a new kernel (`kernel8-rpi4.img` for a Pi 4 or 400,
`kernel8-32.img` for a Pi 3 or Zero 2 W, `kernel7.img` for a Pi 2) can also
be uploaded in the web UI's file manager and started with **Reboot**.
Updates that change other files (the Pi firmware in the root or in
`firmware/`) need the card in a computer.
