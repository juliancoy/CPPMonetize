#include "cppmonetize/MonetizeClient.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

namespace cppmonetize {
namespace {

ApiError makeError(int statusCode, const QString& message, const QString& details = {})
{
    ApiError e;
    e.statusCode = statusCode;
    e.message = message;
    e.details = details;
    return e;
}

QString normalizePath(const QString& path)
{
    if (path.startsWith(QLatin1Char('/'))) {
        return path;
    }
    return QStringLiteral("/") + path;
}

bool isSupabaseOriginBase(const QString& baseUrl)
{
    return baseUrl.contains(QStringLiteral(".supabase.co"), Qt::CaseInsensitive);
}

QString parseErrorMessage(const QJsonObject& obj)
{
    const QStringList keys{
        QStringLiteral("error"),
        QStringLiteral("message"),
        QStringLiteral("detail"),
        QStringLiteral("details")};
    for (const QString& key : keys) {
        const QString value = obj.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

Result<QJsonObject> performJsonRequest(const QString& method,
                                       const QUrl& url,
                                       const QJsonObject& payload,
                                       const QString& bearerToken,
                                       const ClientConfig& config,
                                       const QString& operation)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("X-CPPMonetize-Client", config.clientId.toUtf8());
    const QString clientRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!config.requestIdHeaderName.trimmed().isEmpty()) {
        request.setRawHeader(config.requestIdHeaderName.toUtf8(), clientRequestId.toUtf8());
    }
    if (!bearerToken.trimmed().isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + bearerToken.trimmed()).toUtf8());
    }

    QNetworkReply* reply = nullptr;
    if (method == QStringLiteral("GET")) {
        reply = manager.get(request);
    } else {
        const QByteArray bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        reply = manager.post(request, bytes);
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QElapsedTimer elapsed;
    elapsed.start();
    timer.start(config.timeoutMs);
    loop.exec();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    const bool timedOut = (reply->error() == QNetworkReply::OperationCanceledError && timer.isActive() == false);
    const auto parseResult = QJsonDocument::fromJson(raw);
    const QJsonObject obj = parseResult.isObject() ? parseResult.object() : QJsonObject{};
    const qint64 durationMs = elapsed.elapsed();

    auto emitTelemetry = [&](bool success, const QString& message) {
        if (!config.telemetryHook) {
            return;
        }
        RequestTelemetryEvent event;
        event.operation = operation;
        event.method = method;
        event.url = url.toString();
        event.clientRequestId = clientRequestId;
        event.statusCode = statusCode;
        event.durationMs = durationMs;
        event.success = success;
        event.message = message;
        config.telemetryHook(event);
    };

    if (timedOut) {
        reply->deleteLater();
        emitTelemetry(false, QStringLiteral("Request timed out"));
        return Result<QJsonObject>::fail(makeError(0, QStringLiteral("Request timed out"), url.toString()));
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString parsed = parseErrorMessage(obj);
        const QString msg = !parsed.isEmpty() ? parsed : reply->errorString();
        reply->deleteLater();
        emitTelemetry(false, msg);
        return Result<QJsonObject>::fail(makeError(statusCode, msg, QString::fromUtf8(raw)));
    }

    if (statusCode >= 400) {
        const QString parsed = parseErrorMessage(obj);
        reply->deleteLater();
        emitTelemetry(false, parsed.isEmpty() ? QStringLiteral("HTTP error") : parsed);
        return Result<QJsonObject>::fail(
            makeError(statusCode, parsed.isEmpty() ? QStringLiteral("HTTP error") : parsed, QString::fromUtf8(raw)));
    }

    reply->deleteLater();
    emitTelemetry(true, QStringLiteral("OK"));
    return Result<QJsonObject>::ok(obj);
}

}  // namespace

MonetizeClient::MonetizeClient(ClientConfig config)
    : config_(std::move(config))
{
}

Result<AuthSession> MonetizeClient::signIn(const QString& email, const QString& password) const
{
    const auto resp = postJson(config_.endpoints.authLoginPath,
                               QJsonObject{{QStringLiteral("email"), email}, {QStringLiteral("password"), password}});
    if (!resp.hasValue()) {
        return Result<AuthSession>::fail(resp.error());
    }

    ApiError parseError;
    auto parsed = parseAuthSession(resp.value(), &parseError);
    if (!parsed) {
        return Result<AuthSession>::fail(parseError);
    }
    return Result<AuthSession>::ok(*parsed);
}

Result<AuthSession> MonetizeClient::registerUser(const QString& email, const QString& password) const
{
    const auto resp = postJson(config_.endpoints.authRegisterPath,
                               QJsonObject{{QStringLiteral("email"), email}, {QStringLiteral("password"), password}});
    if (!resp.hasValue()) {
        return Result<AuthSession>::fail(resp.error());
    }

    ApiError parseError;
    auto parsed = parseAuthSession(resp.value(), &parseError);
    if (!parsed) {
        return Result<AuthSession>::fail(parseError);
    }
    return Result<AuthSession>::ok(*parsed);
}

Result<AuthSession> MonetizeClient::whoAmI(const QString& accessToken) const
{
    const auto resp = getJson(config_.endpoints.authWhoAmIPath, accessToken);
    if (!resp.hasValue()) {
        return Result<AuthSession>::fail(resp.error());
    }

    AuthSession session;
    session.accessToken = accessToken;
    session.userId = resp.value().value(QStringLiteral("id")).toString().trimmed();
    session.email = resp.value().value(QStringLiteral("email")).toString().trimmed();
    if (session.userId.isEmpty()) {
        session.userId = resp.value().value(QStringLiteral("user")).toObject().value(QStringLiteral("id")).toString().trimmed();
    }
    if (session.email.isEmpty()) {
        session.email = resp.value().value(QStringLiteral("user")).toObject().value(QStringLiteral("email")).toString().trimmed();
    }
    return Result<AuthSession>::ok(session);
}

Result<OAuthConfig> MonetizeClient::fetchOAuthConfig() const
{
    return oauthFlow_.fetchOAuthConfig(normalizeBaseUrl(), config_.timeoutMs);
}

QString MonetizeClient::buildDesktopStartUrl(const QString& redirectUri,
                                             const QString& state,
                                             const QString& codeChallenge,
                                             const QString& codeChallengeMethod) const
{
    QUrl url(normalizeBaseUrl() + normalizePath(config_.endpoints.authDesktopStartPath));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    q.addQueryItem(QStringLiteral("state"), state);
    q.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
    q.addQueryItem(QStringLiteral("code_challenge_method"), codeChallengeMethod);
    q.addQueryItem(QStringLiteral("client"), config_.clientId);
    url.setQuery(q);
    return url.toString();
}

Result<AuthSession> MonetizeClient::exchangeDesktopCode(const QString& code,
                                                        const QString& state,
                                                        const QString& codeVerifier,
                                                        const QString& redirectUri) const
{
    const auto resp = postJson(config_.endpoints.authDesktopExchangePath,
                               QJsonObject{{QStringLiteral("code"), code},
                                           {QStringLiteral("state"), state},
                                           {QStringLiteral("code_verifier"), codeVerifier},
                                           {QStringLiteral("redirect_uri"), redirectUri},
                                           {QStringLiteral("client"), config_.clientId}});
    if (!resp.hasValue()) {
        return Result<AuthSession>::fail(resp.error());
    }

    ApiError parseError;
    auto parsed = parseAuthSession(resp.value(), &parseError);
    if (!parsed) {
        return Result<AuthSession>::fail(parseError);
    }
    return Result<AuthSession>::ok(*parsed);
}

Result<AiEntitlements> MonetizeClient::getAiEntitlements(const QString& accessToken) const
{
    const auto resp = getJson(config_.endpoints.aiEntitlementsPath, accessToken);
    if (resp.hasValue()) {
        ApiError parseError;
        auto parsed = parseAiEntitlements(resp.value(), &parseError);
        if (parsed) {
            if (!config_.requiredContractPrefix.isEmpty() &&
                !parsed->contractVersion.startsWith(config_.requiredContractPrefix)) {
                return Result<AiEntitlements>::fail(
                    makeError(0,
                              QStringLiteral("Unsupported contract version: %1").arg(parsed->contractVersion),
                              QStringLiteral("required prefix: %1").arg(config_.requiredContractPrefix)));
            }
            return Result<AiEntitlements>::ok(*parsed);
        }
    }

    if (!isSupabaseOriginBase(normalizeBaseUrl())) {
        if (!resp.hasValue()) {
            return Result<AiEntitlements>::fail(resp.error());
        }
        return Result<AiEntitlements>::fail(
            makeError(0, QStringLiteral("Failed to parse AI entitlements response.")));
    }

    // Supabase direct fallback: derive entitlement state from ai_request status function.
    const auto aiUsageResp = getJson(QStringLiteral("/functions/v1/ai_request"), accessToken);
    if (!aiUsageResp.hasValue()) {
        return Result<AiEntitlements>::fail(aiUsageResp.error());
    }

    const QJsonObject obj = aiUsageResp.value();
    AiEntitlements ent;
    ent.entitled = obj.value(QStringLiteral("allow_ai_requests")).toBool(false);
    ent.contractVersion = QStringLiteral("1.supabase");
    ent.userId = obj.value(QStringLiteral("user_id")).toString().trimmed();
    ent.models = QStringList{QStringLiteral("deepseek-chat")};
    ent.fallbackOrder = ent.models;
    ent.requestsPerMinute = 12;
    ent.projectBudget = 200;
    ent.timeoutMs = qMax(1000, config_.timeoutMs);
    ent.retries = 1;

    if (!config_.requiredContractPrefix.isEmpty() &&
        !ent.contractVersion.startsWith(config_.requiredContractPrefix)) {
        return Result<AiEntitlements>::fail(
            makeError(0,
                      QStringLiteral("Unsupported contract version: %1").arg(ent.contractVersion),
                      QStringLiteral("required prefix: %1").arg(config_.requiredContractPrefix)));
    }

    return Result<AiEntitlements>::ok(ent);
}

Result<QJsonObject> MonetizeClient::submitAiTask(const QString& accessToken, const QJsonObject& body) const
{
    if (isSupabaseOriginBase(normalizeBaseUrl())) {
        const QString model = body.value(QStringLiteral("model")).toString().trimmed().isEmpty()
            ? QStringLiteral("deepseek-chat")
            : body.value(QStringLiteral("model")).toString().trimmed();
        const QString action = body.value(QStringLiteral("action")).toString().trimmed();
        const QJsonObject payload = body.value(QStringLiteral("payload")).toObject();

        const QString instructions = payload.value(QStringLiteral("instructions")).toString().trimmed().isEmpty()
            ? QStringLiteral("You are an assistant for desktop editing workflows. Be concise and return JSON when requested.")
            : payload.value(QStringLiteral("instructions")).toString().trimmed();
        const QString transcriptText = payload.value(QStringLiteral("transcript_text")).toString();
        QString userPrompt;
        if (!transcriptText.trimmed().isEmpty()) {
            userPrompt = QStringLiteral("Action: %1\n\nTranscript:\n%2").arg(action, transcriptText);
        } else {
            userPrompt = QStringLiteral("Action: %1\n\nPayload:\n%2")
                             .arg(action,
                                  QString::fromUtf8(
                                      QJsonDocument(payload).toJson(QJsonDocument::Indented)));
        }

        QJsonArray messages;
        messages.push_back(QJsonObject{
            {QStringLiteral("role"), QStringLiteral("system")},
            {QStringLiteral("content"), instructions},
        });
        messages.push_back(QJsonObject{
            {QStringLiteral("role"), QStringLiteral("user")},
            {QStringLiteral("content"), userPrompt},
        });

        QJsonObject deepseekReq{
            {QStringLiteral("model"), model},
            {QStringLiteral("messages"), messages},
            {QStringLiteral("temperature"), 0.2},
        };
        const auto deepseekResp =
            postJson(QStringLiteral("/functions/v1/deepseek_chat"), deepseekReq, accessToken);
        if (!deepseekResp.hasValue()) {
            return Result<QJsonObject>::fail(deepseekResp.error());
        }

        const QJsonObject outer = deepseekResp.value();
        const QJsonObject upstream = outer.value(QStringLiteral("response")).toObject();
        const QJsonArray choices = upstream.value(QStringLiteral("choices")).toArray();
        QString content;
        if (!choices.isEmpty()) {
            content = choices.first().toObject().value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
        }

        QJsonObject normalized;
        normalized.insert(QStringLiteral("provider"), QStringLiteral("deepseek"));
        normalized.insert(QStringLiteral("message"), content);
        normalized.insert(QStringLiteral("text"), content);
        normalized.insert(QStringLiteral("response"), upstream);
        normalized.insert(QStringLiteral("raw"), outer);
        return Result<QJsonObject>::ok(normalized);
    }

    return postJson(config_.endpoints.aiTaskPath, body, accessToken);
}

Result<CheckoutSession> MonetizeClient::createCheckoutForSlug(const QString& accessToken,
                                                              const QString& slug,
                                                              const QString& successUrl,
                                                              const QString& cancelUrl) const
{
    const QString encodedSlug = QString::fromUtf8(QUrl::toPercentEncoding(slug));
    QString path = config_.endpoints.checkoutBySlugPathTemplate;
    path = path.arg(encodedSlug);

    QJsonObject payload;
    if (!successUrl.trimmed().isEmpty()) {
        payload.insert(QStringLiteral("success_url"), successUrl.trimmed());
    }
    if (!cancelUrl.trimmed().isEmpty()) {
        payload.insert(QStringLiteral("cancel_url"), cancelUrl.trimmed());
    }

    const auto resp = postJson(path, payload, accessToken);
    if (!resp.hasValue()) {
        return Result<CheckoutSession>::fail(resp.error());
    }

    ApiError parseError;
    auto parsed = parseCheckoutSession(resp.value(), &parseError);
    if (!parsed) {
        return Result<CheckoutSession>::fail(parseError);
    }
    return Result<CheckoutSession>::ok(*parsed);
}

Result<DownloadGrant> MonetizeClient::getDownloadForSlug(const QString& accessToken, const QString& slug) const
{
    QString path = config_.endpoints.downloadBySlugPathTemplate;
    path = path.arg(QString::fromUtf8(QUrl::toPercentEncoding(slug)));

    const auto resp = getJson(path, accessToken);
    if (!resp.hasValue()) {
        return Result<DownloadGrant>::fail(resp.error());
    }

    ApiError parseError;
    auto parsed = parseDownloadGrant(resp.value(), &parseError);
    if (!parsed) {
        return Result<DownloadGrant>::fail(parseError);
    }
    return Result<DownloadGrant>::ok(*parsed);
}

Result<AiUsageStatus> MonetizeClient::getAiUsageStatus(const QString& accessToken) const
{
    const auto resp = getJson(config_.endpoints.aiUsagePath, accessToken);
    if (!resp.hasValue()) {
        return Result<AiUsageStatus>::fail(resp.error());
    }

    ApiError parseError;
    auto parsed = parseAiUsageStatus(resp.value(), &parseError);
    if (!parsed) {
        return Result<AiUsageStatus>::fail(parseError);
    }
    return Result<AiUsageStatus>::ok(*parsed);
}

Result<AiUsageStatus> MonetizeClient::consumeAiUnits(const QString& accessToken, int units) const
{
    const auto resp = postJson(config_.endpoints.aiUsagePath,
                               QJsonObject{{QStringLiteral("units"), units}},
                               accessToken);
    if (!resp.hasValue()) {
        return Result<AiUsageStatus>::fail(resp.error());
    }

    ApiError parseError;
    auto parsed = parseAiUsageStatus(resp.value(), &parseError);
    if (!parsed) {
        return Result<AiUsageStatus>::fail(parseError);
    }
    return Result<AiUsageStatus>::ok(*parsed);
}

Result<QJsonObject> MonetizeClient::getLicense(const QString& accessToken) const
{
    return getJson(config_.endpoints.licensePath, accessToken);
}

Result<bool> MonetizeClient::verifyLicenseKey(const QString& licenseKey) const
{
    const QString key = licenseKey.trimmed();
    if (key.isEmpty()) {
        return Result<bool>::fail(makeError(400, QStringLiteral("license key is required")));
    }

    QUrl url(normalizeBaseUrl() + normalizePath(config_.endpoints.licenseVerifyPath));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), key);
    url.setQuery(query);

    const auto resp = performJsonRequest(
        QStringLiteral("GET"), url, QJsonObject{}, {}, config_, config_.endpoints.licenseVerifyPath);
    if (!resp.hasValue()) {
        return Result<bool>::fail(resp.error());
    }
    return Result<bool>::ok(true);
}

Result<QJsonObject> MonetizeClient::getJson(const QString& path, const QString& bearerToken) const
{
    const QUrl url(normalizeBaseUrl() + normalizePath(path));
    return performJsonRequest(QStringLiteral("GET"), url, QJsonObject{}, bearerToken, config_, path);
}

Result<QJsonObject> MonetizeClient::postJson(const QString& path,
                                             const QJsonObject& payload,
                                             const QString& bearerToken) const
{
    const QUrl url(normalizeBaseUrl() + normalizePath(path));
    return performJsonRequest(QStringLiteral("POST"), url, payload, bearerToken, config_, path);
}

QString MonetizeClient::normalizeBaseUrl() const
{
    QString base = config_.apiBaseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return base;
}

}  // namespace cppmonetize
