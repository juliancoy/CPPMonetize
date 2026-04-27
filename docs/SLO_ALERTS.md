# CPPMonetize SLO and Alert Checklist

## Service-Level Objectives
- Auth success rate: >= 99.5%
- Entitlement fetch success rate: >= 99.5%
- AI request success rate (entitled users): >= 99.0%
- P95 entitlement latency: <= 800 ms
- P95 AI task submit latency: <= 1500 ms

## Required Metrics
- `auth.requests`, `auth.failures`
- `entitlement.requests`, `entitlement.failures`
- `ai.requests`, `ai.failures`
- `http.latency_ms` (tagged by operation)
- `contract.version.mismatch`

## Alert Conditions
1. Auth failure rate > 2% for 10 min.
2. Entitlement failure rate > 2% for 10 min.
3. AI request failure rate > 5% for 10 min.
4. Contract mismatch count > 0 after release.

## Correlation Requirements
- Client sends `X-Client-Request-Id` for every request.
- Backend logs include the same request ID.
- Incident reports include at least one request ID per failure signature.
