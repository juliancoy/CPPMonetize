#include "cppmonetize/TokenStore.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

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

class FileTokenStore final : public TokenStore {
public:
    explicit FileTokenStore(TokenStoreConfig cfg)
        : cfg_(std::move(cfg))
    {
    }

    Result<bool> storeToken(const QString& token, const QString& userId) override
    {
        if (!writeLine(tokenPath(), token)) {
            return Result<bool>::fail(makeError(0, QStringLiteral("Failed to write token file"), tokenPath()));
        }
        if (!writeLine(userPath(), userId)) {
            return Result<bool>::fail(makeError(0, QStringLiteral("Failed to write user file"), userPath()));
        }
        return Result<bool>::ok(true);
    }

    Result<QString> loadToken() override
    {
        return Result<QString>::ok(readLine(tokenPath()));
    }

    Result<QString> loadUserId() override
    {
        return Result<QString>::ok(readLine(userPath()));
    }

    Result<bool> clear() override
    {
        QFile::remove(tokenPath());
        QFile::remove(userPath());
        return Result<bool>::ok(true);
    }

private:
    QString basePath() const
    {
        QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        if (path.trimmed().isEmpty()) {
            path = QDir::homePath() + QStringLiteral("/.config/") + cfg_.appName;
        }
        QDir dir(path);
        dir.mkpath(QStringLiteral("."));
        return dir.absolutePath();
    }

    QString tokenPath() const
    {
        return basePath() + QStringLiteral("/auth_token.txt");
    }

    QString userPath() const
    {
        return basePath() + QStringLiteral("/auth_user_id.txt");
    }

    static bool writeLine(const QString& path, const QString& value)
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return false;
        }
        QTextStream out(&f);
        out << value << '\n';
        return true;
    }

    static QString readLine(const QString& path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        QTextStream in(&f);
        return in.readLine().trimmed();
    }

    TokenStoreConfig cfg_;
};

#ifdef Q_OS_LINUX
class LinuxSecretToolTokenStore final : public TokenStore {
public:
    explicit LinuxSecretToolTokenStore(TokenStoreConfig cfg)
        : cfg_(std::move(cfg))
    {
    }

    static bool isAvailable()
    {
        QProcess p;
        p.start(QStringLiteral("which"), QStringList{QStringLiteral("secret-tool")});
        p.waitForFinished(1500);
        return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    }

    Result<bool> storeToken(const QString& token, const QString& userId) override
    {
        const auto tokenRes = writeSecret(QStringLiteral("token"), token);
        if (!tokenRes.hasValue()) {
            return tokenRes;
        }
        const auto userRes = writeSecret(QStringLiteral("user_id"), userId);
        if (!userRes.hasValue()) {
            return userRes;
        }
        return Result<bool>::ok(true);
    }

    Result<QString> loadToken() override
    {
        return readSecret(QStringLiteral("token"));
    }

    Result<QString> loadUserId() override
    {
        return readSecret(QStringLiteral("user_id"));
    }

    Result<bool> clear() override
    {
        const auto t = clearSecret(QStringLiteral("token"));
        if (!t.hasValue()) {
            return t;
        }
        const auto u = clearSecret(QStringLiteral("user_id"));
        if (!u.hasValue()) {
            return u;
        }
        return Result<bool>::ok(true);
    }

private:
    Result<bool> writeSecret(const QString& field, const QString& value)
    {
        QProcess p;
        p.start(QStringLiteral("secret-tool"),
                QStringList{QStringLiteral("store"),
                            QStringLiteral("--label=%1 %2").arg(cfg_.serviceName, field),
                            QStringLiteral("service"), cfg_.serviceName,
                            QStringLiteral("field"), field});
        if (!p.waitForStarted(3000)) {
            return Result<bool>::fail(makeError(0, QStringLiteral("Failed to start secret-tool")));
        }
        p.write(value.toUtf8());
        p.closeWriteChannel();
        if (!p.waitForFinished(5000)) {
            p.kill();
            return Result<bool>::fail(makeError(0, QStringLiteral("secret-tool store timed out")));
        }
        if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
            return Result<bool>::fail(makeError(0, QStringLiteral("secret-tool store failed"), QString::fromUtf8(p.readAllStandardError())));
        }
        return Result<bool>::ok(true);
    }

    Result<QString> readSecret(const QString& field)
    {
        QProcess p;
        p.start(QStringLiteral("secret-tool"),
                QStringList{QStringLiteral("lookup"),
                            QStringLiteral("service"), cfg_.serviceName,
                            QStringLiteral("field"), field});
        if (!p.waitForFinished(5000)) {
            p.kill();
            return Result<QString>::fail(makeError(0, QStringLiteral("secret-tool lookup timed out")));
        }
        if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
            return Result<QString>::ok(QString{});
        }
        return Result<QString>::ok(QString::fromUtf8(p.readAllStandardOutput()).trimmed());
    }

    Result<bool> clearSecret(const QString& field)
    {
        QProcess p;
        p.start(QStringLiteral("secret-tool"),
                QStringList{QStringLiteral("clear"),
                            QStringLiteral("service"), cfg_.serviceName,
                            QStringLiteral("field"), field});
        if (!p.waitForFinished(5000)) {
            p.kill();
            return Result<bool>::fail(makeError(0, QStringLiteral("secret-tool clear timed out")));
        }
        return Result<bool>::ok(true);
    }

    TokenStoreConfig cfg_;
};
#endif

}  // namespace

std::unique_ptr<TokenStore> createDefaultTokenStore(const TokenStoreConfig& cfg)
{
#ifdef Q_OS_LINUX
    if (LinuxSecretToolTokenStore::isAvailable()) {
        return std::make_unique<LinuxSecretToolTokenStore>(cfg);
    }
#endif
    return std::make_unique<FileTokenStore>(cfg);
}

}  // namespace cppmonetize
