// Platform 的 Windows 实现：开机自启（注册表 + StartupApproved）、DWM 标题栏着色。

#include "platform/Platform.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <dwmapi.h>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QWidget>

// 旧版 SDK 的 dwmapi.h 没有这几个属性，自己补上（值来自 Windows 11 SDK）
#ifndef DWMWA_BORDER_COLOR
#  define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#  define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#  define DWMWA_TEXT_COLOR 36
#endif
#ifndef DWMWA_COLOR_DEFAULT
#  define DWMWA_COLOR_DEFAULT 0xFFFFFFFF
#endif

namespace Platform {

    namespace {

        const QString kIdentifier = QStringLiteral("CampusNetLogin");

        // ========== 开机自启 ==========
        // Run 键按命令行解析。安装目录默认在 Program Files，路径含空格时必须加引号，
        // 否则登录阶段会去启动被截断的 C:\Program.exe。
        // 任务管理器里的开关不改 Run 值，而是写 StartupApproved\Run 的 REG_BINARY：
        // 首字节 02/06 为启用，03/07 为禁用。只改 Run 值无法重新打开被关掉的项。

        constexpr wchar_t kStartupApprovedRun[] =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";

        QSettings runSettings() {
            return QSettings(
                R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
                QSettings::NativeFormat);
        }

        QString quotedCommand() {
            const QString exePath = QDir::toNativeSeparators(
                QCoreApplication::applicationFilePath());
            return QLatin1Char('"') + exePath + QLatin1Char('"');
        }

        bool startupApprovedDisabled() {
            HKEY key = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, kStartupApprovedRun, 0,
                              KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
                return false;
            }

            BYTE data[16] = {};
            DWORD size = sizeof(data);
            DWORD type = 0;
            const LONG rc = RegQueryValueExW(
                key, reinterpret_cast<const wchar_t *>(kIdentifier.utf16()),
                nullptr, &type, data, &size);
            RegCloseKey(key);

            if (rc != ERROR_SUCCESS || type != REG_BINARY || size < 1)
                return false;
            return data[0] == 0x03 || data[0] == 0x07;
        }

        bool clearStartupApproved() {
            HKEY key = nullptr;
            const LONG openRc = RegOpenKeyExW(
                HKEY_CURRENT_USER, kStartupApprovedRun, 0, KEY_SET_VALUE, &key);
            if (openRc == ERROR_FILE_NOT_FOUND)
                return true;
            if (openRc != ERROR_SUCCESS)
                return false;

            const LONG delRc = RegDeleteValueW(
                key, reinterpret_cast<const wchar_t *>(kIdentifier.utf16()));
            RegCloseKey(key);
            return delRc == ERROR_SUCCESS || delRc == ERROR_FILE_NOT_FOUND;
        }

        bool commandMatches(const QSettings &settings) {
            return settings.value(kIdentifier).toString().compare(
                       quotedCommand(), Qt::CaseInsensitive) == 0;
        }

    } // namespace

    AutoStartState autoStartState() {
        QSettings settings = runSettings();
        if (!settings.contains(kIdentifier))
            return AutoStartState::Disabled;
        // 任务管理器禁用后 Run 值还在。这时不要改写路径，避免把用户关掉的项重新打开。
        if (startupApprovedDisabled())
            return AutoStartState::Disabled;

        if (commandMatches(settings))
            return AutoStartState::Enabled;

        // 旧版本写入的是未加引号的路径，或程序换过目录。项仍然启用时，改成当前 exe 的带引号命令。
        settings.setValue(kIdentifier, quotedCommand());
        settings.sync();
        return (settings.status() == QSettings::NoError && commandMatches(settings))
                   ? AutoStartState::Enabled
                   : AutoStartState::Disabled;
    }

    bool setAutoStartEnabled(bool enabled) {
        QSettings settings = runSettings();

        if (enabled)
            settings.setValue(kIdentifier, quotedCommand());
        else
            settings.remove(kIdentifier);

        settings.sync();
        if (settings.status() != QSettings::NoError)
            return false;
        // 启用和关闭都清掉禁用标记，否则任务管理器里关过一次之后，再勾选也不会启动。
        if (!clearStartupApproved())
            return false;

        QSettings verify = runSettings();
        if (enabled)
            return commandMatches(verify);
        return !verify.contains(kIdentifier);
    }

    // ========== 标题栏 / 边框着色 ==========
    // 仅 Windows 11 支持；旧系统调用失败即静默保持系统外观。

    bool supportsWindowChromeColors() {
        return true;
    }

    void applyWindowChromeColors(QWidget *window, const WindowChromeColors &colors) {
        if (!window)
            return;

        const auto toColorRef = [](const QColor &color) {
            return color.isValid()
                       ? RGB(color.red(), color.green(), color.blue())
                       : static_cast<COLORREF>(DWMWA_COLOR_DEFAULT);
        };

        const HWND hwnd = reinterpret_cast<HWND>(window->winId());
        const COLORREF caption = toColorRef(colors.caption);
        if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption,
                                         sizeof(caption)))) {
            return;
        }

        const COLORREF text = toColorRef(colors.text);
        DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));

        const COLORREF border = toColorRef(colors.border);
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
    }

    QIcon trayIcon(const QIcon &base) {
        return base;
    }

} // namespace Platform
