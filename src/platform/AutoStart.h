//
// Created by Hz6826 on 2026/10/15.
//

// 开机自启的对外 API。具体实现按平台在 src/platform/Platform_{win,mac,linux}.cpp 里，
// 这里只做转发，让 UI 层继续保持原有的调用点（AutoStart::xxx）。
//
// 与旧版的区别：isEnabled() 这种"只看文件/注册表项在不在"的判断换成了三态 state()，
// 平台不支持的场合 UI 可以据此禁用勾选框，而不是"勾上但永远不生效"。

#ifndef AUTOSTART_H
#define AUTOSTART_H

#include <QString>

#include "platform/Platform.h"

namespace AutoStart {

    using AutoStartState = Platform::AutoStartState;

    // 系统里的真实自启状态
    AutoStartState state();

    // 返回"操作被接受"；macOS 上 launchd 的加载是异步的，不代表立刻已生效
    bool setEnabled(bool enabled);

    QString identifier();

} // namespace AutoStart

#endif // AUTOSTART_H
