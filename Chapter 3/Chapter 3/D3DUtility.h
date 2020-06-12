#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <tchar.h>
#include <Windows.h>
#include <string>
#include <vector>

#include "D3D_DESC.h"

#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")

using namespace std;

namespace D3D {
	bool InitD3D (HINSTANCE hInstance, int width, int height, D3D_DESC* pDesc);

	LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	int EnterMsgLoop (bool (*ptr_display)(float deltaTime));
}