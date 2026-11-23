// Platform 的 macOS 实现：LaunchAgent 自启、菜单栏模板图标。
// 不引入 Objective-C++：需要的两个系统能力 QIcon::setIsMask / launchctl 都有纯 Qt 通路。

#include "platform/Platform.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#include <unistd.h>

namespace Platform {

    namespace {

        // 必须与 Info.plist 的 CFBundleIdentifier 一致：
        // LaunchAgent 的 label 与 Bundle ID 对齐后，用户在 launchctl / 系统日志里看到的才是同一个应用
        const QString kBundleId = QStringLiteral("com.hz6826.CampusNetLogin");

        QString launchAgentPath() {
            return QDir::homePath() + "/Library/LaunchAgents/" + kBundleId + ".plist";
        }

        QString currentUserUid() {
            return QString::number(::getuid());
        }

        QString xmlEscape(const QString &text) {
            // & 必须最先处理
            QString escaped = text;
            escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
            escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
            escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
            escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
            escaped.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
            return escaped;
        }

        bool launchctlAvailable() {
            return !QStandardPaths::findExecutable(QStringLiteral("launchctl")).isEmpty();
        }

        // 返回退出码；失败（没能启动/超时）返回 -1，stderr 通过 err 带出
        int runLaunchctl(const QStringList &arguments, QByteArray *err = nullptr) {
            QProcess process;
            process.start(QStringLiteral("launchctl"), arguments);
            if (!process.waitForStarted(5000))
                return -1;
            if (!process.waitForFinished(15000)) {
                process.kill();
                process.waitForFinished(2000);
                return -1;
            }
            if (err)
                *err = process.readAllStandardError();
            return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
        }

    } // namespace

    AutoStartState autoStartState() {
        if (!launchctlAvailable())
            return AutoStartState::Unsupported;

        // LaunchAgent 写在 ~/Library/LaunchAgents 下，登录时由 launchd 自动加载
        return QFile::exists(launchAgentPath()) ? AutoStartState::Enabled
                                                : AutoStartState::Disabled;
    }

    bool setAutoStartEnabled(bool enabled) {
        if (!launchctlAvailable())
            return false;

        const QString path = launchAgentPath();

        if (!enabled) {
            // 先让 launchd 卸载，再删文件；服务本来就没加载时 bootout 会返回非 0，忽略
            runLaunchctl({QStringLiteral("bootout"),
                          QStringLiteral("gui/%1/%2").arg(currentUserUid(), kBundleId)});
            QFile::remove(path);
            return !QFile::exists(path);
        }

        if (!QDir().mkpath(QFileInfo(path).absolutePath()))
            return false;

        const QString content = QString(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "    <key>Label</key>\n"
            "    <string>%1</string>\n"
            "    <key>ProgramArguments</key>\n"
            "    <array>\n"
            "        <string>%2</string>\n"
            "    </array>\n"
            "    <key>RunAtLoad</key>\n"
            "    <true/>\n"
            "    <key>LimitLoadToSessionType</key>\n"
            "    <string>Aqua</string>\n"
            "</dict>\n"
            "</plist>\n"
        ).arg(xmlEscape(kBundleId),
             xmlEscape(QCoreApplication::applicationFilePath()));

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        file.write(content.toUtf8());
        file.close();
        if (!QFile::exists(path))
            return false;

        // 让 launchd 立即接管（bootstrap 取代已弃用的 load）。
        // RunAtLoad=true 会马上启动一次，单实例保护会把它收敛成"唤起已有窗口"，不会出现两个进程。
        QByteArray err;
        const int rc = runLaunchctl(
            {QStringLiteral("bootstrap"),
             QStringLiteral("gui/%1").arg(currentUserUid()), path}, &err);
        if (rc != 0 && !err.contains("already loaded")) {
            // 不当作失败：plist 已经落盘，下次登录仍会被加载；
            // 但要让用户从日志里看得到"这次没立即生效"。
            qWarning() << "launchctl bootstrap 未成功（将在下次登录时生效）:"
                       << rc << QString::fromUtf8(err).trimmed();
        }

        return true;
    }

    bool supportsWindowChromeColors() {
        // 原生标题栏着色需要 NSWindow，当前未实现，静默降级
        return false;
    }

    void applyWindowChromeColors(QWidget *, const WindowChromeColors &) {
        // no-op
    }

    QIcon trayIcon(const QIcon &base) {
        // macOS 菜单栏要求单色模板图，由系统按浅色/深色自动反色
        QIcon icon = base;
        icon.setIsMask(true);
        return icon;
    }

} // namespace Platform
