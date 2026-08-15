#pragma once

// Whether spoofing is currently active. Toggled from the WUPS config menu and
// persisted to storage.
extern bool gEnabled;

#define DEFAULT_ENABLED_CONFIG_VALUE true
#define ENABLED_CONFIG_STRING        "enabled"
