#pragma once

#include <QString>

namespace cppmonetize {

struct AccessTokenIdentity {
    QString email;
    QString userId;

    QString displayIdentity() const
    {
        if (!email.trimmed().isEmpty()) {
            return email.trimmed();
        }
        return userId.trimmed();
    }

    bool hasIdentity() const
    {
        return !email.trimmed().isEmpty() || !userId.trimmed().isEmpty();
    }
};

enum class EmailVerificationState { Verified, Unverified, Unknown };

AccessTokenIdentity parseAccessTokenIdentity(const QString& accessToken);
EmailVerificationState emailVerificationStateFromAccessToken(const QString& accessToken);
QString emailFromAccessToken(const QString& accessToken);
QString profileImageUrlFromAccessToken(const QString& accessToken);

}  // namespace cppmonetize
