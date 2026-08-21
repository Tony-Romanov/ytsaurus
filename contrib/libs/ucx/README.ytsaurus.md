# UCX integration in YTsaurus

This directory contains the official UCX 1.21.0 release archive from
https://github.com/openucx/ucx/releases/download/v1.21.0/ucx-1.21.0.tar.gz.
The archive SHA-256 is
`2374d2fcf3186fbfd5e27633ab153aabaeb6b4f503a88563d2aca67cf51ed2c1`.

The YTsaurus build provides the UCM, UCS, UCT, and UCP layers on Linux with
multi-thread support. The built-in `self`, `posix`, `sysv`, and `tcp`
transports are enabled. Optional transports and integrations that require
external SDKs or hardware libraries (IB/verbs/RDMA-CM, CUDA/GDRCopy, ROCm,
Level Zero, KNEM, XPMEM, and GAUDI) are not part of this target.

Consumers should add the following dependency to their `ya.make`:

```text
PEERDIR(
    contrib/libs/ucx
)
```

The smoke test in `ut` verifies that a multi-threaded UCP context and worker
can be created and that all four built-in transports are registered in a
statically linked binary.

## Files added or modified during the import

The files from the official UCX 1.21.0 archive were not modified. The
following YTsaurus integration files were added:

- `README.ytsaurus.md` — import provenance, supported features, usage, and
  integration change list.
- `ya.make` — YTsaurus library target and Linux source configuration.
- `config.h` — configure-time feature definitions adapted for the hermetic
  YTsaurus toolchain.
- `compat/bfd.h` — include-scanner compatibility stub for the disabled BFD
  backtrace integration.
- `compat/linux/rtnetlink.h` — wrapper selecting the target toolchain's Linux
  routing UAPI header.
- `ut/ya.make` — YTsaurus smoke-test target.
- `ut/smoke.cpp` — runtime verification of UCP context/worker creation and
  static registration of the `self`, `posix`, `sysv`, and `tcp` transports.
