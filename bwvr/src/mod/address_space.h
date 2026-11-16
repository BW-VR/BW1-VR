// The address space of the process in the log (address_space.cpp,
// docs/backend.md, "The address space").
#pragma once

#include "address_space_math.h"

// A walk of VirtualQuery over the address space of the process. It makes no
// allocation, so the crash report can call it. total: the size of the
// address space: 2 GB, or 4 GB when runblack.exe has the flag
// LARGE_ADDRESS_AWARE (large_address.h). False when the walk cannot run.
bool ReadAddressSpace(address_space_math::Totals* out, uint64_t* total);

// For the report of each 450 frames: one line with the address space in use,
// the free part and its largest block. The walk runs on a thread of the
// pool, so the frame thread does not wait for it. The first walk with less
// than 128 MB in the largest free block adds a line that says so.
void AddressSpaceReport();

// A test key (test_high_memory=1): reserves each free block below 2 GB, so
// that each allocation after it gets an address above 2 GB. It shows if the
// game, the mod or a driver keeps an address in a signed number. Only with
// the flag LARGE_ADDRESS_AWARE. The blocks stay reserved to the end.
void ReserveLowAddressSpace();
