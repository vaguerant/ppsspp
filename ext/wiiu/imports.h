/* coreinit */
IMPORT_BEGIN(coreinit);

IMPORT(OSFatal);
IMPORT(OSDynLoad_Acquire);
IMPORT(OSDynLoad_FindExport);
IMPORT(OSDynLoad_Release);
IMPORT(OSSetExceptionCallback);
IMPORT(OSSetExceptionCallbackEx);
IMPORT(OSSetDABR);
IMPORT(OSSavesDone_ReadyToRelease);
IMPORT(OSInitMutex);
IMPORT(OSInitMutexEx);
IMPORT(OSLockMutex);
IMPORT(OSTryLockMutex);
IMPORT(OSUnlockMutex);
IMPORT(OSInitCond);
IMPORT(OSWaitCond);
IMPORT(OSSignalCond);
IMPORT(OSInitSpinLock);
IMPORT(OSUninterruptibleSpinLock_Acquire);
IMPORT(OSUninterruptibleSpinLock_Release);
IMPORT(OSFastMutex_Init);
IMPORT(OSFastMutex_Lock);
IMPORT(OSFastMutex_Unlock);
IMPORT(OSSleepTicks);
IMPORT(OSGetTitleID);
IMPORT(OSIsThreadTerminated);
IMPORT(OSSetThreadPriority);
IMPORT(OSSetThreadName);
IMPORT(OSCreateThread);
IMPORT(OSSetThreadCleanupCallback);
IMPORT(OSResumeThread);
IMPORT(OSIsThreadSuspended);
IMPORT(OSSuspendThread);
IMPORT(OSGetCurrentThread);
IMPORT(OSGetThreadSpecific);
IMPORT(OSSetThreadSpecific);
IMPORT(OSSetThreadDeallocator);
IMPORT(OSSetThreadRunQuantum);
IMPORT(OSExitThread);
IMPORT(OSJoinThread);
IMPORT(OSYieldThread);
IMPORT(OSDetachThread);
IMPORT(OSGetDefaultThread);
IMPORT(OSContinueThread);
IMPORT(OSGetCoreId);
IMPORT(OSIsMainCore);
IMPORT(PPCSetFpIEEEMode);
IMPORT(PPCSetFpNonIEEEMode);
IMPORT(OSGetSystemTime);
IMPORT(OSGetSystemTick);
IMPORT(OSGetTime);
IMPORT(OSGetSymbolName);
IMPORT(OSGetSharedData);
IMPORT(OSBlockMove);
IMPORT(OSBlockSet);
IMPORT(OSEffectiveToPhysical);
IMPORT(OSAllocFromSystem);
IMPORT(OSFreeToSystem);
IMPORT(OSAllocVirtAddr);
IMPORT(OSFreeVirtAddr);
IMPORT(OSQueryVirtAddr);
IMPORT(OSGetMapVirtAddrRange);
IMPORT(OSGetDataPhysAddrRange);
IMPORT(OSGetAvailPhysAddrRange);
IMPORT(OSMapMemory);
IMPORT(OSUnmapMemory);
IMPORT(OSInitSemaphore);
IMPORT(OSInitSemaphoreEx);
IMPORT(OSGetSemaphoreCount);
IMPORT(OSSignalSemaphore);
IMPORT(OSWaitSemaphore);
IMPORT(OSTryWaitSemaphore);
IMPORT(OSTestAndSetAtomic);
IMPORT(OSCreateAlarm);
IMPORT(OSSetAlarmUserData);
IMPORT(OSGetAlarmUserData);
IMPORT(OSSetAlarm);
IMPORT(OSCancelAlarm);
IMPORT(OSGetSystemInfo);

IMPORT(OSScreenInit);
IMPORT(OSScreenGetBufferSizeEx);
IMPORT(OSScreenSetBufferEx);
IMPORT(OSScreenClearBufferEx);
IMPORT(OSScreenFlipBuffersEx);
IMPORT(OSScreenPutFontEx);
IMPORT(OSScreenPutPixelEx);
IMPORT(OSScreenEnableEx);

IMPORT(exit);
IMPORT(_Exit);
IMPORT(OSConsoleWrite);
IMPORT(OSReport);
IMPORT(__os_snprintf);
IMPORT(DisassemblePPCRange);
IMPORT(DisassemblePPCOpcode);

IMPORT(DCInvalidateRange);
IMPORT(DCFlushRange);
IMPORT(DCFlushRangeNoSync);
IMPORT(DCStoreRange);
IMPORT(DCStoreRangeNoSync);
IMPORT(ICInvalidateRange);

IMPORT(__gh_errno_ptr);

IMPORT(MEMGetBaseHeapHandle);
IMPORT(MEMCreateExpHeapEx);
IMPORT(MEMDestroyExpHeap);
IMPORT(MEMAllocFromExpHeapEx);
IMPORT(MEMFreeToExpHeap);
IMPORT(MEMGetTotalFreeSizeForExpHeap);
IMPORT(MEMGetSizeForMBlockExpHeap);
IMPORT(MEMAllocFromFrmHeapEx);
IMPORT(MEMFreeToFrmHeap);
IMPORT(MEMGetAllocatableSizeForFrmHeapEx);

IMPORT(FSInit);
IMPORT(FSShutdown);
IMPORT(FSAddClient);
IMPORT(FSAddClientEx);
IMPORT(FSDelClient);
IMPORT(FSInitCmdBlock);
IMPORT(FSSetCmdPriority);
IMPORT(FSChangeDir);
IMPORT(FSGetFreeSpaceSize);
IMPORT(FSGetStat);
IMPORT(FSRemove);
IMPORT(FSOpenFile);
IMPORT(FSCloseFile);
IMPORT(FSOpenDir);
IMPORT(FSMakeDir);
IMPORT(FSReadDir);
IMPORT(FSRewindDir);
IMPORT(FSCloseDir);
IMPORT(FSGetStatFile);
IMPORT(FSReadFile);
IMPORT(FSWriteFile);
IMPORT(FSSetPosFile);
IMPORT(FSFlushFile);
IMPORT(FSTruncateFile);
IMPORT(FSRename);
IMPORT(FSGetMountSource);
IMPORT(FSMount);
IMPORT(FSUnmount);

IMPORT(IOS_Open);
IMPORT(IOS_Close);
IMPORT(IOS_Ioctl);
IMPORT(IOS_IoctlAsync);

IMPORT(IMIsAPDEnabled);
IMPORT(IMIsDimEnabled);
IMPORT(IMEnableAPD);
IMPORT(IMEnableDim);
IMPORT(IMDisableAPD);
IMPORT(IMDisableDim);

IMPORT_END();

/* nsysnet */
IMPORT_BEGIN(nsysnet);

IMPORT(socket_lib_init);
IMPORT(getaddrinfo);
IMPORT(freeaddrinfo);
IMPORT(getnameinfo);
IMPORT(inet_ntoa);
IMPORT(inet_ntop);
IMPORT(inet_aton);
IMPORT(inet_pton);
IMPORT(ntohl);
IMPORT(ntohs);
IMPORT(htonl);
IMPORT(htons);
IMPORT(accept);
IMPORT(bind);
IMPORT(socketclose);
IMPORT(connect);
IMPORT(getpeername);
IMPORT(getsockname);
IMPORT(getsockopt);
IMPORT(listen);
IMPORT(recv);
IMPORT(recvfrom);
IMPORT(send);
IMPORT(sendto);
IMPORT(setsockopt);
IMPORT(shutdown);
IMPORT(socket);
IMPORT(select);
IMPORT(socketlasterr);
IMPORT(gethostbyname);
IMPORT(gai_strerror);

IMPORT_END();

/* gx2 */
IMPORT_BEGIN(gx2);

IMPORT(GX2Invalidate);
IMPORT(GX2Init);
IMPORT(GX2GetSystemTVScanMode);
IMPORT(GX2CalcTVSize);
IMPORT(GX2SetTVBuffer);
IMPORT(GX2CalcDRCSize);
IMPORT(GX2SetDRCBuffer);
IMPORT(GX2CalcSurfaceSizeAndAlignment);
IMPORT(GX2CopySurface);
IMPORT(GX2CopySurfaceEx);
IMPORT(GX2InitColorBufferRegs);
IMPORT(GX2InitDepthBufferRegs);
IMPORT(GX2SetupContextStateEx);
IMPORT(GX2SetContextState);
IMPORT(GX2SetColorBuffer);
IMPORT(GX2SetDepthBuffer);
IMPORT(GX2SetViewport);
IMPORT(GX2SetScissor);
IMPORT(GX2SetDepthOnlyControl);
IMPORT(GX2InitDepthStencilControlReg);
IMPORT(GX2InitStencilMaskReg);
IMPORT(GX2SetColorControl);
IMPORT(GX2InitColorControlReg);
IMPORT(GX2InitTargetChannelMasksReg);
IMPORT(GX2SetBlendControl);
IMPORT(GX2InitBlendControlReg);
IMPORT(GX2SetBlendControlReg);
IMPORT(GX2SetColorControlReg);
IMPORT(GX2SetTargetChannelMasksReg);
IMPORT(GX2SetDepthStencilControlReg);
IMPORT(GX2SetBlendConstantColor);
IMPORT(GX2SetBlendConstantColorReg);
IMPORT(GX2SetCullOnlyControl);
IMPORT(GX2CalcFetchShaderSizeEx);
IMPORT(GX2InitFetchShaderEx);
IMPORT(GX2SetFetchShader);
IMPORT(GX2SetVertexShader);
IMPORT(GX2SetPixelShader);
IMPORT(GX2SetGeometryShader);
IMPORT(GX2SetGeometryUniformBlock);
IMPORT(GX2SetVertexUniformBlock);
IMPORT(GX2SetPixelUniformBlock);
IMPORT(GX2CalcGeometryShaderInputRingBufferSize);
IMPORT(GX2CalcGeometryShaderOutputRingBufferSize);
IMPORT(GX2SetGeometryShaderInputRingBuffer);
IMPORT(GX2SetGeometryShaderOutputRingBuffer);
IMPORT(GX2SetShaderModeEx);
IMPORT(GX2SetAttribBuffer);
IMPORT(GX2InitTextureRegs);
IMPORT(GX2InitSampler);
IMPORT(GX2InitSamplerBorderType);
IMPORT(GX2InitSamplerClamping);
IMPORT(GX2InitSamplerDepthCompare);
IMPORT(GX2InitSamplerLOD);
IMPORT(GX2InitSamplerXYFilter);
IMPORT(GX2InitSamplerZMFilter);
IMPORT(GX2SetPixelTexture);
IMPORT(GX2SetPixelSampler);
IMPORT(GX2SetVertexTexture);
IMPORT(GX2SetStencilMask);
IMPORT(GX2SetStencilMaskReg);
IMPORT(GX2ClearColor);
IMPORT(GX2ClearBuffersEx);
IMPORT(GX2ClearDepthStencilEx);
IMPORT(GX2CopyColorBufferToScanBuffer);
IMPORT(GX2SwapScanBuffers);
IMPORT(GX2Flush);
IMPORT(GX2WaitForVsync);
IMPORT(GX2SetTVEnable);
IMPORT(GX2SetDRCEnable);
IMPORT(GX2SetSwapInterval);
IMPORT(GX2DrawDone);
IMPORT(GX2Shutdown);
IMPORT(GX2DrawEx);
IMPORT(GX2DrawIndexedEx);
IMPORT(GX2DrawIndexedImmediateEx);
IMPORT(GX2WaitForFlip);
IMPORT(GX2GetSwapStatus);
IMPORT(GX2ResetGPU);
IMPORT(GX2AllocateTilingApertureEx);
IMPORT(GX2FreeTilingAperture);

IMPORT_END();

/* proc_ui */
IMPORT_BEGIN(proc_ui);

IMPORT(ProcUIInit);
IMPORT(ProcUIShutdown);

IMPORT_END();

/* sndcore2 */
IMPORT_BEGIN(sndcore2);

IMPORT(AXInitWithParams);
IMPORT(AXQuit);
IMPORT(AXRegisterFrameCallback);
IMPORT(AXAcquireMultiVoice);
IMPORT(AXSetMultiVoiceDeviceMix);
IMPORT(AXSetMultiVoiceOffsets);
IMPORT(AXSetMultiVoiceCurrentOffset);
IMPORT(AXSetMultiVoiceState);
IMPORT(AXSetMultiVoiceVe);
IMPORT(AXSetMultiVoiceSrcType);
IMPORT(AXSetMultiVoiceSrcRatio);
IMPORT(AXIsMultiVoiceRunning);
IMPORT(AXFreeMultiVoice);
IMPORT(AXGetVoiceOffsets);

IMPORT_END();

/* sysapp */
IMPORT_BEGIN(sysapp);

IMPORT(SYSRelaunchTitle);
IMPORT(SYSLaunchMenu);

IMPORT_END();

/* vpad */
IMPORT_BEGIN(vpad);

IMPORT(VPADInit);
IMPORT(VPADRead);
IMPORT(VPADSetSamplingCallback);
IMPORT(VPADGetTPCalibratedPoint);
IMPORT(VPADGetTPCalibratedPointEx);

IMPORT_END();

/* padscore */
IMPORT_BEGIN(padscore);

IMPORT(KPADInit);
IMPORT(WPADProbe);
IMPORT(WPADSetDataFormat);
IMPORT(WPADEnableURCC);
IMPORT(WPADEnableWiiRemote);
IMPORT(WPADRead);
IMPORT(KPADRead);
IMPORT(KPADReadEx);

IMPORT_END();

/* nsyskbd */
IMPORT_BEGIN(nsyskbd);

IMPORT(KBDSetup);
IMPORT(KBDTeardown);

IMPORT_END();
