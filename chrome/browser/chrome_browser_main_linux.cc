// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_linux.h"

#include <fontconfig/fontconfig.h>

#include <string>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/chromium_strings.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/metrics/metrics_service.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/dbus_thread_manager_linux.h"
#include "media/audio/audio_manager.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "base/linux_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/os_crypt/os_crypt.h"
#include "content/public/browser/browser_thread.h"
#endif

ChromeBrowserMainPartsLinux::ChromeBrowserMainPartsLinux(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsPosix(parameters) {
}

ChromeBrowserMainPartsLinux::~ChromeBrowserMainPartsLinux() {
}

void ChromeBrowserMainPartsLinux::ToolkitInitialized() {
  // Explicitly initialize Fontconfig early on to prevent races later due to
  // implicit initialization in response to threads' first calls to Fontconfig:
  // http://crbug.com/404311
  FcInit();

  ChromeBrowserMainPartsPosix::ToolkitInitialized();
}

void ChromeBrowserMainPartsLinux::PreProfileInit() {
#if !defined(OS_CHROMEOS)
  // Needs to be called after we have chrome::DIR_USER_DATA and
  // g_browser_process.  This happens in PreCreateThreads.
  // base::GetLinuxDistro() will initialize its value if needed.
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::GetLinuxDistro)));
#endif

  media::AudioManager::SetGlobalAppName(
      l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME));

#if !defined(OS_CHROMEOS)
  // Forward to os_crypt the flag to use a specific password store.
  OSCrypt::SetStore(
      parsed_command_line().GetSwitchValueASCII(switches::kPasswordStore));
  // Forward the product name
  OSCrypt::SetProductName(l10n_util::GetStringUTF8(IDS_PRODUCT_NAME));
#endif

  ChromeBrowserMainPartsPosix::PreProfileInit();
}

void ChromeBrowserMainPartsLinux::PostProfileInit() {
  ChromeBrowserMainPartsPosix::PostProfileInit();

  g_browser_process->metrics_service()->RecordBreakpadRegistration(
      breakpad::IsCrashReporterEnabled());
}

void ChromeBrowserMainPartsLinux::PostMainMessageLoopStart() {
#if !defined(OS_CHROMEOS)
  bluez::DBusThreadManagerLinux::Initialize();
  bluez::BluezDBusManager::Initialize(
      bluez::DBusThreadManagerLinux::Get()->GetSystemBus(), false);
#endif

  ChromeBrowserMainPartsPosix::PostMainMessageLoopStart();
}

void ChromeBrowserMainPartsLinux::PostDestroyThreads() {
#if !defined(OS_CHROMEOS)
  bluez::BluezDBusManager::Shutdown();
  bluez::DBusThreadManagerLinux::Shutdown();
#endif

  ChromeBrowserMainPartsPosix::PostDestroyThreads();
}
