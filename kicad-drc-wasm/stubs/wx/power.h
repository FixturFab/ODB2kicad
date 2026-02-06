#pragma once
enum wxPowerType { wxPOWER_SOCKET = 0, wxPOWER_BATTERY, wxPOWER_UNKNOWN };
enum wxBatteryState { wxBATTERY_NORMAL_STATE = 0, wxBATTERY_LOW_STATE, wxBATTERY_CRITICAL_STATE, wxBATTERY_SHUTDOWN_STATE, wxBATTERY_UNKNOWN_STATE };
inline wxPowerType wxGetPowerType() { return wxPOWER_SOCKET; }
inline wxBatteryState wxGetBatteryState() { return wxBATTERY_NORMAL_STATE; }
