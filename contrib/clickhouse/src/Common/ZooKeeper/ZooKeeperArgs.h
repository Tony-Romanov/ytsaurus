#pragma once
#include <Common/ZooKeeper/Types.h>
#include <Common/ZooKeeper/ZooKeeperConstants.h>
#include <Common/GetPriorityForLoadBalancing.h>

namespace DBPoco::Util
{
    class AbstractConfiguration;
}

namespace zkutil
{

constexpr UInt32 ZK_MIN_FALLBACK_SESSION_DEADLINE_SEC = 3 * 60 * 60;
constexpr UInt32 ZK_MAX_FALLBACK_SESSION_DEADLINE_SEC = 6 * 60 * 60;

struct ZooKeeperArgs
{
    struct SessionLifetimeConfiguration
    {
        UInt32 min_sec = ZK_MIN_FALLBACK_SESSION_DEADLINE_SEC;
        UInt32 max_sec = ZK_MAX_FALLBACK_SESSION_DEADLINE_SEC;
        bool operator == (const SessionLifetimeConfiguration &) const = default;
    };
    ZooKeeperArgs(const DBPoco::Util::AbstractConfiguration & config, const String & config_name);

    /// hosts_string -- comma separated [secure://]host:port list
    ZooKeeperArgs(const String & hosts_string); /// NOLINT(google-explicit-constructor)
    ZooKeeperArgs() = default;
    bool operator == (const ZooKeeperArgs &) const = default;

    String zookeeper_name = "zookeeper";
    String implementation = "zookeeper";
    Strings hosts;
    Strings availability_zones;
    String auth_scheme;
    String identity;
    String chroot;
    String sessions_path = "/clickhouse/sessions";
    String client_availability_zone;
    int32_t connection_timeout_ms = Coordination::DEFAULT_CONNECTION_TIMEOUT_MS;
    int32_t session_timeout_ms = Coordination::DEFAULT_SESSION_TIMEOUT_MS;
    int32_t operation_timeout_ms = Coordination::DEFAULT_OPERATION_TIMEOUT_MS;
    bool enable_fault_injections_during_startup = false;
    double send_fault_probability = 0.0;
    double recv_fault_probability = 0.0;
    double send_sleep_probability = 0.0;
    double recv_sleep_probability = 0.0;
    UInt64 send_sleep_ms = 0;
    UInt64 recv_sleep_ms = 0;
    bool use_compression = false;
    bool prefer_local_availability_zone = false;
    bool availability_zone_autodetect = false;

    SessionLifetimeConfiguration fallback_session_lifetime = {};
    DB::GetPriorityForLoadBalancing get_priority_load_balancing;

private:
    void initFromKeeperServerSection(const DBPoco::Util::AbstractConfiguration & config);
    void initFromKeeperSection(const DBPoco::Util::AbstractConfiguration & config, const std::string & config_name);
};

}
