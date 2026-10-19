# Testing and debugging BMApple2 on a PC

BMApple2 isn't tested in QEMU. On the Pi it runs bare metal, on the Circle
kernel with no Linux underneath, and QEMU's Raspberry Pi machines don't
emulate enough of the Pi (its VideoCore display, HDMI sound, USB and Wi-Fi
as BMApple2 uses them) to run it. There would be nothing to strace either:
strace follows a Linux program's system calls, and BMApple2 makes none.

What BMApple2 has instead is a **PC simulator** (`tools/hostsim`, from
BMCoco): the same emulator code that runs on the Pi (KEGS and BMApple2's
port of it, `src/port/`), built as an ordinary program for Linux or macOS, with a
simulated display, sound, keyboard, joysticks and SD card in place of the
Pi's. Because it's an ordinary program, **gdb, lldb, strace and the rest
work on it** as on any other program.

## What it can and can't show

The simulator runs the Apple II or IIgs emulation, the menu, the disks
(5.25", 3.5" and hard disks), the web UI's requests, and BMApple2's
pacing and sound logic. A problem in any of those usually shows up in it
too.

It does **not** run the Pi side: the Circle kernel, the Pi's display and
sound drivers, USB, Wi-Fi and Circle's network stack, and the SD card
driver. And a PC is much faster than a Pi, so a Pi that can't keep up
won't show in it.

So the simulator is the first place to look, and if a problem doesn't show
up there, the Pi's log (at the end of this page) is the next.

## Setting it up

You need a C compiler, `make`, `git` and Python 3. No Raspberry Pi
toolchain is needed for the simulator.

**Linux** (Debian, Ubuntu, Raspberry Pi OS):

```
sudo apt install build-essential git python3 gdb strace
```

**macOS**: install Xcode's command line tools (`xcode-select --install`),
which include `clang`, `make`, `git` and the `lldb` debugger.

Get the source, with KEGS (a git submodule):

```
git clone https://github.com/jhonnaker1/bmapple2
cd bmapple2
git submodule update --init third_party/kegs
```

(`git clone --recursive` works too, but also fetches circle-stdlib, which
only the Pi build needs.)

Build and run the tests:

```
tools/hostsim/run_tests.sh
```

The first build takes a minute or two. The Apple ROMs can't be
distributed, so without any the tests run a small stand-in ROM. To test
with the IIgs and the IIe, point it at a folder with your ROMs (the same
files as in the SD card's `roms` folder: `ROM.03` or `ROM.01`,
`APPLE2E.ROM`, or MAME's parts for them, see docs/ROMS.txt):

```
BMAPPLE2_ROMS=/path/to/your/roms tools/hostsim/run_tests.sh
```

Each test runs in its own folder under `tools/hostsim/out/`, which stands
in for the SD card, with its log (`log.txt`) and screenshots (`.ppm`).

## Using it live

```
tools/hostsim/live.sh /path/to/your/roms
```

then open <http://127.0.0.1:8064/> in a browser. The Apple's screen, sound,
keyboard, mouse and gamepads are in the page, and F10 opens BMApple2's menu
as on the Pi. The simulator's SD card is `tools/hostsim/out/live`: copy
disk images into its `disks` folder, and `bmapple2.cfg` or `config.kegs`
into it if you want the same settings as your Pi's card. Options from
`cmdline.txt` go in `BMAPPLE2_CMDLINE`:

```
BMAPPLE2_CMDLINE="machine=iie" tools/hostsim/live.sh /path/to/your/roms
```

Its messages (the same as the Pi's log) go to the terminal. Ctrl+C stops
it.

## Debugging it

### Where is it stuck? (gdb or lldb)

Start it live as above, get it into the state where it's stuck, and attach
a debugger to it from another terminal.

Linux:

```
sudo gdb -p $(pgrep -f build/bmapple2_sim)
(gdb) thread apply all bt
```

macOS:

```
lldb -p $(pgrep -f build/bmapple2_sim)
(lldb) thread backtrace all
```

The backtrace shows where each thread is, in BMApple2's and KEGS's source
(the simulator is built with debugging information). When the emulation is
running normally, the main thread is somewhere under `run_16ms()` (the
Apple's CPU) or `host_video_present()` (waiting for the next frame). Type
`continue` to let it go on, or `detach` and `quit` to leave it running.

To run it under the debugger from the start instead, run it from its SD
card folder:

```
cd tools/hostsim/out/live
BMAPPLE2_LIVE=8064 BMAPPLE2_CMDLINE="root=." gdb ../../build/bmapple2_sim
(gdb) run < /dev/null
```

and press Ctrl+C in gdb when it's stuck.

### What is it doing? (strace, Linux)

```
strace -f -tt -e trace=file,network -p $(pgrep -f build/bmapple2_sim)
```

shows every file it opens (disk images, ROMs, settings) and the web
page's network traffic, with the time.

### Scripts

The simulator can also run a script of key presses, typing and screenshots
on its standard input, with time measured in frames, so that a problem can
be repeated exactly. For example, with your ROMs in `roms`:

```
cd tools/hostsim/out/live
printf '310 type PRINT "HELLO"\\n\n500 shot hello.ppm\n501 quit\n' |
    BMAPPLE2_CMDLINE="root=." ../../build/bmapple2_sim
```

types `PRINT "HELLO"` and Return at frame 310 and saves the screen at frame
500. The
commands are listed at the top of `tools/hostsim/host_sim.c`, and
`tools/hostsim/run_tests.sh` has many examples.

Useful settings, as environment variables:

| Variable                     | Does                                                        |
|------------------------------|-------------------------------------------------------------|
| `BMAPPLE2_CMDLINE`           | Options as in `cmdline.txt` (`root=.` is the SD card)       |
| `BMAPPLE2_REFRESH`           | The display's refresh rate (default 60)                     |
| `BMAPPLE2_SIM_RATE`          | The sound output's rate (48000 for a Pi 4 or 5's HDMI)      |

## On the Pi itself

On the Pi there is no debugger or strace, but there is a log. Add
`enable_logging=file` to the line in `cmdline.txt`, and BMApple2 writes
`bmapple2.log` in the root of the SD card (and to the serial port: GPIO14 TX
and GPIO15 RX at 115200 baud, for a USB serial adapter, which keeps working
even when the SD card doesn't).

What the log shows:

- The Pi, the display and sound, the machine set up, and (from 0.4.0) the
  CPU clock speed.
- If the emulation stops moving for 3 seconds, `Emulator has not moved for
  3 s; last seen:` and what it was doing (emulating, showing a frame,
  running Circle's tasks such as the network, writing sound...). That line
  is the most useful one for a hang.

When something goes wrong, the most helpful report has:

1. What you did, step by step, and what happened.
2. The BMApple2 version, which Pi, and which machine (IIgs or IIe).
3. `bmapple2.log` from the card, with `enable_logging=file` (taking the card
   out right after the problem; the log is written as it goes).
4. Your `cmdline.txt`, `bmapple2.cfg` and `config.kegs`.
5. If it involves a disk or program, which one, or the file if it's free
   to share.
