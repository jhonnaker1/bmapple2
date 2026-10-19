# Changelog

## Unreleased

- Pi Zero, 2, 3 and 4: run the ARM at its full clock (from BMCoco 0.5.3).
  Without an operating system asking for more the firmware left it at its
  lowest (600 MHz on a Pi 3 B+, not 1.4 GHz). The log shows the clock
- Fix the sound being cut off now and then (from BMCoco 0.5.2): a IIgs or
  IIe (59.92 Hz) locked to a 60 Hz display runs 0.13% fast, more than
  dropping one sample now and then made up for, so the sound buffer filled
  and what didn't fit was cut off. The sound is now squeezed or stretched
  smoothly, by up to 2%. On the Pi Zero to 3 the buffer was also measured
  in 48 kHz samples although they play at 44.1 kHz
- Disk activity (from BMCoco 0.5.3): while a 5.25" or 3.5" drive turns, a
  red light (lit while the disk is read) and its slot, drive and track
  (`S6 D1 T17`) at the bottom left of the picture, and "no disk" in red
  when the drive is empty. Video, Show Disk Activity (`disk_activity=0`)
  turns it off
- Fix a notification (Reset, Restart...) staying on the picture after its
  3 seconds when that part of the screen didn't change
- Video, Screen Size (100% down to 80%), for a TV that cuts off the edges
  (from BMCoco 0.5.3)
- The menus can be used with GPIO joysticks: either one moves, fire chooses
- From BMA800XL 1.4.1 and 1.4.2:
  - The SD card's `licenses` folder: the notices and licences of the code
    from other projects in the kernels (KEGS, Circle and newlib, BMC64's
    Apache 2.0 code, crt-pi, Mesa and Linux's V3D code, the GCC runtime);
    docs/THIRD_PARTY.md says where each is used
  - `./build_sdcard.sh pi4 pi5` builds several boards at once, and runs the
    PC tests first (`--skip-tests` leaves them out)
  - `cmdline.txt` leaves `usbpowerdelay` to Circle (300 ms, 510 ms behind a
    hub) rather than 1000 ms, for a quicker start
  - The Pi 4 and 5's display and GPU code is in `src/av/`, `src/v3d/` and
    `src/kms/`, with the scaled picture centred exactly
  - Troubleshooting: an 8BitDo M30 2.4G whose receiver is in Switch mode
- docs/SIMULATOR.md: testing and debugging BMApple2 on a PC (with gdb, lldb
  or strace), and getting a log from the Pi
- Tests: the machine-switching test no longer overwrites APPLE2E.ROM in
  BMAPPLE2_ROMS; sound tests at 44.1 and 48 kHz

## 0.3.2 – 2026-10-16

- The web UI shows the machine running ("Apple IIgs, ROM 3", "Apple IIe"),
  not always "Apple IIgs"
- About shows the machine's frame rate as "Frames", not "IIgs", and the
  SD card's README lists the Pi 5 and Pi Zero
- Pi 4 and 5: a rate just outside what HDMI allows gets the standard 50 or
  60 Hz mode (as in BMCoco), instead of keeping whatever mode the display had
- The web UI no longer carries BMC64's BASIC listing editor, which BMApple2
  never used

## 0.3.1 – 2026-10-16

- The web UI's welcome line and footer name both machines: "Bare metal
  Apple IIgs and IIe"

## 0.3.0 – 2026-10-15

- From BMA800XL 1.2.0 to 1.4.0 (as in BMCoco 0.4.0):
  - Raspberry Pi 5, Pi 500 and Pi 500+ (`kernel_2712.img`): BMA800XL's
    display driver, run at the IIgs's exact rate; Resolution in the Video
    menu (the Pi 5's firmware ignores `hdmi_mode`); HDMI sound; the power
    button shuts down (docs/PI5.md)
  - Raspberry Pi Zero and Zero W (`kernel.img`), on their one core (likely
    too slow for the IIgs)
  - The Pi 4 and 5's 3D GPU draws the CRT effect and smooth scaling, at the
    full 1080 lines (`crt_gpu=0` for the ARM, as before)
  - CRT Curvature in the Video menu
  - Fix the Pi 2 and 3 starting at 1080p instead of 720p
  - Logging is off unless `enable_logging` is set; File logs to the serial
    port too, and the log file is complete after a crash, a hang or
    switching off

## 0.2.0 – 2026-10-15

- Machine choice in the F10 menu (and `machine=`): Apple IIgs ROM 3, ROM
  01, or Apple IIe, of those there are ROMs for
- Monitors (Video, Monitor, and `monitor=`): colour, or black and white,
  green or amber monochrome, with Hi-Res drawn as sharp dots
- Volume (Sound, Volume, and `volume=`), for the IIe too
- The web UI starts 5.25" disks on a IIgs: its startup slot is set to 6 for
  that start (a IIgs set to Scan never goes on to slot 6)
- APPLE2E.ROM can also be made from the unenhanced IIe's MAME chips
- The PC simulator's live mode has the mouse, and plays stereo sound right

## 0.1.0 – 2026-10-15

- First version: BMApple2, based on BMCoco 0.2.0 (itself based on BMA800XL
  1.1.0 and BMC64), with XRoar replaced by KEGS 1.38
- Apple IIgs, with ROM 01 or ROM 3, and Apple IIe, with a IIe ROM
- ROM 3 and the IIe ROM made from MAME's ROM chips (Ample's ROM set on the
  Mac too) the first time BMApple2 starts
- Raspberry Pi 2, 3, Zero 2 W, 4 B and 400
- Frames locked to the display's refresh (59.92 Hz; on a Pi 4 the display
  is set to that exactly), with the sound kept in step
- USB keyboards (as an ADB keyboard), USB mice, gamepads as the joystick
  (analog stick or D-pad), GPIO joysticks
- KEGS's own settings (F4) for disks, ROM, speed and the rest; BMApple2's
  menu (F10), in the Apple II's font, for the Pi: video, sound, network,
  restart
- The machine's own speed by default: 2.8 MHz for a IIgs, 1 MHz for a IIe
- Web UI, in Apple II colours: manage the SD card, edit config.kegs, and
  start a disk image (put in the drive that boots it, and the machine
  restarted)
- PC test harness with a stand-in 6502 ROM, and a live mode in a web page
