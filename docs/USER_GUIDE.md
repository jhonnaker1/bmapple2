# BMApple2 User Guide

BMApple2 turns a Raspberry Pi into an Apple IIgs (or an Apple IIe). It runs
the KEGS emulator by Kent Dickey on the bare hardware, so it starts in a
couple of seconds and can be switched off at any time.

## The SD card

| File or folder   | What it is                                                  |
|------------------|-------------------------------------------------------------|
| `kernel8-32.img`, `kernel7.img`, `kernel.img`, `kernel8-rpi4.img`, `kernel_2712.img` | BMApple2 for the Pi 3 and Zero 2 W, the Pi 2, the Pi Zero and Zero W, the Pi 4 and Pi 400, and the Pi 5, 500 and 500+ |
| `bootcode.bin`, `start.elf`, `fixup.dat`, `start4.elf`, `fixup4.dat`, `armstub8-rpi4.bin`, `*.dtb`, `overlays/` | Raspberry Pi firmware |
| `config.txt`     | Raspberry Pi firmware settings: the display mode            |
| `cmdline.txt`    | BMApple2's options (see Options)                            |
| `bmapple2.cfg`   | Settings changed in BMApple2's menu (written by BMApple2)   |
| `config.kegs`    | KEGS's settings: the disks in each drive, the ROM, speed, joystick, the IIgs's battery RAM (written by KEGS) |
| `roms/`          | The IIgs or IIe ROM: you supply it (see ROMs)               |
| `disks/`         | For your disk images (any folder works)                     |
| `firmware/`      | Wi-Fi firmware                                              |

KEGS writes to disk images as the emulated machine writes to its disks, and
BMApple2 pushes everything out to the card every second, so the Pi can be
switched off at any time (give it a second after saving).

## ROMs

The Apple ROMs are copyrighted, so BMApple2 doesn't include them. Copy your
own into `roms` (or the root of the card):

| Machine            | ROM file                                        |
|--------------------|-------------------------------------------------|
| Apple IIgs ROM 3   | `ROM.03` (or `APPLE2GS.ROM2`, `Rom03gd`), 256K  |
| Apple IIgs ROM 01  | `ROM.01` (or `APPLE2GS.ROM`, `xgs.rom`, `342-0077-b`), 128K |
| Apple IIe          | `APPLE2E.ROM`, 32K: the 16K IIe ROM in its last 16K, the Disk II card's PROM at $600 (the form other emulators use) |

BMApple2 starts the first it finds: ROM 3, then ROM 01, then the IIe.
Choose another under *Machine* in the **F10** menu (or with `machine=` in
`cmdline.txt`); the choice is kept.

**From a MAME ROM set** (such as Ample's on the Mac, in
`~/Library/Application Support/Ample/roms`): MAME keeps the ROM chips rather
than ROM files. Copy these out of the zip files into `roms`, and BMApple2
makes `ROM.03` and `APPLE2E.ROM` from them the first time it starts:

| Machine            | From MAME's zip files                           |
|--------------------|-------------------------------------------------|
| Apple IIgs ROM 3   | `341-0728` and `341-0748` from `apple2gs.zip`   |
| Apple IIgs ROM 01  | `342-0077-b` from `apple2gs.zip` (in `apple2gsr1`), used as it is |
| Apple IIe          | `342-0304-a.e10` and `342-0303-a.e8` from `apple2e.zip` (in `apple2ee`), the enhanced IIe, or `342-0135-b.64` and `342-0134-a.64` from the same zip, the original IIe; and `341-0027-a.p5` from `a2diskiing.zip` |

## Disks

KEGS has the IIgs's drives: **slot 7** for hard disks (up to 16 of them),
**slot 5** for 3.5" disks (two drives) and **slot 6** for 5.25" disks (two
drives). It reads ProDOS order (`.po`), 2IMG (`.2mg`), hard disk (`.hdv`),
DOS order (`.dsk`, `.do`), nibble (`.nib`) and WOZ (`.woz`) images, and
ShrinkIt archives (`.shk`, `.sdk`).

Press **F4** for KEGS's settings, choose *Disk Configuration*, move to a
drive and press Return to choose a disk image for it. The disks are
remembered in `config.kegs`. Then restart the machine: *Restart* in the
**F10** menu.

On a IIgs the startup slot is set in its Control Panel (Ctrl+Open Apple+Esc,
then *Slots*). With the default, *Scan*, it starts slot 7, or slot 5 if
slot 7 has no disk; it doesn't go on to slot 6. To start a 5.25" disk, set
the startup slot to 6, or type `PR#6` at the `]` prompt. A IIe goes on from
slot 7 to slot 6 by itself.

From the web UI, clicking a disk image starts it: BMApple2 puts it in drive
1 of the slot that boots it (5.25" disks in slot 6, 800K disks in slot 5,
anything bigger in slot 7), empties drive 1 of the slots above it (so it is
the one that starts), and restarts the machine. For a 5.25" disk on a IIgs
it sets the startup slot to 6 for that start, and back to what it was once
the disk has started.

## The menus

**F4** opens KEGS's own settings: disks, the ROM, the speed, the joystick,
the Mockingboard (slot 4), serial ports and more, and the IIgs's video
options. Its own screen explains its keys; F4 closes it.

**F10** (or a gamepad's **Home** button, or **Select+Start**) opens
BMApple2's menu; **Esc** goes back a page and F10 closes it. The arrow keys
or D-pad move, **Return** or **A** chooses.

| Item           | What it does |
|----------------|--------------|
| KEGS Settings  | KEGS's settings, as F4 |
| Reset          | The reset key (Ctrl+F12) |
| Restart        | Starts the machine again, as switching it off and on does: it boots from its disks |
| Machine        | Apple IIgs with ROM 3, IIgs with ROM 01, or Apple IIe: those there are ROMs for (see ROMs). Choosing one starts it |
| Video          | The monitor (colour, or a black and white, green or amber monochrome one), the screen size (smaller for a TV that cuts off the edges), the frame lock, the resolution (Pi 5), the CRT effect and its curvature, smooth scaling, and the disk activity light |
| Sound          | The volume, and HDMI or the headphone jack |
| Network        | Ethernet or Wi-Fi, the web UI (see below) |
| About          | Versions, the ROM, display and network |
| Reboot, Shut Down | Restart BMApple2, or halt the Pi. Shut Down also from a keyboard's power key: Fn+F10 on a Pi 400, which switches off; Fn+F10 starts it again |

Settings changed in BMApple2's menu are kept in `bmapple2.cfg` and used at
the next boot. KEGS keeps its own in `config.kegs`.

## Keyboard and mouse

| PC key                 | Apple                                         |
|------------------------|-----------------------------------------------|
| F10                    | BMApple2's menu                               |
| F4                     | KEGS's settings                               |
| Ctrl+F12               | Reset                                         |
| Windows (Command on a Mac keyboard), F1 | Open Apple (Command)         |
| Alt, F2                | Option (Solid Apple)                          |
| F3                     | Escape                                        |
| F5                     | KEGS's status lines (speed and so on)         |
| F6                     | Speed: 1 MHz, 2.8 MHz, 8 MHz (ZipGS), unlimited |
| Shift+F6, F7           | KEGS's debugger (not useful without a host terminal) |
| F9                     | Swap or invert the joystick's axes (with Shift, without) |
| Pause                  | F15                                           |
| Insert                 | Help                                          |

The other keys are where they are on an Apple keyboard. The IIgs reads the
keyboard itself, in its own layout (its Control Panel chooses one).

A **USB mouse** is the IIgs's mouse.

## Speed

KEGS's default is an 8 MHz ZipGS accelerator; BMApple2's is the machine's
own speed instead: **2.8 MHz** for a IIgs, **1 MHz** for a IIe. Old games
then run as fast as they should, and the Pi 2 and 3 keep up. **F6** (or F4,
*Speed*) changes it, and KEGS keeps the choice in `config.kegs`.

## Joysticks

The Apple II's joystick is analog: two paddles and two (or three) buttons.
BMApple2 has two joysticks; the first is paddles 0 and 1, the second
paddles 2 and 3. Each takes its input from:

- a USB gamepad (gamepad 1 for the first, gamepad 2 for the second). Its
  left analog stick gives the position as it is, the whole range the Apple
  reads; the D-pad pushes to the edges.
- a joystick wired to the GPIO header (see below), which also pushes to the
  edges.

| Gamepad button  | Apple                             |
|-----------------|-----------------------------------|
| A               | Button 0 (gamepad 2: button 2)    |
| B               | Button 1                          |
| X               | Space                             |
| Y               | Return                            |
| Home, Select+Start | BMApple2's menu                |

KEGS's *Joystick* setting (F4) must be *Native Joystick 1*, which is
BMApple2's default; KEGS's own default is the keypad (the numeric keypad's
keys move it), and choosing it there still works.

**Joysticks on the GPIO header**: set `gpio_joysticks=1` in `cmdline.txt`,
then reboot. The wiring is BMC64's "GPIO Config 1". The pins are active low
(each switch connects the pin to ground) and BMApple2 turns on the Pi's
pull-ups:

| Signal | Joystick 1 (GPIO) | Joystick 2 (GPIO) |
|--------|-------------------|-------------------|
| Up     | 17                | 5                 |
| Down   | 18                | 6                 |
| Left   | 27                | 12                |
| Right  | 22                | 13                |
| Fire   | 23                | 19                |

In the menus either GPIO joystick moves the cursor and fire chooses, as a
gamepad's D-pad and A do.

Ground is on pins 6, 9, 14, 20, 25, 30, 34 or 39 of the header. These are
digital (switch) joysticks, like an Atari-style stick with an adapter; an
Apple II's own analog joystick needs an analog to digital converter, which
the Pi doesn't have.

## Display and timing

The IIgs (and KEGS's IIe) shows 59.92 frames a second, and `config.txt`
sets the display to 720p at 60 Hz to match. When the display's refresh
rate is within 2% of that, each frame is shown on exactly one display
refresh, which gives perfectly smooth scrolling (the IIgs then runs 0.1%
fast, which nothing notices). Otherwise the emulation keeps exact time and
frames are shown as they come. On a Pi 4 BMApple2 sets the display to the
IIgs's exact rate itself. `vsync=on` or `vsync=off` in `cmdline.txt` (or
*Frame Lock* in the menu) forces one or the other.

The picture is KEGS's: the 640x400 Apple area (Super Hi-Res is 640x200 or
320x200, the other modes 560x192 or 280x192, all doubled up) and its
border, shown 4:3 as on an Apple monitor.

**Monitor** (in *Video*): a colour monitor, or a monochrome one, in white,
green or amber. A monochrome monitor shows each colour as its brightness,
and Hi-Res as sharp dots rather than colours (BMApple2 keeps the Apple's
monochrome switch on, as a IIgs's Control Panel does when set to a
monochrome monitor).

## Networking and the web UI

BMApple2 can join your network over Ethernet or Wi-Fi and serve a web page
for managing the SD card from any browser on the same network, so files can
be copied to and from the Pi without taking the card out. It is the web UI
from BMC64.

Turn it on in the menu's *Network*. For Ethernet, set *Network* to Ethernet
and reboot. For Wi-Fi, choose *Scan for Wi-Fi Networks*: the first time,
it offers to switch Wi-Fi on and reboot; after that it lists the networks
it can hear, strongest first. Pick yours: it fills in *Wi-Fi Network*, and
if the network needs a password, *Wi-Fi Password* is next. Then reboot.
Set *Wi-Fi Country* to your two letter country code too; it decides which
channels are allowed. A hidden network can be typed in under *Wi-Fi
Network* instead. Or, in `cmdline.txt` (on the same single line as the
other options):

- Ethernet: `network=ethernet`
- Wi-Fi: `network=wifi`, and copy `wpa_supplicant.conf.example` to
  `wpa_supplicant.conf` in the root of the card with your network's name,
  password and country.

Wi-Fi needs the files in the `firmware` folder, which `build_sdcard.sh` puts
there.

Once the Pi has an address, it is shown at the bottom of the screen for a
few seconds (`WEB UI: HTTP://192.168.1.50/`) and written to the log.
Open that address in a browser.

The web UI has:

- **Dashboard**: the Pi's model, address, uptime, temperature, power and
  throttling warnings and free space on the card. Buttons to **Restart**
  the Apple (as switching it off and on), **Reboot** BMApple2, **Shut
  Down** (the Pi halts; switch it off, then on again to restart; a Pi 400
  switches itself off, and its power key starts it again), and **Disable
  Web UI** until the next reboot.
- **Files**: browse the SD card; upload (or drag and drop) files into a
  folder, download, rename, delete, and create folders. Clicking a disk
  image **starts it** (see Disks). `cmdline.txt`, `config.txt`,
  `bmapple2.cfg`, `config.kegs` and `wpa_supplicant.conf` can be **edited**
  in the browser; changes take effect after a reboot, and the previous
  version is kept as `.bak`.

Uploads can't overwrite `cmdline.txt`, `config.txt` or `wpa_supplicant.conf`
(edit them instead), or anything in `firmware`. Everything else, including
the kernels, can be replaced, so a new build of BMApple2 can be uploaded and
started with **Reboot**. A mistake in `cmdline.txt` or `config.txt` can stop
the Pi starting, and then needs the card in a computer (the `.bak` copy
makes that easy).

The web UI is open to anyone on your network. To protect it with a PIN, set
`webui_pin=1234` in `cmdline.txt`; the browser then asks for a user name
(anything) and the PIN. To network without the web UI, set `webui=0`.

The network also sets the Pi's clock, so files saved on the card get the
right date and time. Set `timezone` to your offset from UTC in minutes, for
example `timezone=600` for UTC+10. (The IIgs keeps its own clock, set in
its Control Panel.)

### Calling BBSes: the virtual modem

With the network on, the IIgs's modem port (slot 2) has a virtual modem on
it, KEGS's: a Hayes-compatible modem that "dials" telnet BBSes and other
servers on the internet. Use it from a terminal program such as ProTERM,
Spectrum or Talk is Cheap, set to the modem port. Type:

```
ATDTbbs.example.com:23
```

with the BBS's name or IP address and port (23, telnet's, if there is
none). The call is made in the background, so the Apple keeps running
while it connects: `CONNECT 2400` when it does, `NO CARRIER` if nobody
answers (which can take a few minutes for an address where nothing is
there at all). Then type away; the modem's speed is whatever the terminal
program sets.

Other commands: `+++` (wait a second before and after) to get back to the
modem, `ATH` to hang up, `ATO` to go back online, `ATZ` to reset it (and
turn on the word replies), `ATE0` / `ATE1` echo off and on, `ATV0` / `ATV1`
number or word replies.

Terminal programs often try to set the modem up themselves and may
complain that they can't; KEGS's notes suggest carrying on, forcing the
program online (ProTERM: Cmd-Ctrl-T) and typing `ATZ`.

The serial ports are set up in KEGS's configuration (**F4**, *Serial Port
Configuration*): slot 1 (the printer port) is off by default and slot 2
the virtual modem. *Use Remote IP* connects a port straight to the address
and port given there instead, as soon as the Apple sends something (`PR#1`
from BASIC, say; set *Serial Mask* to *Mask off high bit* for plain text).
Incoming connections (the modem answering calls, and *Use incoming port
6501*) aren't supported on the Pi.

## Options

`cmdline.txt` is a single line of `name=value` options separated by spaces.
BMApple2's menu saves the options it changes to `bmapple2.cfg`, one
`name=value` per line, and those win over `cmdline.txt`. Either file can be
edited from the web UI. Everything about the emulated machine is in KEGS's
settings (F4) and `config.kegs` instead.

| Option              | Values                       | Default  | What it does |
|---------------------|------------------------------|----------|--------------|
| `aspect`            | number                       | `1.0`    | Multiplies the width of the picture |
| `disk_activity`     | `0`, `1`                     | `1`      | While a 5.25" or 3.5" drive turns, show its slot, drive and track at the bottom left (Video, Show Disk Activity) |
| `scale`             | 0.1 to 1.0                   | `1.0`    | Height of the picture as a fraction of the screen (Video, Screen Size; less for a TV that cuts off the edges) |
| `smooth`            | `0`, `1`                     | `0`      | Bilinear filtering instead of sharp pixels |
| `crt_shader`        | `0`, `1`                     | `0`      | CRT look (scanlines) from BMC64 |
| `crt_curvature`     | `0`, `1`                     | `0`      | With `crt_shader`, a curved screen |
| `crt_gpu`           | `0`, `1`                     | `1`      | Pi 4 and 5: the 3D GPU draws the smooth and CRT pictures; `0` has the ARM draw them (see [PI4.md](PI4.md)) |
| `vsync`             | `auto`, `on`, `off`          | `auto`   | Lock frames to the display (see Display and timing) |
| `resolution`        | `auto`, `1920x1080`, `1280x720` | `auto` | Pi 5 only, with `video_driver=kms`: the display's resolution; `auto` keeps the one the firmware chose (the Pi 5's firmware ignores `hdmi_mode`) |
| `video_driver`      | `kms`, `hvs`, `firmware`     | `kms`    | Pi 4 and 5: `kms` runs the display at the IIgs's exact frame rate; `hvs` leaves the mode set in `config.txt`; `firmware` uses the firmware's framebuffer, if the others give trouble (see [PI4.md](PI4.md) and [PI5.md](PI5.md)) |
| `machine`           | `iigs3`, `iigs1`, `iie`      | first with a ROM | Machine to emulate (see ROMs) |
| `monitor`           | `color`, `white`, `green`, `amber` | `color` | The monitor (see Display and timing) |
| `sound`             | `0`, `1`                     | `1`      | Sound on or off |
| `volume`            | 0 to 100                     | `100`    | Sound volume (a IIgs's Control Panel has its own too) |
| `audio_out`         | `auto`, `hdmi`, `headphones` | `auto`   | Where the sound goes (`auto`: HDMI, or the headphone jack if the display takes no sound) |
| `audio_buffer_ms`   | 20 to 500                    | `60`     | Sound buffer; raise it if the sound breaks up |
| `gpio_joysticks`    | `0`, `1`                     | `0`      | Joysticks on the GPIO header |
| `enable_logging`    | `off`, `uart`, `file`        | `off`    | Where the log goes (see Logging) |
| `network`           | `off`, `ethernet`, `wifi`    | `off`    | Networking (see Networking and the web UI) |
| `webui`             | `0`, `1`                     | `1`      | Web UI when networking is on |
| `webui_pin`         | digits                       | none     | PIN for the web UI |
| `timezone`          | minutes east of UTC          | `0`      | Time zone for the network clock |
| `usbpowerdelay`     | milliseconds                 | Circle's | Circle: wait for USB devices to power up (by default 300 ms, 510 ms behind a hub) |

A IIgs also has its own volume, in its Control Panel; a IIe has none, so
use `volume` (or *Sound*, *Volume* in the menu).

## Logging

BMApple2 logs what it is doing, which helps when something doesn't work:
the version, the display and sound set up, the ROM it found, devices coming
and going, network status, and KEGS's own messages (disks inserted, and
anything it didn't like). Choose where it goes with `enable_logging` in
`cmdline.txt`:

- **Off** (the default, also when `enable_logging` isn't set): no log.
- **UART**: the serial port, GPIO14 TX and GPIO15 RX at 115200 baud
  (all boards, the Pi 5 too). Needs a USB serial adapter.
- **File**: the serial port, and `bmapple2.log` in the root of the SD card.
  The file starts with what was logged since boot, is written to the card
  every few seconds (and is complete after a crash, a hang or switching
  off), and starts again at boot once it is over 1 MB. Download it from
  the web UI's file manager to send with a bug report. Writing it costs a
  little time on each frame, so switch it off once you have what you need.

## Troubleshooting

**KEGS's settings screen opens by itself at boot**: it has no ROM. See
ROMs.

**"Check startup device!"**: the IIgs found no disk to start. Put one in a
drive with F4 (see Disks).

**The screen is black, or black text on black**: the IIgs's battery RAM in
`config.kegs` may be blank (all the `bram` lines zero), for example after
starting with a bad ROM; it then sets black text on a black background.
Delete the `bram1` and `bram3` lines from `config.kegs` (in the web UI's
editor) and reboot, or set the colours in the IIgs's Control Panel
(Ctrl+Open Apple+Esc).

**No picture**: uncomment `hdmi_safe=1` in `config.txt`. If that works, your
display doesn't accept 720p at 60 Hz; try another `hdmi_mode`.

**No sound over HDMI**: try `hdmi_drive=2` in `config.txt`, or
`audio_out=hdmi` in `cmdline.txt`. Check the IIgs's volume in its Control
Panel.

**Sound breaks up**: raise `audio_buffer_ms` in `cmdline.txt`, e.g. to 100.

**A terminal program can't reach a BBS**: check that the network is on (its
address shows at the bottom of the screen at start up), that the program
uses the modem port (slot 2), and the BBS's address and port: `ATDT` then
the name or IP address, a colon, and the port. `NO CARRIER` means nothing
answered there.

**A program seems stuck loading**: watch the bottom left of the screen.
While a 5.25" or 3.5" drive turns, a red light shows there (lit while the
disk is read) with the slot, drive and track, as in `S6 D1 T17` (and the
side for a 3.5" disk). A track that doesn't move usually means the wrong
disk or a bad image; `no disk` in red means the drive is empty, which a IIe
starting up with no disk in drive 1 waits on for ever. Hard disks aren't
shown.

**The TV cuts off the top and bottom of the picture or the menu**: many TVs
enlarge the picture and crop its edges (overscan). Make *Video*, *Screen
Size* smaller (90% usually does), or turn overscan off on the TV ("Just
Scan", "Screen Fit" or "1:1").

**Keyboard, mouse or gamepad not working**: unplug it and plug it in again;
devices can come and go while the emulator runs. Some keyboards need a
longer `usbpowerdelay` (add `usbpowerdelay=1000` to `cmdline.txt`).

**8BitDo M30 2.4G buttons are wrong**: its receiver can be left in Switch
mode, where it acts as a Switch Pro controller. With the pad connected,
hold Home (the heart) and D-pad Down for 5 seconds until the LEDs flash; it
goes back to its own mode and keeps it.

**The joystick doesn't move**: KEGS's *Joystick* setting (F4) should be
*Native Joystick 1*.

**Games run too fast**: press F6 until KEGS's status line (F5) says 1 MHz.
