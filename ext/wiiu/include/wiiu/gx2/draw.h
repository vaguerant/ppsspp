#pragma once
#include <wiiu/types.h>
#include "enum.h"
#include <wiiu/os/memory.h>
#include <wiiu/os/debug.h>

#ifdef __cplusplus
extern "C" {
#endif

void GX2SetAttribBuffer(uint32_t index, uint32_t size, uint32_t stride, const void *buffer);

void GX2DrawEx(GX2PrimitiveMode mode,
               uint32_t count,
               uint32_t offset,
               uint32_t numInstances);

void GX2DrawEx2(GX2PrimitiveMode mode,
                uint32_t count,
                uint32_t offset,
                uint32_t numInstances,
                uint32_t baseInstance);

void GX2DrawIndexedEx(GX2PrimitiveMode mode,
                      uint32_t count,
                      GX2IndexType indexType,
                      void *indices,
                      uint32_t offset,
                      uint32_t numInstances);

void GX2DrawIndexedEx2(GX2PrimitiveMode mode,
                       uint32_t count,
                       GX2IndexType indexType,
                       void *indices,
                       uint32_t offset,
                       uint32_t numInstances,
                       uint32_t baseInstance);

void GX2DrawIndexedImmediateEx(GX2PrimitiveMode mode,
                               uint32_t count,
                               GX2IndexType indexType,
                               void *indices,
                               uint32_t offset,
                               uint32_t numInstances);

void GX2SetPrimitiveRestartIndex(uint32_t index);

#ifdef __cplusplus
}
#endif

#if 0
#include "event.h"
#define GX2DrawEx(mode,count,offset, numInstances) do{GX2DrawEx(mode,count,offset, numInstances); GX2DrawDone(); DEBUG_BREAK_ONCE();}while(0)
#define GX2DrawIndexedEx(mode,count,indexType,indices,offset,numInstances) do{GX2DrawIndexedEx(mode,count,indexType,indices,offset,numInstances); GX2DrawDone(); DEBUG_BREAK_ONCE();}while(0)
#endif
