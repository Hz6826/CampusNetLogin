#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

class SingleInstance : public QObject {
    Q_OBJECT
public:
    enum class LockResult {
        Acquired,       // 拿到锁，本进程是唯一实例
        AlreadyRunning, // 已有实例，并且已经通知它唤起窗口
        Failed,         // 通道建不起来（权限/临时目录），单实例保护失效
    };

    explicit SingleInstance(QString key, QObject *parent = nullptr);

    LockResult tryLock();

    signals:
        void anotherInstanceStarted();

private:
    QString m_key;
    QLocalServer *m_server = nullptr;
};
