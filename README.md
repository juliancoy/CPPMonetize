# CPPMonetize

Shared C++/Qt client SDK for Supabase-backed auth and monetization flows used by desktop apps.

## Scope

Client-side only:
- Auth (`/auth/login`, `/auth/register`, `/auth/whoami`)
- Desktop OAuth helpers (`/auth/supabase-config`, `/oauth/{provider}`, desktop start/exchange)
- AI entitlement and usage checks
- Checkout session initiation and paid download URL retrieval
- Token persistence via secure store abstraction

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
cppmonetize::MonetizeClient client(cfg);

auto login = client.signIn("user@example.com", "password");
if (!login.hasValue()) {
    // handle login.error()
}
```

## Token Store Behavior

- Linux: uses `secret-tool` when available.
- Fallback (all OS): file-based storage in `QStandardPaths::AppConfigLocation`.

For production-grade macOS/Windows secure stores, add platform adapters behind `TokenStore`.

## Versioning

- Current SDK: `0.1.0`
- Intended contract compatibility: backend `v1.x`
