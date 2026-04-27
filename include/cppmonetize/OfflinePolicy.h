#pragma once

#include <QtGlobal>

namespace cppmonetize {

inline bool isWithinGraceWindow(qint64 lastSuccessEpochMs,
                                qint64 nowEpochMs,
                                qint64 graceWindowMs)
{
    if (lastSuccessEpochMs <= 0 || nowEpochMs <= 0 || graceWindowMs <= 0) {
        return false;
    }
    if (nowEpochMs < lastSuccessEpochMs) {
        return false;
    }
    return (nowEpochMs - lastSuccessEpochMs) <= graceWindowMs;
}

}  // namespace cppmonetize
