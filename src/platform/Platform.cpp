#include "platform/Platform.h"

#include <QCryptographicHash>
#include <QStandardPaths>
#include <QString>

namespace Platform {

    QString singleInstanceKey(const QString &organization, const QString &application) {
        // 同一个用户的数据目录是稳定的，用它做后缀即可做到"每用户一个实例"；
        // 取哈希是因为 QLocalServer 在 macOS/Linux 上用文件系统 socket，路径有长度上限。
        const QString userScope = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        const QByteArray digest = QCryptographicHash::hash(
            userScope.toUtf8(), QCryptographicHash::Sha1).toHex().left(8);

        return QStringLiteral("%1-%2-%3").arg(
            organization, application, QString::fromLatin1(digest));
    }

} // namespace Platform
