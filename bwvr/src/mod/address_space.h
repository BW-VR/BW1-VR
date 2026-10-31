// The address space of the process in the log (address_space.cpp,
// docs/backend.md, "The address space").
#pragma once

#include "address_space_math.h"

// A walk of VirtualQuery over the address space of the process. It makes no
// allocation, so the crash report can call it. total: the size of the
// address space (2 GB for the game). False when the walk cannot run.
bool ReadAddressSpace(address_space_math::Totals* out, uint64_t* total);

// For the report of each 450 frames: one line with the address space in use,
// the free part and its largest block. The walk runs on a thread of the
// pool, so the frame thread does not wait for it. The first walk with less
// than 128 MB in the largest free block adds a line that says so.
void AddressSpaceReport();
