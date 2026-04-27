#include "cppmonetize/AuthIdentity.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace cppmonetize {

AccessTokenIdentity parseAccessTokenIdentity(const QString& accessToken)
{
    AccessTokenIdentity identity;
    const QString token = accessToken.trimmed();
    if (token.isEmpty()) {
        return identity;
    }

    const QStringList parts = token.split(QLatin1Char('.'));
    if (parts.size() < 2) {
        return identity;
    }

    const QByteArray payloadBytes =
        QByteArray::fromBase64(parts.at(1).toUtf8(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    if (payloadBytes.isEmpty()) {
        return identity;
    }

    const QJsonDocument payloadDoc = QJsonDocument::fromJson(payloadBytes);
    if (!payloadDoc.isObject()) {
        return identity;
    }

    const QJsonObject payload = payloadDoc.object();
    identity.email = payload.value(QStringLiteral("email")).toString().trimmed();
    if (identity.email.isEmpty()) {
        identity.email = payload.value(QStringLiteral("user_metadata"))
                             .toObject()
                             .value(QStringLiteral("email"))
                             .toString()
                             .trimmed();
    }
    identity.userId = payload.value(QStringLiteral("sub")).toString().trimmed();
    return identity;
}

}  // namespace cppmonetize
