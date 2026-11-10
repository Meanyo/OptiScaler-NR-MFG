#pragma once

#include <SysUtils.h>

#include <string>

// Multi Frame Generation on Ada.
//
// nvngx_dlssg.dll gates MFG on the architecture id reported by the driver: 0x1b0 is Blackwell, Ada
// is below it. Two sites decide what a card is allowed to do, and both compare against that constant.
//
//   Advertise, the function that publishes DLSSG.MultiFrameCountMax:
//       mov   ebx, 0x1
//       mov   r8d, 0x3            the Blackwell count
//       cmp   edi, 0x1b0
//       cmovl r8d, ebx            below Blackwell the count becomes 1
//
//   Validate, the function that accepts or rejects a requested count:
//       cmp   eax, 0x1b0
//       jl    ada                 Ada takes this branch and accepts only 1
//       cmp   ebx, 0x3
//       jbe   accept
//
// Patched: the count immediates become 5, the cmovl becomes a nop, and the jl becomes two nops. The
// result is a maximum of five generated frames -- 6X -- on any architecture.
//
// Memory only. The file on disk carries an Authenticode signature and is left alone.
//
// A Streamline wrapper between the game and the snippet can carry a lower ceiling of its own. That
// one is raised where the count crosses slDLSSGGetState. Advertise and validate have no such
// boundary: nothing stands between sl.dlss_g.dll and nvngx_dlssg.dll to intercept.
//
// Ada's provider may place every generated frame at the same temporal midpoint. R10 uses RenoDX
// 1.0's exact-profile PTX rebuild and descriptor redirection: unknown providers fail closed rather
// than relabelling an entire Blackwell fatbin for Ada.
namespace MfgUnlock
{
// What the last attempt found. The signatures are version specific by construction -- they carry the
// shape of the code they patch -- so a module this does not recognise is the expected outcome on a
// version nobody has looked at yet, not a fault. The menu reports this so a report comes back with a
// version number attached rather than "it does not work".
struct Status
{
    bool ModuleFound = false; // nvngx_dlssg.dll was loaded
    bool AdvertiseMatched = false;
    bool ValidateMatched = false;
    unsigned int KernelsRewritten = 0; // RenoDX temporal descriptor redirects
    std::string SnippetVersion; // file version of nvngx_dlssg.dll, empty if it could not be read
    unsigned int CopiesSeen = 0;     // distinct module handles processed
    unsigned int CopiesComplete = 0; // ...of which fully patched (both gates)
    std::string UnmatchedVersion;    // version of the first copy whose signatures missed, for reports
};

// Coherent snapshot of the aggregate state (by value, safe to read from any thread).
// AdvertiseMatched/ValidateMatched are joint: both true only when every seen copy is complete.
Status LastStatus();

// True once any nvngx_dlssg.dll copy has been processed (lock-free, for hot paths).
bool AnyModuleSeen();

// Applies the patches to every loaded nvngx_dlssg.dll copy, once per module. Silent and harmless
// when the config option is off, when nvngx_dlssg.dll is not loaded, or when a signature does not
// match exactly once. The parameterless form patches whatever GetModuleHandleW finds; prefer the
// handle form at load sites so a second copy (e.g. DriverStore) is not missed after the first.
void TryApply();
void TryApply(HMODULE module);

// How many distinct module handles have completed patching (0 when none seen yet).
size_t PatchedModuleCount();

// The generated frame ceiling the patches opened, or 0 when they did not land.
unsigned int UnlockedMax();

} // namespace MfgUnlock
