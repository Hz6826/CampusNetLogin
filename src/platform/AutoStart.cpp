//
// Created by Hz6826 on 2026/10/15.
//

// 兼容壳：真正的平台实现见 Platform_win.cpp / Platform_mac.cpp / Platform_linux.cpp。

#include "AutoStart.h"

namespace AutoStart {

    AutoStartState state() {
        return Platform::autoStartState();
    }

    bool setEnabled(bool enabled) {
        return Platform::setAutoStartEnabled(enabled);
    }

    QString identifier() {
        return QStringLiteral("CampusNetLogin");
    }

} // namespace AutoStart
