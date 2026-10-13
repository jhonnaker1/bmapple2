//
// hdmisound.h
//
// HDMI sound on either of the Pi 4's HDMI ports. Circle's
// CHDMISoundBaseDevice drives HDMI0 only ("This driver does not support
// HDMI1 on Raspberry Pi 4"); this is that driver, adapted to take the
// port, for the Pi 4 only and with DMA only.
//
// Derived from Circle, lib/sound/hdmisoundbasedevice.cpp and
// include/circle/sound/hdmisoundbasedevice.h,
// Copyright (C) 2021-2024 R. Stange <rsta2@o2online.de>, GPL 3 or later:
// the register set-up, the MAI, the audio InfoFrame, N and CTS, the DMA.
// Changed here: the registers are those of the port asked for; the MAI
// sample rate ratio is worked out by this file's own continued fraction
// code; HDMI1's 108 MHz audio clock is ungated.
//
// HDMI1's addresses:
//  - BCM2711 device tree (arch/arm/boot/dts/broadcom/bcm2711.dtsi, node
//    hdmi1): core at 0x7EF05700, PHY at 0x7EF05F00, packet RAM at
//    0x7EF06B00, the shared "hd" block at 0x7EF20000; DMA request 17
//    (HDMI0's is 10).
//  - Linux, drivers/gpu/drm/vc4/vc4_hdmi_regs.h (vc5_hdmi_hdmi1_fields):
//    HDMI1's MAI registers at 0x30 to 0x40 in "hd"
//    (HDMI0's at 0x10 to 0x20); its core registers at the same offsets as
//    HDMI0's.
//  - Linux, drivers/clk/bcm/clk-bcm2711-dvp.c: bit 4 of the DVP's
//    MISC_CONFIG (0x7EF00008) gates HDMI1's 108 MHz audio clock (set =
//    off).
//
// This file is part of BMCoco, a bare metal Tandy Color Computer emulator for
// the Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMCOCO_PI4_HDMISOUND_H_
#define BMCOCO_PI4_HDMISOUND_H_

#include <circle/dmachannel.h>
#include <circle/interrupt.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/spinlock.h>
#include <circle/types.h>

class Pi4HdmiSound : public CSoundBaseDevice {
public:
  // PORT 0 or 1. CHUNK_SIZE as for CHDMISoundBaseDevice: twice the frames
  // per DMA buffer, a multiple of 384.
  Pi4HdmiSound(CInterruptSystem *pInterrupt, unsigned port,
               unsigned nSampleRate = 48000, unsigned nChunkSize = 384 * 10);
  ~Pi4HdmiSound(void);

  boolean Start(void) override;
  void Cancel(void) override;
  boolean IsActive(void) const override;

  // The port a display is plugged into (HDMI0 first), or -1.
  static int ConnectedPort(void);

private:
  enum State { Created, Idle, Running, Cancelled, Error };

  void RunHdmi(void);
  void StopHdmi(void);
  void ResetHdmi(void);
  boolean SetAudioInfoFrame(void);
  void DmaCompletion(unsigned nBuffer, boolean bStatus);
  static void DmaCompletionStub(unsigned nChannel, unsigned nBuffer,
                                boolean bStatus, void *pParam);
  static boolean WaitForBit(uintptr nAddress, u32 nMask, boolean bSet,
                            unsigned nMsTimeout);

  unsigned m_nPort;
  unsigned m_nSampleRate;
  unsigned m_nChunkSize;
  unsigned long m_ulPixelClockRate;

  // The port's registers.
  uintptr m_Core, m_Phy, m_Ram, m_Mai;

  volatile State m_State;
  CDMAChannel *m_pDMAChannel;
  u32 *m_pDMABuffer[2];
  CSpinLock m_SpinLock;
};

#endif
