//
// logging.h
//
// Where the log (Circle's logger and stdio, so KEGS's messages too)
// goes: the serial port (UART), /bmapple2.log on the SD card, or nowhere.
// Set with enable_logging=off|uart|file, as in BMC64, or in the menu.
// Based on BMC64's CLoggingDevice (third_party/common/menu_logging_device.cpp).
//
// This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
// the Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMAPPLE2_LOGGING_H_
#define BMAPPLE2_LOGGING_H_

#include <circle/device.h>
#include <circle/spinlock.h>

#include "host.h"
#include "sdcard/sd_fs.h"

#define LOG_FILE "/bmapple2.log"

class CLogDevice : public CDevice {
public:
  CLogDevice(CDevice *pSerial);

  // HOST_LOG_OFF, HOST_LOG_UART or HOST_LOG_FILE. Opening the file uses
  // FatFs, so this runs at start up or on core 1. Returns false (and logging
  // is off) if the file cannot be opened.
  bool SetDestination(int destination);
  int GetDestination(void) const { return m_Destination; }

  // Any core, also from interrupts. For the file the text is queued.
  int Write(const void *pBuffer, size_t nCount) override;

  // Writes queued text to the file. Core 1 only (FatFs).
  void Drain(void);
  // Writes everything queued and closes the file, before a reboot.
  void Close(void);

private:
  enum { QUEUE_SIZE = 32768, WRITE_SIZE = 1024 };

  CDevice *m_pSerial;
  volatile int m_Destination;
  sd_file *m_pFile;
  CSpinLock m_Lock;
  char m_Queue[QUEUE_SIZE];
  char m_WriteBuffer[WRITE_SIZE];
  unsigned m_QueueRead;
  unsigned m_QueueWrite;
  volatile unsigned m_QueueLength;
  unsigned m_Dropped;
  unsigned m_LastSync;
};

#endif
