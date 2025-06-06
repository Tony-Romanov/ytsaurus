#pragma once

#include "defs.h"

#include <contrib/ydb/core/blobstorage/lwtrace_probes/blobstorage_probes.h>
#include <contrib/ydb/core/base/appdata_fwd.h>

namespace NKikimr {

LWTRACE_USING(BLOBSTORAGE_PROVIDER);

struct TRootCause {
    struct TRootCauseItem {
        ui64 CauseIdx;
        ui64 StartCycles;
        ui64 TransferCycles;
        ui64 VDiskReplyCycles;
        bool IsAccelerate;
        TString RequestType;

        TRootCauseItem(ui64 causeIdx, ui64 startCycles, bool isAccelerate, const TString& requestType)
            : CauseIdx(causeIdx)
            , StartCycles(startCycles)
            , TransferCycles(startCycles)
            , VDiskReplyCycles(startCycles)
            , IsAccelerate(isAccelerate)
            , RequestType(requestType)
        {}
    };
    static constexpr ui64 InvalidCauseIdx = 255;

    bool IsOn = false;
    TDeque<TRootCauseItem> Items;
    ui64 CurrentCauseIdx = InvalidCauseIdx;


    // Walk the cause tree from leaf to root and output it as an LWTRACK
    void RenderTrack(NLWTrace::TOrbit &orbit) {
#ifdef LWTRACE_DISABLE
        Y_UNUSED(orbit);
#else //LWTRACE_DISABLE
        if (HasShuttles(orbit)) {
            if (CurrentCauseIdx < Items.size()) {
                const TRootCauseItem &item = Items[CurrentCauseIdx];
                if (item.CauseIdx < CurrentCauseIdx) {
                    ui64 savedCauseIdx = CurrentCauseIdx;
                    CurrentCauseIdx = item.CauseIdx;
                    RenderTrack(orbit);
                    CurrentCauseIdx = savedCauseIdx;
                }
                NLWTrace::TParams params;
                if (item.IsAccelerate) {
                    params.Param[1].CopyConstruct<TString>(item.RequestType);
                    orbit.AddProbe(&LWTRACE_GET_NAME(DSProxyScheduleAccelerate).Probe, params, item.StartCycles);
                } else {
                    orbit.AddProbe(&LWTRACE_GET_NAME(DSProxyStartTransfer).Probe, params, item.StartCycles);
                    orbit.AddProbe(&LWTRACE_GET_NAME(VDiskStartProcessing).Probe, params, item.TransferCycles);
                    orbit.AddProbe(&LWTRACE_GET_NAME(VDiskReply).Probe, params, item.VDiskReplyCycles);
                }
            }
        }
#endif //LWTRACE_DISABLE
    }

    ui64 RegisterCause(const TString& requestType = "") {
        if (IsOn && Items.size() < InvalidCauseIdx - 1) {
            Items.emplace_back(CurrentCauseIdx, GetCycleCountFast(), false, requestType);
            return Items.size() - 1;
        } else {
            return InvalidCauseIdx;
        }
    }

    ui64 RegisterAccelerate(const TString& requestType) {
        if (IsOn && Items.size() < InvalidCauseIdx - 1) {
            Items.emplace_back(CurrentCauseIdx, GetCycleCountFast(), true, requestType);
            return Items.size() - 1;
        } else {
            return InvalidCauseIdx;
        }
    }

    void OnAccelerate(ui64 causeIdx) {
        CurrentCauseIdx = causeIdx;
    }

    bool OnReply(ui64 causeIdx, double transferDuration, double vDiskDuration) {
        if (causeIdx < Items.size()) {
            CurrentCauseIdx = causeIdx;
            TRootCauseItem &item = Items[CurrentCauseIdx];
            item.VDiskReplyCycles = GetCycleCountFast();
            if (transferDuration + vDiskDuration > 0) {
                ui64 transferCycles = item.StartCycles + ui64((transferDuration / (transferDuration + vDiskDuration))
                        * (item.VDiskReplyCycles - item.StartCycles));
                item.TransferCycles = Max(item.StartCycles, Min(item.VDiskReplyCycles, transferCycles));
            } else {
                item.TransferCycles = item.VDiskReplyCycles;
            }
            return true;
        } else {
            return false;
        }
    }
};

}//NKikimr

