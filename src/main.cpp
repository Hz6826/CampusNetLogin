#include <AppInfo.h>
#include <QApplication>
// ReSharper disable once CppUnusedIncludeDirective
#include <QIcon>
#include <QMessageBox>
#include <QTimer>

#include "net/SrunClient.h"
#include "platform/Platform.h"
#include "ui/MainWindow.h"
#include "ui/TrayController.h"
#include "util/SingleInstance.h"

#ifdef Q_OS_MACOS
#  include <QEvent>
#endif

namespace {

    // 把主窗口拉回前台：二次启动、Dock 点击、托盘菜单共用同一套动作
    void wakeMainWindow(MainWindow &window) {
        if (window.isMinimized()) {
            window.showNormal();
        } else if (!window.isVisible()) {
            window.show();
        }
        QTimer::singleShot(0, &window, [&window] {
            window.raise();
            window.activateWindow();
        });
    }

#ifdef Q_OS_MACOS
    // macOS 上关窗只是收起窗口（setQuitOnLastWindowClosed(false)），
    // 点 Dock 图标时系统不会再走一次进程启动，只能靠应用激活事件把窗口拉回来。
    class DockReopenFilter : public QObject {
    public:
        explicit DockReopenFilter(MainWindow *window, QObject *parent = nullptr)
            : QObject(parent), m_window(window) {}

    protected:
        bool eventFilter(QObject *obj, QEvent *event) override {
            if (event->type() == QEvent::ApplicationActivate
                && m_window != nullptr && !m_window->isVisible()) {
                wakeMainWindow(*m_window);
            }
            return QObject::eventFilter(obj, event);
        }

    private:
        MainWindow *m_window = nullptr;
    };
#endif

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(AppInfo::Author);
    QApplication::setApplicationName(AppInfo::Name);
    QApplication::setApplicationVersion(AppInfo::Version);

#ifdef Q_OS_MACOS
    // macOS 的窗口习惯：关掉窗口不退出应用，退出走 Cmd+Q / 菜单 / 托盘退出
    QApplication::setQuitOnLastWindowClosed(false);
#endif

    // ---------- 单实例保护 ----------
    // key 里带用户数据目录的短哈希：同一台机器上多个用户互不干扰
    SingleInstance instance(
        Platform::singleInstanceKey(AppInfo::Author, AppInfo::Name));

    switch (instance.tryLock()) {
        case SingleInstance::LockResult::Acquired:
            break;
        case SingleInstance::LockResult::AlreadyRunning:
            // 已有实例在跑，已通知它唤起窗口，当前进程静默退出
            return 0;
        case SingleInstance::LockResult::Failed:
            // 静默退出会让用户以为"点了没反应"，这里必须给个可见提示
            QMessageBox::warning(
                nullptr,
                QStringLiteral("单实例保护不可用"),
                QStringLiteral(
                    "无法创建单实例通信通道（可能是权限或临时目录问题）。\n"
                    "程序会继续启动，但可能出现多个窗口。"));
            break;
    }
    // --------------------------------

    QIcon appIcon;
    appIcon.addFile(":/icons/app/app-16.png");
    appIcon.addFile(":/icons/app/app-32.png");
    appIcon.addFile(":/icons/app/app-48.png");
    appIcon.addFile(":/icons/app/app-64.png");
    appIcon.addFile(":/icons/app/app-128.png");
    appIcon.addFile(":/icons/app/app-256.png");
    QApplication::setWindowIcon(appIcon);

    SrunClient client;
    MainWindow window(&client);
    TrayController tray(&client, &window);

    // 第二个实例启动 → 唤起已有窗口
    QObject::connect(&instance, &SingleInstance::anotherInstanceStarted,
                     &window, [&window] { wakeMainWindow(window); });

#ifdef Q_OS_MACOS
    // 关闭窗口后点 Dock 图标 → 重新显示主窗口
    app.installEventFilter(new DockReopenFilter(&window, &app));
#endif

    client.startPolling(window.config().pollTimerIntervalMs);
    window.show();

    return QApplication::exec();
}
