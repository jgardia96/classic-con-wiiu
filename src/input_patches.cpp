#include "globals.h"

#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <wups.h>

// ---------------------------------------------------------------------------
// The core trick:
//
// In KPADStatus, the `classic` and `pro` members of the extension-data union
// live at the *same offsets* and use the *same bit values* for every digital
// button they share (UP/DOWN/LEFT/RIGHT/A/B/X/Y/L/R/ZL/ZR/PLUS/MINUS/HOME),
// and `leftStick`/`rightStick` are laid out identically too. That means a
// Classic Controller's raw sample is already bit-for-bit a valid Pro
// Controller sample - the only two things that differ are:
//
//   1. `extensionType`, which tells the game which member of the union to
//      read, and
//   2. the last 8 bytes of the union, which are `leftTrigger`/`rightTrigger`
//      (floats) for Classic but `charging`/`wired` (int32) for Pro.
//
// So "emulating" a Pro Controller is just relabeling the sample, not
// translating it - which is why this approach is reliable: there's no
// button-mapping table that can be wrong.
//
// The Classic Controller additionally reports some "stick emulated as
// digital button" bits in the upper 16 bits of hold/trigger/release. Those
// bit positions mean something different for the Pro Controller (real
// L3/R3-style stick-click buttons), so we mask them out to avoid phantom
// stick-click presses; real analog position is already conveyed correctly
// via leftStick/rightStick.
// ---------------------------------------------------------------------------

static inline bool IsClassicExtension(uint8_t extensionType) {
    return extensionType == WPAD_EXT_CLASSIC || extensionType == WPAD_EXT_MPLUS_CLASSIC;
}

static constexpr uint32_t kMaxWpadChannels = 7;

static void RemapClassicSamplesToPro(KPADStatus *data, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        KPADStatus &status = data[i];
        if (!IsClassicExtension(status.extensionType)) {
            continue;
        }

        status.pro.hold    &= 0x0000FFFF;
        status.pro.trigger &= 0x0000FFFF;
        status.pro.release &= 0x0000FFFF;
        status.pro.charging = 0;
        status.pro.wired    = 0;

        status.extensionType = WPAD_EXT_PRO_CONTROLLER;
        // `format` is a separate field from `extensionType` and also encodes which
        // controller is attached (WPAD_FMT_CLASSIC vs WPAD_FMT_PRO_CONTROLLER). Games
        // that gate precision-sensitive reads (like the analog sticks) on both fields
        // together would otherwise see a mismatch and silently ignore the stick data
        // even though the digital buttons (checked less strictly) still come through.
        status.format = WPAD_FMT_PRO_CONTROLLER;
    }
}

DECL_FUNCTION(uint32_t, KPADReadEx, KPADChan chan, KPADStatus *data, uint32_t count, KPADError *outError) {
    uint32_t samplesRead = real_KPADReadEx(chan, data, count, outError);
    if (gEnabled && data != nullptr && samplesRead > 0) {
        RemapClassicSamplesToPro(data, samplesRead);
    }
    return samplesRead;
}

DECL_FUNCTION(uint32_t, KPADRead, KPADChan chan, KPADStatus *data, uint32_t count) {
    uint32_t samplesRead = real_KPADRead(chan, data, count);
    if (gEnabled && data != nullptr && samplesRead > 0) {
        RemapClassicSamplesToPro(data, samplesRead);
    }
    return samplesRead;
}

// KPADGetUnifiedWpadStatus() and the raw WPADRead() below both hand back the
// *raw* hardware report instead of the calibrated KPADStatus above, and
// unlike KPADStatus, the raw Classic and Pro reports are NOT bit-compatible:
// different offsets, a 16-bit button field for Classic vs 32-bit for Pro, and
// different stick ranges (Classic is [-512, 511], Pro is [-2048, 2047] -
// exactly 4x). So this one needs an actual translation, not just a relabel.
// Some games read analog sticks via one of these raw paths specifically for
// the extra precision, which is why the sticks can stay broken even after
// the calibrated KPADStatus path above is fixed.
//
// Both callers translate a report in place (the source and destination are
// the same memory, viewed through different union members / pointer casts),
// so every value that's still needed has to be copied out to a local before
// the pro-shaped write begins.
static void TranslateClassicRawBufferToPro(WPADStatusClassic *classicView, WPADStatusProController *proView) {
    WPADStatus core         = classicView->core;
    uint16_t classicButtons = classicView->buttons;
    int16_t leftX           = classicView->leftStick.x;
    int16_t leftY           = classicView->leftStick.y;
    int16_t rightX          = classicView->rightStick.x;
    int16_t rightY          = classicView->rightStick.y;

    core.extensionType = WPAD_EXT_PRO_CONTROLLER;

    proView->core          = core;
    proView->buttons       = classicButtons; // WPAD_CLASSIC_BUTTON_* and WPAD_PRO_BUTTON_* share bit values
    proView->leftStick.x   = static_cast<int16_t>(leftX * 4);
    proView->leftStick.y   = static_cast<int16_t>(leftY * 4);
    proView->rightStick.x  = static_cast<int16_t>(rightX * 4);
    proView->rightStick.y  = static_cast<int16_t>(rightY * 4);
    proView->charging      = 0;
    proView->wired         = 0;
}

DECL_FUNCTION(void, KPADGetUnifiedWpadStatus, KPADChan chan, KPADUnifiedWpadStatus *buffer, uint32_t count) {
    real_KPADGetUnifiedWpadStatus(chan, buffer, count);
    if (gEnabled && buffer != nullptr) {
        for (uint32_t i = 0; i < count; i++) {
            KPADUnifiedWpadStatus &entry = buffer[i];
            if (IsClassicExtension(entry.classic.core.extensionType)) {
                TranslateClassicRawBufferToPro(&entry.classic, &entry.pro);
                entry.format = WPAD_FMT_PRO_CONTROLLER;
            }
        }
    }
}

DECL_FUNCTION(WPADError, WPADProbe, WPADChan chan, WPADExtensionType *outExtensionType) {
    WPADError err = real_WPADProbe(chan, outExtensionType);
    if (gEnabled && outExtensionType != nullptr && IsClassicExtension(static_cast<uint8_t>(*outExtensionType))) {
        *outExtensionType = WPAD_EXT_PRO_CONTROLLER;
    }
    return err;
}

// The raw WPADRead() path is format-dependent: a game calls WPADSetDataFormat()
// to choose what shape of report it wants, then passes a correspondingly
// sized/typed buffer to WPADRead(). Real Classic Controller hardware would
// reject a request for WPAD_FMT_PRO_CONTROLLER outright (the OS returns
// WPAD_ERROR_INVALID for "format is for a disabled device type"), so a game
// that only trusts analog sticks read through this specific path would see
// nothing, no matter what we do to KPADStatus or KPADGetUnifiedWpadStatus.
//
// So here we accept that request instead of forwarding it: keep the real
// link running in Classic format (so hardware keeps talking to us), remember
// that this channel *believes* it negotiated Pro format, and translate every
// WPADRead() result before handing it back. This is safe specifically because
// a game that asked for Pro format and was told "yes" is contractually
// expected (per WPADRead's own documentation) to pass a Pro-sized buffer, so
// writing a full WPADStatusProController into it can't overflow.
static WPADDataFormat sSpoofedFormat[kMaxWpadChannels] = {};
static bool sFormatIsSpoofed[kMaxWpadChannels]         = {};

DECL_FUNCTION(WPADError, WPADSetDataFormat, WPADChan channel, WPADDataFormat format) {
    if (gEnabled && channel < kMaxWpadChannels && format == WPAD_FMT_PRO_CONTROLLER) {
        WPADExtensionType trueExt = WPAD_EXT_UNKNOWN;
        if (real_WPADProbe(channel, &trueExt) == WPAD_ERROR_NONE && IsClassicExtension(static_cast<uint8_t>(trueExt))) {
            if (real_WPADSetDataFormat(channel, WPAD_FMT_CLASSIC) == WPAD_ERROR_NONE) {
                sFormatIsSpoofed[channel] = true;
                sSpoofedFormat[channel]   = WPAD_FMT_PRO_CONTROLLER;
                return WPAD_ERROR_NONE;
            }
        }
    }
    if (channel < kMaxWpadChannels) {
        sFormatIsSpoofed[channel] = false;
    }
    return real_WPADSetDataFormat(channel, format);
}

DECL_FUNCTION(WPADDataFormat, WPADGetDataFormat, WPADChan channel) {
    if (gEnabled && channel < kMaxWpadChannels && sFormatIsSpoofed[channel]) {
        return sSpoofedFormat[channel];
    }
    return real_WPADGetDataFormat(channel);
}

DECL_FUNCTION(void, WPADRead, WPADChan channel, WPADStatus *status) {
    real_WPADRead(channel, status);
    if (gEnabled && status != nullptr && channel < kMaxWpadChannels && sFormatIsSpoofed[channel] &&
        IsClassicExtension(status->extensionType)) {
        TranslateClassicRawBufferToPro(reinterpret_cast<WPADStatusClassic *>(status),
                                        reinterpret_cast<WPADStatusProController *>(status));
    }
}

// Games that watch for extension (re)connection asynchronously register a
// callback via WPADSetExtensionCallback() instead of polling WPADProbe().
// We install a single trampoline per channel that relabels the extension
// type the same way before forwarding to the game's real callback.
static WPADExtensionCallback sRealExtensionCallbacks[kMaxWpadChannels] = {};

static void ExtensionCallbackTrampoline(WPADChan channel, WPADExtensionType ext) {
    if (gEnabled && IsClassicExtension(static_cast<uint8_t>(ext))) {
        ext = WPAD_EXT_PRO_CONTROLLER;
    }
    if (channel < kMaxWpadChannels && sRealExtensionCallbacks[channel] != nullptr) {
        sRealExtensionCallbacks[channel](channel, ext);
    }
}

DECL_FUNCTION(WPADExtensionCallback, WPADSetExtensionCallback, WPADChan channel, WPADExtensionCallback callback) {
    WPADExtensionCallback previousRealCallback = nullptr;
    if (channel < kMaxWpadChannels) {
        previousRealCallback              = sRealExtensionCallbacks[channel];
        sRealExtensionCallbacks[channel] = callback;
    }

    WPADExtensionCallback previousInstalled =
            real_WPADSetExtensionCallback(channel, callback != nullptr ? ExtensionCallbackTrampoline : nullptr);

    // If our own trampoline was already installed, hand back the game's
    // previous callback instead of leaking our internal function pointer.
    return previousInstalled == ExtensionCallbackTrampoline ? previousRealCallback : previousInstalled;
}

WUPS_MUST_REPLACE(KPADReadEx, WUPS_LOADER_LIBRARY_PADSCORE, KPADReadEx);
WUPS_MUST_REPLACE(KPADRead, WUPS_LOADER_LIBRARY_PADSCORE, KPADRead);
WUPS_MUST_REPLACE(KPADGetUnifiedWpadStatus, WUPS_LOADER_LIBRARY_PADSCORE, KPADGetUnifiedWpadStatus);
WUPS_MUST_REPLACE(WPADProbe, WUPS_LOADER_LIBRARY_PADSCORE, WPADProbe);
WUPS_MUST_REPLACE(WPADSetDataFormat, WUPS_LOADER_LIBRARY_PADSCORE, WPADSetDataFormat);
WUPS_MUST_REPLACE(WPADGetDataFormat, WUPS_LOADER_LIBRARY_PADSCORE, WPADGetDataFormat);
WUPS_MUST_REPLACE(WPADRead, WUPS_LOADER_LIBRARY_PADSCORE, WPADRead);
WUPS_MUST_REPLACE(WPADSetExtensionCallback, WUPS_LOADER_LIBRARY_PADSCORE, WPADSetExtensionCallback);
