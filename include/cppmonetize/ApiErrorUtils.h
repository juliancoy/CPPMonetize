#pragma once

#include "cppmonetize/ApiError.h"

namespace cppmonetize {

bool isAuthTokenFailure(const ApiError& error);
bool isSubscriptionRequiredError(const ApiError& error);
bool isBackendSchemaMisconfigured(const ApiError& error);

}  // namespace cppmonetize

