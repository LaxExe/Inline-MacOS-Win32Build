#pragma once

#include "core/buffers/buffers.h"

bool appendCharIfAllowed(CurrentWordBuffer& buf, wchar_t ch, bool secureFieldActive);
