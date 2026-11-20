# Changelog

## 0.4.7 – unreleased

- FujiNet: a FujiNet-PC on another computer, built for the Apple II, can
  connect to BMApple2 (*FujiNet* in the Network menu, or `fujinet=1`), as
  it does to FujiNet Go's AppleWin ("SmartPort over SLIP", port 1985). Its
  devices (its disks, network, clock, printer, modem, and the FujiNet that
  its CONFIG program talks to) become slot 7's first SmartPort units, and
  slot 7 boots its CONFIG; the disks loaded in slot 7 follow them. The
  User Guide says how. Tried with FujiNet-PC on a Mac: a IIgs boots
  CONFIG, which lists fujinet.online's folders, mounts a disk from it and
  boots it, and the program on it (an ISS tracker) fetches the station's
  position through FujiNet's network device
- Tests: a stand-in FujiNet: booting from it, block reads and writes,
  status, control calls, the disks in slot 7 after its units, booting the
  slot's own disk when FujiNet is off or doesn't come, and a FujiNet that
  goes away

## 0.4.6 – 2026-11-19

- Fixed: Reboot and Shut Down left a call made with the virtual modem
  open at the BBS's end. The connections are now closed, and the far end
  told, before the Pi restarts or halts (as BMCoco 0.5.7 does)
- Fixed: changing a Wi-Fi setting in the menu (the network, password or
  country) spoiled a Wi-Fi key of 64 hex digits in `wpa_supplicant.conf`
  (as `wpa_passphrase` writes it): it was cut to 63 and put in quotes, so
  Wi-Fi stopped working. A password with a `"` in it was cut short too.
  Both are now written back as they were
- Fixed: Esc in the menu's Machine list closed the whole menu; it now goes
  back to it
- Time Zone in the Network menu has the zones a half or three quarters of
  an hour off (UTC+5:30, UTC+5:45, UTC-3:30, ...), and shows one set in
  `cmdline.txt` as it is; before, it had whole hours only
- The menu's file browser, unused since KEGS has its own, is gone
- Tests: a reboot closes the modem's call; the Wi-Fi key and a password
  with a `"` kept; Esc in the Machine list; a half-hour time zone
## 0.4.5 – 2026-11-19

- USB sticks, from BMA800XL: up to three FAT sticks, as USB:, USB2: and
  USB3:. KEGS's file browser (F4) shows each as a folder in the card's
  top folder while it is in, and the web UI's Files page has a button for
  each. Pulling a stick out ejects the disks loaded from it; config.kegs
  keeps them for the next boot
- Pi 5: the fan is run by the SoC's temperature (off below 50 °C, faster
  in four steps up to 75 °C), as on Raspberry Pi OS; before it stayed off
  (from BMA800XL 1.4.3, after BMC64)
- From BMA800XL 1.4.3: the Pi 5's power button code is in src/pi45, and
  THIRD_PARTY.md says which firmware release the SD card's is. Its USB
  paddles (Atari) are left out, and BMApple2 already ran the ARM at its
  full clock and drew RGB565 pictures on the GPU
- Web UI uploads are about 5 times faster: 64 KB written to the card at a
  time, and a larger TCP receive window (from BMA800XL)
- The files from BMA800XL carry its licence notices (GPL 3, or BMC64's
  Apache 2.0 with aminch's changes), followed by "Modified for BMApple2"
- Fixed, from BMA800XL: web UI uploads of several MB could stop with
  "upload truncated". The TCP receive window is now Circle's 10 segments
  on Ethernet (the Pi 3's USB Ethernet drops the end of larger bursts)
  and 44 on Wi-Fi
- Fixed, from BMA800XL: on a Pi 5, sound could play at the wrong pitch
  when the display was already in the wanted mode, or didn't list it
- The web UI's file view sorts by name, size or date (click the column,
  again to reverse it), from BMA800XL
- Fixed, from BMA800XL: the web UI could fail to load in Safari, which
  opens spare connections early: one that sent nothing for 2 s was
  closed, just as a request might arrive on it. Idle connections now stay
  open until one sends a request (up to 8, for 30 s)
- Tests: USB sticks in the card's top folder, and pulling one out

## 0.4.4 – 2026-11-11

- Fixed: Caps Lock didn't stay on: capitals needed Shift held, and games
  that want a capital (Alien Typhoon fires with A) didn't take the key. The
  Apple's Caps Lock locks down, a PC's springs back; each press now turns
  it on or off, as in KEGS's Windows version, with the keyboard's Caps
  Lock light showing which (GitHub issue 2)
- Fixed: a gamepad's X button typed Space, and Y Return, so a pad whose
  fire button reports as X sent a key to a game waiting for one (as BMCoco
  0.5.6 fixed). X is now button 0 as A is (button 2 on the second gamepad),
  and Y button 1 as B is; `pad_keys=1` in `cmdline.txt` brings back Space
  and Return
- Tests: Caps Lock locks on and off, with its light;
  the gamepad's X with and without `pad_keys`

## 0.4.3 – 2026-11-09

- Fixed: BMApple2 never started on a Raspberry Pi: a black screen, and no
  log even with `enable_logging=file`. Its kernels are larger than the
  memory Circle sets aside for them (KEGS's debugger logs, which BMApple2
  doesn't use, are 128 MB of it), and Circle stops at once when a kernel
  doesn't fit, before anything can be shown or logged. The space is now
  160 MB, and the build fails if a kernel doesn't fit. Shown in QEMU: 0.4.2
  stops in Circle's start-up with nothing on the serial port, the fixed
  kernels start BMApple2 (Pi Zero and Pi 4)

## 0.4.2 – 2026-10-20

- Fixed: the mouse's right button froze the IIgs, and its wheel's click
  changed the emulation's speed. KEGS takes its desktop versions' middle
  button as a request to stop in its debugger and the right one as a speed
  change, and the USB mouse's buttons came in another order; the IIgs mouse
  has one button, so only the left one is passed on now
- Documentation: the installation guide lists all five kernels (and a Pi
  4 or 5's "not found" screen when they're in a folder), the User Guide's
  Video menu has Resolution and CRT Curvature, and the SD card's README
  mentions the modem

## 0.4.1 – 2026-10-19

- The virtual modem works on the Pi: from a terminal program on the modem
  port (slot 2), `ATDTbbs.example.com:23` calls a telnet BBS. KEGS only
  built its network code for Linux, macOS and Windows, so on the Pi the
  modem did nothing; BMApple2 now has its own copy of it, on Circle's
  sockets, dialling in the background so that the Apple keeps running
  while it connects (`CONNECT`, or `NO CARRIER`). *Use Remote IP* works
  too; incoming connections aren't supported. See "Calling BBSes" in the
  User Guide
- Tests: dialling a stand-in BBS through the modem, and a call nobody
  answers

## 0.4.0 – 2026-10-19

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
