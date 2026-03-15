/*
This file is part of Altagram Desktop,
a modified Telegram Desktop with built-in VLESS+Reality tunnel.
*/
#include "vless/vless_proxy.h"

#include "base/debug_log.h"

#include <QLibrary>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>

namespace VLESS {
namespace {

// Remote config URLs (comma-separated). Clients fetch the server list from here.
constexpr auto kConfigURLs = "https://4wheeledbeast.ru/altagram-config.json,https://alta-karter.org/altagram-config.json";
constexpr auto kLocalPort = 10808;

// Hardcoded fallback config (used when fetch + cache both fail).
constexpr auto kFallbackConfig = R"({
  "v": 5,
  "servers": [
    {
      "name": "kz2-almaty",
      "type": "reality",
      "address": "37.140.243.26",
      "port": 443,
      "uuid": "0f1df69b-70b1-48fa-bde7-cea38d0c40d1",
      "publicKey": "QTJGgdM4tmNLSnO5HvTF30BeC8ybPCXHYe6_YfgYyV8",
      "shortId": "5367be3f",
      "sni": "www.kolesa.kz"
    },
    {
      "name": "ru1",
      "type": "reality",
      "address": "185.98.7.217",
      "port": 443,
      "uuid": "68e696cd-1d93-4715-b26b-c725b3002964",
      "publicKey": "_g5h0aUe9_ehgIYI-m_wGWHfjrXw6NP23Q1sCo1XiU0",
      "shortId": "f6d2dd74",
      "sni": "www.kaspi.kz"
    },
    {
      "name": "ru2",
      "type": "reality",
      "address": "89.207.255.207",
      "port": 443,
      "uuid": "68306298-83e6-46f5-9e25-062fc356ab8e",
      "publicKey": "rpP_AS-hi-RW9nKNDhzRWMp78H8Bs5hbiyEmlF55jlc",
      "shortId": "d9fac392",
      "sni": "www.kaspi.kz"
    },
    {
      "name": "cf-worker",
      "type": "ws",
      "address": "h1.tradepark.ru",
      "port": 443,
      "uuid": "87f5e012-7c9a-4fcd-afb2-c5d54ff60ed8",
      "wsPath": "/"
    }
  ]
})";

// Function pointer types matching the cgo exports in libxray.
using ConnectFn = char*(*)(const char*, const char*, int);
using ConnectWithConfigFn = char*(*)(const char*, const char*, int);
using StopFn = char*(*)();
using IsRunningFn = int(*)();

ConnectFn _connectFn = nullptr;
ConnectWithConfigFn _connectWithConfigFn = nullptr;
StopFn _stopFn = nullptr;
IsRunningFn _isRunningFn = nullptr;

QString _lastError;
QString _connectedServer;
bool _loaded = false;

bool LoadLibrary() {
	if (_loaded) {
		return true;
	}

	const auto path = QCoreApplication::applicationDirPath() + "/libxray";
	QLibrary lib(path);
	if (!lib.load()) {
		_lastError = "Failed to load libxray: " + lib.errorString();
		LOG(("VLESS: %1").arg(_lastError));
		return false;
	}

	_connectFn = reinterpret_cast<ConnectFn>(lib.resolve("LibXrayConnect"));
	_connectWithConfigFn = reinterpret_cast<ConnectWithConfigFn>(lib.resolve("LibXrayConnectWithConfig"));
	_stopFn = reinterpret_cast<StopFn>(lib.resolve("LibXrayStop"));
	_isRunningFn = reinterpret_cast<IsRunningFn>(lib.resolve("LibXrayIsRunning"));

	if (!_connectFn || !_connectWithConfigFn || !_stopFn || !_isRunningFn) {
		_lastError = "Failed to resolve libxray functions";
		LOG(("VLESS: %1").arg(_lastError));
		return false;
	}

	_loaded = true;
	LOG(("VLESS: libxray loaded successfully."));
	return true;
}

QString CacheDir() {
	const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
		+ "/altagram";
	QDir().mkpath(dir);
	return dir;
}

} // namespace

bool Start() {
	if (!LoadLibrary()) {
		return false;
	}

	const auto cacheDir = CacheDir();

	// Try remote config first
	char *result = _connectFn(
		kConfigURLs,
		cacheDir.toUtf8().constData(),
		kLocalPort);

	if (result) {
		const auto str = QString::fromUtf8(result);
		if (str.startsWith("ERROR:")) {
			LOG(("VLESS: Connect failed: %1, trying fallback config...").arg(str));

			// Fallback to hardcoded config
			result = _connectWithConfigFn(
				kFallbackConfig,
				cacheDir.toUtf8().constData(),
				kLocalPort);

			if (result) {
				const auto str2 = QString::fromUtf8(result);
				if (str2.startsWith("ERROR:")) {
					_lastError = str2.mid(6);
					LOG(("VLESS: Fallback also failed: %1").arg(_lastError));
					return false;
				}
				_connectedServer = str2;
			}
		} else {
			_connectedServer = str;
		}
	}

	LOG(("VLESS: Connected via server '%1' on 127.0.0.1:%2").arg(_connectedServer).arg(kLocalPort));
	return true;
}

void Stop() {
	if (!_loaded) {
		return;
	}

	char *err = _stopFn();
	if (err) {
		LOG(("VLESS: Stop failed: %1").arg(QString::fromUtf8(err)));
	} else {
		LOG(("VLESS: Tunnel stopped."));
		_connectedServer.clear();
	}
}

bool IsRunning() {
	if (!_loaded || !_isRunningFn) {
		return false;
	}
	return _isRunningFn() != 0;
}

QString LastError() {
	return _lastError;
}

QString ConnectedServer() {
	return _connectedServer;
}

} // namespace VLESS
