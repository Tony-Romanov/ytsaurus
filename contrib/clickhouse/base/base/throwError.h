#pragma once

/// Throw DB::Exception-like exception before its definition.
/// DB::Exception derived from DBPoco::Exception derived from std::exception.
/// DB::Exception generally caught as DBPoco::Exception. std::exception generally has other catch blocks and could lead to other outcomes.
/// DB::Exception is not defined yet. It'd better to throw DBPoco::Exception but we do not want to include any big header here, even <string>.
/// So we throw some std::exception instead in the hope its catch block is the same as DB::Exception one.
[[noreturn]] void throwError(const char * err);
