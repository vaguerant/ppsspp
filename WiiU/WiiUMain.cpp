#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fstream>

#include <wiiu/os/systeminfo.h>
#include <wiiu/os/thread.h>
#include <wiiu/os/debug.h>
#include <wiiu/gx2/event.h>
#include <wiiu/ax/core.h>

#include <wiiu/ios.h>

#include "base/NativeApp.h"
#include "base/display.h"
#include "Core/Core.h"
#include "Common/Log.h"

#include "Common/GraphicsContext.h"
#include "WiiU/WiiUHost.h"

static int g_QuitRequested;
void System_SendMessage(const char *command, const char *parameter) {
	if (!strcmp(command, "finish")) {
		g_QuitRequested = true;
		UpdateUIState(UISTATE_EXIT);
		Core_Stop();
	}
}
int main(int argc, char **argv) {

	host = new WiiUHost();

	std::string app_name;
	std::string app_name_nice;
	std::string version;
	bool landscape;
	NativeGetAppInfo(&app_name, &app_name_nice, &landscape, &version);

	const char *argv_[] = {
		"sd:/ppsspp/PPSSPP.rpx",
//		"-d",
//		"-v",
//		"sd:/cube.elf",
		nullptr
	};
	NativeInit(sizeof(argv_) / sizeof(*argv_) - 1, argv_, "sd:/ppsspp/", "sd:/ppsspp/", nullptr);
#if 0
	UpdateScreenScale(854,480);
#else
	float dpi_scale = 1.0f;
	g_dpi = 96.0f;
	pixel_xres = 854;
	pixel_yres = 480;
	dp_xres = (float)pixel_xres * dpi_scale;
	dp_yres = (float)pixel_yres * dpi_scale;
	pixel_in_dps_x = (float)pixel_xres / dp_xres;
	pixel_in_dps_y = (float)pixel_yres / dp_yres;
	g_dpi_scale_x = dp_xres / (float)pixel_xres;
	g_dpi_scale_y = dp_yres / (float)pixel_yres;
	g_dpi_scale_real_x = g_dpi_scale_x;
	g_dpi_scale_real_y = g_dpi_scale_y;
#endif
	printf("Pixels: %i x %i\n", pixel_xres, pixel_yres);
	printf("Virtual pixels: %i x %i\n", dp_xres, dp_yres);

	g_Config.iGPUBackend = (int)GPUBackend::GX2;
	g_Config.bEnableSound = true;
	g_Config.bPauseExitsEmulator = false;
	g_Config.bPauseMenuExitsEmulator = false;
	//	g_Config.iCpuCore = (int)CPUCore::JIT;
	//	g_Config.iCpuCore = (int)CPUCore::IR_JIT;
	g_Config.iCpuCore = (int)CPUCore::INTERPRETER;
	g_Config.bSoftwareRendering = true;

	std::string error_string;
	GraphicsContext *ctx;
	host->InitGraphics(&error_string, &ctx);
	NativeInitGraphics(ctx);
	NativeResized();

	host->InitSound();
	while (true) {
		if (g_QuitRequested)
			break;

		if (!Core_IsActive())
			UpdateUIState(UISTATE_MENU);
		Core_Run(ctx);
	}
	host->ShutdownSound();

	NativeShutdownGraphics();
	NativeShutdown();

	return 0;
}

std::string System_GetProperty(SystemProperty prop) {
	switch (prop) {
	case SYSPROP_NAME:
		return "WiiU";
	case SYSPROP_LANGREGION:
		return "en_US";
	default:
		return "";
	}
}

int System_GetPropertyInt(SystemProperty prop) {
	switch (prop) {
	case SYSPROP_DISPLAY_REFRESH_RATE:
		return 60000; // 59940 ?
	case SYSPROP_DISPLAY_XRES:
		return 854;
	case SYSPROP_DISPLAY_YRES:
		return 480;
	case SYSPROP_DEVICE_TYPE:
		return DEVICE_TYPE_TV;
	case SYSPROP_AUDIO_SAMPLE_RATE:
	case SYSPROP_AUDIO_OPTIMAL_SAMPLE_RATE:
		return 48000;
	case SYSPROP_AUDIO_FRAMES_PER_BUFFER:
	case SYSPROP_AUDIO_OPTIMAL_FRAMES_PER_BUFFER:
		return AX_FRAME_SIZE;
	case SYSPROP_SYSTEMVERSION:
	default:
		return -1;
	}
}
bool System_GetPropertyBool(SystemProperty prop) {
	switch (prop) {
	case SYSPROP_APP_GOLD:
#ifdef GOLD
		return true;
#else
		return false;
#endif
	default:
		return false;
	}
}

void System_AskForPermission(SystemPermission permission) {}
PermissionStatus System_GetPermissionStatus(SystemPermission permission) { return PERMISSION_STATUS_GRANTED; }

void LaunchBrowser(const char *url) {}
void ShowKeyboard() {}
void Vibrate(int length_ms) {}