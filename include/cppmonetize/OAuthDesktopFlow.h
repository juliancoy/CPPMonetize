#pragma once

#include "cppmonetize/Result.h"

#include <functional>
#include <QString>

namespace cppmonetize {

struct OAuthConfig {
    bool enabled = false;
    QString supabaseUrl;
    QString supabaseAnonKey;
    QString desktopRedirectBase;
};

struct OAuthCallbackResult {
    QString token;
    QString email;
};

struct PasswordAuthResult {
    QString token;
    QString email;
};

class OAuthDesktopFlow {
public:
    OAuthDesktopFlow() = default;

    Result<OAuthConfig> resolveSupabaseConfig(const QString& apiBaseUrl,
                                              int timeoutMs = 10000) const;
    Result<OAuthConfig> fetchOAuthConfig(const QString& apiBaseUrl, int timeoutMs = 10000) const;
    QString buildBrowserOAuthUrl(const QString& apiBaseUrl,
                                 const QString& provider,
                                 quint16 callbackPort,
                                 const OAuthConfig& config) const;
    QString buildSupabaseAuthorizeUrl(const OAuthConfig& config,
                                      const QString& provider = QStringLiteral("google"),
                                      const QString& redirectTo = QStringLiteral("http://127.0.0.1/callback"),
                                      bool includePkceFlowParams = true) const;
    Result<OAuthCallbackResult> signInWithBrowserPkce(const OAuthConfig& config,
                                                      const QString& provider = QStringLiteral("google"),
                                                      int timeoutMs = 180000,
                                                      bool openBrowser = true,
                                                      const std::function<void(const QString&)>& authUrlReady = {}) const;
    Result<OAuthCallbackResult> signInWithBrowser(const QString& oauthUrl,
                                                  int timeoutMs = 180000) const;
    Result<PasswordAuthResult> signInWithPassword(const OAuthConfig& config,
                                                  const QString& email,
                                                  const QString& password,
                                                  bool registerMode = false,
                                                  int timeoutMs = 20000) const;
};

}  // namespace cppmonetize
