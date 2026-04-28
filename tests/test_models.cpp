#include "cppmonetize/Models.h"
#include "cppmonetize/OfflinePolicy.h"

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

    void offlinePolicy_detectsGraceWindow()
    {
        QVERIFY(isWithinGraceWindow(1000, 1100, 200));
        QVERIFY(!isWithinGraceWindow(1000, 1301, 300));
    }

    void offlinePolicy_rejectsInvalidInputs()
    {
        QVERIFY(!isWithinGraceWindow(0, 1000, 100));
        QVERIFY(!isWithinGraceWindow(1000, 0, 100));
        QVERIFY(!isWithinGraceWindow(1000, 1001, 0));
        QVERIFY(!isWithinGraceWindow(2000, 1000, 1000));
    }

    void deriveCapabilitySet_prefersCapabilitySignals()
    {
        AiEntitlements ent;
        ent.entitled = true;
        ent.contractVersion = QStringLiteral("1.2.0");
        ent.requestsPerMinute = 15;
        ent.projectBudget = 200;

        AiUsageStatus usage;
        usage.hasSubscription = false;
        usage.allowAiRequests = false;
        usage.requiresSubscription = true;

        const CapabilitySet c1 = deriveCapabilitySet(std::optional<AiEntitlements>(ent),
                                                     std::optional<AiUsageStatus>(usage));
        QCOMPARE(c1.values.value(QStringLiteral("ai.allow_requests")).toBool(false), true);
        QCOMPARE(c1.values.value(QStringLiteral("ai.has_subscription")).toBool(true), false);
        QCOMPARE(c1.values.value(QStringLiteral("ai.requires_subscription")).toBool(false), true);
        QCOMPARE(c1.values.value(QStringLiteral("ai.requests_per_minute")).toInt(0), 15);
        QCOMPARE(c1.values.value(QStringLiteral("ai.project_budget")).toInt(0), 200);

        usage.allowAiRequests = true;
        usage.hasSubscription = true;
        const CapabilitySet c2 = deriveCapabilitySet(std::optional<AiEntitlements>(ent),
                                                     std::optional<AiUsageStatus>(usage));
        QCOMPARE(c2.values.value(QStringLiteral("ai.allow_requests")).toBool(false), true);
        QCOMPARE(c2.values.value(QStringLiteral("ai.has_subscription")).toBool(false), true);
    }
};

QTEST_MAIN(ModelParseTests)
#include "test_models.moc"
