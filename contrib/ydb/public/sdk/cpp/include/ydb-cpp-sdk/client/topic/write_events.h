#pragma once

#include "events_common.h"

#include <contrib/ydb/public/sdk/cpp/include/ydb-cpp-sdk/client/types/exceptions/exceptions.h>
#include <contrib/ydb/public/sdk/cpp/include/ydb-cpp-sdk/client/types/status/status.h>

#include <util/generic/ptr.h>

namespace NYdb::inline Dev::NTopic {

struct TWriteStat : public TThrRefBase {
    TDuration WriteTime;
    TDuration MinTimeInPartitionQueue;
    TDuration MaxTimeInPartitionQueue;
    TDuration PartitionQuotedTime;
    TDuration TopicQuotedTime;
    using TPtr = TIntrusivePtr<TWriteStat>;
};

class TContinuationToken : public TNonCopyable {
    friend class TContinuationTokenIssuer;

    bool Valid = true;

public:
    TContinuationToken& operator=(TContinuationToken&& other) {
        if (!other.Valid) {
            ythrow TContractViolation("Cannot move invalid token");
        }
        Valid = std::exchange(other.Valid, false);
        return *this;
    }

    TContinuationToken(TContinuationToken&& other) {
        *this = std::move(other);
    }

private:
    TContinuationToken() = default;
};

class TContinuationTokenIssuer {
protected:
    static auto IssueContinuationToken() {
        return TContinuationToken{};
    }
};

//! Events for write session.
struct TWriteSessionEvent {

    //! Event with acknowledge for written messages.
    struct TWriteAck {
        //! Write result.
        enum EEventState {
            EES_WRITTEN, //! Successfully written.
            EES_ALREADY_WRITTEN, //! Skipped on SeqNo deduplication.
            EES_DISCARDED, //! In case of destruction of writer or retry policy discarded future retries in this writer.
            EES_WRITTEN_IN_TX, //! Successfully written in tx.
        };
        //! Details of successfully written message.
        struct TWrittenMessageDetails {
            uint64_t Offset;
            uint64_t PartitionId;
        };
        //! Same SeqNo as provided on write.
        uint64_t SeqNo;
        EEventState State;
        //! Filled only for EES_WRITTEN. Empty for ALREADY and DISCARDED.
        std::optional<TWrittenMessageDetails> Details;
        //! Write stats from server. See TWriteStat. nullptr for DISCARDED event.
        TWriteStat::TPtr Stat;
    };

    struct TAcksEvent : public TPrintable<TAcksEvent> {
        //! Acks could be batched from several Write requests.
        //! They are provided to client as soon as possible.
        std::vector<TWriteAck> Acks;
    };

    //! Indicates that a writer is ready to accept new message(s).
    //! Continuation token should be kept and then used in write methods.
    struct TReadyToAcceptEvent : public TPrintable<TReadyToAcceptEvent> {
        mutable TContinuationToken ContinuationToken;

        TReadyToAcceptEvent() = delete;
        TReadyToAcceptEvent(TContinuationToken&& t) : ContinuationToken(std::move(t)) {
        }
        TReadyToAcceptEvent(TReadyToAcceptEvent&&) = default;
        TReadyToAcceptEvent(const TReadyToAcceptEvent& other) : ContinuationToken(std::move(other.ContinuationToken)) {
        }
        TReadyToAcceptEvent& operator=(TReadyToAcceptEvent&&) = default;
        TReadyToAcceptEvent& operator=(const TReadyToAcceptEvent& other) {
            ContinuationToken = std::move(other.ContinuationToken);
            return *this;
        }
    };

    using TEvent = std::variant<TAcksEvent, TReadyToAcceptEvent, TSessionClosedEvent>;
};

//! Events debug strings.
template<>
void TPrintable<TWriteSessionEvent::TAcksEvent>::DebugString(TStringBuilder& ret, bool printData) const;
template<>
void TPrintable<TWriteSessionEvent::TReadyToAcceptEvent>::DebugString(TStringBuilder& ret, bool printData) const;

std::string DebugString(const TWriteSessionEvent::TEvent& event);

}
