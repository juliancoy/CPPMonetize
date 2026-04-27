#include "cppmonetize/OAuthDesktopFlow.h"

#include <QDesktopServices>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
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

}  // namespace

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
    if (token.isEmpty()) {
        token = obj.value(QStringLiteral("session"))
                    .toObject()
                    .value(QStringLiteral("access_token"))
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
    result.email = resolvedEmail;
    return Result<PasswordAuthResult>::ok(result);
}

}  // namespace cppmonetize
