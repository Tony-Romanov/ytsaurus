#pragma once

#include "clickhouse_config.h"

#if USE_NURAFT

#include <DBPoco/Net/TCPServerConnection.h>
#include <Common/MultiVersion.h>
#include "IServer.h"
#include <Common/Stopwatch.h>
#include <Common/ZooKeeper/ZooKeeperCommon.h>
#include <Common/ZooKeeper/ZooKeeperConstants.h>
#include <Common/ConcurrentBoundedQueue.h>
#include <Coordination/KeeperDispatcher.h>
#include <IO/WriteBufferFromPocoSocket.h>
#include <IO/ReadBufferFromPocoSocket.h>
#include <unordered_map>
#include <Coordination/KeeperConnectionStats.h>
#include <DBPoco/Timestamp.h>
#include <Compression/CompressedReadBuffer.h>
#include <Compression/CompressedWriteBuffer.h>

namespace DB
{

struct SocketInterruptablePollWrapper;
using SocketInterruptablePollWrapperPtr = std::unique_ptr<SocketInterruptablePollWrapper>;

struct RequestWithResponse
{
    Coordination::ZooKeeperResponsePtr response;
    Coordination::ZooKeeperRequestPtr request; /// it can be nullptr for some responses
};

using ThreadSafeResponseQueue = ConcurrentBoundedQueue<RequestWithResponse>;
using ThreadSafeResponseQueuePtr = std::shared_ptr<ThreadSafeResponseQueue>;

struct LastOp;
using LastOpMultiVersion = MultiVersion<LastOp>;
using LastOpPtr = LastOpMultiVersion::Version;

class KeeperTCPHandler : public DBPoco::Net::TCPServerConnection
{
public:
    static void registerConnection(KeeperTCPHandler * conn);
    static void unregisterConnection(KeeperTCPHandler * conn);
    /// dump all connections statistics
    static void dumpConnections(WriteBufferFromOwnString & buf, bool brief);
    static void resetConnsStats();

private:
    static std::mutex conns_mutex;
    /// all connections
    static std::unordered_set<KeeperTCPHandler *> connections;

public:
    KeeperTCPHandler(
        const DBPoco::Util::AbstractConfiguration & config_ref,
        std::shared_ptr<KeeperDispatcher> keeper_dispatcher_,
        DBPoco::Timespan receive_timeout_,
        DBPoco::Timespan send_timeout_,
        const DBPoco::Net::StreamSocket & socket_);
    void run() override;

    KeeperConnectionStats & getConnectionStats();
    void dumpStats(WriteBufferFromOwnString & buf, bool brief);
    void resetStats();

    ~KeeperTCPHandler() override;

private:
    LoggerPtr log;
    std::shared_ptr<KeeperDispatcher> keeper_dispatcher;
    DBPoco::Timespan operation_timeout;
    DBPoco::Timespan min_session_timeout;
    DBPoco::Timespan max_session_timeout;
    DBPoco::Timespan session_timeout;
    int64_t session_id{-1};
    Stopwatch session_stopwatch;
    SocketInterruptablePollWrapperPtr poll_wrapper;
    DBPoco::Timespan send_timeout;
    DBPoco::Timespan receive_timeout;

    ThreadSafeResponseQueuePtr responses;

    Coordination::XID close_xid = Coordination::CLOSE_XID;

    /// Streams for reading/writing from/to client connection socket.
    std::optional<ReadBufferFromPocoSocket> in;
    std::optional<WriteBufferFromPocoSocket> out;
    std::optional<CompressedReadBuffer> compressed_in;
    std::optional<CompressedWriteBuffer> compressed_out;

    std::atomic<bool> connected{false};

    void runImpl();

    WriteBuffer & getWriteBuffer();
    void flushWriteBuffer();
    ReadBuffer & getReadBuffer();

    void sendHandshake(bool has_leader, bool & use_compression);
    DBPoco::Timespan receiveHandshake(int32_t handshake_length, bool & use_compression);

    static bool isHandShake(int32_t handshake_length);
    bool tryExecuteFourLetterWordCmd(int32_t command);

    std::pair<Coordination::OpNum, Coordination::XID> receiveRequest();

    void packageSent();
    void packageReceived();

    void updateStats(Coordination::ZooKeeperResponsePtr & response, const Coordination::ZooKeeperRequestPtr & request);

    DBPoco::Timestamp established;

    using Operations = std::unordered_map<Coordination::XID, DBPoco::Timestamp>;
    Operations operations;

    LastOpMultiVersion last_op;

    KeeperConnectionStats conn_stats;

};

}
#endif
