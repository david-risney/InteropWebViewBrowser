#include "stdafx.h"
#include "Win32WebViewSampleApp.h"
#include <Commdlg.h>
#include <Shellapi.h>

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;
using namespace ABI::Windows::Web::UI;
using namespace ABI::Windows::Web::UI::Interop;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

const WCHAR App::s_hostClass[] = L"Win32WebViewSampleAppHost"; // the host window class name

void CheckFailure(_In_ HRESULT hr)
{
	if (FAILED(hr))
	{
		WCHAR message[512] = L"";
		StringCchPrintf(message, sizeof(message) / sizeof(*message), L"Error: 0x%x", hr);
		MessageBoxW(nullptr, message, nullptr, MB_OK);
		ExitProcess(-1);
	}
}

template <typename TInterface>
Microsoft::WRL::ComPtr<TInterface> GetActivationFactoryFailFast(_In_z_ PCWSTR factoryClassName)
{
	ComPtr<TInterface> factoryInstance;
	CheckFailure(RoGetActivationFactory(
		HStringReference(factoryClassName).Get(),
		IID_PPV_ARGS(&factoryInstance)));
	return factoryInstance;
}

template <typename TInterface>
Microsoft::WRL::ComPtr<TInterface> ActivateInstanceFailFast(_In_z_ PCWSTR className)
{
	ComPtr<TInterface> classInstanceAsInspectable;
	ComPtr<TInterface> classInstance;
	CheckFailure(RoActivateInstance(
		HStringReference(className).Get(),
		&classInstanceAsInspectable));
	CheckFailure(classInstanceAsInspectable.As(&classInstance));
	return classInstance;
}

ComPtr<IUriRuntimeClass> CreateWinRtUri(_In_z_ PCWSTR uri, _In_ bool allowInvalidUri = false)
{
	auto uriRuntimeClassFactory = GetActivationFactoryFailFast<IUriRuntimeClassFactory>(RuntimeClass_Windows_Foundation_Uri);
	ComPtr<IUriRuntimeClass> uriRuntimeClass;
	if (!allowInvalidUri)
	{
		CheckFailure(uriRuntimeClassFactory->CreateUri(HStringReference(uri).Get(), &uriRuntimeClass));
	}
	else
	{
		uriRuntimeClassFactory->CreateUri(HStringReference(uri).Get(), &uriRuntimeClass);
	}
	return uriRuntimeClass;
}

Rect HwndWindowRectToBoundsRect(_In_ HWND hwnd)
{
	RECT windowRect = { 0 };
	GetWindowRect(hwnd, &windowRect);

	Rect bounds =
	{
		0,
		0,
		static_cast<float>(windowRect.right - windowRect.left),
		static_cast<float>(windowRect.bottom - windowRect.top)
	};

	return bounds;
}

void WebViewWindow::InitializeAsync(HWND parent, RECT position, std::function<void()> initializeCallback)
{
	ComPtr<IWebViewControlProcess> process = ActivateInstanceFailFast<IWebViewControlProcess>(RuntimeClass_Windows_Web_UI_Interop_WebViewControlProcess);
	ComPtr<IAsyncOperation<WebViewControl*>> createWebViewAsyncOperation;
	this->m_window = CreateWindowW(
		App::s_hostClass, L"", (WS_CHILD | WS_VISIBLE),
		position.left, position.top, position.right - position.left, position.bottom - position.top,
		parent, (HMENU)45, nullptr, 0);
	InvalidateRect(this->m_window, nullptr, TRUE);
	CheckFailure(process->CreateWebViewControlAsync(
		reinterpret_cast<INT64>(m_window),
		HwndWindowRectToBoundsRect(m_window),
		&createWebViewAsyncOperation));

	createWebViewAsyncOperation->put_Completed((Callback<IAsyncOperationCompletedHandler<WebViewControl*>>([this, parent, position, initializeCallback](IAsyncOperation<WebViewControl*>* operation, AsyncStatus status) -> HRESULT
	{
		CheckFailure(operation->GetResults(&this->m_webview));

		Resize(position);
		initializeCallback();

		return S_OK;
	}).Get()));
}

void WebViewWindow::Resize(RECT position)
{
	SetWindowPos(m_window, nullptr, position.left, position.top, position.right - position.left, position.bottom - position.top, SWP_NOZORDER);

	Rect bounds = HwndWindowRectToBoundsRect(m_window);
	ComPtr<IWebViewControlSite> site;
	CheckFailure(m_webview.As(&site));
	CheckFailure(site->put_Bounds(bounds));
}

void WebViewWindow::Show(bool show) {
	ShowWindow(this->m_window, show ? SW_SHOWNA : SW_HIDE);
	UpdateWindow(this->m_window);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	return App::RunNewInstance(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

int App::Run(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	m_cmdline = lpCmdLine;
	m_nShow = nCmdShow;
    UNREFERENCED_PARAMETER(hPrevInstance);

	RoInitialize(RO_INIT_SINGLETHREADED);

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    LoadStringW(hInstance, IDC_WIN32WEBVIEWSAMPLEAPP, m_windowClass, s_maxLoadString);
	
    MyRegisterClasses(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WIN32WEBVIEWSAMPLEAPP));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

void App::MyRegisterClasses(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProcStatic;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WIN32WEBVIEWSAMPLEAPP));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_WIN32WEBVIEWSAMPLEAPP);
    wcex.lpszClassName  = m_windowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassExW(&wcex);

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProcStatic;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = nullptr;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = App::s_hostClass;
	wcex.hIconSm = nullptr;

	RegisterClassExW(&wcex);
}

BOOL App::InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   m_hInst = hInstance; // Store instance handle in our global variable

   WCHAR szTitle[s_maxLoadString];                  // The title bar text
   LoadStringW(hInstance, IDS_APP_TITLE, szTitle, s_maxLoadString);
   m_mainWindow = CreateWindowW(m_windowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
   SetWindowLongPtr(m_mainWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

   RECT mainWindowBounds = { 0 };
   GetClientRect(m_mainWindow, &mainWindowBounds);

   RECT controlBounds = { 0 };
   controlBounds.right = mainWindowBounds.right;
   controlBounds.bottom = App::s_controlWebViewHeight;
   this->m_controlWebView.InitializeAsync(this->m_mainWindow, controlBounds, [this, mainWindowBounds, nCmdShow]() {
   
	   m_controlWebView.m_webview->Navigate(CreateWinRtUri(L"https://david-risney.github.io/InteropWebViewBrowser/Win32WebViewSampleApp/controls.html").Get());
	   AddTab([this, nCmdShow]() {
		   ShowWindow(m_mainWindow, nCmdShow);
		   UpdateWindow(m_mainWindow);

		   this->m_controlWebView.Show(true);
		   this->m_tabs[0].Show(true);

		   PostMessage(m_mainWindow, s_initializeWindowMessage, 0, 0);
	   });
   });

   return TRUE;
}

void App::AddTab(std::function<void()> callback) {
	RECT mainWindowBounds = { 0 };
	GetClientRect(m_mainWindow, &mainWindowBounds);
	RECT tabBounds = { 0 };
	tabBounds.right = mainWindowBounds.right;
	tabBounds.top = App::s_controlWebViewHeight;
	tabBounds.bottom = mainWindowBounds.bottom - App::s_controlWebViewHeight;

	m_tabs.push_back(WebViewWindow());
	m_tabs.back().InitializeAsync(this->m_mainWindow, tabBounds, [callback]() {
		// setup event handlers
		callback();
	});
}

WebViewWindow& App::GetCurrentTab() {
	return m_tabs[m_tabIdx];
}

LRESULT CALLBACK App::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, _Out_ bool* handled)
{
	//if (m_hostWindow != nullptr && m_webViewControl != nullptr)
	{
		switch (message) {
		case WM_DESTROY: {
			ComPtr<IWebViewControlSite> webViewControlSite;

			m_controlWebView.m_webview->QueryInterface(IID_PPV_ARGS(&webViewControlSite));
			webViewControlSite->Close();

			for (const auto& tab : m_tabs) {
				tab.m_webview->QueryInterface(IID_PPV_ARGS(&webViewControlSite));
				webViewControlSite->Close();
			}
		} break;

		case WM_SIZE: {
			RECT hwndBounds = { 0 };
			GetClientRect(this->m_mainWindow, &hwndBounds);

			RECT controlBounds = { 0 };
			controlBounds.right = hwndBounds.right;
			controlBounds.bottom = App::s_controlWebViewHeight;
			this->m_controlWebView.Resize(controlBounds);

			RECT tabBounds = { 0 };
			tabBounds.top = App::s_controlWebViewHeight;
			tabBounds.bottom = hwndBounds.bottom;
			tabBounds.right = hwndBounds.right;
			for (auto& tab : m_tabs) {
				tab.Resize(tabBounds);
			}

			*handled = true;
		} break;

		case WM_COMMAND: {
			switch (LOWORD(wParam)) {
			case IDM_NEW_WINDOW: {
				//				App::RunNewInstance(m_hInst, nullptr, m_cmdline, m_nShow, m_process.Get(), m_processOptions.Get());
				*handled = true;
			} break;
			}
		} break;

        case WM_DPICHANGED: {
            RECT* const newWindowRect = (RECT*)lParam;
            SetWindowPos(hWnd,
                nullptr,
                newWindowRect->left,
                newWindowRect->top,
                newWindowRect->right - newWindowRect->left,
                newWindowRect->bottom - newWindowRect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        } break;


		}
	}

	if (!*handled)
	{
		switch (message)
		{
		case WM_COMMAND:
		{
			int wmId = LOWORD(wParam);
			// Parse the menu selections:
			switch (wmId)
			{
			case IDM_EXIT:
				DestroyWindow(hWnd);
				*handled = true;
				break;
			}
		}
		break;

		case s_initializeWindowMessage:
			InitializeWin32WebView();
			*handled = true;
			break;
		}
	}

	return 0;
}

LRESULT CALLBACK App::WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	bool handled = false;

	App* app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (app != nullptr)
	{
		result = app->WndProc(hWnd, message, wParam, lParam, &handled);
	}

	if (!handled)
	{
		result = 0;
		switch (message)
		{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			// TODO: Add any drawing code that uses hdc here...
			EndPaint(hWnd, &ps);
			handled = true;
		}
		break;
		case WM_DESTROY:
			PostQuitMessage(0);
			handled = true;
			break;

		}
	}

	if (!handled)
	{
		result = DefWindowProc(hWnd, message, wParam, lParam);
	}

	return result;
}
