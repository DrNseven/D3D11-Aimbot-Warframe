//warframe multihack by n7, main.cpp

#include <Windows.h>
#include <vector>
#include <d3d11.h>
#include <D3Dcompiler.h>//generateshader
#pragma comment(lib, "D3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib") //timeGetTime
#include "MinHook/include/MinHook.h" //detour x86&x64
#include "FW1FontWrapper/FW1FontWrapper.h" //font
#pragma warning( disable : 4244 )


typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(__stdcall *D3D11ResizeBuffersHook) (IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
typedef void(__stdcall *D3D11DrawIndexedHook) (ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
typedef void(__stdcall *D3D11PSSetShaderResourcesHook) (ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews);
typedef void(__stdcall *D3D11DrawHook) (ID3D11DeviceContext* pContext, UINT VertexCount, UINT StartVertexLocation);

//typedef void(__stdcall *D3D11IASetVertexBuffersHook) (ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets);
//typedef void(__stdcall *D3D11VSSetConstantBuffersHook) (ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers);
//typedef void(__stdcall *D3D11PSSetSamplersHook) (ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers);


D3D11PresentHook phookD3D11Present = NULL;
D3D11ResizeBuffersHook phookD3D11ResizeBuffers = NULL;
D3D11DrawIndexedHook phookD3D11DrawIndexed = NULL;
D3D11PSSetShaderResourcesHook phookD3D11PSSetShaderResources = NULL;
D3D11DrawHook phookD3D11Draw = NULL;

//D3D11IASetVertexBuffersHook phookD3D11IASetVertexBuffers = NULL;
//D3D11VSSetConstantBuffersHook phookD3D11VSSetConstantBuffers = NULL;
//D3D11PSSetSamplersHook phookD3D11PSSetSamplers = NULL;


ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;

DWORD_PTR* pSwapChainVtable = NULL;
DWORD_PTR* pContextVTable = NULL;
DWORD_PTR* pDeviceVTable = NULL;

IFW1Factory *pFW1Factory = NULL;
IFW1FontWrapper *pFontWrapper = NULL;

#include "main.h" //helper funcs
#include "renderer.h" //renderer funcs
std::unique_ptr<Renderer> renderer;

//==========================================================================================================================

HRESULT __stdcall hookD3D11ResizeBuffers(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	if (RenderTargetView != NULL)
		SAFE_RELEASE(RenderTargetView);

	return phookD3D11ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

//==========================================================================================================================

HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (firstTime)
	{
		firstTime = false;

		//PlaySoundA(GetDirectoryFile("flash.wav"), 0, SND_FILENAME | SND_ASYNC | SND_NOSTOP | SND_NODEFAULT);

		//get device
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice)))
		{
			pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice);
			pDevice->GetImmediateContext(&pContext);
		}

		//create font
		hr = FW1CreateFactory(FW1_VERSION, &pFW1Factory);
		if (FAILED(hr)) { Log("Failed FW1CreateFactory"); return hr; }
		hr = pFW1Factory->CreateFontWrapper(pDevice, L"Tahoma", &pFontWrapper);
		if (FAILED(hr)) { Log("Failed CreateFontWrapper"); return hr; }
		pFW1Factory->Release();

		//sample state
		D3D11_SAMPLER_DESC sampDesc;
		ZeroMemory(&sampDesc, sizeof(sampDesc));
		sampDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		pDevice->CreateSamplerState(&sampDesc, &pSamplerState);
		
		//create green texture
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //format
		static const uint32_t s_pixel = 0xff00ff00; //0xffffffff white, 0xff00ff00 green, 0xffff0000 blue, 0xff0000ff red, 0x00000000
		D3D11_SUBRESOURCE_DATA initData = { &s_pixel, sizeof(uint32_t), 0 };
		D3D11_TEXTURE2D_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;// D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;// D3D11_BIND_SHADER_RESOURCE;
		pDevice->CreateTexture2D(&desc, &initData, &texGreen);

		//create red texture
		static const uint32_t s_pixelr = 0xff0000ff; //0xffffffff white, 0xff00ff00 green, 0xffff0000 blue, 0xff0000ff red, 0x00000000
		D3D11_SUBRESOURCE_DATA initDatar = { &s_pixelr, sizeof(uint32_t), 0 };
		D3D11_TEXTURE2D_DESC descr;
		memset(&descr, 0, sizeof(descr));
		descr.Width = descr.Height = descr.MipLevels = descr.ArraySize = 1;
		descr.Format = format;
		descr.SampleDesc.Count = 1;
		descr.Usage = D3D11_USAGE_DEFAULT;// D3D11_USAGE_IMMUTABLE;
		descr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;// D3D11_BIND_SHADER_RESOURCE;
		pDevice->CreateTexture2D(&descr, &initDatar, &texRed);

		//create green shaderresourceview
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		memset(&SRVDesc, 0, sizeof(SRVDesc));
		SRVDesc.Format = format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;
		pDevice->CreateShaderResourceView(texGreen, &SRVDesc, &texSRVg);
		texGreen->Release();

		//create red shaderresourceview
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDescr;
		memset(&SRVDescr, 0, sizeof(SRVDescr));
		SRVDescr.Format = format;
		SRVDescr.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDescr.Texture2D.MipLevels = 1;
		pDevice->CreateShaderResourceView(texRed, &SRVDescr, &texSRVr);
		texRed->Release();

		//renderer
		renderer = std::make_unique<Renderer>(pDevice);

		//load cfg settings
		LoadCfg();
	}


	//create rendertarget
	if (RenderTargetView == NULL)
	{
		//viewport
		pContext->RSGetViewports(&vps, &viewport);
		ScreenCenterX = viewport.Width / 2.0f;
		ScreenCenterY = viewport.Height / 2.0f;

		ID3D11Texture2D* backbuffer = NULL;
		hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backbuffer);
		if (FAILED(hr)) {
			Log("Failed to get BackBuffer");
			pContext->OMGetRenderTargets(1, &RenderTargetView, NULL);
			pContext->OMSetRenderTargets(1, &RenderTargetView, NULL);
			return hr;
		}
		
		hr = pDevice->CreateRenderTargetView(backbuffer, NULL, &RenderTargetView);
		backbuffer->Release();
		if (FAILED(hr)) {
			Log("Failed to get RenderTarget");
			return hr;
		}
	}
	else //call before you draw
		pContext->OMSetRenderTargets(1, &RenderTargetView, NULL);


	if(performance_loss && pFontWrapper)
		pFontWrapper->DrawString(pContext, L"~", 14, 16.0f, 16.0f, 0xffff1612, FW1_RESTORESTATE | FW1_ALIASED);

	if (performance_loss)
	{
		static DWORD lastTime = timeGetTime();
		DWORD timePassed = timeGetTime() - lastTime;
		if (timePassed>2000)
		{
			performance_loss = false;
			lastTime = timeGetTime();
		}
	}

	//shaders
	if (!psRed)
		GenerateShader(pDevice, &psRed, 1.0f, 0.0f, 0.0f);

	if (!psGreen)
		GenerateShader(pDevice, &psGreen, 0.0f, 1.0f, 0.0f);

	
	//menu background
	renderer->begin();
	if (Visible)
		renderer->drawOutlinedRect(Vec4(100, 100, 230, 196), 1.f, Color{ 0.9f, 0.9f, 0.15f, 0.95f }, Color{ 0.f , 1.f, 1.f, 0.5f }); //182 = 10 entries, 196 = 11
	renderer->draw();
	renderer->end();

	//menu
	if (IsReady() == false)
	{
		Init_Menu(pContext, L"D3D11 Menu", 100, 100);
		Do_Menu();
		Color_Font = 0xFFFFFFFF;//white
		Color_Off = 0xFFFF0000;//red
		Color_On = 0xFF00FF00;//green
		Color_Folder = 0xFF2F4F4F;//grey
		Color_Current = 0xFF00BFFF;//orange
	}
	Draw_Menu();
	Navigation();

	renderer->begin();
	//dot crosshair
	if(sOptions[9].Function == 1)
	renderer->drawOutlinedRect(Vec4(viewport.Width/2.0f-1.0f, viewport.Height/2.0f, 3.0f, 2.0f), 1.f, Color{ 1.0f, 1.0f, 1.0f, 1.0f }, Color{ 1.f , 1.f, 1.f, 1.2f });

	if (sOptions[2].Function == 1) //if esp is enabled in menu
	if (AimEspInfo.size() != NULL)
	{
		for (unsigned int i = 0; i < AimEspInfo.size(); i++)
		{
			/*
			//debug
			if ((int)AimEspInfo[i].vOutX != ScreenCenterX && AimEspInfo[i].vOutX > 1 && AimEspInfo[i].vOutY > 1 && AimEspInfo[i].vOutX < viewport.Width && AimEspInfo[i].vOutY < viewport.Height)
			{
				wchar_t reportValueS[256];
				swprintf_s(reportValueS, L"%d", (int)AimEspInfo.size());
				if (pFontWrapper)
					pFontWrapper->DrawString(pContext, reportValueS, 14.0f, 200, 200, 0xffffffff, FW1_RESTORESTATE | FW1_CENTER | FW1_ALIASED);
			}
			*/
			//text esp
			if ((int)AimEspInfo[i].vOutX != ScreenCenterX && AimEspInfo[i].vOutX > 1 && AimEspInfo[i].vOutY > 1 && AimEspInfo[i].vOutX < viewport.Width && AimEspInfo[i].vOutY < viewport.Height)
			{
				if (pFontWrapper)
					pFontWrapper->DrawString(pContext, L"o", 14, AimEspInfo[i].vOutX, AimEspInfo[i].vOutY, 0xffff1612, FW1_RESTORESTATE | FW1_CENTER | FW1_ALIASED);
			}
		}
	}
	renderer->draw();
	renderer->end();

	//RMouse|LMouse|Shift|Ctrl|Alt|Space|X|C
	if (sOptions[4].Function == 0) Daimkey = VK_RBUTTON;
	if (sOptions[4].Function == 1) Daimkey = VK_LBUTTON;
	if (sOptions[4].Function == 2) Daimkey = VK_SHIFT;
	if (sOptions[4].Function == 3) Daimkey = VK_CONTROL;
	if (sOptions[4].Function == 4) Daimkey = VK_MENU;
	if (sOptions[4].Function == 5) Daimkey = VK_SPACE;
	if (sOptions[4].Function == 6) Daimkey = 0x58; //X
	if (sOptions[4].Function == 7) Daimkey = 0x43; //C
	aimheight = sOptions[7].Function;//aimheight
	
	//aimbot
	if ((sOptions[3].Function == 1) || (sOptions[3].Function == 2))//aimbot pve, aimbot pvp
	if (AimEspInfo.size() != NULL)
	{
		UINT BestTarget = -1;
		DOUBLE fClosestPos = 99999;

		for (unsigned int i = 0; i < AimEspInfo.size(); i++)
		{
			//aimfov
			float radiusx = (sOptions[6].Function*10.0f) * (ScreenCenterX / 100.0f);
			float radiusy = (sOptions[6].Function*10.0f) * (ScreenCenterY / 100.0f);

			//get crosshairdistance
			AimEspInfo[i].CrosshairDst = GetDst(AimEspInfo[i].vOutX, AimEspInfo[i].vOutY, ScreenCenterX, ScreenCenterY);

			//if in fov
			if ((int)AimEspInfo[i].vOutX != ScreenCenterX && AimEspInfo[i].vOutX >= ScreenCenterX - radiusx && AimEspInfo[i].vOutX <= ScreenCenterX + radiusx && AimEspInfo[i].vOutY >= ScreenCenterY - radiusy && AimEspInfo[i].vOutY <= ScreenCenterY + radiusy)

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
			DistX /= (1.0f + (sOptions[5].Function*0.5f));
			DistY /= (1.0f + (sOptions[5].Function*0.5f));

			//aim
			if(GetAsyncKeyState(Daimkey) & 0x8000)
			mouse_event(MOUSEEVENTF_MOVE, (float)DistX, (float)DistY, 0, NULL);

			//autoshoot on
			if ((!GetAsyncKeyState(VK_LBUTTON) && (sOptions[8].Function==1) && (GetAsyncKeyState(Daimkey) & 0x8000)))
			{
				if (!IsPressed)
				{
					IsPressed = true;
					mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
				}
			}
		}
	}
	AimEspInfo.clear();

	//autoshoot off
	if (sOptions[8].Function==1 && IsPressed)
	{
		if (timeGetTime() - astime >= asdelay)//sOptions[x].Function*100
		{
			IsPressed = false;
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			astime = timeGetTime();
		}
	}

	/*
	//logger
	if (logger && pFontWrapper) //&& countnum >= 0)
	{
		wchar_t reportValue[256];
		swprintf_s(reportValue, L"(Keys:-O P+ I=Log) countnum = %d", countnum);
		pFontWrapper->DrawString(pContext, reportValue, 16.0f, 320.0f, 100.0f, 0xff00ff00, FW1_RESTORESTATE);

		wchar_t reportValueB[256];
		swprintf_s(reportValueB, L"(Keys:-H J+) ProjCBnum = %d", ProjCBnum);
		pFontWrapper->DrawString(pContext, reportValueB, 16.0f, 320.0f, 140.0f, 0xffffffff, FW1_RESTORESTATE);

		wchar_t reportValueC[256];
		swprintf_s(reportValueC, L"(Keys:-N M+) matProjnum = %d", matProjnum);
		pFontWrapper->DrawString(pContext, reportValueC, 16.0f, 320.0f, 160.0f, 0xffffffff, FW1_RESTORESTATE);

		pFontWrapper->DrawString(pContext, L"F9 = log drawfunc", 16.0f, 320.0f, 180.0f, 0xffffffff, FW1_RESTORESTATE);
	}
	*/
	return phookD3D11Present(pSwapChain, SyncInterval, Flags);
}

//==========================================================================================================================

void __stdcall hookD3D11Draw(ID3D11DeviceContext* pContext, UINT VertexCount, UINT StartVertexLocation)
{
	//get stride & vedesc.ByteWidth
	pContext->IAGetVertexBuffers(0, 1, &veBuffer, &Stride, &veBufferOffset);
	if (veBuffer != NULL)
		veBuffer->GetDesc(&vedesc);
	if (veBuffer != NULL) { veBuffer->Release(); veBuffer = NULL; }

	//get pscdesc.ByteWidth
	pContext->PSGetConstantBuffers(pscStartSlot, 1, &pscBuffer);
	if (pscBuffer != NULL)
		pscBuffer->GetDesc(&pscdesc);
	if (pscBuffer != NULL) { pscBuffer->Release(); pscBuffer = NULL; }

	//model recognition:

	//pve
	//Stride == 24 && VertexCount == 4 && indesc.ByteWidth == 21150 && vedesc.ByteWidth == 1048576 && Descr.Format == 77 && pscdesc.ByteWidth == 1152 && Descr.Buffer.NumElements == 7 && texdesc.Width == 64 && texdesc.Height == 64 && texdesc.Format == 76 && pssrStartSlot == 0 && vsStartSlot == 0 && psStartSlot == 0

	//pvp
	//Stride == 24 && VertexCount == 4 && indesc.ByteWidth == 10614 && vedesc.ByteWidth == 1048576 && Descr.Format == 71 && pscdesc.ByteWidth == 1152 && Descr.Buffer.NumElements == 6 && texdesc.Width == 32 && texdesc.Height == 32 && texdesc.Format == 70 && pssrStartSlot == 0 && vsStartSlot == 0 && psStartSlot == 0

	//esp/aimbot pve
	if ((sOptions[2].Function == 1) || (sOptions[3].Function == 1)) //if esp/aimbot pve option is enabled in menu
		//if (Stride == 24 && VertexCount == 4 && pscdesc.ByteWidth == 1152 && texdesc.Width == 64 && texdesc.Height == 64)//pve lvl symbol
		if (Stride == 24 && VertexCount == 4 && vedesc.ByteWidth == 1048576 && Descr.Format == 77 && pscdesc.ByteWidth == 1152 && Descr.Buffer.NumElements == 7 && texdesc.Width == 64 && texdesc.Height == 64 && texdesc.Format == 76 && pssrStartSlot == 0)//pve lvl symbol
		{
			pContext->OMGetDepthStencilState(&pDepthStencilState, &stencilRef);
			if (pDepthStencilState != NULL)
			{
				D3D11_DEPTH_STENCIL_DESC Desc;
				pDepthStencilState->GetDesc(&Desc);

				//debug
				//if (GetAsyncKeyState(VK_F10) & 1) //log stencilRef & Desc.StencilWriteMask
				//Log("Stride == %d && IndexCount == %d && indesc.ByteWidth == %d && vedesc.ByteWidth == %d && pscdesc.ByteWidth == %d && stencilRef == %d && Desc.StencilWriteMask == %x", Stride, IndexCount, indesc.ByteWidth, vedesc.ByteWidth, pscdesc.ByteWidth, stencilRef, Desc.StencilWriteMask);

				if (stencilRef == 0) //&& Desc.StencilWriteMask == 0xFF)
					AddModel(pContext);//w2s
			}
			SAFE_RELEASE(pDepthStencilState);
		}

	//esp/aimbot pvp
	if (sOptions[3].Function == 2) //if aimbot pvp option is enabled in menu
		//if (Stride == 24 && VertexCount == 4 && pscdesc.ByteWidth == 1152 && texdesc.Width == 32 && texdesc.Height == 32)//pvp hp bar
		if (Stride == 24 && VertexCount == 4 && vedesc.ByteWidth == 1048576 && Descr.Format == 71 && pscdesc.ByteWidth == 1152 && Descr.Buffer.NumElements == 6 && texdesc.Width == 32 && texdesc.Height == 32 && texdesc.Format == 70 && pssrStartSlot == 0)//pvp hp bar
		{
			pContext->OMGetDepthStencilState(&pDepthStencilState, &stencilRef);
			if (pDepthStencilState != NULL)
			{
				D3D11_DEPTH_STENCIL_DESC Desc;
				pDepthStencilState->GetDesc(&Desc);

				//debug
				//if (GetAsyncKeyState(VK_F10) & 1) //log stencilRef & Desc.StencilWriteMask
				//Log("Stride == %d && IndexCount == %d && indesc.ByteWidth == %d && vedesc.ByteWidth == %d && pscdesc.ByteWidth == %d && stencilRef == %d && Desc.StencilWriteMask == %x", Stride, IndexCount, indesc.ByteWidth, vedesc.ByteWidth, pscdesc.ByteWidth, stencilRef, Desc.StencilWriteMask);

				if (stencilRef == 0) //&& Desc.StencilWriteMask == 0xFF)
					AddModel(pContext);//w2s
			}
			SAFE_RELEASE(pDepthStencilState);
		}

	return phookD3D11Draw(pContext, VertexCount, StartVertexLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11DrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	/*
	if (firstTime2)
	{
		firstTime2 = false;

		//create depthstencilstate
		////////////////////////////////////////////////////////////////////////
		// DEPTH FALSE
		D3D11_DEPTH_STENCIL_DESC depthStencilDescfalse = {};
		//
		// Depth state:
		depthStencilDescfalse.DepthEnable = false;
		depthStencilDescfalse.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDescfalse.DepthFunc = D3D11_COMPARISON_GREATER; //greater
		
		//UT4 only 
		depthStencilDescfalse.DepthFunc = D3D11_COMPARISON_GREATER; //greater
		//
		// Stencil state:
		depthStencilDescfalse.StencilEnable = true;
		depthStencilDescfalse.StencilReadMask = 0xFF;
		depthStencilDescfalse.StencilWriteMask = 0xFF;
		//
		// Stencil operations if pixel is front-facing:												
		depthStencilDescfalse.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDescfalse.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDescfalse.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDescfalse.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		//
		// Stencil operations if pixel is back-facing:
		depthStencilDescfalse.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDescfalse.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDescfalse.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDescfalse.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		//
		
		pDevice->CreateDepthStencilState(&depthStencilDescfalse, &depthStencilStatefalse);
		////////////////////////////////////////////////////////////////////////

		////////////////////////////////////////////////////////////////////////
		// DEPTH TRUE
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		//
		// Depth state:
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
		
		//UT4 only
		//
		// Stencil state:
		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;
		//
		// Stencil operations if pixel is front-facing:
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER_EQUAL; //greater_equal
		//
		// Stencil operations if pixel is back-facing:
		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_GREATER_EQUAL; //greater_equal
		//
		
		pDevice->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
		////////////////////////////////////////////////////////////////////////
	}

	//get indesc.ByteWidth
	//pContext->IAGetIndexBuffer(&inBuffer, &inFormat, &inOffset);
	//if (inBuffer != NULL)
	//inBuffer->GetDesc(&indesc);
	//if (inBuffer != NULL) { inBuffer->Release(); inBuffer = NULL; }

	//wallhack/chams
	if (sOptions[0].Function == 1 || sOptions[1].Function == 1) //if wallhack/chams option is enabled in menu
	if (Stride == 32)// && nothingreallyworkswell)
	{
		pContext->OMGetDepthStencilState(&origDepthStencilState, &stencilRef); //need stencilref for optimisation later

		if (sOptions[0].Function == 1)
		pContext->OMSetDepthStencilState(depthStencilStatefalse, stencilRef); //depth off

		if (sOptions[0].Function == 1)
		SAFE_RELEASE(depthStencilStatefalse);

		//pssetshader chams
		//if (sOptions[1].Function == 1)
			//pContext->PSSetShader(psRed, NULL, NULL);
		
		//red texture chams (only use in games where shader turns transparent)
		if (sOptions[1].Function == 1)
		{
			for (int x1 = 0; x1 <= 10; x1++)
			{
				pContext->PSSetShaderResources(x1, 1, &texSRVr);
			}
			pContext->PSSetSamplers(0, 1, &pSamplerState);
		}
		
		phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);

		if (sOptions[0].Function == 1||sOptions[1].Function == 1)
		pContext->OMSetDepthStencilState(depthStencilState, stencilRef); //depth on

		//pssetshader chams
		//if (sOptions[1].Function == 1)
			//pContext->PSSetShader(psGreen, NULL, NULL);
		
		//green texture chams (only use in games where shader turns transparent)
		if (sOptions[1].Function == 1)
			{
			for (int y1 = 0; y1 <= 10; y1++)
			{
				pContext->PSSetShaderResources(y1, 1, &texSRVg);
			}
			pContext->PSSetSamplers(0, 1, &pSamplerState);
		}

		//debug
		if (origDepthStencilState != NULL)
		{
		D3D11_DEPTH_STENCIL_DESC Desc;
		origDepthStencilState->GetDesc(&Desc);

		if (GetAsyncKeyState(VK_F10) & 1) //log Desc stuff
		Log("Desc.BackFace == %d && Desc.DepthEnable == %d && Desc.DepthFunc == %d && Desc.DepthWriteMask == %d && Desc.FrontFace == %d && Desc.StencilEnable == %d && Desc.StencilReadMask == %d && Desc.StencilWriteMask == %d && stencilRef == %d",
		Desc.BackFace, Desc.DepthEnable, Desc.DepthFunc, Desc.DepthWriteMask, Desc.FrontFace, Desc.StencilEnable, Desc.StencilReadMask, Desc.StencilWriteMask, stencilRef);
		}
		
		if (sOptions[0].Function == 1)
		SAFE_RELEASE(origDepthStencilState);
	}
	*/
	/*
	//small bruteforce logger
	//ALT + CTRL + L toggles logger
	if (logger)
	{
		if (countnum == IndexCount / 100)
			if (GetAsyncKeyState('I') & 1)
				Log("Stride == %d && IndexCount == %d && indesc.ByteWidth == %d && vedesc.ByteWidth == %d && Descr.Format == %d && pscdesc.ByteWidth == %d && Descr.Buffer.NumElements == %d && vsStartSlot == %d && psStartSlot == %d && vsdesc.ByteWidth == %d && texdesc.Format == %d && texdesc.Height == %d && texdesc.Width == %d", Stride, IndexCount, indesc.ByteWidth, vedesc.ByteWidth, Descr.Format, pscdesc.ByteWidth, Descr.Buffer.NumElements, vsStartSlot, psStartSlot, vsdesc.ByteWidth, texdesc.Format, texdesc.Height, texdesc.Width);
				//Log("texdesc.ArraySize == %d && texdesc.BindFlags == %d && texdesc.CPUAccessFlags == %d && texdesc.MipLevels == %d && texdesc.MiscFlags == %d && texdesc.SampleDesc == %d && texdesc.Usage == %d", texdesc.ArraySize, texdesc.BindFlags, texdesc.CPUAccessFlags, texdesc.MipLevels, texdesc.MiscFlags, texdesc.SampleDesc, texdesc.Usage);

		if (countnum == IndexCount / 100)
		{
			//delete texture
			return;
		}
	}
	*/
    return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11PSSetShaderResources(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	pssrStartSlot = StartSlot;


	//texture stuff usually not needed
	for (UINT j = 0; j < NumViews; j++)
	{
		//resources loop
		ID3D11ShaderResourceView* pShaderResView = ppShaderResourceViews[j];
		if (pShaderResView)
		{
			pShaderResView->GetDesc(&Descr);

			ID3D11Resource *Resource;
			pShaderResView->GetResource(&Resource);
			ID3D11Texture2D *Texture = (ID3D11Texture2D *)Resource;
			Texture->GetDesc(&texdesc);
			
			SAFE_RELEASE(Resource);
			SAFE_RELEASE(Texture);
		}
	}
	
	/*
	//logger
	if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(0x4C) & 1) //ALT + CTRL + L toggles logger
		logger = !logger;
	if (logger && pFontWrapper) //&& countnum >= 0) //low fps, fix me
	{
		//hold down P key until a texture is wallhacked, press I to log values of those textures
		if (GetAsyncKeyState('O') & 1) //-
			countnum--;
		if (GetAsyncKeyState('P') & 1) //+
			countnum++;
		if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState('9') & 1) //reset, set to -1
			countnum = -1;

		//bruteforce ProjCBnum
		if (GetAsyncKeyState('H') & 1) //-
			ProjCBnum--;
		if (GetAsyncKeyState('J') & 1) //+
			ProjCBnum++;
		if (GetAsyncKeyState(0x38) & 1) //8 reset, set to 0
			ProjCBnum = 0;
		if (ProjCBnum < 0)
			ProjCBnum = 0;

		//bruteforce matProjnum
		if (GetAsyncKeyState('N') & 1) //-
			matProjnum--;
		if (GetAsyncKeyState('M') & 1) //+
			matProjnum++;
		if (GetAsyncKeyState(0x39) & 1) //9 reset, set to 0
			matProjnum = 0;
		if (matProjnum < 0)
			matProjnum = 0;
	}
	*/
	return phookD3D11PSSetShaderResources(pContext, StartSlot, NumViews, ppShaderResourceViews);
}

//==========================================================================================================================

//void __stdcall hookD3D11IASetVertexBuffers(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets)
//{
//Stride = *pStrides;

//return phookD3D11IASetVertexBuffers(pContext, StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
//}

//==========================================================================================================================

//void __stdcall hookD3D11VSSetConstantBuffers(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
//{
		//vsStartSlot = StartSlot;

	//return phookD3D11VSSetConstantBuffers(pContext, StartSlot, NumBuffers, ppConstantBuffers);
//}

//==========================================================================================================================

//void __stdcall hookD3D11PSSetSamplers(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers)
//{
		//psStartSlot = StartSlot;

	//return phookD3D11PSSetSamplers(pContext, StartSlot, NumSamplers, ppSamplers);
//}

//==========================================================================================================================

const int MultisampleCount = 1; // Set to 1 to disable multisampling
LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){ return DefWindowProc(hwnd, uMsg, wParam, lParam); }
DWORD __stdcall InitializeHook(LPVOID)
{
	HMODULE hDXGIDLL = 0;
	do
	{
		hDXGIDLL = GetModuleHandle("dxgi.dll");
		Sleep(4000);
	} while (!hDXGIDLL);
	Sleep(100);

    IDXGISwapChain* pSwapChain;

	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DXGIMsgProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "WX", NULL };
	RegisterClassExA(&wc);
	HWND hWnd = CreateWindowA("WX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

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
	if (MH_CreateHook((DWORD_PTR*)pSwapChainVtable[13], hookD3D11ResizeBuffers, reinterpret_cast<void**>(&phookD3D11ResizeBuffers)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pSwapChainVtable[13]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[13], hookD3D11Draw, reinterpret_cast<void**>(&phookD3D11Draw)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[13]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[12], hookD3D11DrawIndexed, reinterpret_cast<void**>(&phookD3D11DrawIndexed)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[8], hookD3D11PSSetShaderResources, reinterpret_cast<void**>(&phookD3D11PSSetShaderResources)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[8]) != MH_OK) { return 1; }
	
	//if (MH_CreateHook((DWORD_PTR*)pContextVTable[18], hookD3D11IASetVertexBuffers, reinterpret_cast<void**>(&phookD3D11IASetVertexBuffers)) != MH_OK) { return 1; }
	//if (MH_EnableHook((DWORD_PTR*)pContextVTable[18]) != MH_OK) { return 1; }
	//if (MH_CreateHook((DWORD_PTR*)pContextVTable[7], hookD3D11VSSetConstantBuffers, reinterpret_cast<void**>(&phookD3D11VSSetConstantBuffers)) != MH_OK) { return 1; }
	//if (MH_EnableHook((DWORD_PTR*)pContextVTable[7]) != MH_OK) { return 1; }
	//if (MH_CreateHook((DWORD_PTR*)pContextVTable[10], hookD3D11PSSetSamplers, reinterpret_cast<void**>(&phookD3D11PSSetSamplers)) != MH_OK) { return 1; }
	//if (MH_EnableHook((DWORD_PTR*)pContextVTable[10]) != MH_OK) { return 1; }
	
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
		if (MH_DisableHook((DWORD_PTR*)pSwapChainVtable[13]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pContextVTable[13]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pContextVTable[8]) != MH_OK) { return 1; }
		//if (MH_DisableHook((DWORD_PTR*)pContextVTable[18]) != MH_OK) { return 1; }
		//if (MH_DisableHook((DWORD_PTR*)pContextVTable[7]) != MH_OK) { return 1; }
		//if (MH_DisableHook((DWORD_PTR*)pContextVTable[10]) != MH_OK) { return 1; }
		break;
	}
	return TRUE;
}