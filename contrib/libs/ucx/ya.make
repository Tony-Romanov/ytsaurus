LIBRARY()

VERSION(1.21.0)

ORIGINAL_SOURCE(https://github.com/openucx/ucx/releases/download/v1.21.0/ucx-1.21.0.tar.gz)

LICENSE(
    BSD-3-Clause AND
    CC-PDDC
)

LICENSE_TEXTS(LICENSE)

ADDINCL(
    contrib/libs/ucx/compat
    GLOBAL contrib/libs/ucx/src
    contrib/libs/ucx
    contrib/libs/ucx/src/uct
    contrib/libs/clang20-rt/include
)

PEERDIR(
    contrib/libs/libcap
    contrib/libs/linux-headers
)

NO_COMPILER_WARNINGS()

NO_RUNTIME()

NO_UTIL()

CFLAGS(
    -D_GNU_SOURCE
    -DHAVE_CONFIG_H
    -DCPU_FLAGS=
    -DUCM_MALLOC_PREFIX=ucm_dl
    -DUCX_MODULE_DIR=\"\"
    -DUCX_CONFIG_DIR=\"\"
    -DUSE_LOCKS=1
    -DMALLINFO_FIELD_TYPE=int
    -fno-strict-aliasing
)

IF (OS_LINUX)
    SRCS(
        src/ucm/event/event.c
        src/ucm/malloc/malloc_hook.c
        src/ucm/mmap/install.c
        src/ucm/util/replace.c
        src/ucm/util/log.c
        src/ucm/util/reloc.c
        src/ucm/util/sys.c
        src/ucm/bistro/bistro.c
        src/ucm/bistro/bistro_x86_64.c
        src/ucm/bistro/bistro_aarch64.c
        src/ucm/bistro/bistro_ppc64.c
        src/ucm/bistro/bistro_rv64.c
        src/ucm/ptmalloc286/malloc.c

        src/ucs/algorithm/crc.c
        src/ucs/algorithm/qsort_r.c
        src/ucs/algorithm/string_distance.c
        src/ucs/arch/aarch64/cpu.c
        src/ucs/arch/aarch64/global_opts.c
        src/ucs/arch/ppc64/timebase.c
        src/ucs/arch/ppc64/global_opts.c
        src/ucs/arch/rv64/cpu.c
        src/ucs/arch/rv64/global_opts.c
        src/ucs/arch/x86_64/cpu.c
        src/ucs/arch/x86_64/global_opts.c
        src/ucs/arch/cpu.c
        src/ucs/async/async.c
        src/ucs/async/signal.c
        src/ucs/async/pipe.c
        src/ucs/async/eventfd.c
        src/ucs/async/thread.c
        src/ucs/config/global_opts.c
        src/ucs/config/ucm_opts.c
        src/ucs/config/ini.c
        src/ucs/config/parser.c
        src/ucs/datastruct/arbiter.c
        src/ucs/datastruct/array.c
        src/ucs/datastruct/bitmap.c
        src/ucs/datastruct/callbackq.c
        src/ucs/datastruct/frag_list.c
        src/ucs/datastruct/interval_tree.c
        src/ucs/datastruct/lru.c
        src/ucs/datastruct/mpmc.c
        src/ucs/datastruct/mpool.c
        src/ucs/datastruct/mpool_set.c
        src/ucs/datastruct/pgtable.c
        src/ucs/datastruct/piecewise_func.c
        src/ucs/datastruct/ptr_array.c
        src/ucs/datastruct/ptr_map.c
        src/ucs/datastruct/strided_alloc.c
        src/ucs/datastruct/string_buffer.c
        src/ucs/datastruct/string_set.c
        src/ucs/datastruct/usage_tracker.c
        src/ucs/datastruct/conn_match.c
        src/ucs/debug/assert.c
        src/ucs/debug/debug.c
        src/ucs/debug/log.c
        src/ucs/debug/memtrack.c
        src/ucs/memory/memory_type.c
        src/ucs/memory/memtype_cache.c
        src/ucs/memory/numa.c
        src/ucs/memory/rcache.c
        src/ucs/memory/rcache_vfs.c
        src/ucs/profile/profile.c
        src/ucs/stats/stats.c
        src/ucs/sys/event_set.c
        GLOBAL src/ucs/sys/init.c
        src/ucs/sys/math.c
        src/ucs/sys/module.c
        src/ucs/sys/string.c
        src/ucs/sys/sys.c
        src/ucs/sys/iovec.c
        src/ucs/sys/lib.c
        src/ucs/sys/sock.c
        src/ucs/sys/topo/base/topo.c
        src/ucs/sys/stubs.c
        src/ucs/sys/netlink.c
        src/ucs/sys/uid.c
        src/ucs/time/time.c
        src/ucs/time/timer_wheel.c
        src/ucs/time/timerq.c
        src/ucs/type/class.c
        src/ucs/type/status.c
        src/ucs/type/spinlock.c
        src/ucs/type/thread_mode.c
        src/ucs/vfs/base/vfs_obj.c
        src/ucs/vfs/base/vfs_cb.c

        src/uct/base/uct_md.c
        src/uct/base/uct_md_vfs.c
        src/uct/base/uct_mem.c
        src/uct/base/uct_component.c
        src/uct/base/uct_iface.c
        src/uct/base/uct_iface_vfs.c
        src/uct/base/uct_worker.c
        src/uct/base/uct_cm.c
        src/uct/base/uct_vfs_attr.c
        src/uct/sm/base/sm_ep.c
        src/uct/sm/base/sm_md.c
        src/uct/sm/base/sm_iface.c
        src/uct/sm/mm/base/mm_iface.c
        src/uct/sm/mm/base/mm_ep.c
        src/uct/sm/mm/base/mm_md.c
        GLOBAL src/uct/sm/mm/posix/mm_posix.c
        GLOBAL src/uct/sm/mm/sysv/mm_sysv.c
        src/uct/sm/scopy/base/scopy_iface.c
        src/uct/sm/scopy/base/scopy_ep.c
        GLOBAL src/uct/sm/self/self.c
        src/uct/tcp/tcp_ep.c
        GLOBAL src/uct/tcp/tcp_iface.c
        src/uct/tcp/tcp_md.c
        src/uct/tcp/tcp_net.c
        src/uct/tcp/tcp_cm.c
        src/uct/tcp/tcp_base.c
        src/uct/tcp/tcp_sockcm.c
        src/uct/tcp/tcp_listener.c
        src/uct/tcp/tcp_sockcm_ep.c

        src/ucp/am/eager_single.c
        src/ucp/am/eager_multi.c
        src/ucp/am/rndv.c
        src/ucp/core/ucp_context.c
        src/ucp/core/ucp_am.c
        src/ucp/core/ucp_ep.c
        src/ucp/core/ucp_ep_vfs.c
        src/ucp/core/ucp_listener.c
        src/ucp/core/ucp_mm.c
        src/ucp/core/ucp_proxy_ep.c
        src/ucp/core/ucp_request.c
        src/ucp/core/ucp_rkey.c
        src/ucp/core/ucp_version.c
        src/ucp/core/ucp_vfs.c
        src/ucp/core/ucp_worker.c
        src/ucp/core/ucp_device.c
        src/ucp/dt/datatype_iter.c
        src/ucp/dt/dt_iov.c
        src/ucp/dt/dt_generic.c
        src/ucp/dt/dt.c
        src/ucp/proto/lane_type.c
        src/ucp/proto/proto_am.c
        src/ucp/proto/proto_init.c
        src/ucp/proto/proto_common.c
        src/ucp/proto/proto_debug.c
        src/ucp/proto/proto_perf.c
        src/ucp/proto/proto_reconfig.c
        src/ucp/proto/proto_multi.c
        src/ucp/proto/proto_select.c
        src/ucp/proto/proto_single.c
        src/ucp/proto/proto.c
        src/ucp/rma/amo_basic.c
        src/ucp/rma/amo_offload.c
        src/ucp/rma/amo_send.c
        src/ucp/rma/amo_sw.c
        src/ucp/rma/get_am.c
        src/ucp/rma/get_offload.c
        src/ucp/rma/put_am.c
        src/ucp/rma/put_offload.c
        src/ucp/rma/rma_basic.c
        src/ucp/rma/rma_send.c
        src/ucp/rma/rma_sw.c
        src/ucp/rma/flush.c
        src/ucp/rndv/proto_rndv.c
        src/ucp/rndv/rndv_am.c
        src/ucp/rndv/rndv_get.c
        src/ucp/rndv/rndv_ppln.c
        src/ucp/rndv/rndv_put.c
        src/ucp/rndv/rndv_rtr.c
        src/ucp/rndv/rndv_ats.c
        src/ucp/rndv/rndv_rkey_ptr.c
        src/ucp/rndv/rndv.c
        src/ucp/stream/stream_multi.c
        src/ucp/stream/stream_recv.c
        src/ucp/stream/stream_send.c
        src/ucp/tag/eager_multi.c
        src/ucp/tag/eager_rcv.c
        src/ucp/tag/eager_snd.c
        src/ucp/tag/eager_single.c
        src/ucp/tag/probe.c
        src/ucp/tag/tag_rndv.c
        src/ucp/tag/tag_match.c
        src/ucp/tag/tag_recv.c
        src/ucp/tag/tag_send.c
        src/ucp/tag/offload.c
        src/ucp/tag/offload/eager.c
        src/ucp/tag/offload/rndv.c
        src/ucp/wireup/address.c
        src/ucp/wireup/ep_match.c
        src/ucp/wireup/select.c
        src/ucp/wireup/wireup_ep.c
        src/ucp/wireup/wireup.c
        src/ucp/wireup/wireup_cm.c
    )
ENDIF()

END()

IF (OS_LINUX)
    RECURSE_FOR_TESTS(
        ut
    )
ENDIF()
