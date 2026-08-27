#pragma once

#define CDS_VERSION_MAJOR 0
#define CDS_VERSION_MINOR 1
#define CDS_VERSION_PATCH 0

#define CDS_VERSION (CDS_VERSION_MAJOR * 10000 + CDS_VERSION_MINOR * 100 + CDS_VERSION_PATCH)

#define CDS_STRINGIFY_IMPL(x) #x
#define CDS_STRINGIFY(x) CDS_STRINGIFY_IMPL(x)

#define CDS_VERSION_STRING                                                                         \
    CDS_STRINGIFY(CDS_VERSION_MAJOR)                                                               \
    "." CDS_STRINGIFY(CDS_VERSION_MINOR) "." CDS_STRINGIFY(CDS_VERSION_PATCH)
