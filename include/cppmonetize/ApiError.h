#pragma once

#include <QString>

namespace cppmonetize {

struct ApiError {
    int statusCode = 0;
    QString code;
    QString message;
    QString details;

    [[nodiscard]] bool isValid() const { return !message.trimmed().isEmpty() || statusCode != 0; }
};

}  // namespace cppmonetize
