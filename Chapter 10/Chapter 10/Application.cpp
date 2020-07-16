#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"

#include <string>
#include <Windows.h>

#pragma comment (lib, "winmm.lib")

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

const std::string PATH_MIKU = "Model/‰‰¹ƒ~ƒN.pmd";
const std::string PATH_LUKA = "Model/„‰¹ƒ‹ƒJ.pmd";

LRESULT WindowProcedure (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_DESTROY:
			PostQuitMessage (0);
			break;
	}

	return DefWindowProc (hwnd, msg, wParam, lParam);
}

void Application::CreateGameWindow (HWND& hwnd, WNDCLASSEX& windowClass) {
	HINSTANCE hInst = GetModuleHandle (nullptr);
	
	windowClass.cbSize = sizeof (WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;
	windowClass.lpszClassName = _T ("DirectXTest");
	windowClass.hInstance = GetModuleHandle (0);
	RegisterClassEx (&windowClass);

	RECT wrc = {0,0, window_width, window_height};
	AdjustWindowRect (&wrc, WS_OVERLAPPEDWINDOW, false);
	
	hwnd = CreateWindow (windowClass.lpszClassName,
						 _T ("DX12"),
						 WS_OVERLAPPEDWINDOW,
						 CW_USEDEFAULT,
						 CW_USEDEFAULT,
						 wrc.right - wrc.left,
						 wrc.bottom - wrc.top,
						 nullptr,
						 nullptr,
						 windowClass.hInstance,
						 nullptr);
}

void DisplayFPS (float deltaTime) {
	static float lastTime = (float)timeGetTime ();
	static DWORD frameCnt = 0;
	static float timeElapsed = 0.0f;
	static float fps = 0.0f;

	frameCnt++;
	timeElapsed += deltaTime;
	if (timeElapsed >= 1.0f) {
		fps = (float)frameCnt / timeElapsed;
		printf_s ("FPS : %.1f\n", fps);
		timeElapsed = 0.0f;
		frameCnt = 0;
	}
}

SIZE Application::GetWindowSize ()const {
	SIZE ret;
	ret.cx = window_width;
	ret.cy = window_height;
	return ret;
}

void Application::Run () {
	ShowWindow (_hwnd, SW_SHOW);

	MSG msg = {};
	unsigned int frame = 0;

	static float lastTime = (float)timeGetTime ();

	while (msg.message != WM_QUIT) {
		if (PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}

		float currTime = (float)timeGetTime ();
		float deltaTime = (currTime - lastTime) * 0.001f;

		DisplayFPS (deltaTime);

		_dx12->BeginDraw ();

		_dx12->CommandList ()->SetPipelineState (_pmdRenderer->GetPipelineState ());
		
		_dx12->CommandList ()->SetGraphicsRootSignature (_pmdRenderer->GetRootSignature ());

		_dx12->CommandList ()->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		_dx12->SetScene ();

		_pmdActor->Update ();
		_pmdActor->Draw ();

		_dx12->EndDraw ();

		_dx12->Swapchain ()->Present (1, 0);

		lastTime = currTime;
	}
}

bool Application::Init () {
	auto result = CoInitializeEx (0, COINIT_MULTITHREADED);
	CreateGameWindow (_hwnd, _windowClass);

	_dx12.reset (new DX12Wrapper (_hwnd));
	_pmdRenderer.reset (new PMDRenderer (*_dx12));
	_pmdActor.reset (new PMDActor (PATH_MIKU.c_str (), *_pmdRenderer));

	return true;
}

void Application::Terminate () {
	UnregisterClass (_windowClass.lpszClassName, _windowClass.hInstance);
}

Application& Application::Instance () {
	static Application instance;
	return instance;
}

Application::Application () {}

Application::~Application () {}