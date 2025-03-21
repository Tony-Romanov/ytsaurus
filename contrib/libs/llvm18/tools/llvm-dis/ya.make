# Generated by devtools/yamaker.

PROGRAM()

VERSION(18.1.8)

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm18
    contrib/libs/llvm18/lib/BinaryFormat
    contrib/libs/llvm18/lib/Bitcode/Reader
    contrib/libs/llvm18/lib/Bitstream/Reader
    contrib/libs/llvm18/lib/Demangle
    contrib/libs/llvm18/lib/IR
    contrib/libs/llvm18/lib/Remarks
    contrib/libs/llvm18/lib/Support
    contrib/libs/llvm18/lib/TargetParser
)

ADDINCL(
    contrib/libs/llvm18/tools/llvm-dis
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    llvm-dis.cpp
)

END()
