// Platform：屏蔽 4 个平台能力点（自启 / 标题栏着色 / 托盘图标 / 单实例键）
//
// 业务代码只调这里的函数，不写 #ifdef；每个平台一个实现文件：
//   Platform_win.cpp    注册表 Run 键 + StartupApproved + DWM 着色
//   Platform_mac.cpp    ~/Library/LaunchAgents + launchctl + 菜单栏模板图标
//   Platform_linux.cpp  XDG autostart + 无边框着色 no-op

#ifndef PLATFORM_H
#define PLATFORM_H

#include <QColor>
#include <QIcon>
#include <QString>

class QWidget;

namespace Platform {

    // 开机自启的真实状态。macOS 走 LaunchAgent，没有"待用户批准"环节，
    // 但枚举保留完整三态：将来接 SMAppService（macOS 13+）时会用到。
    enum class AutoStartState {
        Disabled,
        Enabled,
        RequiresUserApproval,
        Unsupported,        // 平台不支持（例如没有 launchctl / 无法写自启目录）
    };

    struct WindowChromeColors {
        QColor caption;     // 无效色表示恢复系统默认
        QColor text;
        QColor border;
    };

    AutoStartState autoStartState();

    // 返回"操作被接受"，不代表一定已经生效（macOS 上 launchd 的加载是异步的）
    bool setAutoStartEnabled(bool enabled);

    // 标题栏/边框着色目前只有 Windows 11 支持，其他平台静默降级
    bool supportsWindowChromeColors();
    void applyWindowChromeColors(QWidget *window, const WindowChromeColors &colors);

    // macOS 菜单栏图标需要单色模板图；其他平台原样返回
    QIcon trayIcon(const QIcon &base);

    // 单实例 key：拼上用户数据目录的短哈希，避免同一台机器上多个用户串实例，
    // 同时保证长度不超过 macOS/Linux 上 QLocalServer 的 socket 路径上限
    QString singleInstanceKey(const QString &organization, const QString &application);

} // namespace Platform

#endif // PLATFORM_H
