// dui-demo.cpp : main source file
//
#ifndef LOG_FILTER
#define LOG_FILTER "PDown"
#endif
#include "stdafx.h"
#include "trayicon/SShellNotifyIcon.h"
#include "trayicon/STreeFile.h"
#include "trayicon/ColorText.h"
#include "trayicon/SIECtrl.h"
#include "MainDlg.h"
#include <helper/SLogDef.h>
#include "AppStart.h"
#include "AppInfo.h"
#include "Utils/S2W.h"
#include "Utils/ThreadPool.h"
#include "DAL/DBHelper.h"
#include "Utils/Blink.h"
#include "teemo/teemo.h"


#ifdef _DEBUG
#define SYS_NAMED_RESOURCE _T("soui-sys-resourced.dll")
#else
#define SYS_NAMED_RESOURCE _T("soui-sys-resource.dll")
#endif


class SOUIEngine
{
private:
	SComMgr m_ComMgr;
	SApplication* m_theApp = nullptr;
	bool m_bInitSucessed = false;
	SAutoRefPtr<ILog4zManager>  pLogMgr;                //log4z对象
public:
	SOUIEngine(HINSTANCE hInstance) :m_theApp(NULL), m_bInitSucessed(false) {
		if (m_ComMgr.CreateLog4z((IObjRef**)&pLogMgr) && pLogMgr)
		{
			std::string logdir = S2W::WString2String(AppInfo::GetI()->AppDir, CP_ACP);
			pLogMgr->setLoggerPath(LOG4Z_MAIN_LOGGER_ID, logdir.c_str());
			pLogMgr->setLoggerName(LOG4Z_MAIN_LOGGER_ID, APP_NAME);
			pLogMgr->setLoggerOutFile(LOG4Z_MAIN_LOGGER_ID, true);
			pLogMgr->setLoggerDisplay(LOG4Z_MAIN_LOGGER_ID, false);
			pLogMgr->setLoggerLevel(LOG4Z_MAIN_LOGGER_ID, ILog4zManager::LOG_LEVEL_WARN);
			pLogMgr->start();
		}

		CAutoRefPtr<SOUI::IRenderFactory> pRenderFactory;
		BOOL bLoaded = FALSE;
		//使用SKIA渲染界面
		//bLoaded = m_ComMgr.CreateRender_Skia((IObjRef**)&pRenderFactory);
		bLoaded = m_ComMgr.CreateRender_GDI((IObjRef**)&pRenderFactory);
		CAutoRefPtr<SOUI::IImgDecoderFactory> pImgDecoderFactory;
		bLoaded = m_ComMgr.CreateImgDecoder((IObjRef**)&pImgDecoderFactory);
		pRenderFactory->SetImgDecoderFactory(pImgDecoderFactory);
		m_theApp = new SApplication(pRenderFactory, hInstance, APP_NAMEL, SObjectDefaultRegister(), false);
		m_theApp->SetLogManager(pLogMgr);

		m_bInitSucessed = (TRUE == bLoaded);
	};
	operator bool()const
	{
		return m_bInitSucessed;
	}
	//加载系统资源
	bool LoadSystemRes()
	{
		BOOL bLoaded = FALSE;

#ifdef _DEBUG
		//选择了仅在Release版本打包资源则系统资源在DEBUG下始终使用DLL加载
		{
			HMODULE hModSysResource = LoadLibrary(SYS_NAMED_RESOURCE);
			if (hModSysResource)
			{
				CAutoRefPtr<IResProvider> sysResProvider;
				CreateResProvider(RES_PE, (IObjRef**)&sysResProvider);
				sysResProvider->Init((WPARAM)hModSysResource, 0);
				bLoaded = m_theApp->LoadSystemNamedResource(sysResProvider);
				FreeLibrary(hModSysResource);
			}
			else
			{
				SASSERT(0);
			}
		}
#else
		//钩选了复制系统资源选项
		{
			CAutoRefPtr<IResProvider> pSysResProvider;
			CreateResProvider(RES_PE, (IObjRef**)&pSysResProvider);
			bLoaded = pSysResProvider->Init((WPARAM)m_theApp->GetInstance(), 0);
			SASSERT(bLoaded);
			bLoaded = m_theApp->LoadSystemNamedResource(pSysResProvider);
			SASSERT(bLoaded);
		}
#endif
		return TRUE == bLoaded;
	}
	//加载用户资源
	bool LoadUserRes()
	{
		CAutoRefPtr<IResProvider>   pResProvider;
		BOOL bLoaded = FALSE;
#ifdef _DEBUG		
		//选择了仅在Release版本打包资源则在DEBUG下始终使用文件加载
		{
			CreateResProvider(RES_FILE, (IObjRef**)&pResProvider);
			bLoaded = pResProvider->Init((LPARAM)_T("uires"), 0);
			SASSERT(bLoaded);
		}
#else
		{
			CreateResProvider(RES_PE, (IObjRef**)&pResProvider);
			bLoaded = pResProvider->Init((WPARAM)m_theApp->GetInstance(), 0);
			SASSERT(bLoaded);
		}
#endif

		m_theApp->AddResProvider(pResProvider);

		return TRUE == bLoaded;
	}
	//加载LUA支持
	bool LoadLUAModule()
	{
		BOOL bLoaded = FALSE;
		CAutoRefPtr<SOUI::IScriptFactory> pScriptLuaFactory;
		bLoaded = m_ComMgr.CreateScrpit_Lua((IObjRef**)&pScriptLuaFactory);
		SASSERT_FMT(bLoaded, _T("load interface [%s] failed!"), _T("scirpt_lua"));
		m_theApp->SetScriptFactory(pScriptLuaFactory);
		return TRUE == bLoaded;
	}
	//加载多语言支持
	bool LoadMultiLangsModule()
	{
		return true;
	}
	//注册用户自定义皮肤和控件
	void Regitercustom()
	{
		m_theApp->RegisterWindowClass<SIECtrl>();//注册IECtrl
		m_theApp->RegisterWindowClass<SShellNotifyIcon>();
		m_theApp->RegisterWindowClass<STreeFile>();
		m_theApp->RegisterWindowClass<ColorText>();

	}

	~SOUIEngine()
	{
		pLogMgr->stop();
		if (m_theApp)
		{
			delete m_theApp;
			m_theApp = NULL;
		}
	}

	template<class MainWnd>
	int Run()
	{
		MainWnd dlgMain;
		dlgMain.Create(GetActiveWindow());
		dlgMain.SendMessage(WM_INITDIALOG);
		dlgMain.CenterWindow(dlgMain.m_hWnd);
		dlgMain.ShowWindow(SW_SHOWNORMAL);




		return m_theApp->Run(dlgMain.m_hWnd);
	}

	SApplication* GetApp()
	{
		return m_theApp;
	}
};
//debug时方便调试设置当前目录以便从文件加载资源
std::wstring SetDefaultDir()
{
	TCHAR szCurrentDir[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szCurrentDir, sizeof(szCurrentDir));

	LPTSTR lpInsertPos = _tcsrchr(szCurrentDir, _T('\\'));
#ifdef _DEBUG
	_tcscpy(lpInsertPos + 1, _T("..\\"));
#else
	_tcscpy(lpInsertPos + 1, _T("\0"));
#endif
	SetCurrentDirectory(szCurrentDir);
	return std::wstring(szCurrentDir);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int /*nCmdShow*/)
{
	//APP重启时，先清理hMutex

	std::wstring appdir = SetDefaultDir();
	//if (AppStart::checkRunFromLocal()) return 0;
	if (AppStart::checkMyselfExist()) return 0;
	AppStart::checkBackUpLog();



	HRESULT hRes = OleInitialize(NULL);
	SASSERT(SUCCEEDED(hRes));

	int nRet = 0;
	{
		SOUIEngine souiEngine(hInstance);
		if (souiEngine)
		{
			//加载系统资源
			souiEngine.LoadSystemRes();
			//加载用户资源
			souiEngine.LoadUserRes();
			//注册用户自定义皮肤/控件/布局/其它自定义
			souiEngine.Regitercustom();
			//自定义的初始化
			AppStart::AppStartInit(souiEngine.GetApp());

			SLOG_ERROR(APP_NAME);
			SLOG_ERROR(APP_VER);
			SLOGFMTE(L"log appdir=%ws", appdir.c_str());
			SLOGFMTE(L"log AppDir=%ws", AppInfo::GetI()->AppDir.c_str());
			nRet = souiEngine.Run<CMainDlg>();
			//自定义的清理
			AppStart::AppCloseClean();
			SLOGFMTE(L"log appexit=%ws", appdir.c_str());

		}
		else
		{
			SLOGFMTE(L"log straterror=%ws", appdir.c_str());
			MessageBox(NULL, _T("无法正常初使化SOUI"), _T("错误"), MB_OK | MB_ICONERROR);
		}
	}
	OleUninitialize();
	return nRet;
}
