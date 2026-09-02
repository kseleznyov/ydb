#pragma once
#include <ydb/core/kqp/ut/common/kqp_ut_common.h>

namespace NKikimr::NKqp {

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

    TBaseDBLogWriter(TKikimrRunner& runner, const TSettings& settings): Runner(runner), Settings(settings) {
    }

    TString GetStoreDescription();
    TString GetTableDescription();

    TKikimrRunner& Runner;
    const TSettings Settings;

    void WaitForSchemeOperation(TActorId sender, ui64 txId);
    void ExecuteModifyScheme(NKikimrSchemeOp::TModifyScheme& modifyScheme);
    void CreateStore();
    void CreateTable();

};

}