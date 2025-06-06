# Generated by devtools/yamaker.

LIBRARY()

VERSION(18.1.8)

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm18
    contrib/libs/llvm18/lib/Analysis
    contrib/libs/llvm18/lib/CodeGen
    contrib/libs/llvm18/lib/CodeGen/LowLevelType
    contrib/libs/llvm18/lib/CodeGen/SelectionDAG
    contrib/libs/llvm18/lib/IR
    contrib/libs/llvm18/lib/MC
    contrib/libs/llvm18/lib/Support
    contrib/libs/llvm18/lib/Target
    contrib/libs/llvm18/lib/Transforms/Utils
)

ADDINCL(
    contrib/libs/llvm18/lib/CodeGen/GlobalISel
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    CSEInfo.cpp
    CSEMIRBuilder.cpp
    CallLowering.cpp
    Combiner.cpp
    CombinerHelper.cpp
    GIMatchTableExecutor.cpp
    GISelChangeObserver.cpp
    GISelKnownBits.cpp
    GlobalISel.cpp
    IRTranslator.cpp
    InlineAsmLowering.cpp
    InstructionSelect.cpp
    InstructionSelector.cpp
    LegacyLegalizerInfo.cpp
    LegalityPredicates.cpp
    LegalizeMutations.cpp
    Legalizer.cpp
    LegalizerHelper.cpp
    LegalizerInfo.cpp
    LoadStoreOpt.cpp
    Localizer.cpp
    LostDebugLocObserver.cpp
    MachineIRBuilder.cpp
    RegBankSelect.cpp
    Utils.cpp
)

END()
