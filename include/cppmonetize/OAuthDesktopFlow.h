#pragma once

#include "cppmonetize/Result.h"

#include <QString>

namespace cppmonetize {

struct OAuthConfig {
    bool enabled = false;
    QString supabaseUrl;
    QString desktopRedirectBase;
};

struct OAuthCallbackResult {
    QString token;
    QString email;
};

class OAuthDesktopFlow {
public:
    OAuthDesktopFlow() = default;

    Result<OAuthConfig> fetchOAuthConfig(const QString& apiBaseUrl, int timeoutMs = 10000) const;
    QString buildBrowserOAuthUrl(const QString& apiBaseUrl,
                                 const QString& provider,
                                 quint16 callbackPort,
                                 const OAuthConfig& config) const;
    Result<OAuthCallbackResult> signInWithBrowser(const QString& oauthUrl,
                                                  int timeoutMs = 180000) const;
};

}  // namespace cppmonetize
