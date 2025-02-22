// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_ENTRYPOINTS_H_
#define MOJO_EDK_EMBEDDER_ENTRYPOINTS_H_

#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/thunks.h"

namespace mojo {
namespace edk {

// Creates a MojoSystemThunks struct populated with the EDK's implementation of
// each function. This may be used by embedders to populate thunks for
// application loading.
MOJO_SYSTEM_IMPL_EXPORT MojoSystemThunks MakeSystemThunks();

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_ENTRYPOINTS_H_
