#include "write_log_to_olap.h"

#include <ydb/core/kqp/ut/olap/combinatory/variator.h>
#include <ydb/core/kqp/ut/olap/helpers/get_value.h>
#include <ydb/core/kqp/ut/olap/helpers/local.h>
#include <ydb/core/kqp/ut/olap/helpers/query_executor.h>
#include <ydb/core/kqp/ut/olap/helpers/typed_local.h>
#include <ydb/core/kqp/ut/olap/helpers/writer.h>

#include <ydb/core/kqp/ut/olap/operations/write_log_to_olap.h>

#include <ydb/core/base/tablet_pipecache.h>
#include <ydb/core/protos/schemeshard/operations.pb.h>
#include <ydb/core/tx/columnshard/hooks/testing/controller.h>
#include <ydb/core/tx/columnshard/test_helper/controllers.h>
#include <ydb/core/protos/long_tx_service_config.pb.h>
#include <ydb/core/wrappers/fake_storage.h>

#include <library/cpp/testing/unittest/registar.h>

namespace NKikimr::NKqp {

TString TBaseDBLogWriter::GetStoreDescription() {
    TStringBuilder sb;
    sb << R"(Columns{ Name: "timestamp" Type : "Timestamp" NotNull : true })";
    sb << R"(Columns{ Name: "resource_id" Type : "Utf8" DataAccessorConstructor{ ClassName: "PLAIN" } })";
    sb << "Columns{ Name: \"uid\" Type : \"Utf8\" NotNull : true StorageId : \"" + Settings.OptionalStorageId + "\" }";
    sb << R"(Columns{ Name: "level" Type : "Int32" })";
    sb << "Columns{ Name: \"message\" Type : \"Utf8\" StorageId : \"" + Settings.OptionalStorageId + "\" }";
    sb << R"(Columns{ Name: "new_column1" Type : "Uint64" })";
    /* if (GetWithJsonDocument()) {
        sb << R"(Columns{ Name: "json_payload" Type : "JsonDocument" })";
    } */
    sb << R"(
        KeyColumnNames: "timestamp"
        KeyColumnNames: "uid"
    )";

    TString storeDesc = Sprintf(R"(
        Name: "%s"
        ColumnShardCount: %d
        SchemaPresets {
            Name: "default"
            Schema {
                %s
                }
            }
        )", Settings.StoreName.c_str(), Settings.StoreShardsCount, sb.data());
    return storeDesc;
}

TString TBaseDBLogWriter::GetTableDescription() {
    // @todo Как правильно назвать?
    TString shardingColumns = "[\"timestamp\", \"uid\"]";
    // TString shardingMethod = "HASH_FUNCTION_CONSISTENCY_64";

    TString result = Sprintf(R"(
        Name: "%s"
        ColumnShardCount: %d
        Sharding {
            HashSharding {
                Function: %s
                Columns: %s
            }
        })", Settings.TableName.c_str(),
        Settings.TableShardsCount,
        Settings.ShardingMethod.data(),
        shardingColumns.c_str());
    return result;
}

void TBaseDBLogWriter::WaitForSchemeOperation(TActorId sender, ui64 txId) {
    auto& server = Runner.GetTestServer();
    auto& runtime = *server.GetRuntime();
    auto& settings = server.GetSettings();
    auto request = MakeHolder<NSchemeShard::TEvSchemeShard::TEvNotifyTxCompletion>();
    request->Record.SetTxId(txId);

    // auto tid = ChangeStateStorage(Tests::SchemeRoot, settings.Domain);
    const ui64 mask = static_cast<ui64>(0xff) << 56;
    ui64 tabletId = Tests::SchemeRoot;                                                 // @todo What value outside tests?
    ui32 id = settings.Domain;                                                         // @todo What value outside tests?
    auto tid = (tabletId & ~mask) | static_cast<ui64>(id & 0xff) << 56;

    runtime.SendToPipe(tid, sender, request.Release(), 0, GetPipeConfigWithRetries()); // @todo What GetPipeConfigWithRetries() outside tests?
    runtime.template GrabEdgeEventRethrow<NSchemeShard::TEvSchemeShard::TEvNotifyTxCompletionResult>(sender);
}

void TBaseDBLogWriter::ExecuteModifyScheme(NKikimrSchemeOp::TModifyScheme& modifyScheme) {
    auto& server = Runner.GetTestServer();
    auto request = std::make_unique<TEvTxUserProxy::TEvProposeTransaction>();
    request->Record.SetExecTimeoutPeriod(Max<ui64>());
    *request->Record.MutableTransaction()->MutableModifyScheme() = modifyScheme;
    TActorId sender = server.GetRuntime()->AllocateEdgeActor();
    server.GetRuntime()->Send(new IEventHandle(MakeTxProxyID(), sender, request.release()));
    auto ev = server.GetRuntime()->template GrabEdgeEventRethrow<TEvTxUserProxy::TEvProposeTransactionStatus>(sender);
    auto status = ev->Get()->Record.GetStatus();
    ui64 txId = ev->Get()->Record.GetTxId();
    UNIT_ASSERT(status != TEvTxUserProxy::TEvProposeTransactionStatus::EStatus::ExecError);
    WaitForSchemeOperation(sender, txId);
}

void TBaseDBLogWriter::CreateStore() {
    TString scheme = GetStoreDescription();
    NKikimrSchemeOp::TColumnStoreDescription store;
    UNIT_ASSERT(::google::protobuf::TextFormat::ParseFromString(scheme, &store));
    NKikimrSchemeOp::TModifyScheme op;
    op.SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreateColumnStore);
    op.SetWorkingDir("/Root");
    op.MutableCreateColumnStore()->CopyFrom(store);
    ExecuteModifyScheme(op);
}

void TBaseDBLogWriter::CreateTable() {
    TString storeOrDirName = Settings.StoreName;
    TString scheme = GetTableDescription();
    NKikimrSchemeOp::TColumnTableDescription table;
    UNIT_ASSERT(::google::protobuf::TextFormat::ParseFromString(scheme, &table));
    TString workingDir = "/Root";
    if (!storeOrDirName.empty()) {
        workingDir += "/" + storeOrDirName;
    }

    NKikimrSchemeOp::TModifyScheme op;
    op.SetOperationType(NKikimrSchemeOp::EOperationType::ESchemeOpCreateColumnTable);
    op.SetWorkingDir(workingDir);
    op.MutableCreateColumnTable()->CopyFrom(table);
    // @was: helper.ExecuteModifyScheme(op);
    ExecuteModifyScheme(op);
}

}
