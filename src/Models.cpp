#include "cppmonetize/Models.h"

#include <QJsonArray>

namespace cppmonetize {
namespace {

void setError(ApiError* out, int statusCode, const QString& message)
{
    if (!out) {
        return;
    }
    out->statusCode = statusCode;
    out->message = message;
}

QString firstString(const QJsonObject& obj, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString v = obj.value(key).toString().trimmed();
        if (!v.isEmpty()) {
            return v;
        }
    }
    return {};
}

}  // namespace

std::optional<AuthSession> parseAuthSession(const QJsonObject& obj, ApiError* errorOut)
{
    AuthSession session;
    session.accessToken = firstString(obj, {QStringLiteral("access_token"), QStringLiteral("token")});
    if (session.accessToken.isEmpty()) {
        const QJsonObject s = obj.value(QStringLiteral("session")).toObject();
        session.accessToken = s.value(QStringLiteral("access_token")).toString().trimmed();
    }

    session.userId = obj.value(QStringLiteral("user")).toObject().value(QStringLiteral("id")).toString().trimmed();
    session.email = firstString(obj, {QStringLiteral("email")});
    if (session.email.isEmpty()) {
        session.email = obj.value(QStringLiteral("user")).toObject().value(QStringLiteral("email")).toString().trimmed();
    }

    if (session.accessToken.isEmpty()) {
        setError(errorOut, 0, QStringLiteral("Missing access token in auth response"));
        return std::nullopt;
    }
    return session;
}

std::optional<EntitlementsResponse> parseEntitlementsResponse(const QJsonObject& obj, ApiError* errorOut)
{
    EntitlementsResponse r;
    r.userId = firstString(obj, {QStringLiteral("user_id")});
    if (r.userId.isEmpty()) {
        r.userId = obj.value(QStringLiteral("user")).toObject().value(QStringLiteral("id")).toString().trimmed();
    }

    const QJsonArray arr = obj.value(QStringLiteral("entitlements")).toArray();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject e = v.toObject();
        Entitlement ent;
        ent.scopeType = e.value(QStringLiteral("scope_type")).toString().trimmed();
        ent.scopeId = e.value(QStringLiteral("scope_id")).toString().trimmed();
        ent.sourceType = e.value(QStringLiteral("source_type")).toString().trimmed();
        ent.sourceId = e.value(QStringLiteral("source_id")).toString().trimmed();
        ent.active = e.value(QStringLiteral("active")).toBool(false);
        ent.startsAt = e.value(QStringLiteral("starts_at")).toString().trimmed();
        ent.endsAt = e.value(QStringLiteral("ends_at")).toString().trimmed();
        r.entitlements.push_back(ent);
    }

    if (r.userId.isEmpty() && r.entitlements.isEmpty()) {
        setError(errorOut, 0, QStringLiteral("Entitlements response missing user and entitlements"));
        return std::nullopt;
    }
    return r;
}

std::optional<AiEntitlements> parseAiEntitlements(const QJsonObject& obj, ApiError* errorOut)
{
    AiEntitlements r;
    r.entitled = obj.value(QStringLiteral("entitled")).toBool(false);
    r.contractVersion = firstString(obj, {QStringLiteral("contract_version"), QStringLiteral("version")});
    r.userId = obj.value(QStringLiteral("user")).toObject().value(QStringLiteral("id")).toString().trimmed();

    const QJsonArray models = obj.value(QStringLiteral("models")).toArray();
    for (const QJsonValue& v : models) {
        if (v.isString()) {
            const QString s = v.toString().trimmed();
            if (!s.isEmpty()) {
                r.models.push_back(s);
            }
        } else if (v.isObject()) {
            const QString s = v.toObject().value(QStringLiteral("id")).toString().trimmed();
            if (!s.isEmpty()) {
                r.models.push_back(s);
            }
        }
    }

    const QJsonArray fallback = obj.value(QStringLiteral("fallback_order")).toArray();
    for (const QJsonValue& v : fallback) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty()) {
            r.fallbackOrder.push_back(s);
        }
    }

    const QJsonObject limits = obj.value(QStringLiteral("limits")).toObject();
    r.requestsPerMinute = limits.value(QStringLiteral("requests_per_minute")).toInt(0);
    r.projectBudget = limits.value(QStringLiteral("project_budget")).toInt(0);
    r.timeoutMs = limits.value(QStringLiteral("timeout_ms")).toInt(0);
    r.retries = limits.value(QStringLiteral("retries")).toInt(0);

    if (r.contractVersion.isEmpty()) {
        setError(errorOut, 0, QStringLiteral("AI entitlements response missing contract version"));
        return std::nullopt;
    }
    return r;
}

std::optional<CheckoutSession> parseCheckoutSession(const QJsonObject& obj, ApiError* errorOut)
{
    CheckoutSession s;
    s.checkoutUrl = QUrl(obj.value(QStringLiteral("checkout_url")).toString().trimmed());
    s.sessionId = obj.value(QStringLiteral("session_id")).toString().trimmed();
    s.mode = obj.value(QStringLiteral("mode")).toString().trimmed();
    s.productSlug = obj.value(QStringLiteral("product_slug")).toString().trimmed();

    if (!s.checkoutUrl.isValid() || s.checkoutUrl.isEmpty()) {
        setError(errorOut, 0, QStringLiteral("Checkout response missing valid checkout_url"));
        return std::nullopt;
    }
    return s;
}

std::optional<DownloadGrant> parseDownloadGrant(const QJsonObject& obj, ApiError* errorOut)
{
    DownloadGrant g;
    g.slug = obj.value(QStringLiteral("slug")).toString().trimmed();
    g.name = obj.value(QStringLiteral("name")).toString().trimmed();
    g.downloadUrl = QUrl(obj.value(QStringLiteral("download_url")).toString().trimmed());
    g.expiresInSeconds = obj.value(QStringLiteral("expires_in_seconds")).toInt(0);

    if (!g.downloadUrl.isValid() || g.downloadUrl.isEmpty()) {
        setError(errorOut, 0, QStringLiteral("Download response missing valid download_url"));
        return std::nullopt;
    }
    return g;
}

std::optional<AiUsageStatus> parseAiUsageStatus(const QJsonObject& obj, ApiError* errorOut)
{
    AiUsageStatus s;
    s.userId = obj.value(QStringLiteral("user_id")).toString().trimmed();
    s.usageMonth = obj.value(QStringLiteral("usage_month")).toString().trimmed();
    s.freeLimit = obj.value(QStringLiteral("free_limit")).toInt(0);
    s.freeUsed = obj.value(QStringLiteral("free_used")).toInt(0);
    s.freeRemaining = obj.value(QStringLiteral("free_remaining")).toInt(0);
    s.hasSubscription = obj.value(QStringLiteral("has_subscription")).toBool(false);
    s.allowAiRequests = obj.value(QStringLiteral("allow_ai_requests")).toBool(obj.value(QStringLiteral("ok")).toBool(false));
    s.requiresSubscription = obj.value(QStringLiteral("requires_subscription")).toBool(false);

    if (s.usageMonth.isEmpty() && s.userId.isEmpty()) {
        setError(errorOut, 0, QStringLiteral("AI usage response missing usage_month/user_id"));
        return std::nullopt;
    }
    return s;
}

CapabilitySet deriveCapabilitySet(const std::optional<AiEntitlements>& entitlements,
                                  const std::optional<AiUsageStatus>& usage)
{
    CapabilitySet c;
    if (entitlements.has_value()) {
        const AiEntitlements& e = entitlements.value();
        c.values.insert(QStringLiteral("ai.allow_requests"), e.entitled);
        c.values.insert(QStringLiteral("ai.requests_per_minute"), e.requestsPerMinute);
        c.values.insert(QStringLiteral("ai.project_budget"), e.projectBudget);
        c.values.insert(QStringLiteral("ai.timeout_ms"), e.timeoutMs);
        c.values.insert(QStringLiteral("ai.retries"), e.retries);
        c.values.insert(QStringLiteral("ai.contract_version"), e.contractVersion);
        c.values.insert(QStringLiteral("ai.models"), QJsonArray::fromStringList(e.models));
        c.values.insert(QStringLiteral("ai.fallback_order"), QJsonArray::fromStringList(e.fallbackOrder));
        c.schemaVersion = e.contractVersion;
    }
    if (usage.has_value()) {
        const AiUsageStatus& u = usage.value();
        c.values.insert(QStringLiteral("ai.has_subscription"), u.hasSubscription);
        c.values.insert(QStringLiteral("ai.requires_subscription"), u.requiresSubscription);
        const bool allowFromEnt = c.values.value(QStringLiteral("ai.allow_requests")).toBool(false);
        c.values.insert(QStringLiteral("ai.allow_requests"), allowFromEnt || u.allowAiRequests);
        c.values.insert(QStringLiteral("ai.usage_month"), u.usageMonth);
        c.values.insert(QStringLiteral("ai.free_limit"), u.freeLimit);
        c.values.insert(QStringLiteral("ai.free_used"), u.freeUsed);
        c.values.insert(QStringLiteral("ai.free_remaining"), u.freeRemaining);
    }
    return c;
}

}  // namespace cppmonetize
