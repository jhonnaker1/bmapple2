//
// kernel.cpp
//
// This file is part of BMCoco, a bare metal Tandy Color Computer emulator for the
// Raspberry Pi. See LICENSE for licensing details.
//
// Parts of the USB, frame buffer and sound handling are based on BMC64.
//

#include "kernel.h"
#include "logging.h"
#include "network.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <circle/machineinfo.h>
#include <circle/startup.h>
#include <circle/synchronize.h>
#include <circle/usb/usbhid.h>
#include <circle_glue.h>

extern "C" {
// The emulator's entry point, in src/port/platform.c. Runs on core 1.
void bmcoco_main(void);
}

static const char FromKernel[] = "bmcoco";

// Sample rate the VideoCore plays audio at.
#define HW_SAMPLE_RATE 44100

// HID usage of the first modifier key (left control); the eight modifier
// bits map to usages 0xE0..0xE7.
#define USAGE_FIRST_MODIFIER 0xE0

// GPIO joystick wiring, the same as BMC64's "GPIO Config 1" (in BMC64's
// code that is config_0_joystickPins; its config_1 is the keyboard PCB).
// Pins are active low with pull ups. Order: up, down, left, right, fire.
static const unsigned kJoyGpio[2][5] = {
    {17, 18, 27, 22, 23}, // Joystick 1 (the CoCo's right port)
    {5, 6, 12, 13, 19},   // Joystick 2 (the CoCo's left port)
};

CKernel *CKernel::s_pThis = 0;

// Core 1 counts up s_Core1Beat while it makes progress and names what it is
// doing in s_Core1Stage, so core 0 can log where it got stuck if it stops.
static volatile unsigned s_Core1Beat = 0;
static const char *volatile s_Core1Stage = "starting";

static inline void Core1Beat(const char *stage) {
  s_Core1Stage = stage;
  s_Core1Beat = s_Core1Beat + 1;
}

//
// CEmulatorCore
//

CEmulatorCore::CEmulatorCore(CMemorySystem *pMemorySystem)
    : CMultiCoreSupport(pMemorySystem), m_bLaunch(false) {}

void CEmulatorCore::Launch(void) {
  DataSyncBarrier();
  m_bLaunch = true;
  DataSyncBarrier();
}

void CEmulatorCore::Run(unsigned nCore) {
  if (nCore == 1) {
    while (!m_bLaunch) {
      DataMemBarrier();
    }
    bmcoco_main();
  }

  // Nothing else runs here.
  for (;;) {
    asm volatile("wfi");
  }
}

//
// CKernel
//

CKernel::CKernel(void)
    : mLog(&mSerial), mTimer(&mInterrupt), mLogger(mKernelOptions.GetLogLevel(), &mTimer),
      mGPIOManager(&mInterrupt), mVCHIQ(CMemorySystem::Get(), &mInterrupt),
      mEMMC(&mInterrupt, &mTimer, &mActLED),
      mUSBHCI(&mInterrupt, &mTimer, TRUE), mConsole(&mSerial, &mLog),
      mEmulatorCore(CMemorySystem::Get()), mSound(0), mSoundChannels(0),
      mVolume(VCHIQ_SOUND_VOLUME_DEFAULT),
      mAudioOut(VCHIQSoundDestinationAuto), m_MergedModifiers(0),
      m_KeyQueueHead(0), m_KeyQueueTail(0), m_bGPIOJoysticks(false) {
  s_pThis = this;
  for (unsigned i = 0; i < MAX_KEYBOARDS; i++) {
    m_pKeyboard[i] = 0;
    m_KeyboardIndex[i] = i;
    m_KeyboardModifiers[i] = 0;
    memset(m_KeyboardKeys[i], 0, sizeof m_KeyboardKeys[i]);
  }
  memset(m_KeyDown, 0, sizeof m_KeyDown);
  for (unsigned i = 0; i < HOST_PADS; i++) {
    m_pGamePad[i] = 0;
    m_PadState[i] = -1;
    m_PadAnalog[i][0] = m_PadAnalog[i][1] = -1;
    m_PadAxisX[i] = 0;
    m_PadAxisY[i] = 1;
  }
  memset(m_pJoyPins, 0, sizeof m_pJoyPins);

  // The emulator's display is the lowest dispmanx layer.
  mFrameBuffer.SetLayer(0);
  mFrameBuffer.SetTransparency(false);
}

CKernel::~CKernel(void) {}

bool CKernel::Initialize(void) {
  if (!mSerial.Initialize(115200)) {
    return false;
  }
  if (!mInterrupt.Initialize()) {
    return false;
  }
  if (!mLogger.Initialize(&mLog)) {
    return false;
  }
  mLogger.Write(FromKernel, LogNotice, "BMCoco " BMCOCO_VERSION " (" BMCOCO_GIT
                ", XRoar " XROAR_VERSION ")");

  if (!mTimer.Initialize()) {
    return false;
  }
  if (!mGPIOManager.Initialize()) {
    return false;
  }
  if (!mVCHIQ.Initialize()) {
    return false;
  }
  if (!mEMMC.Initialize()) {
    return false;
  }
  if (f_mount(&mFileSystem, "SD:", 1) != FR_OK) {
    mLogger.Write(FromKernel, LogError, "Cannot mount the SD card");
    return false;
  }
  // Settings saved by the BMCoco menu win over cmdline.txt.
  mOptions.LoadSettings("SD:/bmcoco.cfg");

  // Until here the log went to the serial port.
  const char *logging = mOptions.Get("enable_logging", "uart");
  if (strcmp(logging, "off") == 0) {
    SetLogDestination(HOST_LOG_OFF);
  } else if (strcmp(logging, "file") == 0) {
    SetLogDestination(HOST_LOG_FILE);
  }
  if (!mConsole.Initialize()) {
    return false;
  }
  CGlueStdioInit(mConsole);

  // Before USB, as BMC64 does: the Pi 3's Ethernet is a USB device and
  // joins the network stack once USB finds it.
  NetworkInitialize(&mOptions);

  if (!mUSBHCI.Initialize()) {
    return false;
  }

  const char *audio_out = mOptions.Get("audio_out", "auto");
  if (strcmp(audio_out, "hdmi") == 0) {
    mAudioOut = VCHIQSoundDestinationHDMI;
  } else if (strcmp(audio_out, "analog") == 0 ||
             strcmp(audio_out, "headphones") == 0) {
    mAudioOut = VCHIQSoundDestinationHeadphones;
  }

  // Full scale: the volume (volume= in cmdline.txt and the menu) is
  // XRoar's, so it can change while running.
  mVolume = 0;

  FrameBufferLayer::Initialize();

  if (mOptions.GetBool("gpio_joysticks", false)) {
    SetupGPIOJoysticks();
  }

  return mEmulatorCore.Initialize();
}

TShutdownMode CKernel::Run(void) {
  SetupUSBKeyboards();
  SetupUSBGamePads();

  mEmulatorCore.Launch();

  // Core 0 only looks after USB devices coming and going, and watches that
  // the emulator on core 1 keeps going. Interrupts keep being handled here.
  unsigned last_beat = 0;
  unsigned last_change = CTimer::GetClockTicks();
  bool stalled = false;
  for (;;) {
    if (mUSBHCI.UpdatePlugAndPlay()) {
      SetupUSBKeyboards();
      SetupUSBGamePads();
    }

    unsigned beat = s_Core1Beat;
    unsigned now = CTimer::GetClockTicks();
    if (beat != last_beat) {
      if (stalled) {
        mLogger.Write(FromKernel, LogNotice, "Emulator running again after %u ms",
                      (now - last_change) / (CLOCKHZ / 1000));
        stalled = false;
      }
      last_beat = beat;
      last_change = now;
    } else if (beat != 0 && !stalled && now - last_change > 3 * CLOCKHZ) {
      mLogger.Write(FromKernel, LogWarning,
                    "Emulator has not moved for 3 s; last seen: %s",
                    (const char *)s_Core1Stage);
      stalled = true;
    }

    asm volatile("wfi");
  }

  return ShutdownHalt;
}

//
// USB keyboards
//

void CKernel::SetupUSBKeyboards(void) {
  for (unsigned i = 0; i < MAX_KEYBOARDS; i++) {
    if (m_pKeyboard[i] != 0) {
      continue;
    }
    char name[16];
    snprintf(name, sizeof name, "ukbd%u", i + 1);
    m_pKeyboard[i] =
        (CUSBKeyboardDevice *)mDeviceNameService.GetDevice(name, FALSE);
    if (m_pKeyboard[i] != 0) {
      m_pKeyboard[i]->RegisterRemovedHandler(KeyboardRemovedHandler,
                                             &m_KeyboardIndex[i]);
      m_pKeyboard[i]->RegisterKeyStatusHandlerRaw(KeyStatusHandlerRaw, FALSE,
                                                  &m_KeyboardIndex[i]);
      mLogger.Write(FromKernel, LogNotice, "Keyboard %u connected", i + 1);
    }
  }
}

void CKernel::KeyboardRemovedHandler(CDevice *pDevice, void *pContext) {
  CKernel *k = s_pThis;
  unsigned i = *(unsigned *)pContext;
  if (k == 0 || i >= MAX_KEYBOARDS) {
    return;
  }
  // Release everything that keyboard was holding down.
  static const unsigned char no_keys[6] = {0, 0, 0, 0, 0, 0};
  KeyStatusHandlerRaw(0, no_keys, pContext);
  k->m_pKeyboard[i] = 0;
}

void CKernel::PushKeyEvent(int usage, int pressed) {
  unsigned head = m_KeyQueueHead;
  unsigned next = (head + 1) % KEY_QUEUE_SIZE;
  if (next == m_KeyQueueTail) {
    return; // Full: the emulator is not reading. Drop the event.
  }
  m_KeyQueue[head] = (unsigned short)(usage | (pressed ? 0x100 : 0));
  DataMemBarrier();
  m_KeyQueueHead = next;
}

// Recomputes which keys are down across all keyboards and queues an event for
// every key whose state changed.
void CKernel::UpdateMergedKeys(void) {
  unsigned char down[256];
  unsigned char modifiers = 0;
  memset(down, 0, sizeof down);
  for (unsigned i = 0; i < MAX_KEYBOARDS; i++) {
    modifiers |= m_KeyboardModifiers[i];
    for (unsigned j = 0; j < 6; j++) {
      unsigned char key = m_KeyboardKeys[i][j];
      if (key > 3) { // 0 none, 1..3 are error codes
        down[key] = 1;
      }
    }
  }
  for (unsigned bit = 0; bit < 8; bit++) {
    down[USAGE_FIRST_MODIFIER + bit] = (modifiers >> bit) & 1;
  }

  for (unsigned usage = 4; usage < 256; usage++) {
    if (down[usage] != m_KeyDown[usage]) {
      m_KeyDown[usage] = down[usage];
      PushKeyEvent(usage, down[usage]);
    }
  }
  m_MergedModifiers = modifiers;
}

// USB interrupt context, core 0.
void CKernel::KeyStatusHandlerRaw(unsigned char ucModifiers,
                                  const unsigned char RawKeys[6], void *pArg) {
  CKernel *k = s_pThis;
  unsigned i = *(unsigned *)pArg;
  if (k == 0 || i >= MAX_KEYBOARDS) {
    return;
  }
  // Too many keys at once: the report carries no key information.
  for (unsigned j = 0; j < 6; j++) {
    if (RawKeys[j] == 0x01) {
      return;
    }
  }
  k->m_KeyboardModifiers[i] = ucModifiers;
  memcpy(k->m_KeyboardKeys[i], RawKeys, 6);
  k->UpdateMergedKeys();
}

int CKernel::KeyEvent(int *usage, int *pressed) {
  Core1Beat("reading keys");
  unsigned tail = m_KeyQueueTail;
  if (tail == m_KeyQueueHead) {
    return 0;
  }
  DataMemBarrier();
  unsigned short event = m_KeyQueue[tail];
  m_KeyQueueTail = (tail + 1) % KEY_QUEUE_SIZE;
  *usage = event & 0xff;
  *pressed = (event & 0x100) != 0;
  return 1;
}

int CKernel::KeyDown(int usage) {
  return usage >= 0 && usage < 256 ? m_KeyDown[usage] : 0;
}

unsigned CKernel::KeyModifiers(void) { return m_MergedModifiers; }

//
// USB gamepads
//

// Pads Circle doesn't know whose direction is not on axes 0 and 1, by USB
// vendor and product ID (as BMC64's usb_gamepad_defaults.c).
static const struct {
  const char *vendor;
  int axis_x;
  int axis_y;
} kPadProfiles[] = {
    {"venca3-24", 3, 4}, // 8BitDo M30 2.4G
};

void CKernel::SetupUSBGamePads(void) {
  for (unsigned i = 0; i < HOST_PADS; i++) {
    if (m_pGamePad[i] != 0) {
      continue;
    }
    char name[16];
    snprintf(name, sizeof name, "upad%u", i + 1);
    m_pGamePad[i] =
        (CUSBGamePadDevice *)mDeviceNameService.GetDevice(name, FALSE);
    if (m_pGamePad[i] != 0) {
      m_pGamePad[i]->RegisterRemovedHandler(GamePadRemovedHandler);
      m_pGamePad[i]->RegisterStatusHandler(GamePadStatusHandler);
      m_PadState[i] = 0;
      m_PadAxisX[i] = 0;
      m_PadAxisY[i] = 1;
      CString *vendor = m_pGamePad[i]->GetDevice()->GetName(DeviceNameVendor);
      for (unsigned p = 0; vendor != 0 && p < sizeof kPadProfiles /
                                                  sizeof kPadProfiles[0]; p++) {
        if (strcmp(*vendor, kPadProfiles[p].vendor) == 0) {
          m_PadAxisX[i] = kPadProfiles[p].axis_x;
          m_PadAxisY[i] = kPadProfiles[p].axis_y;
        }
      }
      const char *product = m_pGamePad[i]->GetProperty(CDevice::PropertyProduct);
      if (product == 0) {
        product = "unknown";
      }
      mLogger.Write(FromKernel, LogNotice,
                    "Gamepad %u connected: %s (%s)%s, axes %d/%d", i + 1,
                    product, vendor != 0 ? (const char *)*vendor : "?",
                    (m_pGamePad[i]->GetProperties() & GamePadPropertyIsKnown)
                        ? "" : " (generic)",
                    m_PadAxisX[i], m_PadAxisY[i]);
      delete vendor;
    }
  }
}

void CKernel::GamePadRemovedHandler(CDevice *pDevice, void *pContext) {
  CKernel *k = s_pThis;
  if (k == 0) {
    return;
  }
  for (unsigned i = 0; i < HOST_PADS; i++) {
    if (k->m_pGamePad[i] == pDevice) {
      k->m_pGamePad[i] = 0;
      k->m_PadState[i] = -1;
      k->m_PadAnalog[i][0] = k->m_PadAnalog[i][1] = -1;
    }
  }
}

// Direction of an axis: -1, 0 or 1, with a dead zone of half the range.
static int AxisDirection(const TGamePadState *pState, int axis) {
  if (axis >= pState->naxes) {
    return 0;
  }
  int min = pState->axes[axis].minimum;
  int max = pState->axes[axis].maximum;
  if (max <= min) {
    return 0;
  }
  int centre = (min + max) / 2;
  int threshold = (max - min) / 4;
  int value = pState->axes[axis].value;
  if (value < centre - threshold) {
    return -1;
  }
  if (value > centre + threshold) {
    return 1;
  }
  return 0;
}

// Position of an axis scaled to 0..65535, or -1 if there is no such axis.
static int AxisPosition(const TGamePadState *pState, int axis) {
  if (axis >= pState->naxes) {
    return -1;
  }
  int min = pState->axes[axis].minimum;
  int max = pState->axes[axis].maximum;
  if (max <= min) {
    return -1;
  }
  int value = pState->axes[axis].value;
  if (value < min) {
    value = min;
  } else if (value > max) {
    value = max;
  }
  return (int)(((long long)(value - min) * 65535) / (max - min));
}

// USB interrupt context, core 0.
void CKernel::GamePadStatusHandler(unsigned nDeviceIndex,
                                   const TGamePadState *pState) {
  CKernel *k = s_pThis;
  if (k == 0 || nDeviceIndex >= HOST_PADS || k->m_pGamePad[nDeviceIndex] == 0) {
    return;
  }

  int state = 0;
  unsigned b = pState->buttons;
  bool known = (k->m_pGamePad[nDeviceIndex]->GetProperties() &
                GamePadPropertyIsKnown) != 0;

  if (known) {
    if (b & GamePadButtonA) state |= HOST_PAD_A;
    if (b & GamePadButtonB) state |= HOST_PAD_B;
    if (b & GamePadButtonX) state |= HOST_PAD_X;
    if (b & GamePadButtonY) state |= HOST_PAD_Y;
    if (b & (GamePadButtonLB | GamePadButtonLT)) state |= HOST_PAD_L;
    if (b & (GamePadButtonRB | GamePadButtonRT)) state |= HOST_PAD_R;
    if (b & (GamePadButtonSelect | GamePadButtonMinus)) state |= HOST_PAD_SELECT;
    if (b & (GamePadButtonStart | GamePadButtonPlus)) state |= HOST_PAD_START;
    if (b & GamePadButtonGuide) state |= HOST_PAD_HOME;
    if (b & GamePadButtonUp) state |= HOST_PAD_UP;
    if (b & GamePadButtonDown) state |= HOST_PAD_DOWN;
    if (b & GamePadButtonLeft) state |= HOST_PAD_LEFT;
    if (b & GamePadButtonRight) state |= HOST_PAD_RIGHT;
  } else {
    // Unknown pads report buttons in their own order. This layout suits most
    // cheap USB pads: 0..3 face buttons, 4/5 shoulders, 8 select, 9 start.
    if (b & (1u << 0)) state |= HOST_PAD_A;
    if (b & (1u << 1)) state |= HOST_PAD_B;
    if (b & (1u << 2)) state |= HOST_PAD_X;
    if (b & (1u << 3)) state |= HOST_PAD_Y;
    if (b & ((1u << 4) | (1u << 6))) state |= HOST_PAD_L;
    if (b & ((1u << 5) | (1u << 7))) state |= HOST_PAD_R;
    if (b & (1u << 8)) state |= HOST_PAD_SELECT;
    if (b & (1u << 9)) state |= HOST_PAD_START;
    if (b & (1u << 12)) state |= HOST_PAD_HOME;
  }

  // Hats: 0 is up, counting clockwise in 45 degree steps; anything else
  // is centred.
  if (pState->nhats > 0) {
    switch (pState->hats[0]) {
    case 0: state |= HOST_PAD_UP; break;
    case 1: state |= HOST_PAD_UP | HOST_PAD_RIGHT; break;
    case 2: state |= HOST_PAD_RIGHT; break;
    case 3: state |= HOST_PAD_DOWN | HOST_PAD_RIGHT; break;
    case 4: state |= HOST_PAD_DOWN; break;
    case 5: state |= HOST_PAD_DOWN | HOST_PAD_LEFT; break;
    case 6: state |= HOST_PAD_LEFT; break;
    case 7: state |= HOST_PAD_UP | HOST_PAD_LEFT; break;
    default: break;
    }
  }

  // The left stick (axes 0 and 1 on most unknown pads), or the D-pad on
  // pads that report it as axes (kPadProfiles).
  int x = AxisDirection(pState, known ? (int)GamePadAxisLeftX
                                      : k->m_PadAxisX[nDeviceIndex]);
  int y = AxisDirection(pState, known ? (int)GamePadAxisLeftY
                                      : k->m_PadAxisY[nDeviceIndex]);
  if (x < 0) state |= HOST_PAD_LEFT;
  if (x > 0) state |= HOST_PAD_RIGHT;
  if (y < 0) state |= HOST_PAD_UP;
  if (y > 0) state |= HOST_PAD_DOWN;

  // The same stick as a position, for the CoCo's analog joysticks. Pads
  // whose D-pad is reported as axes (kPadProfiles) have no stick.
  int ax = -1;
  int ay = -1;
  if (known) {
    ax = AxisPosition(pState, GamePadAxisLeftX);
    ay = AxisPosition(pState, GamePadAxisLeftY);
  } else if (k->m_PadAxisX[nDeviceIndex] == 0) {
    ax = AxisPosition(pState, 0);
    ay = AxisPosition(pState, 1);
  }
  if (ax < 0 || ay < 0) {
    ax = ay = -1;
  }
  k->m_PadAnalog[nDeviceIndex][0] = ax;
  k->m_PadAnalog[nDeviceIndex][1] = ay;
  k->m_PadState[nDeviceIndex] = state;
}

int CKernel::PadState(int pad) {
  if (pad < 0 || pad >= HOST_PADS) {
    return -1;
  }
  return m_PadState[pad];
}

int CKernel::PadAxis(int pad, int axis) {
  if (pad < 0 || pad >= HOST_PADS || axis < 0 || axis > 1 ||
      m_PadState[pad] < 0) {
    return -1;
  }
  return m_PadAnalog[pad][axis];
}

//
// GPIO joysticks
//

void CKernel::SetupGPIOJoysticks(void) {
  for (unsigned port = 0; port < 2; port++) {
    for (unsigned i = 0; i < 5; i++) {
      m_pJoyPins[port][i] = new CGPIOPin(kJoyGpio[port][i],
                                         GPIOModeInputPullUp, &mGPIOManager);
    }
  }
  m_bGPIOJoysticks = true;
  mLogger.Write(FromKernel, LogNotice, "GPIO joysticks enabled");
}

int CKernel::GpioJoystick(int port) {
  if (!m_bGPIOJoysticks || port < 0 || port > 1) {
    return 0;
  }
  static const int bits[5] = {HOST_JOY_UP, HOST_JOY_DOWN, HOST_JOY_LEFT,
                              HOST_JOY_RIGHT, HOST_JOY_FIRE};
  int state = 0;
  for (unsigned i = 0; i < 5; i++) {
    if (m_pJoyPins[port][i]->Read() == LOW) {
      state |= bits[i];
    }
  }
  return state;
}

//
// Video
//

int CKernel::VideoInit(int width, int height, uint16_t **pixels, int *pitch) {
  if (mOptions.GetBool("crt_shader", false)) {
    mFrameBuffer.SetUsesShader(true);
  }
  if (mOptions.GetBool("smooth", false)) {
    FrameBufferLayer::SetInterpolation(1);
  }
  int ret = mFrameBuffer.Allocate(1 /* RGB565 */, (uint8_t **)pixels, width,
                                  height, pitch);
  if (ret != 0) {
    mLogger.Write(FromKernel, LogError, "Cannot allocate frame buffer");
    return ret;
  }
  if (mFrameBuffer.UsesShader()) {
    // BMC64's default CRT look: scanlines and fake gamma, optional curvature.
    mFrameBuffer.SetShaderParams(
        mOptions.GetBool("crt_curvature", false), 0.10f, 0.15f, 0 /* mask */,
        0.70f, true /* gamma */, true /* fake gamma */, true /* scanlines */,
        true /* multisample */, 6.0f, 0.12f, 1.5f, 2.4f, 2.2f,
        false /* sharper */, mOptions.GetBool("smooth", false));
  }
  return 0;
}

void CKernel::VideoSetView(int x, int y, int w, int h, double aspect) {
  bool showing = mFrameBuffer.Showing();
  if (showing) {
    mFrameBuffer.Hide();
  }
  mFrameBuffer.SetSrcRect(x, y, w, h);
  // Fill the display's height (times "scale"), with the width following
  // the aspect ratio.
  double scale = mOptions.GetDouble("scale", 1.0);
  if (scale <= 0.1 || scale > 1.0) {
    scale = 1.0;
  }
  mFrameBuffer.SetStretch(aspect, scale, 0, 0, 0, 0);
  mFrameBuffer.Show();
}

void CKernel::DisplaySize(int *width, int *height) {
  int fb_w, fb_h, src_w, src_h, dst_w, dst_h;
  mFrameBuffer.GetDimensions(width, height, &fb_w, &fb_h, &src_w, &src_h,
                             &dst_w, &dst_h);
}

void CKernel::VideoPresent(int wait_vsync) {
  // Let Circle's tasks (VCHIQ in particular) run once per frame.
  Core1Beat("running Circle tasks");
  CScheduler::Get()->Yield();
  Core1Beat("writing the log file");
  mLog.Drain();
  Core1Beat("showing a frame (dispmanx)");
  mFrameBuffer.FrameReady(wait_vsync ? 1 : 0);
  FrameBufferLayer::SwapResources(wait_vsync != 0, &mFrameBuffer, nullptr);
  Core1Beat("emulating");
}

//
// Sound
//

unsigned CKernel::SoundOpen(unsigned freq, int channels,
                            unsigned latency_frames) {
  (void)freq; // The VideoCore always runs at HW_SAMPLE_RATE.
  if (mSound == 0) {
    mSound = new VchiqSound(&mVCHIQ, HW_SAMPLE_RATE, mAudioOut);
  } else if (mSound->PlaybackActive()) {
    if (channels == mSoundChannels) {
      return HW_SAMPLE_RATE;
    }
    mSound->CancelPlayback();
  }
  mSoundChannels = channels;
  if (!mSound->Playback(mVolume, channels, latency_frames)) {
    mLogger.Write(FromKernel, LogError, "Cannot start sound");
    return 0;
  }
  return HW_SAMPLE_RATE;
}

void CKernel::SoundClose(void) {
  // The VCHIQ audio service is left open; reopening it is slow.
}

unsigned CKernel::SoundFreeFrames(void) {
  return mSound != 0 && mSound->PlaybackActive() ? mSound->FreeFrames() : 0;
}

void CKernel::SoundWrite(const int16_t *samples, unsigned frames) {
  if (mSound != 0 && mSound->PlaybackActive()) {
    Core1Beat("writing sound (VCHIQ)");
    mSound->WriteFrames(samples, frames);
    Core1Beat("emulating");
  }
}

//
// System
//

double CKernel::Time(void) {
  return (double)CTimer::GetClockTicks64() / CLOCKHZ;
}

void CKernel::Reboot(void) {
  mLogger.Write(FromKernel, LogNotice, "Rebooting");
  mLog.Close();
  reboot();
}

void CKernel::PowerOff(void) {
  mLogger.Write(FromKernel, LogNotice, "Halting; safe to switch off");
  mLog.Close();
  // Blank the picture so it is clear the Pi has stopped.
  mFrameBuffer.Hide();
  halt();
}

bool CKernel::SetLogDestination(int destination) {
  if (!mLog.SetDestination(destination)) {
    mSerial.Write("Cannot open " LOG_FILE "\n", sizeof("Cannot open " LOG_FILE "\n") - 1);
    return false;
  }
  if (destination == HOST_LOG_FILE) {
    // The file starts with what was logged since boot.
    char text[512];
    int len;
    while ((len = mLogger.Read(text, sizeof text)) > 0) {
      mLog.Write(text, len);
    }
  }
  return true;
}

//
// host.h: C entry points for the emulator.
//

extern "C" {

const char *host_option(const char *name, const char *default_value) {
  Options *o = Options::Get();
  return o ? o->Get(name, default_value) : default_value;
}

int host_option_int(const char *name, int default_value) {
  Options *o = Options::Get();
  return o ? o->GetInt(name, default_value) : default_value;
}

double host_option_double(const char *name, double default_value) {
  Options *o = Options::Get();
  return o ? o->GetDouble(name, default_value) : default_value;
}

int host_option_set(const char *name, const char *value) {
  Options *o = Options::Get();
  return o && o->Set(name, value);
}

int host_options_save(void) {
  Options *o = Options::Get();
  return o && o->SaveSettings();
}

const char *host_model_name(void) {
  return CMachineInfo::Get()->GetMachineName();
}

void host_display_size(int *width, int *height) {
  CKernel::Get()->DisplaySize(width, height);
}

void host_network_status(char *text, unsigned size) {
  static const char *const kStatus[] = {
      "Off",
      "Ethernet starting",
      "Ethernet: waiting for address",
      "Ethernet connected",
      "Ethernet not found",
      "Ethernet failed",
      "Wi-Fi: no network chosen",
      "Wi-Fi starting",
      "Wi-Fi failed",
      "Wi-Fi failed",
      "Wi-Fi starting",
      "Wi-Fi failed",
      "Wi-Fi connecting",
      "Wi-Fi connected",
      "Wi-Fi: no connection",
      "Wi-Fi not found",
      "Wi-Fi not started",
      "Wi-Fi not supported",
      "Wi-Fi: firmware missing",
  };
  char ip[32];
  int status = circle_get_network_status();
  if (circle_get_network_ip_address(ip, sizeof ip)) {
    snprintf(text, size, "%s %s",
             status == NETWORK_ETHERNET_CONNECTED ? "Ethernet" : "Wi-Fi", ip);
  } else if (status >= 0 &&
             status < (int)(sizeof kStatus / sizeof kStatus[0])) {
    snprintf(text, size, "%s", kStatus[status]);
  } else {
    snprintf(text, size, "Unknown");
  }
}

double host_time(void) { return CKernel::Get()->Time(); }

void host_sleep_us(unsigned us) {
  // Sleep in small steps so Circle's tasks keep running.
  double end = host_time() + us / 1000000.0;
  for (;;) {
    Core1Beat("sleeping");
    CScheduler::Get()->Yield();
    CKernel::Get()->DrainLog();
    double left = end - host_time();
    if (left <= 0) {
      break;
    }
    unsigned step = left > 0.001 ? 1000 : (unsigned)(left * 1000000.0);
    if (step > 0) {
      CTimer::SimpleusDelay(step);
    }
  }
}

void host_yield(void) { CScheduler::Get()->Yield(); }

int host_video_init(int width, int height, uint16_t **pixels, int *pitch) {
  return CKernel::Get()->VideoInit(width, height, pixels, pitch);
}

void host_video_set_view(int x, int y, int w, int h, double aspect) {
  CKernel::Get()->VideoSetView(x, y, w, h, aspect);
}

void host_video_present(int wait_vsync) {
  CKernel::Get()->VideoPresent(wait_vsync);
}

unsigned host_sound_open(unsigned freq, int channels, unsigned latency_frames) {
  return CKernel::Get()->SoundOpen(freq, channels, latency_frames);
}

void host_sound_close(void) { CKernel::Get()->SoundClose(); }

unsigned host_sound_free_frames(void) {
  return CKernel::Get()->SoundFreeFrames();
}

void host_sound_write(const int16_t *samples, unsigned frames) {
  CKernel::Get()->SoundWrite(samples, frames);
}

int host_key_event(int *usage, int *pressed) {
  return CKernel::Get()->KeyEvent(usage, pressed);
}

int host_key_down(int usage) { return CKernel::Get()->KeyDown(usage); }

unsigned host_key_modifiers(void) { return CKernel::Get()->KeyModifiers(); }

int host_pad_state(int pad) { return CKernel::Get()->PadState(pad); }

int host_pad_axis(int pad, int axis) { return CKernel::Get()->PadAxis(pad, axis); }

int host_gpio_joystick(int port) { return CKernel::Get()->GpioJoystick(port); }

void host_reboot(void) { CKernel::Get()->Reboot(); }

void host_power_off(void) { CKernel::Get()->PowerOff(); }

extern int circle_get_network_ip_address(char *address, unsigned int size);

int host_network_address(char *address, unsigned size) {
  return circle_get_network_ip_address(address, size);
}

void host_log(const char *message) {
  CLogger::Get()->Write(FromKernel, LogNotice, "%s", message);
}

int host_log_destination(void) { return CKernel::Get()->GetLogDestination(); }

int host_set_log_destination(int destination) {
  return CKernel::Get()->SetLogDestination(destination);
}

} // extern "C"
