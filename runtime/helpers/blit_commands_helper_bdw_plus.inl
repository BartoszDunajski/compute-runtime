/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/blit_commands_helper_base.inl"

namespace NEO {

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsForBuffer(Buffer &dstBuffer, GraphicsAllocation &srcAllocation, typename GfxFamily::XY_COPY_BLT &blitCmd) {}

} // namespace NEO