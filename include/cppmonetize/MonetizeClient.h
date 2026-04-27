#pragma once

#include "cppmonetize/Models.h"
#include "cppmonetize/OAuthDesktopFlow.h"
#include "cppmonetize/Result.h"

#include <QJsonObject>
#include <QString>

namespace cppmonetize {

struct EndpointConfig {
    QString authLoginPath = QStringLiteral("/auth/login");
    QString authRegisterPath = QStringLiteral("/auth/register");
    QString authWhoAmIPath = QStringLiteral("/auth/whoami");
    QString authSupabaseConfigPath = QStringLiteral("/auth/supabase-config");
    QString authDesktopStartPath = QStringLiteral("/api/auth/desktop/start");
    QString authDesktopExchangePath = QStringLiteral("/api/auth/desktop/exchange");
    QString oauthFallbackPathTemplate = QStringLiteral("/oauth/%1");

    QString aiEntitlementsPath = QStringLiteral("/api/ai/entitlements");
    QString aiTaskPath = QStringLiteral("/api/ai/task");
    QString aiUsagePath = QStringLiteral("/api/ai/request");

    QString licensePath = QStringLiteral("/license");
    QString checkoutBySlugPathTemplate = QStringLiteral("/api/packs/%1/purchase/stripe");
    QString downloadBySlugPathTemplate = QStringLiteral("/api/patches/%1/download");
};

struct ClientConfig {
    QString apiBaseUrl;
    int timeoutMs = 15000;
    QString clientId = QStringLiteral("cppmonetize-desktop");
    QString requiredContractPrefix = QStringLiteral("1.");
    EndpointConfig endpoints;
};

class MonetizeClient {
public:
    explicit MonetizeClient(ClientConfig config);

    const ClientConfig& config() const { return config_; }

    Result<AuthSession> signIn(const QString& email, const QString& password) const;
    Result<AuthSession> registerUser(const QString& email, const QString& password) const;
    Result<AuthSession> whoAmI(const QString& accessToken) const;

    Result<OAuthConfig> fetchOAuthConfig() const;
    QString buildDesktopStartUrl(const QString& redirectUri,
                                 const QString& state,
                                 const QString& codeChallenge,
                                 const QString& codeChallengeMethod = QStringLiteral("S256")) const;
    Result<AuthSession> exchangeDesktopCode(const QString& code,
                                            const QString& state,
                                            const QString& codeVerifier,
                                            const QString& redirectUri) const;

    Result<AiEntitlements> getAiEntitlements(const QString& accessToken) const;
    Result<QJsonObject> submitAiTask(const QString& accessToken, const QJsonObject& body) const;

    Result<CheckoutSession> createCheckoutForSlug(const QString& accessToken,
                                                  const QString& slug,
                                                  const QString& successUrl = {},
                                                  const QString& cancelUrl = {}) const;
    Result<DownloadGrant> getDownloadForSlug(const QString& accessToken, const QString& slug) const;

    Result<AiUsageStatus> getAiUsageStatus(const QString& accessToken) const;
    Result<AiUsageStatus> consumeAiUnits(const QString& accessToken, int units) const;

    Result<QJsonObject> getLicense(const QString& accessToken) const;

private:
    Result<QJsonObject> getJson(const QString& path, const QString& bearerToken = {}) const;
    Result<QJsonObject> postJson(const QString& path,
                                 const QJsonObject& payload,
                                 const QString& bearerToken = {}) const;
    QString normalizeBaseUrl() const;

    ClientConfig config_;
    OAuthDesktopFlow oauthFlow_;
};

}  // namespace cppmonetize
