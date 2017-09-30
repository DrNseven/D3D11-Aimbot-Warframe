//d3d11 aimbot for warframe by n7

#include <Windows.h>
#include <vector>
#include <d3d11.h>
#include <D3D11Shader.h>
#include <D3Dcompiler.h>//generateshader
#pragma comment(lib, "D3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib") //timeGetTime
#include "MinHook/include/MinHook.h" //detour x86&x64
#include "FW1FontWrapper/FW1FontWrapper.h" //font


typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
//typedef void(__stdcall *D3D11DrawIndexedHook) (ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
typedef void(__stdcall *D3D11PSSetShaderResourcesHook) (ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews);
typedef void(__stdcall *D3D11DrawHook) (ID3D11DeviceContext* pContext, UINT VertexCount, UINT StartVertexLocation);


D3D11PresentHook phookD3D11Present = NULL;
//D3D11DrawIndexedHook phookD3D11DrawIndexed = NULL;
D3D11PSSetShaderResourcesHook phookD3D11PSSetShaderResources = NULL;
D3D11DrawHook phookD3D11Draw = NULL;


ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;

DWORD_PTR* pSwapChainVtable = NULL;
DWORD_PTR* pContextVTable = NULL;
DWORD_PTR* pDeviceVTable = NULL;

IFW1Factory *pFW1Factory = NULL;
IFW1FontWrapper *pFontWrapper = NULL;

#include "main.h" //helper funcs

//==========================================================================================================================

HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (firstTime)
	{
		firstTime = false;

		PlaySoundA(GetDirectoryFile("dx.wav"), 0, SND_FILENAME | SND_ASYNC | SND_NOSTOP | SND_NODEFAULT);

		//get device
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice)))
		{
			pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice);
			pDevice->GetImmediateContext(&pContext);
		}

		//load cfg settings
		LoadCfg();
		
		//create font
		HRESULT hResult = FW1CreateFactory(FW1_VERSION, &pFW1Factory);
		hResult = pFW1Factory->CreateFontWrapper(pDevice, L"Tahoma", &pFontWrapper);
		pFW1Factory->Release();
	}
	
	//viewport
	pContext->RSGetViewports(&vps, &viewport);
	ScreenCenterX = viewport.Width / 2.0f;
	ScreenCenterY = viewport.Height / 2.0f;

	//call before you draw
	if(RenderTargetView != NULL)
	{
	pContext->OMGetRenderTargets(1, &RenderTargetView, NULL);
	pContext->OMSetRenderTargets(1, &RenderTargetView, NULL);
	}

	//if (pFontWrapper)
		//pFontWrapper->DrawString(pContext, L"D3D11 Hook", 14, 16.0f, 16.0f, 0xffff1612, FW1_RESTORESTATE);

	//menu
	if (IsReady() == false)
	{
		Init_Menu(pContext, L"D3D11 Menu", 100, 120);
		Do_Menu();
		Color_Font = WHITE;
		Color_Off = RED;
		Color_On = GREEN;
		Color_Folder = GRAY;
		Color_Current = ORANGE;
	}
	Draw_Menu();
	Navigation();
	

	//draw aimpoint/esp
	if (sOptions[0].Function==1) //if esp is turned on in menu
	if (AimEspInfo.size() != NULL)
	{
		for (unsigned int i = 0; i < AimEspInfo.size(); i++)
		{
			//text esp
			if (AimEspInfo[i].vOutX > 1 && AimEspInfo[i].vOutY > 1 && AimEspInfo[i].vOutX < viewport.Width && AimEspInfo[i].vOutY < viewport.Height)
			{
				if (pFontWrapper)
				pFontWrapper->DrawString(pContext, L"o", 14, (int)AimEspInfo[i].vOutX, (int)AimEspInfo[i].vOutY, 0xffff1612, FW1_RESTORESTATE| FW1_NOGEOMETRYSHADER | FW1_CENTER | FW1_ALIASED);
			}
		}
	}
	
	//RMouse|LMouse|Shift|Ctrl|Alt|Space|X|C
	if (sOptions[3].Function == 0) Daimkey = VK_RBUTTON;
	if (sOptions[3].Function == 1) Daimkey = VK_LBUTTON;
	if (sOptions[3].Function == 2) Daimkey = VK_SHIFT;
	if (sOptions[3].Function == 3) Daimkey = VK_CONTROL;
	if (sOptions[3].Function == 4) Daimkey = VK_MENU;
	if (sOptions[3].Function == 5) Daimkey = VK_SPACE;
	if (sOptions[3].Function == 6) Daimkey = 0x58; //X
	if (sOptions[3].Function == 7) Daimkey = 0x43; //C
	aimheight = sOptions[7].Function;//aimheight
	
	//aimbot
	if((sOptions[1].Function == 1)||(sOptions[2].Function == 1))//aimbot pve, aimbot pvp
	if (AimEspInfo.size() != NULL)
	{
		UINT BestTarget = -1;
		DOUBLE fClosestPos = 99999;

		for (unsigned int i = 0; i < AimEspInfo.size(); i++)
		{
			//aimfov
			float radiusx = (sOptions[5].Function*10.0f) * (ScreenCenterX / 100.0f);
			float radiusy = (sOptions[5].Function*10.0f) * (ScreenCenterY / 100.0f);

			//get crosshairdistance
			AimEspInfo[i].CrosshairDst = GetmDst(AimEspInfo[i].vOutX, AimEspInfo[i].vOutY, ScreenCenterX, ScreenCenterY);

			//if in fov
			if (AimEspInfo[i].vOutX >= ScreenCenterX - radiusx && AimEspInfo[i].vOutX <= ScreenCenterX + radiusx && AimEspInfo[i].vOutY >= ScreenCenterY - radiusy && AimEspInfo[i].vOutY <= ScreenCenterY + radiusy)

				//get closest/nearest target to crosshair
				if (AimEspInfo[i].CrosshairDst < fClosestPos)
				{
					fClosestPos = AimEspInfo[i].CrosshairDst;
					BestTarget = i;
				}
		}

		//if nearest target to crosshair
		if (BestTarget != -1)
		{
			double DistX = AimEspInfo[BestTarget].vOutX - ScreenCenterX;
			double DistY = AimEspInfo[BestTarget].vOutY - ScreenCenterY;

			//aimsens
			DistX /= (1 + sOptions[4].Function);
			DistY /= (1 + sOptions[4].Function);

			//aim
			if(GetAsyncKeyState(Daimkey) & 0x8000)
			mouse_event(MOUSEEVENTF_MOVE, (float)DistX, (float)DistY, 0, NULL);

			//autoshoot on
			if ((!GetAsyncKeyState(VK_LBUTTON) && (sOptions[7].Function == 1) && (GetAsyncKeyState(Daimkey) & 0x8000)))
			{
				if (!IsPressed)
				{
					mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
					IsPressed = true;
				}
			}
		}
	}
	AimEspInfo.clear();

	//autoshoot off
	if (sOptions[7].Function == 1 && IsPressed)
	{
		if (timeGetTime() - astime >= asdelay)
		{
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			IsPressed = false;
			astime = timeGetTime();
		}
	}
	
	return phookD3D11Present(pSwapChain, SyncInterval, Flags);
}

//==========================================================================================================================

//void __stdcall hookD3D11DrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
//{
	//players/mobs are drawn here
	//return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
//}

//==========================================================================================================================

void __stdcall hookD3D11PSSetShaderResources(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	pssrStartSlot = StartSlot;

	//resources
	ID3D11ShaderResourceView* pShaderResView = *ppShaderResourceViews;
	if (pShaderResView)
	{
		pShaderResView->GetDesc(&Descr);

		ID3D11Resource *Resource;
		pShaderResView->GetResource(&Resource);
		ID3D11Texture2D *Texture = (ID3D11Texture2D *)Resource;
		Texture->GetDesc(&texdesc);
		SAFE_RELEASE(Texture);
		SAFE_RELEASE(Resource);
	}

	return phookD3D11PSSetShaderResources(pContext, StartSlot, NumViews, ppShaderResourceViews);
}

//==========================================================================================================================

void __stdcall hookD3D11Draw(ID3D11DeviceContext* pContext, UINT VertexCount, UINT StartVertexLocation)
{
	//get stride & vdesc.ByteWidth
	pContext->IAGetVertexBuffers(0, 1, &veBuffer, &Stride, &veBufferOffset);
	if (veBuffer)
		veBuffer->GetDesc(&vedesc);
	SAFE_RELEASE(veBuffer);

	//get indesc.ByteWidth
	pContext->IAGetIndexBuffer(&inBuffer, &inFormat, &inOffset);
	if (inBuffer)
		inBuffer->GetDesc(&indesc);
	SAFE_RELEASE(inBuffer);

	//get pscdesc.ByteWidth
	pContext->PSGetConstantBuffers(pscStartSlot, 1, &pcsBuffer);
	if (pcsBuffer)
		pcsBuffer->GetDesc(&pscdesc);
	SAFE_RELEASE(pcsBuffer);

	//model recognition:

	//pve
	//Stride == 24 && VertexCount == 4 && indesc.ByteWidth == 21150 && vedesc.ByteWidth == 1048576 && Descr.Format == 77 && pscdesc.ByteWidth == 1152 && Descr.Buffer.NumElements == 7 && texdesc.Width == 64 && texdesc.Height == 64 && texdesc.Format == 76 && pssrStartSlot == 0 && vsStartSlot == 0 && psStartSlot == 0

	//pvp
	//Stride == 24 && VertexCount == 4 && indesc.ByteWidth == 10614 && vedesc.ByteWidth == 1048576 && Descr.Format == 71 && pscdesc.ByteWidth == 1152 && Descr.Buffer.NumElements == 6 && texdesc.Width == 32 && texdesc.Height == 32 && texdesc.Format == 70 && pssrStartSlot == 0 && vsStartSlot == 0 && psStartSlot == 0

	//esp/aimbot pve
	if ((sOptions[0].Function == 1)||(sOptions[1].Function == 1))//if aimpoint or aimbot pve
	if (Stride == 24 && VertexCount == 4 && vedesc.ByteWidth == 1048576 && Descr.Format == 77 && pscdesc.ByteWidth == 1152 && Descr.Buffer.NumElements == 7 && texdesc.Width == 64 && texdesc.Height == 64 && texdesc.Format == 76 && pssrStartSlot == 0)//pve lvl symbol
	{
		AddModel(pContext);//w2s
	}

	//esp/aimbot pvp
	if (sOptions[2].Function == 1)//if aimpoint or aimbot pvp
	if (Stride == 24 && VertexCount == 4 && vedesc.ByteWidth == 1048576 && Descr.Format == 71 && pscdesc.ByteWidth == 1152 && Descr.Buffer.NumElements == 6 && texdesc.Width == 32 && texdesc.Height == 32 && texdesc.Format == 70 && pssrStartSlot == 0)//pvp hp bar
	{
		AddModel(pContext);//w2s
	}

	return phookD3D11Draw(pContext, VertexCount, StartVertexLocation);
}

//==========================================================================================================================

const int MultisampleCount = 1; // Set to 1 to disable multisampling
LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){ return DefWindowProc(hwnd, uMsg, wParam, lParam); }
DWORD __stdcall InitializeHook(LPVOID)
{
	HMODULE hDXGIDLL = 0;
	do
	{
		hDXGIDLL = GetModuleHandle("dxgi.dll");
		Sleep(8000);
	} while (!hDXGIDLL);
	Sleep(100);

    IDXGISwapChain* pSwapChain;

	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DXGIMsgProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
	RegisterClassExA(&wc);
	HWND hWnd = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

	D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
	D3D_FEATURE_LEVEL obtainedLevel;
	ID3D11Device* d3dDevice = nullptr;
	ID3D11DeviceContext* d3dContext = nullptr;

	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(scd));
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	scd.OutputWindow = hWnd;
	scd.SampleDesc.Count = MultisampleCount;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scd.Windowed = ((GetWindowLongPtr(hWnd, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

	// LibOVR 0.4.3 requires that the width and height for the backbuffer is set even if
	// you use windowed mode, despite being optional according to the D3D11 documentation.
	scd.BufferDesc.Width = 1;
	scd.BufferDesc.Height = 1;
	scd.BufferDesc.RefreshRate.Numerator = 0;
	scd.BufferDesc.RefreshRate.Denominator = 1;

	UINT createFlags = 0;
#ifdef _DEBUG
	// This flag gives you some quite wonderful debug text. Not wonderful for performance, though!
	createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	IDXGISwapChain* d3dSwapChain = 0;

	if (FAILED(D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createFlags,
		requestedLevels,
		sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
		D3D11_SDK_VERSION,
		&scd,
		&pSwapChain,
		&pDevice,
		&obtainedLevel,
		&pContext)))
	{
		MessageBox(hWnd, "Failed to create directX device and swapchain!", "Error", MB_ICONERROR);
		return NULL;
	}


    pSwapChainVtable = (DWORD_PTR*)pSwapChain;
    pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];

    pContextVTable = (DWORD_PTR*)pContext;
    pContextVTable = (DWORD_PTR*)pContextVTable[0];

	pDeviceVTable = (DWORD_PTR*)pDevice;
	pDeviceVTable = (DWORD_PTR*)pDeviceVTable[0];

	if (MH_Initialize() != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pSwapChainVtable[8], hookD3D11Present, reinterpret_cast<void**>(&phookD3D11Present)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pSwapChainVtable[8]) != MH_OK) { return 1; }
	//if (MH_CreateHook((DWORD_PTR*)pContextVTable[12], hookD3D11DrawIndexed, reinterpret_cast<void**>(&phookD3D11DrawIndexed)) != MH_OK) { return 1; }
	//if (MH_EnableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[8], hookD3D11PSSetShaderResources, reinterpret_cast<void**>(&phookD3D11PSSetShaderResources)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[8]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[13], hookD3D11Draw, reinterpret_cast<void**>(&phookD3D11Draw)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[13]) != MH_OK) { return 1; }
	

    DWORD dwOld;
    VirtualProtect(phookD3D11Present, 2, PAGE_EXECUTE_READWRITE, &dwOld);

	while (true) {
		Sleep(10);
	}

	pDevice->Release();
	pContext->Release();
	pSwapChain->Release();

    return NULL;
}

//==========================================================================================================================

BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{ 
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH: // A process is loading the DLL.
		DisableThreadLibraryCalls(hModule);
		GetModuleFileName(hModule, dlldir, 512);
		for (size_t i = strlen(dlldir); i > 0; i--) { if (dlldir[i] == '\\') { dlldir[i + 1] = 0; break; } }
		CreateThread(NULL, 0, InitializeHook, NULL, 0, NULL);
		break;

	case DLL_PROCESS_DETACH: // A process unloads the DLL.
		if (MH_Uninitialize() != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pSwapChainVtable[8]) != MH_OK) { return 1; }
		//if (MH_DisableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pDeviceVTable[8]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pContextVTable[13]) != MH_OK) { return 1; }
		break;
	}
	return TRUE;
}
