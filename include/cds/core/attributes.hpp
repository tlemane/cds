#pragma once

#if defined(_MSC_VER)
#define CDS_NO_UNIQUE_ADDRESS
#else
#define CDS_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
