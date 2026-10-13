# Third-Party Software

BMApple2 incorporates software and other materials from third-party projects. Third-party components remain subject to their respective copyright and license terms.

The original copyright, license, and attribution notices supplied with these components are retained with their source where applicable.

## KEGS

BMApple2 incorporates [KEGS](https://kegs.sourceforge.net/), the Apple IIgs emulator by Kent Dickey, version 1.38, under the GNU General Public License version 3.

See `third_party/kegs/` (from `https://github.com/david-schmidt/KEGS-upstream.git`, a copy of the releases on SourceForge, branch `v1.38`) for the source and its accompanying copyright and license information (`doc/COPYING.txt`). The Apple II character set BMApple2's menu is drawn with comes from KEGS (`src/kegsfont.h`).

## BMCoco

BMApple2 is based on [BMCoco](https://github.com/jhonnaker1/bmcoco), the bare metal Tandy Color Computer emulator for the Raspberry Pi, under the GNU General Public License version 3. Its kernel, display, sound, network, web UI, menu and test harness code is used here.

## BMA800XL

BMCoco, and so BMApple2, is a fork of [BMA800XL](https://github.com/aminch/bma800xl), the bare metal Atari 800XL emulator for the Raspberry Pi, under the GNU General Public License version 3. Its kernel, display, sound, network and web UI code is used here, including BMA800XL 1.1.0's Raspberry Pi 4 display and sound drivers.

## circle-stdlib

BMApple2 uses the [circle-stdlib project](https://github.com/smuehlst/circle-stdlib), v21, which includes [Circle](https://github.com/rsta2/circle) by Rene Stange and the newlib C library.

See [`third_party/circle-stdlib/`](https://github.com/smuehlst/circle-stdlib/tree/v21) for the version used by BMApple2 and its accompanying copyright and license information.

## BMC64

BMApple2 incorporates source code from the [BMC64 project](https://github.com/randyrossi/bmc64) by Randy Rossi and contributors.

See [BMC64](https://github.com/randyrossi/bmc64) for its source and its accompanying copyright and license information.

## Raspberry Pi Firmware

BMApple2 includes the Raspberry Pi boot firmware (`bootcode.bin`, `start.elf`, `fixup.dat`, and for the Raspberry Pi 4 `start4.elf`, `fixup4.dat` and its device trees) from the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware).

See the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware/tree/master/boot) for its accompanying copyright and license information.

## crt-pi

BMApple2 uses the crt-pi CRT shader by davej, as included in BMC64, under the [GNU General Public License version 2 or later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).

## Circle ARM stub

For the Raspberry Pi 4, BMApple2 includes `armstub8-rpi4.bin`, built from [Circle's `boot/armstub`](https://github.com/rsta2/circle/tree/master/boot/armstub) source, under Circle's [GNU General Public License version 3](https://github.com/rsta2/circle/blob/master/COPYING).

## Wi-Fi Firmware

BMApple2 includes the Wi-Fi firmware for the Raspberry Pi's wireless chip from the [RPi-Distro firmware-nonfree repository](https://github.com/RPi-Distro/firmware-nonfree), downloaded when BMApple2 is built.

See the `firmware/` folder on the SD card for the Broadcom, Cypress and Synaptics licenses that accompany it.

## Other Third-Party Components

Additional third-party components and supporting materials may be present in the BMApple2 source tree. Where such material is accompanied by its own copyright, license, or attribution notices, those notices and terms continue to apply.
