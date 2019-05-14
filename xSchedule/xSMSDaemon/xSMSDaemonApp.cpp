/***************************************************************
 * Name:      xSMSDaemonApp.cpp
 * Purpose:   Code for Application Class
 * Author:    xLights ()
 * Created:   2016-12-30
 * Copyright: xLights (http://xlights.org)
 * License:
 **************************************************************/

//(*AppHeaders
#include "xSMSDaemonMain.h"
#include <wx/image.h>
//*)

#include "xSMSDaemonApp.h"

#include <log4cpp/Category.hh>
#include <log4cpp/PropertyConfigurator.hh>
#include <log4cpp/Configurator.hh>
#include <wx/file.h>
#include <wx/msgdlg.h>

#include "../../xLights/xLightsVersion.h"
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/debugrpt.h>
#include <wx/cmdline.h>
#include <wx/confbase.h>
#include <wx/snglinst.h>

#ifdef __WXMSW__
#include <wx/msw/private.h>
#endif

IMPLEMENT_APP_NO_MAIN(xSMSDaemonApp)

//HANDLE ThreadId;
std::string __showDir;
std::string __xScheduleURL;
bool __started = false;
p_xSchedule_Action __action;

void WipeSettings()
{
    static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    logger_base.warn("------ Wiping xSMSDaemon settings ------");

    wxConfigBase* config = wxConfigBase::Get();
    config->DeleteAll();
}

extern "C" {
    // always called when the dll is found ... should not actually do anything
    bool __declspec(dllexport) xSchedule_Load(char* showDir)
    {
        __showDir = std::string(showDir);
        return true;
    }

    void __declspec(dllexport) xSchedule_GetVirtualWebFolder(char* buffer, size_t bufferSize)
    {
        memset(buffer, 0x00, bufferSize);
        strncpy(buffer, "xSMSDaemon", bufferSize - 1);
    }

    void __declspec(dllexport) xSchedule_GetMenuLabel(char* buffer, size_t bufferSize)
    {
        memset(buffer, 0x00, bufferSize);
        strncpy(buffer, "SMS", bufferSize - 1);
    }

    bool __declspec(dllexport) xSchedule_HandleWeb(const char* command, const wchar_t* parameters, const wchar_t* data, const wchar_t* reference, wchar_t* response, size_t responseSize)
    {
        std::wstring resp;
        memset(response, 0x00, responseSize);
        bool res = ((xSMSDaemonFrame*)wxTheApp->GetTopWindow())->Action(std::string(command), std::wstring(parameters), std::wstring(data), std::wstring(reference), resp);
        wchar_t* pr = (wchar_t*)resp.c_str();
        wcsncpy(response, pr, (responseSize / 2) - 1); // divide by 2 as 2 byte characters
        return res;
    }

    // called when we want the plugin to actually interact with the user
    bool __declspec(dllexport) xSchedule_Start(char* showDir, char* xScheduleURL, p_xSchedule_Action action)
    {
        if (__started) return true;

        __action = action;
        __showDir = std::string(showDir);
        __xScheduleURL = std::string(xScheduleURL);

        //ThreadId = CreateThread(NULL, 0, ThreadProc, NULL, 0, NULL);

        int argc = 0;
        char** argv = NULL;
        wxEntryStart(argc, argv);
        if (!wxTheApp || !wxTheApp->CallOnInit())
            return false;

        __started = true;

        return true;
    }

    // called when we want the plugin to exit
    void __declspec(dllexport) xSchedule_Stop()
    {
        static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));

        if (!__started) return;

        wxEntryCleanup();
        __started = false;
    }

    void __declspec(dllexport) xSchedule_WipeSettings()
    {
        WipeSettings();
    }

    // called just before xSchedule exits
    void __declspec(dllexport) xSchedule_Unload()
    {
    }
}

#ifdef __WXMSW__
BOOL APIENTRY DllMain(HANDLE hModule,
    DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        wxSetInstance((HINSTANCE)hModule);
        break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}
#endif

int xSMSDaemonApp::OnExit()
{
    static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    logger_base.info("xSMSDaemon exiting.");

    return 0;
}

void InitialiseLogging(bool fromMain)
{
    static bool loggingInitialised = false;

    if (!loggingInitialised)
    {

#ifdef __WXMSW__
        std::string initFileName = "xschedule.windows.properties";
#endif
#ifdef __WXOSX_MAC__
        std::string initFileName = "xschedule.mac.properties";
        std::string resourceName = wxStandardPaths::Get().GetResourcesDir().ToStdString() + "/xschedule.mac.properties";
        if (!wxFile::Exists(initFileName)) {
            if (fromMain) {
                return;
            }
            else if (wxFile::Exists(resourceName)) {
                initFileName = resourceName;
            }
        }
        loggingInitialised = true;

#endif
#ifdef __LINUX__
        std::string initFileName = wxStandardPaths::Get().GetInstallPrefix() + "/bin/xschedule.linux.properties";
        if (!wxFile::Exists(initFileName)) {
            initFileName = wxStandardPaths::Get().GetInstallPrefix() + "/share/xLights/xschedule.linux.properties";
        }
#endif

        if (!wxFile::Exists(initFileName))
        {
#ifdef _MSC_VER
            // the app is not initialized so GUI is not available and no event loop.
            wxMessageBox(initFileName + " not found in " + wxGetCwd() + ". Logging disabled.");
#endif
        }
        else
        {
            try
            {
                log4cpp::PropertyConfigurator::configure(initFileName);
            }
            catch (log4cpp::ConfigureFailure & e) {
                // ignore config failure ... but logging wont work
                printf("Log issue:  %s\n", e.what());
            }
            catch (const std::exception & ex) {
                printf("Log issue: %s\n", ex.what());
            }
        }
    }
}

bool xSMSDaemonApp::OnInit()
{
    InitialiseLogging(false);

    static log4cpp::Category & logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    logger_base.info("******* OnInit: xSMSDaemon started.");

    //(*AppInitialize
    xSMSDaemonFrame* Frame = new xSMSDaemonFrame(0, __showDir, __xScheduleURL, __action);
    Frame->Show();

    return true;
}