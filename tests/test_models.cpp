#include "cppmonetize/Models.h"

#include <QtTest/QtTest>

using namespace cppmonetize;

class ModelParseTests : public QObject {
    Q_OBJECT

private slots:
    void parseAuthSession_acceptsTokenShapes()
    {
        {
            const QJsonObject obj{{QStringLiteral("token"), QStringLiteral("abc")},
                                  {QStringLiteral("email"), QStringLiteral("u@example.com")}};
            ApiError err;
            const auto parsed = parseAuthSession(obj, &err);
            QVERIFY(parsed.has_value());
            QCOMPARE(parsed->accessToken, QStringLiteral("abc"));
            QCOMPARE(parsed->email, QStringLiteral("u@example.com"));
        }

        {
            const QJsonObject obj{{QStringLiteral("session"), QJsonObject{{QStringLiteral("access_token"), QStringLiteral("def")}}},
                                  {QStringLiteral("user"), QJsonObject{{QStringLiteral("id"), QStringLiteral("user-1")}}}};
            ApiError err;
            const auto parsed = parseAuthSession(obj, &err);
            QVERIFY(parsed.has_value());
            QCOMPARE(parsed->accessToken, QStringLiteral("def"));
            QCOMPARE(parsed->userId, QStringLiteral("user-1"));
        }
    }

    void parseAiEntitlements_readsLimitsAndModels()
    {
        const QJsonObject obj{
            {QStringLiteral("entitled"), true},
            {QStringLiteral("contract_version"), QStringLiteral("1.2.0")},
            {QStringLiteral("user"), QJsonObject{{QStringLiteral("id"), QStringLiteral("u1")}}},
            {QStringLiteral("models"), QJsonArray{QStringLiteral("deepseek-chat"), QJsonObject{{QStringLiteral("id"), QStringLiteral("gpt-4o-mini")}}}},
            {QStringLiteral("fallback_order"), QJsonArray{QStringLiteral("deepseek-chat")}},
            {QStringLiteral("limits"), QJsonObject{{QStringLiteral("requests_per_minute"), 12},
                                                    {QStringLiteral("project_budget"), 100},
                                                    {QStringLiteral("timeout_ms"), 9000},
                                                    {QStringLiteral("retries"), 2}}}
        };

        ApiError err;
        const auto parsed = parseAiEntitlements(obj, &err);
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->entitled, true);
        QCOMPARE(parsed->contractVersion, QStringLiteral("1.2.0"));
        QCOMPARE(parsed->models.size(), 2);
        QCOMPARE(parsed->requestsPerMinute, 12);
    }
};

QTEST_MAIN(ModelParseTests)
#include "test_models.moc"
