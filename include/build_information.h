// SPDX-License-Identifier: MIT
/*
 * H9 CAN build information
 *
 * Copyright (C) 2024 Kamil Pałkowski
 *
 */

#ifndef _BUILD_INFORMATION_H_
#define _BUILD_INFORMATION_H_

#include "build_information_gen.h"

#ifndef GIT_SHA
#define _GIT_SHA ""
#else
#define _GIT_SHA GIT_SHA
#endif

#ifndef DIRTY_WORKING_TREE_BUILD
#define _DIRTY_WORKING_TREE_BUILD ""
#define _SHORT_DIRTY_WORKING_TREE_BUILD ""
#else
#define _DIRTY_WORKING_TREE_BUILD "-dirty"
#define _SHORT_DIRTY_WORKING_TREE_BUILD "-D"
#endif

#define BUILD_INFORMATION_STR _GIT_SHA _DIRTY_WORKING_TREE_BUILD
#define BUILD_INFORMATION_SHORT_STR _GIT_SHA _SHORT_DIRTY_WORKING_TREE_BUILD

#endif /* _BUILD_INFORMATION_H_ */
