# CPPMonetize Rollout and Rollback

## Rollout Order
1. Internal verification build.
2. Small beta cohort.
3. Full production release.

## Pre-Rollout Gate
- SDK tag exists and changelog is frozen.
- Compatibility matrix completed.
- Correlation IDs visible in API logs.
- Alert thresholds configured for auth/entitlement/AI failures.

## Rollout Checklist
1. Pin each app to target SDK tag.
2. Deploy app update to internal channel.
3. Monitor 24h:
- auth failure rate
- entitlement failure rate
- AI request failure rate
4. Promote to beta channel and monitor 24h.
5. Promote to full release.

## Rollback Triggers
- Sustained auth or entitlement failure spike.
- Contract mismatch impacting login or AI gating.
- Crash regression in monetization-critical flows.

## Rollback Procedure
1. Disable monetization enforcement flag if available (AI gating kill switch).
2. Re-pin app(s) to previous known-good SDK tag.
3. Ship rollback patch to affected channel.
4. Confirm error rates normalize.
5. Publish postmortem with root cause and prevention action.

## Post-Rollout Validation
- Verify one successful path per app:
- login
- entitlement check
- AI request (where applicable)
- Verify one controlled failure path produces clear user-facing message.
