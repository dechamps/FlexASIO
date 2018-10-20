#pragma once

struct IASIO;

// Used by FlexASIOTest to instantiate FlexASIO directly, instead of going through the ASIO Host SDK and COM.
// In production, standard COM factory mechanisms are used to instantiate FlexASIO, not these functions.
IASIO * CreateFlexASIO();
void ReleaseFlexASIO(IASIO *);
