// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DriverCore.h"
#define CPU_READ(cpu,addr,time) static_cast<mm5studio::DriverCore*>(cpu)->read(addr)
#define CPU_WRITE(cpu,addr,data,time) static_cast<mm5studio::DriverCore*>(cpu)->write(addr,data,time)
