# Third-Party Software

BMCoco incorporates software and other materials from third-party projects. Third-party components remain subject to their respective copyright and license terms.

The original copyright, license, and attribution notices supplied with these components are retained with their source where applicable.

## XRoar

BMCoco incorporates [XRoar](https://www.6809.org.uk/xroar/), the Dragon and Tandy Colour Computer emulator by Ciaran Anscomb, version 1.12.1, under the GNU General Public License version 3. Its portalib library is under the GNU Lesser General Public License version 3.

See `third_party/xroar/` (from `https://www.6809.org.uk/git/xroar.git`, tag `1.12.1`) for the source and its accompanying copyright and license information. The CoCo 3 character set BMCoco's menu is drawn with comes from XRoar (`src/tcc1014/font-gime.c`).

## BMA800XL

BMCoco is a fork of [BMA800XL](https://github.com/aminch/bma800xl), the bare metal Atari 800XL emulator for the Raspberry Pi, under the GNU General Public License version 3. Its kernel, display, sound, network and web UI code is used here.

## circle-stdlib

BMCoco uses the [circle-stdlib project](https://github.com/smuehlst/circle-stdlib), v21, which includes [Circle](https://github.com/rsta2/circle) by Rene Stange and the newlib C library.

See [`third_party/circle-stdlib/`](https://github.com/smuehlst/circle-stdlib/tree/v21) for the version used by BMCoco and its accompanying copyright and license information.

## BMC64

BMCoco incorporates source code from the [BMC64 project](https://github.com/randyrossi/bmc64) by Randy Rossi and contributors.

See [BMC64](https://github.com/randyrossi/bmc64) for its source and its accompanying copyright and license information.

## Raspberry Pi Firmware

BMCoco includes the Raspberry Pi boot firmware (`bootcode.bin`, `start.elf`, `fixup.dat`) from the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware).

See the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware/tree/master/boot) for its accompanying copyright and license information.

## Wi-Fi Firmware

BMCoco includes the Wi-Fi firmware for the Raspberry Pi's wireless chip from the [RPi-Distro firmware-nonfree repository](https://github.com/RPi-Distro/firmware-nonfree), downloaded when BMCoco is built.

See the `firmware/` folder on the SD card for the Broadcom, Cypress and Synaptics licenses that accompany it.

## Other Third-Party Components

Additional third-party components and supporting materials may be present in the BMCoco source tree. Where such material is accompanied by its own copyright, license, or attribution notices, those notices and terms continue to apply.
