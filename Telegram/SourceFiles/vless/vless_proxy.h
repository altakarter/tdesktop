/*
This file is part of Altagram Desktop,
a modified Telegram Desktop with built-in VLESS+Reality tunnel.
*/
#pragma once

#include <QString>

namespace VLESS {

// Connects to the best available server from remote config.
// Fetches server list from configURLs, tries each until one works.
// Returns true on success, false on failure (check LastError()).
bool Start();

// Stops the tunnel.
void Stop();

// Returns true if the tunnel is currently running.
bool IsRunning();

// Returns the last error message, if any.
QString LastError();

// Returns the name of the connected server, or empty if not connected.
QString ConnectedServer();

// SOCKS5 proxy address for MTProto connections.
constexpr auto kProxyHost = "127.0.0.1";
constexpr uint32 kProxyPort = 10808;

} // namespace VLESS
