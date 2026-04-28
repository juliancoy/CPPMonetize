#include "cppmonetize/ApiErrorUtils.h"

namespace cppmonetize {

bool isAuthTokenFailure(const ApiError& error)
{
    const QString message = error.message.trimmed().toLower();
    const QString details = error.details.trimmed().toLower();
    const QString code = error.code.trimmed().toLower();
    const bool authHint = message.contains(QStringLiteral("invalid jwt")) ||
                          message.contains(QStringLiteral("unauthorized")) ||
                          message.contains(QStringLiteral("token expired")) ||
                          message.contains(QStringLiteral("jwt")) ||
                          details.contains(QStringLiteral("invalid jwt")) ||
                          details.contains(QStringLiteral("unauthorized")) ||
                          details.contains(QStringLiteral("token expired")) ||
                          details.contains(QStringLiteral("jwt")) ||
                          code == QStringLiteral("invalid_jwt") ||
                          code == QStringLiteral("unauthorized");
    if (authHint) {
        return true;
    }
    if ((error.statusCode == 401 || error.statusCode == 403) &&
        message.isEmpty() &&
        details.isEmpty() &&
        code.isEmpty()) {
        return true;
    }
    return false;
}

bool isSubscriptionRequiredError(const ApiError& error)
{
    if (error.statusCode == 402) {
        return true;
    }
    const QString combined =
        (error.message + QLatin1Char(' ') + error.details).trimmed().toLower();
    return combined.contains(QStringLiteral("subscription required")) ||
           combined.contains(QStringLiteral("requires_subscription")) ||
           combined.contains(QStringLiteral("payment required"));
}

bool isBackendSchemaMisconfigured(const ApiError& error)
{
    const QString combined =
        (error.message + QLatin1Char(' ') + error.details).trimmed().toLower();
    return combined.contains(QStringLiteral("schema cache")) ||
           combined.contains(QStringLiteral("could not find the table")) ||
           combined.contains(QStringLiteral("could not find the function")) ||
           combined.contains(QStringLiteral("relation")) && combined.contains(QStringLiteral("does not exist")) ||
           combined.contains(QStringLiteral("undefined function"));
}

}  // namespace cppmonetize

