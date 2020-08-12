#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"

#include <string>
#include <Windows.h>

#pragma comment (lib, "winmm.lib")

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

const std::string PATH_MIKU = "Model/Miku.pmd";
const std::string PATH_LUKA = "Model/Luka.pmd";

const std::string PATH_MOTION_MOTION = "motion/squat.vmd";

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

#pragma region Test
		//_dx12->PreDrawToPera1 ();
		//_dx12->Clear ();
		//_pmdRenderer->BeforeDraw ();
		//_dx12->DrawToPera1 (_pmdRenderer);
		//_pmdActor->Update ();
		//_pmdActor->Draw ();
		//// _dx12->Draw (_pmdRenderer);
		//_dx12->Flip ();
#pragma endregion

		_dx12->PreDrawToPera1 ();
		//_pmdRenderer->Update ();
		_pmdActor->Update ();
		_pmdRenderer->BeforeDraw ();
		_dx12->DrawToPera1 (_pmdRenderer);
		//_pmdRenderer->Draw ();
		_pmdActor->Draw ();
		_dx12->PostDrawToPera1 ();
		_dx12->DrawHorizontalBokeh();
		_dx12->Clear();
		_dx12->Draw (_pmdRenderer);
		_dx12->Flip();

		lastTime = currTime;
	}
}

bool Application::Init () {
	auto result = CoInitializeEx (0, COINIT_MULTITHREADED);
	CreateGameWindow (_hwnd, _windowClass);

	_dx12.reset (new DX12Wrapper (_hwnd));
	_pmdRenderer.reset (new PMDRenderer (*_dx12));
	if (!_dx12->Init ()) {
		return false;
	}
	_pmdRenderer->Init ();

	_pmdActor.reset (new PMDActor (PATH_MIKU.c_str (), *_pmdRenderer));
	_pmdActor->LoadVMDFile (PATH_MOTION_MOTION.c_str (), "pose");
	_pmdActor->PlayAnimation ();

	//_pmdRenderer->AddActor (_pmdActor);

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