# CPPMonetize

Shared C++/Qt client SDK for Supabase-backed auth and monetization flows used by desktop apps.

## Scope

Client-side only:
- Auth (`/auth/login`, `/auth/register`, `/auth/whoami`)
- Desktop OAuth helpers (`/auth/supabase-config`, `/oauth/{provider}`, desktop start/exchange)
- AI entitlement and usage checks
- Checkout session initiation and paid download URL retrieval
- Token persistence via secure store abstraction
- Offline grace-window helper for temporary entitlement-check outages

Out of scope:
- Stripe webhook handling
- Server-side entitlement grants/revokes
- Service-role database operations

## Build

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Operational Docs

- `docs/COMPATIBILITY_MATRIX.md`
- `docs/ROLLOUT_ROLLBACK.md`
- `docs/SLO_ALERTS.md`
- `scripts/compat_smoke.sh` (API smoke helper)

## Integrating in App CMake

```cmake
add_subdirectory(external/CPPMonetize)
target_link_libraries(your_app PRIVATE CPPMonetize::cppmonetize)
```

## Basic Usage

```cpp
#include <cppmonetize/MonetizeClient.h>

cppmonetize::ClientConfig cfg;
cfg.apiBaseUrl = "https://jsynth.us/api";
cfg.clientId = "jsynth-desktop";
cfg.telemetryHook = [](const cppmonetize::RequestTelemetryEvent& ev) {
    if (!ev.success) {
        qWarning() << "[CPPMonetize]" << ev.operation << ev.statusCode
                   << ev.clientRequestId << ev.message;
    }
};
cppmonetize::MonetizeClient client(cfg);

auto login = client.signIn("user@example.com", "password");
if (!login.hasValue()) {
    // handle login.error()
}
```

Every request includes a client-generated correlation header (`X-Client-Request-Id` by default).
Override `ClientConfig::requestIdHeaderName` if your API expects a different header.

## Token Store Behavior

- Linux: uses `secret-tool` when available.
- Fallback (all OS): file-based storage in `QStandardPaths::AppConfigLocation`.

For production-grade macOS/Windows secure stores, add platform adapters behind `TokenStore`.

## Versioning

- Current SDK: `1.0.0`
- Intended contract compatibility: backend `v1.x`
