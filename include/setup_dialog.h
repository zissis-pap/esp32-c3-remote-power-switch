#pragma once

/**
 * Run the interactive boot-time setup dialog over USB Serial/JTAG.
 *
 * Requires runtime_config_load_defaults() to have been called first so that
 * g_cfg is populated before the dialog displays current values.
 *
 * Behaviour:
 *   - Installs the USB Serial/JTAG driver internally (uninstalled on return).
 *   - Prints current settings and a 5-second countdown.
 *   - Press 1 (or wait) → proceed with current settings unchanged.
 *   - Press 2          → edit each field; press Enter to keep the default.
 */
void setup_dialog_run(void);
