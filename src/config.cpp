#include "config.h"
#include "globals.h"

#include <coreinit/debug.h>
#include <string_view>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/storage.h>

static void BoolItemChanged(ConfigItemBoolean *item, bool newValue) {
    if (item == nullptr || item->identifier == nullptr) {
        return;
    }

    if (std::string_view(ENABLED_CONFIG_STRING) == item->identifier) {
        gEnabled = newValue;
    } else {
        return;
    }

    WUPSStorageAPI::Store(item->identifier, newValue);
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle) {
    try {
        WUPSConfigCategory root = WUPSConfigCategory(rootHandle);

        root.add(WUPSConfigItemBoolean::Create(ENABLED_CONFIG_STRING,
                                                "Report Classic Controller as Pro Controller",
                                                DEFAULT_ENABLED_CONFIG_VALUE, gEnabled,
                                                &BoolItemChanged));
    } catch (std::exception &e) {
        OSReport("classic_to_pro: exception building config menu: %s\n", e.what());
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback() {
    WUPSStorageAPI::SaveStorage();
}
