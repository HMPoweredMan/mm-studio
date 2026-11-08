// SPDX-License-Identifier: GPL-3.0-only
// Interactive test harness for the exact same export window used by the VST.
// Not part of the distributed plugin bundle.
#include "NativeExportWindow.h"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){mm5studio::showNativeExport(nullptr,instance);return 0;}
