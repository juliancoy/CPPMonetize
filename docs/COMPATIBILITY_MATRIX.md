# CPPMonetize Compatibility Matrix

## Scope
Validate client/backend compatibility for:
- `JCut`
- `JSynth`
- `motive3d`
- shared Supabase/API backend contracts (`v1.x`)

## Required Axes

| Axis | Values |
|---|---|
| Client version | latest release, previous release |
| Backend contract | current, previous supported |
| Auth path | email/password, browser OAuth |
| Commerce path | entitlement fetch, checkout init, download grant |
| AI path | entitlement check, AI task submit |

## Minimum Matrix

| Case ID | Client | Client Version | Backend | Expected |
|---|---|---|---|---|
| M1 | JCut | latest | current | pass |
| M2 | JSynth | latest | current | pass |
| M3 | motive3d | latest | current | pass |
| M4 | JCut | previous | current | no contract break |
| M5 | JSynth | previous | current | no contract break |
| M6 | motive3d | previous | current | no contract break |
| M7 | JCut | latest | previous supported | graceful compatibility/fail message |
| M8 | JSynth | latest | previous supported | graceful compatibility/fail message |
| M9 | motive3d | latest | previous supported | graceful compatibility/fail message |

## Test Steps (Per Case)
1. Launch app build for target client version.
2. Sign in (email/password).
3. Sign out, then sign in via browser OAuth.
4. Fetch entitlements.
5. If AI-enabled client path exists, submit one AI request.
6. If purchase path exists, request checkout session and one download grant.
7. Verify failure mode text for incompatible contracts is user-readable.

## Artifacts
- Attach app logs with `request_id` values.
- Capture API-side logs filtered by `X-Client-Request-Id`.
- Record pass/fail in a dated run report.

## Exit Criteria
- All M1-M6 pass.
- M7-M9 do not crash and return clear actionable errors.
