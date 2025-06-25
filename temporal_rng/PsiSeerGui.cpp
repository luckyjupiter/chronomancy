#include "PsiSeerGui.hpp"
#include "PsiSeerApp.hpp"

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Box.H>
#include <Fl/Fl_Round_Button.H>
#include <Fl/Fl_Group.H>
#include <Fl/Fl_Output.H>
#include <Fl/Fl_Value_Slider.H>
#include <Fl/Fl_Help_View.H>
#include <Fl/Fl_Light_Button.H>
#include <Fl/Fl_PNG_Image.H>
#include <Fl/Fl_Menu.H>
#include <Fl/Fl_Menu_Bar.H>
#include <Fl/Fl_Text_Display.H>
#include <shlobj.h>
#include <sstream>
#include "GuiCommandLayer.hpp"
#include <cstdio>
#include <iostream>
#include "PsiLogger.h"
#include "test/threads.h"
#include "Logger.hpp"
#include "BoundedRandomWalk.hpp"

#include "SessionPlot.hpp"

int netDir;
int netIndex;


#define RATE    0.001             // how long to hold each image
#define TOTAL   39              // total images: /var/tmp/000{1,2,3}.jpg


#define PSISEER_APPNAME "METrainer"
#define PSISEER_VERSION "1.0"
#define PSISEER_VERSION_FINE "1.0"

#define LICENSE_ID_    "psitrainer@psigenics.com:aiz:ghqehesme"

extern FnPrdOpen PrdOpen;
extern FnPrdGetSource PrdGetSource;
extern FnSetPsiMode SetPsiMode;
extern FnGetPsiMode GetPsiMode;
extern FnPrdWrite PrdWrite;
extern FnPrdClose PrdClose;

extern int prdHandle;

#define PSISEER_IMPULSE_LEN 14

//extern FnGetAvailableSources PRD_GetAvailableSources;
//extern FnSetSource PRD_SetSource;
//extern FnSetMode PRD_SetIntention;

// ASCII -3
//generic -- username not authenticated
//char* userGlobal_ = "_salobk";
//char* passGlobal_ = "lmbo^qlo.-";


Fl_PNG_Image *icosphere_pngs[TOTAL];	// loaded images
bool isGreen = true;
double rate___;


// LOAD ALL IMAGES INTO MEMORY
int LoadImages() {
    for ( int t=0; t<TOTAL; t++ ) {
        char filename[80];
        sprintf(filename, "Media/Images/IcoSphere/%04d.png", t+1);     // 0001.png, 0002.png..
        icosphere_pngs[t] = new Fl_PNG_Image(filename);
        if ( icosphere_pngs[t]->w() == 0 ) { perror(filename); return(1);}
    }
    return(0);
}

int ones___ = 0;
int zeros___ = 0;
Fl_PNG_Image backImage("Media/Images/Background.png");
PsiSeerGui::PsiSeerGui(PsiSeerApp* psiSeerApp)
    : psiSeerApp_(psiSeerApp)
    , helpWindow_(0)
    , isSelfTesting_(false)
    , isAboutShown_(false)
    , animationThread_(0)
	, haveHardware_(false)
{
    // Load animation images
    fl_register_images();
	unsigned long start = GetTickCount();
    if ( LoadImages() )
        throw "image load error";

	unsigned long end  = GetTickCount();
	printf ("%u", end-start);

    chevronLargeDown_ = new Fl_PNG_Image("Media/Images/smallbutton.png");
    chevronLargeUp_ = new Fl_PNG_Image("Media/Images/largebutton.png");

    Fl::scheme("plastic"); // This looks nicer than default
    Fl::lock(); // FLTK multithreading init

    // Initial animation settings (no rotation)
    rotationFilter_.Init(21.65/41.);
    windowP_ = 21/41;

    // Create menu bar
    Fl_Menu_Item MenuItems[] = {
        { "File",      0, 0, 0, FL_SUBMENU },
            {"    Auto Test With Log", 0, (Fl_Callback*)SelfTest, (void*)this},
            {"    Auto Test No Log", 0, (Fl_Callback*)SelfTestNoLog, (void*)this},
            {"Disable Logging",   0, (Fl_Callback*)ToggleLogging, (void*)this, FL_MENU_TOGGLE},
            {"    Exit",      0, (Fl_Callback*)Closer, (void*)this},
        { 0 },

        { "ME Mode",           0, 0, 0, FL_SUBMENU },
            {"Affect",    0, (Fl_Callback*)SetPsiModePK, (void*)this, FL_MENU_RADIO | FL_MENU_VALUE},
            {"Reveal",    0, (Fl_Callback*)SetPsiModeCV, (void*)this, FL_MENU_RADIO},
            {"Predict",    0, (Fl_Callback*)SetPsiModePC, (void*)this, FL_MENU_RADIO},
        { 0 },
        // Hardware = Hardware - Bias
        { "Rng Source",           0,  0, 0, FL_SUBMENU },
            {"PCQNG",         0, (Fl_Callback*)SetSourcePcqn, (void*)this, FL_MENU_RADIO | FL_MENU_VALUE},
            {"Hardware",      0, (Fl_Callback*)SetSourceHdwr, (void*)this, FL_MENU_RADIO},
            {"Hardware - AC",      0, (Fl_Callback*)SetSourceHdwrAc, (void*)this, FL_MENU_RADIO},
            {"PRNG",          0, (Fl_Callback*)SetSourcePrng, (void*)this, FL_MENU_RADIO},
        { 0 },

        { "Options",        0, 0, 0, FL_SUBMENU },
            {"Show Stats",      0, (Fl_Callback*)ToggleStatsView, (void*)this, FL_MENU_TOGGLE},
            {"Hide ME Level",   0, (Fl_Callback*)TogglePsiLevelView, (void*)this, FL_MENU_TOGGLE},
            //            {"Reset Stats",     0, (Fl_Callback*)ResetStats, (void*)this},
//            {"Practice Mode",   0, (Fl_Callback*)TogglePracticeMode, (void*)this, FL_MENU_TOGGLE},
            {"Mute Sounds",     0, (Fl_Callback*)ToggleSounds, (void*)this, FL_MENU_TOGGLE},
            {"Mute Miss Sound",     0, (Fl_Callback*)ToggleMissSound, (void*)this, FL_MENU_TOGGLE},
            {"Hide Miss Ball",     0, (Fl_Callback*)ToggleMissBall, (void*)this, FL_MENU_TOGGLE},
            {"    Change Target", 0, (Fl_Callback*)ToggleTargetDirection, (void*)this},
//			{"PRNG",          0, (Fl_Callback*)SetSourcePrng, (void*)this, FL_MENU_RADIO},
        { 0 },
        

        { "Help",        0, 0, 0, FL_SUBMENU },
            {"Quick Start",   0, (Fl_Callback*)ShowQStart, (void*)this},
            {"Help File",   0, (Fl_Callback*)ShowHelp, (void*)this},
//            {"About",       0, (Fl_Callback*)ToggleAbout, (void*)this},
        { 0 },

//        { "About",        0, 0, 0, FL_SUBMENU },
//        { 0 },

        { 0 }
    };

    isMuted_ = false;
	isMissMuted_ = false;
	isMissVisible_ = true;
    isTargetInverted_ = false;
    arePsiLevelVisible_ = true;

    // Create the main GUI window/layout
    mainWindow_ = new Fl_Double_Window(640, 450, PSISEER_APPNAME " " PSISEER_VERSION);
        commandLayer_ = new GuiCommandLayer(0, 30, 640, 420);
            commandLayer_->box(FL_NO_BOX);
            commandLayer_->callback((Fl_Callback*)Command, (void*)(this));
            commandLayer_->set_visible_focus();
            commandLayer_->image(backImage);
            commandLayer_->deactivate();
        menuBar_ = new Fl_Menu_Bar(0, 0, 640, 30);
            menuBar_->copy(MenuItems);
        practiceModeIndicator_ = new Fl_Box(530, 0, 100, 30, "Practice Mode");
            practiceModeIndicator_->labelfont(1);
            practiceModeIndicator_->labelcolor(FL_DARK_RED);
            practiceModeIndicator_->hide();
            inPracticeMode_ = false;
        statusIndicator_ = new Fl_Box(530, 0, 100, 30, "Target High");
            statusIndicator_->labelfont(1);
//            statusIndicator_->labelcolor(FL_BLUE);
        modeIndicator_ = new Fl_Box(430, 0, 100, 30, "Affect");
            modeIndicator_->labelfont(1);
/*        cmdBox_ = new Fl_Input(280, 8, 20, 15, "");
//            cmdBox_->labelfont(1);
            cmdBox_->labelsize(10);
            cmdBox_->textsize(10);
			cmdBox_->callback(&PsiSeerGui::OnNewCommand, this);
			cmdBox_->when(FL_WHEN_CHANGED);
        sendCmdButton_ = new Fl_Button(299, 8, 20, 15, ">");
            sendCmdButton_->labelfont(1);
            sendCmdButton_->labelsize(10);
//            xCmdButton_->labelcolor((Fl_Color)FL_BLACK);
            sendCmdButton_->callback(&PsiSeerGui::OnSendCommand, this);*/
        sourceIndicator_ = new Fl_Box(320, 0, 100, 30, "RNG:    ");
            sourceIndicator_->labelfont(1);
//            modeIndicator_->labelcolor(FL_BLUE)
        trialStats_ = new Fl_Group(25, 75, 170, 320, " ");
            trialStats_->box(FL_PLASTIC_DOWN_BOX);
            trialStats_->labelcolor(FL_WHITE);
            trialStats_->labelfont(1);
            trialStats_->align(FL_ALIGN_TOP);
            trialN_ = new Fl_Output(100, 101, 70, 24, "Total");
            trialN_->set_output();
            trialHr_ = new Fl_Output(100, 152, 70, 24, "Hit Rate");
            trialHr_->set_output();
            trialPHr_ = new Fl_Output(100, 199, 70, 24, "p(HR)");
            trialPHr_->set_output();
            windowHr_ = new Fl_Output(100, 256, 70, 24, "Win HR");
            windowHr_->set_output();
            windowPHr_ = new Fl_Output(100, 301, 70, 24, "p(WinHR)");
            windowPHr_->set_output();
            trialStats_->hide();
            areStatsVisible_ = false;
            resetStats_ = new Fl_Button(60, 350, 100, 25, "Reset Stats");
                resetStats_->callback((Fl_Callback*)ResetStats, (void*)(this));
        trialStats_->end();
        hitBox_ = new Fl_Box(310, 116, 24, 24, "");
//            hitBox_->box(FL_BOX);
//            hitBox_->color(fl_rgb_color(255, 236, 44));
            hitBox_->image(chevronLargeUp_);
            hitBox_->hide();
        missBox_ = new Fl_Box(310, 355, 24, 24, "");
//            missBox_->box(FL_BOX);
            missBox_->image(chevronLargeDown_);
//            missBox_->color(fl_rgb_color(255, 236, 44));
            missBox_->hide();
        psiBox_ = new Fl_Group(247, 355, 150, 150, " ");
            psiBox_->image(icosphere_pngs[0]);
        psiBox_->end();
//        Fl_Box* copyrightBox = new Fl_Box(236, 270, 200, 19, "\251 2006 Psigenics Corporation");
//            copyrightBox->labelcolor(FL_WHITE);
/*        invertButton_ = new Fl_Light_Button(220, 25, 60, 20, "Invert");
            invertButton_->color(FL_BACKGROUND2_COLOR);
            invertButton_->selection_color(FL_RED);
            invertButton_->hide();*/
        valSliderLabel_ = new Fl_Box(510, 70, 50, 10, "Current");
            valSliderLabel_->labelfont(1);
            valSliderLabel_->labelcolor(FL_WHITE);
        psiValSlider_ = new Fl_Value_Slider(510, 85, 50, 320, "Current");
          psiValSlider_->type(2);
          psiValSlider_->selection_color((Fl_Color)224);
          psiValSlider_->labeltype(FL_NO_LABEL);
          psiValSlider_->labelcolor(FL_BLUE);
          psiValSlider_->align(FL_ALIGN_TOP);
          psiValSlider_->textfont(1);
          psiValSlider_->textsize(18);
          psiValSlider_->textcolor(216);
          psiValSlider_->minimum(20.0);
          psiValSlider_->maximum(1.0);
          psiValSlider_->precision(1);
          psiValSlider_->set_output();
          psiValSlider_->value(1.0);
        psiValMax_ = new Fl_Box(500, 380, 70, 25, "1.0");
            psiValMax_->box(FL_ROUND_UP_BOX);
            psiValMax_->color((Fl_Color)FL_WHITE);
            psiValMax_->selection_color((Fl_Color)1);
            psiValMax_->labelfont(1);
            psiValMax_->labelsize(19);
            psiValMax_->labelcolor((Fl_Color)216);
/*        helpShowButton_ = new Fl_Button(282, 10, 100, 25, "Help");
            helpShowButton_->callback(&PsiSeerGui::ShowHelp, this);
            helpShowButton_->deactivate();*/
        loginDialog_ = new Fl_Group(160, 115, 320, 210);
            loginDialog_->box(FL_RSHADOW_BOX);
            loginDialog_->color(FL_BACKGROUND2_COLOR);
            Fl_Box* loginLabel = new Fl_Box(185, 124, 270, 38, "Enter Your Name");
                loginLabel->labeltype(FL_EMBOSSED_LABEL);
                loginLabel->labelfont(1);
                loginLabel->labelsize(23);
                loginLabel->labelcolor((Fl_Color)4);
            userIdBox_ = new Fl_Input(273, 172, 160, 32, "Name ");
                userIdBox_->labelfont(1);
                userIdBox_->labelsize(20);
                userIdBox_->textsize(20);
                userIdBox_->take_focus();
//            Fl_Box* processingLabel = new Fl_Box(190, 208, 270, 38, "Bounded Walk Processing");
//                processingLabel->labeltype(FL_EMBOSSED_LABEL);
//                processingLabel->labelfont(1);
//                processingLabel->labelsize(12);
//                processingLabel->labelcolor((Fl_Color)0);
//            Fl_Box* remoteApiLabel = new Fl_Box(206, 228, 270, 38, "Allow Remote Sockets Interface");
//                remoteApiLabel->labeltype(FL_EMBOSSED_LABEL);
//                remoteApiLabel->labelfont(1);
//                remoteApiLabel->labelsize(12);
//                remoteApiLabel->labelcolor((Fl_Color)0);
            checkBrw_ = new Fl_Check_Button(225, 218, 18, 18);
            checkSocketsApi_ = new Fl_Check_Button(225, 238, 18, 18);
            checkBrw_->hide();
            checkSocketsApi_->hide();
            loginButton_ = new Fl_Button(235, 275, 80, 30, "Start");
                loginButton_->labelfont(1);
                loginButton_->labelsize(18);
                loginButton_->labelcolor((Fl_Color)FL_BLUE);
                loginButton_->callback(&PsiSeerGui::Authenticate, this);
            Fl_Button* cancelButton = new Fl_Button(325, 275, 80, 30, "Cancel");
                cancelButton->labelfont(1);
                cancelButton->labelsize(18);
                cancelButton->labelcolor((Fl_Color)FL_BLUE);
                cancelButton->callback(&PsiSeerGui::Kill, this);
        loginDialog_->end();
        aboutDialog_ = new Fl_Group(160, 115, 320, 210);
            aboutDialog_->box(FL_RSHADOW_BOX);
            aboutDialog_->color(FL_BACKGROUND2_COLOR);
            new Fl_Box(185, 116, 270, 180, "About METrainer PC\n\nCore Invention METrainer(TM) PC\nVersion 2.1\n\nCopyright 2020 Core Invention, Inc.\nMETrainer(TM) is a trademark of\nCore Invention, Inc. All other\ntrademarks are trademarks of\ntheir respective owners.\n\nMETrainer PC is protected by\nUS Patent Nos. 6,862,605 B2, 6,763,364 B1\n8,073,631 and foreign patents.");
        aboutDialog_->end();
        aboutDialog_->hide();
        selfTestEndButton_ = new Fl_Button(300, 190, 150, 60, "End Auto Test\nExit METrainer");
            selfTestEndButton_->labelfont(1);
            selfTestEndButton_->labelsize(18);
            selfTestEndButton_->labelcolor((Fl_Color)FL_BLUE);
            selfTestEndButton_->callback(&PsiSeerGui::Kill, this);
            selfTestEndButton_->hide();
        /*selfTestEndButton2_ = */new Fl_Button(30, 30, 0, 0, "");
        sessionPlot_ = new SessionPlot();
        helpWindow_ = new Fl_Double_Window(470, 520, PSISEER_APPNAME " Help " PSISEER_VERSION);
            Fl_Help_View* helpView = new Fl_Help_View(15, 15, 440, 460);
            helpView->link(&PsiSeerGui::LoadHelpUrl);
            helpView->load("Help/index.htm");
            helpView->takesevents();
            helpView->end();
            Fl_Button* helpHideButton = new Fl_Button(330, 485, 89, 25, "Close");
                helpHideButton->callback(&PsiSeerGui::HideHelp, this);
        helpWindow_->end();
        qStartWindow_ = new Fl_Double_Window(470, 520, PSISEER_APPNAME " Quick Start " PSISEER_VERSION);
            Fl_Help_View* qStartView = new Fl_Help_View(15, 15, 440, 460);
            qStartView->link(&PsiSeerGui::LoadHelpUrl);
            qStartView->load("Help/qstart.htm");
            qStartView->takesevents();
            qStartView->end();
            Fl_Button* qStartHideButton = new Fl_Button(330, 485, 89, 25, "Close");
                qStartHideButton->callback(&PsiSeerGui::HideQStart, this);
        qStartWindow_->end();
        mainWindow_->callback(&PsiSeerGui::Closer);
        mainWindow_->user_data((void*)this);
    mainWindow_->end();
    mainWindow_->show();
    helpWindow_->hide();

    // populate RNG Source menu
//    char** sources;
//    int sourceCount = PRD_GetAvailableSources(&sources);
/*    int sourceCount = 0;
    Fl_Menu_Item* menuItems = (Fl_Menu_Item*)menuBar_->menu();
    if (sourceCount == 1)
        menuItems[13].hide();
    else
    {
		haveHardware_ = true;
		menuItems[13].setonly();
//        menuItems[10].clear();
//        menuItems[11].set();
    }*/

    menuBar_->deactivate();

    VersionTest();

//    char homePath[MAX_PATH+1];
//    char theDir[1200];
//    SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, homePath);
//    sprintf(theDir, "%s\\METrainer Data\\", homePath);
//    if (CreateDirectory(theDir, NULL) != 0)
//        qStartWindow_->show();
}


void PsiSeerGui::VersionTest()
{
    FILE* pFile = fopen(PSISEER_VERSION ".ver", "r");
    if (pFile == 0)
    {
        qStartWindow_->show();
        pFile = fopen(PSISEER_VERSION ".ver", "w+");
    }
    fclose(pFile);
}


PsiSeerGui::~PsiSeerGui()
{
    if (helpWindow_ != 0)
        HideHelp(0, this);
    if (mainWindow_ != 0)
    {
        delete mainWindow_;
        mainWindow_ = 0;
    }
}

//bool PsiSeerGui::IsInverted()
//{
//    bool inverted = (invertButton_->value() == 1)? true : false;
//    invertButton_->value(1);

//    return inverted;
//}

void PsiSeerGui::Closer(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = (PsiSeerGui*)object;
    if (obj->helpWindow_ != 0)
        delete obj->helpWindow_;
    if (obj->qStartWindow_ != 0)
        delete obj->qStartWindow_;
    delete obj->sessionPlot_;
    exit(0);
}


void PsiSeerGui::ShowBusy()
{
    Fl::lock();
    hitBox_->hide();
    missBox_->hide();
    Fl::awake();
    Fl::unlock();
}


void PsiSeerGui::ShowHit()
{
    if (isSelfTesting_)
        return;
    Fl::lock();
    rotationFilter_.Feed(1, PSISEER_IMPULSE_LEN);
    if (isTargetInverted_)
        missBox_->show();
    else
        hitBox_->show();
    Fl::awake();
    Fl::unlock();
}


void PsiSeerGui::ShowMiss()
{
    if (isSelfTesting_)
        return;
    Fl::lock();
    rotationFilter_.Feed(0, PSISEER_IMPULSE_LEN);
    if (isTargetInverted_)
        hitBox_->show();
    else
        missBox_->show();
    Fl::awake();
    Fl::unlock();
}


void PsiSeerGui::SetTrialStats(double total, double hitRate, double pValue)
{
    char tStr[8], hStr[8], pStr[9];

    Fl::lock();
        sprintf(tStr, "%1.0f", total);
        sprintf(hStr, "%1.4f", hitRate);
        sprintf(pStr, "%1.5f", pValue);

        trialN_->value(tStr);
        trialHr_->value(hStr);
        trialPHr_->value(pStr);
        Fl::awake();
    Fl::unlock();
}


void PsiSeerGui::SetRate(double newRate, double length)
{
    double ppp = newRate * 2. - 1.;
    rate___ = ppp;
}


void PsiSeerGui::SetWinStats(double hitRate, double pValue)
{
    char hStr[8], pStr[9];

    Fl::lock();
        sprintf(hStr, "%1.4f", hitRate);
        sprintf(pStr, "%1.5f", pValue);
        windowHr_->value(hStr);
        windowPHr_->value(pStr);
        
        windowP_ = hitRate;
        Fl::awake();
    Fl::unlock();
}


static double TimeNowUtc_()
{
    FILETIME fileTime;
    unsigned __int64 uint64Time;

    GetSystemTimeAsFileTime(&fileTime);

    uint64Time = fileTime.dwHighDateTime;
    uint64Time <<= 32;
    uint64Time += fileTime.dwLowDateTime;

    return ((double)((__int64)(uint64Time/10000))/1000. - 9435484800.000); // seconds from 1601 to 1900    
}


// Command button pushed
void PsiSeerGui::Command(Fl_Widget* widget, void* object)
{
    static unsigned long prevAniThreadRunValidator = 101;

    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);
    if (widget == obj->commandLayer_)
    {
        static double lastClock = 0;
        double clock = TimeNowUtc_();
        double clockDiff = clock-lastClock;
        lastClock = clock;

        PulseEvent(obj->psiSeerApp_->commandPulse_);
//        fl_create_thread(obj->commandThread_, &PsiSeerApp::CommandEvent, obj->psiSeerApp_);
        Fl::lock();
            obj->commandLayer_->take_focus();
            if (prevAniThreadRunValidator == obj->aniThreadRunValidator_)
            {
                if (clockDiff < 0.150)
                {
                    Fl::unlock();
                    return;
                }

                fl_kill_thread(obj->animationThread_);
                fl_create_thread(obj->animationThread_, &PsiSeerGui::AnimationThread, obj);
            }
            prevAniThreadRunValidator = obj->aniThreadRunValidator_;
            if (obj->isAboutShown_)
                obj->ToggleAbout(0, obj);
        Fl::unlock();
    }
//        psgInstance->psiSeerApp_->CommandEvent();
}


void PsiSeerGui::SelfTest(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    if (obj->psiSeerApp_->logging_ == false)
        obj->psiSeerApp_->logging_ = true;

    obj->isSelfTesting_ = true;
    obj->userIdBox_->value("autotest");
//    Authenticate(0, obj);
    obj->commandLayer_->deactivate();
    obj->menuBar_->deactivate();
    obj->psiSeerApp_->ResetEvent();
//    if (!obj->areStatsVisible_)
//        obj->ToggleStatsView(0, obj);
    obj->isMuted_ = true;
//    obj->psiBox_->hide();
//    obj->selfTestEndButton_->show();
    obj->psiValSlider_->hide();
    obj->psiValMax_->hide();
    obj->valSliderLabel_->hide();
    fl_create_thread(obj->selfTestThread_, &PsiSeerGui::SelfTestThread, obj);
}

void PsiSeerGui::SelfTestNoLog(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    if (obj->psiSeerApp_->logging_ == true)
        obj->psiSeerApp_->logging_ = false;

    obj->isSelfTesting_ = true;
    obj->userIdBox_->value("autotest");
//    Authenticate(0, obj);
    obj->commandLayer_->deactivate();
    obj->menuBar_->deactivate();
    obj->psiSeerApp_->ResetEvent();
//    if (!obj->areStatsVisible_)
//        obj->ToggleStatsView(0, obj);
    obj->isMuted_ = true;
//    obj->psiBox_->hide();
//    obj->selfTestEndButton_->show();
    obj->psiValSlider_->hide();
    obj->psiValMax_->hide();
    obj->valSliderLabel_->hide();
    fl_create_thread(obj->selfTestThread_, &PsiSeerGui::SelfTestThread, obj);
}

void PsiSeerGui::OnNewCommand(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = (PsiSeerGui*)object;
	obj->cmdBox_->textcolor(FL_BLACK);
}

void PsiSeerGui::OnSendCommand(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = (PsiSeerGui*)object;
	const char* theStr = obj->cmdBox_->value();
	char command = (char)strtol(theStr, NULL, 16);
//	std::stringstream convert;
//	char outCommand;
//	convert >> std::hex >> outCommand;
//	convert.clear();
//	convert << (int)command;
//	obj->cmdBox_->value(convert.str().c_str());
	if (PrdWrite(prdHandle, 1, &command) == 1) {
		obj->cmdBox_->textcolor(FL_BLUE);
		if (command==0x33 || command==0x36)
			obj->psiSeerApp_->isAc_ = true;
		else
			obj->psiSeerApp_->isAc_ = false;
	}
}


const char* PsiSeerGui::LoadHelpUrl(Fl_Widget* widget, const char* url)
{
//    Fl_Help_View* helpView = (Fl_Help_View*)widget;

//    helpView->load(url);

    return url;
}


void* PsiSeerGui::SelfTestThread(void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);
    while(1)
    {
        SetEvent(obj->psiSeerApp_->commandPulse_);
//        Command(obj->commandLayer_, obj);
        Sleep(10);
    }
    return 0;
}


void PsiSeerGui::Authenticate(Fl_Widget* widget, void* instance)
{
    PsiSeerGui* psgInstance = static_cast<PsiSeerGui*>(instance);
//    if (widget == psgInstance->loginButton_ )
//    {
        const char* userId = psgInstance->userIdBox_->value();
        prdHandle = PrdOpen(LICENSE_ID_);
        char sourcee[17];
        PrdGetSource(prdHandle, 17, sourcee);
		Fl_Menu_Item* menuItems = (Fl_Menu_Item*)psgInstance->menuBar_->menu();
        if ((sourcee[2] == 'R') && (sourcee[3] == '4')) {
            psgInstance->haveHardware_ = true;
            menuItems[14].hide();
            psgInstance->checkBrw_->set();
        }
        else if ((sourcee[2] != 'Q') && (sourcee[2] != 'N')) {
            psgInstance->haveHardware_ = true;
			menuItems[13].setonly();
		}
		else {
			menuItems[13].hide();
			menuItems[14].hide();
		}
		psgInstance->SetSource(sourcee);
        psgInstance->menuBar_->activate();

        // override settings
        psgInstance->checkSocketsApi_->clear();

/*        for (int i=0; i<strlen(userId); ++i)
        {
            if ((userGlobal_[i]+3) != userId[i])
//                return;
                continue;
        }
        const char* passCode = psgInstance->passcodeBox_->value();
        for (i=0; i<strlen(passCode); ++i)
        {
            if ((passGlobal_[i]+3) != passCode[i])
//                return;
                  continue;
        }
*/
        char source[22];
        strcpy(source, &psgInstance->sourceIndicator_->label()[8]);
//        char* source = "hi";
//        if (PsiLogger_Open((char*)userId, PSISEER_APPNAME, PSISEER_VERSION, source) == -1)
//            return;

        //psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, sourcee, "CombZ -> MV5");
//        PsiLogger_Log(PsiLogger_CreateS3Header());

    
        if (psgInstance->checkSocketsApi_->value() == 1) {
	        sockaddr_in dest;
	        sockaddr_in local;
	        WSAData data;
	        WSAStartup( MAKEWORD( 2, 2 ), &data );

	        local.sin_family = AF_INET;
	        local.sin_addr.s_addr = INADDR_ANY;
	        local.sin_port = 0; // choose any

	        dest.sin_family = AF_INET;
	        dest.sin_addr.s_addr = inet_addr("127.0.0.1");
	        dest.sin_port = htons(4903);

	        // create the socket
	        psgInstance->psiSeerApp_->outSock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	        // bind to the local address
            //	bind(outSock_, (sockaddr*)&local, sizeof(local));
	        connect(psgInstance->psiSeerApp_->outSock_, (sockaddr*)&dest, sizeof(dest));
            psgInstance->psiSeerApp_->useSocketApi_ = true;
        }

		// If Bounded Random Walk Processing Selected
        if (psgInstance->checkBrw_->value() == 1) 
		{
            psgInstance->psiSeerApp_->usingBrw_ = true;
			psgInstance->procTyp_ = "Brw"; // (NEW)

			// Set raw Buffer Block Size
			if(psgInstance->haveHardware_ == true)	// (NEW)
				psgInstance->psiSeerApp_->rawBufferSize_ = psgInstance->BufferSizeFromSerial(sourcee);	// Hardware (bitRate = 250k or 255k x -N devices)
			else
			{
				if ((sourcee[2] == 'Q'))	
					psgInstance->psiSeerApp_->rawBufferSize_ = 480; // PCQNG (bitRate = 252k)
				else
					psgInstance->psiSeerApp_->rawBufferSize_ = 476; // PRNG (bitRate = 250k)
			}
        }
		else
			psgInstance->procTyp_ = "CombZ -> MV5"; // (NEW)

		unsigned int bound = psgInstance->BoundedThresholdFromSerial(sourcee);	// (NEW)
		psgInstance->psiSeerApp_->brw_ = new BoundedRandomWalk(bound);	// (NEW)

		psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, sourcee, psgInstance->procTyp_); // (NEW)

        Fl::lock();
            psgInstance->loginDialog_->hide();
//            psgInstance->resetStats_->activate();
//            psgInstance->helpShowButton_->activate();
            psgInstance->commandLayer_->activate();
            psgInstance->commandLayer_->take_focus();

            if (psgInstance->animationThread_ == 0)
                fl_create_thread(psgInstance->animationThread_, &PsiSeerGui::AnimationThread, psgInstance);
//            rate___ = 4;
//            timeSetEvent(10, 0, ShowNextImage_CB, 0, TIME_PERIODIC );
            
//            rate___ = -.05;
//            Fl::add_timeout(rate___, ShowNextImage_CB);
        Fl::unlock();
 //   }
}


double sliderMax_ = 1.;
void PsiSeerGui::SetSlider(double sliderVal)
{
    if (sliderVal < 1.0)
        sliderVal = 1.0;
    if (sliderMax_ < sliderVal)
        sliderMax_ = sliderVal;

    Fl::lock();
        if (!isSelfTesting_)
            psiValSlider_->value(sliderVal);
        psiValMax_->position(500, (int)(394.75-(sliderMax_*14.75)));
        static char labelStr[10];
        sprintf(labelStr, "%1.1f", sliderMax_);
        psiValMax_->label(labelStr);
        mainWindow_->redraw();
        Fl::awake();
    Fl::unlock();
}



void* PsiSeerGui::SetPsiModePK(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    if (strcmp(obj->modeIndicator_->label(), "Affect"))
    {
        sliderMax_ = 1;
        obj->psiSeerApp_->SetPsiModeEvent(1);
        SetPsiMode(prdHandle, 1);
    }

    Fl::lock();
    obj->modeIndicator_->label("Affect");
    Fl::unlock();

    return 0;
}

void* PsiSeerGui::SetPsiModeCV(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    if (strcmp(obj->modeIndicator_->label(), "Reveal"))
    {
        sliderMax_ = 1;
        obj->psiSeerApp_->SetPsiModeEvent(2);
        SetPsiMode(prdHandle, 2);
    }

    Fl::lock();
    obj->modeIndicator_->label("Reveal");
    Fl::unlock();

    return 0;
}


void* PsiSeerGui::SetPsiModePC(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    if (strcmp(obj->modeIndicator_->label(), "Predict"))
    {
        sliderMax_ = 1;
        obj->psiSeerApp_->SetPsiModeEvent(3);
        SetPsiMode(prdHandle, 3);
    }

    Fl::lock();
        obj->modeIndicator_->label("Predict");
    Fl::unlock();

    return 0;
}


void* PsiSeerGui::SourceMenu(Fl_Widget* widget, void* object)
{
     // populate menu
//    char** sources;
//    int sourceCount = PRD_GetAvailableSources(&sources);

     return 0;
}


void* PsiSeerGui::SetSourceAuto(Fl_Widget* widget, void* object)
{
    return 0;
}


void* PsiSeerGui::SetSourcePcqn(Fl_Widget* widget, void* instance)
{
//    PRD_SetSource("PCQNG   ");

    PsiSeerGui* psgInstance = static_cast<PsiSeerGui*>(instance);
	psgInstance->psiSeerApp_->isAc_ = false;
    psgInstance->SetSource("PCQNG");

//	ResetStats(widget, instance);

	const char* userId = psgInstance->userIdBox_->value();
	delete psgInstance->psiSeerApp_->logger_;
    //psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, "PCQNG", "CombZ -> MV5");
	psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, "PCQNG", psgInstance->procTyp_); // (NEW)

	if (psgInstance->checkBrw_->value() == 1) 
		psgInstance->psiSeerApp_->rawBufferSize_ = psgInstance->BufferSizeFromSerial("PCQNG");	// (NEW)

    return 0;
}


void* PsiSeerGui::SetSourceHdwr(Fl_Widget* widget, void* instance)
{

	PsiSeerGui* psgInstance = static_cast<PsiSeerGui*>(instance);
	const char* userId = psgInstance->userIdBox_->value();

	PrdClose(prdHandle);
    prdHandle = PrdOpen(LICENSE_ID_);

    char sourcee[22];
    PrdGetSource(prdHandle, 17, sourcee);
	psgInstance->psiSeerApp_->isAc_ = false;
    psgInstance->SetSource(sourcee);

//	ResetStats(widget, instance);

	delete psgInstance->psiSeerApp_->logger_;
    //psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, sourcee, "CombZ -> MV5");

	psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, sourcee, psgInstance->procTyp_); // (NEW)

	char command = 0x66;
	if (psgInstance->checkBrw_->value() == 1) { 
		PrdWrite(prdHandle, 1, &command);	
		psgInstance->psiSeerApp_->rawBufferSize_ = psgInstance->BufferSizeFromSerial(sourcee); // (NEW)
	}

	return 0;
}

void* PsiSeerGui::SetSourceHdwrAc(Fl_Widget* widget, void* instance)
{

	PsiSeerGui* psgInstance = static_cast<PsiSeerGui*>(instance);
	const char* userId = psgInstance->userIdBox_->value();

	PrdClose(prdHandle);
    prdHandle = PrdOpen(LICENSE_ID_);

    char sourcee[22];
    PrdGetSource(prdHandle, 17, sourcee);
	strcat(sourcee, "_AC");
    psgInstance->SetSource(sourcee);

	char command = 0x36;
	if (PrdWrite(prdHandle, 1, &command) == 1)
			psgInstance->psiSeerApp_->isAc_ = true;

//	ResetStats(widget, instance);

	delete psgInstance->psiSeerApp_->logger_;
    //psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, sourcee, "CombZ -> MV5");
	psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, sourcee, psgInstance->procTyp_); // (NEW)

	if (psgInstance->checkBrw_->value() == 1)
		psgInstance->psiSeerApp_->rawBufferSize_ = psgInstance->BufferSizeFromSerial(sourcee);	// (NEW)

	return 0;
}

void* PsiSeerGui::SetSourcePrng(Fl_Widget* widget, void* instance)
{
    PsiSeerGui* psgInstance = static_cast<PsiSeerGui*>(instance);
	psgInstance->psiSeerApp_->isAc_ = false;
    psgInstance->SetSource("PRNG");

//	ResetStats(widget, instance);

	const char* userId = psgInstance->userIdBox_->value();
	delete psgInstance->psiSeerApp_->logger_;
    //psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, "PRNG", "CombZ -> MV5");
	psgInstance->psiSeerApp_->logger_ = new Logger(PSISEER_APPNAME " " PSISEER_VERSION, (char*)userId, "PRNG", psgInstance->procTyp_); // (NEW)

	if (psgInstance->checkBrw_->value() == 1)
		psgInstance->psiSeerApp_->rawBufferSize_ = psgInstance->BufferSizeFromSerial("PRNG");	// (NEW)

    return 0;
}


void PsiSeerGui::SetSource(char* source)
{
    Fl::lock();
    static char lbl[20];
    static char src[5];
    if (memcmp(source, "PCQNG", 4) == 0)
        sourceIndicator_->label("RNG: PCQNG");
    else
    {
        memcpy(src, source, 4);
        src[4] = 0;
        sprintf(lbl, "RNG: %s", src);
        sourceIndicator_->label(lbl);
    }
	psiSeerApp_->SetSource(source);
	sliderMax_ = 1;
	psiSeerApp_->ResetEvent();
    Fl::unlock();
}


void PsiSeerGui::ShowHelp(Fl_Widget* widget, void* instance)
{
    PsiSeerGui* psgInstance = static_cast<PsiSeerGui*>(instance);

    Fl::lock();
    psgInstance->helpWindow_->show();
    Fl::awake();
    Fl::unlock();
}


void PsiSeerGui::HideHelp(Fl_Widget* widget, void* instance)
{
    PsiSeerGui* psgInstance = static_cast<PsiSeerGui*>(instance);

    Fl::lock();
    psgInstance->helpWindow_->hide();
    Fl::awake();
    Fl::unlock();
}


void PsiSeerGui::ShowQStart(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    Fl::lock();
    obj->qStartWindow_->show();
    Fl::awake();
    Fl::unlock();
}


void PsiSeerGui::HideQStart(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    Fl::lock();
    obj->qStartWindow_->hide();
    Fl::awake();
    Fl::unlock();
}


void PsiSeerGui::Kill(Fl_Widget* widget, void* instance)
{
    PsiSeerGui* psgInstance = static_cast<PsiSeerGui*>(instance);

    exit(0);

}


void PsiSeerGui::TogglePracticeMode(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = (PsiSeerGui*)object;

    Fl::lock();
    obj->inPracticeMode_ = !obj->inPracticeMode_;
    if (obj->inPracticeMode_)
        obj->practiceModeIndicator_->show();
    else
        obj->practiceModeIndicator_->hide();
    Fl::unlock();

    obj->psiSeerApp_->PracticeEvent();
}


void PsiSeerGui::ToggleLogging(Fl_Widget* widget, void* instance)
{
    PsiSeerGui* obj = (PsiSeerGui*)instance;

    if (obj->psiSeerApp_->logging_ == true)
        obj->psiSeerApp_->logging_ = false;
    else
        obj->psiSeerApp_->logging_ = true;
}


void PsiSeerGui::ToggleAbout(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = (PsiSeerGui*)object;

    Fl::lock();
    obj->isAboutShown_ = !obj->isAboutShown_;
    if (obj->isAboutShown_)
        obj->aboutDialog_->show();
    else
        obj->aboutDialog_->hide();
    Fl::unlock();
}


bool PsiSeerGui::IsPracticeMode()
{
    return inPracticeMode_;
}


bool PsiSeerGui::IsMuted()
{
    return isMuted_;
}

bool PsiSeerGui::IsMissMuted()
{
    return isMissMuted_;
}

bool PsiSeerGui::IsMissVisible()
{
    return isMissVisible_;
}

bool PsiSeerGui::IsTargetInverted()
{
    return isTargetInverted_;
}


void PsiSeerGui::ToggleStatsView(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    Fl::lock();
    obj->areStatsVisible_ = !obj->areStatsVisible_;
    if (obj->areStatsVisible_)
        obj->trialStats_->show();
    else
        obj->trialStats_->hide();
    Fl::unlock();
}


void PsiSeerGui::TogglePsiLevelView(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    Fl::lock();
    obj->arePsiLevelVisible_ = !obj->arePsiLevelVisible_;
    if (obj->arePsiLevelVisible_)
    {
        obj->psiValSlider_->show();
        obj->psiValMax_->show();
        obj->valSliderLabel_->show();
    }
    else
    {
        obj->psiValSlider_->hide();
        obj->psiValMax_->hide();
        obj->valSliderLabel_->hide();
    }
    Fl::unlock();
}


void PsiSeerGui::ToggleSounds(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    obj->isMuted_ = !obj->isMuted_;
}

void PsiSeerGui::ToggleMissSound(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    obj->isMissMuted_ = !obj->isMissMuted_;
}

void PsiSeerGui::ToggleMissBall(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    obj->isMissVisible_ = !obj->isMissVisible_;
}

void PsiSeerGui::ToggleTargetDirection(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    obj->isTargetInverted_ = !obj->isTargetInverted_;

    if (obj->isTargetInverted_)
    {
        obj->missBox_->image(obj->chevronLargeUp_);
        obj->hitBox_->image(obj->chevronLargeDown_);
        obj->statusIndicator_->label("Target Low");

        netDir = -1;
        netIndex = 1;
//        PRD_SetIntention(0);
    }
    else
    {
        obj->missBox_->image(obj->chevronLargeDown_);
        obj->hitBox_->image(obj->chevronLargeUp_);
        obj->statusIndicator_->label("Target High");

        netDir = 1;
        netIndex = 0;
//        PRD_SetIntention(1);
    }
}


void PsiSeerGui::ResetStats(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    obj->psiSeerApp_->ResetEvent();

    Fl::lock();
        obj->psiValSlider_->value(1.0);
        obj->commandLayer_->take_focus();
        obj->psiValMax_->redraw();
    Fl::unlock();
}


void PsiSeerGui::ShowSessionPlot(Fl_Widget* widget, void* object)
{
    PsiSeerGui* obj = static_cast<PsiSeerGui*>(object);

    obj->sessionPlot_->Show();
}


bool PsiSeerGui::IsShowing()
{
    Fl::lock();
    return (bool)(mainWindow_->visible() != 0);
    Fl::unlock();
}

void* PsiSeerGui::AnimationThread(void* instance)
{
//    PsiSeerGui* obj = (PsiSeerGui*)instance;
    static double x = 0;
    static double lastClock = 0;
    static uchar prevSamplePixel[3];
    static int updateMisses = 0;
//    rate___ = .5;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    PsiSeerGui* obj = (PsiSeerGui*)instance;
    while (1)
    {
        double h = 0;
         
//            This->gui_->SetRate(This->trialStats_.Betai(This->lpFilter_->Feed(1)*40, 40));


            double clock = TimeNowUtc_();
            double clockDiff = clock-lastClock;
            if (clockDiff < 0.03)
            {
//                Fl::unlock();
                Sleep(3);
                continue;
            }
//            if (obj->isSelfTesting_)
//            {
//                Fl::unlock();
//                ++obj->aniThreadRunValidator_;
//                Sleep(1);
 //               continue;
//            }
            Fl::lock();
            double filteredVal = obj->rotationFilter_.Feed(obj->windowP_, 20);
            double filPVal = obj->psiStats_.betai(filteredVal*41, (1-filteredVal)*41 + 1, .5);
            double nFact = 41 * log(41) - 41 + (log(41*(1+4*41*(1+2*41))))/6 + log(3.1416)/2;
            double kFact = filteredVal * log(filteredVal) - filteredVal + (log(filteredVal*(1+4*filteredVal*(1+2*filteredVal))))/6 + log(3.1416)/2;
            double nkFact = (41-filteredVal) * log(41-filteredVal) - (41-filteredVal) + (log((41-filteredVal)*(1+4*(41-filteredVal)*(1+2*(41-filteredVal)))))/6 + log(3.1416)/2;
            double testVal = 0.25 * nFact/(kFact*nkFact);
            rate___ = -(filPVal*2-1);


            if (!(fabs(rate___) <= 1.))
            {
                obj->windowP_ = 21/41;
                obj->rotationFilter_.Init(0.5);
                rate___ = 0.05;
            }

            if (obj->isTargetInverted_)
                rate___ = -rate___;

            if (fabs(rate___) >= .03)
            {
                uchar samplePixel[3];
                fl_read_image(samplePixel, 320, 210, 1, 1);
                if (memcmp(samplePixel, prevSamplePixel, 3) != 0)
                {
                    updateMisses = 0;
                }
                else
                {
                    if (++updateMisses > 20)
                    {
                        obj->mainWindow_->remove(obj->psiBox_);
                        delete obj->psiBox_;
                        obj->psiBox_ = new Fl_Group(247, 355, 150, 150, " ");
                        obj->psiBox_->end();
                        obj->mainWindow_->add(obj->psiBox_);
                        updateMisses = 0;
                    }
                }
                memcpy(prevSamplePixel, samplePixel, 3);
            }
            else
                rate___ = 0;
/*            else
            {
                if (rate___ < 0)
                    rate___ = -.01;
                else
                    rate___ = +.01;
            }
*/
            ++obj->aniThreadRunValidator_;

            lastClock = clock;
            x += clockDiff * -rate___ * 195.;
            x += 1000000*TOTAL;
            x = fmod(x, TOTAL);
            if (x < 0.0001)
                x = 0;
            obj->psiBox_->image(icosphere_pngs[(int)(x)]);
            obj->mainWindow_->redraw();
            Fl::awake();

        Fl::unlock();

        Sleep(1);
    }

    return 0;
}


int PsiSeerGui::BitRateFromSerial(std::string id)
{   
	int bitRate = 0;
	
    // find our bitrate
    if (id.find("QWR1") == 0)
        bitRate = 875000;
    if (id.find("QWR2") == 0)
        bitRate = 875000;
    if (id.find("QWR4") == 0)
        bitRate = 100000;
    if (id.find("QWP4") == 0)
        bitRate = 250000;
    if (id.find("PD")==0 && id.find("QK")==4)
    {
        bitRate = (id[6]-'0') * 100000;
        bitRate += (id[7]-'0') * 10000;
        bitRate += (id[8]-'0') * 1000;
    }
    if (id.find("PD")==0 && id.find("QM")==4)
    {
        bitRate = (id[6]-'0') * 1000000;
        bitRate += (id[7]-'0') * 100000;
        bitRate += (id[8]-'0') * 10000;
    }
	if (id.find("PCQNG") == 0)
		bitRate = 250000;
	if (id.find("PRNG") == 0)
		bitRate = 250000;


    // bitrate if array
    int arraySize = 1;
    if (id.find("x") == 9)
    {
        arraySize = (id.at(10)-48);
        bitRate *= arraySize;
    }
	
	return bitRate;
}

int PsiSeerGui::BufferSizeFromSerial(std::string id) {

	int bufferSize = 0;
	int bitRate = 0;

	bitRate = BitRateFromSerial(id);

	// Target output rate ~ 40ms
    bufferSize = (1435*bitRate)/250000;

	return bufferSize;
}

int PsiSeerGui::BoundedThresholdFromSerial(std::string id) {

	int threshold = 0;
	int bitRate = 0;

	bitRate = BitRateFromSerial(id);

	// Target output rate ~ 40ms

	if( (bitRate % 255000) == 0 ) 
		threshold = (int)sqrt((0.2*255000*((double)bitRate/255000.))/5.);	// bounded threshold = sqrt((200 ms * 255,000 * N)/5), 

	if( (bitRate % 250000) == 0 ) 
		threshold = (int)sqrt((0.2*250000*((double)bitRate/250000))/5.);	

	if( (bitRate % 875000) == 0 ) 
		threshold = (int)sqrt((0.2*875000*((double)bitRate/875000))/5.);

    if ((bitRate % 100000) == 0)
        threshold = (int)sqrt((0.2*100000*((double)bitRate/100000))/5.);

	return threshold;
}
