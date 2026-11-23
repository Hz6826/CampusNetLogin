#include "SingleInstance.h"

#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>

SingleInstance::SingleInstance(QString key, QObject *parent)
    : QObject(parent), m_key(std::move(key)) {}

SingleInstance::LockResult SingleInstance::tryLock() {
    auto pingExisting = [this] {
        QLocalSocket socket;
        socket.connectToServer(m_key);
        if (!socket.waitForConnected(200))
            return false;
        socket.write("raise");
        socket.flush();
        socket.waitForBytesWritten(200);
        return true;
    };

    if (pingExisting())
        return LockResult::AlreadyRunning;

    // 只清崩溃留下的管道名。活着的实例在上面已经连上，不会进到这里。
    QLocalServer::removeServer(m_key);

    m_server = new QLocalServer(this);
    if (!m_server->listen(m_key)) {
        // 对方刚 listen 成功。再通知一次并退出，不能再开一个窗口。
        if (pingExisting())
            return LockResult::AlreadyRunning;
        // 连不上也建不起来：不是"已有实例"，而是通道本身有问题
        // （只读的临时目录、socket 路径过长、被安全软件拦截……）。
        // 交给调用方决定怎么提示，不要静默退出。
        qWarning() << "单实例通道创建失败:" << m_key << m_server->errorString();
        m_server->deleteLater();
        m_server = nullptr;
        return LockResult::Failed;
    }

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (auto *conn = m_server->nextPendingConnection()) {
            connect(conn, &QLocalSocket::disconnected,
                    conn, &QObject::deleteLater);
            emit anotherInstanceStarted();
        }
    });

    return LockResult::Acquired;
}
