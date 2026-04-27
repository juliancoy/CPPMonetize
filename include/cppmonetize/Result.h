#pragma once

#include "cppmonetize/ApiError.h"

#include <optional>

namespace cppmonetize {

template <typename T>
class Result {
public:
    static Result<T> ok(T value)
    {
        Result<T> r;
        r.value_ = std::move(value);
        return r;
    }

    static Result<T> fail(ApiError error)
    {
        Result<T> r;
        r.error_ = std::move(error);
        return r;
    }

    [[nodiscard]] bool hasValue() const { return value_.has_value(); }
    [[nodiscard]] const T& value() const { return *value_; }
    [[nodiscard]] T& value() { return *value_; }
    [[nodiscard]] const ApiError& error() const { return error_; }

private:
    std::optional<T> value_;
    ApiError error_;
};

}  // namespace cppmonetize
