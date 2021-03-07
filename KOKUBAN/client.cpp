#include <iostream>
#include <windows.h>
#include <stdio.h>

#include <HD/hd.h>
#include <HDU/hduError.h>
#include <HDU/hduMatrix.h>

#define WIN32
#include "network.cpp"
double xyz[3];
double gForce2[3];

#pragma comment( lib, "ws2_32.lib" )

// --- グローバル変数 ---
hduVector3Dd gStylusPos, gForce;
HHD ghHD = HD_INVALID_HANDLE;
HDSchedulerHandle gSchedulerCallback = HD_INVALID_HANDLE;

// --- 力覚スケジューラ ---
HDCallbackCode HDCALLBACK ContactCB(void* data)
{
	static int sock;
	static bool first = true;

	if (first) {
		sock = udp_open((char*)"192.168.11.39", 11111, 11110);
		first = false;
	}

	HHD hHD = hdGetCurrentDevice();
	hdBeginFrame(hHD);
	hdGetDoublev(HD_CURRENT_POSITION, gStylusPos);
	gForce[0] = gForce2[0];
	gForce[1] = gForce2[1];
	gForce[2] = gForce2[2];
	hdSetDoublev(HD_CURRENT_FORCE, gForce);
	hdEndFrame(hHD);
	HDErrorInfo error;
	if (HD_DEVICE_ERROR(error = hdGetError())) {
		hduPrintError(stderr, &error, "力覚スケジューラ内でエラーが発生しました。");
		if (hduIsSchedulerError(&error)) {
			return HD_CALLBACK_DONE;
		}
	}

	
	xyz[0] = gStylusPos[0];
	xyz[1] = gStylusPos[1];
	xyz[2] = gStylusPos[2];
	double size = udp_write(sock, (char*)xyz, sizeof(double) * 3);
	size = udp_read(sock, (char*)gForce2, sizeof(double) * 3);
	std::cout << gForce2[0] << "," << gForce2[1] << "," << gForce2[2] << std::endl;



	return HD_CALLBACK_CONTINUE;
}

// --- 終了処理 ---
void exitHandler()
{
	hdStopScheduler();
	if (ghHD != HD_INVALID_HANDLE) {
		hdDisableDevice(ghHD);
		ghHD = HD_INVALID_HANDLE;
	}
	printf("終了。\n");
}

int main()
{
	HDErrorInfo error;
	printf("起動します\n");
	atexit(exitHandler);
	ghHD = hdInitDevice(HD_DEFAULT_DEVICE);
	hdEnable(HD_FORCE_OUTPUT);
	hdEnable(HD_MAX_FORCE_CLAMPING);
	hdStartScheduler();
	gSchedulerCallback = hdScheduleAsynchronous(ContactCB, NULL, HD_DEFAULT_SCHEDULER_PRIORITY);

	while (1) {
		Sleep(1000);
	}
}