// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <set>
#include <map>
#include <queue>
#include <algorithm>

#include "base/logging.h"

#include "Common/LogManager.h"
#include "Common/CommonTypes.h"
#include "Core/HLE/HLE.h"
#include "Core/HLE/HLETables.h"
#include "Core/MIPS/MIPSAnalyst.h"
#include "Core/MIPS/MIPSCodeUtils.h"
#include "Core/MIPS/MIPS.h"
#include "Core/CoreTiming.h"
#include "Core/MemMapHelpers.h"
#include "Core/MIPS/JitCommon/JitCommon.h"
#include "Core/Reporting.h"
#include "Common/ChunkFile.h"

#include "Core/HLE/sceAudio.h"
#include "Core/HLE/sceKernel.h"
#include "Core/HLE/sceKernelMemory.h"
#include "Core/HLE/sceKernelThread.h"
#include "Core/HLE/sceKernelModule.h"
#include "Core/HLE/sceKernelInterrupt.h"
#include "Core/HLE/KernelWaitHelpers.h"
#include "Core/HLE/ThreadQueueList.h"

typedef struct
{
	WaitType type;
	const char *name;
} WaitTypeNames;

const WaitTypeNames waitTypeNames[] = {
	{ WAITTYPE_NONE,            "None" },
	{ WAITTYPE_SLEEP,           "Sleep" },
	{ WAITTYPE_DELAY,           "Delay" },
	{ WAITTYPE_SEMA,            "Semaphore" },
	{ WAITTYPE_EVENTFLAG,       "Event flag", },
	{ WAITTYPE_MBX,             "MBX" },
	{ WAITTYPE_VPL,             "VPL" },
	{ WAITTYPE_FPL,             "FPL" },
	{ WAITTYPE_MSGPIPE,         "Message pipe" },
	{ WAITTYPE_THREADEND,       "Thread end" },
	{ WAITTYPE_AUDIOCHANNEL,    "Audio channel" },
	{ WAITTYPE_UMD,             "UMD" },
	{ WAITTYPE_VBLANK,          "VBlank" },
	{ WAITTYPE_MUTEX,           "Mutex" },
	{ WAITTYPE_LWMUTEX,         "LwMutex" },
	{ WAITTYPE_CTRL,            "Control" },
	{ WAITTYPE_IO,              "IO" },
	{ WAITTYPE_GEDRAWSYNC,      "GeDrawSync" },
	{ WAITTYPE_GELISTSYNC,      "GeListSync" },
	{ WAITTYPE_MODULE,          "Module" },
	{ WAITTYPE_HLEDELAY,        "HleDelay" },
	{ WAITTYPE_TLSPL,           "TLS" },
	{ WAITTYPE_VMEM,            "Volatile Mem" },
	{ WAITTYPE_ASYNCIO,         "AsyncIO" },
};

const char *getWaitTypeName(WaitType type)
{
	int waitTypeNamesAmount = sizeof(waitTypeNames)/sizeof(WaitTypeNames);

	for (int i = 0; i < waitTypeNamesAmount; i++)
	{
		if (waitTypeNames[i].type == type)
		{
			return waitTypeNames[i].name;
		}
	}

	return "Unknown";
}

enum ThreadEventType {
	THREADEVENT_CREATE = 1,
	THREADEVENT_START  = 2,
	THREADEVENT_EXIT   = 4,
	THREADEVENT_DELETE = 8,
	THREADEVENT_SUPPORTED = THREADEVENT_CREATE | THREADEVENT_START | THREADEVENT_EXIT | THREADEVENT_DELETE,
};

bool __KernelThreadTriggerEvent(bool isKernel, SceUID threadID, ThreadEventType type);

enum {
	PSP_THREAD_ATTR_KERNEL       = 0x00001000,
	PSP_THREAD_ATTR_VFPU         = 0x00004000,
	PSP_THREAD_ATTR_SCRATCH_SRAM = 0x00008000, // Save/restore scratch as part of context???
	PSP_THREAD_ATTR_NO_FILLSTACK = 0x00100000, // No filling of 0xff.
	PSP_THREAD_ATTR_CLEAR_STACK  = 0x00200000, // Clear thread stack when deleted.
	PSP_THREAD_ATTR_LOW_STACK    = 0x00400000, // Allocate stack from bottom not top.
	PSP_THREAD_ATTR_USER         = 0x80000000,
	PSP_THREAD_ATTR_USBWLAN      = 0xa0000000,
	PSP_THREAD_ATTR_VSH          = 0xc0000000,

	// TODO: Support more, not even sure what all of these mean.
	PSP_THREAD_ATTR_USER_MASK    = 0xf8f060ff,
	PSP_THREAD_ATTR_USER_ERASE   = 0x78800000,
	PSP_THREAD_ATTR_SUPPORTED    = (PSP_THREAD_ATTR_KERNEL | PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_NO_FILLSTACK | PSP_THREAD_ATTR_CLEAR_STACK | PSP_THREAD_ATTR_LOW_STACK | PSP_THREAD_ATTR_USER)
};

struct NativeCallback
{
	SceUInt_le size;
	char name[32];
	SceUID_le threadId;
	u32_le entrypoint;
	u32_le commonArgument;

	s32_le notifyCount;
	s32_le notifyArg;
};

class Callback : public KernelObject
{
public:
	const char *GetName() override { return nc.name; }
	const char *GetTypeName() override { return "CallBack"; }

	void GetQuickInfo(char *ptr, int size) override
	{
		sprintf(ptr, "thread=%i, argument= %08x",
			//hackAddress,
			nc.threadId,
			nc.commonArgument);
	}

	~Callback()
	{
	}

	static u32 GetMissingErrorCode() { return SCE_KERNEL_ERROR_UNKNOWN_CBID; }
	static int GetStaticIDType() { return SCE_KERNEL_TMID_Callback; }
	int GetIDType() const override { return SCE_KERNEL_TMID_Callback; }

	void DoState(PointerWrap &p) override
	{
		auto s = p.Section("Callback", 1);
		if (!s)
			return;

		p.Do(nc);
		// Saved values were moved to mips call, ignoring here.
		u32 legacySaved = 0;
		p.Do(legacySaved);
		p.Do(legacySaved);
		p.Do(legacySaved);
		p.Do(legacySaved);
		p.Do(legacySaved);
	}

	NativeCallback nc;
};

// Real PSP struct, don't change the fields.
struct SceKernelThreadRunStatus
{
	SceSize_le size;
	u32_le status;
	s32_le currentPriority;
	LEndian<WaitType> waitType;
	SceUID_le waitID;
	s32_le wakeupCount;
	SceKernelSysClock runForClocks;
	s32_le numInterruptPreempts;
	s32_le numThreadPreempts;
	s32_le numReleases;
};

// Real PSP struct, don't change the fields.
struct NativeThread
{
	u32_le nativeSize;
	char name[KERNELOBJECT_MAX_NAME_LENGTH+1];

	// Threading stuff
	u32_le attr;
	u32_le status;
	u32_le entrypoint;
	u32_le initialStack;
	u32_le stackSize;
	u32_le gpreg;

	s32_le initialPriority;
	s32_le currentPriority;
	LEndian<WaitType> waitType;
	SceUID_le waitID;
	s32_le wakeupCount;
	s32_le exitStatus;
	SceKernelSysClock runForClocks;
	s32_le numInterruptPreempts;
	s32_le numThreadPreempts;
	s32_le numReleases;
};

struct ThreadWaitInfo {
	u32 waitValue;
	u32 timeoutPtr;
};

// Owns outstanding MIPS calls and provides a way to get them by ID.
class MipsCallManager {
public:
	MipsCallManager() : idGen_(0) {}
	u32 add(MipsCall *call) {
		u32 id = genId();
		calls_.insert(std::pair<int, MipsCall *>(id, call));
		return id;
	}
	MipsCall *get(u32 id) {
		auto iter = calls_.find(id);
		if (iter == calls_.end())
			return NULL;
		return iter->second;
	}
	MipsCall *pop(u32 id) {
		MipsCall *temp = calls_[id];
		calls_.erase(id);
		return temp;
	}
	void clear() {
		for (auto it = calls_.begin(), end = calls_.end(); it != end; ++it) {
			delete it->second;
		}
		calls_.clear();
		types_.clear();
		idGen_ = 0;
	}

	int registerActionType(ActionCreator creator) {
		types_.push_back(creator);
		return (int) types_.size() - 1;
	}

	void restoreActionType(int actionType, ActionCreator creator) {
		if (actionType >= (int) types_.size())
			types_.resize(actionType + 1, NULL);
		types_[actionType] = creator;
	}

	Action *createActionByType(int actionType) {
		if (actionType < (int) types_.size() && types_[actionType] != NULL) {
			Action *a = types_[actionType]();
			a->actionTypeID = actionType;
			return a;
		}
		return NULL;
	}

	void DoState(PointerWrap &p) {
		auto s = p.Section("MipsCallManager", 1);
		if (!s)
			return;

		p.Do(calls_);
		p.Do(idGen_);
	}

private:
	u32 genId() { return ++idGen_; }
	std::map<u32, MipsCall *> calls_;
	std::vector<ActionCreator> types_;
	u32 idGen_;
};

class ActionAfterMipsCall : public Action
{
	ActionAfterMipsCall()
	{
		chainedAction = NULL;
	}

public:
	void run(MipsCall &call) override;

	static Action *Create()
	{
		return new ActionAfterMipsCall();
	}

	void DoState(PointerWrap &p) override
	{
		auto s = p.Section("ActionAfterMipsCall", 1);
		if (!s)
			return;

		p.Do(threadID);
		p.Do(status);
		p.Do(waitType);
		p.Do(waitID);
		p.Do(waitInfo);
		p.Do(isProcessingCallbacks);
		p.Do(currentCallbackId);

		int chainedActionType = 0;
		if (chainedAction != NULL)
			chainedActionType = chainedAction->actionTypeID;
		p.Do(chainedActionType);

		if (chainedActionType != 0)
		{
			if (p.mode == p.MODE_READ)
				chainedAction = __KernelCreateAction(chainedActionType);
			chainedAction->DoState(p);
		}
	}

	SceUID threadID;

	// Saved thread state
	int status;
	WaitType waitType;
	int waitID;
	ThreadWaitInfo waitInfo;
	bool isProcessingCallbacks;
	SceUID currentCallbackId;

	Action *chainedAction;
};

class ActionAfterCallback : public Action
{
public:
	ActionAfterCallback() {}
	void run(MipsCall &call) override;

	static Action *Create()
	{
		return new ActionAfterCallback;
	}

	void setCallback(SceUID cbId_)
	{
		cbId = cbId_;
	}

	void DoState(PointerWrap &p) override
	{
		auto s = p.Section("ActionAfterCallback", 1);
		if (!s)
			return;

		p.Do(cbId);
	}

	SceUID cbId;
};

class Thread : public KernelObject
{
public:
	const char *GetName() override { return nt.name; }
	const char *GetTypeName() override { return "Thread"; }
	void GetQuickInfo(char *ptr, int size) override
	{
		sprintf(ptr, "pc= %08x sp= %08x %s %s %s %s %s %s (wt=%i wid=%i wv= %08x )",
			context.pc, context.r[MIPS_REG_SP],
			(nt.status & THREADSTATUS_RUNNING) ? "RUN" : "", 
			(nt.status & THREADSTATUS_READY) ? "READY" : "", 
			(nt.status & THREADSTATUS_WAIT) ? "WAIT" : "", 
			(nt.status & THREADSTATUS_SUSPEND) ? "SUSPEND" : "", 
			(nt.status & THREADSTATUS_DORMANT) ? "DORMANT" : "",
			(nt.status & THREADSTATUS_DEAD) ? "DEAD" : "",
			(int)nt.waitType,
			nt.waitID,
			waitInfo.waitValue);
	}

	static u32 GetMissingErrorCode() { return SCE_KERNEL_ERROR_UNKNOWN_THID; }
	static int GetStaticIDType() { return SCE_KERNEL_TMID_Thread; }
	int GetIDType() const override { return SCE_KERNEL_TMID_Thread; }

	bool AllocateStack(u32 &stackSize)
	{
		FreeStack();

		bool fromTop = (nt.attr & PSP_THREAD_ATTR_LOW_STACK) == 0;
		if (nt.attr & PSP_THREAD_ATTR_KERNEL)
		{
			// Allocate stacks for kernel threads (idle) in kernel RAM
			currentStack.start = kernelMemory.Alloc(stackSize, fromTop, (std::string("stack/") + nt.name).c_str());
		}
		else
		{
			currentStack.start = userMemory.Alloc(stackSize, fromTop, (std::string("stack/") + nt.name).c_str());
		}
		if (currentStack.start == (u32)-1)
		{
			currentStack.start = 0;
			nt.initialStack = 0;
			ERROR_LOG(SCEKERNEL, "Failed to allocate stack for thread");
			return false;
		}

		nt.initialStack = currentStack.start;
		nt.stackSize = stackSize;
		return true;
	}

	bool FillStack() {
		// Fill the stack.
		if ((nt.attr & PSP_THREAD_ATTR_NO_FILLSTACK) == 0) {
			Memory::Memset(currentStack.start, 0xFF, nt.stackSize);
		}
		context.r[MIPS_REG_SP] = currentStack.start + nt.stackSize;
		currentStack.end = context.r[MIPS_REG_SP];
		// The k0 section is 256 bytes at the top of the stack.
		context.r[MIPS_REG_SP] -= 256;
		context.r[MIPS_REG_K0] = context.r[MIPS_REG_SP];
		u32 k0 = context.r[MIPS_REG_K0];
		Memory::Memset(k0, 0, 0x100);
		Memory::Write_U32(GetUID(),        k0 + 0xc0);
		Memory::Write_U32(nt.initialStack, k0 + 0xc8);
		Memory::Write_U32(0xffffffff,      k0 + 0xf8);
		Memory::Write_U32(0xffffffff,      k0 + 0xfc);
		// After k0 comes the arguments, which is done by sceKernelStartThread().

		Memory::Write_U32(GetUID(), nt.initialStack);
		return true;
	}

	void FreeStack() {
		if (currentStack.start != 0) {
			DEBUG_LOG(SCEKERNEL, "Freeing thread stack %s", nt.name);

			if ((nt.attr & PSP_THREAD_ATTR_CLEAR_STACK) != 0 && nt.initialStack != 0) {
				Memory::Memset(nt.initialStack, 0, nt.stackSize);
			}

			if (nt.attr & PSP_THREAD_ATTR_KERNEL) {
				kernelMemory.Free(currentStack.start);
			} else {
				userMemory.Free(currentStack.start);
			}
			currentStack.start = 0;
		}
	}

	bool PushExtendedStack(u32 size)
	{
		u32 stack = userMemory.Alloc(size, true, (std::string("extended/") + nt.name).c_str());
		if (stack == (u32)-1)
			return false;

		pushedStacks.push_back(currentStack);
		currentStack.start = stack;
		currentStack.end = stack + size;
		nt.initialStack = currentStack.start;
		nt.stackSize = currentStack.end - currentStack.start;

		// We still drop the threadID at the bottom and fill it, but there's no k0.
		Memory::Memset(currentStack.start, 0xFF, nt.stackSize);
		Memory::Write_U32(GetUID(), nt.initialStack);
		return true;
	}

	bool PopExtendedStack()
	{
		if (pushedStacks.size() == 0)
			return false;

		userMemory.Free(currentStack.start);
		currentStack = pushedStacks.back();
		pushedStacks.pop_back();
		nt.initialStack = currentStack.start;
		nt.stackSize = currentStack.end - currentStack.start;
		return true;
	}

	Thread()
	{
		currentStack.start = 0;
	}

	// Can't use a destructor since savestates will call that too.
	void Cleanup()
	{
		// Callbacks are automatically deleted when their owning thread is deleted.
		for (auto it = callbacks.begin(), end = callbacks.end(); it != end; ++it)
			kernelObjects.Destroy<Callback>(*it);

		if (pushedStacks.size() != 0)
		{
			WARN_LOG_REPORT(SCEKERNEL, "Thread ended within an extended stack");
			for (size_t i = 0; i < pushedStacks.size(); ++i)
				userMemory.Free(pushedStacks[i].start);
		}
		FreeStack();
	}

	void setReturnValue(u32 retval);
	void setReturnValue(u64 retval);
	void resumeFromWait();
	bool isWaitingFor(WaitType type, int id) const;
	int getWaitID(WaitType type) const;
	ThreadWaitInfo getWaitInfo() const;

	// Utils
	inline bool isRunning() const { return (nt.status & THREADSTATUS_RUNNING) != 0; }
	inline bool isStopped() const { return (nt.status & THREADSTATUS_DORMANT) != 0; }
	inline bool isReady() const { return (nt.status & THREADSTATUS_READY) != 0; }
	inline bool isWaiting() const { return (nt.status & THREADSTATUS_WAIT) != 0; }
	inline bool isSuspended() const { return (nt.status & THREADSTATUS_SUSPEND) != 0; }

	void DoState(PointerWrap &p) override
	{
		auto s = p.Section("Thread", 1, 5);
		if (!s)
			return;

		p.Do(nt);
		p.Do(waitInfo);
		p.Do(moduleId);
		p.Do(isProcessingCallbacks);
		p.Do(currentMipscallId);
		p.Do(currentCallbackId);

		// TODO: How do I "version" adding a DoState method to ThreadContext?
		p.Do(context);

		if (s <= 3)
		{
			// We must have been loading an old state if we're here.
			// Reorder VFPU data to new order.
			float temp[128];
			memcpy(temp, context.v, 128 * sizeof(float));
			for (int i = 0; i < 128; i++) {
				context.v[voffset[i]] = temp[i];
			}
		}

		if (s <= 2)
		{
			context.other[4] = context.other[5];
			context.other[3] = context.other[4];
		}
		if (s <= 4)
			std::swap(context.hi, context.lo);

		p.Do(callbacks);

		p.Do(pendingMipsCalls);
		p.Do(pushedStacks);
		p.Do(currentStack);

		if (s >= 2)
		{
			p.Do(waitingThreads);
			p.Do(pausedWaits);
		}
	}

	NativeThread nt;

	ThreadWaitInfo waitInfo;
	SceUID moduleId;

	bool isProcessingCallbacks;
	u32 currentMipscallId;
	SceUID currentCallbackId;

	ThreadContext context;

	std::vector<SceUID> callbacks;

	std::list<u32> pendingMipsCalls;

	struct StackInfo {
		u32 start;
		u32 end;
	};
	// This is a stack of... stacks, since sceKernelExtendThreadStack() can recurse.
	// These are stacks that aren't "active" right now, but will pop off once the func returns.
	std::vector<StackInfo> pushedStacks;

	StackInfo currentStack;

	// For thread end.
	std::vector<SceUID> waitingThreads;
	// Key is the callback id it was for, or if no callback, the thread id.
	std::map<SceUID, u64> pausedWaits;
};

struct WaitTypeFuncs
{
	WaitBeginCallbackFunc beginFunc;
	WaitEndCallbackFunc endFunc;
};

bool __KernelExecuteMipsCallOnCurrentThread(u32 callId, bool reschedAfter);

Thread *__KernelCreateThread(SceUID &id, SceUID moduleID, const char *name, u32 entryPoint, u32 priority, int stacksize, u32 attr);
void __KernelResetThread(Thread *t, int lowestPriority);
void __KernelCancelWakeup(SceUID threadID);
void __KernelCancelThreadEndTimeout(SceUID threadID);
bool __KernelCheckThreadCallbacks(Thread *thread, bool force);

//////////////////////////////////////////////////////////////////////////
//STATE BEGIN
//////////////////////////////////////////////////////////////////////////
int g_inCbCount = 0;
// Normally, the same as currentThread.  In an interrupt, remembers the callback's thread id.
SceUID currentCallbackThreadID = 0;
int readyCallbacksCount = 0;
SceUID currentThread;
Thread *currentThreadPtr;
u32 idleThreadHackAddr;
u32 threadReturnHackAddr;
u32 cbReturnHackAddr;
u32 intReturnHackAddr;
u32 extendReturnHackAddr;
u32 moduleReturnHackAddr;
std::vector<ThreadCallback> threadEndListeners;

typedef std::vector<SceUID> ThreadEventHandlerList;
static std::map<SceUID, ThreadEventHandlerList> threadEventHandlers;
static std::vector<SceUID> pendingDeleteThreads;

// Lists all thread ids that aren't deleted/etc.
std::vector<SceUID> threadqueue;

// Lists only ready thread ids.
ThreadQueueList threadReadyQueue;

SceUID threadIdleID[2];

int eventScheduledWakeup;
int eventThreadEndTimeout;

bool dispatchEnabled = true;

MipsCallManager mipsCalls;
int actionAfterCallback;
int actionAfterMipsCall;

// When inside a callback, delays are "paused", and rechecked after the callback returns.
std::map<SceUID, u64> pausedDelays;

// Doesn't need state saving.
WaitTypeFuncs waitTypeFuncs[NUM_WAITTYPES];

// Doesn't really need state saving, just for logging purposes.
static u64 lastSwitchCycles = 0;

//////////////////////////////////////////////////////////////////////////
//STATE END
//////////////////////////////////////////////////////////////////////////

int __KernelRegisterActionType(ActionCreator creator)
{
	return mipsCalls.registerActionType(creator);
}

void __KernelRestoreActionType(int actionType, ActionCreator creator)
{
	mipsCalls.restoreActionType(actionType, creator);
}

Action *__KernelCreateAction(int actionType)
{
	return mipsCalls.createActionByType(actionType);
}

void MipsCall::DoState(PointerWrap &p)
{
	auto s = p.Section("MipsCall", 1);
	if (!s)
		return;

	p.Do(entryPoint);
	p.Do(cbId);
	p.DoArray(args, ARRAY_SIZE(args));
	p.Do(numArgs);
	// No longer used.
	u32 legacySavedIdRegister = 0;
	p.Do(legacySavedIdRegister);
	u32 legacySavedRa = 0;
	p.Do(legacySavedRa);
	p.Do(savedPc);
	p.Do(savedV0);
	p.Do(savedV1);
	p.Do(tag);
	p.Do(savedId);
	p.Do(reschedAfter);

	int actionTypeID = 0;
	if (doAfter != NULL)
		actionTypeID = doAfter->actionTypeID;
	p.Do(actionTypeID);
	if (actionTypeID != 0)
	{
		if (p.mode == p.MODE_READ)
			doAfter = __KernelCreateAction(actionTypeID);
		doAfter->DoState(p);
	}
}

void MipsCall::setReturnValue(u32 value)
{
	savedV0 = value;
}

void MipsCall::setReturnValue(u64 value)
{
	savedV0 = value & 0xFFFFFFFF;
	savedV1 = (value >> 32) & 0xFFFFFFFF;
}

inline Thread *__GetCurrentThread() {
	return currentThreadPtr;
}

inline void __SetCurrentThread(Thread *thread, SceUID threadID, const char *name) {
	currentThread = threadID;
	currentThreadPtr = thread;
	hleCurrentThreadName = name;
}

u32 __KernelMipsCallReturnAddress() {
	return cbReturnHackAddr;
}

u32 __KernelInterruptReturnAddress() {
	return intReturnHackAddr;
}

static void __KernelDelayBeginCallback(SceUID threadID, SceUID prevCallbackId) {
	SceUID pauseKey = prevCallbackId == 0 ? threadID : prevCallbackId;

	u32 error;
	SceUID waitID = __KernelGetWaitID(threadID, WAITTYPE_DELAY, error);
	if (waitID == threadID) {
		// Most waits need to keep track of waiting threads, delays don't.  Use a fake list.
		std::vector<SceUID> dummy;
		HLEKernel::WaitBeginCallback(threadID, prevCallbackId, eventScheduledWakeup, dummy, pausedDelays, true);
		DEBUG_LOG(SCEKERNEL, "sceKernelDelayThreadCB: Suspending delay for callback");
	}
	else
		WARN_LOG_REPORT(SCEKERNEL, "sceKernelDelayThreadCB: beginning callback with bad wait?");
}

static void __KernelDelayEndCallback(SceUID threadID, SceUID prevCallbackId) {
	SceUID pauseKey = prevCallbackId == 0 ? threadID : prevCallbackId;

	if (pausedDelays.find(pauseKey) == pausedDelays.end())
	{
		// This probably should not happen.
		WARN_LOG_REPORT(SCEKERNEL, "sceKernelDelayThreadCB: cannot find delay deadline");
		__KernelResumeThreadFromWait(threadID, 0);
		return;
	}

	u64 delayDeadline = pausedDelays[pauseKey];
	pausedDelays.erase(pauseKey);

	// TODO: Don't wake up if __KernelCurHasReadyCallbacks()?

	s64 cyclesLeft = delayDeadline - CoreTiming::GetTicks();
	if (cyclesLeft < 0)
		__KernelResumeThreadFromWait(threadID, 0);
	else
	{
		CoreTiming::ScheduleEvent(cyclesLeft, eventScheduledWakeup, __KernelGetCurThread());
		DEBUG_LOG(SCEKERNEL, "sceKernelDelayThreadCB: Resuming delay after callback");
	}
}

static void __KernelSleepBeginCallback(SceUID threadID, SceUID prevCallbackId) {
	DEBUG_LOG(SCEKERNEL, "sceKernelSleepThreadCB: Suspending sleep for callback");
}

static void __KernelSleepEndCallback(SceUID threadID, SceUID prevCallbackId) {
	u32 error;
	Thread *thread = kernelObjects.Get<Thread>(threadID, error);
	if (!thread) {
		// This probably should not happen.
		WARN_LOG_REPORT(SCEKERNEL, "sceKernelSleepThreadCB: thread deleted?");
		return;
	}

	// TODO: Don't wake up if __KernelCurHasReadyCallbacks()?

	if (thread->nt.wakeupCount > 0) {
		thread->nt.wakeupCount--;
		DEBUG_LOG(SCEKERNEL, "sceKernelSleepThreadCB: resume from callback, wakeupCount decremented to %i", thread->nt.wakeupCount);
		__KernelResumeThreadFromWait(threadID, 0);
	} else {
		DEBUG_LOG(SCEKERNEL, "sceKernelSleepThreadCB: Resuming sleep after callback");
	}
}

static void __KernelThreadEndBeginCallback(SceUID threadID, SceUID prevCallbackId)
{
	auto result = HLEKernel::WaitBeginCallback<Thread, WAITTYPE_THREADEND, SceUID>(threadID, prevCallbackId, eventThreadEndTimeout);
	if (result == HLEKernel::WAIT_CB_SUCCESS)
		DEBUG_LOG(SCEKERNEL, "sceKernelWaitThreadEndCB: Suspending wait for callback");
	else if (result == HLEKernel::WAIT_CB_BAD_WAIT_DATA)
		ERROR_LOG_REPORT(SCEKERNEL, "sceKernelWaitThreadEndCB: wait not found to pause for callback");
	else
		WARN_LOG_REPORT(SCEKERNEL, "sceKernelWaitThreadEndCB: beginning callback with bad wait id?");
}

static bool __KernelCheckResumeThreadEnd(Thread *t, SceUID waitingThreadID, u32 &error, int result, bool &wokeThreads)
{
	if (!HLEKernel::VerifyWait(waitingThreadID, WAITTYPE_THREADEND, t->GetUID()))
		return true;

	if (t->nt.status == THREADSTATUS_DORMANT)
	{
		u32 timeoutPtr = __KernelGetWaitTimeoutPtr(waitingThreadID, error);
		s64 cyclesLeft = CoreTiming::UnscheduleEvent(eventThreadEndTimeout, waitingThreadID);
		if (timeoutPtr != 0)
			Memory::Write_U32((u32) cyclesToUs(cyclesLeft), timeoutPtr);
		s32 exitStatus = t->nt.exitStatus;
		__KernelResumeThreadFromWait(waitingThreadID, exitStatus);
		return true;
	}

	return false;
}

static void __KernelThreadEndEndCallback(SceUID threadID, SceUID prevCallbackId)
{
	auto result = HLEKernel::WaitEndCallback<Thread, WAITTYPE_THREADEND, SceUID>(threadID, prevCallbackId, eventThreadEndTimeout, __KernelCheckResumeThreadEnd);
	if (result == HLEKernel::WAIT_CB_RESUMED_WAIT)
		DEBUG_LOG(SCEKERNEL, "sceKernelWaitThreadEndCB: Resuming wait from callback");
}

u32 __KernelSetThreadRA(SceUID threadID, u32 nid)
{
	u32 newRA;
	switch (nid)
	{
	case NID_MODULERETURN:
		newRA = moduleReturnHackAddr;
		break;
	default:
		ERROR_LOG_REPORT(SCEKERNEL, "__KernelSetThreadRA(): invalid RA address");
		return -1;
	}

	if (threadID == currentThread)
		currentMIPS->r[MIPS_REG_RA] = newRA;
	else
	{
		u32 error;
		Thread *thread = kernelObjects.Get<Thread>(threadID, error);
		if (!thread)
			return error;

		thread->context.r[MIPS_REG_RA] = newRA;
	}

	return 0;
}

void hleScheduledWakeup(u64 userdata, int cyclesLate);
void hleThreadEndTimeout(u64 userdata, int cyclesLate);

static void __KernelWriteFakeSysCall(u32 nid, u32 *ptr, u32 &pos)
{
	*ptr = pos;
	pos += 8;
	WriteSyscall("FakeSysCalls", nid, *ptr);
	MIPSAnalyst::PrecompileFunction(*ptr, 8);
}

void __KernelThreadingInit()
{
	struct ThreadHack
	{
		u32 nid;
		u32 *addr;
	};

	// Yeah, this is straight out of JPCSP, I should be ashamed.
	const static u32_le idleThreadCode[] = {
		MIPS_MAKE_LUI(MIPS_REG_RA, 0x0800),
		MIPS_MAKE_JR_RA(),
		MIPS_MAKE_SYSCALL("FakeSysCalls", "_sceKernelIdle"),
		MIPS_MAKE_BREAK(0),
	};

	// If you add another func here, don't forget __KernelThreadingDoState() below.
	static ThreadHack threadHacks[] = {
		{NID_THREADRETURN, &threadReturnHackAddr},
		{NID_CALLBACKRETURN, &cbReturnHackAddr},
		{NID_INTERRUPTRETURN, &intReturnHackAddr},
		{NID_EXTENDRETURN, &extendReturnHackAddr},
		{NID_MODULERETURN, &moduleReturnHackAddr},
	};
	u32 blockSize = sizeof(idleThreadCode) + ARRAY_SIZE(threadHacks) * 2 * 4;  // The thread code above plus 8 bytes per "hack"

	dispatchEnabled = true;
	memset(waitTypeFuncs, 0, sizeof(waitTypeFuncs));

	__SetCurrentThread(NULL, 0, NULL);
	g_inCbCount = 0;
	currentCallbackThreadID = 0;
	readyCallbacksCount = 0;
	lastSwitchCycles = 0;
	idleThreadHackAddr = kernelMemory.Alloc(blockSize, false, "threadrethack");

	Memory::Memcpy(idleThreadHackAddr, idleThreadCode, sizeof(idleThreadCode));

	u32 pos = idleThreadHackAddr + sizeof(idleThreadCode);
	for (size_t i = 0; i < ARRAY_SIZE(threadHacks); ++i) {
		__KernelWriteFakeSysCall(threadHacks[i].nid, threadHacks[i].addr, pos);
	}

	eventScheduledWakeup = CoreTiming::RegisterEvent("ScheduledWakeup", &hleScheduledWakeup);
	eventThreadEndTimeout = CoreTiming::RegisterEvent("ThreadEndTimeout", &hleThreadEndTimeout);
	actionAfterMipsCall = __KernelRegisterActionType(ActionAfterMipsCall::Create);
	actionAfterCallback = __KernelRegisterActionType(ActionAfterCallback::Create);

	// Create the two idle threads, as well. With the absolute minimal possible priority.
	// 4096 stack size - don't know what the right value is. Hm, if callbacks are ever to run on these threads...
	__KernelResetThread(__KernelCreateThread(threadIdleID[0], 0, "idle0", idleThreadHackAddr, 0x7f, 4096, PSP_THREAD_ATTR_KERNEL), 0);
	__KernelResetThread(__KernelCreateThread(threadIdleID[1], 0, "idle1", idleThreadHackAddr, 0x7f, 4096, PSP_THREAD_ATTR_KERNEL), 0);
	// These idle threads are later started in LoadExec, which calls __KernelStartIdleThreads below.

	__KernelListenThreadEnd(__KernelCancelWakeup);
	__KernelListenThreadEnd(__KernelCancelThreadEndTimeout);

	__KernelRegisterWaitTypeFuncs(WAITTYPE_DELAY, __KernelDelayBeginCallback, __KernelDelayEndCallback);
	__KernelRegisterWaitTypeFuncs(WAITTYPE_SLEEP, __KernelSleepBeginCallback, __KernelSleepEndCallback);
	__KernelRegisterWaitTypeFuncs(WAITTYPE_THREADEND, __KernelThreadEndBeginCallback, __KernelThreadEndEndCallback);
}

void __KernelThreadingDoState(PointerWrap &p)
{
	auto s = p.Section("sceKernelThread", 1, 3);
	if (!s)
		return;

	p.Do(g_inCbCount);
	p.Do(currentCallbackThreadID);
	p.Do(readyCallbacksCount);
	p.Do(idleThreadHackAddr);
	p.Do(threadReturnHackAddr);
	p.Do(cbReturnHackAddr);
	p.Do(intReturnHackAddr);
	p.Do(extendReturnHackAddr);
	p.Do(moduleReturnHackAddr);

	p.Do(currentThread);
	SceUID dv = 0;
	p.Do(threadqueue, dv);
	p.DoArray(threadIdleID, ARRAY_SIZE(threadIdleID));
	p.Do(dispatchEnabled);

	p.Do(threadReadyQueue);

	p.Do(eventScheduledWakeup);
	CoreTiming::RestoreRegisterEvent(eventScheduledWakeup, "ScheduledWakeup", &hleScheduledWakeup);
	p.Do(eventThreadEndTimeout);
	CoreTiming::RestoreRegisterEvent(eventThreadEndTimeout, "ThreadEndTimeout", &hleThreadEndTimeout);
	p.Do(actionAfterMipsCall);
	__KernelRestoreActionType(actionAfterMipsCall, ActionAfterMipsCall::Create);
	p.Do(actionAfterCallback);
	__KernelRestoreActionType(actionAfterCallback, ActionAfterCallback::Create);

	p.Do(pausedDelays);

	__SetCurrentThread(kernelObjects.GetFast<Thread>(currentThread), currentThread, __KernelGetThreadName(currentThread));
	lastSwitchCycles = CoreTiming::GetTicks();

	if (s >= 2)
		p.Do(threadEventHandlers);
	if (s >= 3)
		p.Do(pendingDeleteThreads);
}

void __KernelThreadingDoStateLate(PointerWrap &p)
{
	// We do this late to give modules time to register actions.
	mipsCalls.DoState(p);
	p.DoMarker("sceKernelThread Late");
}

KernelObject *__KernelThreadObject()
{
	return new Thread;
}

KernelObject *__KernelCallbackObject()
{
	return new Callback;
}

void __KernelListenThreadEnd(ThreadCallback callback)
{
	threadEndListeners.push_back(callback);
}

static void __KernelFireThreadEnd(SceUID threadID)
{
	for (auto iter = threadEndListeners.begin(), end = threadEndListeners.end(); iter != end; ++iter)
	{
		ThreadCallback cb = *iter;
		cb(threadID);
	}
}

// TODO: Use __KernelChangeThreadState instead?  It has other affects...
static void __KernelChangeReadyState(Thread *thread, SceUID threadID, bool ready)
{
	// Passing the id as a parameter is just an optimization, if it's wrong it will cause havoc.
	_dbg_assert_msg_(SCEKERNEL, thread->GetUID() == threadID, "Incorrect threadID");
	int prio = thread->nt.currentPriority;

	if (thread->isReady())
	{
		if (!ready)
			threadReadyQueue.remove(prio, threadID);
	}
	else if (ready)
	{
		if (thread->isRunning())
			threadReadyQueue.push_front(prio, threadID);
		else
			threadReadyQueue.push_back(prio, threadID);
		thread->nt.status = THREADSTATUS_READY;
	}
}

static void __KernelChangeReadyState(SceUID threadID, bool ready)
{
	u32 error;
	Thread *thread = kernelObjects.Get<Thread>(threadID, error);
	if (thread)
		__KernelChangeReadyState(thread, threadID, ready);
	else
		WARN_LOG(SCEKERNEL, "Trying to change the ready state of an unknown thread?");
}

void __KernelStartIdleThreads(SceUID moduleId)
{
	for (int i = 0; i < 2; i++)
	{
		u32 error;
		Thread *t = kernelObjects.Get<Thread>(threadIdleID[i], error);
		t->nt.gpreg = __KernelGetModuleGP(moduleId);
		t->context.r[MIPS_REG_GP] = t->nt.gpreg;
		//t->context.pc += 4;	// ADJUSTPC
		threadReadyQueue.prepare(t->nt.currentPriority);
		__KernelChangeReadyState(t, threadIdleID[i], true);
	}
}

bool __KernelSwitchOffThread(const char *reason)
{
	if (!reason)
		reason = "switch off thread";

	SceUID threadID = currentThread;

	if (threadID != threadIdleID[0] && threadID != threadIdleID[1])
	{
		Thread *current = __GetCurrentThread();
		if (current && current->isRunning())
			__KernelChangeReadyState(current, threadID, true);

		// Idle 0 chosen entirely arbitrarily.
		Thread *t = kernelObjects.GetFast<Thread>(threadIdleID[0]);
		if (t)
		{
			hleSkipDeadbeef();
			__KernelSwitchContext(t, reason);
			return true;
		}
		else
			ERROR_LOG(SCEKERNEL, "Unable to switch to idle thread.");
	}

	return false;
}

bool __KernelSwitchToThread(SceUID threadID, const char *reason)
{
	if (!reason)
		reason = "switch to thread";

	if (currentThread != threadIdleID[0] && currentThread != threadIdleID[1])
	{
		ERROR_LOG_REPORT(SCEKERNEL, "__KernelSwitchToThread used when already on a thread.");
		return false;
	}

	if (currentThread == threadID)
		return false;

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (!t)
	{
		ERROR_LOG_REPORT(SCEKERNEL, "__KernelSwitchToThread: %x doesn't exist", threadID);
		hleReSchedule("switch to deleted thread");
	}
	else if (t->isReady() || t->isRunning())
	{
		Thread *current = __GetCurrentThread();
		if (current && current->isRunning())
			__KernelChangeReadyState(current, currentThread, true);

		__KernelSwitchContext(t, reason);
		return true;
	}
	else
	{
		hleReSchedule("switch to waiting thread");
	}

	return false;
}

void __KernelIdle()
{
	// Don't skip 0xDEADBEEF here, this is called directly bypassing CallSyscall().
	// That means the hle flag would stick around until the next call.

	CoreTiming::Idle();
	// We Advance within __KernelReSchedule(), so anything that has now happened after idle
	// will be triggered properly upon reschedule.
	__KernelReSchedule("idle");
}

void __KernelThreadingShutdown()
{
	kernelMemory.Free(threadReturnHackAddr);
	threadqueue.clear();
	threadReadyQueue.clear();
	threadEndListeners.clear();
	mipsCalls.clear();
	threadReturnHackAddr = 0;
	cbReturnHackAddr = 0;
	__SetCurrentThread(NULL, 0, NULL);
	intReturnHackAddr = 0;
	pausedDelays.clear();
	threadEventHandlers.clear();
	pendingDeleteThreads.clear();
}

const char *__KernelGetThreadName(SceUID threadID)
{
	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
		return t->nt.name;
	return "ERROR";
}

u32 __KernelGetWaitValue(SceUID threadID, u32 &error)
{
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		return t->getWaitInfo().waitValue;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "__KernelGetWaitValue ERROR: thread %i", threadID);
		return 0;
	}
}

u32 __KernelGetWaitTimeoutPtr(SceUID threadID, u32 &error)
{
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		return t->getWaitInfo().timeoutPtr;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "__KernelGetWaitTimeoutPtr ERROR: thread %i", threadID);
		return 0;
	}
}

SceUID __KernelGetWaitID(SceUID threadID, WaitType type, u32 &error)
{
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		return t->getWaitID(type);
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "__KernelGetWaitID ERROR: thread %i", threadID);
		return -1;
	}
}

SceUID __KernelGetCurrentCallbackID(SceUID threadID, u32 &error)
{
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
		return t->currentCallbackId;
	else
	{
		ERROR_LOG(SCEKERNEL, "__KernelGetCurrentCallbackID ERROR: thread %i", threadID);
		return 0;
	}
}

u32 sceKernelReferThreadStatus(u32 threadID, u32 statusPtr)
{
	static const u32 THREADINFO_SIZE = 104;
	static const u32 THREADINFO_SIZE_AFTER_260 = 108;

	if (threadID == 0)
		threadID = __KernelGetCurThread();

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (!t)
	{
		ERROR_LOG(SCEKERNEL, "%08x=sceKernelReferThreadStatus(%i, %08x): bad thread", error, threadID, statusPtr);
		return error;
	}

	u32 wantedSize = Memory::Read_U32(statusPtr);

	if (sceKernelGetCompiledSdkVersion() > 0x02060010)
	{
		if (wantedSize > THREADINFO_SIZE_AFTER_260)
		{
			ERROR_LOG(SCEKERNEL, "%08x=sceKernelReferThreadStatus(%i, %08x): bad size %d", SCE_KERNEL_ERROR_ILLEGAL_SIZE, threadID, statusPtr, wantedSize);
			return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		}

		VERBOSE_LOG(SCEKERNEL, "sceKernelReferThreadStatus(%i, %08x)", threadID, statusPtr);

		t->nt.nativeSize = THREADINFO_SIZE_AFTER_260;
		if (wantedSize != 0)
			Memory::Memcpy(statusPtr, &t->nt, std::min(wantedSize, (u32)sizeof(t->nt)));
		// TODO: What is this value?  Basic tests show 0...
		if (wantedSize > sizeof(t->nt))
			Memory::Memset(statusPtr + sizeof(t->nt), 0, wantedSize - sizeof(t->nt));
	}
	else
	{
		VERBOSE_LOG(SCEKERNEL, "sceKernelReferThreadStatus(%i, %08x)", threadID, statusPtr);

		t->nt.nativeSize = THREADINFO_SIZE;
		u32 sz = std::min(THREADINFO_SIZE, wantedSize);
		if (sz != 0)
			Memory::Memcpy(statusPtr, &t->nt, sz);
	}

	hleEatCycles(1220);
	hleReSchedule("refer thread status");
	return 0;
}

// Thanks JPCSP
u32 sceKernelReferThreadRunStatus(u32 threadID, u32 statusPtr)
{
	if (threadID == 0)
		threadID = __KernelGetCurThread();

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (!t)
	{
		ERROR_LOG(SCEKERNEL,"sceKernelReferThreadRunStatus Error %08x", error);
		return error;
	}

	DEBUG_LOG(SCEKERNEL,"sceKernelReferThreadRunStatus(%i, %08x)", threadID, statusPtr);
	if (!Memory::IsValidAddress(statusPtr))
		return -1;

	auto runStatus = PSPPointer<SceKernelThreadRunStatus>::Create(statusPtr);

	// TODO: Check size?
	runStatus->size = sizeof(SceKernelThreadRunStatus);
	runStatus->status = t->nt.status;
	runStatus->currentPriority = t->nt.currentPriority;
	runStatus->waitType = t->nt.waitType;
	runStatus->waitID = t->nt.waitID;
	runStatus->wakeupCount = t->nt.wakeupCount;
	runStatus->runForClocks = t->nt.runForClocks;
	runStatus->numInterruptPreempts = t->nt.numInterruptPreempts;
	runStatus->numThreadPreempts = t->nt.numThreadPreempts;
	runStatus->numReleases = t->nt.numReleases;

	return 0;
}

int sceKernelGetThreadExitStatus(SceUID threadID)
{
	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		if (t->nt.status == THREADSTATUS_DORMANT) // TODO: can be dormant before starting, too, need to avoid that
		{
			DEBUG_LOG(SCEKERNEL,"sceKernelGetThreadExitStatus(%i)", threadID);
			return t->nt.exitStatus;
		}
		else
		{
			DEBUG_LOG(SCEKERNEL,"sceKernelGetThreadExitStatus(%i): not dormant", threadID);
			return SCE_KERNEL_ERROR_NOT_DORMANT;
		}
	}
	else
	{
		ERROR_LOG(SCEKERNEL,"sceKernelGetThreadExitStatus Error %08x", error);
		return SCE_KERNEL_ERROR_UNKNOWN_THID;
	}
}

u32 sceKernelGetThreadmanIdType(u32 uid) {
	int type;
	if (kernelObjects.GetIDType(uid, &type)) {
		if (type < 0x1000) {
			DEBUG_LOG(SCEKERNEL, "%i=sceKernelGetThreadmanIdType(%i)", type, uid);
			return type;
		} else {
			// This means a partition memory block or module, etc.
			ERROR_LOG(SCEKERNEL, "sceKernelGetThreadmanIdType(%i): invalid object type %i", uid, type);
			return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
		}
	} else {
		ERROR_LOG(SCEKERNEL, "sceKernelGetThreadmanIdType(%i) - FAILED", uid);
		return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
	}
}

static bool __ThreadmanIdListIsSleeping(const Thread *t) {
	return t->isWaitingFor(WAITTYPE_SLEEP, 0);
}

static bool __ThreadmanIdListIsDelayed(const Thread *t) {
	return t->isWaitingFor(WAITTYPE_DELAY, t->GetUID());
}

static bool __ThreadmanIdListIsSuspended(const Thread *t) {
	return t->isSuspended();
}

static bool __ThreadmanIdListIsDormant(const Thread *t) {
	return t->isStopped();
}

u32 sceKernelGetThreadmanIdList(u32 type, u32 readBufPtr, u32 readBufSize, u32 idCountPtr) {
	if (readBufSize >= 0x8000000) {
		// Not exact, it's probably if the sum ends up negative or something.
		ERROR_LOG_REPORT(SCEKERNEL, "sceKernelGetThreadmanIdList(%i, %08x, %i, %08x): invalid size", type, readBufPtr, readBufSize, idCountPtr);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}
	if (!Memory::IsValidAddress(readBufPtr) && readBufSize > 0) {
		// Crashes on a PSP.
		ERROR_LOG_REPORT(SCEKERNEL, "sceKernelGetThreadmanIdList(%i, %08x, %i, %08x): invalid pointer", type, readBufPtr, readBufSize, idCountPtr);
		return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
	}

	u32 total = 0;
	auto uids = PSPPointer<SceUID_le>::Create(readBufPtr);
	u32 error;
	if (type > 0 && type <= SCE_KERNEL_TMID_Tlspl) {
		DEBUG_LOG(SCEKERNEL, "sceKernelGetThreadmanIdList(%i, %08x, %i, %08x)", type, readBufPtr, readBufSize, idCountPtr);
		total = kernelObjects.ListIDType(type, uids, readBufSize);
	} else if (type >= SCE_KERNEL_TMID_SleepThread && type <= SCE_KERNEL_TMID_DormantThread) {
		bool (*checkFunc)(const Thread *t) = NULL;
		switch (type) {
		case SCE_KERNEL_TMID_SleepThread:
			checkFunc = &__ThreadmanIdListIsSleeping;
			break;

		case SCE_KERNEL_TMID_DelayThread:
			checkFunc = &__ThreadmanIdListIsDelayed;
			break;

		case SCE_KERNEL_TMID_SuspendThread:
			checkFunc = &__ThreadmanIdListIsSuspended;
			break;

		case SCE_KERNEL_TMID_DormantThread:
			checkFunc = &__ThreadmanIdListIsDormant;
			break;

		default:
			_dbg_assert_msg_(SCEKERNEL, false, "Unexpected type %d", type);
		}

		for (size_t i = 0; i < threadqueue.size(); i++) {
			const Thread *t = kernelObjects.Get<Thread>(threadqueue[i], error);
			if (checkFunc(t)) {
				if (total < readBufSize) {
					*uids++ = threadqueue[i];
				}
				++total;
			}
		}
	} else {
		ERROR_LOG_REPORT(SCEKERNEL, "sceKernelGetThreadmanIdList(%i, %08x, %i, %08x): invalid type", type, readBufPtr, readBufSize, idCountPtr);
		return SCE_KERNEL_ERROR_ILLEGAL_TYPE;
	}

	if (Memory::IsValidAddress(idCountPtr)) {
		Memory::Write_U32(total, idCountPtr);
	}
	return total > readBufSize ? readBufSize : total;
}

// Saves the current CPU context
void __KernelSaveContext(ThreadContext *ctx, bool vfpuEnabled)
{
	// r and f are immediately next to each other and must be.
	memcpy((void *)ctx->r, (void *)currentMIPS->r, sizeof(ctx->r) + sizeof(ctx->f));

	if (vfpuEnabled)
	{
		memcpy(ctx->v, currentMIPS->v, sizeof(ctx->v));
		memcpy(ctx->vfpuCtrl, currentMIPS->vfpuCtrl, sizeof(ctx->vfpuCtrl));
	}

	memcpy(ctx->other, currentMIPS->other, sizeof(ctx->other));
}

// Loads a CPU context
void __KernelLoadContext(ThreadContext *ctx, bool vfpuEnabled)
{
	// r and f are immediately next to each other and must be.
	memcpy((void *)currentMIPS->r, (void *)ctx->r, sizeof(ctx->r) + sizeof(ctx->f));

	if (vfpuEnabled)
	{
		memcpy(currentMIPS->v, ctx->v, sizeof(ctx->v));
		memcpy(currentMIPS->vfpuCtrl, ctx->vfpuCtrl, sizeof(ctx->vfpuCtrl));
	}

	memcpy(currentMIPS->other, ctx->other, sizeof(ctx->other));
	if (MIPSComp::jit) {
		// When thread switching, we must update the rounding mode if cached in the jit.
		MIPSComp::jit->UpdateFCR31();
	}

	// Reset the llBit, the other thread may have touched memory.
	currentMIPS->llBit = 0;
}

u32 __KernelResumeThreadFromWait(SceUID threadID, u32 retval)
{
	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		t->resumeFromWait();
		t->setReturnValue(retval);
		return 0;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "__KernelResumeThreadFromWait(%d): bad thread: %08x", threadID, error);
		return error;
	}
}

u32 __KernelResumeThreadFromWait(SceUID threadID, u64 retval)
{
	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		t->resumeFromWait();
		t->setReturnValue(retval);
		return 0;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "__KernelResumeThreadFromWait(%d): bad thread: %08x", threadID, error);
		return error;
	}
}

// makes the current thread wait for an event
void __KernelWaitCurThread(WaitType type, SceUID waitID, u32 waitValue, u32 timeoutPtr, bool processCallbacks, const char *reason)
{
	if (!dispatchEnabled)
	{
		WARN_LOG_REPORT(SCEKERNEL, "Ignoring wait, dispatching disabled... right thing to do?");
		return;
	}

	Thread *thread = __GetCurrentThread();
	thread->nt.waitID = waitID;
	thread->nt.waitType = type;
	__KernelChangeThreadState(thread, ThreadStatus(THREADSTATUS_WAIT | (thread->nt.status & THREADSTATUS_SUSPEND)));
	thread->nt.numReleases++;
	thread->waitInfo.waitValue = waitValue;
	thread->waitInfo.timeoutPtr = timeoutPtr;

	// TODO: time waster
	if (!reason)
		reason = "started wait";

	hleReSchedule(processCallbacks, reason);
}

void __KernelWaitCallbacksCurThread(WaitType type, SceUID waitID, u32 waitValue, u32 timeoutPtr)
{
	if (!dispatchEnabled)
	{
		WARN_LOG_REPORT(SCEKERNEL, "Ignoring wait, dispatching disabled... right thing to do?");
		return;
	}

	Thread *thread = __GetCurrentThread();
	thread->nt.waitID = waitID;
	thread->nt.waitType = type;
	__KernelChangeThreadState(thread, ThreadStatus(THREADSTATUS_WAIT | (thread->nt.status & THREADSTATUS_SUSPEND)));
	// TODO: Probably not...?
	thread->nt.numReleases++;
	thread->waitInfo.waitValue = waitValue;
	thread->waitInfo.timeoutPtr = timeoutPtr;

	__KernelForceCallbacks();
}

void hleScheduledWakeup(u64 userdata, int cyclesLate)
{
	SceUID threadID = (SceUID)userdata;
	u32 error;
	if (__KernelGetWaitID(threadID, WAITTYPE_DELAY, error) == threadID)
	{
		__KernelResumeThreadFromWait(threadID, 0);
		__KernelReSchedule("thread delay finished");
	}
}

void __KernelScheduleWakeup(SceUID threadID, s64 usFromNow)
{
	s64 cycles = usToCycles(usFromNow);
	CoreTiming::ScheduleEvent(cycles, eventScheduledWakeup, threadID);
}

void __KernelCancelWakeup(SceUID threadID)
{
	CoreTiming::UnscheduleEvent(eventScheduledWakeup, threadID);
}

void hleThreadEndTimeout(u64 userdata, int cyclesLate)
{
	SceUID threadID = (SceUID) userdata;
	HLEKernel::WaitExecTimeout<Thread, WAITTYPE_THREADEND>(threadID);
}

static void __KernelScheduleThreadEndTimeout(SceUID threadID, SceUID waitForID, s64 usFromNow)
{
	s64 cycles = usToCycles(usFromNow);
	CoreTiming::ScheduleEvent(cycles, eventThreadEndTimeout, threadID);
}

void __KernelCancelThreadEndTimeout(SceUID threadID)
{
	CoreTiming::UnscheduleEvent(eventThreadEndTimeout, threadID);
}

static void __KernelRemoveFromThreadQueue(SceUID threadID)
{
	int prio = __KernelGetThreadPrio(threadID);
	if (prio != 0)
		threadReadyQueue.remove(prio, threadID);

	threadqueue.erase(std::remove(threadqueue.begin(), threadqueue.end(), threadID), threadqueue.end());
}

void __KernelStopThread(SceUID threadID, int exitStatus, const char *reason)
{
	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		__KernelChangeReadyState(t, threadID, false);
		t->nt.exitStatus = exitStatus;
		t->nt.status = THREADSTATUS_DORMANT;
		__KernelFireThreadEnd(threadID);
		for (size_t i = 0; i < t->waitingThreads.size(); ++i)
		{
			const SceUID waitingThread = t->waitingThreads[i];
			u32 timeoutPtr = __KernelGetWaitTimeoutPtr(waitingThread, error);
			if (HLEKernel::VerifyWait(waitingThread, WAITTYPE_THREADEND, threadID))
			{
				s64 cyclesLeft = CoreTiming::UnscheduleEvent(eventThreadEndTimeout, waitingThread);
				if (timeoutPtr != 0)
					Memory::Write_U32((u32) cyclesToUs(cyclesLeft), timeoutPtr);

				HLEKernel::ResumeFromWait(waitingThread, WAITTYPE_THREADEND, threadID, exitStatus);
			}
		}
		t->waitingThreads.clear();

		// Stopped threads are never waiting.
		t->nt.waitType = WAITTYPE_NONE;
		t->nt.waitID = 0;
	}
	else
		ERROR_LOG_REPORT(SCEKERNEL, "__KernelStopThread: thread %d does not exist", threadID);
}

u32 __KernelDeleteThread(SceUID threadID, int exitStatus, const char *reason)
{
	__KernelStopThread(threadID, exitStatus, reason);
	__KernelRemoveFromThreadQueue(threadID);

	if (currentThread == threadID)
		__SetCurrentThread(NULL, 0, NULL);
	if (currentCallbackThreadID == threadID)
	{
		currentCallbackThreadID = 0;
		g_inCbCount = 0;
	}

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		for (auto it = t->callbacks.begin(), end = t->callbacks.end(); it != end; ++it)
		{
			Callback *callback = kernelObjects.Get<Callback>(*it, error);
			if (callback && callback->nc.notifyCount != 0)
				readyCallbacksCount--;
		}

		t->Cleanup();

		// Before triggering, set v0.  It'll be restored if one is called.
		RETURN(error);
		t->nt.status = THREADSTATUS_DEAD;

		if (__KernelThreadTriggerEvent((t->nt.attr & PSP_THREAD_ATTR_KERNEL) != 0, threadID, THREADEVENT_DELETE)) {
			// Don't delete it yet.  We'll delete later.
			pendingDeleteThreads.push_back(threadID);
			return 0;
		} else {
			return kernelObjects.Destroy<Thread>(threadID);
		}
	} else {
		RETURN(error);
		return error;
	}
}

static void __ReportThreadQueueEmpty() {
	// We failed to find a thread to schedule.
	// This means something horrible happened to the idle threads.
	u32 error;
	Thread *idleThread0 = kernelObjects.Get<Thread>(threadIdleID[0], error);
	Thread *idleThread1 = kernelObjects.Get<Thread>(threadIdleID[1], error);

	char idleDescription0[256];
	int idleStatus0 = -1;
	if (idleThread0) {
		idleThread0->GetQuickInfo(idleDescription0, sizeof(idleDescription0));
		idleStatus0 = idleThread0->nt.status;
	} else {
		sprintf(idleDescription0, "DELETED");
	}

	char idleDescription1[256];
	int idleStatus1 = -1;
	if (idleThread1) {
		idleThread1->GetQuickInfo(idleDescription1, sizeof(idleDescription1));
		idleStatus1 = idleThread1->nt.status;
	} else {
		sprintf(idleDescription1, "DELETED");
	}

	ERROR_LOG_REPORT_ONCE(threadqueueempty, SCEKERNEL, "Failed to reschedule: out of threads on queue (%d, %d)", idleStatus0, idleStatus1);
	WARN_LOG(SCEKERNEL, "Failed to reschedule: idle0 -> %s", idleDescription0);
	WARN_LOG(SCEKERNEL, "Failed to reschedule: idle1 -> %s", idleDescription1);
}

// Returns NULL if the current thread is fine.
static Thread *__KernelNextThread() {
	SceUID bestThread;

	// If the current thread is running, it's a valid candidate.
	Thread *cur = __GetCurrentThread();
	if (cur && cur->isRunning()) {
		bestThread = threadReadyQueue.pop_first_better(cur->nt.currentPriority);
		if (bestThread != 0)
			__KernelChangeReadyState(cur, currentThread, true);
	} else {
		bestThread = threadReadyQueue.pop_first();

		if (bestThread == 0) {
			// Zoinks.  No thread?
			__ReportThreadQueueEmpty();

			// Let's try to get back on track, if possible.
			bestThread = threadIdleID[1];
		}
	}

	// Assume threadReadyQueue has not become corrupt.
	if (bestThread != 0)
		return kernelObjects.GetFast<Thread>(bestThread);
	else
		return 0;
}

void __KernelReSchedule(const char *reason)
{
	// First, let's check if there are any pending callbacks to trigger.
	// TODO: Could probably take this out of __KernelReSchedule() which is a bit hot.
	__KernelCheckCallbacks();

	// Execute any pending events while we're doing scheduling.
	CoreTiming::Advance();
	if (__IsInInterrupt() || !__KernelIsDispatchEnabled()) {
		// Threads don't get changed within interrupts or while dispatch is disabled.
		reason = "In Interrupt Or Callback";
		return;
	}

	Thread *nextThread = __KernelNextThread();
	if (nextThread) {
		__KernelSwitchContext(nextThread, reason);
	}
	// Otherwise, no need to switch.
}

void __KernelReSchedule(bool doCallbacks, const char *reason)
{
	Thread *thread = __GetCurrentThread();
	if (doCallbacks && thread != nullptr) {
		thread->isProcessingCallbacks = doCallbacks;
	}

	// Note - this calls the function above, not this one. Overloading...
	__KernelReSchedule(reason);
	if (doCallbacks && thread != nullptr && thread->GetUID() == currentThread) {
		if (thread->isRunning()) {
			thread->isProcessingCallbacks = false;
		}
	}
}

int sceKernelCheckThreadStack()
{
	u32 error;
	Thread *t = kernelObjects.Get<Thread>(__KernelGetCurThread(), error);
	if (t) {
		u32 diff = labs((long)((s64)currentMIPS->r[MIPS_REG_SP] - (s64)t->currentStack.start));
		DEBUG_LOG(SCEKERNEL, "%i=sceKernelCheckThreadStack()", diff);
		return diff;
	} else {
		ERROR_LOG_REPORT(SCEKERNEL, "sceKernelCheckThreadStack() - not on thread");
		return -1;
	}
}

void ThreadContext::reset()
{
	for (int i = 0; i<32; i++)
	{
		r[i] = 0xDEADBEEF;
		fi[i] = 0x7f800001;
	}
	r[0] = 0;
	for (int i = 0; i<128; i++)
	{
		vi[i] = 0x7f800001;
	}
	for (int i = 0; i<15; i++)
	{
		vfpuCtrl[i] = 0x00000000;
	}
	vfpuCtrl[VFPU_CTRL_SPREFIX] = 0xe4; // neutral
	vfpuCtrl[VFPU_CTRL_TPREFIX] = 0xe4; // neutral
	vfpuCtrl[VFPU_CTRL_DPREFIX] = 0x0;	// neutral
	vfpuCtrl[VFPU_CTRL_CC] = 0x3f;
	vfpuCtrl[VFPU_CTRL_INF4] = 0;
	vfpuCtrl[VFPU_CTRL_REV] = 0x7772ceab;
	vfpuCtrl[VFPU_CTRL_RCX0] = 0x3f800001;
	vfpuCtrl[VFPU_CTRL_RCX1] = 0x3f800002;
	vfpuCtrl[VFPU_CTRL_RCX2] = 0x3f800004;
	vfpuCtrl[VFPU_CTRL_RCX3] = 0x3f800008;
	vfpuCtrl[VFPU_CTRL_RCX4] = 0x3f800000;
	vfpuCtrl[VFPU_CTRL_RCX5] = 0x3f800000;
	vfpuCtrl[VFPU_CTRL_RCX6] = 0x3f800000;
	vfpuCtrl[VFPU_CTRL_RCX7] = 0x3f800000;
	fpcond = 0;
	fcr31 = 0x00000e00;
	hi = 0xDEADBEEF;
	lo = 0xDEADBEEF;
}

void __KernelResetThread(Thread *t, int lowestPriority)
{
	t->context.reset();
	t->context.pc = t->nt.entrypoint;

	// If the thread would be better than lowestPriority, reset to its initial.  Yes, kinda odd...
	if (t->nt.currentPriority < lowestPriority)
		t->nt.currentPriority = t->nt.initialPriority;

	t->nt.waitType = WAITTYPE_NONE;
	t->nt.waitID = 0;
	memset(&t->waitInfo, 0, sizeof(t->waitInfo));

	t->nt.exitStatus = SCE_KERNEL_ERROR_NOT_DORMANT;
	t->isProcessingCallbacks = false;
	t->currentCallbackId = 0;
	t->currentMipscallId = 0;
	t->pendingMipsCalls.clear();

	// This will be overwritten when starting the thread, but let's point it somewhere useful.
	t->context.r[MIPS_REG_RA] = threadReturnHackAddr;
	// TODO: Not sure if it's reset here, but this makes sense.
	t->context.r[MIPS_REG_GP] = t->nt.gpreg;
	t->FillStack();

	if (!t->waitingThreads.empty())
		ERROR_LOG_REPORT(SCEKERNEL, "Resetting thread with threads waiting on end?");
}

Thread *__KernelCreateThread(SceUID &id, SceUID moduleId, const char *name, u32 entryPoint, u32 priority, int stacksize, u32 attr)
{
	Thread *t = new Thread;
	id = kernelObjects.Create(t);

	threadqueue.push_back(id);
	threadReadyQueue.prepare(priority);

	memset(&t->nt, 0xCD, sizeof(t->nt));

	t->nt.entrypoint = entryPoint;
	t->nt.nativeSize = sizeof(t->nt);
	t->nt.attr = attr;
	// TODO: I have no idea what this value is but the PSP firmware seems to add it on create.
	t->nt.attr |= 0xFF;
	t->nt.initialPriority = t->nt.currentPriority = priority;
	t->nt.stackSize = stacksize;
	t->nt.status = THREADSTATUS_DORMANT;

	t->nt.numInterruptPreempts = 0;
	t->nt.numReleases = 0;
	t->nt.numThreadPreempts = 0;
	t->nt.runForClocks.lo = 0;
	t->nt.runForClocks.hi = 0;
	t->nt.wakeupCount = 0;
	t->nt.initialStack = 0;
	t->nt.waitID = 0;
	t->nt.exitStatus = SCE_KERNEL_ERROR_DORMANT;
	t->nt.waitType = WAITTYPE_NONE;

	if (moduleId)
		t->nt.gpreg = __KernelGetModuleGP(moduleId);
	else
		t->nt.gpreg = 0;  // sceKernelStartThread will take care of this.
	t->moduleId = moduleId;

	strncpy(t->nt.name, name, KERNELOBJECT_MAX_NAME_LENGTH);
	t->nt.name[KERNELOBJECT_MAX_NAME_LENGTH] = '\0';

	u32 stackSize = t->nt.stackSize;
	t->AllocateStack(stackSize);  // can change the stacksize!
	t->nt.stackSize = stackSize;
	return t;
}

SceUID __KernelSetupRootThread(SceUID moduleID, int args, const char *argp, int prio, int stacksize, int attr) 
{
	//grab mips regs
	SceUID id;
	Thread *thread = __KernelCreateThread(id, moduleID, "root", currentMIPS->pc, prio, stacksize, attr);
	if (thread->currentStack.start == 0)
		ERROR_LOG_REPORT(SCEKERNEL, "Unable to allocate stack for root thread.");
	__KernelResetThread(thread, 0);

	Thread *prevThread = __GetCurrentThread();
	if (prevThread && prevThread->isRunning())
		__KernelChangeReadyState(currentThread, true);
	__SetCurrentThread(thread, id, "root");
	thread->nt.status = THREADSTATUS_RUNNING; // do not schedule

	strcpy(thread->nt.name, "root");

	__KernelLoadContext(&thread->context, (attr & PSP_THREAD_ATTR_VFPU) != 0);
	currentMIPS->r[MIPS_REG_A0] = args;
	currentMIPS->r[MIPS_REG_SP] -= (args + 0xf) & ~0xf;
	u32 location = currentMIPS->r[MIPS_REG_SP];
	currentMIPS->r[MIPS_REG_A1] = location;
	if (argp)
		Memory::Memcpy(location, argp, args);
	// Let's assume same as starting a new thread, 64 bytes for safety/kernel.
	currentMIPS->r[MIPS_REG_SP] -= 64;

	return id;
}

SceUID __KernelCreateThreadInternal(const char *threadName, SceUID moduleID, u32 entry, u32 prio, int stacksize, u32 attr)
{
	SceUID id;
	Thread *newThread = __KernelCreateThread(id, moduleID, threadName, entry, prio, stacksize, attr);
	if (newThread->currentStack.start == 0)
		return SCE_KERNEL_ERROR_NO_MEMORY;

	return id;
}

int __KernelCreateThread(const char *threadName, SceUID moduleID, u32 entry, u32 prio, int stacksize, u32 attr, u32 optionAddr) {
	if (threadName == nullptr)
		return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_ERROR, "NULL thread name");

	if ((u32)stacksize < 0x200)
		return hleReportWarning(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE, "bogus thread stack size %08x", stacksize);
	if (prio < 0x08 || prio > 0x77) {
		WARN_LOG_REPORT(SCEKERNEL, "sceKernelCreateThread(name=%s): bogus priority %08x", threadName, prio);
		// TODO: Should return this error.
		// return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
		prio = prio < 0x08 ? 0x08 : 0x77;
	}
	if (!Memory::IsValidAddress(entry)) {
		// The PSP firmware seems to allow NULL...?
		if (entry != 0)
			return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_ADDR, "invalid thread entry %08x", entry);
	}
	if ((attr & ~PSP_THREAD_ATTR_USER_MASK) != 0)
		return hleReportWarning(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_ATTR, "illegal thread attributes %08x", attr);

	if ((attr & ~PSP_THREAD_ATTR_SUPPORTED) != 0)
		WARN_LOG_REPORT(SCEKERNEL, "sceKernelCreateThread(name=%s): unsupported attributes %08x", threadName, attr);

	// TODO: Not sure what these values are, but they are removed from the attr silently.
	// Some are USB/VSH specific, probably removes when they are from the wrong module?
	attr &= ~PSP_THREAD_ATTR_USER_ERASE;

	// We're assuming all threads created are user threads.
	if ((attr & PSP_THREAD_ATTR_KERNEL) == 0)
		attr |= PSP_THREAD_ATTR_USER;

	SceUID id = __KernelCreateThreadInternal(threadName, moduleID, entry, prio, stacksize, attr);
	if ((u32)id == SCE_KERNEL_ERROR_NO_MEMORY)
		return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_NO_MEMORY, "out of memory, %08x stack requested", stacksize);

	if (optionAddr != 0)
		WARN_LOG_REPORT(SCEKERNEL, "sceKernelCreateThread(name=%s): unsupported options parameter %08x", threadName, optionAddr);

	// Creating a thread resumes dispatch automatically.  Probably can't create without it.
	dispatchEnabled = true;

	hleEatCycles(32000);
	// This won't schedule to the new thread, but it may to one woken from eating cycles.
	// Technically, this should not eat all at once, and reschedule in the middle, but that's hard.
	hleReSchedule("thread created");

	// Before triggering, set v0, since we restore on return.
	RETURN(id);
	__KernelThreadTriggerEvent((attr & PSP_THREAD_ATTR_KERNEL) != 0, id, THREADEVENT_CREATE);
	return hleLogSuccessInfoI(SCEKERNEL, id);
}

int sceKernelCreateThread(const char *threadName, u32 entry, u32 prio, int stacksize, u32 attr, u32 optionAddr) {
	return __KernelCreateThread(threadName, __KernelGetCurThreadModuleId(), entry, prio, stacksize, attr, optionAddr);
}

int __KernelStartThread(SceUID threadToStartID, int argSize, u32 argBlockPtr, bool forceArgs) {
	u32 error;
	Thread *startThread = kernelObjects.Get<Thread>(threadToStartID, error);
	if (startThread == 0)
		return error;

	Thread *cur = __GetCurrentThread();
	__KernelResetThread(startThread, cur ? (s32)cur->nt.currentPriority : 0);

	u32 &sp = startThread->context.r[MIPS_REG_SP];
	// Force args means just use those as a0/a1 without any special treatment.
	// This is a hack to avoid allocating memory for helper threads which take args.
	if ((argBlockPtr && argSize > 0) || forceArgs) {
		// Make room for the arguments, always 0x10 aligned.
		if (!forceArgs)
			sp -= (argSize + 0xf) & ~0xf;
		startThread->context.r[MIPS_REG_A0] = argSize;
		startThread->context.r[MIPS_REG_A1] = sp;
	} else {
		startThread->context.r[MIPS_REG_A0] = 0;
		startThread->context.r[MIPS_REG_A1] = 0;
	}

	// Now copy argument to stack.
	if (!forceArgs && Memory::IsValidAddress(argBlockPtr))
		Memory::Memcpy(sp, argBlockPtr, argSize);

	// On the PSP, there's an extra 64 bytes of stack eaten after the args.
	// This could be stack overflow safety, or just stack eaten by the kernel entry func.
	sp -= 64;

	// At the bottom of those 64 bytes, the return syscall and ra is written.
	// Test Drive Unlimited actually depends on it being in the correct place.
	WriteSyscall("FakeSysCalls", NID_THREADRETURN, sp);
	Memory::Write_U32(MIPS_MAKE_B(-1), sp + 8);
	Memory::Write_U32(MIPS_MAKE_NOP(), sp + 12);

	// Point ra at our return stub, and start fp off matching sp.
	startThread->context.r[MIPS_REG_RA] = sp;
	startThread->context.r[MIPS_REG_FP] = sp;

	// Smaller is better for priority.  Only switch if the new thread is better.
	if (cur && cur->nt.currentPriority > startThread->nt.currentPriority) {
		__KernelChangeReadyState(cur, currentThread, true);
		hleReSchedule("thread started");
	}

	// Starting a thread automatically resumes the dispatch thread if the new thread has worse priority.
	// Seems strange but also seems reproducible.
	if (cur && cur->nt.currentPriority <= startThread->nt.currentPriority) {
		dispatchEnabled = true;
	}

	__KernelChangeReadyState(startThread, threadToStartID, true);

	// Need to write out v0 before triggering event.
	// TODO: Technically the wrong place.  This should trigger when the thread actually starts (e.g. if suspended.)
	RETURN(0);
	__KernelThreadTriggerEvent((startThread->nt.attr & PSP_THREAD_ATTR_KERNEL) != 0, threadToStartID, THREADEVENT_START);
	return 0;
}

int __KernelStartThreadValidate(SceUID threadToStartID, int argSize, u32 argBlockPtr, bool forceArgs) {
	if (threadToStartID == 0)
		return hleLogError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_THID, "thread id is 0");
	if (argSize < 0 || argBlockPtr & 0x80000000)
		return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_ADDR, "bad thread argument pointer/length %08x / %08x", argSize, argBlockPtr);

	u32 error = 0;
	Thread *startThread = kernelObjects.Get<Thread>(threadToStartID, error);
	if (startThread == 0)
		return hleLogError(SCEKERNEL, error, "thread does not exist");

	if (startThread->nt.status != THREADSTATUS_DORMANT)
		return hleLogWarning(SCEKERNEL, SCE_KERNEL_ERROR_NOT_DORMANT, "thread already running");

	hleEatCycles(3400);
	return __KernelStartThread(threadToStartID, argSize, argBlockPtr, forceArgs);
}

// int sceKernelStartThread(SceUID threadToStartID, SceSize argSize, void *argBlock)
int sceKernelStartThread(SceUID threadToStartID, int argSize, u32 argBlockPtr) {
	return hleLogSuccessInfoI(SCEKERNEL, __KernelStartThreadValidate(threadToStartID, argSize, argBlockPtr));
}

int sceKernelGetThreadStackFreeSize(SceUID threadID)
{
	DEBUG_LOG(SCEKERNEL, "sceKernelGetThreadStackFreeSize(%i)", threadID);

	if (threadID == 0)
		threadID = __KernelGetCurThread();

	u32 error;
	Thread *thread = kernelObjects.Get<Thread>(threadID, error);
	if (thread == 0)
	{
		ERROR_LOG(SCEKERNEL, "sceKernelGetThreadStackFreeSize: invalid thread id %i", threadID);
		return error;
	}

	// Scan the stack for 0xFF, starting after 0x10 (the thread id is written there.)
	// Obviously this doesn't work great if PSP_THREAD_ATTR_NO_FILLSTACK is used.
	int sz = 0;
	for (u32 offset = 0x10; offset < thread->nt.stackSize; ++offset)
	{
		if (Memory::Read_U8(thread->currentStack.start + offset) != 0xFF)
			break;
		sz++;
	}

	return sz & ~3;
}

void __KernelReturnFromThread()
{
	hleSkipDeadbeef();

	int exitStatus = currentMIPS->r[MIPS_REG_V0];
	Thread *thread = __GetCurrentThread();
	_dbg_assert_msg_(SCEKERNEL, thread != NULL, "Returned from a NULL thread.");

	INFO_LOG(SCEKERNEL,"__KernelReturnFromThread: %d", exitStatus);
	__KernelStopThread(currentThread, exitStatus, "thread returned");

	hleReSchedule("thread returned");

	// TODO: This should trigger ON the thread when it exits.
	__KernelThreadTriggerEvent((thread->nt.attr & PSP_THREAD_ATTR_KERNEL) != 0, thread->GetUID(), THREADEVENT_EXIT);

	// The stack will be deallocated when the thread is deleted.
}

void sceKernelExitThread(int exitStatus)
{
	Thread *thread = __GetCurrentThread();
	_dbg_assert_msg_(SCEKERNEL, thread != NULL, "Exited from a NULL thread.");

	INFO_LOG(SCEKERNEL, "sceKernelExitThread(%d)", exitStatus);
	__KernelStopThread(currentThread, exitStatus, "thread exited");

	hleReSchedule("thread exited");

	// TODO: This should trigger ON the thread when it exits.
	__KernelThreadTriggerEvent((thread->nt.attr & PSP_THREAD_ATTR_KERNEL) != 0, thread->GetUID(), THREADEVENT_EXIT);

	// The stack will be deallocated when the thread is deleted.
}

void _sceKernelExitThread(int exitStatus)
{
	Thread *thread = __GetCurrentThread();
	_dbg_assert_msg_(SCEKERNEL, thread != NULL, "_Exited from a NULL thread.");

	ERROR_LOG_REPORT(SCEKERNEL, "_sceKernelExitThread(%d): should not be called directly", exitStatus);
	__KernelStopThread(currentThread, exitStatus, "thread _exited");

	hleReSchedule("thread _exited");

	// TODO: This should trigger ON the thread when it exits.
	__KernelThreadTriggerEvent((thread->nt.attr & PSP_THREAD_ATTR_KERNEL) != 0, thread->GetUID(), THREADEVENT_EXIT);

	// The stack will be deallocated when the thread is deleted.
}

void sceKernelExitDeleteThread(int exitStatus)
{
	Thread *thread = __GetCurrentThread();
	if (thread)
	{
		INFO_LOG(SCEKERNEL,"sceKernelExitDeleteThread(%d)", exitStatus);
		uint32_t thread_attr = thread->nt.attr;
		uint32_t uid = thread->GetUID();
		__KernelDeleteThread(currentThread, exitStatus, "thread exited with delete");
		// Temporary hack since we don't reschedule within callbacks.
		g_inCbCount = 0;

		hleReSchedule("thread exited with delete");

		// TODO: This should trigger ON the thread when it exits.
		__KernelThreadTriggerEvent((thread_attr & PSP_THREAD_ATTR_KERNEL) != 0, uid, THREADEVENT_EXIT);
	}
	else
		ERROR_LOG_REPORT(SCEKERNEL, "sceKernelExitDeleteThread(%d) ERROR - could not find myself!", exitStatus);
}

u32 sceKernelSuspendDispatchThread()
{
	if (!__InterruptsEnabled())
	{
		DEBUG_LOG(SCEKERNEL, "sceKernelSuspendDispatchThread(): interrupts disabled");
		return SCE_KERNEL_ERROR_CPUDI;
	}

	u32 oldDispatchEnabled = dispatchEnabled;
	dispatchEnabled = false;
	DEBUG_LOG(SCEKERNEL, "%i=sceKernelSuspendDispatchThread()", oldDispatchEnabled);
	hleEatCycles(940);
	return oldDispatchEnabled;
}

u32 sceKernelResumeDispatchThread(u32 enabled)
{
	if (!__InterruptsEnabled())
	{
		DEBUG_LOG(SCEKERNEL, "sceKernelResumeDispatchThread(%i): interrupts disabled", enabled);
		return SCE_KERNEL_ERROR_CPUDI;
	}

	u32 oldDispatchEnabled = dispatchEnabled;
	dispatchEnabled = enabled != 0;
	DEBUG_LOG(SCEKERNEL, "sceKernelResumeDispatchThread(%i) - from %i", enabled, oldDispatchEnabled);
	hleReSchedule("dispatch resumed");
	hleEatCycles(940);
	return 0;
}

bool __KernelIsDispatchEnabled()
{
	// Dispatch can never be enabled when interrupts are disabled.
	return dispatchEnabled && __InterruptsEnabled();
}

int sceKernelRotateThreadReadyQueue(int priority)
{
	VERBOSE_LOG(SCEKERNEL, "sceKernelRotateThreadReadyQueue(%x)", priority);

	Thread *cur = __GetCurrentThread();

	// 0 is special, it means "my current priority."
	if (priority == 0)
		priority = cur->nt.currentPriority;

	if (priority <= 0x07 || priority > 0x77)
		return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;

	if (!threadReadyQueue.empty(priority))
	{
		// In other words, yield to everyone else.
		if (cur->nt.currentPriority == priority)
		{
			threadReadyQueue.push_back(priority, currentThread);
			cur->nt.status = (cur->nt.status & ~THREADSTATUS_RUNNING) | THREADSTATUS_READY;
		}
		// Yield the next thread of this priority to all other threads of same priority.
		else
			threadReadyQueue.rotate(priority);
	}

	hleReSchedule("rotatethreadreadyqueue");
	hleEatCycles(250);
	return 0;
}

int sceKernelDeleteThread(int threadID)
{
	if (threadID == 0 || threadID == currentThread)
	{
		ERROR_LOG(SCEKERNEL, "sceKernelDeleteThread(%i): cannot delete current thread", threadID);
		return SCE_KERNEL_ERROR_NOT_DORMANT;
	}

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		if (!t->isStopped())
		{
			ERROR_LOG(SCEKERNEL, "sceKernelDeleteThread(%i): thread not dormant", threadID);
			return SCE_KERNEL_ERROR_NOT_DORMANT;
		}

		DEBUG_LOG(SCEKERNEL, "sceKernelDeleteThread(%i)", threadID);
		return __KernelDeleteThread(threadID, SCE_KERNEL_ERROR_THREAD_TERMINATED, "thread deleted");
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "sceKernelDeleteThread(%i): thread doesn't exist", threadID);
		return error;
	}
}

int sceKernelTerminateDeleteThread(int threadID)
{
	if (threadID == 0 || threadID == currentThread)
	{
		ERROR_LOG(SCEKERNEL, "sceKernelTerminateDeleteThread(%i): cannot terminate current thread", threadID);
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		bool wasStopped = t->isStopped();
		uint32_t attr = t->nt.attr;
		uint32_t uid = t->GetUID();

		INFO_LOG(SCEKERNEL, "sceKernelTerminateDeleteThread(%i)", threadID);
		error = __KernelDeleteThread(threadID, SCE_KERNEL_ERROR_THREAD_TERMINATED, "thread terminated with delete");

		if (!wasStopped) {
			// Set v0 before calling the handler, or it'll get lost.
			RETURN(error);
			__KernelThreadTriggerEvent((attr & PSP_THREAD_ATTR_KERNEL) != 0, uid, THREADEVENT_EXIT);
		}

		return error;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "sceKernelTerminateDeleteThread(%i): thread doesn't exist", threadID);
		return error;
	}
}

int sceKernelTerminateThread(SceUID threadID) {
	if (__IsInInterrupt() && sceKernelGetCompiledSdkVersion() >= 0x03080000) {
		return hleLogError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_CONTEXT, "in interrupt");
	}
	if (threadID == 0 || threadID == currentThread) {
		return hleLogError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_THID, "cannot terminate current thread");
	}

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t) {
		if (t->isStopped()) {
			return hleLogError(SCEKERNEL, SCE_KERNEL_ERROR_DORMANT, "already stopped");
		}

		// TODO: Should this reschedule?  Seems like not.
		__KernelStopThread(threadID, SCE_KERNEL_ERROR_THREAD_TERMINATED, "thread terminated");

		// On terminate, we reset the thread priority.  On exit, we don't always (see __KernelResetThread.)
		t->nt.currentPriority = t->nt.initialPriority;

		// Need to set v0 since it'll be restored.
		RETURN(0);
		__KernelThreadTriggerEvent((t->nt.attr & PSP_THREAD_ATTR_KERNEL) != 0, t->GetUID(), THREADEVENT_EXIT);

		return hleLogSuccessInfoI(SCEKERNEL, 0);
	} else {
		return hleLogError(SCEKERNEL, error, "thread doesn't exist");
	}
}

SceUID __KernelGetCurThread()
{
	return currentThread;
}

SceUID __KernelGetCurThreadModuleId()
{
	Thread *t = __GetCurrentThread();
	if (t)
		return t->moduleId;
	return 0;
}

u32 __KernelGetCurThreadStack()
{
	Thread *t = __GetCurrentThread();
	if (t)
		return t->currentStack.end;
	return 0;
}

u32 __KernelGetCurThreadStackStart()
{
	Thread *t = __GetCurrentThread();
	if (t)
		return t->currentStack.start;
	return 0;
}

SceUID sceKernelGetThreadId()
{
	VERBOSE_LOG(SCEKERNEL, "%i = sceKernelGetThreadId()", currentThread);
	hleEatCycles(180);
	return currentThread;
}

int sceKernelGetThreadCurrentPriority() {
	u32 retVal = __GetCurrentThread()->nt.currentPriority;
	return hleLogSuccessI(SCEKERNEL, retVal);
}

int sceKernelChangeCurrentThreadAttr(u32 clearAttr, u32 setAttr) {
	// Seems like this is the only allowed attribute?
	if ((clearAttr & ~PSP_THREAD_ATTR_VFPU) != 0 || (setAttr & ~PSP_THREAD_ATTR_VFPU) != 0) {
		return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_ATTR, "invalid attr");
	}

	Thread *t = __GetCurrentThread();
	if (!t)
		return hleReportError(SCEKERNEL, -1, "no current thread");

	t->nt.attr = (t->nt.attr & ~clearAttr) | setAttr;
	return hleLogSuccessI(SCEKERNEL, 0);
}

int sceKernelChangeThreadPriority(SceUID threadID, int priority) {
	if (threadID == 0) {
		threadID = __KernelGetCurThread();
	}

	// 0 means the current (running) thread's priority, not target's.
	if (priority == 0) {
		Thread *cur = __GetCurrentThread();
		if (!cur) {
			ERROR_LOG_REPORT(SCEKERNEL, "sceKernelChangeThreadPriority(%i, %i): no current thread?", threadID, priority);
		} else {
			priority = cur->nt.currentPriority;
		}
	}

	u32 error;
	Thread *thread = kernelObjects.Get<Thread>(threadID, error);
	if (thread) {
		if (thread->isStopped()) {
			return hleLogError(SCEKERNEL, SCE_KERNEL_ERROR_DORMANT, "thread is dormant");
		}

		if (priority < 0x08 || priority > 0x77) {
			return hleLogError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_PRIORITY, "bogus priority");
		}

		int old = thread->nt.currentPriority;
		threadReadyQueue.remove(old, threadID);

		thread->nt.currentPriority = priority;
		threadReadyQueue.prepare(thread->nt.currentPriority);
		if (thread->isRunning()) {
			thread->nt.status = (thread->nt.status & ~THREADSTATUS_RUNNING) | THREADSTATUS_READY;
		}
		if (thread->isReady()) {
			threadReadyQueue.push_back(thread->nt.currentPriority, threadID);
		}

		hleEatCycles(450);
		hleReSchedule("change thread priority");

		return hleLogSuccessI(SCEKERNEL, 0);
	} else {
		return hleLogError(SCEKERNEL, error, "thread not found");
	}
}

static s64 __KernelDelayThreadUs(u64 usec) {
	if (usec < 200) {
		return 210;
	}

	if (usec > 0x8000000000000000ULL) {
		// Wrap around (behavior seen on firmware) and potentially wake up soon.
		usec -= 0x8000000000000000ULL;
	}
	if (usec > 0x0010000000000000ULL) {
		// This will probably overflow when we convert to cycles.
		// Note: converting millenia to hundreds of years.  Should be safe, basically perma-delay.
		usec >>= 12;
	}

	// It never wakes up right away.  It usually takes at least 15 extra us, but let's be nicer.
	return usec + 10;
}

int sceKernelDelayThreadCB(u32 usec) {
	hleEatCycles(2000);
	// Note: Sometimes (0) won't delay, potentially based on how much the thread is doing.
	// But a loop with just 0 often does delay, and games depend on this.  So we err on that side.
	SceUID curThread = __KernelGetCurThread();
	s64 delayUs = __KernelDelayThreadUs(usec);
	__KernelScheduleWakeup(curThread, delayUs);
	__KernelWaitCurThread(WAITTYPE_DELAY, curThread, 0, 0, true, "thread delayed");
	return hleLogSuccessI(SCEKERNEL, 0, "delaying %lld usecs", delayUs);
}

int sceKernelDelayThread(u32 usec) {
	hleEatCycles(2000);
	// Note: Sometimes (0) won't delay, potentially based on how much the thread is doing.
	// But a loop with just 0 often does delay, and games depend on this.  So we err on that side.
	SceUID curThread = __KernelGetCurThread();
	s64 delayUs = __KernelDelayThreadUs(usec);
	__KernelScheduleWakeup(curThread, delayUs);
	__KernelWaitCurThread(WAITTYPE_DELAY, curThread, 0, 0, false, "thread delayed");
	return hleLogSuccessI(SCEKERNEL, 0, "delaying %lld usecs", delayUs);
}

int sceKernelDelaySysClockThreadCB(u32 sysclockAddr) {
	auto sysclock = PSPPointer<SceKernelSysClock>::Create(sysclockAddr);
	if (!sysclock.IsValid()) {
		// Note: crashes on real firmware.
		return hleLogError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_ADDRESS, "bad pointer");
	}

	// This is just a u64 of usecs.  All bits are respected, but overflow can happen for very large values.
	u64 usec = sysclock->lo | ((u64)sysclock->hi << 32);

	SceUID curThread = __KernelGetCurThread();
	s64 delayUs = __KernelDelayThreadUs(usec);
	__KernelScheduleWakeup(curThread, delayUs);
	__KernelWaitCurThread(WAITTYPE_DELAY, curThread, 0, 0, true, "thread delayed");
	return hleLogSuccessI(SCEKERNEL, 0, "delaying %lld usecs", delayUs);
}

int sceKernelDelaySysClockThread(u32 sysclockAddr) {
	auto sysclock = PSPPointer<SceKernelSysClock>::Create(sysclockAddr);
	if (!sysclock.IsValid()) {
		// Note: crashes on real firmware.
		return hleLogError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_ADDRESS, "bad pointer");
	}

	// This is just a u64 of usecs.  All bits are respected, but overflow can happen for very large values.
	u64 usec = sysclock->lo | ((u64)sysclock->hi << 32);

	SceUID curThread = __KernelGetCurThread();
	s64 delayUs = __KernelDelayThreadUs(usec);
	__KernelScheduleWakeup(curThread, delayUs);
	__KernelWaitCurThread(WAITTYPE_DELAY, curThread, 0, 0, false, "thread delayed");
	return hleLogSuccessI(SCEKERNEL, 0, "delaying %lld usecs", delayUs);
}

u32 __KernelGetThreadPrio(SceUID id)
{
	u32 error;
	Thread *thread = kernelObjects.Get<Thread>(id, error);
	if (thread)
		return thread->nt.currentPriority;
	return 0;
}

bool __KernelThreadSortPriority(SceUID thread1, SceUID thread2)
{
	return __KernelGetThreadPrio(thread1) < __KernelGetThreadPrio(thread2);
}

//////////////////////////////////////////////////////////////////////////
// WAIT/SLEEP ETC
//////////////////////////////////////////////////////////////////////////
int sceKernelWakeupThread(SceUID uid) {
	if (uid == currentThread) {
		return hleLogWarning(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_THID, "unable to wakeup current thread");
	}

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(uid, error);
	if (t) {
		if (!t->isWaitingFor(WAITTYPE_SLEEP, 0)) {
			t->nt.wakeupCount++;
			return hleLogSuccessI(SCEKERNEL, 0, "wakeupCount incremented to %i", t->nt.wakeupCount);
		} else {
			__KernelResumeThreadFromWait(uid, 0);
			hleReSchedule("thread woken up");
			return hleLogSuccessVerboseI(SCEKERNEL, 0, "woke thread at %i", t->nt.wakeupCount);
		}
	} else {
		return hleLogError(SCEKERNEL, error, "bad thread id");
	}
}

int sceKernelCancelWakeupThread(SceUID uid) {
	if (uid == 0) {
		uid = __KernelGetCurThread();
	}

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(uid, error);
	if (t) {
		int wCount = t->nt.wakeupCount;
		t->nt.wakeupCount = 0;
		return hleLogSuccessI(SCEKERNEL, wCount, "wakeupCount reset to 0");
	} else {
		return hleLogError(SCEKERNEL, error, "bad thread id");
	}
}

static int __KernelSleepThread(bool doCallbacks) {
	Thread *thread = __GetCurrentThread();
	if (!thread) {
		ERROR_LOG_REPORT(SCEKERNEL, "sceKernelSleepThread*(): bad current thread");
		return -1;
	}

	if (thread->nt.wakeupCount > 0) {
		thread->nt.wakeupCount--;
		return hleLogSuccessI(SCEKERNEL, 0, "wakeupCount decremented to %i", thread->nt.wakeupCount);
	} else {
		__KernelWaitCurThread(WAITTYPE_SLEEP, 0, 0, 0, doCallbacks, "thread slept");
		return hleLogSuccessVerboseI(SCEKERNEL, 0, "sleeping");
	}
	return 0;
}

int sceKernelSleepThread() {
	return __KernelSleepThread(false);
}

//the homebrew PollCallbacks
int sceKernelSleepThreadCB() {
	return __KernelSleepThread(true);
}

int sceKernelWaitThreadEnd(SceUID threadID, u32 timeoutPtr)
{
	DEBUG_LOG(SCEKERNEL, "sceKernelWaitThreadEnd(%i, %08x)", threadID, timeoutPtr);
	if (threadID == 0 || threadID == currentThread)
		return SCE_KERNEL_ERROR_ILLEGAL_THID;

	if (!__KernelIsDispatchEnabled())
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	if (__IsInInterrupt())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		if (t->nt.status != THREADSTATUS_DORMANT)
		{
			if (Memory::IsValidAddress(timeoutPtr))
				__KernelScheduleThreadEndTimeout(currentThread, threadID, Memory::Read_U32(timeoutPtr));
			if (std::find(t->waitingThreads.begin(), t->waitingThreads.end(), currentThread) == t->waitingThreads.end())
				t->waitingThreads.push_back(currentThread);
			__KernelWaitCurThread(WAITTYPE_THREADEND, threadID, 0, timeoutPtr, false, "thread wait end");
		}

		return t->nt.exitStatus;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "sceKernelWaitThreadEnd - bad thread %i", threadID);
		return error;
	}
}

int sceKernelWaitThreadEndCB(SceUID threadID, u32 timeoutPtr)
{
	DEBUG_LOG(SCEKERNEL, "sceKernelWaitThreadEndCB(%i, 0x%X)", threadID, timeoutPtr);
	if (threadID == 0 || threadID == currentThread)
		return SCE_KERNEL_ERROR_ILLEGAL_THID;

	if (!__KernelIsDispatchEnabled())
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	if (__IsInInterrupt())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		if (t->nt.status != THREADSTATUS_DORMANT)
		{
			if (Memory::IsValidAddress(timeoutPtr))
				__KernelScheduleThreadEndTimeout(currentThread, threadID, Memory::Read_U32(timeoutPtr));
			if (std::find(t->waitingThreads.begin(), t->waitingThreads.end(), currentThread) == t->waitingThreads.end())
				t->waitingThreads.push_back(currentThread);
			__KernelWaitCurThread(WAITTYPE_THREADEND, threadID, 0, timeoutPtr, true, "thread wait end");
		}
		else
			hleCheckCurrentCallbacks();

		return t->nt.exitStatus;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "sceKernelWaitThreadEndCB - bad thread %i", threadID);
		return error;
	}
}

int sceKernelReleaseWaitThread(SceUID threadID)
{
	DEBUG_LOG(SCEKERNEL, "sceKernelReleaseWaitThread(%i)", threadID);
	if (__KernelInCallback())
		WARN_LOG_REPORT(SCEKERNEL, "UNTESTED sceKernelReleaseWaitThread() might not do the right thing in a callback");

	if (threadID == 0 || threadID == currentThread)
		return SCE_KERNEL_ERROR_ILLEGAL_THID;

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		if (!t->isWaiting())
			return SCE_KERNEL_ERROR_NOT_WAIT;
		if (t->nt.waitType == WAITTYPE_HLEDELAY)
		{
			WARN_LOG_REPORT_ONCE(rwt_delay, SCEKERNEL, "sceKernelReleaseWaitThread(): Refusing to wake HLE-delayed thread, right thing to do?");
			return SCE_KERNEL_ERROR_NOT_WAIT;
		}
		if (t->nt.waitType == WAITTYPE_MODULE)
		{
			WARN_LOG_REPORT_ONCE(rwt_sm, SCEKERNEL, "sceKernelReleaseWaitThread(): Refusing to wake start_module thread, right thing to do?");
			return SCE_KERNEL_ERROR_NOT_WAIT;
		}

		__KernelResumeThreadFromWait(threadID, SCE_KERNEL_ERROR_RELEASE_WAIT);
		hleReSchedule("thread released from wait");
		return 0;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "sceKernelReleaseWaitThread - bad thread %i", threadID);
		return error;
	}
}

int sceKernelSuspendThread(SceUID threadID)
{
	// TODO: What about interrupts/callbacks?
	if (threadID == 0 || threadID == currentThread)
	{
		ERROR_LOG(SCEKERNEL, "sceKernelSuspendThread(%d): cannot suspend current thread", threadID);
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		if (t->isStopped())
		{
			ERROR_LOG(SCEKERNEL, "sceKernelSuspendThread(%d): thread not running", threadID);
			return SCE_KERNEL_ERROR_DORMANT;
		}
		if (t->isSuspended())
		{
			ERROR_LOG(SCEKERNEL, "sceKernelSuspendThread(%d): thread already suspended", threadID);
			return SCE_KERNEL_ERROR_SUSPEND;
		}

		DEBUG_LOG(SCEKERNEL, "sceKernelSuspendThread(%d)", threadID);
		if (t->isReady())
			__KernelChangeReadyState(t, threadID, false);
		t->nt.status = (t->nt.status & ~THREADSTATUS_READY) | THREADSTATUS_SUSPEND;
		return 0;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "sceKernelSuspendThread(%d): bad thread", threadID);
		return error;
	}
}

int sceKernelResumeThread(SceUID threadID)
{
	// TODO: What about interrupts/callbacks?
	if (threadID == 0 || threadID == currentThread)
	{
		ERROR_LOG(SCEKERNEL, "sceKernelResumeThread(%d): cannot suspend current thread", threadID);
		return SCE_KERNEL_ERROR_ILLEGAL_THID;
	}

	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadID, error);
	if (t)
	{
		if (!t->isSuspended())
		{
			ERROR_LOG(SCEKERNEL, "sceKernelResumeThread(%d): thread not suspended", threadID);
			return SCE_KERNEL_ERROR_NOT_SUSPEND;
		}
		DEBUG_LOG(SCEKERNEL, "sceKernelResumeThread(%d)", threadID);
		t->nt.status &= ~THREADSTATUS_SUSPEND;

		// If it was dormant, waiting, etc. before we don't flip its ready state.
		if (t->nt.status == 0)
			__KernelChangeReadyState(t, threadID, true);
		return 0;
	}
	else
	{
		ERROR_LOG(SCEKERNEL, "sceKernelResumeThread(%d): bad thread", threadID);
		return error;
	}
}



//////////////////////////////////////////////////////////////////////////
// CALLBACKS
//////////////////////////////////////////////////////////////////////////

SceUID sceKernelCreateCallback(const char *name, u32 entrypoint, u32 signalArg)
{
	if (!name)
		return hleReportWarning(SCEKERNEL, SCE_KERNEL_ERROR_ERROR, "invalid name");
	if (entrypoint & 0xF0000000)
		return hleReportWarning(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_ADDR, "invalid func");

	Callback *cb = new Callback;
	SceUID id = kernelObjects.Create(cb);

	strncpy(cb->nc.name, name, KERNELOBJECT_MAX_NAME_LENGTH);
	cb->nc.name[KERNELOBJECT_MAX_NAME_LENGTH] = 0;
	cb->nc.size = sizeof(NativeCallback);
	cb->nc.entrypoint = entrypoint;
	cb->nc.threadId = __KernelGetCurThread();
	cb->nc.commonArgument = signalArg;
	cb->nc.notifyCount = 0;
	cb->nc.notifyArg = 0;

	Thread *thread = __GetCurrentThread();
	if (thread)
		thread->callbacks.push_back(id);

	return hleLogSuccessI(SCEKERNEL, id);
}

int sceKernelDeleteCallback(SceUID cbId)
{
	u32 error;
	Callback *cb = kernelObjects.Get<Callback>(cbId, error);
	if (cb)
	{
		Thread *thread = kernelObjects.Get<Thread>(cb->nc.threadId, error);
		if (thread)
			thread->callbacks.erase(std::remove(thread->callbacks.begin(), thread->callbacks.end(), cbId), thread->callbacks.end());
		if (cb->nc.notifyCount != 0)
			readyCallbacksCount--;

		return hleLogSuccessI(SCEKERNEL, kernelObjects.Destroy<Callback>(cbId));
	} else {
		return hleLogError(SCEKERNEL, error, "bad cbId");
	}
}

// Generally very rarely used, but Numblast uses it like candy.
int sceKernelNotifyCallback(SceUID cbId, int notifyArg)
{
	u32 error;
	Callback *cb = kernelObjects.Get<Callback>(cbId, error);
	if (cb) {
		__KernelNotifyCallback(cbId, notifyArg);
		return hleLogSuccessI(SCEKERNEL, 0);
	} else {
		return hleLogError(SCEKERNEL, error, "bad cbId");
	}
}

int sceKernelCancelCallback(SceUID cbId)
{
	u32 error;
	Callback *cb = kernelObjects.Get<Callback>(cbId, error);
	if (cb) {
		// This just resets the notify count.
		cb->nc.notifyArg = 0;
		return hleLogSuccessI(SCEKERNEL, 0);
	} else {
		return hleLogError(SCEKERNEL, error, "bad cbId");
	}
}

int sceKernelGetCallbackCount(SceUID cbId)
{
	u32 error;
	Callback *cb = kernelObjects.Get<Callback>(cbId, error);
	if (cb) {
		return hleLogSuccessVerboseI(SCEKERNEL, cb->nc.notifyCount);
	} else {
		return hleLogError(SCEKERNEL, error, "bad cbId");
	}
}

int sceKernelReferCallbackStatus(SceUID cbId, u32 statusAddr)
{
	u32 error;
	Callback *c = kernelObjects.Get<Callback>(cbId, error);
	if (c) {
		if (Memory::IsValidAddress(statusAddr) && Memory::Read_U32(statusAddr) != 0) {
			Memory::WriteStruct(statusAddr, &c->nc);
			return hleLogSuccessI(SCEKERNEL, 0);
		} else {
			return hleLogDebug(SCEKERNEL, 0, "struct size was 0");
		}
	} else {
		return hleLogError(SCEKERNEL, error, "bad cbId");
	}
}

u32 sceKernelExtendThreadStack(u32 size, u32 entryAddr, u32 entryParameter)
{
	if (size < 512)
		return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE, "xxx", "stack size too small");

	Thread *thread = __GetCurrentThread();
	if (!thread)
		return hleReportError(SCEKERNEL, -1, "xxx", "not on a thread?");

	if (!thread->PushExtendedStack(size))
		return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_NO_MEMORY, "xxx", "could not allocate new stack");

	// The stack has been changed now, so it's do or die time.

	// Push the old SP, RA, and PC onto the stack (so we can restore them later.)
	Memory::Write_U32(currentMIPS->r[MIPS_REG_RA], thread->currentStack.end - 4);
	Memory::Write_U32(currentMIPS->r[MIPS_REG_SP], thread->currentStack.end - 8);
	Memory::Write_U32(currentMIPS->pc, thread->currentStack.end - 12);

	currentMIPS->pc = entryAddr;
	currentMIPS->r[MIPS_REG_A0] = entryParameter;
	currentMIPS->r[MIPS_REG_RA] = extendReturnHackAddr;
	// Stack should stay aligned even though we saved only 3 regs.
	currentMIPS->r[MIPS_REG_SP] = thread->currentStack.end - 0x10;

	hleSkipDeadbeef();
	return hleLogSuccessI(SCEKERNEL, 0);
}

void __KernelReturnFromExtendStack()
{
	hleSkipDeadbeef();

	Thread *thread = __GetCurrentThread();
	if (!thread)
	{
		ERROR_LOG_REPORT(SCEKERNEL, "__KernelReturnFromExtendStack() - not on a thread?");
		return;
	}

	// Grab the saved regs at the top of the stack.
	u32 restoreRA = Memory::Read_U32(thread->currentStack.end - 4);
	u32 restoreSP = Memory::Read_U32(thread->currentStack.end - 8);
	u32 restorePC = Memory::Read_U32(thread->currentStack.end - 12);

	if (!thread->PopExtendedStack())
	{
		ERROR_LOG_REPORT(SCEKERNEL, "__KernelReturnFromExtendStack() - no stack to restore?");
		return;
	}

	DEBUG_LOG(SCEKERNEL, "__KernelReturnFromExtendStack()");
	currentMIPS->r[MIPS_REG_RA] = restoreRA;
	currentMIPS->r[MIPS_REG_SP] = restoreSP;
	currentMIPS->pc = restorePC;

	// We retain whatever is in v0/v1, it gets passed on to the caller of sceKernelExtendThreadStack().
}

void ActionAfterMipsCall::run(MipsCall &call) {
	u32 error;
	Thread *thread = kernelObjects.Get<Thread>(threadID, error);
	if (thread) {
		// Resume waiting after a callback, but not from terminate/delete.
		if ((thread->nt.status & (THREADSTATUS_DEAD | THREADSTATUS_DORMANT)) == 0) {
			__KernelChangeReadyState(thread, threadID, (status & THREADSTATUS_READY) != 0);
			thread->nt.status = status;
		}
		thread->nt.waitType = waitType;
		thread->nt.waitID = waitID;
		thread->waitInfo = waitInfo;
		thread->isProcessingCallbacks = isProcessingCallbacks;
		thread->currentCallbackId = currentCallbackId;
	}

	if (chainedAction) {
		chainedAction->run(call);
		delete chainedAction;
	}
}

void Thread::setReturnValue(u32 retval)
{
	if (GetUID() == currentThread) {
		currentMIPS->r[MIPS_REG_V0] = retval;
	} else {
		context.r[MIPS_REG_V0] = retval;
	}
}

void Thread::setReturnValue(u64 retval)
{
	if (GetUID() == currentThread) {
		currentMIPS->r[MIPS_REG_V0] = retval & 0xFFFFFFFF;
		currentMIPS->r[MIPS_REG_V1] = (retval >> 32) & 0xFFFFFFFF;
	} else {
		context.r[MIPS_REG_V0] = retval & 0xFFFFFFFF;
		context.r[MIPS_REG_V1] = (retval >> 32) & 0xFFFFFFFF;
	}
}

void Thread::resumeFromWait()
{
	nt.status &= ~THREADSTATUS_WAIT;
	if (!(nt.status & (THREADSTATUS_WAITSUSPEND | THREADSTATUS_DORMANT | THREADSTATUS_DEAD)))
		__KernelChangeReadyState(this, GetUID(), true);

	// Non-waiting threads do not process callbacks.
	isProcessingCallbacks = false;
}

bool Thread::isWaitingFor(WaitType type, int id) const
{
	if (nt.status & THREADSTATUS_WAIT)
		return nt.waitType == type && nt.waitID == id;
	return false;
}

int Thread::getWaitID(WaitType type) const
{
	if (nt.waitType == type)
		return nt.waitID;
	return 0;
}

ThreadWaitInfo Thread::getWaitInfo() const
{
	return waitInfo;
}

void __KernelSwitchContext(Thread *target, const char *reason) 
{
	u32 oldPC = 0;
	SceUID oldUID = 0;
	const char *oldName = hleCurrentThreadName != NULL ? hleCurrentThreadName : "(none)";

	Thread *cur = __GetCurrentThread();
	if (cur)  // It might just have been deleted.
	{
		__KernelSaveContext(&cur->context, (cur->nt.attr & PSP_THREAD_ATTR_VFPU) != 0);
		oldPC = currentMIPS->pc;
		oldUID = cur->GetUID();

		// Normally this is taken care of in __KernelNextThread().
		if (cur->isRunning())
			__KernelChangeReadyState(cur, oldUID, true);
	}

	if (target)
	{
		__SetCurrentThread(target, target->GetUID(), target->nt.name);
		__KernelChangeReadyState(target, currentThread, false);
		target->nt.status = (target->nt.status | THREADSTATUS_RUNNING) & ~THREADSTATUS_READY;

		__KernelLoadContext(&target->context, (target->nt.attr & PSP_THREAD_ATTR_VFPU) != 0);
	}
	else
		__SetCurrentThread(NULL, 0, NULL);

	const bool fromIdle = oldUID == threadIdleID[0] || oldUID == threadIdleID[1];
	const bool toIdle = currentThread == threadIdleID[0] || currentThread == threadIdleID[1];
#if DEBUG_LEVEL <= MAX_LOGLEVEL || DEBUG_LOG == NOTICE_LOG
	if (!(fromIdle && toIdle))
	{
		u64 nowCycles = CoreTiming::GetTicks();
		s64 consumedCycles = nowCycles - lastSwitchCycles;
		lastSwitchCycles = nowCycles;

		DEBUG_LOG(SCEKERNEL, "Context switch: %s -> %s (%i->%i, pc: %08x->%08x, %s) +%lldus",
			oldName, hleCurrentThreadName,
			oldUID, currentThread,
			oldPC, currentMIPS->pc,
			reason,
			cyclesToUs(consumedCycles));
	}
#endif

	// Switching threads eats some cycles.  This is a low approximation.
	if (fromIdle && toIdle) {
		// Don't eat any cycles going between idle.
	} else if (fromIdle || toIdle) {
		currentMIPS->downcount -= 1200;
	} else {
		currentMIPS->downcount -= 2700;
	}

	if (target)
	{
		// No longer waiting.
		target->nt.waitType = WAITTYPE_NONE;
		target->nt.waitID = 0;

		__KernelExecutePendingMipsCalls(target, true);
	}
}

void __KernelChangeThreadState(Thread *thread, ThreadStatus newStatus) {
	if (!thread || thread->nt.status == newStatus)
		return;

	if (!dispatchEnabled && thread == __GetCurrentThread() && newStatus != THREADSTATUS_RUNNING) {
		ERROR_LOG(SCEKERNEL, "Dispatching suspended, not changing thread state");
		return;
	}

	// TODO: JPSCP has many conditions here, like removing wait timeout actions etc.
	// if (thread->nt.status == THREADSTATUS_WAIT && newStatus != THREADSTATUS_WAITSUSPEND) {

	__KernelChangeReadyState(thread, thread->GetUID(), (newStatus & THREADSTATUS_READY) != 0);
	thread->nt.status = newStatus;

	if (newStatus == THREADSTATUS_WAIT) {
		if (thread->nt.waitType == WAITTYPE_NONE) {
			ERROR_LOG(SCEKERNEL, "Waittype none not allowed here");
		}

		// Schedule deletion of stopped threads here.  if (thread->isStopped())
	}
}


static bool __CanExecuteCallbackNow(Thread *thread) {
	return g_inCbCount == 0;
}

void __KernelCallAddress(Thread *thread, u32 entryPoint, Action *afterAction, const u32 args[], int numargs, bool reschedAfter, SceUID cbId)
{
	if (!thread || thread->isStopped()) {
		WARN_LOG_REPORT(SCEKERNEL, "Running mipscall on dormant thread");
	}

	_dbg_assert_msg_(SCEKERNEL, numargs <= 6, "MipsCalls can only take 6 args.");

	if (thread) {
		ActionAfterMipsCall *after = (ActionAfterMipsCall *) __KernelCreateAction(actionAfterMipsCall);
		after->chainedAction = afterAction;
		after->threadID = thread->GetUID();
		after->status = thread->nt.status;
		after->waitType = (WaitType)(u32)thread->nt.waitType;
		after->waitID = thread->nt.waitID;
		after->waitInfo = thread->waitInfo;
		after->isProcessingCallbacks = thread->isProcessingCallbacks;
		after->currentCallbackId = thread->currentCallbackId;

		afterAction = after;

		if (thread->nt.waitType != WAITTYPE_NONE) {
			// If it's a callback, tell the wait to stop.
			if (cbId > 0) {
				if (waitTypeFuncs[thread->nt.waitType].beginFunc != NULL) {
					waitTypeFuncs[thread->nt.waitType].beginFunc(after->threadID, thread->currentCallbackId);
				} else {
					ERROR_LOG_REPORT(HLE, "Missing begin/restore funcs for wait type %d", thread->nt.waitType);
				}
			}

			// Release thread from waiting
			thread->nt.waitType = WAITTYPE_NONE;
		}

		__KernelChangeThreadState(thread, THREADSTATUS_READY);
	}

	MipsCall *call = new MipsCall();
	call->entryPoint = entryPoint;
	for (int i = 0; i < numargs; i++) {
		call->args[i] = args[i];
	}
	call->numArgs = (int) numargs;
	call->doAfter = afterAction;
	call->tag = "callAddress";
	call->cbId = cbId;

	u32 callId = mipsCalls.add(call);

	bool called = false;
	if (!thread || thread == __GetCurrentThread()) {
		if (__CanExecuteCallbackNow(thread)) {
			thread = __GetCurrentThread();
			__KernelChangeThreadState(thread, THREADSTATUS_RUNNING);
			called = __KernelExecuteMipsCallOnCurrentThread(callId, reschedAfter);
		}
	}

	if (!called) {
		if (thread) {
			DEBUG_LOG(SCEKERNEL, "Making mipscall pending on thread");
			thread->pendingMipsCalls.push_back(callId);
		} else {
			WARN_LOG(SCEKERNEL, "Ignoring mispcall on NULL/deleted thread");
		}
	}
}

void __KernelDirectMipsCall(u32 entryPoint, Action *afterAction, u32 args[], int numargs, bool reschedAfter)
{
	__KernelCallAddress(__GetCurrentThread(), entryPoint, afterAction, args, numargs, reschedAfter, 0);
}

bool __KernelExecuteMipsCallOnCurrentThread(u32 callId, bool reschedAfter)
{
	hleSkipDeadbeef();

	Thread *cur = __GetCurrentThread();
	if (cur == nullptr) {
		ERROR_LOG(SCEKERNEL, "__KernelExecuteMipsCallOnCurrentThread(): Bad current thread");
		return false;
	}

	if (g_inCbCount > 0) {
		WARN_LOG_REPORT(SCEKERNEL, "__KernelExecuteMipsCallOnCurrentThread(): Already in a callback!");
	}
	DEBUG_LOG(SCEKERNEL, "Executing mipscall %i", callId);
	MipsCall *call = mipsCalls.get(callId);

	// Grab some MIPS stack space.
	u32 &sp = currentMIPS->r[MIPS_REG_SP];
	if (!Memory::IsValidAddress(sp - 32 * 4)) {
		ERROR_LOG_REPORT(SCEKERNEL, "__KernelExecuteMipsCallOnCurrentThread(): Not enough free stack");
		return false;
	}

	// Let's just save regs generously.  Better to be safe.
	sp -= 32 * 4;
	for (int i = MIPS_REG_A0; i <= MIPS_REG_T7; ++i) {
		Memory::Write_U32(currentMIPS->r[i], sp + i * 4);
	}
	Memory::Write_U32(currentMIPS->r[MIPS_REG_T8], sp + MIPS_REG_T8 * 4);
	Memory::Write_U32(currentMIPS->r[MIPS_REG_T9], sp + MIPS_REG_T9 * 4);
	Memory::Write_U32(currentMIPS->r[MIPS_REG_RA], sp + MIPS_REG_RA * 4);

	// Save the few regs that need saving
	call->savedPc = currentMIPS->pc;
	call->savedV0 = currentMIPS->r[MIPS_REG_V0];
	call->savedV1 = currentMIPS->r[MIPS_REG_V1];
	call->savedId = cur->currentMipscallId;
	call->reschedAfter = reschedAfter;

	// Set up the new state
	currentMIPS->pc = call->entryPoint;
	currentMIPS->r[MIPS_REG_RA] = __KernelMipsCallReturnAddress();
	cur->currentMipscallId = callId;
	for (int i = 0; i < call->numArgs; i++) {
		currentMIPS->r[MIPS_REG_A0 + i] = call->args[i];
	}

	if (call->cbId != 0)
		g_inCbCount++;
	currentCallbackThreadID = currentThread;

	return true;
}

void __KernelReturnFromMipsCall()
{
	hleSkipDeadbeef();

	Thread *cur = __GetCurrentThread();
	if (cur == NULL)
	{
		ERROR_LOG(SCEKERNEL, "__KernelReturnFromMipsCall(): Bad current thread");
		return;
	}

	u32 callId = cur->currentMipscallId;
	MipsCall *call = mipsCalls.pop(callId);

	// Value returned by the callback function
	u32 retVal = currentMIPS->r[MIPS_REG_V0];
	DEBUG_LOG(SCEKERNEL,"__KernelReturnFromMipsCall(), returned %08x", retVal);

	// Should also save/restore wait state here.
	if (call->doAfter)
	{
		call->doAfter->run(*call);
		delete call->doAfter;
	}

	u32 &sp = currentMIPS->r[MIPS_REG_SP];
	for (int i = MIPS_REG_A0; i <= MIPS_REG_T7; ++i) {
		currentMIPS->r[i] = Memory::Read_U32(sp + i * 4);
	}
	currentMIPS->r[MIPS_REG_T8] = Memory::Read_U32(sp + MIPS_REG_T8 * 4);
	currentMIPS->r[MIPS_REG_T9] = Memory::Read_U32(sp + MIPS_REG_T9 * 4);
	currentMIPS->r[MIPS_REG_RA] = Memory::Read_U32(sp + MIPS_REG_RA * 4);
	sp += 32 * 4;

	currentMIPS->pc = call->savedPc;
	// This is how we set the return value.
	currentMIPS->r[MIPS_REG_V0] = call->savedV0;
	currentMIPS->r[MIPS_REG_V1] = call->savedV1;
	cur->currentMipscallId = call->savedId;

	// If the thread called ExitDelete, we might've already decreased g_inCbCount.
	if (call->cbId != 0 && g_inCbCount > 0) {
		g_inCbCount--;
	}
	currentCallbackThreadID = 0;

	if (cur->nt.waitType != WAITTYPE_NONE)
	{
		if (call->cbId > 0)
		{
			if (waitTypeFuncs[cur->nt.waitType].endFunc != NULL)
				waitTypeFuncs[cur->nt.waitType].endFunc(cur->GetUID(), cur->currentCallbackId);
			else
				ERROR_LOG_REPORT(HLE, "Missing begin/restore funcs for wait type %d", cur->nt.waitType);
		}
	}

	// yeah! back in the real world, let's keep going. Should we process more callbacks?
	if (!__KernelExecutePendingMipsCalls(cur, call->reschedAfter)) {
		// Sometimes, we want to stay on the thread.
		int threadReady = cur->nt.status & (THREADSTATUS_READY | THREADSTATUS_RUNNING);
		if (call->reschedAfter || threadReady == 0)
			__KernelReSchedule("return from callback");

		// Now seems like a good time to clear out any pending deletes.
		for (SceUID delThread : pendingDeleteThreads) {
			kernelObjects.Destroy<Thread>(delThread);
		}
		pendingDeleteThreads.clear();
	}

	delete call;
}

// First arg must be current thread, passed to avoid perf cost of a lookup.
bool __KernelExecutePendingMipsCalls(Thread *thread, bool reschedAfter)
{
	_dbg_assert_msg_(SCEKERNEL, thread->GetUID() == __KernelGetCurThread(), "__KernelExecutePendingMipsCalls() should be called only with the current thread.");

	if (thread->pendingMipsCalls.empty()) {
		// Nothing to do
		return false;
	}

	if (__CanExecuteCallbackNow(thread))
	{
		// Pop off the first pending mips call
		u32 callId = thread->pendingMipsCalls.front();
		thread->pendingMipsCalls.pop_front();
		if (__KernelExecuteMipsCallOnCurrentThread(callId, reschedAfter)) {
			return true;
		}
	}
	return false;
}

// Executes the callback, when it next is context switched to.
static void __KernelRunCallbackOnThread(SceUID cbId, Thread *thread, bool reschedAfter)
{
	u32 error;
	Callback *cb = kernelObjects.Get<Callback>(cbId, error);
	if (!cb) {
		ERROR_LOG(SCEKERNEL, "__KernelRunCallbackOnThread: Bad cbId %i", cbId);
		return;
	}

	DEBUG_LOG(SCEKERNEL, "__KernelRunCallbackOnThread: Turning callback %i into pending mipscall", cbId);

	// Alright, we're on the right thread
	// Should save/restore wait state?

	const u32 args[] = {(u32) cb->nc.notifyCount, (u32) cb->nc.notifyArg, cb->nc.commonArgument};

	// Clear the notify count / arg
	cb->nc.notifyCount = 0;
	cb->nc.notifyArg = 0;

	ActionAfterCallback *action = (ActionAfterCallback *) __KernelCreateAction(actionAfterCallback);
	if (action != NULL)
		action->setCallback(cbId);
	else
		ERROR_LOG(SCEKERNEL, "Something went wrong creating a restore action for a callback.");

	__KernelCallAddress(thread, cb->nc.entrypoint, action, args, 3, reschedAfter, cbId);
}

void ActionAfterCallback::run(MipsCall &call) {
	if (cbId != -1) {
		u32 error;
		Callback *cb = kernelObjects.Get<Callback>(cbId, error);
		if (cb)
		{
			Thread *t = kernelObjects.Get<Thread>(cb->nc.threadId, error);
			if (t)
			{
				// Check for other callbacks to run (including ones this callback scheduled.)
				__KernelCheckThreadCallbacks(t, true);
			}

			DEBUG_LOG(SCEKERNEL, "Left callback %i - %s", cbId, cb->nc.name);
			// Callbacks that don't return 0 are deleted. But should this be done here?
			if (currentMIPS->r[MIPS_REG_V0] != 0)
			{
				DEBUG_LOG(SCEKERNEL, "ActionAfterCallback::run(): Callback returned non-zero, gets deleted!");
				kernelObjects.Destroy<Callback>(cbId);
			}
		}
	}
}

bool __KernelCurHasReadyCallbacks() {
	if (readyCallbacksCount == 0) {
		return false;
	}

	Thread *thread = __GetCurrentThread();
	u32 error;
	for (auto it = thread->callbacks.begin(), end = thread->callbacks.end(); it != end; ++it) {
		Callback *callback = kernelObjects.Get<Callback>(*it, error);
		if (callback && callback->nc.notifyCount != 0) {
			return true;
		}
	}
	return false;
}

// Check callbacks on the current thread only.
// Returns true if any callbacks were processed on the current thread.
bool __KernelCheckThreadCallbacks(Thread *thread, bool force) {
	if (!thread || (!thread->isProcessingCallbacks && !force)) {
		return false;
	}

	if (!thread->callbacks.empty()) {
		u32 error;
		for (auto it = thread->callbacks.begin(), end = thread->callbacks.end(); it != end; ++it) {
			Callback *callback = kernelObjects.Get<Callback>(*it, error);
			if (callback && callback->nc.notifyCount != 0) {
				__KernelRunCallbackOnThread(callback->GetUID(), thread, !force);
				readyCallbacksCount--;
				return true;
			}
		}
	}
	return false;
}

// Checks for callbacks on all threads
bool __KernelCheckCallbacks() {
	// Let's not check every thread all the time, callbacks are fairly uncommon.
	if (readyCallbacksCount == 0) {
		return false;
	}
	if (readyCallbacksCount < 0) {
		ERROR_LOG_REPORT(SCEKERNEL, "readyCallbacksCount became negative: %i", readyCallbacksCount);
	}
	if (__IsInInterrupt() || !__KernelIsDispatchEnabled() || __KernelInCallback()) {
		// TODO: Technically, other callbacks can run when a thread within a callback is waiting.
		// However, callbacks that were pending before the current callback started won't be run.
		// This is pretty uncommon, and not yet handled correctly.
		return false;
	}

	bool processed = false;

	u32 error;
	for (auto iter = threadqueue.begin(); iter != threadqueue.end(); ++iter) {
		Thread *thread = kernelObjects.Get<Thread>(*iter, error);
		if (thread && __KernelCheckThreadCallbacks(thread, false)) {
			processed = true;
		}
	}

	if (processed) {
		return __KernelExecutePendingMipsCalls(__GetCurrentThread(), true);
	}
	return false;
}

bool __KernelForceCallbacks()
{
	// Let's not check every thread all the time, callbacks are fairly uncommon.
	if (readyCallbacksCount == 0) {
		return false;
	}
	if (readyCallbacksCount < 0) {
		ERROR_LOG_REPORT(SCEKERNEL, "readyCallbacksCount became negative: %i", readyCallbacksCount);
	}

	Thread *curThread = __GetCurrentThread();	

	bool callbacksProcessed = __KernelCheckThreadCallbacks(curThread, true);
	if (callbacksProcessed)
		__KernelExecutePendingMipsCalls(curThread, false);

	return callbacksProcessed;
}

// Not wrapped because it has special return logic.
void sceKernelCheckCallback()
{
	// Start with yes.
	RETURN(1);

	bool callbacksProcessed = __KernelForceCallbacks();

	if (callbacksProcessed) {
		DEBUG_LOG(SCEKERNEL, "sceKernelCheckCallback() - processed a callback.");
		// The RETURN(1) above is still active here, unless __KernelForceCallbacks changed it.
	} else {
		RETURN(0);
	}
	hleEatCycles(230);
}

bool __KernelInCallback()
{
	return (g_inCbCount != 0);
}

void __KernelNotifyCallback(SceUID cbId, int notifyArg)
{
	u32 error;

	Callback *cb = kernelObjects.Get<Callback>(cbId, error);
	if (!cb) {
		// Yeah, we're screwed, this shouldn't happen.
		ERROR_LOG(SCEKERNEL, "__KernelNotifyCallback - invalid callback %08x", cbId);
		return;
	}
	if (cb->nc.notifyCount == 0) {
		readyCallbacksCount++;
	}
	cb->nc.notifyCount++;
	cb->nc.notifyArg = notifyArg;
}

void __KernelRegisterWaitTypeFuncs(WaitType type, WaitBeginCallbackFunc beginFunc, WaitEndCallbackFunc endFunc)
{
	waitTypeFuncs[type].beginFunc = beginFunc;
	waitTypeFuncs[type].endFunc = endFunc;
}

std::vector<DebugThreadInfo> GetThreadsInfo()
{
	std::vector<DebugThreadInfo> threadList;

	u32 error;
	for (auto iter = threadqueue.begin(); iter != threadqueue.end(); ++iter)
	{
		Thread *t = kernelObjects.Get<Thread>(*iter, error);
		if (!t)
			continue;

		DebugThreadInfo info;
		info.id = *iter;
		strncpy(info.name,t->GetName(),KERNELOBJECT_MAX_NAME_LENGTH);
		info.name[KERNELOBJECT_MAX_NAME_LENGTH] = 0;
		info.status = t->nt.status;
		info.entrypoint = t->nt.entrypoint;
		info.initialStack = t->nt.initialStack;
		info.stackSize = (u32)t->nt.stackSize;
		info.priority = t->nt.currentPriority;
		info.waitType = (WaitType)(u32)t->nt.waitType;
		if(*iter == currentThread)
			info.curPC = currentMIPS->pc;
		else
			info.curPC = t->context.pc;
		info.isCurrent = (*iter == currentThread);
		threadList.push_back(info);
	}

	return threadList;
}

void __KernelChangeThreadState(SceUID threadId, ThreadStatus newStatus)
{
	u32 error;
	Thread *t = kernelObjects.Get<Thread>(threadId, error);
	if (!t)
		return;

	__KernelChangeThreadState(t, newStatus);
}

int sceKernelRegisterExitCallback(SceUID cbId)
{
	u32 error;
	Callback *cb = kernelObjects.Get<Callback>(cbId, error);
	if (!cb)
	{
		WARN_LOG(SCEKERNEL, "sceKernelRegisterExitCallback(%i): invalid callback id", cbId);
		if (sceKernelGetCompiledSdkVersion() >= 0x3090500)
			return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
		return 0;
	}

	DEBUG_LOG(SCEKERNEL, "sceKernelRegisterExitCallback(%i)", cbId);
	registeredExitCbId = cbId;
	return 0;
}

int LoadExecForUser_362A956B()
{
	WARN_LOG_REPORT(SCEKERNEL, "LoadExecForUser_362A956B()");
	u32 error;
	Callback *cb = kernelObjects.Get<Callback>(registeredExitCbId, error);
	if (!cb) {
		WARN_LOG(SCEKERNEL, "LoadExecForUser_362A956B() : registeredExitCbId not found 0x%x", registeredExitCbId);
		return SCE_KERNEL_ERROR_UNKNOWN_CBID;
	}
	int cbArg = cb->nc.commonArgument;
	if (!Memory::IsValidAddress(cbArg)) {
		WARN_LOG(SCEKERNEL, "LoadExecForUser_362A956B() : invalid address for cbArg (0x%08X)", cbArg);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}
	u32 unknown1 = Memory::Read_U32(cbArg - 8);
	if (unknown1 >= 4) {
		WARN_LOG(SCEKERNEL, "LoadExecForUser_362A956B() : invalid value unknown1 (0x%08X)", unknown1);
		return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
	}
	u32 parameterArea = Memory::Read_U32(cbArg - 4);
	if (!Memory::IsValidAddress(parameterArea)) {
		WARN_LOG(SCEKERNEL, "LoadExecForUser_362A956B() : invalid address for parameterArea on userMemory  (0x%08X)", parameterArea);
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}
	
	u32 size = Memory::Read_U32(parameterArea);
	if (size < 12) {
		WARN_LOG(SCEKERNEL, "LoadExecForUser_362A956B() : invalid parameterArea size %d", size);
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	}
	Memory::Write_U32(0, parameterArea + 4);
	Memory::Write_U32(-1, parameterArea + 8);
	return 0;
}

static const SceUID SCE_TE_THREADID_ALL_USER = 0xFFFFFFF0;

struct NativeThreadEventHandler {
	u32 size;
	char name[KERNELOBJECT_MAX_NAME_LENGTH + 1];
	SceUID threadID;
	u32 mask;
	u32 handlerPtr;
	u32 commonArg;
};

struct ThreadEventHandler : public KernelObject {
	const char *GetName() { return nteh.name; }
	const char *GetTypeName() { return "ThreadEventHandler"; }
	static u32 GetMissingErrorCode() { return SCE_KERNEL_ERROR_UNKNOWN_TEID; }
	static int GetStaticIDType() { return SCE_KERNEL_TMID_ThreadEventHandler; }
	int GetIDType() const { return SCE_KERNEL_TMID_ThreadEventHandler; }

	virtual void DoState(PointerWrap &p) {
		auto s = p.Section("ThreadEventHandler", 1);
		if (!s)
			return;

		p.Do(nteh);
	}

	NativeThreadEventHandler nteh;
};

KernelObject *__KernelThreadEventHandlerObject() {
	// Default object to load from state.
	return new ThreadEventHandler;
}

bool __KernelThreadTriggerEvent(const ThreadEventHandlerList &handlers, SceUID threadID, ThreadEventType type) {
	Thread *thread = __GetCurrentThread();
	if (!thread || thread->isStopped()) {
		SceUID nextThreadID = threadReadyQueue.peek_first();
		thread = kernelObjects.GetFast<Thread>(nextThreadID);
	}

	bool hadHandlers = false;
	for (auto it = handlers.begin(), end = handlers.end(); it != end; ++it) {
		u32 error;
		const auto teh = kernelObjects.Get<ThreadEventHandler>(*it, error);
		if (!teh || (teh->nteh.mask & type) == 0) {
			continue;
		}

		const u32 args[] = {(u32)type, (u32)threadID, teh->nteh.commonArg};
		__KernelCallAddress(thread, teh->nteh.handlerPtr, nullptr, args, ARRAY_SIZE(args), true, 0);
		hadHandlers = true;
	}

	return hadHandlers;
}

bool __KernelThreadTriggerEvent(bool isKernel, SceUID threadID, ThreadEventType type) {
	bool hadExactHandlers = false;
	auto exactHandlers = threadEventHandlers.find(threadID);
	if (exactHandlers != threadEventHandlers.end()) {
		hadExactHandlers = __KernelThreadTriggerEvent(exactHandlers->second, threadID, type);
	}

	bool hadKindHandlers = false;
	if (isKernel) {
		auto kernelHandlers = threadEventHandlers.find(SCE_TE_THREADID_ALL_USER);
		if (kernelHandlers != threadEventHandlers.end()) {
			hadKindHandlers = __KernelThreadTriggerEvent(kernelHandlers->second, threadID, type);
		}
	} else {
		auto userHandlers = threadEventHandlers.find(SCE_TE_THREADID_ALL_USER);
		if (userHandlers != threadEventHandlers.end()) {
			hadKindHandlers = __KernelThreadTriggerEvent(userHandlers->second, threadID, type);
		}
	}

	return hadKindHandlers || hadExactHandlers;
}

SceUID sceKernelRegisterThreadEventHandler(const char *name, SceUID threadID, u32 mask, u32 handlerPtr, u32 commonArg) {
	if (!name) {
		return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_ERROR, "invalid name");
	}
	if (threadID == 0) {
		// "atexit"?
		if (mask != THREADEVENT_EXIT) {
			return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_ATTR, "invalid thread id");
		}
	}
	u32 error;
	if (kernelObjects.Get<Thread>(threadID, error) == NULL && threadID != SCE_TE_THREADID_ALL_USER) {
		return hleReportError(SCEKERNEL, error, "bad thread id");
	}
	if ((mask & ~THREADEVENT_SUPPORTED) != 0) {
		return hleReportError(SCEKERNEL, SCE_KERNEL_ERROR_ILLEGAL_MASK, "invalid event mask");
	}

	auto teh = new ThreadEventHandler;
	teh->nteh.size = sizeof(teh->nteh);
	strncpy(teh->nteh.name, name, KERNELOBJECT_MAX_NAME_LENGTH);
	teh->nteh.name[KERNELOBJECT_MAX_NAME_LENGTH] = '\0';
	teh->nteh.threadID = threadID;
	teh->nteh.mask = mask;
	teh->nteh.handlerPtr = handlerPtr;
	teh->nteh.commonArg = commonArg;

	SceUID uid = kernelObjects.Create(teh);
	threadEventHandlers[threadID].push_back(uid);

	return hleLogSuccessI(SCEKERNEL, uid);
}

int sceKernelReleaseThreadEventHandler(SceUID uid) {
	u32 error;
	auto teh = kernelObjects.Get<ThreadEventHandler>(uid, error);
	if (!teh) {
		return hleReportError(SCEKERNEL, error, "bad handler id");
	}

	auto &handlers = threadEventHandlers[teh->nteh.threadID];
	handlers.erase(std::remove(handlers.begin(), handlers.end(), uid), handlers.end());
	return hleLogSuccessI(SCEKERNEL, kernelObjects.Destroy<ThreadEventHandler>(uid));
}

int sceKernelReferThreadEventHandlerStatus(SceUID uid, u32 infoPtr) {
	u32 error;
	auto teh = kernelObjects.Get<ThreadEventHandler>(uid, error);
	if (!teh) {
		return hleReportError(SCEKERNEL, error, "bad handler id");
	}

	if (Memory::IsValidAddress(infoPtr) && Memory::Read_U32(infoPtr) != 0) {
		Memory::WriteStruct(infoPtr, &teh->nteh);
		return hleLogSuccessI(SCEKERNEL, 0);
	} else {
		return hleLogDebug(SCEKERNEL, 0, "struct size was 0");
	}
}
