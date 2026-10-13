# Changelog

## 0.1.0 – 2026-10-13

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
