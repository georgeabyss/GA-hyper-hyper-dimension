#pragma once

// Parameter IDs, shared by the processor and the editor.
// Never change them: hosts and saved sessions refer to parameters by these strings.
namespace pid
{
    // HYPER
    inline constexpr const char* hyperOn  = "hyperon";
    inline constexpr const char* unison   = "unison";
    inline constexpr const char* retrig   = "retrig";
    inline constexpr const char* rate     = "rate";
    inline constexpr const char* detune   = "detune";
    inline constexpr const char* spread   = "spread";
    inline constexpr const char* hyperMix = "hmix";

    // DIMENSION
    inline constexpr const char* dimOn    = "dimon";
    inline constexpr const char* dimSize  = "dsize";
    inline constexpr const char* dimMix   = "dmix";

    // SPACEY (the widener; its IDs date from when it was called "wide")
    inline constexpr const char* spaceyOn      = "wide";
    inline constexpr const char* spaceyMode    = "widemode";
    inline constexpr const char* spaceySpacing = "widespace";
    inline constexpr const char* spaceyAmount  = "wideamt";

    // output and settings
    inline constexpr const char* dryWet     = "mix";
    inline constexpr const char* bassMono   = "bassmono";
    inline constexpr const char* bypass     = "bypass";
    inline constexpr const char* osFactor   = "os";
    inline constexpr const char* osFilter   = "osfilter";
    inline constexpr const char* phaseProtect = "phaseguard";   // was called Phase Guard
}

// Settings that are not parameters, saved as properties of the state tree
namespace prop
{
    inline constexpr const char* scopeOpen = "scopeOpen";
    inline constexpr const char* uiScale   = "uiScale";
    inline constexpr const char* preset    = "preset";       // name of the current preset
    inline constexpr const char* presetIsUser = "presetUser"; // ... and whether it is a user preset
    inline constexpr const char* lockDryWet   = "lockDryWet";   // locked values stay when a preset loads
    inline constexpr const char* lockBassMono = "lockBassMono";
}
