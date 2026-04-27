#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${API_BASE_URL:-}" ]]; then
  echo "API_BASE_URL is required" >&2
  exit 1
fi
if [[ -z "${TEST_EMAIL:-}" || -z "${TEST_PASSWORD:-}" ]]; then
  echo "TEST_EMAIL and TEST_PASSWORD are required" >&2
  exit 1
fi

request_id="compat-smoke-$(date +%s)"
auth_json="$(curl -sS -X POST "${API_BASE_URL%/}/auth/login" \
  -H "Content-Type: application/json" \
  -H "X-Client-Request-Id: ${request_id}-login" \
  -d "{\"email\":\"${TEST_EMAIL}\",\"password\":\"${TEST_PASSWORD}\"}")"

token="$(printf '%s' "${auth_json}" | sed -n 's/.*"token":"\([^"]*\)".*/\1/p')"
if [[ -z "${token}" ]]; then
  echo "Failed to obtain token from /auth/login response" >&2
  echo "${auth_json}" >&2
  exit 1
fi

echo "Auth login: ok"

curl -sS "${API_BASE_URL%/}/api/ai/entitlements" \
  -H "Authorization: Bearer ${token}" \
  -H "X-Client-Request-Id: ${request_id}-entitlements" \
  >/dev/null
echo "AI entitlements: ok"

curl -sS "${API_BASE_URL%/}/license" \
  -H "Authorization: Bearer ${token}" \
  -H "X-Client-Request-Id: ${request_id}-license" \
  >/dev/null
echo "License fetch: ok"

echo "Compatibility smoke complete (request prefix: ${request_id})"
