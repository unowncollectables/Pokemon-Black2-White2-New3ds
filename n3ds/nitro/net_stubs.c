/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * net_stubs.c — formerly the GFL_Net session fiction.
 *
 * Wi-Fi Stage 1 retired every stub that lived here: the real GFL_Net
 * layer (src/gfl/net.c, net_system.c, net_state.c, net_handle.c,
 * net_command.c, net_irc_wireless.c, net_lower_data.c) and the real
 * error layer (src/system/net_err.c) now link via
 * scripts/n3ds_link_closure.py, over the real DWC/NitroWiFi stack and
 * the transport bottom in net_bottom.c. GFL_NetIsInitialized no longer
 * answers FALSE forever — it reports the Wi-Fi device table's real
 * running state, which is what lets the Mystery Gift Wi-Fi path start.
 *
 * The file stays in the build (n3ds/Makefile) as the place where a
 * future GFL_Net-adjacent host shim would live. Do not re-add session
 * fiction here: if a GFL_Net symbol is missing, link the real object.
 */
