#include "cppmonetize/OAuthDesktopFlow.h"

#include <QDesktopServices>
#include <QEventLoop>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

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

Result<QJsonObject> getJson(const QString& url, int timeoutMs)
{
    QNetworkAccessManager network;
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = network.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    const bool timedOut = (reply->error() == QNetworkReply::OperationCanceledError && timer.isActive() == false);
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};

    if (timedOut) {
        reply->deleteLater();
        return Result<QJsonObject>::fail(makeError(0, QStringLiteral("Request timed out"), url));
    }
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return Result<QJsonObject>::fail(makeError(statusCode, reply->errorString(), QString::fromUtf8(raw)));
    }

    reply->deleteLater();
    return Result<QJsonObject>::ok(obj);
}

QString normalizeBaseUrl(QString value)
{
    value = value.trimmed();
    while (value.endsWith(QLatin1Char('/'))) {
        value.chop(1);
    }
    return value;
}

bool isSupabaseOrigin(const QString& baseUrl)
{
    return baseUrl.contains(QStringLiteral(".supabase.co"), Qt::CaseInsensitive);
}

QString readBuiltInSupabaseUrl()
{
#ifdef CPPMONETIZE_DEFAULT_SUPABASE_URL
    return QStringLiteral(CPPMONETIZE_DEFAULT_SUPABASE_URL).trimmed();
#else
    return {};
#endif
}

QString readBuiltInSupabaseAnonKey()
{
#ifdef CPPMONETIZE_DEFAULT_SUPABASE_ANON_KEY
    return QStringLiteral(CPPMONETIZE_DEFAULT_SUPABASE_ANON_KEY).trimmed();
#else
    return {};
#endif
}

QString readSupabaseAnonKeyFromEnv()
{
    QString key = qEnvironmentVariable("SUPABASE_ANON_KEY").trimmed();
    if (key.isEmpty()) {
        key = qEnvironmentVariable("SB_ANON_KEY").trimmed();
    }
    if (!key.isEmpty()) {
        return key;
    }

    auto parseEnvFile = [](const QString& path) -> QString {
        QFile file(path);
        if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            if (line.startsWith(QStringLiteral("export "))) {
                line = line.mid(7).trimmed();
            }
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq <= 0) {
                continue;
            }
            const QString name = line.left(eq).trimmed();
            QString value = line.mid(eq + 1).trimmed();
            if (name != QStringLiteral("SUPABASE_ANON_KEY") &&
                name != QStringLiteral("SB_ANON_KEY")) {
                continue;
            }
            if ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) ||
                (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))) {
                value = value.mid(1, value.size() - 2);
            }
            return value.trimmed();
        }
        return {};
    };

    QStringList candidates;
    const QString cwd = QDir::currentPath();
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!cwd.isEmpty()) {
        candidates.push_back(QDir(cwd).filePath(QStringLiteral(".env.supabase")));
        candidates.push_back(QDir(cwd).filePath(QStringLiteral("../.env.supabase")));
        candidates.push_back(QDir(cwd).filePath(QStringLiteral("../../.env.supabase")));
    }
    if (!appDir.isEmpty()) {
        candidates.push_back(QDir(appDir).filePath(QStringLiteral(".env.supabase")));
        candidates.push_back(QDir(appDir).filePath(QStringLiteral("../.env.supabase")));
        candidates.push_back(QDir(appDir).filePath(QStringLiteral("../../.env.supabase")));
    }
    candidates.removeDuplicates();
    for (const QString& candidate : candidates) {
        const QString resolved = parseEnvFile(QFileInfo(candidate).absoluteFilePath());
        if (!resolved.isEmpty()) {
            return resolved;
        }
    }
    if (key.isEmpty()) {
        key = readBuiltInSupabaseAnonKey();
    }
    return key;
}

QString readSupabaseUrlFromEnv()
{
    QString value = qEnvironmentVariable("SUPABASE_URL").trimmed();
    if (value.isEmpty()) {
        value = qEnvironmentVariable("SB_URL").trimmed();
    }
    if (!value.isEmpty()) {
        return value;
    }

    auto parseEnvFile = [](const QString& path) -> QString {
        QFile file(path);
        if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            if (line.startsWith(QStringLiteral("export "))) {
                line = line.mid(7).trimmed();
            }
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq <= 0) {
                continue;
            }
            const QString name = line.left(eq).trimmed();
            QString envValue = line.mid(eq + 1).trimmed();
            if (name != QStringLiteral("SUPABASE_URL") &&
                name != QStringLiteral("SB_URL")) {
                continue;
            }
            if ((envValue.startsWith(QLatin1Char('"')) && envValue.endsWith(QLatin1Char('"'))) ||
                (envValue.startsWith(QLatin1Char('\'')) && envValue.endsWith(QLatin1Char('\'')))) {
                envValue = envValue.mid(1, envValue.size() - 2);
            }
            return envValue.trimmed();
        }
        return {};
    };

    QStringList candidates;
    const QString cwd = QDir::currentPath();
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!cwd.isEmpty()) {
        candidates.push_back(QDir(cwd).filePath(QStringLiteral(".env.supabase")));
        candidates.push_back(QDir(cwd).filePath(QStringLiteral("../.env.supabase")));
        candidates.push_back(QDir(cwd).filePath(QStringLiteral("../../.env.supabase")));
    }
    if (!appDir.isEmpty()) {
        candidates.push_back(QDir(appDir).filePath(QStringLiteral(".env.supabase")));
        candidates.push_back(QDir(appDir).filePath(QStringLiteral("../.env.supabase")));
        candidates.push_back(QDir(appDir).filePath(QStringLiteral("../../.env.supabase")));
    }
    candidates.removeDuplicates();
    for (const QString& candidate : candidates) {
        const QString resolved = parseEnvFile(QFileInfo(candidate).absoluteFilePath());
        if (!resolved.isEmpty()) {
            return resolved;
        }
    }
    return {};
}

QString randomPkceToken(int byteCount)
{
    QByteArray data;
    data.resize(byteCount);
    for (int i = 0; i < byteCount; ++i) {
        data[i] = static_cast<char>(QRandomGenerator::global()->bounded(0, 256));
    }
    QString token = QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding |
                                                      QByteArray::OmitTrailingEquals));
    token.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9\\-_.~]")));
    return token;
}

QString base64UrlSha256(const QString& value)
{
    const QByteArray hash = QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toBase64(QByteArray::Base64UrlEncoding |
                                             QByteArray::OmitTrailingEquals));
}

Result<OAuthCallbackResult> exchangeSupabasePkceCode(const OAuthConfig& config,
                                                     const QString& authCode,
                                                     const QString& codeVerifier,
                                                     int timeoutMs)
{
    if (!config.enabled || config.supabaseUrl.trimmed().isEmpty()) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Supabase auth is not configured")));
    }
    if (config.supabaseAnonKey.trimmed().isEmpty()) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Supabase anon key is missing")));
    }
    if (authCode.trimmed().isEmpty() || codeVerifier.trimmed().isEmpty()) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Missing auth code for PKCE exchange")));
    }

    QUrl tokenUrl(config.supabaseUrl + QStringLiteral("/auth/v1/token?grant_type=pkce"));
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("apikey", config.supabaseAnonKey.toUtf8());
    request.setRawHeader("Accept", "application/json");

    const QJsonObject payload{
        {QStringLiteral("auth_code"), authCode.trimmed()},
        {QStringLiteral("code_verifier"), codeVerifier.trimmed()},
    };

    QNetworkAccessManager network;
    QNetworkReply* reply = network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    const bool timedOut =
        (reply->error() == QNetworkReply::OperationCanceledError && timer.isActive() == false);
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};

    if (timedOut) {
        reply->deleteLater();
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Supabase token exchange timed out")));
    }

    QString token = obj.value(QStringLiteral("access_token")).toString().trimmed();
    QString refreshToken = obj.value(QStringLiteral("refresh_token")).toString().trimmed();
    if (token.isEmpty()) {
        token = obj.value(QStringLiteral("session"))
                    .toObject()
                    .value(QStringLiteral("access_token"))
                    .toString()
                    .trimmed();
    }
    if (refreshToken.isEmpty()) {
        refreshToken = obj.value(QStringLiteral("session"))
                           .toObject()
                           .value(QStringLiteral("refresh_token"))
                           .toString()
                           .trimmed();
    }
    QString resolvedEmail = obj.value(QStringLiteral("user"))
                                .toObject()
                                .value(QStringLiteral("email"))
                                .toString()
                                .trimmed();

    if (statusCode >= 400 || reply->error() != QNetworkReply::NoError || token.isEmpty()) {
        QString detail = obj.value(QStringLiteral("error_description")).toString().trimmed();
        if (detail.isEmpty()) {
            detail = obj.value(QStringLiteral("msg")).toString().trimmed();
        }
        if (detail.isEmpty()) {
            detail = obj.value(QStringLiteral("error")).toString().trimmed();
        }
        if (detail.isEmpty()) {
            detail = QString::fromUtf8(raw).trimmed();
        }
        if (detail.isEmpty()) {
            detail = reply->errorString();
        }
        if (detail.isEmpty()) {
            detail = QStringLiteral("Failed to exchange OAuth code.");
        }
        reply->deleteLater();
        return Result<OAuthCallbackResult>::fail(makeError(statusCode, detail));
    }

    reply->deleteLater();
    OAuthCallbackResult result;
    result.token = token;
    result.refreshToken = refreshToken;
    result.email = resolvedEmail;
    return Result<OAuthCallbackResult>::ok(result);
}

}  // namespace

Result<OAuthConfig> OAuthDesktopFlow::resolveSupabaseConfig(const QString& apiBaseUrl, int timeoutMs) const
{
    Q_UNUSED(timeoutMs);

    QString normalizedBase = normalizeBaseUrl(apiBaseUrl);
    if (normalizedBase.isEmpty()) {
        normalizedBase = normalizeBaseUrl(readBuiltInSupabaseUrl());
    }
    if (!isSupabaseOrigin(normalizedBase)) {
        const QString envSupabaseUrl = normalizeBaseUrl(readSupabaseUrlFromEnv());
        if (isSupabaseOrigin(envSupabaseUrl)) {
            normalizedBase = envSupabaseUrl;
        } else if (normalizedBase.isEmpty()) {
            normalizedBase = normalizeBaseUrl(readBuiltInSupabaseUrl());
        }
    }
    if (normalizedBase.isEmpty()) {
        return Result<OAuthConfig>::fail(makeError(0, QStringLiteral("OAuth config base URL is empty")));
    }

    if (!isSupabaseOrigin(normalizedBase)) {
        return Result<OAuthConfig>::fail(
            makeError(0,
                      QStringLiteral("Only Supabase origin URLs are supported for OAuth login. "
                                     "Set SUPABASE_URL (or SB_URL) to your Supabase project URL.")));
    }

    QString supabaseBase = normalizedBase;
    if (supabaseBase.endsWith(QStringLiteral("/api"))) {
        supabaseBase.chop(4);
    }

    OAuthConfig cfg;
    cfg.enabled = true;
    cfg.supabaseUrl = normalizeBaseUrl(supabaseBase);
    cfg.supabaseAnonKey = readSupabaseAnonKeyFromEnv();
    if (cfg.supabaseAnonKey.isEmpty()) {
        return Result<OAuthConfig>::fail(
            makeError(0, QStringLiteral("Missing Supabase anon key in SUPABASE_ANON_KEY or SB_ANON_KEY")));
    }
    return Result<OAuthConfig>::ok(cfg);
}

Result<OAuthConfig> OAuthDesktopFlow::fetchOAuthConfig(const QString& apiBaseUrl, int timeoutMs) const
{
    const QString base = apiBaseUrl.endsWith(QLatin1Char('/')) ? apiBaseUrl.left(apiBaseUrl.size() - 1) : apiBaseUrl;
    const auto resp = getJson(base + QStringLiteral("/auth/supabase-config"), timeoutMs);
    if (!resp.hasValue()) {
        return Result<OAuthConfig>::fail(resp.error());
    }

    OAuthConfig cfg;
    cfg.enabled = resp.value().value(QStringLiteral("enabled")).toBool(false);
    cfg.supabaseUrl = resp.value().value(QStringLiteral("supabase_url")).toString().trimmed();
    cfg.supabaseAnonKey = resp.value().value(QStringLiteral("supabase_anon_key")).toString().trimmed();
    cfg.desktopRedirectBase = resp.value().value(QStringLiteral("desktop_redirect_base")).toString().trimmed();

    if (cfg.enabled && (cfg.supabaseUrl.isEmpty() || cfg.desktopRedirectBase.isEmpty())) {
        return Result<OAuthConfig>::fail(makeError(0, QStringLiteral("OAuth config is enabled but incomplete")));
    }
    return Result<OAuthConfig>::ok(cfg);
}

QString OAuthDesktopFlow::buildBrowserOAuthUrl(const QString& apiBaseUrl,
                                               const QString& provider,
                                               quint16 callbackPort,
                                               const OAuthConfig& config) const
{
    if (config.enabled && !config.supabaseUrl.isEmpty() && !config.desktopRedirectBase.isEmpty()) {
        QUrl redirectTo(config.desktopRedirectBase);
        QUrlQuery redirectQuery(redirectTo.query());
        redirectQuery.addQueryItem(QStringLiteral("port"), QString::number(callbackPort));
        redirectQuery.addQueryItem(QStringLiteral("provider"), provider);
        redirectTo.setQuery(redirectQuery);

        QUrl authUrl(config.supabaseUrl + QStringLiteral("/auth/v1/authorize"));
        QUrlQuery authQuery;
        authQuery.addQueryItem(QStringLiteral("provider"), provider);
        authQuery.addQueryItem(QStringLiteral("redirect_to"), redirectTo.toString());
        authUrl.setQuery(authQuery);
        return authUrl.toString();
    }

    const QString base = apiBaseUrl.endsWith(QLatin1Char('/')) ? apiBaseUrl.left(apiBaseUrl.size() - 1) : apiBaseUrl;
    return base + QStringLiteral("/oauth/") + provider +
           QStringLiteral("?redirect=http://localhost:%1/callback").arg(callbackPort);
}

QString OAuthDesktopFlow::buildSupabaseAuthorizeUrl(const OAuthConfig& config,
                                                    const QString& provider,
                                                    const QString& redirectTo,
                                                    bool includePkceFlowParams) const
{
    const QString supabaseUrl = normalizeBaseUrl(config.supabaseUrl);
    if (supabaseUrl.isEmpty()) {
        return QString();
    }
    const QString resolvedProvider = provider.trimmed().isEmpty()
        ? QStringLiteral("google")
        : provider.trimmed().toLower();
    const QString resolvedRedirect = redirectTo.trimmed().isEmpty()
        ? QStringLiteral("http://127.0.0.1/callback")
        : redirectTo.trimmed();

    QUrl authUrl(supabaseUrl + QStringLiteral("/auth/v1/authorize"));
    QUrlQuery authQuery;
    authQuery.addQueryItem(QStringLiteral("provider"), resolvedProvider);
    authQuery.addQueryItem(QStringLiteral("redirect_to"), resolvedRedirect);
    if (includePkceFlowParams) {
        authQuery.addQueryItem(QStringLiteral("flow_type"), QStringLiteral("pkce"));
        authQuery.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    }
    authUrl.setQuery(authQuery);
    return authUrl.toString();
}

Result<OAuthCallbackResult> OAuthDesktopFlow::signInWithBrowserPkce(const OAuthConfig& config,
                                                                    const QString& provider,
                                                                    int timeoutMs,
                                                                    bool openBrowser,
                                                                    const std::function<void(const QString&)>& authUrlReady) const
{
    if (!config.enabled || config.supabaseUrl.trimmed().isEmpty()) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Supabase auth is not configured")));
    }
    if (config.supabaseAnonKey.trimmed().isEmpty()) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Supabase anon key is missing")));
    }

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Unable to open localhost callback port")));
    }

    const QString resolvedProvider = provider.trimmed().isEmpty()
        ? QStringLiteral("google")
        : provider.trimmed().toLower();
    const QString codeVerifier = randomPkceToken(48);
    const QString codeChallenge = base64UrlSha256(codeVerifier);

    const QString redirectTo = QStringLiteral("http://127.0.0.1:%1/callback").arg(server.serverPort());
    QUrl authUrl(config.supabaseUrl + QStringLiteral("/auth/v1/authorize"));
    QUrlQuery authQuery;
    authQuery.addQueryItem(QStringLiteral("provider"), resolvedProvider);
    authQuery.addQueryItem(QStringLiteral("redirect_to"), redirectTo);
    authQuery.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
    authQuery.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    authQuery.addQueryItem(QStringLiteral("flow_type"), QStringLiteral("pkce"));
    authQuery.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    authUrl.setQuery(authQuery);

    if (authUrlReady) {
        authUrlReady(authUrl.toString());
    }
    if (openBrowser && !QDesktopServices::openUrl(authUrl)) {
        return Result<OAuthCallbackResult>::fail(
            makeError(0, QStringLiteral("Unable to open browser URL"), authUrl.toString()));
    }

    QString token;
    QString email;
    QString callbackError;
    bool callbackReceived = false;
    bool timedOut = false;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        loop.quit();
    });
    timer.start(timeoutMs);

    QObject::connect(&server, &QTcpServer::newConnection, &loop, [&]() {
        QTcpSocket* sock = server.nextPendingConnection();
        if (!sock) {
            return;
        }

        QObject::connect(sock, &QTcpSocket::readyRead, sock, [&, sock]() {
            const QString request = QString::fromUtf8(sock->readAll());
            const QString firstLine = request.section('\n', 0, 0).trimmed();
            const QString path = firstLine.section(' ', 1, 1);
            const QUrl url(QStringLiteral("http://localhost") + path);
            const QUrlQuery query(url.query());

            const QString reqError = query.queryItemValue(QStringLiteral("error")).trimmed();
            if (!reqError.isEmpty()) {
                callbackError = query.queryItemValue(QStringLiteral("error_description")).trimmed();
                if (callbackError.isEmpty()) {
                    callbackError = reqError;
                }
            } else {
                const QString authCode = query.queryItemValue(QStringLiteral("code")).trimmed();
                const auto exchangeResult = exchangeSupabasePkceCode(config, authCode, codeVerifier, timeoutMs);
                if (!exchangeResult.hasValue()) {
                    callbackError = exchangeResult.error().message;
                } else {
                    token = exchangeResult.value().token;
                    email = exchangeResult.value().email;
                }
            }

            const QByteArray body = token.isEmpty()
                ? QByteArray(
                      "<!doctype html><html><head><meta charset=\"utf-8\">"
                      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                      "<title>Sign-in Status</title>"
                      "<style>"
                      ":root{color-scheme:light dark;}"
                      "body{margin:0;font-family:Inter,Segoe UI,Arial,sans-serif;"
                      "background:radial-gradient(circle at 20% 0%,#2b3f68 0%,#101828 45%,#0b1220 100%);"
                      "min-height:100vh;display:grid;place-items:center;color:#e5ecff;}"
                      ".card{width:min(560px,92vw);background:rgba(17,24,39,.88);"
                      "border:1px solid rgba(148,163,184,.25);border-radius:16px;padding:24px;"
                      "box-shadow:0 18px 40px rgba(0,0,0,.35);}"
                      ".badge{display:inline-block;padding:4px 10px;border-radius:999px;"
                      "font-size:12px;font-weight:700;letter-spacing:.04em;text-transform:uppercase;"
                      "background:#7f1d1d;color:#fecaca;border:1px solid #ef4444;}"
                      "h1{margin:14px 0 10px;font-size:22px;line-height:1.2;}"
                      "p{margin:0;color:#cbd5e1;line-height:1.55;}"
                      ".hint{margin-top:14px;font-size:13px;color:#94a3b8;}"
                      "</style></head><body><main class=\"card\">"
                      "<span class=\"badge\">Sign-in Failed</span>"
                      "<h1>Sign-in did not complete</h1>"
                      "<p>Return to the app and try again. This tab can be closed.</p>"
                      "<p class=\"hint\">If this keeps happening, restart the login flow from the app.</p>"
                      "</main></body></html>")
                : QByteArray(
                      "<!doctype html><html><head><meta charset=\"utf-8\">"
                      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                      "<title>Sign-in Complete</title>"
                      "<style>"
                      ":root{color-scheme:light dark;}"
                      "body{margin:0;font-family:Inter,Segoe UI,Arial,sans-serif;"
                      "background:radial-gradient(circle at 20% 0%,#24534a 0%,#0f2e31 45%,#0b1220 100%);"
                      "min-height:100vh;display:grid;place-items:center;color:#e5ecff;}"
                      ".card{width:min(560px,92vw);background:rgba(17,24,39,.88);"
                      "border:1px solid rgba(148,163,184,.25);border-radius:16px;padding:24px;"
                      "box-shadow:0 18px 40px rgba(0,0,0,.35);}"
                      ".badge{display:inline-block;padding:4px 10px;border-radius:999px;"
                      "font-size:12px;font-weight:700;letter-spacing:.04em;text-transform:uppercase;"
                      "background:#14532d;color:#bbf7d0;border:1px solid #22c55e;}"
                      "h1{margin:14px 0 10px;font-size:22px;line-height:1.2;}"
                      "p{margin:0;color:#cbd5e1;line-height:1.55;}"
                      ".hint{margin-top:14px;font-size:13px;color:#94a3b8;}"
                      "</style></head><body><main class=\"card\">"
                      "<span class=\"badge\">Success</span>"
                      "<h1>Sign-in complete</h1>"
                      "<p>You are now signed in. Return to the app to continue.</p>"
                      "<p class=\"hint\">This tab can be closed.</p>"
                      "</main></body></html>");
            QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
            response += QByteArray::number(body.size());
            response += "\r\n\r\n";
            response += body;
            sock->write(response);
            sock->flush();
            sock->disconnectFromHost();
            sock->deleteLater();

            callbackReceived = true;
            loop.quit();
        });
    });

    loop.exec();
    server.close();

    if (timedOut) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("OAuth callback timed out")));
    }
    if (!callbackReceived || token.isEmpty()) {
        return Result<OAuthCallbackResult>::fail(
            makeError(0, callbackError.isEmpty() ? QStringLiteral("OAuth callback missing token") : callbackError));
    }

    OAuthCallbackResult result;
    result.token = token;
    result.email = email;
    return Result<OAuthCallbackResult>::ok(result);
}

Result<OAuthCallbackResult> OAuthDesktopFlow::signInWithBrowser(const QString& oauthUrl, int timeoutMs) const
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Unable to open localhost callback port")));
    }

    if (!QDesktopServices::openUrl(QUrl(oauthUrl))) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Unable to open browser URL"), oauthUrl));
    }

    QString token;
    QString email;
    bool callbackReceived = false;
    bool timedOut = false;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        loop.quit();
    });
    timer.start(timeoutMs);

    QObject::connect(&server, &QTcpServer::newConnection, &loop, [&]() {
        QTcpSocket* sock = server.nextPendingConnection();
        if (!sock) {
            return;
        }

        QObject::connect(sock, &QTcpSocket::readyRead, sock, [&, sock]() {
            const QString request = QString::fromUtf8(sock->readAll());
            const QString firstLine = request.section('\n', 0, 0).trimmed();
            const QString path = firstLine.section(' ', 1, 1);
            const QUrl url(QStringLiteral("http://localhost") + path);
            const QUrlQuery query(url.query());

            token = query.queryItemValue(QStringLiteral("token")).trimmed();
            if (token.isEmpty()) {
                token = query.queryItemValue(QStringLiteral("access_token")).trimmed();
            }
            email = query.queryItemValue(QStringLiteral("email")).trimmed();

            if (token.isEmpty() && path.contains(QLatin1Char('#'))) {
                const QString fragment = path.section(QLatin1Char('#'), 1);
                const QUrlQuery fragmentQuery(fragment);
                token = fragmentQuery.queryItemValue(QStringLiteral("token")).trimmed();
                if (token.isEmpty()) {
                    token = fragmentQuery.queryItemValue(QStringLiteral("access_token")).trimmed();
                }
                email = fragmentQuery.queryItemValue(QStringLiteral("email")).trimmed();
            }

            const QByteArray body = token.isEmpty()
                ? QByteArray("<html><body>Sign-in did not complete. You can close this tab.</body></html>")
                : QByteArray("<html><body>Sign-in complete. You can close this tab.</body></html>");
            QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
            response += QByteArray::number(body.size());
            response += "\r\n\r\n";
            response += body;
            sock->write(response);
            sock->flush();
            sock->disconnectFromHost();
            sock->deleteLater();

            callbackReceived = true;
            loop.quit();
        });
    });

    loop.exec();
    server.close();

    if (timedOut) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("OAuth callback timed out")));
    }
    if (!callbackReceived || token.isEmpty()) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("OAuth callback missing token")));
    }

    OAuthCallbackResult result;
    result.token = token;
    result.email = email;
    return Result<OAuthCallbackResult>::ok(result);
}

Result<PasswordAuthResult> OAuthDesktopFlow::signInWithPassword(const OAuthConfig& config,
                                                                const QString& email,
                                                                const QString& password,
                                                                bool registerMode,
                                                                int timeoutMs) const
{
    if (!config.enabled || config.supabaseUrl.trimmed().isEmpty()) {
        return Result<PasswordAuthResult>::fail(makeError(0, QStringLiteral("Supabase auth is not configured")));
    }
    if (config.supabaseAnonKey.trimmed().isEmpty()) {
        return Result<PasswordAuthResult>::fail(makeError(0, QStringLiteral("Supabase anon key is missing")));
    }

    const QString endpoint = registerMode
        ? QStringLiteral("/auth/v1/signup")
        : QStringLiteral("/auth/v1/token?grant_type=password");
    const QUrl url(config.supabaseUrl + endpoint);

    QNetworkAccessManager network;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("apikey", config.supabaseAnonKey.toUtf8());
    request.setRawHeader("Accept", "application/json");

    const QJsonObject payload{
        {QStringLiteral("email"), email.trimmed()},
        {QStringLiteral("password"), password},
    };
    QNetworkReply* reply = network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    const bool timedOut =
        (reply->error() == QNetworkReply::OperationCanceledError && timer.isActive() == false);
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};

    const auto firstString = [&](std::initializer_list<const char*> keys) -> QString {
        for (const char* key : keys) {
            const QString value = obj.value(QLatin1String(key)).toString().trimmed();
            if (!value.isEmpty()) {
                return value;
            }
        }
        return {};
    };

    if (timedOut) {
        reply->deleteLater();
        return Result<PasswordAuthResult>::fail(makeError(0, QStringLiteral("Supabase auth request timed out")));
    }

    QString token = firstString({"access_token"});
    QString refreshToken = firstString({"refresh_token"});
    if (token.isEmpty()) {
        token = obj.value(QStringLiteral("session"))
                    .toObject()
                    .value(QStringLiteral("access_token"))
                    .toString()
                    .trimmed();
    }
    if (refreshToken.isEmpty()) {
        refreshToken = obj.value(QStringLiteral("session"))
                           .toObject()
                           .value(QStringLiteral("refresh_token"))
                           .toString()
                           .trimmed();
    }
    QString resolvedEmail = obj.value(QStringLiteral("user"))
                                .toObject()
                                .value(QStringLiteral("email"))
                                .toString()
                                .trimmed();
    if (resolvedEmail.isEmpty()) {
        resolvedEmail = email.trimmed();
    }

    if (statusCode >= 400 || reply->error() != QNetworkReply::NoError) {
        QString detail = firstString({"error_description", "msg", "error"});
        if (detail.isEmpty()) {
            detail = QString::fromUtf8(raw).trimmed();
        }
        if (detail.isEmpty()) {
            detail = reply->errorString();
        }
        const QString errorCode = obj.value(QStringLiteral("error_code")).toString().trimmed();
        if (errorCode == QStringLiteral("invalid_credentials")) {
            detail = QStringLiteral("Invalid email or password.");
        }
        reply->deleteLater();
        return Result<PasswordAuthResult>::fail(makeError(statusCode, detail));
    }

    if (token.isEmpty()) {
        reply->deleteLater();
        if (registerMode) {
            return Result<PasswordAuthResult>::fail(
                makeError(statusCode, QStringLiteral("Check your email to confirm your account, then sign in.")));
        }
        return Result<PasswordAuthResult>::fail(makeError(statusCode, QStringLiteral("Missing access token")));
    }

    reply->deleteLater();
    PasswordAuthResult result;
    result.token = token;
    result.refreshToken = refreshToken;
    result.email = resolvedEmail;
    return Result<PasswordAuthResult>::ok(result);
}

Result<OAuthCallbackResult> OAuthDesktopFlow::refreshWithToken(const OAuthConfig& config,
                                                               const QString& refreshToken,
                                                               int timeoutMs) const
{
    if (!config.enabled || config.supabaseUrl.trimmed().isEmpty()) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Supabase auth is not configured")));
    }
    if (config.supabaseAnonKey.trimmed().isEmpty()) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Supabase anon key is missing")));
    }
    if (refreshToken.trimmed().isEmpty()) {
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Refresh token is missing")));
    }

    const QUrl url(config.supabaseUrl + QStringLiteral("/auth/v1/token?grant_type=refresh_token"));
    QNetworkAccessManager network;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("apikey", config.supabaseAnonKey.toUtf8());
    request.setRawHeader("Accept", "application/json");

    const QJsonObject payload{
        {QStringLiteral("refresh_token"), refreshToken.trimmed()},
    };
    QNetworkReply* reply = network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray raw = reply->readAll();
    const bool timedOut =
        (reply->error() == QNetworkReply::OperationCanceledError && timer.isActive() == false);
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};

    if (timedOut) {
        reply->deleteLater();
        return Result<OAuthCallbackResult>::fail(makeError(0, QStringLiteral("Supabase token refresh timed out")));
    }

    QString accessToken = obj.value(QStringLiteral("access_token")).toString().trimmed();
    QString refreshedToken = obj.value(QStringLiteral("refresh_token")).toString().trimmed();
    if (accessToken.isEmpty()) {
        accessToken = obj.value(QStringLiteral("session"))
                          .toObject()
                          .value(QStringLiteral("access_token"))
                          .toString()
                          .trimmed();
    }
    if (refreshedToken.isEmpty()) {
        refreshedToken = obj.value(QStringLiteral("session"))
                             .toObject()
                             .value(QStringLiteral("refresh_token"))
                             .toString()
                             .trimmed();
    }
    const QString email = obj.value(QStringLiteral("user"))
                              .toObject()
                              .value(QStringLiteral("email"))
                              .toString()
                              .trimmed();

    if (statusCode >= 400 || reply->error() != QNetworkReply::NoError || accessToken.isEmpty()) {
        QString detail = obj.value(QStringLiteral("error_description")).toString().trimmed();
        if (detail.isEmpty()) {
            detail = obj.value(QStringLiteral("msg")).toString().trimmed();
        }
        if (detail.isEmpty()) {
            detail = obj.value(QStringLiteral("error")).toString().trimmed();
        }
        if (detail.isEmpty()) {
            detail = QString::fromUtf8(raw).trimmed();
        }
        if (detail.isEmpty()) {
            detail = reply->errorString();
        }
        reply->deleteLater();
        return Result<OAuthCallbackResult>::fail(makeError(statusCode, detail.isEmpty() ? QStringLiteral("Failed to refresh OAuth token.") : detail));
    }

    reply->deleteLater();
    OAuthCallbackResult result;
    result.token = accessToken;
    result.refreshToken = refreshedToken.isEmpty() ? refreshToken.trimmed() : refreshedToken;
    result.email = email;
    return Result<OAuthCallbackResult>::ok(result);
}

}  // namespace cppmonetize
