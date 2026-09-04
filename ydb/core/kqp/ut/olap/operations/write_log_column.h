#pragma once

#include "arrow_type_mapping.h"

#include <contrib/libs/apache/arrow/cpp/src/arrow/type.h>
#include <contrib/libs/apache/arrow/cpp/src/arrow/util/key_value_metadata.h>

#include <util/datetime/base.h>
#include <util/generic/string.h>
#include <util/system/types.h>

#include <string>
#include <vector>

namespace NKikimr::NKqp::NLogToDB {

class TBaseDBLogColumn {
public:
    struct TSettings {
        TString Extra;
        bool IsPK {false};
        bool NotNull {false};
        bool IsShardingKey {false};
    };

    TBaseDBLogColumn() = default;
    TBaseDBLogColumn(TString name, TString type)
        : TBaseDBLogColumn(std::move(name), std::move(type), TSettings()) {}
    TBaseDBLogColumn(TString name, TString type, TSettings settings)
        : Name(std::move(name))
        , Type(std::move(type))
        , Settings(std::move(settings)) {}
    virtual ~TBaseDBLogColumn() = default;

    const TString Name;
    const TString Type;
    const TSettings Settings;

    virtual std::shared_ptr<arrow::DataType> GetArrowDataType() const = 0;

    std::shared_ptr<arrow::Field> MakeArrowField() const {
        auto arrowSchemaField = std::make_shared<arrow::KeyValueMetadata>(
            std::vector<std::string>{"ydb.type"},
            std::vector<std::string>{std::string(Type)});
        return arrow::field(std::string(Name), GetArrowDataType(), !Settings.NotNull, arrowSchemaField);
    }

    virtual void Reset() = 0;
    virtual std::shared_ptr<arrow::Array> MakeArray() = 0;

    virtual bool AddDummyValue() = 0;
};

template <typename T>
class TTypedDBLogColumn : public TBaseDBLogColumn {
public:
    using TValueType = T;
    using TArrowBuilderType = TArrowTypeMapper<TValueType>::TArrowBuilderType;
    using TArrowArrayType = TArrowTypeMapper<TValueType>::TArrowArrayType;
    static constexpr const char* TypeName = TArrowTypeMapper<TValueType>::TypeName;

    std::shared_ptr<TArrowBuilderType> Builder;

    TTypedDBLogColumn(TString name, TSettings settings)
        : TBaseDBLogColumn(std::move(name), TypeName, std::move(settings)),
        Builder(TArrowTypeMapper<TValueType>::CreateBuilder()) {}

    std::shared_ptr<arrow::DataType> GetArrowDataType() const override {
        return TArrowTypeMapper<TValueType>::GetArrowDataType();
    }

    void Reset() override {
        Builder->Reset();
    }

    bool AppendValue(const TValueType& value) {
        return TArrowTypeMapper<TValueType>::AppendValue(*Builder, value);
    }

    std::shared_ptr<arrow::Array> MakeArray() override {
        std::shared_ptr<arrow::Array> result;
        auto status = Builder->Finish(&result);
        if (!status.ok()) {
            return nullptr;
        }
        return result;
    }

    bool AddDummyValue() override {
        TValueType value{};
        return AppendValue(value);
    }
};

using TBoolDBLogColumn = TTypedDBLogColumn<bool>;
using TInt8DBLogColumn = TTypedDBLogColumn<i8>;
using TUint8DBLogColumn = TTypedDBLogColumn<ui8>;
using TInt16DBLogColumn = TTypedDBLogColumn<i16>;
using TUint16DBLogColumn = TTypedDBLogColumn<ui16>;
using TInt32DBLogColumn = TTypedDBLogColumn<i32>;
using TUint32DBLogColumn = TTypedDBLogColumn<ui32>;
using TInt64DBLogColumn = TTypedDBLogColumn<i64>;
using TUint64DBLogColumn = TTypedDBLogColumn<ui64>;
using TFloatDBLogColumn = TTypedDBLogColumn<float>;
using TDoubleDBLogColumn = TTypedDBLogColumn<double>;
using TStringDBLogColumn = TTypedDBLogColumn<TString>;
using TInstantDBLogColumn = TTypedDBLogColumn<TInstant>;

}
