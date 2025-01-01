#include<windows.h>
#include<d3dx9.h>
#include<DxErr.h>
#include"resource.h"

LPDIRECT3D9 pD3D;
LPDIRECT3DDEVICE9 pD3DDev;
D3DPRESENT_PARAMETERS d3dpp;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

#define NAME_OF_THE_APP L"D3DX - Gravity"

#define RELEASENULL(pObject) if (pObject) {pObject->Release(); pObject = NULL;}

#define NUM_PLANETS         300
#define NUM_PARTICLES       1500
#define NUM_MATERIALS       25
#define NUM_SPHERES         25  //reused by planets
#define FULLSCREEN_WIDTH    640
#define FULLSCREEN_HEIGHT   480

#define Y_OFFSET(distance)       distance*(Random(2.0f)-1.0f)

// for Draw the particles
#define D3DFVF_VERTEX       (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_PSIZE)
typedef struct _D3dVERTEX
{
	D3DXVECTOR3 v;
	D3DXVECTOR3 n;
	float psize;
	D3DCOLORVALUE Diffuse;
	D3DCOLORVALUE Specular;
} D3DVERTEX;

typedef struct _SUN
{
	D3DXVECTOR4 pos;
	float distance;
	LPD3DXMESH pSphere;	//changed type
	DWORD dwMaterial;
} SUN;

typedef struct _PLANET
{
	D3DXVECTOR4 pos;
	float distance;
	DWORD dwSphere;
	DWORD dwMaterial;
	BOOL bAttract;
} PLANET;

typedef struct _PARTICLE
{
	D3DXVECTOR4 pos;
	float distance;
	DWORD dwMaterial;
	BOOL bAttract;
} PARTICLE;

class CGravity
{
public:
	CGravity();
	~CGravity();
	void                    PauseDrawing();
	void                    RestartDrawing();
	void                    UpdateTime();
	void                    UnInit();
	HRESULT					InitD3D(HWND hWnd);
	HRESULT                 InitD3DX();
	HRESULT                 InitRenderer();
	HRESULT                 HandleModeChanges();
	void                    DestroySpheres();
	HRESULT                 GenerateSpheres();
	HRESULT                 Draw();
	void                    ApplyGravity(float* pfDistance,
		D3DXVECTOR4* pPos,
		BOOL* pbAttract);

	BOOL                    m_bD3DXReady;
	BOOL                    m_bActive;
	BOOL                    m_bIsFullscreen;

	HWND                    m_hwndMain;
	RECT                    m_rWindowedRect;

	//LPDIRECT3DDEVICE9       m_pD3DDev;
	//LPDIRECT3D9             m_pD3D;
	//LPDIRECTDRAW9           m_pDD;

	float                   m_fViewRot[3];
	float                   m_fSunRot[3];

	//ID3DXContext* m_pD3DX;

	SUN                     m_Sun;
	PLANET                  m_Planets[NUM_PLANETS];
	PARTICLE                m_Particles[NUM_PARTICLES];

	//ID3DXMatrixStack* m_pWorldStack;
	//ID3DXMatrixStack* m_pViewStack;

	D3DXMATRIX m_WorldMat;
	D3DXMATRIX m_ViewMat;

	D3DLIGHT9               m_LightOnSun;
	D3DLIGHT9               m_LightFromSun;
	D3DMATERIAL9            m_SunMaterial;
	D3DMATERIAL9            m_PlanetMaterials[NUM_MATERIALS];
	ID3DXMesh* m_Spheres[NUM_SPHERES];

	double                  m_dAbsoluteTime;
	double                  m_dElapsedTime;
	double                  m_dPeriod;
	LARGE_INTEGER           m_liLastTime;
};

CGravity* g_pGravity;

CGravity::CGravity()
{
	m_bD3DXReady = FALSE;
	m_bIsFullscreen = FALSE;
	pD3DDev = NULL;
	pD3D = NULL;
	//m_pD3D = NULL;
	//m_pDD = NULL;
	//m_pD3DX = NULL;
	m_fViewRot[0] = 1.0f;
	m_fViewRot[1] = -1.0f;
	m_fViewRot[2] = 0.0f;
	m_fSunRot[0] = 0.0f;
	m_fSunRot[1] = 0.0f;
	m_fSunRot[2] = 0.0f;
	//m_pWorldStack = NULL;
	//m_pViewStack = NULL;
	m_bActive = !m_bIsFullscreen;
	m_liLastTime.QuadPart = 0;
	m_dAbsoluteTime = 0;
	m_dElapsedTime = 0;
	m_Sun.pSphere = NULL;

	for (int i = 0; i < NUM_SPHERES; i++)
	{
		m_Spheres[i] = NULL;
	}

	LARGE_INTEGER liFrequency;
	if (!QueryPerformanceFrequency(&liFrequency))
		liFrequency.QuadPart = 1;

	m_dPeriod = (double)1 / liFrequency.QuadPart;
	if (!QueryPerformanceCounter(&m_liLastTime))
		m_liLastTime.QuadPart = 0;
}

CGravity::~CGravity()
{
	g_pGravity->UnInit();
}

void InterpretError(HRESULT hr)
{
	WCHAR errStr[100];
	wcscpy_s(errStr, DXGetErrorString(hr));
	MessageBox(NULL, errStr, L"D3DX Error", MB_OK);
}

float Random(float fMax)
{
	return fMax * rand() / RAND_MAX;
}

void CGravity::UpdateTime()
{
	LARGE_INTEGER liCurrTime;
	if (!QueryPerformanceCounter(&liCurrTime))
		liCurrTime.QuadPart = m_liLastTime.QuadPart + 1;

	m_dElapsedTime = (double)(liCurrTime.QuadPart - m_liLastTime.QuadPart) *
		m_dPeriod;

	m_dAbsoluteTime += m_dElapsedTime;
	m_liLastTime = liCurrTime;
}


void CGravity::PauseDrawing()
{
	g_pGravity->m_bActive = FALSE;
	if (m_bIsFullscreen)
		ShowCursor(TRUE);
}

void CGravity::RestartDrawing()
{
	g_pGravity->m_bActive = TRUE;
	if (m_bIsFullscreen)
		ShowCursor(FALSE);
}

//*****************************************************************************
// Renderer Initialization Code
//*****************************************************************************

HRESULT CGravity::InitD3D(HWND hWnd)
{
	// Direct3D作成
	if (NULL == (pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
	{
		MessageBox(0, L"Direct3Dの作成に失敗しました", L"", MB_OK);
		return E_FAIL;
	}
	// Direct3D Device作成
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.BackBufferWidth = m_bIsFullscreen ? FULLSCREEN_WIDTH : 0;
	d3dpp.BackBufferHeight = m_bIsFullscreen ? FULLSCREEN_HEIGHT : 0;
	d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	d3dpp.BackBufferCount = 1;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.Windowed = TRUE;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;//D24S8でないとclear D3DCLEAR_STENCILでエラーが出る

	if (FAILED(pD3D->CreateDevice(
		D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&d3dpp, &pD3DDev)))
	{
		MessageBox(0, L"HALモードDIRECT3Dデバイス失敗\nREFモード再試行 ", NULL, MB_OK);
		if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&d3dpp, &pD3DDev)))
		{
			MessageBox(0, L"DIRECT3Dデバイス作成に失敗", NULL, MB_OK);
			return E_FAIL;
		}
	}
	return S_OK;
}

HRESULT CGravity::InitD3DX()
{
	HRESULT hr;
	//DWORD i;
	//char buff[1024];

	if (FAILED(hr = InitD3D(m_hwndMain)))
	{
		return hr;
	}
	//if (FAILED(hr = D3DXInitialize()))
	//	return hr;

	// D3DX Initialization
	//hr = D3DXCreateContextEx(D3DX_DEFAULT,           // D3DX handle
	//	m_bIsFullscreen ? D3DX_CONTEXT_FULLSCREEN : 0, // flags
	//	m_hwndMain,
	//	NULL,                   // focusWnd
	//	D3DX_DEFAULT,           // colorbits
	//	D3DX_DEFAULT,           // alphabits
	//	D3DX_DEFAULT,           // numdepthbits
	//	0,                      // numstencilbits
	//	D3DX_DEFAULT,           // numbackbuffers
	//	m_bIsFullscreen ? FULLSCREEN_WIDTH : D3DX_DEFAULT,  // width
	//	m_bIsFullscreen ? FULLSCREEN_HEIGHT : D3DX_DEFAULT, // height
	//	D3DX_DEFAULT,           // refresh rate
	//	&m_pD3DX                // returned D3DX interface
	//);
	//if (FAILED(hr))
	//	return hr;
	//m_pD3DDev = m_pD3DX->GetD3DDevice();
	//if (m_pD3DDev == NULL)
	//	return E_FAIL;
	//m_pD3D = m_pD3DX->GetD3D();
	//if (m_pD3D == NULL)
	//	return E_FAIL;
	//m_pDD = m_pD3DX->GetDD();
	//if (m_pDD == NULL)
	//	return E_FAIL;

	m_bD3DXReady = TRUE;
	return InitRenderer();
}

// ***************************************************************************
// Renderer Initialization Code
// ***************************************************************************

HRESULT CGravity::InitRenderer()
{
	HRESULT hr;
	int i;

	if (!m_bD3DXReady)
		return E_FAIL;

	//hr = m_pD3DX->SetClearColor(D3DRGBA(0, 0, 0, 0));
	//if (FAILED(hr))
	//	return hr;

	srand(4);
	hr = pD3DDev->SetRenderState(D3DRS_DITHERENABLE, TRUE);//no texture
	if (FAILED(hr))
		return hr;

	hr = pD3DDev->SetRenderState(D3DRS_ZENABLE, TRUE);
	if (FAILED(hr))
		return hr;

	hr = pD3DDev->SetRenderState(D3DRS_LIGHTING, TRUE);
	if (FAILED(hr))
		return hr;

	hr = pD3DDev->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
	if (FAILED(hr))
		return hr;

	//hr = m_pD3DX->Clear(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER);
	//if (FAILED(hr))
	//	return hr;

	if (FAILED(pD3DDev->Clear(0, NULL,
		D3DCLEAR_TARGET |
		D3DCLEAR_ZBUFFER,
		D3DCOLOR_XRGB(0, 0, 0),
		1.0f,
		0)))
	{
		return E_FAIL;
	}

	float dvR, dvG, dvB, dvA;
	dvR = 1.0f;
	dvG = 0.9f;
	dvB = 0.6f;
	dvA = 1.0f;
	memset(&m_LightFromSun, 0, sizeof(D3DLIGHT9));

	// Light which illuminates the "planets"
	m_LightFromSun.Type = D3DLIGHT_POINT;
	m_LightFromSun.Attenuation0 = 0.5f;
	m_LightFromSun.Attenuation1 = 0.008f;
	m_LightFromSun.Attenuation2 = 0.0f;
	m_LightFromSun.Diffuse.r = dvR;
	m_LightFromSun.Diffuse.g = dvG;
	m_LightFromSun.Diffuse.b = dvB;
	m_LightFromSun.Diffuse.a = dvA;
	m_LightFromSun.Specular.r = dvR;
	m_LightFromSun.Specular.g = dvG;
	m_LightFromSun.Specular.b = dvB;
	m_LightFromSun.Specular.a = dvA;
	m_LightFromSun.Range = 5000.0f;

	// Light which illuminates the "sun"
	memcpy(&m_LightOnSun, &m_LightFromSun, sizeof(D3DLIGHT9));
	//m_LightOnSun.Type = D3DLIGHT_SPOT;
	m_LightOnSun.Attenuation0 = 0.0f;
	m_LightOnSun.Attenuation1 = 0.003f;
	m_LightOnSun.Attenuation2 = 0.0f;
	m_LightOnSun.Diffuse.r = dvR;
	m_LightOnSun.Diffuse.g = dvG;
	m_LightOnSun.Diffuse.b = dvB;
	m_LightOnSun.Diffuse.a = dvA;
	m_LightOnSun.Specular.r = 1.0f;
	m_LightOnSun.Specular.g = 1.0f;
	m_LightOnSun.Specular.b = 1.0f;
	m_LightOnSun.Specular.a = 1.0f;

	//hr = pD3DDev->LightEnable(0, TRUE);
	//if (FAILED(hr))
	//	return hr;

	memset(&m_SunMaterial, 0, sizeof(D3DMATERIAL9));
	m_SunMaterial.Diffuse.r = dvR;
	m_SunMaterial.Diffuse.g = dvG;
	m_SunMaterial.Diffuse.b = dvB;
	m_SunMaterial.Diffuse.a = dvA;
	m_SunMaterial.Specular.r = 1.0f;
	m_SunMaterial.Specular.g = 1.0f;
	m_SunMaterial.Specular.b = 1.0f;
	m_SunMaterial.Specular.a = 1.0f;
	//m_SunMaterial.Emissive.r = dvR;
	//m_SunMaterial.Emissive.g = dvG;
	//m_SunMaterial.Emissive.b = dvB;
	//m_SunMaterial.Emissive.a = dvA;
	m_SunMaterial.Power = 3.0f;

	for (i = 0; i < NUM_MATERIALS; i++)
	{
		memcpy(&m_PlanetMaterials[i], &m_SunMaterial, sizeof(D3DMATERIAL9));
		m_PlanetMaterials[i].Diffuse.r = Random(1.0f);
		m_PlanetMaterials[i].Diffuse.g = Random(1.0f);
		m_PlanetMaterials[i].Diffuse.b = Random(1.0f);
		//m_PlanetMaterials[i].Emissive.r = 0.0f;
		//m_PlanetMaterials[i].Emissive.g = 0.0f;
		//m_PlanetMaterials[i].Emissive.b = 0.0f;
		//m_PlanetMaterials[i].Emissive.a = 0.0f;
		m_PlanetMaterials[i].Power = 1.0f;
	}

	if (FAILED(hr = GenerateSpheres()))
		return hr;

	//float fPlanetRad;
	for (i = 0; i < NUM_PARTICLES; i++)
	{
		// Make the planets.
		//fPlanetRad = (float)Random(3.2f) + 0.7f;

		D3DXMATRIX initRot;
		D3DXVECTOR3 v(0.0f, 1.0f, 0.0f);
		D3DXMatrixRotationAxis(&initRot, &v, Random(2 * D3DX_PI));// delete "+1"
		m_Particles[i].distance = Random(400.0f) + 50.0f;
		D3DXVECTOR4 vUnit(1.0f, Y_OFFSET(m_Particles[i].distance / 5000.0f), 0.0f, 1.0f);
		D3DXVec4Normalize(&vUnit, &vUnit);
		D3DXVec4Transform(&vUnit, &vUnit, &initRot);
		m_Particles[i].pos = vUnit;

		m_Particles[i].dwMaterial = (DWORD)Random((FLOAT)NUM_MATERIALS);
		m_Particles[i].bAttract = TRUE;
	}

	// Create Matrix Stack
	//D3DXCreateMatrixStack(0, &m_pWorldStack);

	// Create Matrix Stack
	//D3DXCreateMatrixStack(0, &m_pViewStack);

	return S_OK;
}

// ***************************************************************************
// GenerateSpheres
// ***************************************************************************

HRESULT CGravity::GenerateSpheres()
{
	HRESULT hr;

	float fPlanetRad;
	for (int i = 0; i < NUM_SPHERES; i++)
	{
		fPlanetRad = (float)Random(3.2f) + 0.7f;
		hr = D3DXCreateSphere(pD3DDev,
			fPlanetRad,
			(int)max(4, fPlanetRad * 2),
			(int)max(3, fPlanetRad * 3),
			/*1,*/
			&m_Spheres[i],
			NULL);
		if (FAILED(hr))
			return hr;
	}

	// Make the sun.
	hr = D3DXCreateSphere(pD3DDev, 20.0f, 25, 50, /*1,*/ &m_Sun.pSphere, NULL);
	if (FAILED(hr))
		return hr;

	for (int i = 0; i < NUM_PLANETS; i++)
	{
		// Make the planets.
		fPlanetRad = (float)Random(3.2f) + 0.7f;

		D3DXMATRIX initRot;
		D3DXVECTOR3 v(0.0f, 1.0f, 0.0f);
		D3DXMatrixRotationAxis(&initRot, &v, Random(2 * D3DX_PI) + 1);
		m_Planets[i].distance = Random(400.0f) + 50.0f;
		D3DXVECTOR4 vUnit(1.0f, Y_OFFSET((float)m_Planets[i].distance / 5000.0f), 0.0f, 0.0f);
		D3DXVec4Normalize(&vUnit, &vUnit);
		D3DXVec4Transform(&vUnit, &vUnit, &initRot);
		m_Planets[i].pos = vUnit;


		m_Planets[i].dwSphere = (DWORD)Random((FLOAT)NUM_SPHERES);

		m_Planets[i].dwMaterial = (DWORD)Random((FLOAT)NUM_MATERIALS);
		m_Planets[i].bAttract = TRUE;
	}

	return S_OK;
}

void CGravity::DestroySpheres()
{
	for (int i = 0; i < NUM_SPHERES; i++)
	{
		RELEASENULL(m_Spheres[i]);
	}
}

void CGravity::UnInit()
{
	DestroySpheres();
	RELEASENULL(m_Sun.pSphere);
	RELEASENULL(pD3DDev);
	RELEASENULL(pD3D);
	//RELEASENULL(m_pDD);
	//RELEASENULL(m_pWorldStack);
	//RELEASENULL(m_pViewStack);
	//RELEASENULL(m_pD3DX);
	m_bD3DXReady = FALSE;
	//D3DXUninitialize();
}

void CGravity::ApplyGravity(float* pfDistance, D3DXVECTOR4* pPos, BOOL* pbAttract)
{
	BOOL bFlippedState = FALSE;
	if (*pbAttract == TRUE)
	{
		// Do some fake gravity stuff... (a little too much gravity... :) )
		if (*pfDistance > 1)
		{
			// Get sucked towards the sun:
			*pfDistance *= (float)pow(0.999, m_dElapsedTime * 10);

			if (*pfDistance < 50)
			{
				*pfDistance -= (float)(m_dElapsedTime * 15);
			}
			else if (*pfDistance < 100)
			{
				*pfDistance -= (float)(m_dElapsedTime * 10);
			}
			else if (*pfDistance < 150)
			{
				*pfDistance -= (float)(m_dElapsedTime * 5);
			}
			*pfDistance = max(0.1f, *pfDistance);

			D3DXMATRIX rot;
			D3DXVECTOR3 v(0.0f, 1.0f, 0.0f);
			D3DXMatrixRotationAxis(&rot, &v,
				(float)(80.0f * pow(*pfDistance, -1.1f) * m_dElapsedTime));
			D3DXVec4Transform(pPos, pPos, &rot);
			(*pPos).y *= (float)pow(0.999, m_dElapsedTime * 30);
		}
		if (*pfDistance <= 1)
		{
			// Teleport out of the sun.
			*pbAttract = FALSE;
			bFlippedState = TRUE;
		}
	}
	if (*pbAttract == FALSE)
	{
		if (*pfDistance > 500)
		{
			// move to a new far away location, and
			// start getting sucked in again
			*pfDistance = Random(100.0f) + 500.0f;
			(*pPos).y = Y_OFFSET(*pfDistance / 10000.0f);
			D3DXVec4Normalize(pPos, pPos);
			*pbAttract = TRUE;
			return;
		}
		if (bFlippedState)
		{
			(*pPos).y = 20.0f;
			D3DXVec4Normalize(pPos, pPos);
			if (Random(1.0f) > 0.5f)
			{
				(*pPos).y = -(*pPos).y;
			}
			*pfDistance = 10.0f;
		}
		*pfDistance *= (float)pow(2.0, m_dElapsedTime);
	}
}

// *********************************************************************************
// Rendering Code
// *********************************************************************************

HRESULT CGravity::Draw()
{
	HRESULT hr;
	int i;

	if (!m_bD3DXReady)
	{
		return E_FAIL;
	}
	if (!m_bActive)
	{
		return S_OK;
	}

	hr = pD3DDev->BeginScene();
	if (SUCCEEDED(hr))
	{
		//hr = m_pD3DX->Clear(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER);
		//if (FAILED(hr))
		//	return hr;

		if (FAILED(pD3DDev->Clear(0, NULL,
			D3DCLEAR_TARGET |
			D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
			D3DCOLOR_XRGB(0, 0, 0),
			1.0f,
			0)))
		{
			return E_FAIL;
		}

		UpdateTime();

		float fViewDist = 400 + 300 * (float)sin(m_dAbsoluteTime * 0.2);

		//m_LightOnSun.Position.x = 0.0f;
		//m_LightOnSun.Position.y = 0.0f;
		//m_LightOnSun.Position.z = -100.f + fViewDist;
		//pD3DDev->SetLight(0, &m_LightOnSun);
		//pD3DDev->LightEnable(0, TRUE);

		// Set up state for drawing the sun.

		m_fSunRot[0] = (float)(m_dAbsoluteTime / 2);
		m_fSunRot[1] = (float)(m_dAbsoluteTime / 2);
		m_fSunRot[2] = (float)(m_dAbsoluteTime);

		pD3DDev->SetMaterial(&m_SunMaterial);

		D3DXMATRIX matSunWorld, matTemp;
		D3DXVECTOR3 v(1.0f, 0.0f, 0.0f);
		D3DXMatrixRotationAxis(&matSunWorld, &v,
			m_fSunRot[0]);
		D3DXVECTOR3 v1(0.0f, 1.0f, 0.0f);
		D3DXMatrixRotationAxis(&matTemp, &v1,
			m_fSunRot[1]);
		D3DXMatrixMultiply(&matSunWorld, &matSunWorld, &matTemp);
		D3DXVECTOR3 v2(0.0f, 0.0f, 1.0f);
		D3DXMatrixRotationAxis(&matTemp, &v2,
			m_fSunRot[2]);
		D3DXMatrixMultiply(&matSunWorld, &matSunWorld, &matTemp);
		//D3DXMatrixTranslation(&matTemp, 0.0f, 0.0f, fViewDist);
		//D3DXMatrixMultiply(&matSunWorld, &matSunWorld, &matTemp);

		//D3DMATRIX m(matSunWorld);
		pD3DDev->SetTransform(D3DTS_WORLD, &matSunWorld);

		//D3DXMATRIX matSunView;
		//D3DXMatrixIdentity(&matSunView);
		//pD3DDev->SetTransform(D3DTS_VIEW, &matSunView);//Sun

		///////////////////////////////////////////////////
		//D3DVIEWPORT9 vp;
		//if (FAILED(pD3DDev->GetViewport(&vp)))
		//{
		//	return E_FAIL;
		//}
		D3DXMATRIXA16 matProj;
		D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 2, d3dpp.BackBufferWidth / d3dpp.BackBufferHeight, 1.0f, 10000.0f);
		pD3DDev->SetTransform(D3DTS_PROJECTION, &matProj);
		///////////////////////////////////////////////////

		//m_Sun.pSphere->DrawSubset(0);


		//m_pViewStack->LoadIdentity();
		D3DXMatrixIdentity(&m_ViewMat);
		m_fViewRot[0] -= (float)(0.075 * m_dElapsedTime);
		m_fViewRot[1] += (float)(0.01 * m_dElapsedTime);
		m_fViewRot[2] += (float)(0.015 * m_dElapsedTime);

		//D3DXVECTOR3 vv0(1.0f, 0.0f, 0.0f);
		////m_pViewStack->RotateAxis(&vv0, m_fViewRot[0]);
		//D3DXMatrixRotationAxis(&m_ViewMat, &vv0, m_fViewRot[0]);
		//D3DXVECTOR3 vv1(0.0f, 1.0f, 0.0f);
		////m_pViewStack->RotateAxis(&vv1, m_fViewRot[1]);
		//D3DXMatrixRotationAxis(&m_ViewMat, &vv1, m_fViewRot[1]);
		//D3DXVECTOR3 vv2(0.0f, 0.0f, 1.0f);
		////m_pViewStack->RotateAxis(&vv2, m_fViewRot[2]);
		//D3DXMatrixRotationAxis(&m_ViewMat, &vv2, m_fViewRot[2]);
		D3DXMATRIX RotMat;
		D3DXMatrixRotationYawPitchRoll(&RotMat, m_fViewRot[1], m_fViewRot[0], m_fViewRot[2]);
		//m_pViewStack->Translate(0.0f, 0.0f, fViewDist);
		//D3DXMATRIX trans;
		//D3DXMatrixTranslation(&trans, fViewDist, 0.0f, 0.0f);

		D3DXMatrixMultiply(&m_ViewMat, &m_ViewMat, &RotMat);


		//pD3DDev->SetTransform(D3DTS_VIEW,
		//	(D3DMATRIX*)m_pViewStack->GetTop());
		///////////////////////////////////////////////////
		D3DXVECTOR3 m0(0.0f, 0.0f, fViewDist);
		D3DXVec3TransformCoord(&m0, &m0, &m_ViewMat);
		D3DXVECTOR3 m2(0.0f, 0.0f, 0.0f);
		D3DXVECTOR3 m3(0.0f, 1.0f, 0.0f);
		D3DXMatrixLookAtLH(&m_ViewMat, &m0, &m2, &m3);
		///////////////////////////////////////////////////
		pD3DDev->SetTransform(D3DTS_VIEW, (D3DMATRIX*)&m_ViewMat);//Camera

		// Set Light on Sun
		m_LightOnSun.Position = m0;
		//m_LightOnSun.Direction = m2;
		pD3DDev->SetLight(0, &m_LightOnSun);
		pD3DDev->LightEnable(0, TRUE);

		m_Sun.pSphere->DrawSubset(0);

		// Set up state for drawing the planets
		m_LightFromSun.Position.x = 0.0f;
		m_LightFromSun.Position.y = 0.0f;
		m_LightFromSun.Position.z = 0.0f;
		pD3DDev->SetLight(0, &m_LightFromSun);
		pD3DDev->LightEnable(0, TRUE);

		// Draw the planets
		for (i = 0; i < NUM_PLANETS; i++)
		{
			pD3DDev->SetMaterial(&m_PlanetMaterials[m_Planets[i].dwMaterial]);
			//m_pWorldStack->LoadIdentity();
			D3DXMatrixIdentity(&m_WorldMat);

			// Do some fake gravity stuff... (a little too much gravity... :) )
			ApplyGravity(&m_Planets[i].distance, &m_Planets[i].pos, &m_Planets[i].bAttract);
			//m_pWorldStack->Translate(m_Planets[i].pos.x * m_Planets[i].distance,
			//	m_Planets[i].pos.y * m_Planets[i].distance,
			//	m_Planets[i].pos.z * m_Planets[i].distance);
			D3DXMATRIX trans;
			D3DXMatrixTranslation(&trans, m_Planets[i].pos.x * m_Planets[i].distance,
				m_Planets[i].pos.y * m_Planets[i].distance,
				m_Planets[i].pos.z * m_Planets[i].distance);
			D3DXMatrixMultiply(&m_WorldMat, &m_WorldMat, &trans);

			//pD3DDev->SetTransform(D3DTS_WORLD,
			//	(D3DMATRIX*)m_pWorldStack->GetTop());
			pD3DDev->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&m_WorldMat);//Planets

			m_Spheres[m_Planets[i].dwSphere]->DrawSubset(0);
		}

		//m_pWorldStack->LoadIdentity();
		D3DXMatrixIdentity(&m_WorldMat);
		pD3DDev->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&m_WorldMat);//Particles

		// Draw the particles
		D3DVERTEX vParticle;
		for (i = 0; i < NUM_PARTICLES; i++)
		{
			//pD3DDev->SetMaterial(&m_PlanetMaterials[m_Particles[i].dwMaterial]);
			D3DMATERIAL9* pMaterial;
			pMaterial = &m_PlanetMaterials[m_Particles[i].dwMaterial];

			// Do some fake gravity stuff... (a little too much gravity... :) )
			ApplyGravity(&m_Particles[i].distance, &m_Particles[i].pos,
				&m_Particles[i].bAttract);

			vParticle.v.x = m_Particles[i].pos.x * m_Particles[i].distance;
			vParticle.v.y = m_Particles[i].pos.y * m_Particles[i].distance;
			vParticle.v.z = m_Particles[i].pos.z * m_Particles[i].distance;
			vParticle.n.x = -m_Particles[i].pos.x;
			vParticle.n.y = -m_Particles[i].pos.y;
			vParticle.n.z = -m_Particles[i].pos.z;
			vParticle.psize = 1.3f;////////////////////////////////////////
			vParticle.Diffuse = pMaterial->Diffuse;
			vParticle.Specular = pMaterial->Specular;

			pD3DDev->SetFVF(D3DFVF_VERTEX);
			pD3DDev->DrawPrimitiveUP(D3DPT_POINTLIST,
				1,
				&vParticle,
				sizeof(D3DVERTEX)/*,
				D3DDP_WAIT*/);//Particles
		}

		pD3DDev->EndScene();
	}

	//hr = m_pD3DX->UpdateFrame(0);
	//if (hr == DDERR_SURFACELOST || hr == DDERR_SURFACEBUSY)
	//	hr = HandleModeChanges();
	if (FAILED(pD3DDev->Present(0, 0, 0, 0))) {
		//m_pD3DDev->Reset(&d3dpp);
		hr = HandleModeChanges();
	}

	return hr;
}

HRESULT CGravity::HandleModeChanges()
{
	HRESULT hr;
	hr = pD3DDev->TestCooperativeLevel();

	if (SUCCEEDED(hr) || hr == D3DERR_DEVICELOST)
	{
		UnInit();

		if (FAILED(hr = InitD3DX()))
			return hr;
	}
	else if (hr != D3DERR_DEVICENOTRESET &&
		hr != D3DERR_DRIVERINTERNALERROR)
	{
		// Busted!!
		return hr;
	}
	return S_OK;
}



// アプリケーションのエントリー関数
int APIENTRY WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInstance, _In_   LPSTR lpCmdLine, _In_ int nCmdShow)
{
	HRESULT hr;
	HACCEL hAccelApp;
	HCURSOR hcur = NULL;
	int ret = 0;

	HWND hWnd = NULL;
	MSG msg;

	//ウィンドウの初期化
	const WCHAR szAppName[] = NAME_OF_THE_APP;	//changed
	WNDCLASSEX wndclass;


	g_pGravity = new CGravity; // set up our data AFTER starting up d3dx!
	if (!g_pGravity)
	{
		ret = -1;
		goto Exit;
	}


	wndclass.cbSize = sizeof(wndclass);
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInst;
	wndclass.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
	wndclass.hCursor = hcur; //LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = szAppName;
	wndclass.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));

	RegisterClassEx(&wndclass);

	g_pGravity->m_hwndMain = CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 400, 400, NULL, NULL, hInst, NULL);

	if (!g_pGravity->m_hwndMain)
	{
		ret = -1;
		goto Exit;
	}

	// Hide the cursor if necessary
	if (g_pGravity->m_bIsFullscreen)
	{
		ShowCursor(FALSE);
	}


	ShowWindow(g_pGravity->m_hwndMain, nCmdShow);
	UpdateWindow(g_pGravity->m_hwndMain);

	//Direct3D初期化
	//if (FAILED(InitD3d(hWnd)))
	//{
	//	return(0);
	//}

	hAccelApp = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));
	if (!hAccelApp)
	{
		ret = -1;
		goto Exit;
	}

	// Initialize D3DX
	hr = g_pGravity->InitD3DX();
	if (FAILED(hr))
	{
		InterpretError(hr);
		ret = -1;
		goto Exit;
	}

	//メッセージループ
	ZeroMemory(&msg, sizeof(msg));
	PeekMessage(&msg, NULL, 0U, 0U, PM_NOREMOVE);
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			if (!TranslateAccelerator(g_pGravity->m_hwndMain, hAccelApp, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			//Draw();
			if (g_pGravity && g_pGravity->m_bActive)
			{
				hr = g_pGravity->Draw();
				if (FAILED(hr))
				{
					InterpretError(hr);
					g_pGravity->m_bD3DXReady = FALSE;
					PostQuitMessage(-1);
				}
			}
			else
			{
				WaitMessage();
			}
		}
	}
	//return(INT)msg.wParam;
	delete g_pGravity;

Exit:
	if (hcur)
		DestroyCursor(hcur);

	return ret;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ACTIVATEAPP:
	{
		if (!g_pGravity)
			break;

		if (g_pGravity->m_bIsFullscreen)
		{
			if ((BOOL)wParam)
				g_pGravity->RestartDrawing();
			else
				g_pGravity->PauseDrawing();
		}
	}
	break;
	case WM_CREATE:
		break;
	case WM_CLOSE:
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		if (g_pGravity
			&& g_pGravity->m_bD3DXReady
			&& !g_pGravity->m_bIsFullscreen
			)
		{
			HRESULT hr;

			if (wParam == SIZE_MINIMIZED)
			{
				g_pGravity->m_bActive = FALSE;
				break;
			}
			else if (LOWORD(lParam) > 0 && HIWORD(lParam) > 0)//Resize
			{
				d3dpp.BackBufferWidth = LOWORD(lParam);
				d3dpp.BackBufferHeight = HIWORD(lParam);
				if (FAILED(hr = pD3DDev->Reset(&d3dpp)))
				{
					InterpretError(hr);
					g_pGravity->m_bD3DXReady = FALSE;
					PostQuitMessage(0);
				}
			};
			g_pGravity->m_bActive = TRUE;

		}
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
		{
			PostQuitMessage(0);
			break;
		}
		}
		break;
	case WM_COMMAND:
		if (1 == HIWORD(wParam))
		{
			switch (LOWORD(wParam))
			{
			case IDM_FULLSCREEN: // Alt + Return Key
				if (g_pGravity && g_pGravity->m_bD3DXReady)
				{
					HRESULT hr;
					g_pGravity->m_bIsFullscreen = !g_pGravity->m_bIsFullscreen;
					g_pGravity->m_bD3DXReady = FALSE;

					if (g_pGravity->m_bIsFullscreen)
					{
						// going to fullscreen
						GetWindowRect(hWnd, &g_pGravity->m_rWindowedRect);
					}
					ShowCursor(!(g_pGravity->m_bIsFullscreen));
					hr = pD3DDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
					if (FAILED(hr))
					{
						InterpretError(hr);
						g_pGravity->PauseDrawing();
						PostQuitMessage(-1);
						break;
					}
					g_pGravity->UnInit();

					if (!g_pGravity->m_bIsFullscreen)
					{
						RECT& r = g_pGravity->m_rWindowedRect;
						SetWindowPos(hWnd, HWND_NOTOPMOST,
							r.left,
							r.top,
							r.right - r.left,
							r.bottom - r.top,
							SWP_NOACTIVATE);
					}

					hr = g_pGravity->InitD3DX();
					if (FAILED(hr))
					{
						InterpretError(hr);
						g_pGravity->PauseDrawing();
						PostQuitMessage(-1);
						break;
					}
				}
				break;
			}
		}
		break;
	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

