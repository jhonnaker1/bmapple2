# Updating KEGS and Circle

## KEGS

KEGS is a git submodule in `third_party/kegs`, from
`https://github.com/david-schmidt/KEGS-upstream.git`, which keeps each
release from [KEGS's SourceForge page](https://kegs.sourceforge.net/) on a
branch of its own (`v1.38`). It is used unmodified; everything specific to
BMApple2 is in `src/port/`:

| File | What it has to do with KEGS |
|---|---|
| `src/port/kegs.mk` | The list of KEGS source files to compile (from KEGS's `src/ldvars`) |
| `src/port/platform.c` | KEGS's front end, in place of `xdriver.c` / `windriver.c` / the Mac's: `kegs_init()`, `run_16ms()`, `video_out_data()`, the key, mouse and colour mask calls, `get_dtime()` and `micro_sleep()` |
| `src/port/snddrv.c` | In place of `sound_driver.c`: `snddrv_init()`, `snddrv_send_sound()` |
| `src/port/joystick.c` | In place of `joystick_driver.c`: `joystick_init()`, `joystick_update()`, `joystick_update_buttons()` |
| `src/port/scc_stub.c` | In place of `scc_unixdriver.c` on the Pi |
| `src/port/scc_socket_driver.c` | KEGS's `scc_socket_driver.c` (the virtual modem), changed: Circle's sockets, dialling in the background. When KEGS is updated, take its new version and make the changes (marked `BMAPPLE2`) again |
| `src/port/keymap.c` | USB keys to the ADB codes KEGS takes |
| `src/port/osd.c` | Draws the menu with KEGS's font (`g_font_array`) |
| `src/port/compat/` | Empty `sys/ioctl.h`, `sys/ipc.h`, `sys/shm.h` for newlib |

To move to a new KEGS release:

1. Fetch the new release's branch and check it out:

   ```
   cd third_party/kegs
   git fetch origin
   git checkout v1.39
   cd ../..
   ```

2. Compare the new `src/ldvars` (OBJECTS) with `src/port/kegs.mk`, and
   add or remove files. Compare the front end functions KEGS's own drivers
   call (`xdriver.c` is the simplest) with `src/port/platform.c`: new calls
   at start up, renamed functions, new settings.
3. Build and test on the PC first: `tools/hostsim/run_tests.sh`, with
   `BMAPPLE2_ROMS` set to a folder with a IIgs ROM, and boot a GS/OS disk
   in the live simulator (`tools/hostsim/live.sh`). Then build the kernels
   (`./build_sdcard.sh`) and try them on a Pi.
4. Update the version in `docs/THIRD_PARTY.md` and the branch in
   `.gitmodules`, and commit:

   ```
   git add third_party/kegs src/port .gitmodules docs/THIRD_PARTY.md
   git commit -m "Update KEGS to 1.39"
   ```

## circle-stdlib (Circle and newlib)

circle-stdlib is a submodule in `third_party/circle-stdlib`, pinned to the
same revision BMC64 uses. `make_all.sh` applies the patches in `src/patches/`
to Circle before building:

- `circle_sysconfig_patch.diff` enables multi-core and the USB FIQ
- `circle_linux_bug_patch.diff` turns off `BUG_ON`, whose checks in Circle's
  Linux emulation (used by VCHIQ for display and sound) only allow core 0;
  the emulator calls them from core 1, as BMC64 does
- `circle_bolt_keyboard_patch.diff` adds Logitech Bolt keyboard support
  (from BMC64)

To update, move the submodule to a new circle-stdlib release, run
`git submodule update --init --recursive`, then `./build_sdcard.sh`. If a
patch no longer applies, update it from the matching BMC64 patch or by
hand. `src/fbl.cpp` and `src/vchiqsoundbasedevice.cpp` use Circle's VC4
addon and may need the same changes BMC64 makes when it updates Circle.
