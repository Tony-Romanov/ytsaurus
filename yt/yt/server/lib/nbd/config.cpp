#include "config.h"

namespace NYT::NNbd {

////////////////////////////////////////////////////////////////////////////////

void TChunkBlockDeviceConfig::Register(TRegistrar registrar)
{
    registrar.Parameter("size", &TThis::Size)
        .GreaterThanOrEqual(0);
    registrar.Parameter("medium_index", &TThis::MediumIndex)
        .Default(0);
    registrar.Parameter("fs_type", &TThis::FsType)
        .Default(EFilesystemType::Ext4);
    registrar.Parameter("keep_session_alive_period", &TThis::KeepSessionAlivePeriod)
        .Default(TDuration::Seconds(1));
    registrar.Parameter("data_node_nbd_service_rpc_timeout", &TThis::DataNodeNbdServiceRpcTimeout)
        .Default(TDuration::Seconds(5));
    registrar.Parameter("data_node_nbd_service_make_timeout", &TThis::DataNodeNbdServiceMakeTimeout)
        .Default(TDuration::Seconds(5));
}

////////////////////////////////////////////////////////////////////////////////

void TFileSystemBlockDeviceConfig::Register(TRegistrar)
{ }

////////////////////////////////////////////////////////////////////////////////

void TMemoryBlockDeviceConfig::Register(TRegistrar registrar)
{
    registrar.Parameter("size", &TThis::Size)
        .GreaterThanOrEqual(0);
}

////////////////////////////////////////////////////////////////////////////////

void TDynamicTableBlockDeviceConfig::Register(TRegistrar registrar)
{
    registrar.Parameter("size", &TThis::Size)
        .GreaterThanOrEqual(0);
    registrar.Parameter("block_size", &TThis::BlockSize)
        .GreaterThanOrEqual(0);
    registrar.Parameter("read_batch_size", &TThis::ReadBatchSize)
        .Default(16).GreaterThan(0);
    registrar.Parameter("write_batch_size", &TThis::WriteBatchSize)
        .Default(16).GreaterThan(0);
    registrar.Parameter("table_path", &TThis::TablePath);
}

////////////////////////////////////////////////////////////////////////////////

void TIdsConfig::Register(TRegistrar registrar)
{
    registrar.Parameter("port", &TThis::Port)
        .Default(10809);
    registrar.Parameter("max_backlog_size", &TThis::MaxBacklogSize)
        .Default(1'000);
}

////////////////////////////////////////////////////////////////////////////////

void TUdsConfig::Register(TRegistrar registrar)
{
    registrar.Parameter("path", &TThis::Path)
        .Default("/tmp/nbd.sock");
    registrar.Parameter("max_backlog_size", &TThis::MaxBacklogSize)
        .Default(1'000);
}

////////////////////////////////////////////////////////////////////////////////

void TNbdTestOptions::Register(TRegistrar registrar)
{
    registrar.Parameter("block_device_sleep_before_read", &TThis::BlockDeviceSleepBeforeRead)
        .Default();
    registrar.Parameter("block_device_sleep_before_write", &TThis::BlockDeviceSleepBeforeWrite)
        .Default();
    registrar.Parameter("set_block_device_error_on_read", &TThis::SetBlockDeviceErrorOnRead)
        .Default();
    registrar.Parameter("set_block_device_error_on_write", &TThis::SetBlockDeviceErrorOnWrite)
        .Default();
    registrar.Parameter("abort_connection_on_read", &TThis::AbortConnectionOnRead)
        .Default();
    registrar.Parameter("abort_connection_on_write", &TThis::AbortConnectionOnWrite)
        .Default();
}

////////////////////////////////////////////////////////////////////////////////

void TNbdServerConfig::Register(TRegistrar registrar)
{
    registrar.Parameter("internet_domain_socket", &TThis::InternetDomainSocket)
        .Default();
    registrar.Parameter("unix_domain_socket", &TThis::UnixDomainSocket)
        .Default();
    registrar.Parameter("thread_count", &TThis::ThreadCount)
        .Default(1);
    registrar.Parameter("test_options", &TThis::TestOptions)
        .DefaultNew();

    registrar.Postprocessor([] (TThis* config) {
        if (config->InternetDomainSocket && config->UnixDomainSocket) {
            THROW_ERROR_EXCEPTION("\"internet_domain_socket\" and \"unix_domain_socket\" cannot be both present");
        }

        if (!config->InternetDomainSocket && !config->UnixDomainSocket) {
            THROW_ERROR_EXCEPTION("\"internet_domain_socket\" and \"unix_domain_socket\" cannot be both missing");
        }
    });
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NNbd
