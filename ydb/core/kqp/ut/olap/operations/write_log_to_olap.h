#pragma once
#include <ydb/core/kqp/ut/common/kqp_ut_common.h>

#include <memory>

namespace NKikimr::NKqp {

class TBaseDBLogColumn {
public:
    TBaseDBLogColumn(TString name, TString type, TString extra = {}, bool isPK = false, bool notNull = false, bool isShardingKey = false)
        : Name(std::move(name))
        , Type(std::move(type))
        , Extra(std::move(extra))
        , IsPK(isPK)
        , NotNull(notNull)
        , IsShardingKey(isShardingKey) {}

    TBaseDBLogColumn(TString name, TString type, bool isPK = false, bool notNull = false, bool isShardingKey = false)
        : Name(std::move(name))
        , Type(std::move(type))
        , IsPK(isPK)
        , NotNull(notNull)
        , IsShardingKey(isShardingKey) {}

    const TString Name;
    const TString Type;
    const TString Extra;
    const bool IsPK;
    const bool NotNull;
    const bool IsShardingKey;
};

class TBaseDBLogWriter{
public:

    struct TSettings {
        TString OptionalStorageId = "__MEMORY";
        TString TableName{"olapTable"};
        TString StoreName{"olapStore"};
        ui32 StoreShardsCount = 4;
        ui32 TableShardsCount = 3;

        TString ShardingMethod = "HASH_FUNCTION_CONSISTENCY_64";
    };

    TBaseDBLogWriter(TKikimrRunner& runner, const TSettings& settings, TVector<std::shared_ptr<TBaseDBLogColumn>> columns)
        : Runner(runner)
        , Settings(settings)
        , Columns(std::move(columns))
    {
    }

    TString GetStoreDescription();
    TString GetTableDescription();

    TKikimrRunner& Runner;
    const TSettings Settings;
    const TVector<std::shared_ptr<TBaseDBLogColumn>> Columns;

    void WaitForSchemeOperation(TActorId sender, ui64 txId);
    void ExecuteModifyScheme(NKikimrSchemeOp::TModifyScheme& modifyScheme);
    void CreateStore();
    void CreateTable();

};

}