#include "cppmonetize/AuthIdentity.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>

namespace cppmonetize {

namespace {
QJsonObject tokenPayloadObject(const QString& accessToken)
{
    const QString token = accessToken.trimmed();
    if (token.isEmpty()) {
        return {};
    }
    const QStringList parts = token.split(QLatin1Char('.'));
    if (parts.size() < 2) {
        return {};
    }
    const QByteArray payloadBytes =
        QByteArray::fromBase64(parts.at(1).toUtf8(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    if (payloadBytes.isEmpty()) {
        return {};
    }
    const QJsonDocument payloadDoc = QJsonDocument::fromJson(payloadBytes);
    if (!payloadDoc.isObject()) {
        return {};
    }
    return payloadDoc.object();
}
}  // namespace

AccessTokenIdentity parseAccessTokenIdentity(const QString& accessToken)
{
    AccessTokenIdentity identity;
    const QJsonObject payload = tokenPayloadObject(accessToken);
    if (payload.isEmpty()) {
        return identity;
    }
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

EmailVerificationState emailVerificationStateFromAccessToken(const QString& accessToken)
{
    const QJsonObject payload = tokenPayloadObject(accessToken);
    if (payload.isEmpty()) {
        return EmailVerificationState::Unknown;
    }
    const bool hasEmailVerified = payload.contains(QStringLiteral("email_verified"));
    if (hasEmailVerified) {
        return payload.value(QStringLiteral("email_verified")).toBool(false)
            ? EmailVerificationState::Verified
            : EmailVerificationState::Unverified;
    }
    const bool hasEmailConfirmedAt = payload.contains(QStringLiteral("email_confirmed_at"));
    const QString emailConfirmedAt = payload.value(QStringLiteral("email_confirmed_at")).toString().trimmed();
    if (hasEmailConfirmedAt) {
        return emailConfirmedAt.isEmpty() ? EmailVerificationState::Unverified : EmailVerificationState::Verified;
    }
    const bool hasConfirmedAt = payload.contains(QStringLiteral("confirmed_at"));
    const QString confirmedAt = payload.value(QStringLiteral("confirmed_at")).toString().trimmed();
    if (hasConfirmedAt) {
        return confirmedAt.isEmpty() ? EmailVerificationState::Unverified : EmailVerificationState::Verified;
    }
    return EmailVerificationState::Unknown;
}

QString emailFromAccessToken(const QString& accessToken)
{
    return tokenPayloadObject(accessToken).value(QStringLiteral("email")).toString().trimmed();
}

QString profileImageUrlFromAccessToken(const QString& accessToken)
{
    const QJsonObject payload = tokenPayloadObject(accessToken);
    if (payload.isEmpty()) {
        return QString();
    }
    QString imageUrl = payload.value(QStringLiteral("avatar_url")).toString().trimmed();
    if (imageUrl.isEmpty()) {
        imageUrl = payload.value(QStringLiteral("picture")).toString().trimmed();
    }
    const QJsonObject userMeta = payload.value(QStringLiteral("user_metadata")).toObject();
    if (imageUrl.isEmpty()) {
        imageUrl = userMeta.value(QStringLiteral("avatar_url")).toString().trimmed();
    }
    if (imageUrl.isEmpty()) {
        imageUrl = userMeta.value(QStringLiteral("picture")).toString().trimmed();
    }
    const QUrl url(imageUrl);
    if (!url.isValid() || url.scheme().trimmed().isEmpty()) {
        return QString();
    }
    return url.toString();
}

}  // namespace cppmonetize
