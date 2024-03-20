// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) Contributors to the OpenEXR Project.

#pragma once

//
// Dealing with FPEs
//
//#define HAVE_UCONTEXT_H 0
#undef HAVE_UCONTEXT_H
#define IEX_HAVE_CONTROL_REGISTER_SUPPORT 1
/* #undef IEX_HAVE_SIGCONTEXT_CONTROL_REGISTER_SUPPORT */
