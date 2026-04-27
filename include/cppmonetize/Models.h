#pragma once

#include "cppmonetize/ApiError.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace cppmonetize {

struct AuthSession {
    QString accessToken;
    QString userId;
    QString email;
};

struct Entitlement {
    QString scopeType;
    QString scopeId;
    QString sourceType;
    QString sourceId;
    bool active = false;
    QString startsAt;
    QString endsAt;
};

struct EntitlementsResponse {
    QString userId;
    QList<Entitlement> entitlements;
};

struct AiEntitlements {
    bool entitled = false;
    QString contractVersion;
    QString userId;
    QStringList models;
    QStringList fallbackOrder;
    int requestsPerMinute = 0;
    int projectBudget = 0;
    int timeoutMs = 0;
    int retries = 0;
};

struct CheckoutSession {
    QUrl checkoutUrl;
    QString sessionId;
    QString mode;
    QString productSlug;
};

struct DownloadGrant {
    QString slug;
    QString name;
    QUrl downloadUrl;
    int expiresInSeconds = 0;
};

struct AiUsageStatus {
    QString userId;
    QString usageMonth;
    int freeLimit = 0;
    int freeUsed = 0;
    int freeRemaining = 0;
    bool hasSubscription = false;
    bool allowAiRequests = false;
    bool requiresSubscription = false;
};

std::optional<AuthSession> parseAuthSession(const QJsonObject& obj, ApiError* errorOut = nullptr);
std::optional<EntitlementsResponse> parseEntitlementsResponse(const QJsonObject& obj, ApiError* errorOut = nullptr);
std::optional<AiEntitlements> parseAiEntitlements(const QJsonObject& obj, ApiError* errorOut = nullptr);
std::optional<CheckoutSession> parseCheckoutSession(const QJsonObject& obj, ApiError* errorOut = nullptr);
std::optional<DownloadGrant> parseDownloadGrant(const QJsonObject& obj, ApiError* errorOut = nullptr);
std::optional<AiUsageStatus> parseAiUsageStatus(const QJsonObject& obj, ApiError* errorOut = nullptr);

}  // namespace cppmonetize
