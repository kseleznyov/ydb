#pragma once

#include "structured_message.h"

#include <ydb/library/actors/core/log_iface.h>
#include <util/datetime/base.h>

#include <memory>

namespace NActors::NStructuredLog {

struct TLogMessage {
    TInstant Time;
    NLog::EPrio Priority;
    NLog::EComponent Component;
    const char* FileName;
    ui64 LineNumber;
    const TString TextMessage;
    TStructuredMessage StructuredMessage;
};

class ILogSink {
public:
    virtual void Write(const TLogMessage&) = 0;
    virtual ~ILogSink() = default;
};
using ILogSinkSPtr = std::shared_ptr<ILogSink>;
}


