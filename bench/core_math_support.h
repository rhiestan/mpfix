/* Copyright 2026 The mpfix Authors
   SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */

/* Included into CORE-MATH's lgamma.c, which assigns the C library's signgam.
   Not every library has it, so bench/CMakeLists.txt renames it to this. */
extern int mpfix_bench_signgam;
