# BMCoco User Guide

BMCoco turns a Raspberry Pi into a Tandy Color Computer. It runs the XRoar
emulator by Ciaran Anscomb on the bare hardware, so it starts in a couple of
seconds and can be switched off at any time.

## The SD card

| File or folder   | What it is                                                  |
|------------------|-------------------------------------------------------------|
| `kernel8-32.img`, `kernel7.img` | BMCoco for the Pi 3 / Zero 2 W, and the Pi 2  |
| `config.txt`     | Raspberry Pi firmware settings: the display mode            |
| `cmdline.txt`    | BMCoco's options (see Options)                              |
| `bmcoco.cfg`     | Settings changed in the menu (written by BMCoco)            |
| `xroar.conf`     | Optional: XRoar's own configuration file, for anything else |
| `roms/`          | The CoCo's ROMs: you supply them (see ROMs)                 |
| `disks/`, `tapes/`, `carts/`, `programs/` | For your CoCo files (any folder works) |
| `firmware/`      | Wi-Fi firmware                                              |

Changed disk images are written back to the card two seconds after the CoCo
last wrote to them (the first time, the original is kept as `.bak`). Wait
that long after saving before switching off.

## ROMs

The CoCo's ROMs are copyrighted, so BMCoco doesn't include them. Copy your
own into `roms` (or the root of the card). XRoar finds them by name:

| Machine            | ROM files                                       |
|--------------------|-------------------------------------------------|
| CoCo 1/2           | `bas13.rom` and `extbas11.rom` (older: `bas10`-`bas12`, `extbas10`) |
| Disks (RS-DOS)     | `disk11.rom` (or `disk10.rom`)                  |
| CoCo 3             | `coco3.rom`                                     |
| MC-10              | `mc10.rom`                                      |
| Dragon 32 / 64     | `d32.rom` / `d64_1.rom`, `d64_2.rom`            |

BMCoco starts the first of these it has ROMs for: CoCo 2 (NTSC), CoCo
(NTSC), CoCo 3, ... If it has none, it says which files it is missing.

All the CoCo ROMs can be downloaded from the
[Color Computer Archive](https://colorcomputerarchive.com/repo/ROMs/XRoar/CoCo/),
already named this way. Copy the files from both its `BASIC_OS` folder
(Color BASIC, Extended BASIC, CoCo 3) and its `DOS` folder (Disk BASIC)
into `roms`.

## The menu

**F12** (or a gamepad's **Home** button, or **Select+Start**) opens the
menu; **Esc** goes back a page and F12 closes it. The arrow keys or D-pad
move, **Enter** or **A** chooses. In a list, typing a letter jumps to the
next item starting with it.

| Item           | What it does |
|----------------|--------------|
| Run a Program  | Choose a disk, tape, cartridge or program and start it: XRoar inserts it and types the commands to run it (`RUN"..."` from a disk, `CLOAD` from a tape, ...) |
| Disk Drives    | Insert or eject a disk in drives 0-3, and write protect it |
| Cassette       | Insert a tape (then type `CLOAD` or `CLOADM`), rewind, eject, fast loading |
| Cartridge      | Plug in a cartridge file, one of XRoar's built in cartridges (Disk BASIC and others), or none |
| Machine        | CoCo 1, 2 or 3, PAL or NTSC, with or without a 6309, and the other machines XRoar emulates. Machines without ROMs are marked |
| Joysticks      | What is plugged into each joystick port, which gamepad is which joystick, GPIO joysticks, and a joystick test |
| Video          | TV input (composite with artifact colours, S-Video, or RGB for the CoCo 3), the artifact colour renderer, how much border to show, the frame lock, CRT effect and smooth scaling |
| Keyboard       | Typed characters on or off, and the PC keyboard's layout |
| Sound          | Volume, and HDMI or the headphone jack |
| Network        | Ethernet or Wi-Fi, the web UI (see below) |
| Turbo          | Run as fast as the Pi can (also F11) |
| Reset, Hard Reset | The reset button (F10), or power off and on (Shift+F10) |
| About          | Versions, the machine, ROM status, display and network |
| Reboot, Shut Down | Restart BMCoco, or halt the Pi |

Settings changed in the menu are kept in `bmcoco.cfg` and used at the next
boot.

## Keyboard

| PC key                 | CoCo                                          |
|------------------------|-----------------------------------------------|
| F12                    | BMCoco menu                                   |
| F11                    | Turbo on/off                                  |
| F10, Shift+F10         | Reset, hard reset                             |
| Esc                    | BREAK                                         |
| Home                   | CLEAR                                         |
| Arrow keys, Tab        | Arrow keys (Tab is right arrow)               |
| Backspace, Delete      | Left arrow (backspace)                        |
| Shift+Backspace        | Erase the line                                |
| Shift+Enter            | Caps lock (SHIFT+0)                           |
| Alt                    | CoCo 3 ALT                                    |
| Caps Lock, Windows     | CoCo 3 CTRL                                   |
| F1, F2                 | CoCo 3 F1, F2                                 |

By default keys type the characters printed on them, in the layout set with
`keyboard=` in `cmdline.txt` (or *Keyboard* in the menu): Shift+' types `"`
and Shift+8 `*`, as you would expect. Some games read the keyboard matrix
directly and want the keys where the CoCo had them: turn off *Typed
Characters* in the menu, and the PC keys then sit in the CoCo's places
(Shift+2 is `"`, the key right of 0 is `:`).

## Joysticks

The CoCo's joysticks are analog. BMCoco has two, Joystick 1 and Joystick 2,
plugged into the CoCo's right and left ports (most games use the right
one). Each takes its input from:

- a USB gamepad (by default gamepad 1 for Joystick 1, gamepad 2 for
  Joystick 2; change it in the menu). Its left analog stick gives the
  position as it is, the whole 0-63 range the CoCo reads; the D-pad pushes
  to the edges.
- a joystick wired to the GPIO header (see below), which also pushes to the
  edges.

| Gamepad button  | CoCo                       |
|-----------------|----------------------------|
| A               | Fire                       |
| B               | Second fire button (CoCo 3)|
| X               | Space                      |
| Y               | Enter                      |
| Home, Select+Start | BMCoco menu             |

In the menu's *Joysticks* page each port can also be given **Arrow keys**:
the arrow keys then move that joystick and left Alt fires (the arrows then
no longer reach the CoCo as keys). *Joystick Test* shows what the CoCo
reads.

**Joysticks on the GPIO header**: set `gpio_joysticks=1` in `cmdline.txt`
(or in the menu, then reboot). The wiring is BMC64's "GPIO Config 1". The
pins are active low (each switch connects the pin to ground) and BMCoco
turns on the Pi's pull-ups:

| Signal | Joystick 1 (GPIO) | Joystick 2 (GPIO) |
|--------|-------------------|-------------------|
| Up     | 17                | 5                 |
| Down   | 18                | 6                 |
| Left   | 27                | 12                |
| Right  | 22                | 13                |
| Fire   | 23                | 19                |

Ground is on pins 6, 9, 14, 20, 25, 30, 34 or 39 of the header. These are
digital (switch) joysticks, like an Atari-style stick with an adapter;
the CoCo's own analog joysticks need an analog to digital converter, which
the Pi doesn't have.

## Display and timing

BMCoco emulates an NTSC CoCo (60 Hz), and `config.txt` sets the display to
720p at 60 Hz to match. When the display's refresh rate is within 2% of the
machine's, each CoCo frame is shown on exactly one display refresh, which
gives perfectly smooth scrolling (the CoCo then runs 0.1% fast, which
nothing notices). Otherwise the emulation keeps exact time and frames are
shown as they come. The About page shows which is in use.

For a PAL CoCo or a Dragon (50 Hz), choose it in *Machine* and set
`hdmi_mode=19` (720p 50 Hz) in `config.txt`. `vsync=on` or `vsync=off` in
`cmdline.txt` (or *Frame Lock* in the menu) forces one or the other.

**Artifact colours**: many CoCo 1/2 games draw in PMODE 4 and rely on the
way an NTSC TV smears its black and white dots into red and blue. *TV
Input: Composite* shows them; on a real CoCo which of the two colours
appeared depended on luck at power on, so if a game's colours are swapped
choose the other phase (blue-red or red-blue). *S-Video* shows the dots in
black and white. The *Artifact Colours* renderer chooses between a quick
table (*Simple*, *5-bit*) and XRoar's simulation of an NTSC TV
(*Partial*, *Simulated*: the most accurate, but it uses more of the Pi).

## Networking and the web UI

BMCoco can join your network over Ethernet or Wi-Fi and serve a web page
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
Network* instead. Or, in `cmdline.txt` (on the same single line as the other options):

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
  throttling warnings and free space on the card. Buttons to **Hard Reset**
  the CoCo (power it off and on), **Reboot** BMCoco, **Shut Down** (the
  Pi halts; switch it off, then on again to restart), and **Disable Web UI**
  until the next reboot.
- **Files**: browse the SD card; upload (or drag and drop) files into a
  folder, download, rename, delete, and create folders. Clicking a disk
  image, program, cartridge or tape **starts it** on the CoCo, the same as
  *Run a Program*. `cmdline.txt`, `config.txt`, `bmcoco.cfg`,
  `xroar.conf` and `wpa_supplicant.conf` can be **edited** in the browser; changes take
  effect after a reboot, and the previous version is kept as `.bak`.

Uploads can't overwrite `cmdline.txt`, `config.txt` or `wpa_supplicant.conf`
(edit them instead), or anything in `firmware`. Everything else, including
the kernels, can be replaced, so a new build of BMCoco can be
uploaded and started with **Reboot**. A mistake in `cmdline.txt` or
`config.txt` can stop the Pi starting, and then needs the card in a computer
(the `.bak` copy makes that easy).

The web UI is open to anyone on your network. To protect it with a PIN, set
`webui_pin=1234` in `cmdline.txt`; the browser then asks for a user name
(anything) and the PIN. To network without the web UI, set `webui=0`.

The network also sets the Pi's clock, so files saved on the card get the
right date and time. Set `timezone` to your offset from UTC in minutes, for
example `timezone=600` for UTC+10.

## Options

`cmdline.txt` is a single line of `name=value` options separated by spaces.
The menu saves the options it changes to `bmcoco.cfg`, one `name=value` per
line, and those win over `cmdline.txt`. Either file can be edited from the
web UI.

| Option              | Values                       | Default  | What it does |
|---------------------|------------------------------|----------|--------------|
| `machine`           | `coco2bus`, `cocous`, `coco3`, `coco3h`, `coco2b`, `coco`, `coco3p`, `mc10`, `dragon64`, ... | first with ROMs | Machine to emulate |
| `autostart`         | path, e.g. `/disks/game.dsk` | none     | Disk, tape, cartridge or program to start at boot |
| `tv_input`          | `cmp-br`, `cmp-rb`, `cmp`, `rgb` | machine's | Composite (two phases), S-Video, RGB (CoCo 3) |
| `artifacts`         | `none`, `simple`, `5bit`, `partial`, `simulated` | XRoar's | Artifact colour renderer |
| `picture`           | `zoomed`, `title`, `action`, `underscan` | `title` | How much border to show |
| `aspect`            | number                       | `1.0`    | Multiplies the width of the picture |
| `scale`             | 0.1 to 1.0                   | `1.0`    | Height of the picture as a fraction of the screen |
| `smooth`            | `0`, `1`                     | `0`      | Bilinear filtering instead of sharp pixels |
| `crt_shader`        | `0`, `1`                     | `0`      | CRT look (scanlines) from BMC64 |
| `crt_curvature`     | `0`, `1`                     | `0`      | With `crt_shader`, a curved screen |
| `vsync`             | `auto`, `on`, `off`          | `auto`   | Lock frames to the display (see Display and timing) |
| `audio_out`         | `auto`, `hdmi`, `headphones` | `auto`   | Where the sound goes |
| `volume`            | 0 to 100                     | `70`     | Sound volume |
| `audio_buffer_ms`   | 20 to 500                    | `60`     | Sound buffer; raise it if the sound breaks up |
| `keyboard`          | `us`, `gb`, `de`, `fr`, ... (XRoar's `-kbd-lang`) | `us` | PC keyboard layout |
| `typed_keys`        | `0`, `1`                     | `1`      | Keys type the characters printed on them (see Keyboard) |
| `joy_right`, `joy_left` | `joy0`, `joy1`, `kjoy0` (arrow keys) | `joy0`, `joy1` | What is plugged into each joystick port |
| `joy1_pad`, `joy2_pad` | `0` automatic, `1`-`4` a gamepad, `-1` none | `0` | Gamepad for Joystick 1 and 2 |
| `gpio_joysticks`    | `0`, `1`                     | `0`      | Joysticks on the GPIO header |
| `enable_logging`    | `off`, `uart`, `file`        | `uart`   | Where the log goes (see Logging) |
| `network`           | `off`, `ethernet`, `wifi`    | `off`    | Networking (see Networking and the web UI) |
| `webui`             | `0`, `1`                     | `1`      | Web UI when networking is on |
| `webui_pin`         | digits                       | none     | PIN for the web UI |
| `timezone`          | minutes east of UTC          | `0`      | Time zone for the network clock |
| `usbpowerdelay`     | milliseconds                 | `1000`   | Circle: wait for USB devices to power up |

Anything else XRoar can do can go in `xroar.conf` in the root of the card,
one option per line without the leading `-` (see XRoar's manual), for
example `ram 64` or `machine-cpu 6309`.

## Logging

BMCoco logs what it is doing, which helps when something doesn't work:
the version, the display and sound set up, gamepads coming and going,
network status, and XRoar's own messages (which ROMs it found, disks
inserted). Choose where it goes with `enable_logging` in `cmdline.txt`:

- **UART** (the default): the serial port, GPIO14 TX and GPIO15 RX at
  115200 baud. Needs a USB serial adapter.
- **File**: `bmcoco.log` in the root of the SD card. It starts with what
  was logged since boot, is written to the card every few seconds, and
  starts again at boot once it is over 1 MB. Download it from the web UI's
  file manager to send with a bug report. Writing it costs a little time
  on each frame, so switch it off once you have what you need.
- **Off**: nothing is logged.

## Troubleshooting

**"ROMs not found"**: see ROMs. The names must match (`bas13.rom`, not
`BASIC13.ROM`, although case doesn't matter on the card).

**No picture**: uncomment `hdmi_safe=1` in `config.txt`. If that works, your
display doesn't accept 720p at 60 Hz; try another `hdmi_mode`.

**No sound over HDMI**: try `hdmi_drive=2` in `config.txt`, or
`audio_out=hdmi` in `cmdline.txt`.

**Sound breaks up**: raise `audio_buffer_ms` in `cmdline.txt`, e.g. to 100.

**Keyboard or gamepad not working**: unplug it and plug it in again; devices
can come and go while the emulator runs. Some keyboards need a longer
`usbpowerdelay`.

**A game's colours are wrong**: choose the other composite phase in *Video*,
*TV Input*.

**A game doesn't respond to keys**: turn off *Typed Characters* in
*Keyboard*.

**A disk won't load**: Disk BASIC needs `disk11.rom` in `roms`. `DIR` lists
the disk; `RUN"NAME"` starts a BASIC program and `LOADM"NAME":EXEC` a
machine code one.
