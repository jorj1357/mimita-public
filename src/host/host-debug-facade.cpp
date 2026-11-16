// Resident Debug facade for host sources.
//
// Game sources link their own debug-log.cpp. Resident sources cannot depend on
// that game-owned symbol, so the host compiles the same facade against the
// resident StructuredLogger sink.

// The migration EXE also links the game-owned debug-log.cpp. Mark this host
// copy weak before the definitions so the game-owned definitions win there,
// while the strict cold EXE keeps the host copy.
#pragma weak _ZN5Debug12logThrottledENS_8CategoryEPKcfS2_z
#pragma weak _ZN5Debug13startupReportEv
#pragma weak _ZN5Debug3logENS_8CategoryEPKcz
#pragma weak _ZN5Debug4warnENS_8CategoryEPKcz
#pragma weak _ZN5Debug5errorENS_8CategoryEPKcz
#pragma weak _ZN5Debug7enabledENS_8CategoryE
#pragma weak _ZN5Debug7logAutoENS_8CategoryEPKcz
#pragma weak _ZN5Debug7logOnceENS_8CategoryEPKcS2_z

#include "../debug/debug-log.cpp"
