// Platform 的 Linux 实现：XDG autostart 自启。

#include "platform/Platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace Platform {

    namespace {

        const QString kIdentifier = QStringLiteral("CampusNetLogin");

        QString autostartPath() {
            const QString configHome = QStandardPaths::writableLocation(
                QStandardPaths::GenericConfigLocation);
            return configHome + "/autostart/" + kIdentifier + ".desktop";
        }

        // Desktop Entry 规范：含空格的路径要加双引号，\ " ` $ 需反斜杠转义
        QString desktopExecEscape(const QString &path) {
            QString escaped = path;
            escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
            escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
            escaped.replace(QLatin1Char('`'), QStringLiteral("\\`"));
            escaped.replace(QLatin1Char('$'), QStringLiteral("\\$"));
            return QLatin1Char('"') + escaped + QLatin1Char('"');
        }

    } // namespace

    AutoStartState autoStartState() {
        const QString path = autostartPath();
        if (!QFile::exists(path))
            return AutoStartState::Disabled;

        // 桌面环境允许在 .desktop 里把条目关掉但保留文件；
        // 只看文件存在会误报"已启用"，用户关掉自启后 UI 还显示勾选。
        const QSettings settings(path, QSettings::IniFormat);
        if (settings.value(QStringLiteral("Desktop Entry/Hidden"), false).toBool())
            return AutoStartState::Disabled;
        if (!settings.value(QStringLiteral("Desktop Entry/X-GNOME-Autostart-enabled"), true).toBool())
            return AutoStartState::Disabled;

        return AutoStartState::Enabled;
    }

    bool setAutoStartEnabled(bool enabled) {
        const QString path = autostartPath();

        if (!enabled) {
            QFile::remove(path);
            return !QFile::exists(path);
        }

        if (!QDir().mkpath(QFileInfo(path).absolutePath()))
            return false;

        // 遵循 XDG Autostart 规范
        const QString content = QString(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=%1\n"
            "Exec=%2\n"
            "Terminal=false\n"
            "X-GNOME-Autostart-enabled=true\n"
        ).arg(kIdentifier,
              desktopExecEscape(QCoreApplication::applicationFilePath()));

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        file.write(content.toUtf8());
        file.close();

        return QFile::exists(path);
    }

    bool supportsWindowChromeColors() {
        return false;
    }

    void applyWindowChromeColors(QWidget *, const WindowChromeColors &) {
        // no-op
    }

    QIcon trayIcon(const QIcon &base) {
        return base;
    }

} // namespace Platform
