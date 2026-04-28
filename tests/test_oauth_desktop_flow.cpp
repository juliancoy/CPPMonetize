#include "cppmonetize/OAuthDesktopFlow.h"

#include <QtTest/QtTest>

using namespace cppmonetize;

namespace {

class ScopedEnvVar {
public:
    explicit ScopedEnvVar(const char* name)
        : name_(name)
    {
        const QByteArray key(name_);
        hadValue_ = qEnvironmentVariableIsSet(key.constData());
        if (hadValue_) {
            oldValue_ = qgetenv(key.constData());
        }
    }

    ~ScopedEnvVar()
    {
        if (hadValue_) {
            qputenv(name_, oldValue_);
        } else {
            qunsetenv(name_);
        }
    }

private:
    const char* name_;
    bool hadValue_ = false;
    QByteArray oldValue_;
};

}  // namespace

class OAuthDesktopFlowTests : public QObject {
    Q_OBJECT

private slots:
    void resolveSupabaseConfig_usesExplicitEnvAnonKeyForSupabaseOrigin()
    {
        ScopedEnvVar supabaseAnonKey("SUPABASE_ANON_KEY");
        ScopedEnvVar sbAnonKey("SB_ANON_KEY");
        qputenv("SUPABASE_ANON_KEY", QByteArray("unit-test-anon-key"));
        qunsetenv("SB_ANON_KEY");

        OAuthDesktopFlow flow;
        const auto result = flow.resolveSupabaseConfig(QStringLiteral("https://unit-test-project.supabase.co"), 50);

        QVERIFY(result.hasValue());
        QCOMPARE(result.value().enabled, true);
        QCOMPARE(result.value().supabaseUrl, QStringLiteral("https://unit-test-project.supabase.co"));
        QCOMPARE(result.value().supabaseAnonKey, QStringLiteral("unit-test-anon-key"));
    }

    void signInWithPassword_rejectsMissingConfigWithoutNetwork()
    {
        OAuthDesktopFlow flow;
        OAuthConfig config;
        config.enabled = false;
        config.supabaseUrl.clear();
        config.supabaseAnonKey.clear();

        const auto result = flow.signInWithPassword(config,
                                                    QStringLiteral("u@example.com"),
                                                    QStringLiteral("pw"),
                                                    false,
                                                    50);

        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().statusCode, 0);
        QVERIFY(result.error().message.contains(QStringLiteral("not configured"), Qt::CaseInsensitive));
    }

    void resolveSupabaseConfig_usesSupabaseUrlFallbackForNonSupabaseOrigin()
    {
        ScopedEnvVar supabaseUrl("SUPABASE_URL");
        ScopedEnvVar sbUrl("SB_URL");
        ScopedEnvVar supabaseAnonKey("SUPABASE_ANON_KEY");
        qputenv("SUPABASE_URL", QByteArray("https://unit-test-project.supabase.co"));
        qunsetenv("SB_URL");
        qputenv("SUPABASE_ANON_KEY", QByteArray("unit-test-anon-key"));

        OAuthDesktopFlow flow;
        const auto result = flow.resolveSupabaseConfig(QStringLiteral("https://jsynth.us/api"), 50);

        QVERIFY(result.hasValue());
        QCOMPARE(result.value().enabled, true);
        QCOMPARE(result.value().supabaseUrl, QStringLiteral("https://unit-test-project.supabase.co"));
        QCOMPARE(result.value().supabaseAnonKey, QStringLiteral("unit-test-anon-key"));
    }
};

QTEST_MAIN(OAuthDesktopFlowTests)
#include "test_oauth_desktop_flow.moc"
