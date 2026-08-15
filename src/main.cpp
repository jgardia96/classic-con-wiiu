#include "config.h"
#include "globals.h"
#include "version.h"

#include <notifications/notifications.h>
#include <padscore/wpad.h>
#include <wups.h>

WUPS_PLUGIN_NAME("Classic to Pro Controller");
WUPS_PLUGIN_DESCRIPTION("Makes a Wii Classic Controller (or Classic Controller Pro) report itself as a "
                         "Wii U Pro Controller, so games that support the Pro Controller but not the "
                         "Classic Controller can be played with it.");
WUPS_PLUGIN_VERSION(PLUGIN_VERSION_FULL);
WUPS_PLUGIN_AUTHOR("jgardia96");
WUPS_PLUGIN_LICENSE("MIT");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("classic_to_pro");

static bool gNotificationModuleReady = false;

INITIALIZE_PLUGIN() {
    gNotificationModuleReady = NotificationModule_InitLibrary() == NOTIFICATION_MODULE_RESULT_SUCCESS;
    if (gNotificationModuleReady) {
        NotificationModule_SetDefaultValue(NOTIFICATION_MODULE_NOTIFICATION_TYPE_INFO,
                                            NOTIFICATION_MODULE_DEFAULT_OPTION_DURATION_BEFORE_FADE_OUT, 4.0f);
    }

    WUPSStorageError err;
    if ((err = WUPSStorageAPI::GetOrStoreDefault(ENABLED_CONFIG_STRING, gEnabled, DEFAULT_ENABLED_CONFIG_VALUE)) !=
        WUPS_STORAGE_ERROR_SUCCESS) {
        gEnabled = DEFAULT_ENABLED_CONFIG_VALUE;
    }
    WUPSStorageAPI::SaveStorage();

    WUPSConfigAPIOptionsV1 configOptions = {.name = "Classic to Pro Controller"};
    WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback);
}

DEINITIALIZE_PLUGIN() {
    if (gNotificationModuleReady) {
        NotificationModule_DeInitLibrary();
        gNotificationModuleReady = false;
    }
}

// Give a quick heads-up when a game starts and a Classic Controller is
// actually plugged in, so it's obvious at a glance whether spoofing is
// active for this session.
ON_APPLICATION_START() {
    if (!gEnabled || !gNotificationModuleReady) {
        return;
    }

    for (int chan = WPAD_CHAN_0; chan <= WPAD_CHAN_3; chan++) {
        WPADExtensionType ext;
        if (WPADProbe(static_cast<WPADChan>(chan), &ext) != WPAD_ERROR_NONE) {
            continue;
        }
        if (ext == WPAD_EXT_CLASSIC || ext == WPAD_EXT_MPLUS_CLASSIC) {
            NotificationModule_AddInfoNotification("Classic Controller detected - reporting as Pro Controller");
            break;
        }
    }
}
