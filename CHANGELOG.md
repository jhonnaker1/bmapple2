## 0.2.0 – 2026-10-13

- From BMA800XL 1.1.0: Raspberry Pi 4 B and Pi 400 support
  - BMA800XL's own display driver, on either HDMI port, with the display
    run at an NTSC CoCo 1/2's exact refresh rate (59.92 Hz); the CoCo 3
    and PAL CoCos lock to 60 or 50 Hz, as on a Pi 3 (docs/PI4.md)
  - CRT shader and smooth scaling
  - Sound on either HDMI port or the headphone jack
  - Pi 400: Shut Down switches it off; Fn+F10 shuts down
- A keyboard's power key shuts down
- Fix the smooth setting being stuck on for the Pi 2 and 3 (from BMA800XL)
- The Pi 4 scaler and display driver tests run in CI

## 0.1.1 – 2026-10-12

- Web UI in Color Computer colours: the VDG's green text screen, its
  orange, and its eight colours as a colour bar (it had BMA800XL's Atari
  colours)

## 0.1.0 – 2026-10-12

- First version: BMCoco, forked from BMA800XL 1.0.1, with the atari800
  emulator replaced by XRoar 1.12.1
- Tandy Color Computer 1, 2 and 3 (and the Dragon and MC-10, from XRoar)
- Frames locked to the display refresh; timer pacing otherwise
- NTSC artifact colours, S-Video and RGB
- Disks (RS-DOS), tapes, cartridges and machine code programs
- USB keyboards (typed characters or the CoCo's layout), gamepads as analog
  joysticks, GPIO joysticks
- On-screen menu (F12) drawn in the CoCo 3's font
- Ethernet / Wi-Fi with BMC64's web UI
- Builds on Linux and macOS
