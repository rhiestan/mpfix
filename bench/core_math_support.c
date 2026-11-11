/* Copyright 2026 The mpfix Authors
   SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */

/* CORE-MATH's lgamma sets the C library's signgam, which not every library
   has. bench/CMakeLists.txt builds lgamma.c with signgam renamed to this. */
int mpfix_bench_signgam;
