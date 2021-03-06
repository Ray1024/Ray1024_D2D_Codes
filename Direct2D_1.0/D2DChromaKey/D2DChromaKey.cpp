//-----------------------------------------------------------------
// 功能：色度键过滤示例，更多详细解释请参考：http://www.cnblogs.com/Ray1024/
// 作者：Ray1024
// 网址：http://www.cnblogs.com/Ray1024/
//-----------------------------------------------------------------

#include "D2DChromaKey.h"

//-----------------------------------------------------------------
// 从文件加载D2D位图
//-----------------------------------------------------------------
HRESULT LoadBitmapFromFile(
	ID2D1RenderTarget *pRenderTarget,
	IWICImagingFactory *pIWICFactory,
	LPCWSTR uri,
	UINT width,
	UINT height,
	ID2D1Bitmap **ppBitmap)
{
	IWICBitmapDecoder *pDecoder = NULL;
	IWICBitmapFrameDecode *pSource = NULL;
	IWICStream *pStream = NULL;
	IWICFormatConverter *pConverter = NULL;
	IWICBitmapScaler *pScaler = NULL;

	// 加载位图-------------------------------------------------
	HRESULT hr = pIWICFactory->CreateDecoderFromFilename(
		uri,
		NULL,
		GENERIC_READ,
		WICDecodeMetadataCacheOnLoad,
		&pDecoder
		);

	if (SUCCEEDED(hr))
	{
		hr = pDecoder->GetFrame(0, &pSource);
	}
	if (SUCCEEDED(hr))
	{
		hr = pIWICFactory->CreateFormatConverter(&pConverter);
	}

	if (SUCCEEDED(hr))
	{
		if (width != 0 || height != 0)
		{
			UINT originalWidth, originalHeight;
			hr = pSource->GetSize(&originalWidth, &originalHeight);
			if (SUCCEEDED(hr))
			{
				if (width == 0)
				{
					FLOAT scalar = static_cast<FLOAT>(height) / static_cast<FLOAT>(originalHeight);
					width = static_cast<UINT>(scalar * static_cast<FLOAT>(originalWidth));
				}
				else if (height == 0)
				{
					FLOAT scalar = static_cast<FLOAT>(width) / static_cast<FLOAT>(originalWidth);
					height = static_cast<UINT>(scalar * static_cast<FLOAT>(originalHeight));
				}

				hr = pIWICFactory->CreateBitmapScaler(&pScaler);
				if (SUCCEEDED(hr))
				{
					hr = pScaler->Initialize(
						pSource,
						width,
						height,
						WICBitmapInterpolationModeCubic
						);
				}
				if (SUCCEEDED(hr))
				{
					hr = pConverter->Initialize(
						pScaler,
						GUID_WICPixelFormat32bppPBGRA,
						WICBitmapDitherTypeNone,
						NULL,
						0.f,
						WICBitmapPaletteTypeMedianCut
						);
				}
			}
		}
		else // Don't scale the image.
		{
			hr = pConverter->Initialize(
				pSource,
				GUID_WICPixelFormat32bppPBGRA,
				WICBitmapDitherTypeNone,
				NULL,
				0.f,
				WICBitmapPaletteTypeMedianCut
				);
		}
	}
	if (SUCCEEDED(hr))
	{

		// Create a Direct2D bitmap from the WIC bitmap.
		hr = pRenderTarget->CreateBitmapFromWicBitmap(
			pConverter,
			NULL,
			ppBitmap
			);
	}

	SafeRelease(&pDecoder);
	SafeRelease(&pSource);
	SafeRelease(&pStream);
	SafeRelease(&pConverter);
	SafeRelease(&pScaler);

	return hr;
}

//-----------------------------------------------------------------
// 从文件加载D2D位图，并筛选滤掉色度键(chroma key)
//-----------------------------------------------------------------
ID2D1Bitmap* GetBitmapWithChromaKeyFilter(
		IWICImagingFactory *pIWICFactory,
		ID2D1RenderTarget *pRenderTarget,
		LPCWSTR uri,
		const D2D1_COLOR_F &chromaColor)
{
		ID2D1Bitmap*			pBitmap		= NULL;
		IWICBitmapDecoder*		pDecoder	= NULL;
		IWICBitmapFrameDecode*	pSource		= NULL;
		IWICBitmap*				pWIC		= NULL;
		IWICFormatConverter*	pConverter	= NULL;
		IWICBitmapScaler*		pScaler		= NULL;
		UINT					originalWidth	= 0;
		UINT					originalHeight	= 0;

		// 1.加载IWICBitmap对象

		HRESULT hr = pIWICFactory->CreateDecoderFromFilename(
			uri,
			NULL,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			&pDecoder
			);

		if (SUCCEEDED(hr))
		{
			hr = pDecoder->GetFrame(0, &pSource);
		}

		if (SUCCEEDED(hr))
		{
			hr = pSource->GetSize(&originalWidth,&originalHeight);
		}

		if (SUCCEEDED(hr))
		{
			hr = pIWICFactory->CreateBitmapFromSourceRect(
				pSource, 0,0,(UINT)originalWidth,(UINT)originalHeight, &pWIC);
		}

		if (SUCCEEDED(hr))
		{
			// 2.从IWICBitmap对象读取像素数据
			IWICBitmapLock *pILock = NULL;
			WICRect rcLock = { 0, 0, originalWidth, originalHeight };
			hr = pWIC->Lock(&rcLock, WICBitmapLockWrite, &pILock);

			if (SUCCEEDED(hr))
			{
				UINT cbBufferSize = 0;
				BYTE *pv = NULL;

				if (SUCCEEDED(hr))
				{
					// 获取锁定矩形中左上角像素的指针
					hr = pILock->GetDataPointer(&cbBufferSize, &pv);
				}

				// 3.进行纹理混合的像素计算
				for (unsigned int i=0; i<cbBufferSize; i+=4)
				{
// 					if (pv[i+3] != 0)
// 					{
// 						pv[i]	*=color.b;
// 						pv[i+1]	*=color.g;
// 						pv[i+2]	*=color.r;
// 						pv[i+3] *=color.a;
// 					}

					float b = pv[i];
					float g = pv[i+1];
					float r = pv[i+2];
					float a = pv[i+3];

					if (chromaColor.b * 255 == pv[i] &&
						chromaColor.g * 255 == pv[i+1] &&
						chromaColor.r * 255 == pv[i+2] &&
						chromaColor.a * 255 == pv[i+3])
					{
						pv[i]	= 0;
						pv[i+1] = 0;
						pv[i+2] = 0;
						pv[i+3] = 0;
					}
				}

				// 4.颜色混合操作结束，释放IWICBitmapLock对象
				pILock->Release();
			}
		}

		// 5.使用IWICBitmap对象创建D2D位图

		if (SUCCEEDED(hr))
		{
			hr = pIWICFactory->CreateFormatConverter(&pConverter);
		}
		if (SUCCEEDED(hr))
		{
			hr = pIWICFactory->CreateBitmapScaler(&pScaler);
		}
		if (SUCCEEDED(hr))
		{
			hr = pScaler->Initialize(pWIC, (UINT)originalWidth, (UINT)originalHeight, WICBitmapInterpolationModeCubic);
		}
		if (SUCCEEDED(hr))
		{
			hr = pConverter->Initialize(
				pScaler,
				GUID_WICPixelFormat32bppPBGRA,
				WICBitmapDitherTypeNone,
				NULL,
				0.f,
				WICBitmapPaletteTypeMedianCut
				);
		}

		if (SUCCEEDED(hr))
		{
			hr = pRenderTarget->CreateBitmapFromWicBitmap(
				pConverter,
				NULL,
				&pBitmap
				);
		}

		SafeRelease(&pConverter);
		SafeRelease(&pScaler);
		SafeRelease(&pDecoder);
		SafeRelease(&pSource);

		return pBitmap;
}


/******************************************************************
程序入口
******************************************************************/
int WINAPI WinMain(
    HINSTANCE	/* hInstance */		,
    HINSTANCE	/* hPrevInstance */	,
    LPSTR		/* lpCmdLine */		,
    int			/* nCmdShow */		)
{
    // Ignoring the return value because we want to continue running even in the
    // unlikely event that HeapSetInformation fails.
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    if (SUCCEEDED(CoInitialize(NULL)))
    {
        {
            DemoApp app;

            if (SUCCEEDED(app.Initialize()))
            {
                app.RunMessageLoop();
            }
        }
        CoUninitialize();
    }

    return 0;
}

/******************************************************************
DemoApp实现
******************************************************************/

DemoApp::DemoApp() :
    m_hwnd(NULL),
    m_pD2DFactory(NULL),
	m_pDWriteFactory(NULL),
    m_pRT(NULL),
	m_pWICFactory(NULL),
	m_pBitmap(NULL),
	m_pBitmap1(NULL),
	m_pBitmapChromaKey1(NULL),
	m_pBitmapChromaKey2(NULL)
{
}

DemoApp::~DemoApp()
{
	SafeRelease(&m_pWICFactory);
    SafeRelease(&m_pD2DFactory);
	SafeRelease(&m_pDWriteFactory);
    SafeRelease(&m_pRT);
    SafeRelease(&m_pBitmap);
}

HRESULT DemoApp::Initialize()
{
    HRESULT hr;

    //register window class
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = DemoApp::WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = sizeof(LONG_PTR);
    wcex.hInstance     = HINST_THISCOMPONENT;
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName  = NULL;
    wcex.lpszClassName = L"D2DDemoApp";

    RegisterClassEx(&wcex);

    hr = CreateDeviceIndependentResources();
    if (SUCCEEDED(hr))
    {
        // Create the application window.
        //
        // Because the CreateWindow function takes its size in pixels, we
        // obtain the system DPI and use it to scale the window size.
        FLOAT dpiX, dpiY;
        m_pD2DFactory->GetDesktopDpi(&dpiX, &dpiY);

        m_hwnd = CreateWindow(
            L"D2DDemoApp",
            L"D2DChromaKey",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            static_cast<UINT>(ceil(860.f * dpiX / 96.f)),
            static_cast<UINT>(ceil(260.f * dpiY / 96.f)),
            NULL,
            NULL,
            HINST_THISCOMPONENT,
            this
            );
        hr = m_hwnd ? S_OK : E_FAIL;
        if (SUCCEEDED(hr))
        {
			ShowWindow(m_hwnd, SW_SHOWNORMAL);

			UpdateWindow(m_hwnd);
        }
    }

    return hr;
}

HRESULT DemoApp::CreateDeviceIndependentResources()
{
    HRESULT hr;
    ID2D1GeometrySink *pSink = NULL;

    // 创建D2D工厂
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

	if (m_pDWriteFactory == NULL && SUCCEEDED(hr))
	{
		hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(m_pDWriteFactory),
			reinterpret_cast<IUnknown **>(&m_pDWriteFactory));
	}

	if (m_pWICFactory == NULL && SUCCEEDED(hr))
	{
		if (!SUCCEEDED(
			CoCreateInstance(
			CLSID_WICImagingFactory,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_pWICFactory)
			)
			))
			return FALSE;
	}

    return hr;
}

HRESULT DemoApp::CreateDeviceResources()
{
    HRESULT hr = S_OK;

    if (!m_pRT)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(
            rc.right - rc.left,
            rc.bottom - rc.top
            );

        // 创建render target
        hr = m_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &m_pRT
            );

		// 创建位图，并进行色度键过滤
		if (SUCCEEDED(hr))
		{
			// 过滤黑色部分
			LoadBitmapFromFile(m_pRT, m_pWICFactory,  L"bitmap.png", 0, 0, &m_pBitmap);	
			LoadBitmapFromFile(m_pRT, m_pWICFactory, L"bitmapBlend.png", 0, 0, &m_pBitmap1);	
		}

		// 创建位图，并进行色度键过滤
		if (SUCCEEDED(hr))
		{
			// 过滤黑色部分
			m_pBitmapChromaKey1 = GetBitmapWithChromaKeyFilter(m_pWICFactory, m_pRT, L"bitmap.png", D2D1::ColorF(0, 0, 0, 1));	
			m_pBitmapChromaKey2 = GetBitmapWithChromaKeyFilter(m_pWICFactory, m_pRT, L"bitmapBlend.png", D2D1::ColorF(0, 0, 0, 1));
		}
    }

    return hr;
}

void DemoApp::DiscardDeviceResources()
{
    SafeRelease(&m_pRT);
    SafeRelease(&m_pBitmap);
}

void DemoApp::RunMessageLoop()
{
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

HRESULT DemoApp::OnRender()
{
    HRESULT hr;

    hr = CreateDeviceResources();
    if (SUCCEEDED(hr) && !(m_pRT->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
    {
        // 开始绘制
        m_pRT->BeginDraw();

        m_pRT->SetTransform(D2D1::Matrix3x2F::Identity());
        m_pRT->Clear(D2D1::ColorF(D2D1::ColorF::White));

		// 绘制正常图片
		if (m_pBitmap)
		{
			m_pRT->DrawBitmap(
				m_pBitmap,
				D2D1::RectF(0,0,
				m_pBitmap->GetSize().width,
				m_pBitmap->GetSize().height));
		}

		if (m_pBitmap1)
		{
			m_pRT->DrawBitmap(
				m_pBitmap1,
				D2D1::RectF(250,0,
				250+m_pBitmap->GetSize().width,
				m_pBitmap->GetSize().height));
		}
		
		// 绘制两张色度键过滤后的图片

		if (m_pBitmapChromaKey1)
		{
			m_pRT->DrawBitmap(
				m_pBitmapChromaKey1,
				D2D1::RectF(600,0,
				600+m_pBitmapChromaKey1->GetSize().width,
				m_pBitmapChromaKey1->GetSize().height));
		}

		if (m_pBitmapChromaKey2)
		{
			m_pRT->DrawBitmap(
				m_pBitmapChromaKey2,
				D2D1::RectF(600,0,
				600+m_pBitmapChromaKey2->GetSize().width,
				m_pBitmapChromaKey2->GetSize().height));
		}

        // 结束绘制
        hr = m_pRT->EndDraw();

        if (hr == D2DERR_RECREATE_TARGET)
        {
            hr = S_OK;
            DiscardDeviceResources();
        }
    }

    InvalidateRect(m_hwnd, NULL, FALSE);

    return hr;
}

void DemoApp::OnResize(UINT width, UINT height)
{
    if (m_pRT)
    {
        D2D1_SIZE_U size;
        size.width = width;
        size.height = height;

        // Note: This method can fail, but it's okay to ignore the
        // error here -- it will be repeated on the next call to
        // EndDraw.
        m_pRT->Resize(size);
    }
}

LRESULT CALLBACK DemoApp::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

    if (message == WM_CREATE)
    {
        LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
        DemoApp *pDemoApp = (DemoApp *)pcs->lpCreateParams;

        ::SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            PtrToUlong(pDemoApp)
            );

        result = 1;
    }
    else
    {
        DemoApp *pDemoApp = reinterpret_cast<DemoApp *>(static_cast<LONG_PTR>(
            ::GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA
                )));

        bool wasHandled = false;

        if (pDemoApp)
        {
            switch(message)
            {
            case WM_SIZE:
                {
                    UINT width = LOWORD(lParam);
                    UINT height = HIWORD(lParam);
                    pDemoApp->OnResize(width, height);
                }
                result = 0;
                wasHandled = true;
                break;

            case WM_PAINT:
            case WM_DISPLAYCHANGE:
                {
                    PAINTSTRUCT ps;
                    BeginPaint(hwnd, &ps);

                    pDemoApp->OnRender();
                    EndPaint(hwnd, &ps);
                }
                result = 0;
                wasHandled = true;
                break;

            case WM_DESTROY:
                {
                    PostQuitMessage(0);
                }
                result = 1;
                wasHandled = true;
                break;
            }
        }

        if (!wasHandled)
        {
            result = DefWindowProc(hwnd, message, wParam, lParam);
        }
    }

    return result;
}