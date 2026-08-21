#pragma once

/*
 * UCX includes bfd.h only when HAVE_DETAILED_BACKTRACE is enabled. The
 * YTsaurus build intentionally disables that optional feature, but ymake's
 * include scanner still needs to resolve the guarded include.
 */
