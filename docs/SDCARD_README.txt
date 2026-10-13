BMApple2 - bare metal Apple IIgs and Apple IIe emulator for the Raspberry Pi
============================================================================

Copy everything here to the root of a FAT32 SD card and boot a Raspberry
Pi 4, 400, 3, Zero 2 W or 2 from it, with a USB keyboard (and a mouse for
the IIgs desktop, and a gamepad if you like) connected.

First copy an Apple IIgs ROM (ROM.03 or ROM.01) or an Apple IIe ROM into
the roms folder (see roms/ROMS.txt). They are copyrighted, so they are not
included.

Disks: press F4 for KEGS's settings, Disk Configuration, and choose a disk
image for a drive (slot 7 for hard disks, 5 for 3.5", 6 for 5.25"). They
are remembered in config.kegs. Or start one from the web UI.

  F10           BMApple2's menu       F4            KEGS settings (disks, ROM)
  Ctrl+F12      Reset                 F6            Speed
  Windows/Cmd   Open Apple            Alt           Option (Solid Apple)
  F1, F2        Open Apple, Option    F3            Escape

Gamepads are the joystick (paddles): stick or D-pad to move, A and B the
buttons, X Space, Y Return, Home (or Select+Start) for the menu.

Put your disk images in the disks folder (or anywhere).

Networking: choose Ethernet or Wi-Fi under Network in the menu (or add
network=ethernet or network=wifi to cmdline.txt), then open the address
shown on screen in a browser to manage the files on this card and start
disk images.

Options are in cmdline.txt; see docs/USER_GUIDE.md in BMApple2's source for
all of them.
