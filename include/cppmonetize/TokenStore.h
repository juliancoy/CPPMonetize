#pragma once

#include "cppmonetize/ApiError.h"
#include "cppmonetize/Result.h"

#include <QString>

#include <memory>

namespace cppmonetize {

class TokenStore {
public:
    virtual ~TokenStore() = default;

    virtual Result<bool> storeToken(const QString& token, const QString& userId) = 0;
    virtual Result<bool> storeSession(const QString& accessToken,
                                      const QString& refreshToken,
                                      const QString& userId) = 0;
    virtual Result<QString> loadToken() = 0;
    virtual Result<QString> loadRefreshToken() = 0;
    virtual Result<QString> loadUserId() = 0;
    virtual Result<bool> clear() = 0;
};

struct TokenStoreConfig {
    QString appName = QStringLiteral("cppmonetize");
    QString serviceName = QStringLiteral("cppmonetize.auth");
    QString orgName = QStringLiteral("cppmonetize");
};

std::unique_ptr<TokenStore> createDefaultTokenStore(const TokenStoreConfig& cfg = {});

}  // namespace cppmonetize
