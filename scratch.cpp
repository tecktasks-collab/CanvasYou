#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <mmsystem.h>
#include "app.h"
#include "init.h"
#include "renderer.h"
#include "manage_screens.h"
#include "gpu_client.h"
#include "boot.h"
#include "store.h"
#include "sqlite3.h"
#include "document.h"
#include "edit_view.h"
#include "templates.h"
#include "history.h"
#include "audio.h"
#include "svg_icon.h"
#include "session_recorder.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#define PROGRAM_SIDE_WIDTH 240
#define PROGRAM_WALL_MIN 80
#define PROGRAM_GAP 8
#define PROGRAM_SPLIT 6
#define PROGRAM_TOP_HEIGHT 72
#define PROGRAM_TITLE_H 34
#define PROGRAM_CAPTION_W 46
#define PROGRAM_TITLE_MENU_MAX 8
#define PROGRAM_MODE_BTN_W 76
#define PROGRAM_MODE_BTN_H 56
#define PROGRAM_MARGIN 8
#define PROGRAM_PANEL_RADIUS 10
#define PROGRAM_TOP_SEP_SPACE 25
#define PROGRAM_VIEW_SHOW 0
#define PROGRAM_VIEW_EDIT 1
#define PROGRAM_SLIDE_MAX 64
#define PROGRAM_SLIDE_RADIUS 8
#define PROGRAM_SLIDE_CHIN 20
#define PROGRAM_PREVIEW_PAD 10
#define PROGRAM_PROJECT_ROW 30
#define PROGRAM_PANEL_HEAD 34
#define PROGRAM_LIST_INSET 6
#define PROGRAM_LIB_TAB_H 28
#define PROGRAM_LIB_MIN 120
#define PROGRAM_PROJECTS_MIN 100
#define LEFT_LIB_MEDIA 0
#define LEFT_LIB_PLAYLISTS 1
#define LEFT_LIB_TRANSITIONS 2
#define LEFT_LIB_MEDIA_MAX 256
#define LEFT_LIB_CARD_W 148
#define LEFT_LIB_CARD_GAP 8
#define LEFT_LIB_PLAYLIST_MAX 64
#define LEFT_LIB_ITEM_MAX 128
#define PROGRAM_AUDIO_ROW 28
#define PROGRAM_MIRROR_H 30
#define PROGRAM_SEG_W 124
#define PROGRAM_SEG_H 32
#define PROGRAM_MIRROR_DROP 240
#define PROGRAM_MEDIA_BTN 30
#define PROGRAM_MEDIA_GAP 10
#define PROGRAM_TRANSPORT_H 30
#define PROGRAM_SCRUB_H 14
#define PROGRAM_SCRUB_HIT_H 24
#define PROGRAM_SCRUB_KNOB_R 11
#define PROGRAM_TRANSPORT_BLOCK_H (PROGRAM_SCRUB_HIT_H + 2 + PROGRAM_TRANSPORT_H)
#define ID_SCREEN_MIRROR 510
#define ID_MEDIA_VIDEO_BTN 511
#define ID_MEDIA_AUDIO_BTN 512
#define ID_MEDIA_VIDEO_PLAY 513
#define ID_MEDIA_VIDEO_SCRUB 514
#define ID_MEDIA_TIMER 515
#define ID_MEDIA_AUDIO_PLAY 516
#define ID_MEDIA_AUDIO_SCRUB 517
#define ID_MEDIA_CLEAR_BG 518
#define ID_MEDIA_CLEAR_SLIDE 519
#define ID_QUICK_SLIDES 520
#define ID_QUICK_SLIDES_ADD 521
#define ID_QUICK_SLIDES_EDIT 522
#define ID_TEMPLATES_BTN 523
#define ID_AUDIO_TRACKS 530
#define ID_AUDIO_PLAYLISTS 531
#define ID_AUDIO_NEW_PLAYLIST 532
#define ID_AUDIO_CLEAR 533
#define ID_AUDIO_AUTOPLAY 540
#define ID_AUDIO_LOOP 541
#define ID_AUDIO_SHUFFLE 542
#define ID_SCENES_BTN 534
#define ID_DISPLAYS_BTN 535
#define ID_MEDIA_VIDEO_BACK 536
#define ID_MEDIA_VIDEO_FWD 537
#define ID_MEDIA_AUDIO_BACK 538
#define ID_MEDIA_AUDIO_FWD 539
#define IDM_CTX_CUT 560
#define IDM_CTX_COPY 561
#define IDM_CTX_PASTE 562
#define IDM_CTX_DELETE 563
#define IDM_CTX_EDIT_SLIDE 564
#define IDM_CTX_USE_AS_SLIDE 565
#define IDM_CTX_USE_AS_BG 566
#define IDM_CTX_TRANSITIONS 567
#define ID_XFADE_TIMER 525
#define ID_TRANS_GENIE_TIMER 526
#define ID_LIVE_SAVE_TIMER 528
#define ID_TRANS_STRESS_TIMER 529
#define ID_SPLASH_TIMER 527
#define XFADE_MS 300
// BG still-plane fades run at this max width so CPU compose + GDI present
// stay near 60fps (full 1080p was ~5fps).
#define BG_XFADE_MAX_W 960
#define TRANS_MS_MIN 100
#define TRANS_MS_MAX 5000
#define TRANS_MS_STEP 100
#define TRANS_GENIE_W 220
#define TRANS_GENIE_HEAD 28
#define TRANS_GENIE_ROW 26
#define TRANS_GENIE_TYPE_ROWS 7
#define TRANS_GENIE_DIR_H 28
#define TRANS_GENIE_H (TRANS_GENIE_HEAD + (TRANS_GENIE_ROW * TRANS_GENIE_TYPE_ROWS) + TRANS_GENIE_DIR_H + TRANS_GENIE_ROW)
#define TRANS_GENIE_STEPS 8
#define TRANS_PIN_W 72
#define TRANS_HIT_APPLY 7
#define TRANS_HIT_DIR0 20
#define TRANS_GENIE_DEFAULT_KIND (-1)
#define IDM_SCRUB_DURATION 520
#define IDM_SCRUB_REMAINING 521
#define MEDIA_PANEL_NONE 0
#define MEDIA_PANEL_VIDEO 1
#define MEDIA_PANEL_AUDIO 2
#define PROGRAM_CLEAR_H 26

#define PROGRAM_COLOR_FRAME RGB(0x34, 0x34, 0x34)
#define PROGRAM_COLOR_WELL RGB(0x2C, 0x2C, 0x2C)
#define PROGRAM_COLOR_WELL_DARK RGB(0x22, 0x22, 0x22)
#define PROGRAM_COLOR_WELL_LIGHT RGB(0x4A, 0x4A, 0x4A)
#define PROGRAM_COLOR_TOP RGB(0x2A, 0x2A, 0x2A)
#define PROGRAM_COLOR_SELECT RGB(0xE8, 0xE8, 0xE8)
#define PROGRAM_COLOR_INK RGB(0xD0, 0xD0, 0xD0)
#define PROGRAM_COLOR_LIVE RGB(0x6E, 0x14, 0x14)
#define PROGRAM_COLOR_LIVE_INK RGB(0xE0, 0x50, 0x50)
#define PROGRAM_COLOR_DANGER_INK RGB(0xE8, 0xA0, 0xA0)
#define PROGRAM_COLOR_LINE RGB(0x3A, 0x3A, 0x3A)
#define PROGRAM_COLOR_CONTROL_EDGE RGB(0x45, 0x45, 0x45)
#define PROGRAM_TILE_W 76
#define PROGRAM_TILE_H 56
#define PROGRAM_SCENES_TILE_W 96
#define PROGRAM_TILE_GAP 4
#define IMPORT_PURPLE RGB(0xA8, 0x6A, 0xC8)
#define IMPORT_MAX 32
#define IMPORT_QUEUED 0
#define IMPORT_COPYING 1
#define IMPORT_COPIED 2
#define IMPORT_DONE 3
#define IMPORT_FAILED 4
#define WM_IMPORT_READY (WM_APP + 41)

#define ID_BUTTON 1
#define ID_SCREENS 2
#define ID_SLIDES 3
#define ID_PROGRAM 4
#define ID_SCREEN_1 201
#define ID_SCREEN_2 202
#define ID_SCREEN_3 203
#define ID_OPEN_ALL 204
#define ID_SCREEN_ITEM_BASE 700
#define ID_SCREEN_ITEM_LAST (ID_SCREEN_ITEM_BASE + BOOT_MAX_SCREENS - 1)
#define IDM_VIRTUAL_NEW 720
#define VIRTUAL_MODE_COUNT 9
#define ID_VIRTUAL_ITEM_BASE 800
#define ID_VIRTUAL_ITEM_LAST (ID_VIRTUAL_ITEM_BASE + 31)
#define ID_VIRTUAL_MODE_BASE 840
#define ID_VIRTUAL_MODE_LAST (ID_VIRTUAL_MODE_BASE + VIRTUAL_MODE_COUNT - 1)
#define VIRTUAL_VIEW_MIN_W 800
#define VIRTUAL_VIEW_MIN_H 480

struct VirtualMode
{
    const char* label;
    int width;
    int height;
    int refreshHz;
};

static const VirtualMode kVirtualModes[VIRTUAL_MODE_COUNT] = {
    { "720p24", 1280, 720, 24 },
    { "720p30", 1280, 720, 30 },
    { "720p50", 1280, 720, 50 },
    { "720p60", 1280, 720, 60 },
    { "1080p30", 1920, 1080, 30 },
    { "1080p60", 1920, 1080, 60 },
    { "2160p25", 3840, 2160, 25 },
    { "2160p30", 3840, 2160, 30 },
    { "2160p60", 3840, 2160, 60 },
};

#define IDM_YOU_LOST 101
#define IDM_THE_GAME 102
#define IDM_HOE 103
#define IDM_FILE_EXIT 1001
#define IDM_MANAGE_SCREENS 1002
#define IDM_SCENES 1009
#define ID_SCENE_NAME_EDIT 2146
#define LOOK_GAP 8
#define LOOK_RAIL_W 230
#define LOOK_HEADER_H 46
#define LOOK_ROW_H 36
#define LOOK_BTN 26
#define LOOK_MAIN_PAD_X 22
#define LOOK_MAIN_PAD_Y 18
#define LOOK_TITLE_H 49
#define LOOK_LABEL_W 104
#define LOOK_CARD_PAD 10
#define LOOK_CARD_GAP 10
#define LOOK_CARD_HEAD 78
#define LOOK_COL_W 124
#define LOOK_COL_GAP 8
#define LOOK_CAP_H 24
#define LOOK_THUMB_H 70
#define LOOK_CELL_GAP 10
#define LOOK_CELL_H 46
#define LOOK_CHECK 18
#define LOOK_SEG_H 22
#define LOOK_SEG_SPAN_W 44
#define LOOK_SEG_MIRROR_W 54
#define LOOK_LEGEND_GAP 16
#define LOOK_LEGEND_H 18
#define LOOK_MAX 16
#define LOOK_NAME_MAX 64
#define LOOK_MAX_SCREENS BOOT_MAX_SCREENS
#define LOOK_MAX_DISPLAYS (BOOT_MAX_SCREENS * BOOT_MAX_SCREENS)
#define LOOK_COLOR_BASE RGB(0x26, 0x26, 0x26)
#define LOOK_COLOR_SURFACE RGB(0x2E, 0x2E, 0x2E)
#define LOOK_COLOR_RAISED RGB(0x38, 0x38, 0x38)
#define LOOK_COLOR_RAISED_HI RGB(0x42, 0x42, 0x42)
#define LOOK_COLOR_SUNKEN RGB(0x1E, 0x1E, 0x1E)
#define LOOK_COLOR_INK RGB(0xD0, 0xD0, 0xD0)
#define LOOK_COLOR_INK_DIM RGB(0x8C, 0x8C, 0x8C)
#define LOOK_COLOR_INK_FAINT RGB(0x5E, 0x5E, 0x5E)
#define LOOK_COLOR_INK_BRIGHT RGB(0xF2, 0xF2, 0xF2)
#define LOOK_COLOR_ACCENT RGB(0xE8, 0xE8, 0xE8)
#define LOOK_COLOR_ACCENT_INK RGB(0x1E, 0x1E, 0x1E)
#define LOOK_COLOR_LIVE_INK RGB(0x7F, 0xD0, 0x7F)
#define IDM_FILE_NEW_PROJECT 1003
#define IDM_FILE_NEW_SLIDE 1004
#define IDM_FILE_SEARCH_IMPORT 1031
#define IDM_FILE_IMPORT 1032
#define SEARCH_SCAN_TIMER 1
#define SEARCH_TITLE_H 40
#define SEARCH_HEAD_H 56
#define SEARCH_FOOT_H 64
#define SEARCH_PAD 18
#define SEARCH_CARD_W 176
#define SEARCH_GAP 12
#define SEARCH_W 980
#define SEARCH_H 680
#define SEARCH_HOT_NONE -1
#define SEARCH_HOT_IMPORT -2
#define SEARCH_HOT_CLOSE -3
#define SEARCH_HOT_CAPTION -4
#define ID_SEARCH_LIST 2210
#define ID_SEARCH_IMPORT 2211
#define ID_SEARCH_CLOSE 2212
#define ID_SEARCH_STATUS 2213
#define SEARCH_IMPORT_MAX 512
#define SEARCH_KNOWN_MAX 2048
#define IDM_EDIT_UNDO 1005
#define IDM_EDIT_REDO 1006
#define IDM_EDIT_CUT 1010
#define IDM_EDIT_COPY 1011
#define IDM_EDIT_PASTE 1012
#define IDM_EDIT_DUPLICATE 1013
#define IDM_EDIT_DELETE 1008
#define IDM_EDIT_PREFERENCES 1007
#define IDM_EDIT_PREFERENCES_BETA 1014
#define IDM_SLIDE_NEW 1020
#define IDM_SLIDE_DUPLICATE 1021
#define IDM_SLIDE_QUICK 1022
#define IDM_SLIDE_SELECT_ALL 1023
#define IDM_SLIDE_NEXT 1024
#define IDM_SLIDE_PREV 1025
#define IDM_SLIDE_FIRST 1026
#define IDM_SLIDE_LAST 1027
#define IDM_SLIDE_USE_SLIDE 1028
#define IDM_SLIDE_USE_BG 1029
#define IDM_SLIDE_EDIT 1030
#define ID_NAME_EDIT 1101
#define ID_NAME_OK 1102
#define ID_NAME_CANCEL 1103
#define ID_PREF_FILES 1201
#define ID_PREF_ASSETS 1202
#define ID_PREF_SCREENS 1203
#define ID_PREF_BROWSE_FILES 1204
#define ID_PREF_BROWSE_ASSETS 1205
#define ID_PREF_BROWSE_SCREENS 1206
#define ID_PREF_OK 1207
#define ID_PREF_CANCEL 1208

WNDPROC g_originalButtonProc = nullptr;
HINSTANCE g_instance = nullptr;
HWND g_mainWindow = nullptr;
HWND g_scenesWindow = nullptr;
HFONT g_sceneFont = nullptr;
HWND g_sceneNameEdit = nullptr;
int g_sceneNameEditing = -1;
WNDPROC g_sceneNameEditProc = nullptr;
int g_lookCount = 0;
char g_lookNames[LOOK_MAX][LOOK_NAME_MAX] = {};
int g_lookIndex = 0;
int g_appliedLook = 0;
int g_lookBgMode[LOOK_MAX][LOOK_MAX_SCREENS] = {};
int g_lookSlideMode[LOOK_MAX][LOOK_MAX_SCREENS] = {};
bool g_lookBgOn[LOOK_MAX][LOOK_MAX_DISPLAYS] = {};
bool g_lookSlideOn[LOOK_MAX][LOOK_MAX_DISPLAYS] = {};
bool g_lookStateReady = false;
struct LookDisplay
{
    int screen;
    int slot;
    bool ghost;
    // A Mirror screen is one column covering every glass; its switches live at
    // the store index of the screen's first glass.
    bool mirror;
    int store;
};
static LookDisplay g_lookDisplays[LOOK_MAX_DISPLAYS] = {};
static int g_lookDisplayCount = 0;
static int g_lookScreenCount = 0;
int g_lookScrollX = 0;
int g_lookScrollY = 0;
HWND g_hostPreview = nullptr;
HWND g_programWindow = nullptr;
HWND g_programTop = nullptr;
HWND g_programLeft = nullptr;
HWND g_programPreview = nullptr;
HWND g_programMirror = nullptr;
int g_programTopSep[2] = { -1, -1 };
// The Program window draws its own title bar, so its menu is not attached with SetMenu.
HMENU g_programMenuBar = nullptr;
int g_audioRailSplitY = 0;
int g_previewScreen = 0;
int g_previewSlot = -1;
HWND g_programVideoBtn = nullptr;
HWND g_programAudioBtn = nullptr;
HWND g_programClearBgBtn = nullptr;
HWND g_programClearSlideBtn = nullptr;
HWND g_programVideoPlay = nullptr;
HWND g_programVideoScrub = nullptr;
HWND g_programVideoTime = nullptr;
HWND g_programAudioPlay = nullptr;
HWND g_programAudioScrub = nullptr;
HWND g_programAudioLabel = nullptr;
HWND g_programVideoBack = nullptr;
HWND g_programVideoFwd = nullptr;
HWND g_programVideoDuration = nullptr;
HWND g_programAudioBack = nullptr;
HWND g_programAudioFwd = nullptr;
HWND g_programAudioDuration = nullptr;
HWND g_programMiddle = nullptr;
HWND g_programRight = nullptr;
HWND g_audioTracks = nullptr;
HWND g_audioPlaylists = nullptr;
HWND g_audioNewPlaylist = nullptr;
HWND g_audioLoopBtn = nullptr;
HWND g_audioShuffleBtn = nullptr;
HWND g_programClearAudioBtn = nullptr;
HWND g_programQuickSlidesBtn = nullptr;
HWND g_quickSlidesWindow = nullptr;
HWND g_quickSlidesEdit = nullptr;
HWND g_programShowButton = nullptr;
HWND g_programEditButton = nullptr;
HWND g_importButton = nullptr;
HWND g_programTemplatesBtn = nullptr;
HWND g_programScenesBtn = nullptr;
bool g_scenesMenuOpen = false;
HWND g_programDisplaysBtn = nullptr;
HWND g_templatePanel = nullptr;
HWND g_importWindow = nullptr;
HWND g_programModeHover = nullptr;
HWND g_mediaChromeHover = nullptr;
HWND g_programButton = nullptr;
HWND g_prefsWindow = nullptr;
HWND g_nameDialog = nullptr;
HFONT g_programFont = nullptr;
char g_nameResult[128] = {};
int g_programView = PROGRAM_VIEW_SHOW;
int g_programLeftWidth = 240;
int g_programRightWidth = 240;
int g_audioSplitDrag = 0;
int g_audioSplitStartY = 0;
int g_audioSplitStartTop = 0;
bool g_audioListsRefreshing = false;
int g_programSplit = 0;
int g_programSplitStartX = 0;
int g_programSplitStartWidth = 0;
int g_programSlideScroll = 0;
int g_programSlideHeld = -1;
int g_programSlideDrop = -1;
int g_programSlideDownX = 0;
int g_programSlideDownY = 0;
bool g_programSlideDragging = false;
bool g_slideSelected[PROGRAM_SLIDE_MAX] = {};
int g_slideSelectAnchor = -1;
bool g_slideBoxSelecting = false;
POINT g_slideBoxStart = {};
POINT g_slideBoxEnd = {};
bool g_slidePointerCtrl = false;
bool g_slidePointerShift = false;
AudioTrack g_trackClipboard[AUDIO_TRACK_MAX] = {};
int g_trackClipboardCount = 0;
bool g_trackClipboardCut = false;
bool g_slideClipboardCut = false;
enum ContextTarget
{
    CTX_NONE = 0,
    CTX_TRACKS = 1,
    CTX_SLIDES = 2
};
static int g_contextTarget = CTX_NONE;
StoreProject g_programProjects[STORE_PROJECT_MAX] = {};
int g_programProjectCount = 0;
int g_programProjectHeld = -1;
int g_programProjectDrop = -1;
int g_programProjectDownY = 0;
bool g_programProjectDragging = false;
int g_leftLibHeight = 220;
int g_leftLibTab = LEFT_LIB_MEDIA;
int g_leftLibSplitDrag = 0;
int g_leftLibSplitStartY = 0;
int g_leftLibSplitStartH = 0;
struct LeftLibMediaItem
{
    char name[128];
    char path[MAX_PATH];
    char stored[MAX_PATH];
    bool video;
    HBITMAP thumb;
    int thumbW;
    int thumbH;
    HBITMAP scrub;
    int scrubW;
    int scrubH;
    float scrubRatio;
};
LeftLibMediaItem g_leftLibMedia[LEFT_LIB_MEDIA_MAX] = {};
int g_leftLibMediaCount = 0;
int g_leftLibMediaSel = -1;
int g_leftLibMediaScroll = 0;
int g_leftLibMediaHover = -1;
float g_leftLibMediaHoverRatio = 0.0f;
int g_leftLibMediaHeld = -1;
int g_leftLibMediaDownX = 0;
int g_leftLibMediaDownY = 0;
bool g_leftLibMediaDragging = false;
RECT g_leftLibMediaBody = {};
struct LeftLibPlaylist
{
    int id;
    char name[128];
};
LeftLibPlaylist g_leftLibPlaylists[LEFT_LIB_PLAYLIST_MAX] = {};
int g_leftLibPlaylistCount = 0;
int g_leftLibPlaylistSel = -1;
struct LeftLibPlaylistItem
{
    char name[128];
    char path[MAX_PATH];
};
LeftLibPlaylistItem g_leftLibItems[LEFT_LIB_ITEM_MAX] = {};
int g_leftLibItemCount = 0;
HWND g_screenWindows[BOOT_MAX_SCREENS] = {};
bool g_liveArmed = false;
bool g_liveHold = false;
int g_mediaPanel = MEDIA_PANEL_VIDEO;
bool g_showRemaining = false;
bool g_videoScrubDragging = false;
bool g_audioScrubDragging = false;
double g_videoScrubRatio = 0.0;
double g_audioScrubRatio = 0.0;
double g_audioVolume = 1.0;
DWORD g_lastVideoSeekMs = 0;
DWORD g_lastAudioSeekMs = 0;
bool g_videoBackgroundCleared = false;
static bool g_backgroundActive = false;
static bool g_liveBgHeld = false;
static HBITMAP g_liveBgFrame = nullptr;
static int g_liveBgW = 0;
static int g_liveBgH = 0;
static wchar_t g_liveBgVideo[MAX_PATH] = {};
static HBITMAP g_liveBgStill = nullptr;
static int g_liveBgStillW = 0;
static int g_liveBgStillH = 0;
static HBITMAP g_holdOverlay = nullptr;
static int g_holdOverlayW = 0;
static int g_holdOverlayH = 0;
// Background already stretched onto the live canvas, built once per background
// and reused by every pane on every frame.
static HBITMAP g_holdCanvas = nullptr;
static int g_holdCanvasW = 0;
static int g_holdCanvasH = 0;
static HBITMAP g_holdCanvasSource = nullptr;
// One finished frame per output window. Panes are composed here in memory and
// the whole picture is copied to the window in a single step.
struct LiveComposeBuffer
{
    HWND hwnd;
    HBITMAP dib;
    int width;
    int height;
};
static LiveComposeBuffer g_liveComposeBuffers[GPU_MAX_TARGETS] = {};
static bool g_xfadeOn = false;
static HBITMAP g_xfadeFrame = nullptr;
static unsigned char* g_xfadeFromBits = nullptr;
static unsigned char* g_xfadeToBits = nullptr;
static unsigned char* g_xfadeOutBits = nullptr;
static int g_xfadeBitsCap = 0;
static int g_xfadeFromW = 0;
static int g_xfadeFromH = 0;
static int g_xfadeToW = 0;
static int g_xfadeToH = 0;
static int g_xfadeFrameW = 0;
static int g_xfadeFrameH = 0;
static int g_xfadeTo = -1;
static int g_xfadeKind = SLIDE_TRANSITION_CROSSFADE;
static int g_xfadeDir = SLIDE_TRANSITION_DIR_LEFT;
static int g_xfadeMs = XFADE_MS;
static DWORD g_xfadeStart = 0;
static bool g_xfadeClockArmed = false;
static HWND g_transGenie = nullptr;
static int g_transAnchorSlide = -1;
static RECT g_transAnchorCard = {};
static bool g_transOpenUp = false;
static int g_transStep = 0;
static int g_transHover = -1;
static bool g_transDirMenuOpen = false;
static int g_transCurrent = SLIDE_TRANSITION_NONE;
static int g_transDir = SLIDE_TRANSITION_DIR_LEFT;
static int g_transMs = 300;
static bool g_transRemembered = false;
#define TRANS_DEFAULT_NONE 0
#define TRANS_DEFAULT_SLIDE 1
#define TRANS_DEFAULT_BG 2
static int g_transDefaultEdit = TRANS_DEFAULT_NONE;
static HWND g_transDefaultPanel = nullptr;
static RECT g_transFootSlide = {};
static RECT g_transFootBg = {};
static int g_transDefaultHover = -1;
static int g_transDefaultPressed = -1;
static int g_transDefaultScroll = 0;
static DWORD g_transDefaultDismissTick = 0;
static bool g_transDefaultClosing = false;
static bool g_transDefaultDirMenuOpen = false;
static int g_transLibScroll = 0;
static RECT g_transLibBody = {};
#define TRANS_GRID_MAX 6
#define TRANS_DEFAULT_PANEL_HEAD 28
#define TRANS_DEFAULT_PANEL_FOOT 36
#define TRANS_DEFAULT_PANEL_PAD 8
#define TRANS_DEFAULT_PANEL_COLS 2
#define TRANS_DEFAULT_PANEL_GAP 6
#define TRANS_DEFAULT_PANEL_THUMB_W 120
#define TRANS_DEFAULT_PANEL_THUMB_H 68
#define TRANS_DEFAULT_PANEL_CARD_H (TRANS_DEFAULT_PANEL_THUMB_H + 20)
struct TransGridCell
{
    int kind;
    int dir;
    const char* label;
    HBITMAP thumb;
    int thumbW;
    int thumbH;
    int thumbDir;
    HBITMAP scrub;
    int scrubW;
    int scrubH;
    float scrubRatio;
    int scrubDir;
};
static TransGridCell g_transGrid[TRANS_GRID_MAX] = {};
static bool g_transGridReady = false;
static HBITMAP g_transDemoA = nullptr;
static HBITMAP g_transDemoB = nullptr;
static int g_transDemoW = 0;
static int g_transDemoH = 0;
static int g_transLibHover = -1;
static float g_transLibHoverRatio = 0.0f;
static float g_transDefaultHoverRatio = 0.0f;
static bool g_bgXfadeOn = false;
static unsigned char* g_bgXfadeFromBits = nullptr;
static unsigned char* g_bgXfadeToBits = nullptr;
static unsigned char* g_bgXfadeOutBits = nullptr;
static int g_bgXfadeBitsCap = 0;
static int g_bgXfadeW = 0;
static int g_bgXfadeH = 0;
static int g_bgXfadeKind = SLIDE_TRANSITION_CROSSFADE;
static int g_bgXfadeDir = SLIDE_TRANSITION_DIR_LEFT;
static int g_bgXfadeMs = 300;
static DWORD g_bgXfadeStart = 0;
static bool g_bgXfadeClockArmed = false;
static int g_bgXfadePendingIndex = -1;
static wchar_t g_bgXfadePendingVideo[MAX_PATH] = {};
// Video→video: keep the outgoing clip playing through the fade, then swap at
// Finish. Stopping GPU for a still xfade was the multi-hundred-ms lag.
static bool g_bgXfadeHoldOutgoingVideo = false;

static void EventLog(const char* text)
{
    if (g_xfadeOn)
    {
        return;
    }
    char path[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0)
    {
        return;
    }
    char* slash = strrchr(path, '\\');
    if (slash != nullptr)
    {
        slash[1] = '\0';
    }
    lstrcatA(path, "clear_bg_events.log");

    FILE* file = nullptr;
    if (fopen_s(&file, path, "a") != 0 || file == nullptr)
    {
        return;
    }

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    fprintf(
        file,
        "%02u:%02u:%02u.%03u %s | cleared=%d live=%d hold=%d gpuReady=%d hasVideo=%d\n",
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds,
        text != nullptr ? text : "",
        g_videoBackgroundCleared ? 1 : 0,
        g_liveArmed ? 1 : 0,
        g_liveHold ? 1 : 0,
        GpuClient_Ready() ? 1 : 0,
        Renderer_CurrentSlideHasVideo() ? 1 : 0
    );

    GpuPlaybackStatus status = {};
    if (GpuClient_GetStatus(&status))
    {
        fprintf(
            file,
            "    status hasMedia=%d playing=%d ready=%d cur=%.3f dur=%.3f\n",
            status.hasMedia,
            status.playing,
            status.ready,
            status.current,
            status.duration
        );
    }
    else
    {
        fprintf(file, "    status unavailable\n");
    }
    fclose(file);
}

bool GpuOwnsOutputs()
{
    return GpuClient_OwnsOutputs();
}

bool App_IsLive()
{
    return g_liveArmed;
}

bool App_LiveHold()
{
    return g_liveHold;
}

bool App_OutputCleared()
{
    return !g_liveArmed && !g_backgroundActive;
}

static void PaintWindowBlack(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return;
    }

    RECT client = {};
    GetClientRect(hwnd, &client);
    if (client.right <= 0 || client.bottom <= 0)
    {
        return;
    }

    HDC hdc = GetDC(hwnd);
    if (hdc == nullptr)
    {
        return;
    }
    HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &client, fill);
    DeleteObject(fill);
    ReleaseDC(hwnd, hdc);
}

static void PaintLiveScreensBlack()
{
    for (int i = 0; i < BOOT_MAX_SCREENS; ++i)
    {
        PaintWindowBlack(g_screenWindows[i]);
    }

    HWND managed[48] = {};
    const int managedCount = ManageScreens_CollectHwnds(managed, 48);
    for (int i = 0; i < managedCount; ++i)
    {
        PaintWindowBlack(managed[i]);
    }
}

void ClearVideoBackground();

void ClearLiveOutputs()
{
    EventLog("ClearLiveOutputs begin");
    g_liveArmed = false;
    g_liveHold = false;
    g_backgroundActive = false;
    g_videoBackgroundCleared = true;
    GpuClient_SetSlideOverlay(nullptr, 0, 0, false);
    // Boot / open on a clear background — never auto-play the current slide's video.
    ClearVideoBackground();
    EventLog("ClearLiveOutputs end");

    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
}

void SaveProgramWalls()
{
    if (!Store_Ready())
    {
        return;
    }

    Store_SetInt("program.left_width", g_programLeftWidth);
    Store_SetInt("program.right_width", g_programRightWidth);
}

int ClampProgramWall(int width, int otherWidth, int clientW)
{
    int maxWidth = clientW - otherWidth - (PROGRAM_GAP * 2) - (PROGRAM_MARGIN * 2) - PROGRAM_WALL_MIN;
    if (maxWidth < PROGRAM_WALL_MIN)
    {
        maxWidth = PROGRAM_WALL_MIN;
    }
    if (width < PROGRAM_WALL_MIN)
    {
        width = PROGRAM_WALL_MIN;
    }
    if (width > maxWidth)
    {
        width = maxWidth;
    }
    return width;
}

void EnsureProgramFont()
{
    if (g_programFont != nullptr)
    {
        return;
    }
    g_programFont = CreateFontA(
        -14,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI"
    );
}

HFONT ProgramUiFont()
{
    EnsureProgramFont();
    if (g_programFont != nullptr)
    {
        return g_programFont;
    }
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

static HFONT ProgramTileFont()
{
    static HFONT font = nullptr;
    if (font == nullptr)
    {
        font = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
    return font != nullptr ? font : ProgramUiFont();
}

static void SceneRound(HDC hdc, const RECT* rc, float radius, COLORREF fill, COLORREF border = CLR_INVALID, float borderW = 1.0f);
static void SceneMaskCorners(HDC hdc, const RECT* rc, float radius, COLORREF outside);

enum
{
    PROGRAM_FONT_HEAD,
    PROGRAM_FONT_CAPTION,
    PROGRAM_FONT_CAPTION_BOLD,
    PROGRAM_FONT_BADGE,
    PROGRAM_FONT_PILL,
    PROGRAM_FONT_TIME,
    PROGRAM_FONT_COUNT
};

static HFONT ProgramFont(int which)
{
    static HFONT fonts[PROGRAM_FONT_COUNT] = {};
    if (which < 0 || which >= PROGRAM_FONT_COUNT)
    {
        return ProgramUiFont();
    }
    if (fonts[which] == nullptr)
    {
        static const int heights[PROGRAM_FONT_COUNT] = { 11, 12, 12, 11, 10, 12 };
        static const int weights[PROGRAM_FONT_COUNT] = {
            FW_NORMAL, FW_NORMAL, FW_SEMIBOLD, FW_NORMAL, FW_BOLD, FW_NORMAL
        };
        fonts[which] = CreateFontA(-heights[which], 0, 0, 0, weights[which], FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
    return fonts[which] != nullptr ? fonts[which] : ProgramUiFont();
}

static void ProgramPanelText(HDC hdc, const char* text, RECT rc, int font, COLORREF color, UINT flags)
{
    HFONT old = (HFONT)SelectObject(hdc, ProgramFont(font));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rc, flags | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static void PaintProgramPanelHead(HDC hdc, const RECT* panel, const char* title, int count)
{
    RECT head = { panel->left + 14, panel->top + 10, panel->right - 12, panel->top + 28 };
    SetTextCharacterExtra(hdc, 1);
    ProgramPanelText(hdc, title, head, PROGRAM_FONT_HEAD, LOOK_COLOR_INK_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextCharacterExtra(hdc, 0);
    if (count < 0)
    {
        return;
    }
    char text[16] = {};
    wsprintfA(text, "%d", count);
    HFONT old = (HFONT)SelectObject(hdc, ProgramFont(PROGRAM_FONT_BADGE));
    SIZE size = {};
    GetTextExtentPoint32A(hdc, text, lstrlenA(text), &size);
    SelectObject(hdc, old);
    const int badgeW = size.cx + 16;
    const int midY = (head.top + head.bottom) / 2;
    RECT badge = { head.right - badgeW, midY - 9, head.right, midY + 9 };
    SceneRound(hdc, &badge, 9.0f, PROGRAM_COLOR_LINE);
    ProgramPanelText(hdc, text, badge, PROGRAM_FONT_BADGE, LOOK_COLOR_INK_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

#define PROGRAM_TILE_IDLE 0
#define PROGRAM_TILE_HOVER 1
#define PROGRAM_TILE_ON 2
#define PROGRAM_TILE_ICON 22
#define PROGRAM_TILE_ICON_Y 20

static void PaintProgramTileBack(HDC hdc, const RECT* client, int state)
{
    HBRUSH back = CreateSolidBrush(LOOK_COLOR_BASE);
    FillRect(hdc, client, back);
    DeleteObject(back);
    if (state != PROGRAM_TILE_IDLE)
    {
        SceneRound(hdc, client, 8.0f, state == PROGRAM_TILE_ON ? LOOK_COLOR_RAISED : RGB(0x30, 0x30, 0x30));
    }
}

static void PaintProgramTileLabel(HDC hdc, const RECT* client, const char* text, COLORREF ink)
{
    RECT label = *client;
    label.top = client->top + PROGRAM_TILE_ICON_Y + 13;
    label.left += 3;
    label.right -= 3;
    HFONT old = (HFONT)SelectObject(hdc, ProgramTileFont());
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ink);
    DrawTextA(hdc, text, -1, &label, DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static void PaintProgramLivePill(HDC hdc, int left, int top)
{
    SetTextCharacterExtra(hdc, 1);
    HFONT old = (HFONT)SelectObject(hdc, ProgramFont(PROGRAM_FONT_PILL));
    SIZE size = {};
    GetTextExtentPoint32A(hdc, "LIVE", 4, &size);
    SelectObject(hdc, old);
    RECT pill = { left, top, left + size.cx + 12, top + 16 };
    SceneRound(hdc, &pill, 4.0f, PROGRAM_COLOR_LIVE_INK);
    RECT text = pill;
    text.left += 1;
    ProgramPanelText(hdc, "LIVE", text, PROGRAM_FONT_PILL, RGB(0xFF, 0xFF, 0xFF), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextCharacterExtra(hdc, 0);
}

void App_OnStart()
{
    g_programLeftWidth = PROGRAM_SIDE_WIDTH;
    g_programRightWidth = PROGRAM_SIDE_WIDTH;

    if (!App_Ready())
    {
        return;
    }

    char documentPath[MAX_PATH] = {};
    BootTicket ticket = {};
    if (Boot_Read(&ticket))
    {
        g_programLeftWidth = ticket.leftWidth;
        g_programRightWidth = ticket.rightWidth;
        Store_OpenAt(ticket.dataRoot);
        Audio_EnsureSchema();
        g_leftLibHeight = Store_GetInt("program.left_lib_height", 220);
        if (g_leftLibHeight < PROGRAM_LIB_MIN)
        {
            g_leftLibHeight = PROGRAM_LIB_MIN;
        }
        if (Store_Ready())
        {
            ManageScreens_LoadFromStore();
        }
        else
        {
            ManageScreens_LoadPending(&ticket);
        }
        lstrcpynA(documentPath, ticket.document, MAX_PATH);
    }
    else
    {
    Store_Open();
        Audio_EnsureSchema();
        g_leftLibHeight = Store_GetInt("program.left_lib_height", 220);
        if (g_leftLibHeight < PROGRAM_LIB_MIN)
        {
            g_leftLibHeight = PROGRAM_LIB_MIN;
        }
    ManageScreens_LoadFromStore();
        Store_GetString("last.document", documentPath, MAX_PATH, "");
    }
    if (Store_Ready() && Store_GetInt("general.reopen_last", 1) == 0)
    {
        documentPath[0] = '\0';
    }

    Document_EnsureFolders();
    Template_RefreshCatalog();

    if (documentPath[0] != '\0' && Document_Open(documentPath))
    {
        g_showRemaining = Store_Ready() && Store_GetInt("playback.show_remaining", 0) != 0;
        return;
    }

    Renderer_NewDocument();
    g_showRemaining = Store_Ready() && Store_GetInt("playback.show_remaining", 0) != 0;
}

void LayoutProgramPreview();
void LayoutHostPreview(HWND parent);
void LayoutAudioRail();
void RefreshAudioLists();
void RefreshSlideLists();
void UpdateProgramMiddleScroll(HWND hwnd);
void ShowProgramView();
static void RefreshLeftLib();
static void ShowSearchToImport(HWND owner);
static void ImportMediaFromExplorer(HWND owner);
static HFONT PrefFont(int px, int weight);
static void PrefText(HDC hdc, const char* text, RECT rc, HFONT font, COLORREF color, UINT flags);
static void PaintPrefButton(HDC hdc, const RECT* rc, const char* text, bool primary, bool hot);
static HFONT ProgramCaptionGlyphFont();
void ShowQuickSlides();
void HideQuickSlides();
void PlaceQuickSlidesWindow();
void SetProgramView(int view);
void SelectSlide(int index);
void RefreshAfterDocumentChange();
void MeasureProgramSlideGrid(int clientW, int count, int* cellW, int* cellH, int* cols, int* rows, int* contentH);
void ClearVideoBackground();
void ClearLiveSlide();
void PresentCurrent();
static void PresentXfadeTick();
static void LoadLookState();
static void SaveLookState();
static void RebuildLookLayout();
static void ApplyLiveLook();
static void ApplyLook(int index);
void ShowScenesWindow();
void LayoutProgramModeButtons();
void UpdateMediaTransportUi();
int ProgramSlideCount();
void LayoutProgramSlideCells(const RECT* client, RECT* cells, int count);
static void DrawMediaVideoIcon(HDC hdc, int cx, int cy, COLORREF color);
static void DrawMediaAudioIcon(HDC hdc, int cx, int cy, COLORREF color);
static void DrawMediaPlayIcon(HDC hdc, int cx, int cy, COLORREF color);
static void DrawMediaPauseIcon(HDC hdc, int cx, int cy, COLORREF color);
static void PlaySelectedAudioTrack();
static bool HandleContextMenuCommand(int id);
void FitProgramSlideCard(const RECT* cell, int sourceW, int sourceH, RECT* thumb, RECT* chin);
static void StopSlideXfade();
static void StartSlideXfade(int fromIndex, int toIndex);
static void TickSlideXfade();
static void TickBackgroundXfade();
static bool SlideXfadeActive();
static void RefreshTransitionTray();
static HBITMAP LeftLibLoadImageThumb(const wchar_t* path, int width, int height);
static HBITMAP LeftLibLoadVideoFrameAt(const wchar_t* path, float ratio, int width, int height);
static bool GrabBitmap32(HBITMAP src, int srcW, int srcH, int destW, int destH, unsigned char* dest);
static HBITMAP SlideXfadeFrame(int* width, int* height);
static const unsigned char* SlideXfadeBits(int* width, int* height);
static void BlitAlphaOverlay(
    HDC dest,
    int destX,
    int destY,
    int destW,
    int destH,
    HBITMAP bmp,
    int srcX,
    int srcY,
    int srcW,
    int srcH
);
static void ShowTransitionGenie(int slide);
static void CloseTransitionGenie();
static void ShowTransDefaultPanel(int target);
static void CloseTransDefaultPanel();
static void ToggleTransDefaultPanel(int target);
static void ToggleTemplatePanel();
static void CloseTemplatePanel();
static LRESULT CALLBACK TransDefaultPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void HighlightSlide(int index);
static void ProgramCut();
static void ProgramCopy();
static void ProgramPaste();
static void ProgramDuplicate();
static void ProgramDelete();
static void SelectAllSlides();
static void HighlightStep(int delta);
static void HighlightByArrow(WPARAM key);
static void GoLiveByArrow(WPARAM key);
static void HighlightFirstLast(bool first);
static void GoLiveFirstLast(bool first);
static void GoLiveStep(int delta);
static void UseSelectedAsSlide();
static void UseSelectedAsBackground();
static void EditSelectedSlide();
static void AddQuickSlidesFromText(HWND owner);
static bool HandleProgramKeys(const MSG* msg);
static void ApplySceneTitleBar(HWND hwnd);

void RefreshProgramScreenMirror()
{
    if (g_programMirror == nullptr)
    {
        return;
    }

    EnsureProgramFont();
    SendMessageA(g_programMirror, CB_RESETCONTENT, 0, 0);
    const int count = ManageScreens_ScreenCount();
    if (count <= 0)
    {
        SendMessageA(g_programMirror, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("No screens configured"));
        SendMessage(g_programMirror, CB_SETCURSEL, 0, 0);
        g_previewScreen = 0;
        g_previewSlot = -1;
        return;
    }

    if (g_previewScreen < 0 || g_previewScreen >= count)
    {
        g_previewScreen = 0;
        g_previewSlot = -1;
    }

    int restore = 0;
    int added = 0;
    char screenName[64] = {};
    char label[160] = {};
    for (int screen = 0; screen < count; ++screen)
    {
        if (!ManageScreens_GetScreenLabel(screen, screenName, sizeof(screenName)))
        {
            wsprintfA(screenName, "Screen %d", screen + 1);
        }
        const int slots = ManageScreens_GetScreenOutputCount(screen);
        const bool mirror = slots > 1 && ManageScreens_GetScreenLayout(screen) == SCREEN_LAYOUT_MIRROR;
        if (slots > 1)
        {
            const int index = (int)SendMessageA(
                g_programMirror,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(screenName)
            );
            SendMessage(g_programMirror, CB_SETITEMDATA, index, MAKELPARAM(screen, 0));
            if (g_previewScreen == screen && (g_previewSlot < 0 || mirror))
            {
                restore = added;
            }
            added += 1;
        }
        if (mirror)
        {
            continue;
        }
        if (slots <= 0)
        {
            const int index = (int)SendMessageA(
                g_programMirror,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(screenName)
            );
            SendMessage(g_programMirror, CB_SETITEMDATA, index, MAKELPARAM(screen, 0));
            if (g_previewScreen == screen)
            {
                restore = added;
            }
            added += 1;
            continue;
        }
        for (int slot = 0; slot < slots; ++slot)
        {
            if (count > 1 || slots > 1)
            {
                wsprintfA(label, "%s - Display %d", screenName, slot + 1);
            }
            else if (!ManageScreens_GetScreenOutputLabel(screen, slot, label, sizeof(label)))
            {
                wsprintfA(label, "Display %d", slot + 1);
            }
            const int index = (int)SendMessageA(
                g_programMirror,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(label)
            );
            SendMessage(g_programMirror, CB_SETITEMDATA, index, MAKELPARAM(screen, slot + 1));
            if (g_previewScreen == screen && g_previewSlot == slot)
            {
                restore = added;
            }
            added += 1;
        }
    }

    if (added <= 0)
    {
        SendMessageA(g_programMirror, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("No screens configured"));
        SendMessage(g_programMirror, CB_SETCURSEL, 0, 0);
        return;
    }
    if (restore < 0 || restore >= added)
    {
        restore = 0;
    }
    SendMessage(g_programMirror, CB_SETCURSEL, restore, 0);
    const LPARAM data = SendMessage(g_programMirror, CB_GETITEMDATA, restore, 0);
    g_previewScreen = LOWORD(data);
    g_previewSlot = HIWORD(data) > 0 ? (int)HIWORD(data) - 1 : -1;
}

static void FormatMediaTime(char* out, int outChars, double seconds)
{
    if (out == nullptr || outChars < 8)
    {
        return;
    }
    if (seconds < 0.0 || seconds != seconds)
    {
        seconds = 0.0;
    }
    int total = static_cast<int>(seconds + 0.5);
    const int mins = total / 60;
    const int secs = total % 60;
    wsprintfA(out, "%d:%02d", mins, secs);
}

static bool IsVideoExtension(const wchar_t* path)
{
    const wchar_t* dot = wcsrchr(path, L'.');
    if (dot == nullptr)
    {
        return false;
    }
    return lstrcmpiW(dot, L".mp4") == 0 ||
        lstrcmpiW(dot, L".mov") == 0 ||
        lstrcmpiW(dot, L".wmv") == 0 ||
        lstrcmpiW(dot, L".avi") == 0 ||
        lstrcmpiW(dot, L".mkv") == 0 ||
        lstrcmpiW(dot, L".m4v") == 0 ||
        lstrcmpiW(dot, L".webm") == 0;
}

static bool IsImageExtension(const wchar_t* path)
{
    const wchar_t* dot = wcsrchr(path, L'.');
    if (dot == nullptr)
    {
        return false;
    }
    return lstrcmpiW(dot, L".png") == 0 ||
        lstrcmpiW(dot, L".jpg") == 0 ||
        lstrcmpiW(dot, L".jpeg") == 0 ||
        lstrcmpiW(dot, L".bmp") == 0 ||
        lstrcmpiW(dot, L".gif") == 0 ||
        lstrcmpiW(dot, L".tif") == 0 ||
        lstrcmpiW(dot, L".tiff") == 0 ||
        lstrcmpiW(dot, L".webp") == 0;
}

struct ImportJob
{
    int id;
    int state;
    bool used;
    bool video;
    bool background;
    LONG64 copied;
    LONG64 total;
    wchar_t source[MAX_PATH];
    wchar_t dest[MAX_PATH];
    wchar_t stored[MAX_PATH];
    wchar_t name[MAX_PATH];
};

static ImportJob g_imports[IMPORT_MAX] = {};
static CRITICAL_SECTION g_importLock;
static bool g_importLockReady = false;
static HANDLE g_importWake = nullptr;
static HANDLE g_importThread = nullptr;
static LONG g_importStop = 0;
static LONG g_importClosing = 0;
static BOOL g_importCancel = FALSE;
static int g_importNextId = 1;
static int g_importScroll = 0;
static bool g_importHover = false;
static int g_importButtonShown = 0;

static LONG64 ImportRead64(LONG64* value)
{
    return InterlockedCompareExchange64(value, 0, 0);
}

static const wchar_t* ImportFileName(const wchar_t* path)
{
    if (path == nullptr || path[0] == L'\0')
    {
        return L"";
    }
    const wchar_t* slash = wcsrchr(path, L'\\');
    const wchar_t* fwd = wcsrchr(path, L'/');
    if (fwd != nullptr && (slash == nullptr || fwd > slash))
    {
        slash = fwd;
    }
    return slash != nullptr ? slash + 1 : path;
}

static int ImportJobPercent(const ImportJob* job)
{
    if (job->state == IMPORT_DONE || job->state == IMPORT_COPIED)
    {
        return 100;
    }
    if (job->state == IMPORT_FAILED)
    {
        return 0;
    }
    const LONG64 total = ImportRead64(const_cast<LONG64*>(&job->total));
    const LONG64 copied = ImportRead64(const_cast<LONG64*>(&job->copied));
    if (total <= 0)
    {
        return 0;
    }
    if (copied >= total)
    {
        return 100;
    }
    return static_cast<int>((copied * 100) / total);
}

static int ImportOverallPercent(int* activeOut)
{
    int active = 0;
    LONG64 copied = 0;
    LONG64 total = 0;
    if (g_importLockReady)
    {
        EnterCriticalSection(&g_importLock);
        for (int i = 0; i < IMPORT_MAX; ++i)
        {
            ImportJob* job = &g_imports[i];
            if (!job->used)
            {
                continue;
            }
            if (job->state == IMPORT_QUEUED || job->state == IMPORT_COPYING || job->state == IMPORT_COPIED)
            {
                active += 1;
            }
            if (job->state == IMPORT_FAILED)
            {
                continue;
            }
            copied += ImportRead64(&job->copied);
            total += ImportRead64(&job->total);
        }
        LeaveCriticalSection(&g_importLock);
    }
    if (activeOut != nullptr)
    {
        *activeOut = active;
    }
    if (total <= 0 || copied <= 0)
    {
        return 0;
    }
    if (copied >= total)
    {
        return 100;
    }
    return static_cast<int>((copied * 100) / total);
}

static void PaintImportCircle(HDC hdc, int cx, int cy, int radius, int percent)
{
    const int left = cx - radius;
    const int top = cy - radius;
    const int right = cx + radius;
    const int bottom = cy + radius;
    HBRUSH hole = CreateSolidBrush(PROGRAM_COLOR_WELL_DARK);
    HPEN ring = CreatePen(PS_SOLID, 2, PROGRAM_COLOR_WELL_LIGHT);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hole);
    HPEN oldPen = (HPEN)SelectObject(hdc, ring);
    Ellipse(hdc, left, top, right, bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hole);
    DeleteObject(ring);

    if (percent <= 0)
    {
        return;
    }
    if (percent > 100)
    {
        percent = 100;
    }

    HBRUSH fill = CreateSolidBrush(IMPORT_PURPLE);
    HPEN fillPen = CreatePen(PS_SOLID, 1, IMPORT_PURPLE);
    oldBrush = (HBRUSH)SelectObject(hdc, fill);
    oldPen = (HPEN)SelectObject(hdc, fillPen);
    const int inset = 4;
    if (percent >= 100)
    {
        Ellipse(hdc, left + inset, top + inset, right - inset, bottom - inset);
    }
    else
    {
        const double sweep = (percent / 100.0) * 6.283185307179586;
        const int rad = radius - inset;
        const int xStart = cx + static_cast<int>(sin(sweep) * rad);
        const int yStart = cy - static_cast<int>(cos(sweep) * rad);
        Pie(hdc, cx - rad, cy - rad, cx + rad, cy + rad, xStart, yStart, cx, cy - rad);
    }
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(fill);
    DeleteObject(fillPen);
}

static DWORD CALLBACK ImportCopyProgress(
    LARGE_INTEGER totalFileSize,
    LARGE_INTEGER totalBytesTransferred,
    LARGE_INTEGER,
    LARGE_INTEGER,
    DWORD,
    DWORD,
    HANDLE,
    HANDLE,
    LPVOID param
)
{
    ImportJob* job = static_cast<ImportJob*>(param);
    if (job != nullptr)
    {
        if (totalFileSize.QuadPart > 0)
        {
            InterlockedExchange64(&job->total, totalFileSize.QuadPart);
        }
        InterlockedExchange64(&job->copied, totalBytesTransferred.QuadPart);
    }
    if (g_importCancel)
    {
        return PROGRESS_CANCEL;
    }
    return PROGRESS_CONTINUE;
}

static DWORD WINAPI ImportThread(LPVOID)
{
    while (InterlockedCompareExchange(&g_importStop, 0, 0) == 0)
    {
        ImportJob* job = nullptr;
        wchar_t source[MAX_PATH] = {};
        wchar_t dest[MAX_PATH] = {};
        int id = 0;
        EnterCriticalSection(&g_importLock);
        for (int i = 0; i < IMPORT_MAX; ++i)
        {
            if (g_imports[i].used && g_imports[i].state == IMPORT_QUEUED)
            {
                g_imports[i].state = IMPORT_COPYING;
                job = &g_imports[i];
                id = job->id;
                lstrcpynW(source, job->source, MAX_PATH);
                lstrcpynW(dest, job->dest, MAX_PATH);
                break;
            }
        }
        LeaveCriticalSection(&g_importLock);

        if (job == nullptr)
        {
            WaitForSingleObject(g_importWake, INFINITE);
            continue;
        }

        BOOL ok = FALSE;
        if (!g_importCancel)
        {
            ok = CopyFileExW(source, dest, ImportCopyProgress, job, &g_importCancel, 0);
        }
        if (!ok)
        {
            DeleteFileW(dest);
        }

        EnterCriticalSection(&g_importLock);
        if (job->used && job->id == id)
        {
            if (ok)
            {
                const LONG64 total = ImportRead64(&job->total);
                InterlockedExchange64(&job->copied, total);
                job->state = IMPORT_COPIED;
            }
            else
            {
                job->state = IMPORT_FAILED;
            }
        }
        LeaveCriticalSection(&g_importLock);

        if (InterlockedCompareExchange(&g_importClosing, 0, 0) == 0 && g_programWindow != nullptr)
        {
            PostMessage(g_programWindow, WM_IMPORT_READY, static_cast<WPARAM>(id), ok ? 1 : 0);
        }
    }
    return 0;
}

static void ImportEnsureWorker()
{
    if (!g_importLockReady)
    {
        InitializeCriticalSection(&g_importLock);
        g_importLockReady = true;
    }
    if (g_importWake == nullptr)
    {
        g_importWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    }
    if (g_importThread == nullptr)
    {
        InterlockedExchange(&g_importStop, 0);
        g_importCancel = FALSE;
        g_importThread = CreateThread(nullptr, 0, ImportThread, nullptr, 0, nullptr);
    }
}

static void ImportUiTick();

static void ImportShutdown()
{
    InterlockedExchange(&g_importClosing, 1);
    g_importCancel = TRUE;
    InterlockedExchange(&g_importStop, 1);
    if (g_importWake != nullptr)
    {
        SetEvent(g_importWake);
    }
    bool threadGone = g_importThread == nullptr;
    if (g_importThread != nullptr)
    {
        if (WaitForSingleObject(g_importThread, 5000) == WAIT_OBJECT_0)
        {
            CloseHandle(g_importThread);
            g_importThread = nullptr;
            threadGone = true;
        }
    }
    if (g_importLockReady)
    {
        EnterCriticalSection(&g_importLock);
        for (int i = 0; i < IMPORT_MAX; ++i)
        {
            if (g_imports[i].used && g_imports[i].state == IMPORT_QUEUED)
            {
                DeleteFileW(g_imports[i].dest);
            }
            g_imports[i].used = false;
        }
        LeaveCriticalSection(&g_importLock);
    }
    if (threadGone)
    {
        g_importCancel = FALSE;
        InterlockedExchange(&g_importStop, 0);
        InterlockedExchange(&g_importClosing, 0);
    }
    g_importButtonShown = 0;
}

static bool FinishDroppedMedia(HWND owner, const wchar_t* stored, bool video, bool background)
{
    const int previous = Renderer_CurrentSlide();
    History_Remember();
    if (background)
    {
        if (!Renderer_AddFullscreenMediaSlide(stored, video, true))
        {
            MessageBoxA(owner, "Could not add that as a background.", "Import", MB_OK | MB_ICONWARNING);
            return false;
        }
    }
    else if (!Renderer_AddFullscreenMediaSlide(stored, video, false))
    {
        MessageBoxA(owner, "Could not create a slide for that media.", "Import", MB_OK | MB_ICONWARNING);
        return false;
    }
    else if (previous >= 0)
    {
        // Slide add leaves current on the new card — keep the prior selection.
        Renderer_SelectSlide(previous);
    }

    if (Document_HasProject())
    {
        Document_Save();
    }
    RefreshSlideLists();
    if (previous >= 0)
    {
        HighlightSlide(previous);
    }
    LayoutProgramPreview();
    return true;
}

static bool QueueDroppedMedia(HWND owner, const wchar_t* path, bool video, bool background)
{
    wchar_t stored[MAX_PATH] = {};
    wchar_t dest[MAX_PATH] = {};
    const int plan = Document_PlanImport(path, stored, MAX_PATH, dest, MAX_PATH);
    if (plan == 0)
    {
        MessageBoxA(owner, "Could not copy that media into the project assets folder.", "Import", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (plan == 2)
    {
        return FinishDroppedMedia(owner, stored, video, background);
    }

    HANDLE created = CreateFileW(
        dest,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (created == INVALID_HANDLE_VALUE)
    {
        MessageBoxA(owner, "Could not copy that media into the project assets folder.", "Import", MB_OK | MB_ICONWARNING);
        return false;
    }
    CloseHandle(created);

    ImportEnsureWorker();
    if (!g_importLockReady || g_importThread == nullptr)
    {
        DeleteFileW(dest);
        MessageBoxA(owner, "Could not copy that media into the project assets folder.", "Import", MB_OK | MB_ICONWARNING);
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA info = {};
    LONG64 total = 0;
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &info))
    {
        total = (static_cast<LONG64>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    }

    bool queued = false;
    EnterCriticalSection(&g_importLock);
    bool busy = false;
    for (int i = 0; i < IMPORT_MAX; ++i)
    {
        if (g_imports[i].used &&
            (g_imports[i].state == IMPORT_QUEUED ||
             g_imports[i].state == IMPORT_COPYING ||
             g_imports[i].state == IMPORT_COPIED))
        {
            busy = true;
            break;
        }
    }
    if (!busy)
    {
        for (int i = 0; i < IMPORT_MAX; ++i)
        {
            g_imports[i].used = false;
        }
        g_importScroll = 0;
    }

    for (int i = 0; i < IMPORT_MAX; ++i)
    {
        if (g_imports[i].used)
        {
            continue;
        }
        ImportJob* job = &g_imports[i];
        ZeroMemory(job, sizeof(*job));
        job->id = g_importNextId++;
        if (g_importNextId < 1)
        {
            g_importNextId = 1;
        }
        job->state = IMPORT_QUEUED;
        job->used = true;
        job->video = video;
        job->background = background;
        InterlockedExchange64(&job->copied, 0);
        InterlockedExchange64(&job->total, total);
        lstrcpynW(job->source, path, MAX_PATH);
        lstrcpynW(job->dest, dest, MAX_PATH);
        lstrcpynW(job->stored, stored, MAX_PATH);
        lstrcpynW(job->name, ImportFileName(path), MAX_PATH);
        queued = true;
        break;
    }
    LeaveCriticalSection(&g_importLock);

    if (!queued)
    {
        DeleteFileW(dest);
        MessageBoxA(owner, "Too many files are already importing.", "Import", MB_OK | MB_ICONWARNING);
        return false;
    }

    SetEvent(g_importWake);
    ImportUiTick();
    return true;
}

static void ImportOnReady(int id, bool ok)
{
    if (InterlockedCompareExchange(&g_importClosing, 0, 0) != 0 || !g_importLockReady)
    {
        return;
    }

    wchar_t stored[MAX_PATH] = {};
    bool video = false;
    bool background = false;
    bool found = false;
    EnterCriticalSection(&g_importLock);
    for (int i = 0; i < IMPORT_MAX; ++i)
    {
        if (!g_imports[i].used || g_imports[i].id != id)
        {
            continue;
        }
        found = true;
        lstrcpynW(stored, g_imports[i].stored, MAX_PATH);
        video = g_imports[i].video;
        background = g_imports[i].background;
        if (!ok)
        {
            g_imports[i].state = IMPORT_FAILED;
        }
        break;
    }
    LeaveCriticalSection(&g_importLock);

    if (!found)
    {
        ImportUiTick();
        return;
    }

    if (ok)
    {
        if (g_programWindow != nullptr)
        {
            FinishDroppedMedia(g_programWindow, stored, video, background);
        }
        EnterCriticalSection(&g_importLock);
        for (int i = 0; i < IMPORT_MAX; ++i)
        {
            if (g_imports[i].used && g_imports[i].id == id)
            {
                g_imports[i].state = IMPORT_DONE;
                break;
            }
        }
        LeaveCriticalSection(&g_importLock);
    }
    else if (g_importWindow == nullptr || !IsWindowVisible(g_importWindow))
    {
        MessageBoxA(
            g_programWindow,
            "Could not copy that media into the project assets folder.",
            "Import",
            MB_OK | MB_ICONWARNING
        );
    }
    ImportUiTick();
}

static void ImportUiTick()
{
    int active = 0;
    if (g_importLockReady)
    {
        EnterCriticalSection(&g_importLock);
        for (int i = 0; i < IMPORT_MAX; ++i)
        {
            if (!g_imports[i].used)
            {
                continue;
            }
            if (g_imports[i].state == IMPORT_QUEUED ||
                g_imports[i].state == IMPORT_COPYING ||
                g_imports[i].state == IMPORT_COPIED)
            {
                active += 1;
            }
        }
        const bool windowOpen = g_importWindow != nullptr && IsWindow(g_importWindow);
        if (active == 0 && !windowOpen)
        {
            for (int i = 0; i < IMPORT_MAX; ++i)
            {
                g_imports[i].used = false;
            }
            g_importScroll = 0;
        }
        LeaveCriticalSection(&g_importLock);
    }

    if (g_importButton != nullptr && IsWindow(g_importButton))
    {
        const bool show = active > 0;
        if (show != (g_importButtonShown != 0))
        {
            ShowWindow(g_importButton, show ? SW_SHOW : SW_HIDE);
            g_importButtonShown = show ? 1 : 0;
            LayoutProgramModeButtons();
        }
        if (show)
        {
            InvalidateRect(g_importButton, nullptr, FALSE);
        }
    }
    else
    {
        g_importButtonShown = 0;
    }

    if (g_importWindow != nullptr && IsWindow(g_importWindow))
    {
        InvalidateRect(g_importWindow, nullptr, FALSE);
    }
}

static void OpenImportWindow();

static LRESULT CALLBACK ImportButtonProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
            if (!g_importHover)
            {
                g_importHover = true;
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_MOUSELEAVE:
            g_importHover = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONUP:
            OpenImportWindow();
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HBRUSH back = CreateSolidBrush(g_importHover ? RGB(0x3A, 0x3A, 0x3A) : PROGRAM_COLOR_TOP);
            FillRect(hdc, &client, back);
            DeleteObject(back);
            const int cx = (client.left + client.right) / 2;
            const int cy = (client.top + client.bottom) / 2;
            PaintImportCircle(hdc, cx, cy, 18, ImportOverallPercent(nullptr));
            EndPaint(hwnd, &paint);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void PaintImportRows(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    HBRUSH back = CreateSolidBrush(PROGRAM_COLOR_FRAME);
    FillRect(hdc, &client, back);
    DeleteObject(back);

    ImportJob rows[IMPORT_MAX] = {};
    int count = 0;
    if (g_importLockReady)
    {
        EnterCriticalSection(&g_importLock);
        for (int i = 0; i < IMPORT_MAX; ++i)
        {
            if (!g_imports[i].used)
            {
                continue;
            }
            rows[count] = g_imports[i];
            rows[count].copied = ImportRead64(&g_imports[i].copied);
            rows[count].total = ImportRead64(&g_imports[i].total);
            count += 1;
        }
        LeaveCriticalSection(&g_importLock);
    }

    const int rowH = 58;
    const int content = count > 0 ? (12 + count * rowH) : client.bottom;
    int maxScroll = content - client.bottom;
    if (maxScroll < 0)
    {
        maxScroll = 0;
    }
    if (g_importScroll > maxScroll)
    {
        g_importScroll = maxScroll;
    }
    if (g_importScroll < 0)
    {
        g_importScroll = 0;
    }

    SCROLLINFO info = {};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = content > 0 ? content : 0;
    info.nPage = static_cast<UINT>(client.bottom > 0 ? client.bottom : 0);
    info.nPos = g_importScroll;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);

    HFONT oldFont = (HFONT)SelectObject(hdc, ProgramUiFont());
    SetBkMode(hdc, TRANSPARENT);
    if (count == 0)
    {
        SetTextColor(hdc, PROGRAM_COLOR_INK);
        DrawTextA(hdc, "Nothing is importing.", -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        const int y = 12 + (i * rowH) - g_importScroll;
        if (y > client.bottom || y + rowH < 0)
        {
            continue;
        }

        RECT nameRc = { 16, y, client.right - 16, y + 22 };
        SetTextColor(hdc, PROGRAM_COLOR_SELECT);
        DrawTextW(hdc, rows[i].name, -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        const int percent = ImportJobPercent(&rows[i]);
        RECT bar = { 16, y + 28, client.right - 88, y + 40 };
        HBRUSH track = CreateSolidBrush(PROGRAM_COLOR_WELL_DARK);
        FillRect(hdc, &bar, track);
        DeleteObject(track);
        if (percent > 0)
        {
            RECT fill = bar;
            fill.right = fill.left + ((bar.right - bar.left) * percent) / 100;
            if (fill.right < fill.left + 2 && percent > 0)
            {
                fill.right = fill.left + 2;
            }
            HBRUSH purple = CreateSolidBrush(IMPORT_PURPLE);
            FillRect(hdc, &fill, purple);
            DeleteObject(purple);
        }

        const char* label = "Waiting";
        char percentText[16] = {};
        if (rows[i].state == IMPORT_FAILED)
        {
            label = "Failed";
        }
        else if (rows[i].state == IMPORT_DONE)
        {
            label = "Done";
        }
        else if (rows[i].state == IMPORT_COPIED || rows[i].state == IMPORT_COPYING)
        {
            wsprintfA(percentText, "%d%%", percent);
            label = percentText;
        }
        RECT labelRc = { bar.right + 8, y + 24, client.right - 12, y + 44 };
        SetTextColor(hdc, rows[i].state == IMPORT_FAILED ? RGB(0xE0, 0x80, 0x80) : PROGRAM_COLOR_INK);
        DrawTextA(hdc, label, -1, &labelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(hdc, oldFont);
}

static LRESULT CALLBACK ImportProgressProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            PaintImportRows(hwnd, hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_SIZE:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_VSCROLL:
        {
            SCROLLINFO info = {};
            info.cbSize = sizeof(info);
            info.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &info);
            int next = info.nPos;
            switch (LOWORD(wParam))
            {
                case SB_LINEUP:
                    next -= 24;
                    break;
                case SB_LINEDOWN:
                    next += 24;
                    break;
                case SB_PAGEUP:
                    next -= static_cast<int>(info.nPage);
                    break;
                case SB_PAGEDOWN:
                    next += static_cast<int>(info.nPage);
                    break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION:
                    next = info.nTrackPos;
                    break;
                default:
                    return 0;
            }
            g_importScroll = next;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEWHEEL:
            g_importScroll += -GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 48;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_DESTROY:
            if (g_importWindow == hwnd)
            {
                g_importWindow = nullptr;
                ImportUiTick();
            }
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void OpenImportWindow()
{
    if (g_importWindow != nullptr && IsWindow(g_importWindow))
    {
        ShowWindow(g_importWindow, SW_SHOW);
        SetForegroundWindow(g_importWindow);
        return;
    }

    RECT anchor = {};
    int x = 80;
    int y = 80;
    if (g_importButton != nullptr && IsWindow(g_importButton))
    {
        GetWindowRect(g_importButton, &anchor);
        x = anchor.right - 440;
        y = anchor.bottom + 8;
    }
    RECT work = {};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    if (x < work.left)
    {
        x = work.left;
    }
    if (y < work.top)
    {
        y = work.top;
    }
    if (x + 440 > work.right)
    {
        x = work.right - 440;
    }
    if (y + 360 > work.bottom)
    {
        y = work.bottom - 360;
    }

    g_importWindow = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        "ImportProgressWindow",
        "Imports",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VSCROLL,
        x,
        y,
        440,
        360,
        g_programWindow,
        nullptr,
        g_instance,
        nullptr
    );
    if (g_importWindow != nullptr)
    {
        ShowWindow(g_importWindow, SW_SHOW);
    }
}

bool ImportDroppedMedia(HWND owner, const wchar_t* path)
{
    if (path == nullptr || path[0] == L'\0' || !App_Ready())
    {
        return false;
    }

    const bool video = IsVideoExtension(path);
    const bool image = IsImageExtension(path);
    if (!video && !image)
    {
        return false;
    }

    const bool background = g_programView == PROGRAM_VIEW_SHOW;
    if (!Document_HasProject())
    {
        return FinishDroppedMedia(owner, path, video, background);
    }
    return QueueDroppedMedia(owner, path, video, background);
}

static void SetTimeLabel(HWND label, const char* text)
{
    char current[40] = {};
    GetWindowTextA(label, current, sizeof(current));
    if (lstrcmpA(current, text) != 0)
    {
        SetWindowTextA(label, text);
    }
}

static void SetTransportTimes(HWND currentLabel, HWND durationLabel, double current, double duration)
{
    char text[24] = {};
    if (currentLabel != nullptr)
    {
        FormatMediaTime(text, sizeof(text), current);
        SetTimeLabel(currentLabel, text);
    }
    if (durationLabel != nullptr)
    {
        char time[16] = {};
        if (g_showRemaining)
        {
            FormatMediaTime(time, sizeof(time), duration > current ? (duration - current) : 0.0);
            wsprintfA(text, "-%s", time);
        }
        else
        {
            FormatMediaTime(time, sizeof(time), duration);
            lstrcpyA(text, time);
        }
        SetTimeLabel(durationLabel, text);
    }
}

void UpdateMediaTransportUi()
{
    GpuPlaybackStatus status = {};
    const bool has = GpuClient_GetStatus(&status);

    if (!g_videoScrubDragging && has && status.duration > 0.0)
    {
        g_videoScrubRatio = status.current / status.duration;
        if (g_videoScrubRatio < 0.0)
        {
            g_videoScrubRatio = 0.0;
        }
        if (g_videoScrubRatio > 1.0)
        {
            g_videoScrubRatio = 1.0;
        }
    }

    // Play/pause buttons read live state when they paint (icon, not text).
    if (g_programVideoPlay != nullptr)
    {
        InvalidateRect(g_programVideoPlay, nullptr, FALSE);
    }
    if (g_programAudioPlay != nullptr)
    {
        InvalidateRect(g_programAudioPlay, nullptr, FALSE);
    }

    if (!g_audioScrubDragging && Audio_HasMedia())
    {
        const double duration = Audio_DurationSeconds();
        if (duration > 0.0)
        {
            g_audioScrubRatio = Audio_CurrentSeconds() / duration;
            if (g_audioScrubRatio < 0.0)
            {
                g_audioScrubRatio = 0.0;
            }
            if (g_audioScrubRatio > 1.0)
            {
                g_audioScrubRatio = 1.0;
            }
        }
    }

    {
        const double duration = Audio_HasMedia() ? Audio_DurationSeconds() : 0.0;
        const double current = (duration > 0.0)
            ? (g_audioScrubDragging ? (g_audioScrubRatio * duration) : Audio_CurrentSeconds())
            : 0.0;
        SetTransportTimes(g_programAudioLabel, g_programAudioDuration, current, duration);
    }
    {
        const double duration = (has && status.duration > 0.0) ? status.duration : 0.0;
        const double current = duration > 0.0
            ? (g_videoScrubDragging ? (g_videoScrubRatio * duration) : status.current)
            : 0.0;
        SetTransportTimes(g_programVideoTime, g_programVideoDuration, current, duration);
    }

    if (g_programVideoScrub != nullptr)
    {
        InvalidateRect(g_programVideoScrub, nullptr, FALSE);
    }
    if (g_programAudioScrub != nullptr && g_mediaPanel == MEDIA_PANEL_AUDIO)
    {
        InvalidateRect(g_programAudioScrub, nullptr, FALSE);
    }
    static int lastSlideLive = -1;
    static int lastBgLive = -1;
    static int lastAudioLive = -1;
    const int slideLive = g_liveArmed ? 1 : 0;
    const int bgLive = g_backgroundActive ? 1 : 0;
    const int audioLive = Audio_HasMedia() ? 1 : 0;
    if (g_programClearSlideBtn != nullptr && slideLive != lastSlideLive)
    {
        lastSlideLive = slideLive;
        InvalidateRect(g_programClearSlideBtn, nullptr, FALSE);
    }
    if (g_programClearBgBtn != nullptr && bgLive != lastBgLive)
    {
        lastBgLive = bgLive;
        InvalidateRect(g_programClearBgBtn, nullptr, FALSE);
    }
    if (g_programClearAudioBtn != nullptr && audioLive != lastAudioLive)
    {
        lastAudioLive = audioLive;
        InvalidateRect(g_programClearAudioBtn, nullptr, FALSE);
    }
}

static double ScrubRatioFromPoint(HWND hwnd, LPARAM lParam)
{
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const int width = rc.right - rc.left - (PROGRAM_SCRUB_KNOB_R * 2);
    if (width <= 0)
    {
        return 0.0;
    }
    int x = GET_X_LPARAM(lParam) - PROGRAM_SCRUB_KNOB_R;
    if (x < 0)
    {
        x = 0;
    }
    if (x > width)
    {
        x = width;
    }
    return static_cast<double>(x) / static_cast<double>(width);
}

static void SeekVideoFromRatio(double ratio, bool force)
{
    if (ratio < 0.0)
    {
        ratio = 0.0;
    }
    if (ratio > 1.0)
    {
        ratio = 1.0;
    }
    g_videoScrubRatio = ratio;

    const DWORD now = GetTickCount();
    if (!force && (now - g_lastVideoSeekMs) < 40)
    {
        return;
    }

    GpuPlaybackStatus status = {};
    if (!GpuClient_GetStatus(&status) || status.duration <= 0.0)
    {
        return;
    }

    g_lastVideoSeekMs = now;
    GpuClient_Seek(status.duration * ratio);
}

static void SeekAudioFromRatio(double ratio, bool force)
{
    if (ratio < 0.0)
    {
        ratio = 0.0;
    }
    if (ratio > 1.0)
    {
        ratio = 1.0;
    }
    g_audioScrubRatio = ratio;

    const double duration = Audio_DurationSeconds();
    if (duration <= 0.0)
    {
        return;
    }

    const DWORD now = GetTickCount();
    if (!force && (now - g_lastAudioSeekMs) < 40)
    {
        return;
    }
    g_lastAudioSeekMs = now;
    Audio_SeekSeconds(duration * ratio);
}

static double ClampSeconds(double seconds, double duration)
{
    if (seconds < 0.0)
    {
        return 0.0;
    }
    return seconds > duration ? duration : seconds;
}

static int SkipBackSeconds()
{
    const int seconds = Store_GetInt("playback.skip_back", 10);
    return seconds > 0 ? seconds : 10;
}

static int SkipForwardSeconds()
{
    const int seconds = Store_GetInt("playback.skip_forward", 30);
    return seconds > 0 ? seconds : 30;
}

static void SkipVideoSeconds(double delta)
{
    GpuPlaybackStatus status = {};
    if (!GpuClient_GetStatus(&status) || status.duration <= 0.0)
    {
        return;
    }
    const double target = ClampSeconds(status.current + delta, status.duration);
    g_videoScrubRatio = target / status.duration;
    g_lastVideoSeekMs = GetTickCount();
    GpuClient_Seek(target);
}

static void SkipAudioSeconds(double delta)
{
    const double duration = Audio_HasMedia() ? Audio_DurationSeconds() : 0.0;
    if (duration <= 0.0)
    {
        return;
    }
    const double target = ClampSeconds(Audio_CurrentSeconds() + delta, duration);
    g_audioScrubRatio = target / duration;
    g_lastAudioSeekMs = GetTickCount();
    Audio_SeekSeconds(target);
}

static int SelectedTrackIds(int* outIds, int maxIds)
{
    if (g_audioTracks == nullptr || outIds == nullptr || maxIds <= 0)
    {
        return 0;
    }
    int indices[AUDIO_TRACK_MAX] = {};
    const int selCount = static_cast<int>(SendMessageA(
        g_audioTracks,
        LB_GETSELITEMS,
        AUDIO_TRACK_MAX,
        reinterpret_cast<LPARAM>(indices)
    ));
    int count = 0;
    for (int i = 0; i < selCount && count < maxIds; ++i)
    {
        const int id = static_cast<int>(SendMessageA(g_audioTracks, LB_GETITEMDATA, indices[i], 0));
        if (id > 0)
        {
            outIds[count++] = id;
        }
    }
    return count;
}

static int SelectedSlideIndices(int* out, int maxOut)
{
    if (out == nullptr || maxOut <= 0)
    {
        return 0;
    }
    const int slideCount = ProgramSlideCount();
    int count = 0;
    for (int i = 0; i < slideCount && count < maxOut; ++i)
    {
        if (g_slideSelected[i])
        {
            out[count++] = i;
        }
    }
    return count;
}

static void ClearSlideSelection()
{
    ZeroMemory(g_slideSelected, sizeof(g_slideSelected));
}

static void SetSlideSelectionRange(int from, int to, bool additive)
{
    const int slideCount = ProgramSlideCount();
    if (!additive)
    {
        ClearSlideSelection();
    }
    if (from > to)
    {
        const int swap = from;
        from = to;
        to = swap;
    }
    if (from < 0)
    {
        from = 0;
    }
    if (to >= slideCount)
    {
        to = slideCount - 1;
    }
    for (int i = from; i <= to; ++i)
    {
        g_slideSelected[i] = true;
    }
}

static void ApplySlideClickSelect(int index, bool ctrl, bool shift)
{
    const int slideCount = ProgramSlideCount();
    if (index < 0 || index >= slideCount)
    {
        return;
    }
    if (shift && g_slideSelectAnchor >= 0)
    {
        SetSlideSelectionRange(g_slideSelectAnchor, index, ctrl);
    }
    else if (ctrl)
    {
        g_slideSelected[index] = !g_slideSelected[index];
        g_slideSelectAnchor = index;
    }
    else
    {
        ClearSlideSelection();
        g_slideSelected[index] = true;
        g_slideSelectAnchor = index;
    }
}

static void ApplySlideBoxSelection(bool ctrl)
{
    const int left = g_slideBoxStart.x < g_slideBoxEnd.x ? g_slideBoxStart.x : g_slideBoxEnd.x;
    const int top = g_slideBoxStart.y < g_slideBoxEnd.y ? g_slideBoxStart.y : g_slideBoxEnd.y;
    const int right = (g_slideBoxStart.x > g_slideBoxEnd.x ? g_slideBoxStart.x : g_slideBoxEnd.x) + 1;
    const int bottom = (g_slideBoxStart.y > g_slideBoxEnd.y ? g_slideBoxStart.y : g_slideBoxEnd.y) + 1;
    RECT box = { left, top, right, bottom };
    if (!ctrl)
    {
        ClearSlideSelection();
    }
    if (g_programMiddle == nullptr)
    {
        return;
    }
    RECT client = {};
    GetClientRect(g_programMiddle, &client);
    RECT cells[PROGRAM_SLIDE_MAX] = {};
    const int count = ProgramSlideCount();
    LayoutProgramSlideCells(&client, cells, count);
    for (int i = 0; i < count; ++i)
    {
        RECT hit = {};
        if (IntersectRect(&hit, &cells[i], &box))
        {
            g_slideSelected[i] = true;
        }
    }
}

static void ShowEditContextMenu(HWND owner, int x, int y, int target)
{
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }
    g_contextTarget = target;
    bool hasSelection = false;
    bool canPaste = false;
    if (target == CTX_TRACKS)
    {
        int ids[AUDIO_TRACK_MAX] = {};
        hasSelection = SelectedTrackIds(ids, AUDIO_TRACK_MAX) > 0;
        canPaste = g_trackClipboardCount > 0;
    }
    else if (target == CTX_SLIDES)
    {
        int indices[PROGRAM_SLIDE_MAX] = {};
        const int count = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
        hasSelection = count > 0;
        canPaste = Renderer_HasSlideClipboard();
        // Editing is a one-slide action.
        AppendMenuA(menu, MF_STRING | (count == 1 ? 0 : MF_GRAYED), IDM_CTX_EDIT_SLIDE, "&Edit slide");
        const bool canUseAsSlide = count == 1 && Renderer_IsBackgroundSlide(indices[0]);
        const bool canUseAsBg = count == 1 && !Renderer_IsBackgroundSlide(indices[0]);
        const bool canTransition = count >= 1;
        AppendMenuA(menu, MF_STRING | (canUseAsSlide ? 0 : MF_GRAYED), IDM_CTX_USE_AS_SLIDE, "&Use as slide");
        AppendMenuA(menu, MF_STRING | (canUseAsBg ? 0 : MF_GRAYED), IDM_CTX_USE_AS_BG, "Use as &background");
        AppendMenuA(menu, MF_STRING | (canTransition ? 0 : MF_GRAYED), IDM_CTX_TRANSITIONS, "&Transitions");
        AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuA(menu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), IDM_CTX_CUT, "Cu&t");
    AppendMenuA(menu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), IDM_CTX_COPY, "&Copy");
    AppendMenuA(menu, MF_STRING | (canPaste ? 0 : MF_GRAYED), IDM_CTX_PASTE, "&Paste");
    AppendMenuA(menu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), IDM_CTX_DELETE, "&Delete");
    const int command = TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD,
        x,
        y,
        0,
        owner,
        nullptr
    );
    DestroyMenu(menu);
    if (command != 0)
    {
        HandleContextMenuCommand(command);
    }
    else
    {
        g_contextTarget = CTX_NONE;
    }
}

static void CopySelectedTracks(bool cut)
{
    int ids[AUDIO_TRACK_MAX] = {};
    const int count = SelectedTrackIds(ids, AUDIO_TRACK_MAX);
    g_trackClipboardCount = 0;
    g_trackClipboardCut = cut;
    if (count <= 0)
    {
        return;
    }

    AudioTrack tracks[AUDIO_TRACK_MAX] = {};
    const int trackCount = Audio_ListTracks(Audio_ActivePlaylistId(), tracks, AUDIO_TRACK_MAX);
    for (int i = 0; i < count; ++i)
    {
        for (int j = 0; j < trackCount; ++j)
        {
            if (tracks[j].id == ids[i])
            {
                g_trackClipboard[g_trackClipboardCount++] = tracks[j];
                break;
            }
        }
    }

    if (cut && g_trackClipboardCount > 0)
    {
        Audio_DeleteTracks(ids, count);
        RefreshAudioLists();
    }
}

static void PasteTrackClipboard()
{
    if (g_trackClipboardCount <= 0)
    {
        return;
    }
    const int playlistId = Audio_ActivePlaylistId();
    if (playlistId <= 0)
    {
        return;
    }
    bool added = false;
    for (int i = 0; i < g_trackClipboardCount; ++i)
    {
        if (Audio_InsertTrackCopy(playlistId, g_trackClipboard[i].name, g_trackClipboard[i].path))
        {
            added = true;
        }
    }
    if (g_trackClipboardCut)
    {
        g_trackClipboardCount = 0;
        g_trackClipboardCut = false;
    }
    if (added)
    {
        RefreshAudioLists();
    }
}

static void DeleteSelectedTracks()
{
    int ids[AUDIO_TRACK_MAX] = {};
    const int count = SelectedTrackIds(ids, AUDIO_TRACK_MAX);
    if (count <= 0)
    {
        return;
    }
    Audio_DeleteTracks(ids, count);
    RefreshAudioLists();
}

static void ReleaseHoldCanvas()
{
    if (g_holdCanvas != nullptr)
    {
        DeleteObject(g_holdCanvas);
        g_holdCanvas = nullptr;
    }
    g_holdCanvasW = 0;
    g_holdCanvasH = 0;
    g_holdCanvasSource = nullptr;
}

static void ReleaseLiveBackground()
{
    ReleaseHoldCanvas();
    if (g_liveBgFrame != nullptr)
    {
        DeleteObject(g_liveBgFrame);
        g_liveBgFrame = nullptr;
    }
    if (g_liveBgStill != nullptr)
    {
        DeleteObject(g_liveBgStill);
        g_liveBgStill = nullptr;
    }
    g_liveBgStillW = 0;
    g_liveBgStillH = 0;
    g_liveBgW = 0;
    g_liveBgH = 0;
    g_liveBgVideo[0] = L'\0';
    g_liveBgHeld = false;
    if (g_holdOverlay != nullptr)
    {
        DeleteObject(g_holdOverlay);
        g_holdOverlay = nullptr;
        g_holdOverlayW = 0;
        g_holdOverlayH = 0;
    }
}

static HBITMAP CreateXfadeDib(int width, int height, void** bits)
{
    if (width < 1 || height < 1 || bits == nullptr)
    {
        return nullptr;
    }
    *bits = nullptr;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, bits, nullptr, 0);
}

static void ReleaseXfadeBitmaps()
{
    if (g_xfadeFrame != nullptr)
    {
        DeleteObject(g_xfadeFrame);
        g_xfadeFrame = nullptr;
    }
    free(g_xfadeFromBits);
    free(g_xfadeToBits);
    free(g_xfadeOutBits);
    g_xfadeFromBits = nullptr;
    g_xfadeToBits = nullptr;
    g_xfadeOutBits = nullptr;
    g_xfadeBitsCap = 0;
    g_xfadeFromW = 0;
    g_xfadeFromH = 0;
    g_xfadeToW = 0;
    g_xfadeToH = 0;
    g_xfadeFrameW = 0;
    g_xfadeFrameH = 0;
}

static void StopSlideXfade()
{
    if (g_xfadeOn)
    {
        timeEndPeriod(1);
    }
    g_xfadeOn = false;
    g_xfadeClockArmed = false;
    g_xfadeTo = -1;
    g_xfadeKind = SLIDE_TRANSITION_CROSSFADE;
    g_xfadeDir = SLIDE_TRANSITION_DIR_LEFT;
    g_xfadeMs = XFADE_MS;
    g_xfadeStart = 0;
    ReleaseXfadeBitmaps();
    // Shared ID_XFADE_TIMER also drives background fades — leave it running
    // when a BG transition is still in flight.
    if (g_programWindow != nullptr && !g_bgXfadeOn)
    {
        KillTimer(g_programWindow, ID_XFADE_TIMER);
    }
}

static float SlideXfadeT()
{
    if (!g_xfadeOn)
    {
        return 1.0f;
    }
    if (!g_xfadeClockArmed)
    {
        return 0.0f;
    }
    const DWORD elapsed = GetTickCount() - g_xfadeStart;
    const int duration = g_xfadeMs > 0 ? g_xfadeMs : XFADE_MS;
    if (elapsed >= static_cast<DWORD>(duration))
    {
        return 1.0f;
    }
    return static_cast<float>(elapsed) / static_cast<float>(duration);
}

static unsigned char* AllocXfadePlane(int bytes)
{
    if (bytes < 1)
    {
        return nullptr;
    }
    return static_cast<unsigned char*>(malloc(static_cast<size_t>(bytes)));
}

static bool GrabBitmap32(HBITMAP src, int srcW, int srcH, int destW, int destH, unsigned char* dest)
{
    if (src == nullptr || dest == nullptr || srcW < 1 || srcH < 1 || destW < 1 || destH < 1)
    {
        return false;
    }

    DIBSECTION section = {};
    if (GetObject(src, sizeof(section), &section) != 0 &&
        section.dsBm.bmBits != nullptr &&
        section.dsBm.bmBitsPixel == 32)
    {
        const int stride = section.dsBm.bmWidthBytes;
        const unsigned char* bits = static_cast<const unsigned char*>(section.dsBm.bmBits);
        // GetObject stores bmHeight and dsBmih.biHeight as a positive magnitude
        // even when CreateDIBSection was given a negative height. Slide frames
        // are top-down, so memory row 0 is the top of the picture.
        const int rowBytes = destW * 4;
        for (int y = 0; y < destH; ++y)
        {
            int sy = destH == srcH ? y : MulDiv(y, srcH, destH);
            if (sy < 0)
            {
                sy = 0;
            }
            if (sy >= srcH)
            {
                sy = srcH - 1;
            }
            const int srcRow = sy;
            const unsigned char* row = bits + (static_cast<size_t>(srcRow) * stride);
            unsigned char* out = dest + (static_cast<size_t>(y) * rowBytes);
            if (destW == srcW)
            {
                memcpy(out, row, static_cast<size_t>(rowBytes));
            }
            else
            {
                for (int x = 0; x < destW; ++x)
                {
                    int sx = MulDiv(x, srcW, destW);
                    if (sx < 0)
                    {
                        sx = 0;
                    }
                    if (sx >= srcW)
                    {
                        sx = srcW - 1;
                    }
                    memcpy(out + (x * 4), row + (sx * 4), 4);
                }
            }
        }
        return true;
    }

    void* dibBits = nullptr;
    HBITMAP dib = CreateXfadeDib(destW, destH, &dibBits);
    if (dib == nullptr || dibBits == nullptr)
    {
        return false;
    }

    HDC screen = GetDC(nullptr);
    HDC destDc = CreateCompatibleDC(screen);
    HDC srcDc = CreateCompatibleDC(screen);
    bool ok = false;
    if (destDc != nullptr && srcDc != nullptr)
    {
        HBITMAP oldDest = (HBITMAP)SelectObject(destDc, dib);
        HBITMAP oldSrc = (HBITMAP)SelectObject(srcDc, src);
        SetStretchBltMode(destDc, COLORONCOLOR);
        ok = StretchBlt(destDc, 0, 0, destW, destH, srcDc, 0, 0, srcW, srcH, SRCCOPY) != FALSE;
        SelectObject(srcDc, oldSrc);
        SelectObject(destDc, oldDest);
    }
    if (srcDc != nullptr)
    {
        DeleteDC(srcDc);
    }
    if (destDc != nullptr)
    {
        DeleteDC(destDc);
    }
    ReleaseDC(nullptr, screen);
    if (ok)
    {
        const int pixels = destW * destH;
        memcpy(dest, dibBits, static_cast<size_t>(pixels) * 4);
        for (int i = 0; i < pixels; ++i)
        {
            if (dest[i * 4 + 3] == 0)
            {
                dest[i * 4 + 3] = 255;
            }
        }
    }
    DeleteObject(dib);
    return ok;
}

static bool SyncXfadeFrameBitmap()
{
    const int width = g_xfadeToW;
    const int height = g_xfadeToH;
    if (g_xfadeOutBits == nullptr || width < 1 || height < 1)
    {
        return false;
    }
    if (g_xfadeFrame == nullptr || g_xfadeFrameW != width || g_xfadeFrameH != height)
    {
        if (g_xfadeFrame != nullptr)
        {
            DeleteObject(g_xfadeFrame);
            g_xfadeFrame = nullptr;
        }
        void* destBits = nullptr;
        g_xfadeFrame = CreateXfadeDib(width, height, &destBits);
        g_xfadeFrameW = width;
        g_xfadeFrameH = height;
        if (g_xfadeFrame == nullptr || destBits == nullptr)
        {
            return false;
        }
    }

    DIBSECTION section = {};
    if (GetObject(g_xfadeFrame, sizeof(section), &section) == 0 || section.dsBm.bmBits == nullptr)
    {
        return false;
    }
    const int stride = section.dsBm.bmWidthBytes;
    const unsigned char* src = g_xfadeOutBits;
    unsigned char* dest = static_cast<unsigned char*>(section.dsBm.bmBits);
    const int packed = width * 4;
    if (stride == packed)
    {
        memcpy(dest, src, static_cast<size_t>(packed) * static_cast<size_t>(height));
        return true;
    }
    for (int y = 0; y < height; ++y)
    {
        memcpy(dest + (static_cast<size_t>(y) * stride), src + (static_cast<size_t>(y) * packed), static_cast<size_t>(packed));
    }
    return true;
}

static void CopyXfadePixel(unsigned char* out, const unsigned char* plane, int width, int height, int x, int y)
{
    if (x < 0 || y < 0 || x >= width || y >= height)
    {
        out[0] = 0;
        out[1] = 0;
        out[2] = 0;
        out[3] = 255;
        return;
    }
    memcpy(out, plane + (static_cast<size_t>(y) * width + x) * 4, 4);
}

static void LerpXfadePixel(unsigned char* out, const unsigned char* a, const unsigned char* b, int toW)
{
    const int fromW = 255 - toW;
    out[0] = static_cast<unsigned char>((a[0] * fromW + b[0] * toW) / 255);
    out[1] = static_cast<unsigned char>((a[1] * fromW + b[1] * toW) / 255);
    out[2] = static_cast<unsigned char>((a[2] * fromW + b[2] * toW) / 255);
    out[3] = static_cast<unsigned char>((a[3] * fromW + b[3] * toW) / 255);
}

static void ComposeTransitionPixels(
    int kind,
    int dir,
    float t,
    const unsigned char* from,
    const unsigned char* toBits,
    unsigned char* out,
    int width,
    int height)
{
    if (from == nullptr || toBits == nullptr || out == nullptr || width < 1 || height < 1)
    {
        return;
    }
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    if (t > 1.0f)
    {
        t = 1.0f;
    }

    if (kind == SLIDE_TRANSITION_NONE)
    {
        // Cut card: static A | B split so the grid stays readable.
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                CopyXfadePixel(
                    out + (static_cast<size_t>(y) * width + x) * 4,
                    x < width / 2 ? from : toBits,
                    width,
                    height,
                    x,
                    y
                );
            }
        }
        return;
    }

    if (kind == SLIDE_TRANSITION_CROSSFADE)
    {
        const int toW = static_cast<int>(t * 255.0f + 0.5f);
        const int fromW = 255 - toW;
        const int pixels = width * height;
        const unsigned char* a = from;
        const unsigned char* b = toBits;
        unsigned char* d = out;
        for (int i = 0; i < pixels; ++i, a += 4, b += 4, d += 4)
        {
            d[0] = static_cast<unsigned char>((a[0] * fromW + b[0] * toW) / 255);
            d[1] = static_cast<unsigned char>((a[1] * fromW + b[1] * toW) / 255);
            d[2] = static_cast<unsigned char>((a[2] * fromW + b[2] * toW) / 255);
            d[3] = static_cast<unsigned char>((a[3] * fromW + b[3] * toW) / 255);
        }
        return;
    }

    if (kind == SLIDE_TRANSITION_FADE_BLACK)
    {
        const int pixels = width * height;
        if (t < 0.5f)
        {
            const int toW = static_cast<int>((t / 0.5f) * 255.0f + 0.5f);
            const int fromW = 255 - toW;
            const unsigned char* a = from;
            unsigned char* d = out;
            for (int i = 0; i < pixels; ++i, a += 4, d += 4)
            {
                d[0] = static_cast<unsigned char>((a[0] * fromW) / 255);
                d[1] = static_cast<unsigned char>((a[1] * fromW) / 255);
                d[2] = static_cast<unsigned char>((a[2] * fromW) / 255);
                d[3] = 255;
            }
        }
        else
        {
            const int toW = static_cast<int>(((t - 0.5f) / 0.5f) * 255.0f + 0.5f);
            const unsigned char* b = toBits;
            unsigned char* d = out;
            for (int i = 0; i < pixels; ++i, b += 4, d += 4)
            {
                d[0] = static_cast<unsigned char>((b[0] * toW) / 255);
                d[1] = static_cast<unsigned char>((b[1] * toW) / 255);
                d[2] = static_cast<unsigned char>((b[2] * toW) / 255);
                d[3] = 255;
            }
        }
        return;
    }

    if (kind == SLIDE_TRANSITION_WIPE)
    {
        // Row/column memcpy instead of per-pixel calls.
        if (dir == SLIDE_TRANSITION_DIR_LEFT || dir == SLIDE_TRANSITION_DIR_RIGHT)
        {
            int edge = static_cast<int>(t * width + 0.5f);
            if (edge < 0)
            {
                edge = 0;
            }
            if (edge > width)
            {
                edge = width;
            }
            const int fromStart = (dir == SLIDE_TRANSITION_DIR_RIGHT) ? edge : 0;
            const int fromCount = (dir == SLIDE_TRANSITION_DIR_RIGHT) ? (width - edge) : (width - edge);
            const int toStart = (dir == SLIDE_TRANSITION_DIR_RIGHT) ? 0 : (width - edge);
            const int toCount = edge;
            for (int y = 0; y < height; ++y)
            {
                const size_t row = static_cast<size_t>(y) * static_cast<size_t>(width) * 4u;
                if (fromCount > 0)
                {
                    memcpy(
                        out + row + static_cast<size_t>(fromStart) * 4u,
                        from + row + static_cast<size_t>(fromStart) * 4u,
                        static_cast<size_t>(fromCount) * 4u
                    );
                }
                if (toCount > 0)
                {
                    memcpy(
                        out + row + static_cast<size_t>(toStart) * 4u,
                        toBits + row + static_cast<size_t>(toStart) * 4u,
                        static_cast<size_t>(toCount) * 4u
                    );
                }
            }
            return;
        }

        int edge = static_cast<int>(t * height + 0.5f);
        if (edge < 0)
        {
            edge = 0;
        }
        if (edge > height)
        {
            edge = height;
        }
        const int fromY0 = (dir == SLIDE_TRANSITION_DIR_DOWN) ? edge : 0;
        const int fromRows = (dir == SLIDE_TRANSITION_DIR_DOWN) ? (height - edge) : (height - edge);
        const int toY0 = (dir == SLIDE_TRANSITION_DIR_DOWN) ? 0 : (height - edge);
        const int toRows = edge;
        const size_t rowBytes = static_cast<size_t>(width) * 4u;
        if (fromRows > 0)
        {
            memcpy(
                out + static_cast<size_t>(fromY0) * rowBytes,
                from + static_cast<size_t>(fromY0) * rowBytes,
                static_cast<size_t>(fromRows) * rowBytes
            );
        }
        if (toRows > 0)
        {
            memcpy(
                out + static_cast<size_t>(toY0) * rowBytes,
                toBits + static_cast<size_t>(toY0) * rowBytes,
                static_cast<size_t>(toRows) * rowBytes
            );
        }
        return;
    }

    const int ox = (dir == SLIDE_TRANSITION_DIR_LEFT) ? -static_cast<int>(t * width + 0.5f)
                 : (dir == SLIDE_TRANSITION_DIR_RIGHT) ? static_cast<int>(t * width + 0.5f)
                 : 0;
    const int oy = (dir == SLIDE_TRANSITION_DIR_UP) ? -static_cast<int>(t * height + 0.5f)
                 : (dir == SLIDE_TRANSITION_DIR_DOWN) ? static_cast<int>(t * height + 0.5f)
                 : 0;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            unsigned char* pixel = out + (static_cast<size_t>(y) * width + x) * 4;
            if (kind == SLIDE_TRANSITION_PUSH)
            {
                const int fx = x - ox;
                const int fy = y - oy;
                if (fx >= 0 && fx < width && fy >= 0 && fy < height)
                {
                    CopyXfadePixel(pixel, from, width, height, fx, fy);
                }
                else
                {
                    const int tx = x - ox - ((dir == SLIDE_TRANSITION_DIR_LEFT) ? width
                                         : (dir == SLIDE_TRANSITION_DIR_RIGHT) ? -width : 0);
                    const int ty = y - oy - ((dir == SLIDE_TRANSITION_DIR_UP) ? height
                                         : (dir == SLIDE_TRANSITION_DIR_DOWN) ? -height : 0);
                    CopyXfadePixel(pixel, toBits, width, height, tx, ty);
                }
            }
            else
            {
                // Cover / reveal
                int originX = 0;
                int originY = 0;
                if (dir == SLIDE_TRANSITION_DIR_LEFT)
                {
                    originX = static_cast<int>((1.0f - t) * width + 0.5f);
                }
                else if (dir == SLIDE_TRANSITION_DIR_RIGHT)
                {
                    originX = -static_cast<int>((1.0f - t) * width + 0.5f);
                }
                else if (dir == SLIDE_TRANSITION_DIR_UP)
                {
                    originY = static_cast<int>((1.0f - t) * height + 0.5f);
                }
                else
                {
                    originY = -static_cast<int>((1.0f - t) * height + 0.5f);
                }
                const int sx = x - originX;
                const int sy = y - originY;
                if (sx >= 0 && sx < width && sy >= 0 && sy < height)
                {
                    CopyXfadePixel(pixel, toBits, width, height, sx, sy);
                }
                else
                {
                    CopyXfadePixel(pixel, from, width, height, x, y);
                }
            }
        }
    }
}

static bool ComposeXfadeFrame()
{
    if (!g_xfadeOn || g_xfadeFromBits == nullptr || g_xfadeToBits == nullptr || g_xfadeOutBits == nullptr)
    {
        return false;
    }
    if (g_xfadeToW < 1 || g_xfadeToH < 1)
    {
        return false;
    }
    ComposeTransitionPixels(
        g_xfadeKind,
        g_xfadeDir,
        SlideXfadeT(),
        g_xfadeFromBits,
        g_xfadeToBits,
        g_xfadeOutBits,
        g_xfadeToW,
        g_xfadeToH
    );
    return SyncXfadeFrameBitmap();
}

static bool AllocXfadePlanes(int width, int height)
{
    const int bytes = width * height * 4;
    if (bytes < 4)
    {
        return false;
    }
    if (g_xfadeBitsCap < bytes)
    {
        unsigned char* from = AllocXfadePlane(bytes);
        unsigned char* to = AllocXfadePlane(bytes);
        unsigned char* out = AllocXfadePlane(bytes);
        if (from == nullptr || to == nullptr || out == nullptr)
        {
            free(from);
            free(to);
            free(out);
            return false;
        }
        free(g_xfadeFromBits);
        free(g_xfadeToBits);
        free(g_xfadeOutBits);
        g_xfadeFromBits = from;
        g_xfadeToBits = to;
        g_xfadeOutBits = out;
        g_xfadeBitsCap = bytes;
    }
    return true;
}

static void StartSlideXfade(int fromIndex, int toIndex)
{
    int toW = 0;
    int toH = 0;
    HBITMAP toSrc = Renderer_SlideFrame(toIndex, &toW, &toH);
    if (toSrc == nullptr || toW < 1 || toH < 1)
    {
        return;
    }

    unsigned char* keep = nullptr;
    int keepW = 0;
    int keepH = 0;
    if (g_xfadeOn && g_xfadeOutBits != nullptr && g_xfadeToW > 0 && g_xfadeToH > 0)
    {
        keepW = g_xfadeToW;
        keepH = g_xfadeToH;
        keep = AllocXfadePlane(keepW * keepH * 4);
        if (keep != nullptr)
        {
            memcpy(keep, g_xfadeOutBits, static_cast<size_t>(keepW) * static_cast<size_t>(keepH) * 4);
        }
    }

    int fromW = 0;
    int fromH = 0;
    HBITMAP fromSrc = nullptr;
    if (keep == nullptr)
    {
        fromSrc = Renderer_SlideFrame(fromIndex, &fromW, &fromH);
        if (fromSrc == nullptr || fromW < 1 || fromH < 1)
        {
            fromSrc = nullptr;
        }
    }

    StopSlideXfade();
    if (!AllocXfadePlanes(toW, toH))
    {
        free(keep);
        return;
    }
    g_xfadeToW = toW;
    g_xfadeToH = toH;
    g_xfadeFromW = toW;
    g_xfadeFromH = toH;

    bool grabbed = false;
    if (keep != nullptr)
    {
        if (keepW == toW && keepH == toH)
        {
            memcpy(g_xfadeFromBits, keep, static_cast<size_t>(toW) * static_cast<size_t>(toH) * 4);
            grabbed = true;
        }
        else
        {
            for (int y = 0; y < toH; ++y)
            {
                const int sy = MulDiv(y, keepH, toH);
                const unsigned char* row = keep + (static_cast<size_t>(sy) * keepW * 4);
                unsigned char* out = g_xfadeFromBits + (static_cast<size_t>(y) * toW * 4);
                for (int x = 0; x < toW; ++x)
                {
                    const int sx = MulDiv(x, keepW, toW);
                    memcpy(out + (x * 4), row + (sx * 4), 4);
                }
            }
            grabbed = true;
        }
    }
    else if (fromSrc != nullptr)
    {
        grabbed = GrabBitmap32(fromSrc, fromW, fromH, toW, toH, g_xfadeFromBits);
    }
    else
    {
        memset(g_xfadeFromBits, 0, static_cast<size_t>(toW) * static_cast<size_t>(toH) * 4);
        grabbed = true;
    }
    free(keep);
    if (!grabbed || !GrabBitmap32(toSrc, toW, toH, toW, toH, g_xfadeToBits))
    {
        StopSlideXfade();
        return;
    }

    g_xfadeTo = toIndex;
    g_xfadeKind = Renderer_EffectiveSlideTransition(toIndex);
    g_xfadeDir = Renderer_EffectiveSlideTransitionDir(toIndex);
    // Destination slide's transitionMs drives the whole effect (genie pinwheel / NudgeTransitionMs).
    g_xfadeMs = Renderer_EffectiveSlideTransitionMs(toIndex);
    g_xfadeStart = 0;
    g_xfadeClockArmed = false;
    g_xfadeOn = true;
    timeBeginPeriod(1);
    if (!ComposeXfadeFrame())
    {
        StopSlideXfade();
        return;
    }
    if (g_programWindow != nullptr)
    {
        SetTimer(g_programWindow, ID_XFADE_TIMER, 10, nullptr);
    }
}

static void ArmSlideXfadeClock()
{
    if (!g_xfadeOn)
    {
        return;
    }
    g_xfadeStart = GetTickCount();
    g_xfadeClockArmed = true;
}

static void ScheduleLiveSave()
{
    if (g_programWindow != nullptr)
    {
        SetTimer(g_programWindow, ID_LIVE_SAVE_TIMER, 800, nullptr);
        return;
    }
    Document_Save();
}

static void TickSlideXfade()
{
    if (!g_xfadeOn)
    {
        // Shared with background xfade — only the WM_TIMER handler kills the timer
        // once both planes are idle. Killing here aborted BG fades after one tick.
        return;
    }
    if (SlideXfadeT() >= 1.0f)
    {
        StopSlideXfade();
        PresentCurrent();
        return;
    }
    if (ComposeXfadeFrame())
    {
        PresentXfadeTick();
    }
}

static bool SlideXfadeActive()
{
    return g_xfadeOn && g_xfadeOutBits != nullptr && g_xfadeToW > 0 && g_xfadeToH > 0;
}

static HBITMAP SlideXfadeFrame(int* width, int* height)
{
    if (!SlideXfadeActive())
    {
        return nullptr;
    }
    if (width != nullptr)
    {
        *width = g_xfadeFrameW > 0 ? g_xfadeFrameW : g_xfadeToW;
    }
    if (height != nullptr)
    {
        *height = g_xfadeFrameH > 0 ? g_xfadeFrameH : g_xfadeToH;
    }
    return g_xfadeFrame;
}

static const unsigned char* SlideXfadeBits(int* width, int* height)
{
    if (!SlideXfadeActive())
    {
        return nullptr;
    }
    if (width != nullptr)
    {
        *width = g_xfadeToW;
    }
    if (height != nullptr)
    {
        *height = g_xfadeToH;
    }
    return g_xfadeOutBits;
}

static void BlitAlphaOverlay(
    HDC dest,
    int destX,
    int destY,
    int destW,
    int destH,
    HBITMAP bmp,
    int srcX,
    int srcY,
    int srcW,
    int srcH
)
{
    if (dest == nullptr || bmp == nullptr || destW <= 0 || destH <= 0 || srcW <= 0 || srcH <= 0)
    {
        return;
    }
    HDC mem = CreateCompatibleDC(dest);
    if (mem == nullptr)
    {
        return;
    }
    HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(dest, destX, destY, destW, destH, mem, srcX, srcY, srcW, srcH, blend);
    SelectObject(mem, old);
    DeleteDC(mem);
}

static void StopBackgroundXfade()
{
    g_bgXfadeOn = false;
    g_bgXfadeClockArmed = false;
    g_bgXfadeStart = 0;
    g_bgXfadePendingIndex = -1;
    g_bgXfadePendingVideo[0] = L'\0';
    g_bgXfadeHoldOutgoingVideo = false;
    free(g_bgXfadeFromBits);
    free(g_bgXfadeToBits);
    free(g_bgXfadeOutBits);
    g_bgXfadeFromBits = nullptr;
    g_bgXfadeToBits = nullptr;
    g_bgXfadeOutBits = nullptr;
    g_bgXfadeBitsCap = 0;
    g_bgXfadeW = 0;
    g_bgXfadeH = 0;
}

static void BgXfadeWorkingSize(int srcW, int srcH, int* outW, int* outH)
{
    if (outW == nullptr || outH == nullptr)
    {
        return;
    }
    if (srcW < 1 || srcH < 1)
    {
        *outW = 0;
        *outH = 0;
        return;
    }
    if (srcW <= BG_XFADE_MAX_W)
    {
        *outW = srcW;
        *outH = srcH;
        return;
    }
    *outW = BG_XFADE_MAX_W;
    *outH = MulDiv(srcH, BG_XFADE_MAX_W, srcW);
    if (*outH < 1)
    {
        *outH = 1;
    }
}

static bool AllocBgXfadePlanes(int width, int height)
{
    const int bytes = width * height * 4;
    if (bytes < 4)
    {
        return false;
    }
    if (g_bgXfadeBitsCap >= bytes)
    {
        return true;
    }
    unsigned char* from = AllocXfadePlane(bytes);
    unsigned char* to = AllocXfadePlane(bytes);
    unsigned char* out = AllocXfadePlane(bytes);
    if (from == nullptr || to == nullptr || out == nullptr)
    {
        free(from);
        free(to);
        free(out);
        return false;
    }
    free(g_bgXfadeFromBits);
    free(g_bgXfadeToBits);
    free(g_bgXfadeOutBits);
    g_bgXfadeFromBits = from;
    g_bgXfadeToBits = to;
    g_bgXfadeOutBits = out;
    g_bgXfadeBitsCap = bytes;
    return true;
}

static float BackgroundXfadeT()
{
    if (!g_bgXfadeOn)
    {
        return 1.0f;
    }
    if (!g_bgXfadeClockArmed)
    {
        return 0.0f;
    }
    const DWORD elapsed = GetTickCount() - g_bgXfadeStart;
    const int duration = g_bgXfadeMs > 0 ? g_bgXfadeMs : XFADE_MS;
    if (elapsed >= static_cast<DWORD>(duration))
    {
        return 1.0f;
    }
    return static_cast<float>(elapsed) / static_cast<float>(duration);
}

static bool SyncLiveBgFromBits(const unsigned char* bits, int width, int height)
{
    if (bits == nullptr || width < 1 || height < 1)
    {
        return false;
    }

    void* destBits = nullptr;
    // Reuse the live hold DIB across ticks — recreating it every 10ms hitchs switches.
    if (g_liveBgFrame != nullptr && g_liveBgW == width && g_liveBgH == height)
    {
        DIBSECTION section = {};
        if (GetObject(g_liveBgFrame, sizeof(section), &section) != 0 &&
            section.dsBm.bmBits != nullptr &&
            section.dsBm.bmBitsPixel == 32)
        {
            destBits = section.dsBm.bmBits;
            const int stride = section.dsBm.bmWidthBytes;
            const int packed = width * 4;
            const unsigned char* src = bits;
            unsigned char* dest = static_cast<unsigned char*>(destBits);
            if (stride == packed)
            {
                memcpy(dest, src, static_cast<size_t>(packed) * static_cast<size_t>(height));
            }
            else
            {
                for (int y = 0; y < height; ++y)
                {
                    memcpy(
                        dest + (static_cast<size_t>(y) * stride),
                        src + (static_cast<size_t>(y) * packed),
                        static_cast<size_t>(packed)
                    );
                }
            }
            g_holdCanvasSource = nullptr;
            return true;
        }
    }

    HBITMAP dib = CreateXfadeDib(width, height, &destBits);
    if (dib == nullptr || destBits == nullptr)
    {
        if (dib != nullptr)
        {
            DeleteObject(dib);
        }
        return false;
    }
    memcpy(destBits, bits, static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    if (g_liveBgFrame != nullptr)
    {
        DeleteObject(g_liveBgFrame);
    }
    g_liveBgFrame = dib;
    g_liveBgW = width;
    g_liveBgH = height;
    g_holdCanvasSource = nullptr;
    return true;
}

static void InstallLiveBackgroundMedia(HBITMAP frame, int width, int height, const wchar_t* videoPath)
{
    if (frame == nullptr || width < 1 || height < 1)
    {
        return;
    }
    if (g_liveBgFrame != nullptr && g_liveBgFrame != frame)
    {
        DeleteObject(g_liveBgFrame);
    }
    g_liveBgFrame = frame;
    g_liveBgW = width;
    g_liveBgH = height;
    g_holdCanvasSource = nullptr;
    if (g_liveBgStill != nullptr)
    {
        DeleteObject(g_liveBgStill);
        g_liveBgStill = nullptr;
        g_liveBgStillW = 0;
        g_liveBgStillH = 0;
    }
    g_liveBgVideo[0] = L'\0';
    if (videoPath != nullptr && videoPath[0] != L'\0')
    {
        lstrcpynW(g_liveBgVideo, videoPath, MAX_PATH);
    }
    g_liveBgHeld = true;
    g_backgroundActive = true;
}

static HBITMAP DuplicateBgBitmap(HBITMAP src, int width, int height)
{
    if (src == nullptr || width < 1 || height < 1)
    {
        return nullptr;
    }
    void* bits = nullptr;
    HBITMAP dib = CreateXfadeDib(width, height, &bits);
    if (dib == nullptr || bits == nullptr)
    {
        if (dib != nullptr)
        {
            DeleteObject(dib);
        }
        return nullptr;
    }
    if (!GrabBitmap32(src, width, height, width, height, static_cast<unsigned char*>(bits)))
    {
        DeleteObject(dib);
        return nullptr;
    }
    return dib;
}

static void InstallLiveBackgroundFrame(HBITMAP frame, int width, int height, int index)
{
    wchar_t videoPath[MAX_PATH] = {};
    if (index >= 0)
    {
        Renderer_BackgroundVideoPath(videoPath, MAX_PATH);
    }
    InstallLiveBackgroundMedia(frame, width, height, videoPath);
    // Video installs go straight to GPU PLAY — skip a second full-frame DIB copy.
}

static HBITMAP MakeBlankBgHold(int width, int height)
{
    void* bits = nullptr;
    HBITMAP dib = CreateXfadeDib(width, height, &bits);
    if (dib == nullptr || bits == nullptr)
    {
        if (dib != nullptr)
        {
            DeleteObject(dib);
        }
        return nullptr;
    }
    ZeroMemory(bits, static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    return dib;
}

// Transition any live bed (BG slide or Media-tab item) with the given kind.
// Owns `neu` on all paths. pendingIndex < 0 = library media (use pendingVideo).
static void BeginBackgroundTransition(
    HBITMAP neu,
    int newW,
    int newH,
    int pendingIndex,
    const wchar_t* pendingVideo,
    int kind,
    int dir,
    int ms
)
{
    if (neu == nullptr || newW < 1 || newH < 1)
    {
        if (neu != nullptr)
        {
            DeleteObject(neu);
        }
        return;
    }

    // Cut (or zero-ms): swap immediately. Any other kind runs the still-plane
    // xfade below so the user's BG transition actually plays.
    const bool toVideo = pendingVideo != nullptr && pendingVideo[0] != L'\0';
    const bool wantFade = kind != SLIDE_TRANSITION_NONE && ms > 0;
    if (toVideo && !wantFade)
    {
        StopBackgroundXfade();
        if (pendingIndex >= 0)
        {
            InstallLiveBackgroundFrame(neu, newW, newH, pendingIndex);
        }
        else
        {
            InstallLiveBackgroundMedia(neu, newW, newH, pendingVideo);
        }
        PresentCurrent();
        return;
    }

    if (wantFade)
    {
        int workW = 0;
        int workH = 0;
        BgXfadeWorkingSize(newW, newH, &workW, &workH);
        // Rapid retarget: keep whatever is already on glass as "from" so a new
        // click mid-fade does not punch a high-contrast poster over live video.
        const int planeBytes = workW * workH * 4;
        if (g_bgXfadeOn &&
            g_bgXfadeOutBits != nullptr &&
            g_bgXfadeFromBits != nullptr &&
            g_bgXfadeToBits != nullptr &&
            g_bgXfadeW == workW &&
            g_bgXfadeH == workH &&
            g_bgXfadeBitsCap >= planeBytes)
        {
            memcpy(
                g_bgXfadeFromBits,
                g_bgXfadeOutBits,
                static_cast<size_t>(planeBytes)
            );
            if (GrabBitmap32(neu, newW, newH, workW, workH, g_bgXfadeToBits))
            {
                DeleteObject(neu);
                g_bgXfadeKind = kind;
                g_bgXfadeDir = dir;
                g_bgXfadeMs = ms;
                g_bgXfadeStart = 0;
                g_bgXfadeClockArmed = false;
                g_bgXfadePendingIndex = pendingIndex;
                g_bgXfadePendingVideo[0] = L'\0';
                if (pendingVideo != nullptr && pendingVideo[0] != L'\0')
                {
                    lstrcpynW(g_bgXfadePendingVideo, pendingVideo, MAX_PATH);
                }
                else if (pendingIndex >= 0)
                {
                    Renderer_BackgroundVideoPath(g_bgXfadePendingVideo, MAX_PATH);
                }
                // Still-plane xfade must own the glass — holding outgoing GPU
                // video would hide the transition and look like a delayed cut.
                g_bgXfadeHoldOutgoingVideo = false;
                g_liveBgVideo[0] = L'\0';
                ComposeTransitionPixels(
                    g_bgXfadeKind,
                    g_bgXfadeDir,
                    0.0f,
                    g_bgXfadeFromBits,
                    g_bgXfadeToBits,
                    g_bgXfadeOutBits,
                    g_bgXfadeW,
                    g_bgXfadeH
                );
                SyncLiveBgFromBits(g_bgXfadeOutBits, g_bgXfadeW, g_bgXfadeH);
                g_liveBgHeld = true;
                g_backgroundActive = true;
                if (g_programWindow != nullptr)
                {
                    SetTimer(g_programWindow, ID_XFADE_TIMER, 16, nullptr);
                }
                PresentXfadeTick();
                return;
            }
        }

        StopBackgroundXfade();
        if (AllocBgXfadePlanes(workW, workH))
        {
            // Prefer the existing hold / still — never reopen the file with MF here
            // (that seek hitch was the lag on every BG switch).
            if (g_liveBgFrame == nullptr && g_liveBgStill != nullptr &&
                g_liveBgStillW > 0 && g_liveBgStillH > 0)
            {
                g_liveBgFrame = g_liveBgStill;
                g_liveBgW = g_liveBgStillW;
                g_liveBgH = g_liveBgStillH;
                g_liveBgStill = nullptr;
                g_liveBgStillW = 0;
                g_liveBgStillH = 0;
            }

            bool fromOk = false;
            if (g_liveBgFrame != nullptr && g_liveBgW > 0 && g_liveBgH > 0)
            {
                fromOk = GrabBitmap32(
                    g_liveBgFrame,
                    g_liveBgW,
                    g_liveBgH,
                    workW,
                    workH,
                    g_bgXfadeFromBits
                );
            }
            else
            {
                memset(
                    g_bgXfadeFromBits,
                    0,
                    static_cast<size_t>(workW) * static_cast<size_t>(workH) * 4
                );
                fromOk = true;
            }
            const bool toOk = GrabBitmap32(neu, newW, newH, workW, workH, g_bgXfadeToBits);
            if (fromOk && toOk)
            {
                DeleteObject(neu);
                g_bgXfadeOn = true;
                g_bgXfadeW = workW;
                g_bgXfadeH = workH;
                g_bgXfadeKind = kind;
                g_bgXfadeDir = dir;
                g_bgXfadeMs = ms;
                g_bgXfadeStart = 0;
                g_bgXfadeClockArmed = false;
                g_bgXfadePendingIndex = pendingIndex;
                g_bgXfadePendingVideo[0] = L'\0';
                if (pendingVideo != nullptr && pendingVideo[0] != L'\0')
                {
                    lstrcpynW(g_bgXfadePendingVideo, pendingVideo, MAX_PATH);
                }
                else if (pendingIndex >= 0)
                {
                    Renderer_BackgroundVideoPath(g_bgXfadePendingVideo, MAX_PATH);
                }
                // Paint the still-plane transition; PLAY the pending clip at Finish.
                g_bgXfadeHoldOutgoingVideo = false;
                g_liveBgVideo[0] = L'\0';
                if (g_liveBgStill != nullptr)
                {
                    DeleteObject(g_liveBgStill);
                    g_liveBgStill = nullptr;
                    g_liveBgStillW = 0;
                    g_liveBgStillH = 0;
                }
                ComposeTransitionPixels(
                    g_bgXfadeKind,
                    g_bgXfadeDir,
                    0.0f,
                    g_bgXfadeFromBits,
                    g_bgXfadeToBits,
                    g_bgXfadeOutBits,
                    g_bgXfadeW,
                    g_bgXfadeH
                );
                SyncLiveBgFromBits(g_bgXfadeOutBits, g_bgXfadeW, g_bgXfadeH);
                g_liveBgHeld = true;
                g_backgroundActive = true;
                if (g_programWindow != nullptr)
                {
                    SetTimer(g_programWindow, ID_XFADE_TIMER, 16, nullptr);
                }
                PresentXfadeTick();
                return;
            }
        }
    }

    StopBackgroundXfade();
    if (pendingIndex >= 0)
    {
        InstallLiveBackgroundFrame(neu, newW, newH, pendingIndex);
    }
    else
    {
        InstallLiveBackgroundMedia(neu, newW, newH, pendingVideo);
    }
    PresentCurrent();
}

static void FinishBackgroundXfade()
{
    const int pending = g_bgXfadePendingIndex;
    wchar_t pendingVideo[MAX_PATH] = {};
    lstrcpynW(pendingVideo, g_bgXfadePendingVideo, MAX_PATH);
    // Lock the destination plane onto the live frame before freeing xfade buffers,
    // so the stage never drops to black between the last tick and the final bed.
    if (g_bgXfadeToBits != nullptr && g_bgXfadeW > 0 && g_bgXfadeH > 0)
    {
        SyncLiveBgFromBits(g_bgXfadeToBits, g_bgXfadeW, g_bgXfadeH);
        g_liveBgHeld = true;
        g_backgroundActive = true;
    }
    StopBackgroundXfade();
    if (pending < 0)
    {
        // Media-tab bed: keep the synced poster and restore any pending video path.
        if (pendingVideo[0] != L'\0')
        {
            lstrcpynW(g_liveBgVideo, pendingVideo, MAX_PATH);
        }
        g_liveBgHeld = g_liveBgFrame != nullptr || g_liveBgVideo[0] != L'\0';
        g_backgroundActive = g_liveBgHeld;
        PresentCurrent();
        return;
    }

    wchar_t slideVideo[MAX_PATH] = {};
    Renderer_BackgroundVideoPath(slideVideo, MAX_PATH);
    if (slideVideo[0] != L'\0')
    {
        // to-bits already synced onto the hold — arm video without a second still render.
        lstrcpynW(g_liveBgVideo, slideVideo, MAX_PATH);
        g_liveBgHeld = g_liveBgFrame != nullptr || g_liveBgVideo[0] != L'\0';
        g_backgroundActive = g_liveBgHeld;
        PresentCurrent();
        return;
    }

    int bedW = 0;
    int bedH = 0;
    HBITMAP bed = Renderer_RenderBackgroundBed(pending, &bedW, &bedH);
    if (bed != nullptr && bedW > 0 && bedH > 0)
    {
        InstallLiveBackgroundFrame(bed, bedW, bedH, pending);
    }
    else if (bed != nullptr)
    {
        DeleteObject(bed);
    }
    // If bed render fails, keep the last synced xfade frame rather than clearing.
    g_liveBgHeld = g_liveBgFrame != nullptr || g_liveBgVideo[0] != L'\0';
    g_backgroundActive = g_liveBgHeld;
    PresentCurrent();
}

static void TickBackgroundXfade()
{
    if (!g_bgXfadeOn)
    {
        return;
    }
    if (!g_bgXfadeClockArmed)
    {
        g_bgXfadeStart = GetTickCount();
        g_bgXfadeClockArmed = true;
    }
    const float t = BackgroundXfadeT();
    if (t >= 1.0f)
    {
        FinishBackgroundXfade();
        return;
    }
    ComposeTransitionPixels(
        g_bgXfadeKind,
        g_bgXfadeDir,
        t,
        g_bgXfadeFromBits,
        g_bgXfadeToBits,
        g_bgXfadeOutBits,
        g_bgXfadeW,
        g_bgXfadeH
    );
    SyncLiveBgFromBits(g_bgXfadeOutBits, g_bgXfadeW, g_bgXfadeH);
    g_liveBgVideo[0] = L'\0';
    g_liveBgHeld = true;
    g_backgroundActive = true;
    // Same light present as slide xfade — PresentCurrent was re-laying out the
    // whole program UI every tick (~5fps).
    PresentXfadeTick();
}

static HBITMAP g_cachedBgStill = nullptr;
static int g_cachedBgStillIndex = -1;
static int g_cachedBgStillW = 0;
static int g_cachedBgStillH = 0;

static void ClearCachedBgStill()
{
    if (g_cachedBgStill != nullptr)
    {
        DeleteObject(g_cachedBgStill);
        g_cachedBgStill = nullptr;
    }
    g_cachedBgStillIndex = -1;
    g_cachedBgStillW = 0;
    g_cachedBgStillH = 0;
}

static void CaptureLiveBackground(int index)
{
    // Arm first so BackgroundVideoPath works for the pending target.
    Renderer_ArmBackground(index);

    int newW = 0;
    int newH = 0;
    wchar_t videoPath[MAX_PATH] = {};
    Renderer_BackgroundVideoPath(videoPath, MAX_PATH);
    const int transKind = Renderer_EffectiveSlideTransition(index);
    const int transMs = Renderer_EffectiveSlideTransitionMs(index);
    const bool wantFade = transKind != SLIDE_TRANSITION_NONE && transMs > 0;
    HBITMAP neu = nullptr;
    if (videoPath[0] != L'\0')
    {
        if (g_cachedBgStillIndex == index &&
            g_cachedBgStill != nullptr &&
            g_cachedBgStillW > 0 &&
            g_cachedBgStillH > 0)
        {
            neu = DuplicateBgBitmap(g_cachedBgStill, g_cachedBgStillW, g_cachedBgStillH);
            newW = g_cachedBgStillW;
            newH = g_cachedBgStillH;
        }
        else if (wantFade)
        {
            // Destination poster for the transition planes (uses already-loaded
            // element images — not an MF reopen). Cache for flip-backs.
            neu = Renderer_RenderBackgroundStill(index, &newW, &newH);
            if (neu != nullptr && newW > 0 && newH > 0)
            {
                ClearCachedBgStill();
                g_cachedBgStill = DuplicateBgBitmap(neu, newW, newH);
                if (g_cachedBgStill != nullptr)
                {
                    g_cachedBgStillIndex = index;
                    g_cachedBgStillW = newW;
                    g_cachedBgStillH = newH;
                }
            }
        }
        else if (g_liveBgFrame != nullptr && g_liveBgW > 0 && g_liveBgH > 0)
        {
            // Cut: keep current pixels as hold and swap PLAY immediately.
            neu = DuplicateBgBitmap(g_liveBgFrame, g_liveBgW, g_liveBgH);
            newW = g_liveBgW;
            newH = g_liveBgH;
        }
        else
        {
            Renderer_GetSlideSize(index, &newW, &newH);
            if (newW < 1 || newH < 1)
            {
                newW = 1920;
                newH = 1080;
            }
            neu = MakeBlankBgHold(newW, newH);
        }
    }
    else
    {
        neu = Renderer_RenderBackgroundBed(index, &newW, &newH);
    }

    if (neu == nullptr || newW < 1 || newH < 1)
    {
        // Keep whatever is already on stage — never blank the live BG on a failed render.
        if (neu != nullptr)
        {
            DeleteObject(neu);
        }
        return;
    }

    BeginBackgroundTransition(
        neu,
        newW,
        newH,
        index,
        videoPath,
        transKind,
        Renderer_EffectiveSlideTransitionDir(index),
        transMs
    );
}

static void BlitHoldFrame(HDC hdc, int x, int y, int width, int height, bool keyed)
{
    if (hdc == nullptr || g_liveBgFrame == nullptr || width <= 0 || height <= 0)
    {
        return;
    }
    const int srcW = g_liveBgW > 0 ? g_liveBgW : 1;
    const int srcH = g_liveBgH > 0 ? g_liveBgH : 1;
    if (keyed)
    {
        BlitAlphaOverlay(hdc, x, y, width, height, g_liveBgFrame, 0, 0, srcW, srcH);
        return;
    }
    HDC src = CreateCompatibleDC(hdc);
    if (src == nullptr)
    {
        return;
    }
    HBITMAP old = (HBITMAP)SelectObject(src, g_liveBgFrame);
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, x, y, width, height, src, 0, 0, srcW, srcH, SRCCOPY);
    SelectObject(src, old);
    DeleteDC(src);
}

static void HighlightSlide(int index)
{
    const int count = ProgramSlideCount();
    if (index < 0 || index >= count)
    {
        return;
    }
    ClearSlideSelection();
    g_slideSelected[index] = true;
    g_slideSelectAnchor = index;
    if (g_programView == PROGRAM_VIEW_EDIT)
    {
        EditView_OpenSlide(index);
    }
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
}

static int FirstHighlightedSlide()
{
    const int count = ProgramSlideCount();
    if (g_slideSelectAnchor >= 0 && g_slideSelectAnchor < count && g_slideSelected[g_slideSelectAnchor])
    {
        return g_slideSelectAnchor;
    }
    for (int i = 0; i < count; ++i)
    {
        if (g_slideSelected[i])
        {
            return i;
        }
    }
    const int current = Renderer_CurrentSlide();
    if (current >= 0 && current < count)
    {
        return current;
    }
    return count > 0 ? 0 : -1;
}

static int StepSlideIndex(int delta)
{
    const int count = ProgramSlideCount();
    if (count <= 0)
    {
        return -1;
    }
    int from = FirstHighlightedSlide();
    if (from < 0)
    {
        from = 0;
    }
    int next = from + delta;
    if (next < 0)
    {
        next = 0;
    }
    if (next >= count)
    {
        next = count - 1;
    }
    return next;
}

static void HighlightStep(int delta)
{
    const int next = StepSlideIndex(delta);
    if (next >= 0)
    {
        HighlightSlide(next);
    }
}

static void GoLiveStep(int delta)
{
    const int next = StepSlideIndex(delta);
    if (next >= 0)
    {
        SelectSlide(next);
    }
}

static int SlideGridCols()
{
    if (g_programMiddle == nullptr || g_programView != PROGRAM_VIEW_SHOW)
    {
        return 1;
    }
    RECT client = {};
    GetClientRect(g_programMiddle, &client);
    int cellW = 0;
    int cellH = 0;
    int cols = 1;
    int rows = 0;
    int contentH = 0;
    MeasureProgramSlideGrid(client.right, ProgramSlideCount(), &cellW, &cellH, &cols, &rows, &contentH);
    return cols > 0 ? cols : 1;
}

static int ArrowSlideDelta(WPARAM key)
{
    const int cols = SlideGridCols();
    if (key == VK_LEFT)
    {
        return -1;
    }
    if (key == VK_RIGHT)
    {
        return 1;
    }
    if (key == VK_UP)
    {
        return -cols;
    }
    if (key == VK_DOWN)
    {
        return cols;
    }
    return 0;
}

static void HighlightByArrow(WPARAM key)
{
    HighlightStep(ArrowSlideDelta(key));
}

static void GoLiveByArrow(WPARAM key)
{
    GoLiveStep(ArrowSlideDelta(key));
}

static void HighlightFirstLast(bool first)
{
    const int count = ProgramSlideCount();
    if (count <= 0)
    {
        return;
    }
    HighlightSlide(first ? 0 : count - 1);
}

static void GoLiveFirstLast(bool first)
{
    const int count = ProgramSlideCount();
    if (count <= 0)
    {
        return;
    }
    SelectSlide(first ? 0 : count - 1);
}

static void SelectAllSlides()
{
    const int count = ProgramSlideCount();
    ClearSlideSelection();
    for (int i = 0; i < count; ++i)
    {
        g_slideSelected[i] = true;
    }
    if (count > 0)
    {
        g_slideSelectAnchor = 0;
    }
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
    if (g_programView == PROGRAM_VIEW_EDIT)
    {
        EditView_Refresh();
    }
}

static bool ConfirmDeleteSlides(int count)
{
    if (count <= 1 || Store_GetInt("general.confirm_delete", 1) == 0)
    {
        return true;
    }
    char message[128] = {};
    wsprintfA(message, "Delete %d slides?", count);
    const int answer = MessageBoxA(
        g_programWindow != nullptr ? g_programWindow : g_mainWindow,
        message,
        "Delete Slides",
        MB_OKCANCEL | MB_ICONWARNING
    );
    return answer == IDOK;
}

static void CopySelectedSlides(bool cut)
{
    int indices[PROGRAM_SLIDE_MAX] = {};
    const int count = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
    if (count <= 0)
    {
        return;
    }
    if (cut && !ConfirmDeleteSlides(count))
    {
        return;
    }
    if (cut)
    {
        History_Remember();
    }
    if (!Renderer_CopySlides(indices, count))
    {
        return;
    }
    g_slideClipboardCut = cut;
    if (cut)
    {
        if (Renderer_DeleteSlides(indices, count))
        {
            ClearSlideSelection();
            const int keep = Renderer_CurrentSlide();
            if (keep >= 0 && keep < PROGRAM_SLIDE_MAX)
            {
                g_slideSelected[keep] = true;
                g_slideSelectAnchor = keep;
            }
            Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
            RefreshSlideLists();
        }
    }
}

static void PasteSlideClipboard()
{
    if (!Renderer_HasSlideClipboard())
    {
        return;
    }
    History_Remember();
    int insertAt = Renderer_SlideCount();
    int indices[PROGRAM_SLIDE_MAX] = {};
    const int count = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
    if (count > 0)
    {
        insertAt = indices[count - 1] + 1;
    }
    if (!Renderer_PasteSlides(insertAt))
    {
        return;
    }
    if (g_slideClipboardCut)
    {
        g_slideClipboardCut = false;
    }
    ClearSlideSelection();
    const int pasted = Renderer_SlideCount() - insertAt;
    for (int i = 0; i < pasted && insertAt + i < PROGRAM_SLIDE_MAX; ++i)
    {
        g_slideSelected[insertAt + i] = true;
    }
    g_slideSelectAnchor = insertAt;
    if (g_programView == PROGRAM_VIEW_EDIT)
    {
        EditView_OpenSlide(insertAt);
    }
    Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    RefreshSlideLists();
}

static void DeleteSelectedSlides()
{
    int indices[PROGRAM_SLIDE_MAX] = {};
    const int count = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
    if (count <= 0)
    {
        return;
    }
    if (!ConfirmDeleteSlides(count))
    {
        return;
    }
    History_Remember();
    if (!Renderer_DeleteSlides(indices, count))
    {
        return;
    }
    ClearSlideSelection();
    const int keep = Renderer_CurrentSlide();
    if (keep >= 0 && keep < PROGRAM_SLIDE_MAX)
    {
        g_slideSelected[keep] = true;
        g_slideSelectAnchor = keep;
    }
    Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    RefreshSlideLists();
}

static void DuplicateSelectedSlides()
{
    int indices[PROGRAM_SLIDE_MAX] = {};
    const int count = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
    if (count <= 0)
    {
        const int one = FirstHighlightedSlide();
        if (one < 0)
        {
            return;
        }
        indices[0] = one;
    }
    const int copyCount = count > 0 ? count : 1;
    History_Remember();
    if (!Renderer_CopySlides(indices, copyCount))
    {
        return;
    }
    const int insertAt = indices[copyCount - 1] + 1;
    if (!Renderer_PasteSlides(insertAt))
    {
        return;
    }
    ClearSlideSelection();
    for (int i = 0; i < copyCount && insertAt + i < PROGRAM_SLIDE_MAX; ++i)
    {
        g_slideSelected[insertAt + i] = true;
    }
    g_slideSelectAnchor = insertAt;
    if (g_programView == PROGRAM_VIEW_EDIT)
    {
        EditView_OpenSlide(insertAt);
    }
    Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    RefreshSlideLists();
}

static void ProgramCut()
{
    if (g_programView == PROGRAM_VIEW_EDIT && EditView_HasSelection())
    {
        EditView_CutSelected();
        return;
    }
    CopySelectedSlides(true);
}

static void ProgramCopy()
{
    if (g_programView == PROGRAM_VIEW_EDIT && EditView_HasSelection())
    {
        EditView_CopySelected();
        return;
    }
    CopySelectedSlides(false);
}

static void ProgramPaste()
{
    if (g_programView == PROGRAM_VIEW_EDIT && Renderer_FreeformHasClipboard())
    {
        EditView_Paste();
        return;
    }
    PasteSlideClipboard();
}

static void ProgramDuplicate()
{
    if (g_programView == PROGRAM_VIEW_EDIT && EditView_HasSelection())
    {
        EditView_DuplicateSelected();
        return;
    }
    DuplicateSelectedSlides();
}

static void ProgramDelete()
{
    if (g_programView == PROGRAM_VIEW_EDIT && EditView_HasSelection())
    {
        EditView_DeleteSelected();
        return;
    }
    DeleteSelectedSlides();
}

static void UseSelectedAsSlide()
{
    int indices[PROGRAM_SLIDE_MAX] = {};
    if (SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX) != 1)
    {
        return;
    }
    History_Remember();
    if (!Renderer_UseAsSlide(indices[0]))
    {
        return;
    }
    Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    RefreshSlideLists();
}

static void UseSelectedAsBackground()
{
    int indices[PROGRAM_SLIDE_MAX] = {};
    if (SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX) != 1)
    {
        return;
    }
    History_Remember();
    if (!Renderer_UseAsBackground(indices[0]))
    {
        return;
    }
    Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    RefreshSlideLists();
}

static void EditSelectedSlide()
{
    int indices[PROGRAM_SLIDE_MAX] = {};
    if (SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX) != 1)
    {
        const int one = FirstHighlightedSlide();
        if (one < 0)
        {
            return;
        }
        indices[0] = one;
    }
    SetProgramView(PROGRAM_VIEW_EDIT);
    EditView_OpenSlide(indices[0]);
}

static void RestoreAfterHistory()
{
    RefreshAfterDocumentChange();
    const int current = Renderer_CurrentSlide();
    if (current >= 0)
    {
        HighlightSlide(current);
    }
    if (g_programView == PROGRAM_VIEW_EDIT)
    {
        EditView_OpenSlide(Renderer_EditSlide());
    }
}

static bool HandleContextMenuCommand(int id)
{
    const int target = g_contextTarget;
    g_contextTarget = CTX_NONE;
    if (target == CTX_TRACKS)
    {
        switch (id)
        {
            case IDM_CTX_CUT:
                CopySelectedTracks(true);
                return true;
            case IDM_CTX_COPY:
                CopySelectedTracks(false);
                return true;
            case IDM_CTX_PASTE:
                PasteTrackClipboard();
                return true;
            case IDM_CTX_DELETE:
                DeleteSelectedTracks();
                return true;
        }
        return false;
    }
    if (target == CTX_SLIDES)
    {
        switch (id)
        {
            case IDM_CTX_EDIT_SLIDE:
                EditSelectedSlide();
                return true;
            case IDM_CTX_USE_AS_SLIDE:
                UseSelectedAsSlide();
                return true;
            case IDM_CTX_USE_AS_BG:
                UseSelectedAsBackground();
                return true;
            case IDM_CTX_TRANSITIONS:
            {
                int pick[PROGRAM_SLIDE_MAX] = {};
                if (SelectedSlideIndices(pick, PROGRAM_SLIDE_MAX) == 1)
                {
                    ShowTransitionGenie(pick[0]);
                }
                return true;
            }
            case IDM_CTX_CUT:
                CopySelectedSlides(true);
                return true;
            case IDM_CTX_COPY:
                CopySelectedSlides(false);
                return true;
            case IDM_CTX_PASTE:
                PasteSlideClipboard();
                return true;
            case IDM_CTX_DELETE:
                DeleteSelectedSlides();
                return true;
        }
    }
    return false;
}

static void PlaySelectedAudioTrack()
{
    if (g_audioTracks == nullptr)
    {
        return;
    }
    int ids[1] = {};
    if (SelectedTrackIds(ids, 1) != 1)
    {
        const int sel = static_cast<int>(SendMessageA(g_audioTracks, LB_GETCURSEL, 0, 0));
        if (sel < 0)
        {
            return;
        }
        ids[0] = static_cast<int>(SendMessageA(g_audioTracks, LB_GETITEMDATA, sel, 0));
    }
    const int trackId = ids[0];
    if (trackId <= 0)
    {
        return;
    }

    if (Audio_PlayingTrackId() == trackId && Audio_HasMedia())
    {
        if (Audio_IsPlaying())
        {
            Audio_Pause();
        }
        else
        {
            Audio_Resume();
        }
    }
    else if (!Audio_PlayTrack(trackId))
    {
        char detail[256] = {};
        Audio_LastError(detail, sizeof(detail));
        char text[512] = {};
        wsprintfA(
            text,
            "Could not play that audio file.%s%s",
            detail[0] != '\0' ? "\n\n" : "",
            detail
        );
        MessageBoxA(g_programWindow, text, "Audio", MB_OK | MB_ICONWARNING);
        return;
    }

    g_mediaPanel = MEDIA_PANEL_AUDIO;
    LayoutProgramPreview();
    if (g_programClearAudioBtn != nullptr)
    {
        InvalidateRect(g_programClearAudioBtn, nullptr, FALSE);
    }
    UpdateMediaTransportUi();
}

LRESULT CALLBACK MediaScrubProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    const int id = GetDlgCtrlID(hwnd);
    const bool audioScrub = (id == ID_MEDIA_AUDIO_SCRUB);

    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_LBUTTONDOWN:
        {
            SetCapture(hwnd);
            if (audioScrub)
            {
                g_audioScrubDragging = true;
                SeekAudioFromRatio(ScrubRatioFromPoint(hwnd, lParam), true);
            }
            else
            {
                g_videoScrubDragging = true;
                SeekVideoFromRatio(ScrubRatioFromPoint(hwnd, lParam), true);
            }
            UpdateMediaTransportUi();
            return 0;
        }

        case WM_MOUSEMOVE:
            if (audioScrub)
            {
                if (!g_audioScrubDragging)
                {
                    break;
                }
                SeekAudioFromRatio(ScrubRatioFromPoint(hwnd, lParam), false);
                UpdateMediaTransportUi();
                return 0;
            }
            if (!g_videoScrubDragging)
            {
                break;
            }
            SeekVideoFromRatio(ScrubRatioFromPoint(hwnd, lParam), false);
            UpdateMediaTransportUi();
            return 0;

        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            if (audioScrub)
            {
                if (g_audioScrubDragging)
                {
                    if (uMsg == WM_LBUTTONUP)
                    {
                        SeekAudioFromRatio(ScrubRatioFromPoint(hwnd, lParam), true);
                        if (GetCapture() == hwnd)
                        {
                            ReleaseCapture();
                        }
                    }
                    g_audioScrubDragging = false;
                    UpdateMediaTransportUi();
                }
                return 0;
            }
            if (g_videoScrubDragging)
            {
                if (uMsg == WM_LBUTTONUP)
                {
                    SeekVideoFromRatio(ScrubRatioFromPoint(hwnd, lParam), true);
                    if (GetCapture() == hwnd)
                    {
                        ReleaseCapture();
                    }
                }
                g_videoScrubDragging = false;
                UpdateMediaTransportUi();
            }
            return 0;

        case WM_RBUTTONUP:
            if (audioScrub)
            {
                return 0;
            }
            {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ClientToScreen(hwnd, &pt);
                HMENU menu = CreatePopupMenu();
                AppendMenuA(
                    menu,
                    MF_STRING | (!g_showRemaining ? MF_CHECKED : 0),
                    IDM_SCRUB_DURATION,
                    "Show duration"
                );
                AppendMenuA(
                    menu,
                    MF_STRING | (g_showRemaining ? MF_CHECKED : 0),
                    IDM_SCRUB_REMAINING,
                    "Show time remaining"
                );
                const int cmd = TrackPopupMenu(
                    menu,
                    TPM_RETURNCMD | TPM_RIGHTBUTTON,
                    pt.x,
                    pt.y,
                    0,
                    hwnd,
                    nullptr
                );
                DestroyMenu(menu);
                if (cmd == IDM_SCRUB_DURATION || cmd == IDM_SCRUB_REMAINING)
                {
                    g_showRemaining = (cmd == IDM_SCRUB_REMAINING);
                    if (Store_Ready())
                    {
                        Store_SetInt("playback.show_remaining", g_showRemaining ? 1 : 0);
                    }
                    UpdateMediaTransportUi();
                }
            }
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);

            double ratio = audioScrub ? g_audioScrubRatio : g_videoScrubRatio;
            if (ratio < 0.0)
            {
                ratio = 0.0;
            }
            if (ratio > 1.0)
            {
                ratio = 1.0;
            }

            const int w = max(1, (int)client.right);
            const int h = max(1, (int)client.bottom);
            HDC memory = CreateCompatibleDC(hdc);
            HBITMAP canvas = CreateCompatibleBitmap(hdc, w, h);
            HBITMAP oldCanvas = (HBITMAP)SelectObject(memory, canvas);
            HBRUSH back = CreateSolidBrush(LOOK_COLOR_SURFACE);
            FillRect(memory, &client, back);
            DeleteObject(back);
            {
                Gdiplus::Graphics g(memory);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                const float r = (float)PROGRAM_SCRUB_KNOB_R;
                const float cy = h / 2.0f;
                const float trackL = r;
                const float trackW = max(0.0f, w - (r * 2.0f));
                const float knobX = trackL + (float)(trackW * ratio);
                const float barH = (float)PROGRAM_SCRUB_H;
                const float barTop = cy - (barH / 2.0f);
                auto pill = [&](float left, float right, COLORREF color)
                {
                    if (right - left < 1.0f)
                    {
                        return;
                    }
                    Gdiplus::GraphicsPath path;
                    const float d = min(barH, right - left);
                    path.AddArc(left, barTop, d, barH, 90.0f, 180.0f);
                    path.AddArc(right - d, barTop, d, barH, 270.0f, 180.0f);
                    path.CloseFigure();
                    Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
                    g.FillPath(&brush, &path);
                };
                pill(trackL - (barH / 2.0f), trackL + trackW + (barH / 2.0f), LOOK_COLOR_SUNKEN);
                pill(trackL - (barH / 2.0f), knobX, audioScrub ? PROGRAM_COLOR_WELL_LIGHT : PROGRAM_COLOR_SELECT);
                Gdiplus::SolidBrush ring(Gdiplus::Color(255, 0, 0, 0));
                g.FillEllipse(&ring, knobX - r, cy - r, r * 2.0f, r * 2.0f);
                const float inner = r - 3.0f;
                Gdiplus::SolidBrush center(Gdiplus::Color(255, 0xC8, 0xC8, 0xC8));
                g.FillEllipse(&center, knobX - inner, cy - inner, inner * 2.0f, inner * 2.0f);
            }
            BitBlt(hdc, 0, 0, w, h, memory, 0, 0, SRCCOPY);
            SelectObject(memory, oldCanvas);
            DeleteObject(canvas);
            DeleteDC(memory);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void ShowScenesMenu(HWND button)
{
    LoadLookState();
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }
    if (g_lookCount == 0)
    {
        AppendMenuA(menu, MF_STRING | MF_GRAYED, 0, "No scenes");
    }
    for (int i = 0; i < g_lookCount; ++i)
    {
        AppendMenuA(menu, MF_STRING | (i == g_appliedLook ? MF_CHECKED : 0), 1 + i, g_lookNames[i]);
    }
    AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(menu, MF_STRING, LOOK_MAX + 1, "Edit scenes...");

    RECT rc = {};
    GetWindowRect(button, &rc);
    g_scenesMenuOpen = true;
    InvalidateRect(button, nullptr, FALSE);
    UpdateWindow(button);
    const int cmd = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
        rc.right,
        rc.bottom + 2,
        0,
        button,
        nullptr
    );
    DestroyMenu(menu);
    g_scenesMenuOpen = false;
    InvalidateRect(button, nullptr, FALSE);
    if (cmd >= 1 && cmd <= g_lookCount)
    {
        ApplyLook(cmd - 1);
    }
    else if (cmd == LOOK_MAX + 1)
    {
        ShowScenesWindow();
    }
}

// The skip SVGs have 10 and 30 drawn in; other amounts redraw the arrow with the number as text.
static void DrawSkipIcon(HDC hdc, int cx, int cy, int size, bool back, int seconds, COLORREF color)
{
    if (seconds == (back ? 10 : 30))
    {
        SvgIcon_Draw(hdc, back ? "skip_back_10" : "skip_forward_30", cx, cy, size, color);
        return;
    }
    const float s = size / 24.0f;
    const float ox = cx - size / 2.0f;
    const float oy = cy - size / 2.0f;
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)), 1.5f * s);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    const float r = 8.5f * s;
    g.DrawArc(&pen, ox + 12.0f * s - r, oy + 13.0f * s - r, r * 2.0f, r * 2.0f, 270.0f, back ? 300.0f : -300.0f);
    const float tipX = ox + 12.0f * s;
    const float tailX = ox + (back ? 14.5f : 9.5f) * s;
    Gdiplus::PointF arrow[3] = {
        Gdiplus::PointF(tailX, oy + 2.0f * s),
        Gdiplus::PointF(tipX, oy + 4.5f * s),
        Gdiplus::PointF(tailX, oy + 7.0f * s)
    };
    g.DrawLines(&pen, arrow, 3);

    static HFONT font = nullptr;
    if (font == nullptr)
    {
        font = CreateFontA(-8, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
    char text[8] = {};
    wsprintfA(text, "%d", seconds);
    RECT rc = { cx - size / 2, (int)(oy + 13.0f * s) - 6, cx + size / 2, (int)(oy + 13.0f * s) + 6 };
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static void PaintMediaChromeButton(HWND hwnd, HDC hdc, const RECT& client)
{
    const int id = GetDlgCtrlID(hwnd);
    const bool panelActive =
        (id == ID_MEDIA_VIDEO_BTN && g_mediaPanel == MEDIA_PANEL_VIDEO) ||
        (id == ID_MEDIA_AUDIO_BTN && g_mediaPanel == MEDIA_PANEL_AUDIO) ||
        (id == ID_QUICK_SLIDES && g_quickSlidesWindow != nullptr && IsWindowVisible(g_quickSlidesWindow)) ||
        (id == ID_TEMPLATES_BTN && g_templatePanel != nullptr) ||
        (id == ID_SCENES_BTN && g_scenesMenuOpen);
    bool live = false;
    if (id == ID_MEDIA_CLEAR_SLIDE)
    {
        live = g_liveArmed;
    }
    else if (id == ID_MEDIA_CLEAR_BG)
    {
        live = g_backgroundActive;
    }
    else if (id == ID_AUDIO_CLEAR)
    {
        live = Audio_HasMedia();
    }
    COLORREF fillColor = PROGRAM_COLOR_WELL_DARK;
    if (live)
    {
        fillColor = PROGRAM_COLOR_LIVE;
    }
    else if (panelActive)
    {
        fillColor = RGB(0x40, 0x40, 0x40);
    }
    const bool hover = g_mediaChromeHover == hwnd;
    const int cx = (client.left + client.right) / 2;
    const int cy = (client.top + client.bottom) / 2;
    SetBkMode(hdc, TRANSPARENT);
    if (id == ID_MEDIA_CLEAR_SLIDE || id == ID_MEDIA_CLEAR_BG || id == ID_AUDIO_CLEAR || id == ID_TEMPLATES_BTN ||
        id == ID_SCENES_BTN || id == ID_DISPLAYS_BTN || id == ID_QUICK_SLIDES)
    {
        PaintProgramTileBack(hdc, &client, panelActive ? PROGRAM_TILE_ON : (hover ? PROGRAM_TILE_HOVER : PROGRAM_TILE_IDLE));
        const COLORREF iconInk = live ? PROGRAM_COLOR_LIVE_INK : (panelActive ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
        const COLORREF textInk = live ? PROGRAM_COLOR_DANGER_INK : (panelActive ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
        const char* icon = id == ID_MEDIA_CLEAR_SLIDE ? "clear_slide"
            : (id == ID_MEDIA_CLEAR_BG ? "clear_background"
            : (id == ID_AUDIO_CLEAR ? "clear_audio"
            : (id == ID_SCENES_BTN ? "scenes"
            : (id == ID_QUICK_SLIDES ? "quick_slides"
            : (id == ID_DISPLAYS_BTN ? (ManageScreens_OutputsEnabled() ? "2" : "2a") : "templates")))));
        if (id == ID_DISPLAYS_BTN)
        {
            SvgIcon_Draw(hdc, icon, cx, cy, 30, ManageScreens_OutputsEnabled() ? RGB(0x89, 0xCF, 0xF0) : LOOK_COLOR_INK);
            return;
        }
        SvgIcon_Draw(hdc, icon, cx, client.top + PROGRAM_TILE_ICON_Y, PROGRAM_TILE_ICON, iconInk);
        char text[LOOK_NAME_MAX] = {};
        if (id == ID_SCENES_BTN)
        {
            LoadLookState();
            if (g_lookCount > 0 && g_appliedLook >= 0 && g_appliedLook < g_lookCount)
            {
                lstrcpynA(text, g_lookNames[g_appliedLook], sizeof(text));
            }
            else
            {
                lstrcpyA(text, "No scenes");
            }
        }
        else
        {
            GetWindowTextA(hwnd, text, sizeof(text));
        }
        PaintProgramTileLabel(hdc, &client, text, textInk);
        return;
    }
    if (id == ID_MEDIA_VIDEO_BTN || id == ID_MEDIA_AUDIO_BTN)
    {
        HBRUSH back = CreateSolidBrush(LOOK_COLOR_SUNKEN);
        FillRect(hdc, &client, back);
        DeleteObject(back);
        if (panelActive)
        {
            SceneRound(hdc, &client, 4.0f, LOOK_COLOR_RAISED_HI);
        }
        else if (hover)
        {
            SceneRound(hdc, &client, 4.0f, RGB(0x2A, 0x2A, 0x2A));
        }
        HFONT old = (HFONT)SelectObject(hdc, ProgramFont(PROGRAM_FONT_CAPTION));
        SetTextColor(hdc, panelActive ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK_DIM);
        RECT textRc = client;
        DrawTextA(hdc, id == ID_MEDIA_VIDEO_BTN ? "Video" : "Audio", -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        return;
    }
    if (id == ID_AUDIO_NEW_PLAYLIST)
    {
        HBRUSH back = CreateSolidBrush(LOOK_COLOR_SURFACE);
        FillRect(hdc, &client, back);
        DeleteObject(back);
        SceneRound(hdc, &client, 6.0f, hover ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
        char text[32] = {};
        GetWindowTextA(hwnd, text, sizeof(text));
        HFONT old = (HFONT)SelectObject(hdc, ProgramUiFont());
        SetTextColor(hdc, LOOK_COLOR_INK_BRIGHT);
        RECT textRc = client;
        DrawTextA(hdc, text, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, old);
        return;
    }
    if (id == ID_AUDIO_LOOP || id == ID_AUDIO_SHUFFLE)
    {
        HBRUSH back = CreateSolidBrush(LOOK_COLOR_SURFACE);
        FillRect(hdc, &client, back);
        DeleteObject(back);
        const bool on = (id == ID_AUDIO_LOOP && Audio_Loop()) || (id == ID_AUDIO_SHUFFLE && Audio_Shuffle());
        SceneRound(
            hdc,
            &client,
            6.0f,
            on || hover ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED,
            on ? LOOK_COLOR_ACCENT : PROGRAM_COLOR_CONTROL_EDGE
        );
        const int cx = (client.left + client.right) / 2;
        const int cy = (client.top + client.bottom) / 2;
        const COLORREF ink = on ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK;
        SvgIcon_Draw(hdc, id == ID_AUDIO_LOOP ? "loop" : "shuffle", cx, cy, 16, ink);
        return;
    }
    const bool transport =
        id == ID_MEDIA_VIDEO_PLAY || id == ID_MEDIA_AUDIO_PLAY ||
        id == ID_MEDIA_VIDEO_BACK || id == ID_MEDIA_AUDIO_BACK ||
        id == ID_MEDIA_VIDEO_FWD || id == ID_MEDIA_AUDIO_FWD;
    if (transport)
    {
        HBRUSH back = CreateSolidBrush(LOOK_COLOR_SURFACE);
        FillRect(hdc, &client, back);
        DeleteObject(back);
        const bool play = id == ID_MEDIA_VIDEO_PLAY || id == ID_MEDIA_AUDIO_PLAY;
        if (play || hover)
        {
            SceneRound(hdc, &client, 6.0f, hover ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED);
        }
    }
    else
    {
        HBRUSH fill = CreateSolidBrush(fillColor);
        FillRect(hdc, &client, fill);
        DeleteObject(fill);
    }
    SetTextColor(hdc, live ? PROGRAM_COLOR_SELECT : PROGRAM_COLOR_INK);
    if (id == ID_MEDIA_VIDEO_PLAY || id == ID_MEDIA_AUDIO_PLAY)
    {
        bool playing = false;
        if (id == ID_MEDIA_VIDEO_PLAY)
        {
            GpuPlaybackStatus status = {};
            playing = GpuClient_GetStatus(&status) && status.hasMedia != 0 && status.playing != 0;
        }
        else
        {
            playing = Audio_IsPlaying();
        }
        if (playing)
        {
            if (!SvgIcon_Draw(hdc, "pause", cx, cy, 16, LOOK_COLOR_INK_BRIGHT))
            {
                DrawMediaPauseIcon(hdc, cx, cy, LOOK_COLOR_INK_BRIGHT);
            }
        }
        else if (!SvgIcon_Draw(hdc, "play", cx, cy, 16, LOOK_COLOR_INK))
        {
            DrawMediaPlayIcon(hdc, cx, cy, LOOK_COLOR_INK);
        }
    }
    else if (id == ID_MEDIA_VIDEO_BACK || id == ID_MEDIA_AUDIO_BACK)
    {
        DrawSkipIcon(hdc, cx, cy, 19, true, SkipBackSeconds(), LOOK_COLOR_INK);
    }
    else if (id == ID_MEDIA_VIDEO_FWD || id == ID_MEDIA_AUDIO_FWD)
    {
        DrawSkipIcon(hdc, cx, cy, 19, false, SkipForwardSeconds(), LOOK_COLOR_INK);
    }
    else
    {
        HFONT old = (HFONT)SelectObject(hdc, ProgramUiFont());
        char text[32] = {};
        GetWindowTextA(hwnd, text, sizeof(text));
        RECT textRc = client;
        DrawTextA(hdc, text, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
    }
}

LRESULT CALLBACK MediaChromeButtonProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
            if (g_mediaChromeHover != hwnd)
            {
                if (g_mediaChromeHover != nullptr)
                {
                    InvalidateRect(g_mediaChromeHover, nullptr, FALSE);
                }
                g_mediaChromeHover = hwnd;
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_MOUSELEAVE:
            if (g_mediaChromeHover == hwnd)
            {
                g_mediaChromeHover = nullptr;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_DESTROY:
            if (g_mediaChromeHover == hwnd)
            {
                g_mediaChromeHover = nullptr;
            }
            break;

        case WM_LBUTTONUP:
        {
            const int id = GetDlgCtrlID(hwnd);
            if (id == ID_MEDIA_VIDEO_BTN)
            {
                g_mediaPanel = MEDIA_PANEL_VIDEO;
            }
            else if (id == ID_MEDIA_AUDIO_BTN)
            {
                g_mediaPanel = MEDIA_PANEL_AUDIO;
            }
            else if (id == ID_MEDIA_VIDEO_PLAY)
            {
                GpuPlaybackStatus status = {};
                if (GpuClient_GetStatus(&status) && status.playing)
                {
                    GpuClient_Pause();
                }
                else
                {
                    if (Renderer_ArmedBackground() >= 0)
                    {
                        g_backgroundActive = true;
                        g_videoBackgroundCleared = false;
                    }
                    PresentCurrent();
                    GpuClient_Resume();
                }
                UpdateMediaTransportUi();
            }
            else if (id == ID_MEDIA_VIDEO_BACK || id == ID_MEDIA_VIDEO_FWD)
            {
                SkipVideoSeconds(id == ID_MEDIA_VIDEO_BACK ? -(double)SkipBackSeconds() : (double)SkipForwardSeconds());
                UpdateMediaTransportUi();
                return 0;
            }
            else if (id == ID_MEDIA_AUDIO_BACK || id == ID_MEDIA_AUDIO_FWD)
            {
                SkipAudioSeconds(id == ID_MEDIA_AUDIO_BACK ? -(double)SkipBackSeconds() : (double)SkipForwardSeconds());
                UpdateMediaTransportUi();
                return 0;
            }
            else if (id == ID_MEDIA_CLEAR_SLIDE)
            {
                EventLog("UI Clear slide clicked");
                ClearLiveSlide();
                EventLog("UI Clear slide done");
            }
            else if (id == ID_MEDIA_CLEAR_BG)
            {
                EventLog("UI Clear background clicked");
                ClearVideoBackground();
                EventLog("UI Clear background done");
            }
            else if (id == ID_AUDIO_CLEAR)
            {
                EventLog("UI Clear audio clicked");
                Audio_ClearLayer();
                g_audioScrubRatio = 0.0;
                if (g_programClearAudioBtn != nullptr)
                {
                    InvalidateRect(g_programClearAudioBtn, nullptr, FALSE);
                }
                UpdateMediaTransportUi();
            }
            else if (id == ID_QUICK_SLIDES)
            {
                ShowQuickSlides();
            }
            else if (id == ID_TEMPLATES_BTN)
            {
                ToggleTemplatePanel();
                return 0;
            }
            else if (id == ID_SCENES_BTN)
            {
                ShowScenesMenu(hwnd);
                return 0;
            }
            else if (id == ID_DISPLAYS_BTN)
            {
                ManageScreens_SetOutputsEnabled(!ManageScreens_OutputsEnabled());
                EventLog(ManageScreens_OutputsEnabled() ? "UI Displays on" : "UI Displays off");
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            else if (id == ID_AUDIO_NEW_PLAYLIST)
            {
                char name[64] = {};
                const int count = Audio_ListPlaylists(nullptr, 0);
                wsprintfA(name, "Playlist %d", count + 1);
                const int idNew = Audio_AddPlaylist(name);
                if (idNew > 0)
                {
                    Audio_SetActivePlaylistId(idNew);
                    RefreshAudioLists();
                }
            }
            else if (id == ID_AUDIO_LOOP)
            {
                Audio_SetLoop(!Audio_Loop());
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else if (id == ID_AUDIO_SHUFFLE)
            {
                Audio_SetShuffle(!Audio_Shuffle());
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else if (id == ID_MEDIA_AUDIO_PLAY)
            {
                if (Audio_HasMedia())
                {
                    if (Audio_IsPlaying())
                    {
                        Audio_Pause();
                    }
                    else
                    {
                        Audio_Resume();
                    }
                }
                else
                {
                    // Nothing loaded (cold start or after Clear audio):
                    // start whatever track is highlighted in the rail.
                    PlaySelectedAudioTrack();
                }
                UpdateMediaTransportUi();
            }
            LayoutProgramPreview();
            if (g_programLeft != nullptr)
            {
                InvalidateRect(g_programLeft, nullptr, FALSE);
            }
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HDC memory = CreateCompatibleDC(hdc);
            HBITMAP canvas = CreateCompatibleBitmap(hdc, max(1, (int)client.right), max(1, (int)client.bottom));
            HBITMAP oldCanvas = (HBITMAP)SelectObject(memory, canvas);
            PaintMediaChromeButton(hwnd, memory, client);
            BitBlt(hdc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
            SelectObject(memory, oldCanvas);
            DeleteObject(canvas);
            DeleteDC(memory);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void BlackOutHwnd(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return;
    }
    PaintWindowBlack(hwnd);
}

static bool BackgroundLayerOn()
{
    return g_backgroundActive && g_liveBgHeld;
}

static int PreviewScreenIndex()
{
    const int count = ManageScreens_ScreenCount();
    if (g_previewScreen >= 0 && g_previewScreen < count)
    {
        return g_previewScreen;
    }
    int active = ManageScreens_ActiveScreen();
    if (active >= 0 && active < count)
    {
        return active;
    }
    return 0;
}

static bool PreviewSplitBySeat()
{
    if (g_previewSlot >= 0)
    {
        return false;
    }
    const int screen = PreviewScreenIndex();
    const int seats = ManageScreens_GetScreenOutputCount(screen);
    if (seats <= 1)
    {
        return false;
    }
    return ManageScreens_GetScreenLayout(screen) == SCREEN_LAYOUT_SPAN;
}

static void ReadPreviewComboSelection()
{
    if (g_programMirror == nullptr)
    {
        return;
    }
    const int sel = (int)SendMessage(g_programMirror, CB_GETCURSEL, 0, 0);
    if (sel < 0)
    {
        return;
    }
    const LPARAM data = SendMessage(g_programMirror, CB_GETITEMDATA, sel, 0);
    g_previewScreen = (int)LOWORD(data);
    g_previewSlot = HIWORD(data) > 0 ? (int)HIWORD(data) - 1 : -1;
}

static void ApplyPreviewComboChange()
{
    ReadPreviewComboSelection();
    LayoutProgramPreview();
    LiveOutput_Push();
}

static int FindLookDisplay(int screen, int slot)
{
    for (int i = 0; i < g_lookDisplayCount; ++i)
    {
        if (g_lookDisplays[i].ghost || g_lookDisplays[i].screen != screen)
        {
            continue;
        }
        if (g_lookDisplays[i].mirror || g_lookDisplays[i].slot == slot)
        {
            return i;
        }
    }
    return -1;
}

static void LookSeatRect(int screen, int slot, int canvasW, int canvasH, RECT* rc);
static void MapRectToCompose(const RECT* src, int srcW, int srcH, int composeW, int composeH, RECT* out);

static void LookOutputRects(
    int look,
    int screen,
    int slot,
    int composeW,
    int composeH,
    RECT* videoSrc,
    RECT* overlaySrc,
    bool* bgChecked,
    bool* slideChecked
)
{
    int screenW = composeW;
    int screenH = composeH;
    ManageScreens_GetScreenCanvas(screen, &screenW, &screenH, nullptr);
    RECT seat = {};
    LookSeatRect(screen, slot, screenW, screenH, &seat);
    RECT screenFull = { 0, 0, screenW, screenH };
    const bool spanScreen = ManageScreens_GetScreenLayout(screen) == SCREEN_LAYOUT_SPAN;
    const bool hasLook = look >= 0 && look < g_lookCount;
    const int bgMode = hasLook ? g_lookBgMode[look][screen] : 0;
    const int slideMode = hasLook ? g_lookSlideMode[look][screen] : 1;
    const RECT* bgSrc = (spanScreen && bgMode == 0) ? &seat : &screenFull;
    const RECT* slideSrc = (spanScreen && slideMode == 0) ? &seat : &screenFull;
    MapRectToCompose(bgSrc, screenW, screenH, composeW, composeH, videoSrc);
    MapRectToCompose(slideSrc, screenW, screenH, composeW, composeH, overlaySrc);
    const int display = FindLookDisplay(screen, slot);
    const int store = display < 0 ? -1 : g_lookDisplays[display].store;
    if (bgChecked != nullptr)
    {
        *bgChecked = !hasLook || store < 0 || g_lookBgOn[look][store];
    }
    if (slideChecked != nullptr)
    {
        *slideChecked = !hasLook || store < 0 || g_lookSlideOn[look][store];
    }
}

static void LookSeatRect(int screen, int slot, int canvasW, int canvasH, RECT* rc)
{
    if (rc == nullptr)
    {
        return;
    }
    if (canvasW < 1)
    {
        canvasW = 1;
    }
    if (canvasH < 1)
    {
        canvasH = 1;
    }
    rc->left = 0;
    rc->top = 0;
    rc->right = canvasW;
    rc->bottom = canvasH;
    int left = 0;
    int right = canvasW > 0 ? canvasW - 1 : 0;
    if (!ManageScreens_GetScreenOutputSlice(screen, slot, &left, &right))
    {
        return;
    }
    rc->left = left;
    rc->right = right + 1;
    if (rc->right > canvasW)
    {
        rc->right = canvasW;
    }
    if (rc->right <= rc->left)
    {
        rc->right = rc->left + 1;
    }
}

static void MapRectToCompose(const RECT* src, int srcW, int srcH, int composeW, int composeH, RECT* out)
{
    if (out == nullptr)
    {
        return;
    }
    if (srcW < 1)
    {
        srcW = 1;
    }
    if (srcH < 1)
    {
        srcH = 1;
    }
    if (src == nullptr)
    {
        out->left = 0;
        out->top = 0;
        out->right = composeW;
        out->bottom = composeH;
        return;
    }
    out->left = MulDiv(src->left, composeW, srcW);
    out->top = MulDiv(src->top, composeH, srcH);
    out->right = MulDiv(src->right, composeW, srcW);
    out->bottom = MulDiv(src->bottom, composeH, srcH);
    if (out->right <= out->left)
    {
        out->right = out->left + 1;
    }
    if (out->bottom <= out->top)
    {
        out->bottom = out->top + 1;
    }
}

static int LiveOutput_AppendSpec(
    GpuOutputTarget* out,
    int count,
    int max,
    HWND hwnd,
    const RECT* videoSrc,
    bool videoOn,
    const RECT* overlaySrc,
    bool overlayOn,
    int paneIndex,
    int paneCount
)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || out == nullptr || count >= max)
    {
        return count;
    }
    if (paneCount < 1)
    {
        paneCount = 1;
    }
    if (paneIndex < 0)
    {
        paneIndex = 0;
    }
    if (paneIndex >= paneCount)
    {
        paneIndex = paneCount - 1;
    }
    for (int i = 0; i < count; ++i)
    {
        if (out[i].hwnd == hwnd && out[i].paneIndex == paneIndex && paneCount <= 1 && out[i].paneCount <= 1)
        {
            return count;
        }
        if (out[i].hwnd == hwnd && out[i].paneIndex == paneIndex && out[i].paneCount == paneCount)
        {
            return count;
        }
    }

    GpuOutputTarget spec = {};
    spec.hwnd = hwnd;
    spec.paneIndex = paneIndex;
    spec.paneCount = paneCount;
    if (videoSrc != nullptr)
    {
        spec.srcX = videoSrc->left;
        spec.srcY = videoSrc->top;
        spec.srcW = videoSrc->right - videoSrc->left;
        spec.srcH = videoSrc->bottom - videoSrc->top;
    }
    if (overlaySrc != nullptr)
    {
        spec.ovX = overlaySrc->left;
        spec.ovY = overlaySrc->top;
        spec.ovW = overlaySrc->right - overlaySrc->left;
        spec.ovH = overlaySrc->bottom - overlaySrc->top;
    }
    if (spec.srcW < 1)
    {
        spec.srcW = 1;
    }
    if (spec.srcH < 1)
    {
        spec.srcH = 1;
    }
    if (spec.ovW < 1)
    {
        spec.ovW = 1;
    }
    if (spec.ovH < 1)
    {
        spec.ovH = 1;
    }
    spec.videoOn = videoOn ? 1 : 0;
    spec.overlayOn = overlayOn ? 1 : 0;
    out[count] = spec;
    return count + 1;
}

static int LiveOutput_CollectOutputs(GpuOutputTarget* out, int max, int* canvasW, int* canvasH)
{
    LoadLookState();
    RebuildLookLayout();

    int composeW = 1920;
    int composeH = 1080;
    ManageScreens_GetScreenCanvas(PreviewScreenIndex(), &composeW, &composeH, nullptr);
    if (composeW < 1)
    {
        composeW = 1920;
    }
    if (composeH < 1)
    {
        composeH = 1080;
    }
    if (canvasW != nullptr)
    {
        *canvasW = composeW;
    }
    if (canvasH != nullptr)
    {
        *canvasH = composeH;
    }

    RECT full = { 0, 0, composeW, composeH };
    const bool slideLive = g_liveArmed;
    const bool bgLive = BackgroundLayerOn();
    int look = g_appliedLook;
    if (look < 0 || look >= g_lookCount)
    {
        look = -1;
    }

    int count = 0;
    for (int d = 0; d < g_lookDisplayCount && count < max; ++d)
    {
        if (g_lookDisplays[d].ghost)
        {
            continue;
        }
        const int screen = g_lookDisplays[d].screen;
        const int glasses = g_lookDisplays[d].mirror ? ManageScreens_GetScreenOutputCount(screen) : 1;
        for (int k = 0; k < glasses && count < max; ++k)
        {
            const int slot = g_lookDisplays[d].mirror ? k : g_lookDisplays[d].slot;
            HWND hwnd = ManageScreens_GetScreenOutputHwnd(screen, slot);
            if (hwnd == nullptr)
            {
                continue;
            }

            RECT videoSrc = {};
            RECT overlaySrc = {};
            bool bgOn = true;
            bool slideOn = true;
            LookOutputRects(look, screen, slot, composeW, composeH, &videoSrc, &overlaySrc, &bgOn, &slideOn);
            count = LiveOutput_AppendSpec(
                out,
                count,
                max,
                hwnd,
                &videoSrc,
                bgLive && bgOn,
                &overlaySrc,
                slideLive && slideOn,
                0,
                1
            );
        }
    }

    const int previewScreen = PreviewScreenIndex();
    const int previewSeats = ManageScreens_GetScreenOutputCount(previewScreen);
    if (!PreviewSplitBySeat())
    {
        RECT previewVideo = full;
        RECT previewOverlay = full;
        bool previewBg = bgLive;
        bool previewSlide = slideLive;
        const bool mirrorScreen =
            previewSeats > 1 && ManageScreens_GetScreenLayout(previewScreen) == SCREEN_LAYOUT_MIRROR;
        if (g_previewSlot >= 0 || mirrorScreen)
        {
            bool bgOn = true;
            bool slideOn = true;
            LookOutputRects(
                look,
                previewScreen,
                g_previewSlot >= 0 ? g_previewSlot : 0,
                composeW,
                composeH,
                &previewVideo,
                &previewOverlay,
                &bgOn,
                &slideOn
            );
            previewBg = bgLive && bgOn;
            previewSlide = slideLive && slideOn;
        }
        count = LiveOutput_AppendSpec(out, count, max, g_hostPreview, &previewVideo, previewBg, &previewOverlay, previewSlide, 0, 1);
        count = LiveOutput_AppendSpec(out, count, max, g_programPreview, &previewVideo, previewBg, &previewOverlay, previewSlide, 0, 1);
    }
    else
    {
        for (int slot = 0; slot < previewSeats && count < max; ++slot)
        {
            RECT previewVideo = full;
            RECT previewOverlay = full;
            bool bgOn = true;
            bool slideOn = true;
            LookOutputRects(
                look,
                previewScreen,
                slot,
                composeW,
                composeH,
                &previewVideo,
                &previewOverlay,
                &bgOn,
                &slideOn
            );
            count = LiveOutput_AppendSpec(
                out,
                count,
                max,
                g_hostPreview,
                &previewVideo,
                bgLive && bgOn,
                &previewOverlay,
                slideLive && slideOn,
                slot,
                previewSeats
            );
            count = LiveOutput_AppendSpec(
                out,
                count,
                max,
                g_programPreview,
                &previewVideo,
                bgLive && bgOn,
                &previewOverlay,
                slideLive && slideOn,
                slot,
                previewSeats
            );
        }
    }

    for (int i = 0; i < BOOT_MAX_SCREENS; ++i)
    {
        count = LiveOutput_AppendSpec(out, count, max, g_screenWindows[i], &full, bgLive, &full, slideLive, 0, 1);
    }
    return count;
}

static void LiveOutput_BlitFrame(HDC hdc, int width, int height)
{
    if (hdc == nullptr || width <= 0 || height <= 0)
    {
        return;
    }

    RECT well = { 0, 0, width, height };
    HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &well, fill);
    DeleteObject(fill);

    if (g_backgroundActive && g_liveBgHeld)
    {
        BlitHoldFrame(hdc, 0, 0, width, height, false);
    }
    if (g_liveArmed)
    {
        int fadeW = 0;
        int fadeH = 0;
        HBITMAP fade = SlideXfadeFrame(&fadeW, &fadeH);
        if (fade != nullptr)
        {
            BlitAlphaOverlay(hdc, 0, 0, width, height, fade, 0, 0, fadeW, fadeH);
        }
        else
        {
            const int content = Renderer_CurrentSlide();
            if (content >= 0)
            {
                Renderer_BlitSlideContentAt(hdc, content, 0, 0, width, height);
            }
        }
    }
}

static void LiveOutput_BlitLayerSlice(
    HDC dest,
    int destX,
    int destY,
    int destW,
    int destH,
    int slideIndex,
    int srcX,
    int srcY,
    int srcW,
    int srcH,
    int canvasW,
    int canvasH,
    bool keyed
)
{
    if (dest == nullptr || destW <= 0 || destH <= 0 || slideIndex < 0)
    {
        return;
    }
    if (srcW < 1 || srcH < 1 || canvasW < 1 || canvasH < 1)
    {
        return;
    }

    int frameW = 0;
    int frameH = 0;
    HBITMAP frame = Renderer_SlideFrame(slideIndex, &frameW, &frameH);
    if (frame == nullptr)
    {
        return;
    }
    if (frameW < 1)
    {
        frameW = canvasW;
    }
    if (frameH < 1)
    {
        frameH = canvasH;
    }
    if (keyed)
    {
        BlitAlphaOverlay(dest, destX, destY, destW, destH, frame, srcX, srcY, srcW, srcH);
        return;
    }
    HDC mem = CreateCompatibleDC(dest);
    if (mem == nullptr)
    {
        return;
    }
    HBITMAP old = (HBITMAP)SelectObject(mem, frame);
    SetStretchBltMode(dest, COLORONCOLOR);
    StretchBlt(dest, destX, destY, destW, destH, mem, srcX, srcY, srcW, srcH, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
}

static HBITMAP HoldCanvas(int canvasW, int canvasH)
{
    if (g_liveBgFrame == nullptr || canvasW < 1 || canvasH < 1)
    {
        return nullptr;
    }
    if (g_holdCanvas != nullptr &&
        g_holdCanvasW == canvasW &&
        g_holdCanvasH == canvasH &&
        g_holdCanvasSource == g_liveBgFrame)
    {
        return g_holdCanvas;
    }

    ReleaseHoldCanvas();
    void* bits = nullptr;
    HBITMAP canvas = CreateXfadeDib(canvasW, canvasH, &bits);
    if (canvas == nullptr || bits == nullptr)
    {
        if (canvas != nullptr)
        {
            DeleteObject(canvas);
        }
        return nullptr;
    }

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    if (mem == nullptr)
    {
        ReleaseDC(nullptr, screen);
        DeleteObject(canvas);
        return nullptr;
    }
    HBITMAP old = (HBITMAP)SelectObject(mem, canvas);
    RECT well = { 0, 0, canvasW, canvasH };
    HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(mem, &well, fill);
    DeleteObject(fill);
    BlitHoldFrame(mem, 0, 0, canvasW, canvasH, false);
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);

    g_holdCanvas = canvas;
    g_holdCanvasW = canvasW;
    g_holdCanvasH = canvasH;
    g_holdCanvasSource = g_liveBgFrame;
    return g_holdCanvas;
}

static HBITMAP LiveComposeBufferFor(HWND hwnd, int width, int height)
{
    if (hwnd == nullptr || width < 1 || height < 1)
    {
        return nullptr;
    }
    int freeSlot = -1;
    for (int i = 0; i < GPU_MAX_TARGETS; ++i)
    {
        LiveComposeBuffer* slot = &g_liveComposeBuffers[i];
        if (slot->hwnd == hwnd)
        {
            if (slot->dib != nullptr && slot->width == width && slot->height == height)
            {
                return slot->dib;
            }
            if (slot->dib != nullptr)
            {
                DeleteObject(slot->dib);
                slot->dib = nullptr;
            }
            void* bits = nullptr;
            slot->dib = CreateXfadeDib(width, height, &bits);
            slot->width = slot->dib != nullptr ? width : 0;
            slot->height = slot->dib != nullptr ? height : 0;
            return slot->dib;
        }
        if (slot->hwnd != nullptr && !IsWindow(slot->hwnd))
        {
            if (slot->dib != nullptr)
            {
                DeleteObject(slot->dib);
                slot->dib = nullptr;
            }
            slot->hwnd = nullptr;
            slot->width = 0;
            slot->height = 0;
        }
        if (freeSlot < 0 && slot->hwnd == nullptr)
        {
            freeSlot = i;
        }
    }
    if (freeSlot < 0)
    {
        return nullptr;
    }
    LiveComposeBuffer* slot = &g_liveComposeBuffers[freeSlot];
    void* bits = nullptr;
    slot->dib = CreateXfadeDib(width, height, &bits);
    if (slot->dib == nullptr)
    {
        return nullptr;
    }
    slot->hwnd = hwnd;
    slot->width = width;
    slot->height = height;
    return slot->dib;
}

static void LiveOutput_BlitHoldSlice(
    HDC dest,
    int destX,
    int destY,
    int destW,
    int destH,
    int srcX,
    int srcY,
    int srcW,
    int srcH,
    int canvasW,
    int canvasH,
    bool keyed
)
{
    if (dest == nullptr || destW <= 0 || destH <= 0 || g_liveBgFrame == nullptr)
    {
        return;
    }
    if (srcW < 1 || srcH < 1 || canvasW < 1 || canvasH < 1)
    {
        return;
    }

    if (keyed)
    {
        BlitAlphaOverlay(dest, destX, destY, destW, destH, g_liveBgFrame, srcX, srcY, srcW, srcH);
        return;
    }
    HBITMAP canvas = HoldCanvas(canvasW, canvasH);
    if (canvas == nullptr)
    {
        return;
    }
    HDC mem = CreateCompatibleDC(dest);
    if (mem == nullptr)
    {
        return;
    }
    HBITMAP old = (HBITMAP)SelectObject(mem, canvas);
    SetStretchBltMode(dest, COLORONCOLOR);
    StretchBlt(dest, destX, destY, destW, destH, mem, srcX, srcY, srcW, srcH, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
}

static void LiveOutput_PaneRect(int clientW, int clientH, int paneIndex, int paneCount, RECT* out)
{
    if (out == nullptr)
    {
        return;
    }
    if (clientW < 1)
    {
        clientW = 1;
    }
    if (clientH < 1)
    {
        clientH = 1;
    }
    if (paneCount < 1)
    {
        paneCount = 1;
    }
    if (paneIndex < 0)
    {
        paneIndex = 0;
    }
    if (paneIndex >= paneCount)
    {
        paneIndex = paneCount - 1;
    }
    const int slice = clientW / paneCount;
    out->left = paneIndex * slice;
    out->top = 0;
    out->right = (paneIndex + 1 >= paneCount) ? clientW : (paneIndex + 1) * slice;
    out->bottom = clientH;
    if (out->right <= out->left)
    {
        out->right = out->left + 1;
    }
}

static void LiveOutput_PaintWindow(
    HWND hwnd,
    const GpuOutputTarget* outs,
    int count,
    bool blackOnly,
    int canvasW,
    int canvasH
)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || outs == nullptr)
    {
        return;
    }

    RECT client = {};
    GetClientRect(hwnd, &client);
    if (client.right <= 0 || client.bottom <= 0)
    {
        return;
    }

    bool any = false;
    bool lit = false;
    for (int i = 0; i < count; ++i)
    {
        if (outs[i].hwnd != hwnd)
        {
            continue;
        }
        any = true;
        if (!blackOnly && (outs[i].videoOn != 0 || outs[i].overlayOn != 0))
        {
            lit = true;
        }
    }
    if (!any)
    {
        return;
    }
    if (!lit)
    {
        BlackOutHwnd(hwnd);
        return;
    }

    HDC hdc = GetDC(hwnd);
    if (hdc == nullptr)
    {
        return;
    }

    // Build the whole frame in memory, then copy it to the window in one
    // step. Drawing pane by pane on the window itself let the screen show a
    // half-built picture: black, then one background, then one slide.
    HBITMAP composeBmp = LiveComposeBufferFor(hwnd, client.right, client.bottom);
    HDC composeDc = composeBmp != nullptr ? CreateCompatibleDC(hdc) : nullptr;
    HBITMAP oldCompose = nullptr;
    HDC paintDc = hdc;
    if (composeDc != nullptr)
    {
        oldCompose = (HBITMAP)SelectObject(composeDc, composeBmp);
        paintDc = composeDc;
    }

    RECT well = { 0, 0, client.right, client.bottom };
    HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(paintDc, &well, fill);
    DeleteObject(fill);

    for (int i = 0; i < count; ++i)
    {
        if (outs[i].hwnd != hwnd)
        {
            continue;
        }
        RECT pane = {};
        LiveOutput_PaneRect(client.right, client.bottom, outs[i].paneIndex, outs[i].paneCount, &pane);
        const int destW = pane.right - pane.left;
        const int destH = pane.bottom - pane.top;
        if (destW <= 0 || destH <= 0)
        {
            continue;
        }
        if (outs[i].videoOn)
        {
            if (g_backgroundActive && g_liveBgHeld && g_liveBgFrame != nullptr)
            {
                LiveOutput_BlitHoldSlice(
                    paintDc,
                    pane.left,
                    pane.top,
                    destW,
                    destH,
                    outs[i].srcX,
                    outs[i].srcY,
                    outs[i].srcW,
                    outs[i].srcH,
                    canvasW,
                    canvasH,
                    false
                );
            }
        }
        if (outs[i].overlayOn)
        {
            int fadeW = 0;
            int fadeH = 0;
            HBITMAP fade = SlideXfadeFrame(&fadeW, &fadeH);
            if (fade != nullptr)
            {
                const int srcW = fadeW > 0 ? fadeW : canvasW;
                const int srcH = fadeH > 0 ? fadeH : canvasH;
                BlitAlphaOverlay(
                    paintDc,
                    pane.left,
                    pane.top,
                    destW,
                    destH,
                    fade,
                    outs[i].ovX,
                    outs[i].ovY,
                    outs[i].ovW > 0 ? outs[i].ovW : srcW,
                    outs[i].ovH > 0 ? outs[i].ovH : srcH
                );
            }
            else
            {
                const int content = Renderer_CurrentSlide();
                if (content >= 0)
                {
                    LiveOutput_BlitLayerSlice(
                        paintDc,
                        pane.left,
                        pane.top,
                        destW,
                        destH,
                        content,
                        outs[i].ovX,
                        outs[i].ovY,
                        outs[i].ovW,
                        outs[i].ovH,
                        canvasW,
                        canvasH,
                        true
                    );
                }
            }
        }
    }

    if (composeDc != nullptr)
    {
        BitBlt(hdc, 0, 0, client.right, client.bottom, composeDc, 0, 0, SRCCOPY);
        SelectObject(composeDc, oldCompose);
        DeleteDC(composeDc);
    }
    ReleaseDC(hwnd, hdc);
}

static void LiveOutput_PaintOutputs(const GpuOutputTarget* outs, int count, bool blackOnly, int canvasW, int canvasH)
{
    bool done[GPU_MAX_TARGETS] = {};
    for (int i = 0; i < count; ++i)
    {
        if (done[i] || outs[i].hwnd == nullptr)
        {
            continue;
        }
        LiveOutput_PaintWindow(outs[i].hwnd, outs, count, blackOnly, canvasW, canvasH);
        for (int j = i; j < count; ++j)
        {
            if (outs[j].hwnd == outs[i].hwnd)
            {
                done[j] = true;
            }
        }
    }
}

void LiveOutput_PaintHwnd(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return;
    }
    if (App_OutputCleared() || App_LiveHold())
    {
        PaintWindowBlack(hwnd);
        return;
    }

    GpuOutputTarget outs[GPU_MAX_TARGETS] = {};
    int canvasW = 1920;
    int canvasH = 1080;
    const int count = LiveOutput_CollectOutputs(outs, GPU_MAX_TARGETS, &canvasW, &canvasH);
    for (int i = 0; i < count; ++i)
    {
        if (outs[i].hwnd == hwnd)
        {
            LiveOutput_PaintWindow(hwnd, outs, count, false, canvasW, canvasH);
            return;
        }
    }
    PaintWindowBlack(hwnd);
}

static void ResolveBackgroundPath(wchar_t* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    out[0] = L'\0';
    if (g_backgroundActive && g_liveBgHeld && g_liveBgVideo[0] != L'\0')
    {
        lstrcpynW(out, g_liveBgVideo, outChars);
    }
}

static HBITMAP ComposeHoldOverlay(bool slideOn, int* width, int* height)
{
    int overlayW = g_liveBgW > 0 ? g_liveBgW : 1920;
    int overlayH = g_liveBgH > 0 ? g_liveBgH : 1080;
    if (overlayW < 1 || overlayH < 1)
    {
        return nullptr;
    }
    if (g_holdOverlay == nullptr || g_holdOverlayW != overlayW || g_holdOverlayH != overlayH)
    {
        if (g_holdOverlay != nullptr)
        {
            DeleteObject(g_holdOverlay);
            g_holdOverlay = nullptr;
        }
        void* bits = nullptr;
        g_holdOverlay = CreateXfadeDib(overlayW, overlayH, &bits);
        g_holdOverlayW = overlayW;
        g_holdOverlayH = overlayH;
    }
    if (g_holdOverlay == nullptr)
    {
        return nullptr;
    }

    DIBSECTION section = {};
    if (GetObject(g_holdOverlay, sizeof(section), &section) != 0 && section.dsBm.bmBits != nullptr)
    {
        const int bytes = section.dsBm.bmWidthBytes *
            (section.dsBm.bmHeight < 0 ? -section.dsBm.bmHeight : section.dsBm.bmHeight);
        ZeroMemory(section.dsBm.bmBits, bytes);
    }

    HDC screen = GetDC(nullptr);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP oldDest = (HBITMAP)SelectObject(dest, g_holdOverlay);
    // Content plane only — GPU already plays the live video bed underneath.
    if (slideOn)
    {
        int fadeW = 0;
        int fadeH = 0;
        HBITMAP fade = SlideXfadeFrame(&fadeW, &fadeH);
        if (fade != nullptr)
        {
            BlitAlphaOverlay(dest, 0, 0, overlayW, overlayH, fade, 0, 0, fadeW, fadeH);
        }
        else
        {
            const int content = Renderer_CurrentSlide();
            if (content >= 0)
            {
                Renderer_BlitSlideContentAt(dest, content, 0, 0, overlayW, overlayH);
            }
        }
    }
    SelectObject(dest, oldDest);
    DeleteDC(dest);
    ReleaseDC(nullptr, screen);

    if (width != nullptr)
    {
        *width = overlayW;
    }
    if (height != nullptr)
    {
        *height = overlayH;
    }
    return g_holdOverlay;
}

static void PublishSlideOverlay(bool slideOn)
{
    int width = 0;
    int height = 0;
    const bool videoBg = g_backgroundActive && g_liveBgHeld && g_liveBgVideo[0] != L'\0';
    const unsigned char* fadeBits = SlideXfadeBits(&width, &height);
    if (fadeBits != nullptr)
    {
        GpuClient_SetSlideOverlayBits(fadeBits, width, height, slideOn);
        return;
    }
    if (videoBg)
    {
        HBITMAP frame = ComposeHoldOverlay(slideOn, &width, &height);
        GpuClient_SetSlideOverlay(frame, width, height, frame != nullptr);
        return;
    }

    if (!slideOn)
    {
        GpuClient_SetSlideOverlay(nullptr, 0, 0, false);
        return;
    }

    HBITMAP frame = Renderer_SlideFrame(Renderer_CurrentSlide(), &width, &height);
    GpuClient_SetSlideOverlay(frame, width, height, frame != nullptr);
}

// Always-on live output engine. UI only changes state; this pushes the frame
// to every live screen plus the courtesy previews.
static void ScenesNoteLive();

void LiveOutput_Push()
{
    if (!App_Ready())
    {
        return;
    }
    ScenesNoteLive();

    if (g_liveHold)
    {
        return;
    }

    GpuOutputTarget outputs[GPU_MAX_TARGETS] = {};
    int canvasW = 1920;
    int canvasH = 1080;
    const int outputCount = LiveOutput_CollectOutputs(outputs, GPU_MAX_TARGETS, &canvasW, &canvasH);
    const bool slideOn = g_liveArmed;
    const bool bgOn = BackgroundLayerOn();
    const bool fading = SlideXfadeActive();

    if (fading)
    {
        PublishSlideOverlay(slideOn);
        wchar_t path[MAX_PATH] = {};
        ResolveBackgroundPath(path, MAX_PATH);
        if (bgOn && path[0] != L'\0' && GpuClient_Ready())
        {
            GpuLayer layer = {};
            lstrcpynW(layer.path, path, MAX_PATH);
            layer.x = 0;
            layer.y = 0;
            layer.w = canvasW;
            layer.h = canvasH;
            layer.slideW = canvasW;
            layer.slideH = canvasH;
            layer.mode = IMAGE_FIT;
            GpuClient_PlayOutputs(outputs, outputCount, &layer, 1);
            if (GpuClient_OwnsOutputs())
            {
                return;
            }
        }
        LiveOutput_PaintOutputs(outputs, outputCount, false, canvasW, canvasH);
        return;
    }

    if (!slideOn && !bgOn)
    {
        PublishSlideOverlay(false);
        if (GpuClient_Ready())
        {
            GpuClient_Stop();
        }
        LiveOutput_PaintOutputs(outputs, outputCount, true, canvasW, canvasH);
        PaintLiveScreensBlack();
        return;
    }

    if (!GpuClient_Ready())
    {
        GpuClient_Connect();
    }

    if (bgOn)
    {
        wchar_t path[MAX_PATH] = {};
        ResolveBackgroundPath(path, MAX_PATH);
        if (path[0] != L'\0')
        {
            GpuLayer layer = {};
            lstrcpynW(layer.path, path, MAX_PATH);
            layer.x = 0;
            layer.y = 0;
            layer.w = canvasW;
            layer.h = canvasH;
            layer.slideW = canvasW;
            layer.slideH = canvasH;
            layer.mode = IMAGE_FIT;
            PublishSlideOverlay(slideOn);
            if (GpuClient_Ready() && outputCount > 0)
            {
                GpuClient_PlayOutputs(outputs, outputCount, &layer, 1);
                EventLog("LiveOutput_Push BG");
                return;
            }
        }

        // Still / composed BG: keep the slide overlay published and paint GDI so
        // the courtesy preview never goes black while a BG is armed.
        PublishSlideOverlay(slideOn);
        if (GpuClient_Ready())
        {
            GpuClient_Stop();
        }
        LiveOutput_PaintOutputs(outputs, outputCount, false, canvasW, canvasH);
        EventLog("LiveOutput_Push BG STILL");
        return;
    }

    VideoLayer layers[GPU_MAX_LAYERS] = {};
    const int layerCount = slideOn ? Renderer_GetVideoLayers(layers, GPU_MAX_LAYERS) : 0;
    if (GpuClient_Ready() && layerCount > 0 && outputCount > 0)
    {
        GpuLayer gpuLayers[GPU_MAX_LAYERS] = {};
        for (int i = 0; i < layerCount; ++i)
        {
            lstrcpynW(gpuLayers[i].path, layers[i].path, MAX_PATH);
            int srcW = layers[i].slideW > 0 ? layers[i].slideW : canvasW;
            int srcH = layers[i].slideH > 0 ? layers[i].slideH : canvasH;
            gpuLayers[i].x = MulDiv(layers[i].x, canvasW, srcW);
            gpuLayers[i].y = MulDiv(layers[i].y, canvasH, srcH);
            gpuLayers[i].w = MulDiv(layers[i].w, canvasW, srcW);
            gpuLayers[i].h = MulDiv(layers[i].h, canvasH, srcH);
            gpuLayers[i].slideW = canvasW;
            gpuLayers[i].slideH = canvasH;
            gpuLayers[i].mode = layers[i].mode;
        }
        PublishSlideOverlay(false);
        GpuClient_PlayOutputs(outputs, outputCount, gpuLayers, layerCount);
        EventLog("LiveOutput_Push PLAY");
        return;
    }

    PublishSlideOverlay(false);
    if (GpuClient_Ready())
    {
        GpuClient_Stop();
    }
    LiveOutput_PaintOutputs(outputs, outputCount, !slideOn, canvasW, canvasH);
    EventLog(slideOn ? "LiveOutput_Push GDI" : "LiveOutput_Push BLACK");
}

void ClearVideoBackground()
{
    EventLog("ClearVideoBackground begin");
    g_backgroundActive = false;
    ReleaseLiveBackground();
    g_videoBackgroundCleared = true;
    g_videoScrubDragging = false;
    g_videoScrubRatio = 0.0;

    LiveOutput_Push();

    LayoutProgramPreview();
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
    UpdateMediaTransportUi();
    EventLog("ClearVideoBackground end");
}

static void PresentXfadeTick()
{
    if (!App_Ready() || g_liveHold)
    {
        return;
    }
    LiveOutput_Push();
}

void PresentCurrent()
{
    if (!App_Ready())
    {
        return;
    }

    if (g_liveHold)
    {
        EventLog("PresentCurrent skip liveHold");
        if (g_programMiddle != nullptr)
        {
            InvalidateRect(g_programMiddle, nullptr, FALSE);
        }
        return;
    }

    LiveOutput_Push();

    if (g_mainWindow != nullptr)
    {
        LayoutHostPreview(g_mainWindow);
    }

    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
    if (g_programPreview != nullptr)
    {
        LayoutProgramPreview();
        if (g_programLeft != nullptr)
        {
            InvalidateRect(g_programLeft, nullptr, FALSE);
        }
    }
    UpdateMediaTransportUi();
}

void ClearLiveSlide()
{
    EventLog("ClearLiveSlide begin");
    StopSlideXfade();
    g_liveArmed = false;
    g_liveHold = false;
    GpuClient_SetSlideOverlay(nullptr, 0, 0, false);
    PresentCurrent();
    if (g_programClearSlideBtn != nullptr)
    {
        InvalidateRect(g_programClearSlideBtn, nullptr, FALSE);
    }
    EventLog("ClearLiveSlide end");
}

void AppendScreensMenu(HMENU bar)
{
    HMENU screensMenu = CreatePopupMenu();
    HMENU virtualMenu = CreatePopupMenu();
    HMENU newMenu = CreatePopupMenu();
    for (int i = 0; i < VIRTUAL_MODE_COUNT; ++i)
    {
        AppendMenuA(newMenu, MF_STRING, ID_VIRTUAL_MODE_BASE + i, kVirtualModes[i].label);
    }
    AppendMenuA(virtualMenu, MF_POPUP, (UINT_PTR)newMenu, "&New");

    int count = ManageScreens_VirtualCount();
    if (count > 32)
    {
        count = 32;
    }
    if (count > 0)
    {
        AppendMenuA(virtualMenu, MF_SEPARATOR, 0, nullptr);
    }
    for (int i = 0; i < count; ++i)
    {
        char label[80] = {};
        if (!ManageScreens_GetVirtualLabel(i, label, sizeof(label)))
        {
            wsprintfA(label, "Display %d", ManageScreens_GetVirtualId(i));
        }
        AppendMenuA(virtualMenu, MF_STRING, ID_VIRTUAL_ITEM_BASE + i, label);
    }

    AppendMenuA(screensMenu, MF_POPUP, (UINT_PTR)virtualMenu, "&Virtual Display");
    AppendMenuA(screensMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(screensMenu, MF_STRING, ID_OPEN_ALL, "Open &all displays");
    AppendMenuA(screensMenu, MF_STRING, IDM_MANAGE_SCREENS, "&Manage screens...");
    AppendMenuA(screensMenu, MF_STRING, IDM_SCENES, "Sce&nes...");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)screensMenu, "&Screens");
}

HMENU CreateFileBar(bool includeScreens)
{
    HMENU bar = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_NEW_PROJECT, "&New Project...");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, nullptr);
    HMENU importMenu = CreatePopupMenu();
    AppendMenuA(importMenu, MF_STRING, IDM_FILE_IMPORT, "&Import...");
    AppendMenuA(importMenu, MF_STRING, IDM_FILE_SEARCH_IMPORT, "&Search to Import...");
    AppendMenuA(fileMenu, MF_POPUP, (UINT_PTR)importMenu, "&Import");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_EXIT, "E&xit");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)fileMenu, "&File");

    HMENU editMenu = CreatePopupMenu();
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_UNDO, "&Undo\tCtrl+Z");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_REDO, "&Redo\tCtrl+Y");
    AppendMenuA(editMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_CUT, "Cu&t\tCtrl+X");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_COPY, "&Copy\tCtrl+C");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_PASTE, "&Paste\tCtrl+V");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_DUPLICATE, "D&uplicate\tCtrl+D");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_DELETE, "&Delete\tDel");
    AppendMenuA(editMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_PREFERENCES, "&Preferences...");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_PREFERENCES_BETA, "Preferences (&beta)...");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)editMenu, "&Edit");

    HMENU slidesMenu = CreatePopupMenu();
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_NEW, "&New");
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_DUPLICATE, "&Duplicate\tCtrl+D");
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_QUICK, "&Quick Slides...");
    AppendMenuA(slidesMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_SELECT_ALL, "Select &all\tCtrl+A");
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_NEXT, "&Next");
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_PREV, "&Previous");
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_FIRST, "Go to &first");
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_LAST, "Go to &last");
    AppendMenuA(slidesMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_USE_SLIDE, "&Use as slide");
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_USE_BG, "Use as &background");
    AppendMenuA(slidesMenu, MF_STRING, IDM_SLIDE_EDIT, "&Edit slide");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)slidesMenu, "&Slides");

    if (includeScreens)
    {
        AppendScreensMenu(bar);
    }
    return bar;
}

HMENU CreateMainMenu()
{
    return CreateFileBar(true);
}

HMENU CreateProgramMenu()
{
    return CreateFileBar(true);
}

void RefreshScenesWindow();

void App_RebuildScreenMenus()
{
    if (g_programWindow != nullptr && IsWindow(g_programWindow))
    {
        HMENU previous = g_programMenuBar;
        g_programMenuBar = CreateProgramMenu();
        InvalidateRect(g_programWindow, nullptr, FALSE);
        if (previous != nullptr)
        {
            DestroyMenu(previous);
        }
    }
    RefreshScenesWindow();
    UpdateDocumentMenus();
}

bool PickPictureFile(HWND owner, wchar_t* path, int pathChars)
{
    if (path == nullptr || pathChars <= 0)
    {
        return false;
    }

    path[0] = L'\0';
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = path;
    dialog.nMaxFile = pathChars;
    dialog.lpstrFilter = L"Pictures (*.jpg;*.png;*.bmp;*.gif)\0*.jpg;*.jpeg;*.png;*.bmp;*.gif\0All files\0*.*\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&dialog) == TRUE;
}

bool PickVideoFile(HWND owner, wchar_t* path, int pathChars)
{
    if (path == nullptr || pathChars <= 0)
    {
        return false;
    }

    path[0] = L'\0';
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = path;
    dialog.nMaxFile = pathChars;
    dialog.lpstrFilter = L"Video (*.mp4;*.wmv;*.mov;*.avi;*.mkv)\0*.mp4;*.wmv;*.mov;*.avi;*.mkv\0All files\0*.*\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&dialog) == TRUE;
}

void SelectSlide(int index)
{
    const bool isBackground = Renderer_IsBackgroundSlide(index);
    if (isBackground)
    {
        CaptureLiveBackground(index);
        g_videoBackgroundCleared = false;
        if (g_liveBgVideo[0] != L'\0' || g_bgXfadeOn)
        {
            if (g_mediaPanel != MEDIA_PANEL_VIDEO)
            {
                g_mediaPanel = MEDIA_PANEL_VIDEO;
                LayoutProgramPreview();
            }
        }
    }

    const int outgoing = Renderer_CurrentSlide();
    const int incomingTrans = Renderer_EffectiveSlideTransition(index);
    const bool animateIn =
        !isBackground &&
        outgoing != index &&
        incomingTrans != SLIDE_TRANSITION_NONE &&
        (g_liveArmed || BackgroundLayerOn() || SlideXfadeActive());
    if (animateIn)
    {
        StartSlideXfade(outgoing, index);
    }
    else if (!isBackground)
    {
        StopSlideXfade();
    }

    if (!Renderer_SelectSlide(index))
    {
        StopSlideXfade();
        return;
    }

    if (index >= 0 && index < PROGRAM_SLIDE_MAX)
    {
        if (!g_slideSelected[index])
        {
            ClearSlideSelection();
            g_slideSelected[index] = true;
            g_slideSelectAnchor = index;
        }
    }

    g_liveHold = false;
    g_liveArmed = true;
    PresentCurrent();
    ArmSlideXfadeClock();
    ScheduleLiveSave();
}

void PlayForegroundSound()
{
    PlaySoundA(
        "C:\\Windows\\Media\\Windows Foreground.wav",
        nullptr,
        SND_FILENAME | SND_ASYNC
    );
}

void ShowClassicContextMenu(HWND owner, int screenX, int screenY)
{
    PlayForegroundSound();

    HMENU menu = CreatePopupMenu();
    AppendMenuA(menu, MF_STRING, IDM_YOU_LOST, "You lost");
    AppendMenuA(menu, MF_STRING, IDM_THE_GAME, "The Game");
    AppendMenuA(menu, MF_STRING, IDM_HOE, "Hoe");

    const int command = TrackPopupMenu(
        menu,
        TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
        screenX,
        screenY,
        0,
        owner,
        nullptr
    );

    DestroyMenu(menu);

    switch (command)
    {
        case IDM_YOU_LOST:
            PlayForegroundSound();
            MessageBoxA(owner, "You lost", "Menu", MB_OK);
            break;
        case IDM_THE_GAME:
            PlayForegroundSound();
            MessageBoxA(owner, "The Game", "Menu", MB_OK);
            break;
        case IDM_HOE:
            PlayForegroundSound();
            MessageBoxA(owner, "Hoe", "Menu", MB_OK);
            break;
    }
}

LRESULT CALLBACK ButtonProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    if (uMsg == WM_CONTEXTMENU)
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (x == -1 && y == -1)
        {
            RECT rc;
            GetWindowRect(hwnd, &rc);
            x = rc.left;
            y = rc.bottom;
        }

        ShowClassicContextMenu(GetParent(hwnd), x, y);
        return 0;
    }

    if (uMsg == WM_RBUTTONUP)
    {
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        ClientToScreen(hwnd, &pt);
        ShowClassicContextMenu(GetParent(hwnd), pt.x, pt.y);
        return 0;
    }

    return CallWindowProcA(g_originalButtonProc, hwnd, uMsg, wParam, lParam);
}

struct MonitorList
{
    RECT rects[3];
    int count;
};

BOOL CALLBACK MonitorEnumProc(HMONITOR, HDC, LPRECT lprcMonitor, LPARAM dwData)
{
    MonitorList* list = reinterpret_cast<MonitorList*>(dwData);
    if (list->count >= 3)
    {
        return FALSE;
    }

    list->rects[list->count] = *lprcMonitor;
    list->count += 1;
    return TRUE;
}

void GetScreenRect(int index, RECT* outRect)
{
    const int width = 800;
    const int height = 480;
    const int offset = 40 * (index + 1);

    MonitorList monitors = {};
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

    RECT origin = {};
    if (index >= 0 && index < monitors.count)
    {
        origin = monitors.rects[index];
    }
    else
    {
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &origin, 0);
    }

    outRect->left = origin.left + offset;
    outRect->top = origin.top + offset;
    outRect->right = outRect->left + width;
    outRect->bottom = outRect->top + height;
}

static void FormatScreenWindowTitle(int index, char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }

    char name[64] = {};
    if (!ManageScreens_GetScreenLabel(index, name, sizeof(name)))
    {
        wsprintfA(name, "Screen %d", index + 1);
    }

    int width = 1920;
    int height = 1080;
    int refreshHz = 60;
    ManageScreens_GetScreenCanvas(index, &width, &height, &refreshHz);

    const char* mode = nullptr;
    for (int i = 0; i < VIRTUAL_MODE_COUNT; ++i)
    {
        if (kVirtualModes[i].width == width &&
            kVirtualModes[i].height == height &&
            kVirtualModes[i].refreshHz == refreshHz)
        {
            mode = kVirtualModes[i].label;
            break;
        }
    }

    if (mode != nullptr)
    {
        wsprintfA(out, "%s - %s", name, mode);
    }
    else
    {
        wsprintfA(out, "%s - %dx%d %dHz", name, width, height, refreshHz);
    }
}

static HWND VirtualScreenOwner()
{
    if (g_programWindow != nullptr && IsWindow(g_programWindow))
    {
        return g_programWindow;
    }
    return nullptr;
}

static HWND ScreensHost()
{
    ShowProgramView();
    return VirtualScreenOwner();
}

void OpenBlankScreen(int index)
{
    if (index < 0 || index >= BOOT_MAX_SCREENS)
    {
        return;
    }

    char title[96] = {};
    if (!ManageScreens_GetOutputLabel(index, title, sizeof(title)))
    {
        FormatScreenWindowTitle(index, title, sizeof(title));
    }
    HWND owner = VirtualScreenOwner();

    HWND existing = g_screenWindows[index];
    if (existing == nullptr)
    {
        existing = ManageScreens_GetOutputHwnd(index);
        if (existing != nullptr)
        {
            char className[32] = {};
            GetClassNameA(existing, className, sizeof(className));
            if (lstrcmpiA(className, "BlankScreenWindow") != 0)
            {
                existing = nullptr;
            }
        }
    }

    if (existing != nullptr)
    {
        g_screenWindows[index] = existing;
        ManageScreens_SetOutputHwnd(index, existing);
        SetWindowLongPtrA(existing, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
        SetWindowTextA(existing, title);
        ShowWindow(existing, SW_SHOW);
        PresentCurrent();
        return;
    }

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    RECT windowRc = { 0, 0, VIRTUAL_VIEW_MIN_W, VIRTUAL_VIEW_MIN_H };
    AdjustWindowRectEx(&windowRc, style, FALSE, 0);

    RECT work = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    const int offset = 36 + index * 28;

    HWND screen = CreateWindowExA(
        0,
        "BlankScreenWindow",
        title,
        style,
        work.left + offset,
        work.top + offset,
        windowRc.right - windowRc.left,
        windowRc.bottom - windowRc.top,
        owner,
        nullptr,
        g_instance,
        reinterpret_cast<LPVOID>(static_cast<LONG_PTR>(index))
    );

    g_screenWindows[index] = screen;
    ManageScreens_SetOutputHwnd(index, screen);
    ShowWindow(screen, SW_SHOW);
    PresentCurrent();
}

void App_RefreshScreenViewerTitles()
{
    const int count = ManageScreens_VirtualCount();
    for (int i = 0; i < count; ++i)
    {
        const int id = ManageScreens_GetVirtualId(i);
        HWND hwnd = ManageScreens_GetVirtualHwnd(id);
        if (hwnd == nullptr || !IsWindow(hwnd))
        {
            continue;
        }
        char title[96] = {};
        if (!ManageScreens_GetVirtualLabel(i, title, sizeof(title)))
        {
            wsprintfA(title, "Display %d", id);
        }
        SetWindowTextA(hwnd, title);
    }
}

void App_OpenScreenViewer(int index)
{
    OpenBlankScreen(index);
}

void App_OpenVirtualDisplay(int id)
{
    if (id <= 0)
    {
        return;
    }

    int index = -1;
    const int count = ManageScreens_VirtualCount();
    for (int i = 0; i < count; ++i)
    {
        if (ManageScreens_GetVirtualId(i) == id)
        {
            index = i;
            break;
        }
    }
    if (index < 0)
    {
        return;
    }

    char title[96] = {};
    if (!ManageScreens_GetVirtualLabel(index, title, sizeof(title)))
    {
        wsprintfA(title, "Display %d", id);
    }

    HWND owner = VirtualScreenOwner();
    HWND existing = ManageScreens_GetVirtualHwnd(id);
    if (existing != nullptr && IsWindow(existing))
    {
        char className[32] = {};
        GetClassNameA(existing, className, sizeof(className));
        if (lstrcmpiA(className, "BlankScreenWindow") == 0)
        {
            SetWindowLongPtrA(existing, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner));
            SetWindowTextA(existing, title);
            ShowWindow(existing, SW_SHOW);
            PresentCurrent();
            return;
        }
    }

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    RECT windowRc = { 0, 0, VIRTUAL_VIEW_MIN_W, VIRTUAL_VIEW_MIN_H };
    AdjustWindowRectEx(&windowRc, style, FALSE, 0);

    RECT work = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    const int offset = 36 + index * 28;

    HWND screen = CreateWindowExA(
        0,
        "BlankScreenWindow",
        title,
        style,
        work.left + offset,
        work.top + offset,
        windowRc.right - windowRc.left,
        windowRc.bottom - windowRc.top,
        owner,
        nullptr,
        g_instance,
        reinterpret_cast<LPVOID>(static_cast<LONG_PTR>(id))
    );

    ManageScreens_SetVirtualHwnd(id, screen);
    ShowWindow(screen, SW_SHOW);
    PresentCurrent();
}

void ShowScenesWindow();
void OpenAllScreens();

bool HandleScreensCommand(HWND hwnd, int id)
{
    const bool screensCommand =
        id == IDM_VIRTUAL_NEW ||
        (id >= ID_VIRTUAL_ITEM_BASE && id <= ID_VIRTUAL_ITEM_LAST) ||
        (id >= ID_VIRTUAL_MODE_BASE && id <= ID_VIRTUAL_MODE_LAST) ||
        (id >= ID_SCREEN_ITEM_BASE && id <= ID_SCREEN_ITEM_LAST) ||
        id == ID_SCREEN_1 ||
        id == ID_SCREEN_2 ||
        id == ID_SCREEN_3 ||
        id == ID_OPEN_ALL ||
        id == IDM_MANAGE_SCREENS ||
        id == IDM_SCENES;
    if (!screensCommand)
    {
        return false;
    }

    HWND host = ScreensHost();
    if (host == nullptr)
    {
        host = hwnd;
    }

    if (id >= ID_VIRTUAL_MODE_BASE && id <= ID_VIRTUAL_MODE_LAST)
    {
        const int virtId = ManageScreens_AddVirtualDisplay(id - ID_VIRTUAL_MODE_BASE);
        if (virtId <= 0)
        {
            MessageBoxA(host, "You already have the maximum number of virtual displays.", "Screens", MB_OK | MB_ICONINFORMATION);
            return true;
        }
        App_OpenVirtualDisplay(virtId);
        App_RebuildScreenMenus();
        return true;
    }

    if (id >= ID_VIRTUAL_ITEM_BASE && id <= ID_VIRTUAL_ITEM_LAST)
    {
        App_OpenVirtualDisplay(ManageScreens_GetVirtualId(id - ID_VIRTUAL_ITEM_BASE));
        return true;
    }

    if (id == IDM_VIRTUAL_NEW)
    {
        const int virtId = ManageScreens_AddVirtualDisplay(5);
        if (virtId <= 0)
        {
            MessageBoxA(host, "You already have the maximum number of virtual displays.", "Screens", MB_OK | MB_ICONINFORMATION);
            return true;
        }
        App_OpenVirtualDisplay(virtId);
        App_RebuildScreenMenus();
        return true;
    }

    if (id >= ID_SCREEN_ITEM_BASE && id <= ID_SCREEN_ITEM_LAST)
    {
        OpenBlankScreen(id - ID_SCREEN_ITEM_BASE);
        return true;
    }

    switch (id)
    {
        case ID_SCREEN_1:
            App_OpenVirtualDisplay(ManageScreens_GetVirtualId(0));
            return true;
        case ID_SCREEN_2:
            App_OpenVirtualDisplay(ManageScreens_GetVirtualId(1));
            return true;
        case ID_SCREEN_3:
            App_OpenVirtualDisplay(ManageScreens_GetVirtualId(2));
            return true;
        case ID_OPEN_ALL:
            OpenAllScreens();
            return true;
        case IDM_MANAGE_SCREENS:
            ManageScreens_Show(host, g_instance);
            return true;
        case IDM_SCENES:
            ShowScenesWindow();
            return true;
        default:
            return false;
    }
}

static void EnableHostMenu(HWND host, UINT id, bool on)
{
    if (host == nullptr)
    {
        return;
    }
    HMENU menu = host == g_programWindow ? g_programMenuBar : GetMenu(host);
    EnableMenuItem(menu, id, MF_BYCOMMAND | (on ? MF_ENABLED : MF_GRAYED));
}

void UpdateDocumentMenus()
{
    const bool hasProject = Document_HasProject();
    int indices[PROGRAM_SLIDE_MAX] = {};
    const int selectedSlides = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
    const bool hasSlideSel = selectedSlides > 0 || FirstHighlightedSlide() >= 0;
    const bool hasObject = g_programView == PROGRAM_VIEW_EDIT && EditView_HasSelection();
    const bool canCutCopy = hasObject || selectedSlides > 0;
    const bool canPaste = (g_programView == PROGRAM_VIEW_EDIT && Renderer_FreeformHasClipboard()) ||
        Renderer_HasSlideClipboard();
    const bool oneSlide = selectedSlides == 1;
    const bool canUseSlide = oneSlide && Renderer_IsBackgroundSlide(indices[0]);
    const bool canUseBg = oneSlide && !Renderer_IsBackgroundSlide(indices[0]);

    HWND hosts[2] = { g_mainWindow, g_programWindow };
    for (int i = 0; i < 2; ++i)
    {
        EnableHostMenu(hosts[i], IDM_EDIT_UNDO, History_CanUndo());
        EnableHostMenu(hosts[i], IDM_EDIT_REDO, History_CanRedo());
        EnableHostMenu(hosts[i], IDM_EDIT_CUT, canCutCopy);
        EnableHostMenu(hosts[i], IDM_EDIT_COPY, canCutCopy);
        EnableHostMenu(hosts[i], IDM_EDIT_PASTE, canPaste);
        EnableHostMenu(hosts[i], IDM_EDIT_DUPLICATE, canCutCopy || hasSlideSel);
        EnableHostMenu(hosts[i], IDM_EDIT_DELETE, canCutCopy);
        EnableHostMenu(hosts[i], IDM_SLIDE_NEW, hasProject);
        EnableHostMenu(hosts[i], IDM_SLIDE_DUPLICATE, hasProject && hasSlideSel);
        EnableHostMenu(hosts[i], IDM_SLIDE_QUICK, hasProject);
        EnableHostMenu(hosts[i], IDM_SLIDE_SELECT_ALL, hasProject);
        EnableHostMenu(hosts[i], IDM_SLIDE_NEXT, hasProject);
        EnableHostMenu(hosts[i], IDM_SLIDE_PREV, hasProject);
        EnableHostMenu(hosts[i], IDM_SLIDE_FIRST, hasProject);
        EnableHostMenu(hosts[i], IDM_SLIDE_LAST, hasProject);
        EnableHostMenu(hosts[i], IDM_SLIDE_USE_SLIDE, canUseSlide);
        EnableHostMenu(hosts[i], IDM_SLIDE_USE_BG, canUseBg);
        EnableHostMenu(hosts[i], IDM_SLIDE_EDIT, oneSlide || FirstHighlightedSlide() >= 0);
    }
}

void LoadProgramProjects()
{
    g_programProjectCount = Store_ListProjects(g_programProjects, STORE_PROJECT_MAX);
}

static void SyncVideoPreload()
{
    if (!App_Ready() || !GpuClient_Ready())
    {
        return;
    }
    static wchar_t paths[GPU_PRELOAD_MAX][MAX_PATH];
    const int count = Renderer_CollectVideoPaths(paths, GPU_PRELOAD_MAX);
    GpuClient_Preload(paths, count);
}

void RefreshAfterDocumentChange()
{
    SyncVideoPreload();
    StopSlideXfade();
    CloseTransitionGenie();
    CloseTransDefaultPanel();
    Renderer_NormalizeInheritedTransitions();
    g_programSlideScroll = 0;
    LoadProgramProjects();
    RefreshLeftLib();
    if (g_liveArmed)
    {
        g_liveHold = true;
    }
    if (g_programMiddle != nullptr)
    {
        UpdateProgramMiddleScroll(g_programMiddle);
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
    EditView_Refresh();
    UpdateDocumentMenus();
}

void RefreshSlideLists()
{
    if (g_programMiddle != nullptr)
    {
        UpdateProgramMiddleScroll(g_programMiddle);
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
    EditView_Refresh();
}

bool BrowseForFolder(HWND owner, char* path, int pathChars)
{
    if (path == nullptr || pathChars <= 0)
    {
        return false;
    }

    BROWSEINFOA browse = {};
    browse.hwndOwner = owner;
    browse.lpszTitle = "Choose a folder";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&browse);
    if (pidl == nullptr)
    {
        return false;
    }

    const BOOL ok = SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);
    return ok == TRUE && path[0] != '\0';
}

LRESULT CALLBACK ProjectNameProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_NAME_OK)
            {
                GetDlgItemTextA(hwnd, ID_NAME_EDIT, g_nameResult, sizeof(g_nameResult));
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == ID_NAME_CANCEL)
            {
                g_nameResult[0] = '\0';
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            g_nameResult[0] = '\0';
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_nameDialog = nullptr;
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

bool PromptProjectName(HWND owner, char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return false;
    }

    out[0] = '\0';
    g_nameResult[0] = '\0';
    g_nameDialog = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        "ProjectNameDialog",
        "New Project",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        360,
        160,
        owner,
        nullptr,
        g_instance,
        nullptr
    );
    if (g_nameDialog == nullptr)
    {
        return false;
    }

    CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        16,
        20,
        312,
        24,
        g_nameDialog,
        (HMENU)ID_NAME_EDIT,
        g_instance,
        nullptr
    );
    CreateWindowA(
        "STATIC",
        "Project name",
        WS_CHILD | WS_VISIBLE,
        16,
        2,
        200,
        16,
        g_nameDialog,
        nullptr,
        g_instance,
        nullptr
    );
    CreateWindowA(
        "BUTTON",
        "OK",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        148,
        70,
        80,
        26,
        g_nameDialog,
        (HMENU)ID_NAME_OK,
        g_instance,
        nullptr
    );
    CreateWindowA(
        "BUTTON",
        "Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        238,
        70,
        80,
        26,
        g_nameDialog,
        (HMENU)ID_NAME_CANCEL,
        g_instance,
        nullptr
    );

    RECT ownerRc = {};
    GetWindowRect(owner != nullptr ? owner : GetDesktopWindow(), &ownerRc);
    RECT dialogRc = {};
    GetWindowRect(g_nameDialog, &dialogRc);
    const int width = dialogRc.right - dialogRc.left;
    const int height = dialogRc.bottom - dialogRc.top;
    SetWindowPos(
        g_nameDialog,
        HWND_TOP,
        ownerRc.left + ((ownerRc.right - ownerRc.left - width) / 2),
        ownerRc.top + ((ownerRc.bottom - ownerRc.top - height) / 2),
        0,
        0,
        SWP_NOSIZE
    );

    if (owner != nullptr)
    {
        EnableWindow(owner, FALSE);
    }
    ShowWindow(g_nameDialog, SW_SHOW);
    SetFocus(GetDlgItem(g_nameDialog, ID_NAME_EDIT));

    MSG msg = {};
    while (g_nameDialog != nullptr && GetMessage(&msg, nullptr, 0, 0))
    {
        if (!IsDialogMessageA(g_nameDialog, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (owner != nullptr)
    {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    if (g_nameResult[0] == '\0')
    {
        return false;
    }
    lstrcpynA(out, g_nameResult, outChars);
    return true;
}

void AddPrefRow(HWND parent, const char* label, int y, int editId, int browseId, const char* value)
{
    CreateWindowA("STATIC", label, WS_CHILD | WS_VISIBLE, 16, y, 180, 16, parent, nullptr, g_instance, nullptr);
    HWND edit = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        value,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        16,
        y + 18,
        360,
        24,
        parent,
        (HMENU)(UINT_PTR)editId,
        g_instance,
        nullptr
    );
    CreateWindowA(
        "BUTTON",
        "Browse...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        386,
        y + 16,
        88,
        28,
        parent,
        (HMENU)(UINT_PTR)browseId,
        g_instance,
        nullptr
    );
    (void)edit;
}

void BrowsePrefPath(HWND hwnd, int editId)
{
    char path[MAX_PATH] = {};
    if (!BrowseForFolder(hwnd, path, MAX_PATH))
    {
        return;
    }
    SetDlgItemTextA(hwnd, editId, path);
}

LRESULT CALLBACK PreferencesProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case ID_PREF_BROWSE_FILES:
                    BrowsePrefPath(hwnd, ID_PREF_FILES);
                    return 0;
                case ID_PREF_BROWSE_ASSETS:
                    BrowsePrefPath(hwnd, ID_PREF_ASSETS);
                    return 0;
                case ID_PREF_BROWSE_SCREENS:
                    BrowsePrefPath(hwnd, ID_PREF_SCREENS);
                    return 0;
                case ID_PREF_OK:
                {
                    char files[MAX_PATH] = {};
                    char assets[MAX_PATH] = {};
                    char screens[MAX_PATH] = {};
                    GetDlgItemTextA(hwnd, ID_PREF_FILES, files, MAX_PATH);
                    GetDlgItemTextA(hwnd, ID_PREF_ASSETS, assets, MAX_PATH);
                    GetDlgItemTextA(hwnd, ID_PREF_SCREENS, screens, MAX_PATH);
                    Document_SetFilesDir(files);
                    Document_SetAssetsDir(assets);
                    Document_SetScreensDir(screens);
                    Document_EnsureFolders();
                    DestroyWindow(hwnd);
                    return 0;
                }
                case ID_PREF_CANCEL:
                    DestroyWindow(hwnd);
                    return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_prefsWindow = nullptr;
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void ShowPreferences(HWND owner)
{
    if (g_prefsWindow != nullptr)
    {
        SetForegroundWindow(g_prefsWindow);
        return;
    }

    g_prefsWindow = CreateWindowExA(
        0,
        "PreferencesWindow",
        "Preferences",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        510,
        340,
        owner,
        nullptr,
        g_instance,
        nullptr
    );
    if (g_prefsWindow == nullptr)
    {
        return;
    }

    char files[MAX_PATH] = {};
    char assets[MAX_PATH] = {};
    char screens[MAX_PATH] = {};
    Document_GetFilesDir(files, MAX_PATH);
    Document_GetAssetsDir(assets, MAX_PATH);
    Document_GetScreensDir(screens, MAX_PATH);

    AddPrefRow(g_prefsWindow, "Project files", 16, ID_PREF_FILES, ID_PREF_BROWSE_FILES, files);
    AddPrefRow(g_prefsWindow, "Assets", 80, ID_PREF_ASSETS, ID_PREF_BROWSE_ASSETS, assets);
    AddPrefRow(g_prefsWindow, "Screen configuration files", 144, ID_PREF_SCREENS, ID_PREF_BROWSE_SCREENS, screens);

    CreateWindowA(
        "BUTTON",
        "OK",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        294,
        250,
        88,
        28,
        g_prefsWindow,
        (HMENU)ID_PREF_OK,
        g_instance,
        nullptr
    );
    CreateWindowA(
        "BUTTON",
        "Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        392,
        250,
        88,
        28,
        g_prefsWindow,
        (HMENU)ID_PREF_CANCEL,
        g_instance,
        nullptr
    );

    ShowWindow(g_prefsWindow, SW_SHOW);
}

void StartNewProject(HWND owner)
{
    if (!App_Ready())
    {
        return;
    }

    char name[128] = {};
    if (!PromptProjectName(owner, name, sizeof(name)))
    {
        return;
    }
    if (!Document_NewProject(name))
    {
        MessageBoxA(
            owner,
            "Could not create that project. Pick a different name.",
            "New Project",
            MB_OK | MB_ICONWARNING
        );
        return;
    }
    History_Clear();
    RefreshAfterDocumentChange();
}

void InsertNewSlide(HWND owner)
{
    if (!Document_HasProject())
    {
        return;
    }
    History_Remember();
    if (!Document_AddSlide())
    {
        MessageBoxA(owner, "Could not add a slide to this project.", "New Slide", MB_OK | MB_ICONWARNING);
        return;
    }
    RefreshAfterDocumentChange();
    HighlightSlide(Renderer_SlideCount() - 1);
}

void ShowPreferencesBeta(HWND owner);

bool HandleAppMenuCommand(HWND hwnd, int id)
{
    switch (id)
    {
        case IDM_FILE_NEW_PROJECT:
            StartNewProject(hwnd);
            return true;
        case IDM_FILE_IMPORT:
            ImportMediaFromExplorer(hwnd);
            return true;
        case IDM_FILE_SEARCH_IMPORT:
            ShowSearchToImport(hwnd);
            return true;
        case IDM_FILE_NEW_SLIDE:
        case IDM_SLIDE_NEW:
            InsertNewSlide(hwnd);
            return true;
        case IDM_EDIT_UNDO:
            if (History_Undo())
            {
                RestoreAfterHistory();
            }
            return true;
        case IDM_EDIT_REDO:
            if (History_Redo())
            {
                RestoreAfterHistory();
            }
            return true;
        case IDM_EDIT_CUT:
            ProgramCut();
            return true;
        case IDM_EDIT_COPY:
            ProgramCopy();
            return true;
        case IDM_EDIT_PASTE:
            ProgramPaste();
            return true;
        case IDM_EDIT_DUPLICATE:
        case IDM_SLIDE_DUPLICATE:
            ProgramDuplicate();
            return true;
        case IDM_EDIT_DELETE:
            ProgramDelete();
            return true;
        case IDM_EDIT_PREFERENCES:
            ShowPreferences(hwnd);
            return true;
        case IDM_EDIT_PREFERENCES_BETA:
            ShowPreferencesBeta(hwnd);
            return true;
        case IDM_SLIDE_QUICK:
            SetProgramView(PROGRAM_VIEW_EDIT);
            ShowQuickSlides();
            return true;
        case IDM_SLIDE_SELECT_ALL:
            SelectAllSlides();
            return true;
        case IDM_SLIDE_NEXT:
            if (g_programView == PROGRAM_VIEW_SHOW)
            {
                GoLiveStep(1);
            }
            else
            {
                HighlightStep(1);
            }
            return true;
        case IDM_SLIDE_PREV:
            if (g_programView == PROGRAM_VIEW_SHOW)
            {
                GoLiveStep(-1);
            }
            else
            {
                HighlightStep(-1);
            }
            return true;
        case IDM_SLIDE_FIRST:
            if (g_programView == PROGRAM_VIEW_SHOW)
            {
                GoLiveFirstLast(true);
            }
            else
            {
                HighlightFirstLast(true);
            }
            return true;
        case IDM_SLIDE_LAST:
            if (g_programView == PROGRAM_VIEW_SHOW)
            {
                GoLiveFirstLast(false);
            }
            else
            {
                HighlightFirstLast(false);
            }
            return true;
        case IDM_SLIDE_USE_SLIDE:
            UseSelectedAsSlide();
            return true;
        case IDM_SLIDE_USE_BG:
            UseSelectedAsBackground();
            return true;
        case IDM_SLIDE_EDIT:
            EditSelectedSlide();
            return true;
        default:
            return false;
    }
}

void OpenAllScreens()
{
    const int count = ManageScreens_VirtualCount();
    for (int i = 0; i < count; ++i)
    {
        App_OpenVirtualDisplay(ManageScreens_GetVirtualId(i));
    }
}

void LayoutTopButtons(HWND parent)
{
    RECT rc = {};
    GetClientRect(parent, &rc);


    if (g_programButton != nullptr)
    {
        SetWindowPos(
            g_programButton,
            nullptr,
            rc.right - 390,
            50,
            120,
            40,
            SWP_NOZORDER | SWP_NOACTIVATE
        );
    }
}

void LayoutHostPreview(HWND parent)
{
    if (g_hostPreview == nullptr)
    {
        return;
    }

    RECT rc = {};
    GetClientRect(parent, &rc);

    const int top = 110;
    const int margin = 20;
    const int availW = rc.right - (margin * 2);
    const int availH = rc.bottom - top - margin;
    if (availW < 80 || availH < 48)
    {
        return;
    }

    int sourceW = 800;
    int sourceH = 480;
    if (App_Ready())
    {
        Renderer_GetSlideSize(Renderer_CurrentSlide(), &sourceW, &sourceH);
    }
    if (sourceW < 1)
    {
        sourceW = 800;
    }
    if (sourceH < 1)
    {
        sourceH = 480;
    }

    int width = availW;
    int height = MulDiv(width, sourceH, sourceW);
    if (height > availH)
    {
        height = availH;
        width = MulDiv(height, sourceW, sourceH);
    }

    SetWindowPos(
        g_hostPreview,
        nullptr,
        (rc.right - width) / 2,
        top + ((availH - height) / 2),
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE
    );
}

int ProgramSlideCount()
{
    const int count = Renderer_SlideCount();
    if (count > PROGRAM_SLIDE_MAX)
    {
        return PROGRAM_SLIDE_MAX;
    }
    return count;
}

void MeasureProgramSlideGrid(int clientW, int count, int* cellW, int* cellH, int* cols, int* rows, int* contentH)
{
    const int pad = 10;
    const int gap = 8;
    int innerW = clientW - (pad * 2);
    int columnCount = 4;
    if (innerW < 360)
    {
        columnCount = 1;
    }
    else if (innerW < 640)
    {
        columnCount = 2;
    }
    if (count > 0 && columnCount > count)
    {
        columnCount = count;
    }

    int width = 8;
    if (columnCount > 0)
    {
        width = (innerW - (gap * (columnCount - 1))) / columnCount;
    }
    if (width < 8)
    {
        width = 8;
    }

    int slideW = 1920;
    int slideH = 1080;
    Renderer_GetSlideSize(0, &slideW, &slideH);
    if (slideW < 1)
    {
        slideW = 1920;
    }
    if (slideH < 1)
    {
        slideH = 1080;
    }
    int thumbH = MulDiv(width - 8, slideH, slideW);
    if (thumbH < 8)
    {
        thumbH = 8;
    }
    const int height = thumbH + PROGRAM_SLIDE_CHIN + 8;
    const int rowCount = count > 0 ? ((count + columnCount - 1) / columnCount) : 0;
    int totalH = pad * 2;
    if (rowCount > 0)
    {
        totalH += (rowCount * height) + (gap * (rowCount - 1));
    }

    if (cellW != nullptr)
    {
        *cellW = width;
    }
    if (cellH != nullptr)
    {
        *cellH = height;
    }
    if (cols != nullptr)
    {
        *cols = columnCount;
    }
    if (rows != nullptr)
    {
        *rows = rowCount;
    }
    if (contentH != nullptr)
    {
        *contentH = totalH;
    }
}

void LayoutProgramSlideCells(const RECT* client, RECT* cells, int count)
{
    if (client == nullptr || cells == nullptr || count <= 0)
    {
        return;
    }

    const int pad = 10;
    const int gap = 8;
    int cellW = 8;
    int cellH = 8;
    int cols = 1;
    MeasureProgramSlideGrid(client->right - client->left, count, &cellW, &cellH, &cols, nullptr, nullptr);

    for (int i = 0; i < count; ++i)
    {
        const int col = i % cols;
        const int row = i / cols;
        cells[i].left = client->left + pad + (col * (cellW + gap));
        cells[i].top = client->top + pad + (row * (cellH + gap)) - g_programSlideScroll;
        cells[i].right = cells[i].left + cellW;
        cells[i].bottom = cells[i].top + cellH;
    }
}

void UpdateProgramMiddleScroll(HWND hwnd)
{
    if (hwnd == nullptr)
    {
        return;
    }

    // ShowScrollBar changes the client area, which sends WM_SIZE and lands back
    // here before the outer call has finished. Thumbnail height tracks the pane
    // width, so "needs a scrollbar" can flip on every re-entry (show -> narrower
    // -> fits -> hide -> wider -> overflows -> show ...) and recurse until the
    // stack is exhausted. Never re-enter.
    static bool s_inUpdate = false;
    if (s_inUpdate)
    {
        return;
    }
    s_inUpdate = true;

    RECT client = {};
    GetClientRect(hwnd, &client);
    int contentH = 0;

    // Decide from the width the pane has *without* a scrollbar so the answer
    // depends only on the window size, not on the current scrollbar state.
    const bool scrollVisible = (GetWindowLongPtrA(hwnd, GWL_STYLE) & WS_VSCROLL) != 0;
    const int fullWidth = client.right + (scrollVisible ? GetSystemMetrics(SM_CXVSCROLL) : 0);
    MeasureProgramSlideGrid(fullWidth, ProgramSlideCount(), nullptr, nullptr, nullptr, nullptr, &contentH);

    const bool needScroll = contentH > client.bottom;
    if (needScroll != scrollVisible)
    {
    ShowScrollBar(hwnd, SB_VERT, needScroll ? TRUE : FALSE);
    GetClientRect(hwnd, &client);
    }
    // Scroll range uses the real client width, which is what PaintProgramSlides lays out with.
    MeasureProgramSlideGrid(client.right, ProgramSlideCount(), nullptr, nullptr, nullptr, nullptr, &contentH);

    SCROLLINFO info = {};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = contentH > 0 ? contentH - 1 : 0;
    info.nPage = client.bottom > 0 ? client.bottom : 1;
    if (g_programSlideScroll < 0)
    {
        g_programSlideScroll = 0;
    }
    const int maxPos = contentH > client.bottom ? contentH - client.bottom : 0;
    if (g_programSlideScroll > maxPos)
    {
        g_programSlideScroll = maxPos;
    }
    info.nPos = g_programSlideScroll;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
    if (!needScroll)
    {
        g_programSlideScroll = 0;
    }
    s_inUpdate = false;
}

void ScrollProgramMiddle(HWND hwnd, int delta)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    int contentH = 0;
    MeasureProgramSlideGrid(client.right, ProgramSlideCount(), nullptr, nullptr, nullptr, nullptr, &contentH);
    int maxPos = contentH > client.bottom ? contentH - client.bottom : 0;
    int next = g_programSlideScroll + delta;
    if (next < 0)
    {
        next = 0;
    }
    if (next > maxPos)
    {
        next = maxPos;
    }
    if (next == g_programSlideScroll)
    {
        return;
    }
    g_programSlideScroll = next;
    SetScrollPos(hwnd, SB_VERT, g_programSlideScroll, TRUE);
    InvalidateRect(hwnd, nullptr, FALSE);
}

int HitTestProgramSlide(HWND hwnd, int x, int y)
{
    if (hwnd == nullptr)
    {
        return -1;
    }

    RECT client = {};
    GetClientRect(hwnd, &client);
    RECT cells[PROGRAM_SLIDE_MAX] = {};
    const int count = ProgramSlideCount();
    LayoutProgramSlideCells(&client, cells, count);
    POINT pt = { x, y };
    for (int i = 0; i < count; ++i)
    {
        if (PtInRect(&cells[i], pt))
        {
            return i;
        }
    }
    return -1;
}

int ProgramSlideDropFromPoint(HWND hwnd, int x, int y)
{
    const int count = ProgramSlideCount();
    if (count <= 1)
    {
        return 0;
    }

    RECT client = {};
    GetClientRect(hwnd, &client);
    RECT cells[PROGRAM_SLIDE_MAX] = {};
    LayoutProgramSlideCells(&client, cells, count);

    POINT pt = { x, y };
    for (int i = 0; i < count; ++i)
    {
        if (PtInRect(&cells[i], pt))
        {
            const int mid = (cells[i].left + cells[i].right) / 2;
            return x >= mid ? i + 1 : i;
        }
    }

    if (y < cells[0].top)
    {
        return 0;
    }
    if (y >= cells[count - 1].bottom)
    {
        return count;
    }
    for (int i = 0; i < count; ++i)
    {
        if (y < cells[i].bottom)
        {
            const int mid = (cells[i].left + cells[i].right) / 2;
            return x >= mid ? i + 1 : i;
        }
    }
    return count;
}

void DrawProgramSlideInsertMark(HDC hdc, int x, int top, int bottom)
{
    HPEN pen = CreatePen(PS_SOLID, 2, PROGRAM_COLOR_SELECT);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, top, nullptr);
    LineTo(hdc, x, bottom);
    MoveToEx(hdc, x - 5, top, nullptr);
    LineTo(hdc, x + 5, top);
    MoveToEx(hdc, x - 5, bottom, nullptr);
    LineTo(hdc, x + 5, bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void FinishProgramSlidePointer(bool selectIfClick)
{
    if (g_slideBoxSelecting)
    {
        if (selectIfClick)
        {
            ApplySlideBoxSelection(g_slidePointerCtrl);
            int indices[PROGRAM_SLIDE_MAX] = {};
            const int count = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
            if (count > 0)
            {
                g_slideSelectAnchor = indices[count - 1];
            }
        }
        g_slideBoxSelecting = false;
        if (g_programMiddle != nullptr)
        {
            InvalidateRect(g_programMiddle, nullptr, FALSE);
        }
        return;
    }

    if (g_programSlideHeld < 0)
    {
        return;
    }

    if (g_programSlideDragging)
    {
        History_Remember();
        if (Renderer_MoveSlide(g_programSlideHeld, g_programSlideDrop))
        {
            ClearSlideSelection();
            const int keep = Renderer_CurrentSlide();
            if (keep >= 0 && keep < PROGRAM_SLIDE_MAX)
            {
                g_slideSelected[keep] = true;
                g_slideSelectAnchor = keep;
            }
            Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
        }
        RefreshSlideLists();
    }
    else if (selectIfClick)
    {
        ApplySlideClickSelect(g_programSlideHeld, g_slidePointerCtrl, g_slidePointerShift);
        if (!g_slidePointerCtrl && !g_slidePointerShift)
        {
            SelectSlide(g_programSlideHeld);
        }
    }

    g_programSlideHeld = -1;
    g_programSlideDrop = -1;
    g_programSlideDragging = false;
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
}

void FitProgramSlideCard(const RECT* cell, int sourceW, int sourceH, RECT* thumb, RECT* chin)
{
    const int pad = 6;
    int boxW = cell->right - cell->left - (pad * 2);
    int boxH = cell->bottom - cell->top - (pad * 2) - PROGRAM_SLIDE_CHIN;
    if (boxW < 1)
    {
        boxW = 1;
    }
    if (boxH < 1)
    {
        boxH = 1;
    }
    if (sourceW < 1)
    {
        sourceW = 1;
    }
    if (sourceH < 1)
    {
        sourceH = 1;
    }

    int width = boxW;
    int height = MulDiv(width, sourceH, sourceW);
    if (height > boxH)
    {
        height = boxH;
        width = MulDiv(height, sourceW, sourceH);
    }

    const int cardLeft = cell->left + pad + ((boxW - width) / 2);
    const int cardTop = cell->top + pad;
    thumb->left = cardLeft;
    thumb->top = cardTop;
    thumb->right = cardLeft + width;
    thumb->bottom = cardTop + height;

    chin->left = thumb->left;
    chin->top = thumb->bottom;
    chin->right = thumb->right;
    chin->bottom = thumb->bottom + PROGRAM_SLIDE_CHIN;
}

static void CaptureSlideCardScreen(HWND hwnd, int slide, RECT* out)
{
    if (out == nullptr)
    {
        return;
    }
    SetRectEmpty(out);
    if (hwnd == nullptr || slide < 0)
    {
        return;
    }
    RECT client = {};
    GetClientRect(hwnd, &client);
    RECT cells[PROGRAM_SLIDE_MAX] = {};
    const int count = ProgramSlideCount();
    if (slide >= count)
    {
        return;
    }
    LayoutProgramSlideCells(&client, cells, count);
    int sourceW = 0;
    int sourceH = 0;
    Renderer_GetSlideSize(slide, &sourceW, &sourceH);
    RECT thumb = {};
    RECT chin = {};
    FitProgramSlideCard(&cells[slide], sourceW, sourceH, &thumb, &chin);
    RECT card = { thumb.left, thumb.top, thumb.right, chin.bottom };
    MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&card), 2);
    *out = card;
}

static void PlaceTransitionGenie(int width, int height)
{
    if (g_transGenie == nullptr)
    {
        return;
    }
    RECT card = g_transAnchorCard;
    int x = card.left;
    if (width < TRANS_GENIE_W)
    {
        width = TRANS_GENIE_W;
    }
    const int cardW = card.right - card.left;
    if (cardW > width)
    {
        width = cardW;
    }
    int y = g_transOpenUp ? (card.top - 2 - height) : (card.bottom + 2);
    SetWindowPos(g_transGenie, HWND_TOP, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static void TransRememberOnce()
{
    if (!g_transRemembered)
    {
        History_Remember();
        g_transRemembered = true;
    }
}

static void LoadTransitionDefaultsIntoUi()
{
    if (g_transDefaultEdit == TRANS_DEFAULT_BG)
    {
        g_transCurrent = Renderer_DefaultBgTransition();
        g_transDir = Renderer_DefaultBgTransitionDir();
        g_transMs = Renderer_DefaultBgTransitionMs();
    }
    else if (g_transDefaultEdit == TRANS_DEFAULT_SLIDE)
    {
        g_transCurrent = Renderer_DefaultSlideTransition();
        g_transDir = Renderer_DefaultSlideTransitionDir();
        g_transMs = Renderer_DefaultSlideTransitionMs();
    }
}

static void SaveTransitionDefaultFromUi()
{
    if (g_transDefaultEdit == TRANS_DEFAULT_BG)
    {
        Renderer_SetDefaultBgTransition(g_transCurrent, g_transDir, g_transMs);
    }
    else if (g_transDefaultEdit == TRANS_DEFAULT_SLIDE)
    {
        Renderer_SetDefaultSlideTransition(g_transCurrent, g_transDir, g_transMs);
    }
    // Drop baked Cut/echo overrides so inheriting slides pick up the new global.
    Renderer_NormalizeInheritedTransitions();
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
}

static int FirstTransitionTargetSlide()
{
    int indices[PROGRAM_SLIDE_MAX] = {};
    const int selected = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
    if (selected > 0)
    {
        return indices[0];
    }
    const int current = Renderer_CurrentSlide();
    if (current >= 0 && current < ProgramSlideCount())
    {
        return current;
    }
    return Renderer_EditSlide();
}

static void SyncTransUiFromSelection()
{
    const int slide = FirstTransitionTargetSlide();
    if (slide < 0)
    {
        return;
    }
    g_transCurrent = Renderer_EffectiveSlideTransition(slide);
    g_transDir = Renderer_EffectiveSlideTransitionDir(slide);
    g_transMs = Renderer_EffectiveSlideTransitionMs(slide);
}

static void ApplyTransitionToSelection(int kind, int dir, int ms, bool setKind, bool setDir, bool setMs)
{
    int indices[PROGRAM_SLIDE_MAX] = {};
    int count = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
    if (count < 1)
    {
        const int fallback = FirstTransitionTargetSlide();
        if (fallback < 0)
        {
            return;
        }
        indices[0] = fallback;
        count = 1;
    }

    bool any = false;
    for (int i = 0; i < count; ++i)
    {
        const int slide = indices[i];
        const bool already =
            Renderer_SlideUsesTransitionOverride(slide) &&
            (!setKind || Renderer_GetSlideTransition(slide) == kind) &&
            (!setDir || Renderer_GetSlideTransitionDir(slide) == dir) &&
            (!setMs || Renderer_GetSlideTransitionMs(slide) == ms);
        if (already)
        {
            continue;
        }
        if (!any)
        {
            TransRememberOnce();
            any = true;
        }
        if (setKind)
        {
            Renderer_SetSlideTransition(slide, kind);
        }
        if (setDir)
        {
            Renderer_SetSlideTransitionDir(slide, dir);
        }
        if (setMs)
        {
            Renderer_SetSlideTransitionMs(slide, ms);
        }
    }
    if (any)
    {
        Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    }
    if (g_transDefaultPanel == nullptr && g_transGenie == nullptr)
    {
        if (setKind)
        {
            g_transCurrent = kind;
        }
        if (setDir)
        {
            g_transDir = dir;
        }
        if (setMs)
        {
            g_transMs = ms;
        }
    }
    RefreshTransitionTray();
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
}

static const char* TransDirName(int dir)
{
    switch (dir)
    {
        case SLIDE_TRANSITION_DIR_RIGHT: return "Right";
        case SLIDE_TRANSITION_DIR_UP: return "Up";
        case SLIDE_TRANSITION_DIR_DOWN: return "Down";
        default: return "Left";
    }
}

static int PickTransDir(HWND hwnd, int x, int y, int current)
{
    HMENU menu = CreatePopupMenu();
    const char* labels[4] = { "Left", "Right", "Up", "Down" };
    for (int i = 0; i < 4; ++i)
    {
        AppendMenuA(menu, MF_STRING | (current == i ? MF_CHECKED : 0), i + 1, labels[i]);
    }
    const int cmd = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
        x,
        y,
        0,
        hwnd,
        nullptr
    );
    DestroyMenu(menu);
    return (cmd >= 1 && cmd <= 4) ? cmd - 1 : -1;
}

struct TransLibFooter
{
    RECT slide;
    RECT bg;
    RECT versions;
    RECT minus;
    RECT plus;
    RECT ms;
    bool versionsOn;
};

static void LayoutTransLibFooter(int left, int right, int top, int bottom, TransLibFooter* out)
{
    ZeroMemory(out, sizeof(*out));
    out->versionsOn = Renderer_TransitionNeedsDir(g_transCurrent);
    const int gap = 4;
    const int slideW = 62;
    const int bgW = 42;
    const int verW = out->versionsOn ? 68 : 0;
    const int step = 24;
    int x = left;
    SetRect(&out->slide, x, top, x + slideW, bottom);
    x += slideW + gap;
    SetRect(&out->bg, x, top, x + bgW, bottom);
    x += bgW + gap;
    if (out->versionsOn)
    {
        SetRect(&out->versions, x, top, x + verW, bottom);
        x += verW + gap;
    }
    SetRect(&out->minus, x, top, x + step, bottom);
    SetRect(&out->plus, right - step, top, right, bottom);
    SetRect(&out->ms, out->minus.right + 2, top, out->plus.left - 2, bottom);
}

static void CommitTransitionChoice()
{
    SaveTransitionDefaultFromUi();
    if (g_transDefaultPanel != nullptr)
    {
        InvalidateRect(g_transDefaultPanel, nullptr, FALSE);
    }
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
}

static void FreeTransGridThumbs()
{
    for (int i = 0; i < TRANS_GRID_MAX; ++i)
    {
        if (g_transGrid[i].thumb != nullptr)
        {
            DeleteObject(g_transGrid[i].thumb);
            g_transGrid[i].thumb = nullptr;
        }
        g_transGrid[i].thumbW = 0;
        g_transGrid[i].thumbH = 0;
        g_transGrid[i].thumbDir = SLIDE_TRANSITION_DIR_LEFT;
        if (g_transGrid[i].scrub != nullptr)
        {
            DeleteObject(g_transGrid[i].scrub);
            g_transGrid[i].scrub = nullptr;
        }
        g_transGrid[i].scrubW = 0;
        g_transGrid[i].scrubH = 0;
        g_transGrid[i].scrubRatio = 0.0f;
        g_transGrid[i].scrubDir = SLIDE_TRANSITION_DIR_LEFT;
    }
    if (g_transDemoA != nullptr)
    {
        DeleteObject(g_transDemoA);
        g_transDemoA = nullptr;
    }
    if (g_transDemoB != nullptr)
    {
        DeleteObject(g_transDemoB);
        g_transDemoB = nullptr;
    }
    g_transDemoW = 0;
    g_transDemoH = 0;
    g_transGridReady = false;
    g_transLibHover = -1;
    g_transLibHoverRatio = 0.0f;
    g_transDefaultHoverRatio = 0.0f;
}

static bool LoadTransDemoBitmaps(int width, int height)
{
    if (g_transDemoA != nullptr && g_transDemoB != nullptr &&
        g_transDemoW == width && g_transDemoH == height)
    {
        return true;
    }
    if (g_transDemoA != nullptr)
    {
        DeleteObject(g_transDemoA);
        g_transDemoA = nullptr;
    }
    if (g_transDemoB != nullptr)
    {
        DeleteObject(g_transDemoB);
        g_transDemoB = nullptr;
    }
    wchar_t module[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    wchar_t* slash = wcsrchr(module, L'\\');
    if (slash != nullptr)
    {
        slash[1] = L'\0';
    }
    wchar_t pathA[MAX_PATH] = {};
    wchar_t pathB[MAX_PATH] = {};
    lstrcpynW(pathA, module, MAX_PATH);
    lstrcpynW(pathB, module, MAX_PATH);
    lstrcatW(pathA, L"Assets\\transition_demo_A.jpg");
    lstrcatW(pathB, L"Assets\\transition_demo_B.jpg");
    g_transDemoA = LeftLibLoadImageThumb(pathA, width, height);
    g_transDemoB = LeftLibLoadImageThumb(pathB, width, height);
    if (g_transDemoA == nullptr || g_transDemoB == nullptr)
    {
        if (g_transDemoA != nullptr)
        {
            DeleteObject(g_transDemoA);
            g_transDemoA = nullptr;
        }
        if (g_transDemoB != nullptr)
        {
            DeleteObject(g_transDemoB);
            g_transDemoB = nullptr;
        }
        return false;
    }
    g_transDemoW = width;
    g_transDemoH = height;
    return true;
}

static void EnsureTransGrid()
{
    static const TransGridCell kSeed[TRANS_GRID_MAX] = {
        { SLIDE_TRANSITION_NONE, SLIDE_TRANSITION_DIR_LEFT, "Cut", nullptr, 0, 0, SLIDE_TRANSITION_DIR_LEFT, nullptr, 0, 0, 0.0f, SLIDE_TRANSITION_DIR_LEFT },
        { SLIDE_TRANSITION_CROSSFADE, SLIDE_TRANSITION_DIR_LEFT, "Crossfade", nullptr, 0, 0, SLIDE_TRANSITION_DIR_LEFT, nullptr, 0, 0, 0.0f, SLIDE_TRANSITION_DIR_LEFT },
        { SLIDE_TRANSITION_FADE_BLACK, SLIDE_TRANSITION_DIR_LEFT, "Fade black", nullptr, 0, 0, SLIDE_TRANSITION_DIR_LEFT, nullptr, 0, 0, 0.0f, SLIDE_TRANSITION_DIR_LEFT },
        { SLIDE_TRANSITION_PUSH, SLIDE_TRANSITION_DIR_LEFT, "Push", nullptr, 0, 0, SLIDE_TRANSITION_DIR_LEFT, nullptr, 0, 0, 0.0f, SLIDE_TRANSITION_DIR_LEFT },
        { SLIDE_TRANSITION_WIPE, SLIDE_TRANSITION_DIR_LEFT, "Wipe", nullptr, 0, 0, SLIDE_TRANSITION_DIR_LEFT, nullptr, 0, 0, 0.0f, SLIDE_TRANSITION_DIR_LEFT },
        { SLIDE_TRANSITION_COVER, SLIDE_TRANSITION_DIR_LEFT, "Cover", nullptr, 0, 0, SLIDE_TRANSITION_DIR_LEFT, nullptr, 0, 0, 0.0f, SLIDE_TRANSITION_DIR_LEFT },
    };
    if (!g_transGridReady)
    {
        for (int i = 0; i < TRANS_GRID_MAX; ++i)
        {
            g_transGrid[i] = kSeed[i];
        }
        g_transGridReady = true;
    }
}

static HBITMAP ComposeTransDemoFrame(int kind, int dir, float t, int width, int height)
{
    if (width < 8 || height < 8 || !LoadTransDemoBitmaps(width, height))
    {
        return nullptr;
    }
    unsigned char* from = AllocXfadePlane(width * height * 4);
    unsigned char* to = AllocXfadePlane(width * height * 4);
    unsigned char* out = AllocXfadePlane(width * height * 4);
    if (from == nullptr || to == nullptr || out == nullptr)
    {
        free(from);
        free(to);
        free(out);
        return nullptr;
    }
    HBITMAP dib = nullptr;
    if (GrabBitmap32(g_transDemoA, g_transDemoW, g_transDemoH, width, height, from) &&
        GrabBitmap32(g_transDemoB, g_transDemoW, g_transDemoH, width, height, to))
    {
        if (t < 0.0f)
        {
            t = 0.0f;
        }
        if (t > 1.0f)
        {
            t = 1.0f;
        }
        ComposeTransitionPixels(kind, dir, t, from, to, out, width, height);
        void* bits = nullptr;
        dib = CreateXfadeDib(width, height, &bits);
        if (dib != nullptr && bits != nullptr)
        {
            memcpy(bits, out, static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        }
        else if (dib != nullptr)
        {
            DeleteObject(dib);
            dib = nullptr;
        }
    }
    free(from);
    free(to);
    free(out);
    return dib;
}

static void EnsureTransGridThumb(int index, int width, int height, int dir)
{
    EnsureTransGrid();
    if (index < 0 || index >= TRANS_GRID_MAX || width < 8 || height < 8)
    {
        return;
    }
    TransGridCell* cell = &g_transGrid[index];
    if (!Renderer_TransitionNeedsDir(cell->kind))
    {
        dir = SLIDE_TRANSITION_DIR_LEFT;
    }
    if (cell->thumb != nullptr &&
        cell->thumbW == width &&
        cell->thumbH == height &&
        cell->thumbDir == dir)
    {
        return;
    }
    if (cell->thumb != nullptr)
    {
        DeleteObject(cell->thumb);
        cell->thumb = nullptr;
    }
    // Resting card shows the mid-point of A→B.
    cell->thumb = ComposeTransDemoFrame(cell->kind, dir, 0.5f, width, height);
    cell->thumbW = cell->thumb != nullptr ? width : 0;
    cell->thumbH = cell->thumb != nullptr ? height : 0;
    cell->thumbDir = dir;
}

static void UpdateTransHoverScrub(int index, float ratio, int width, int height, int dir)
{
    EnsureTransGrid();
    if (index < 0 || index >= TRANS_GRID_MAX || width < 8 || height < 8)
    {
        return;
    }
    TransGridCell* cell = &g_transGrid[index];
    if (!Renderer_TransitionNeedsDir(cell->kind))
    {
        dir = SLIDE_TRANSITION_DIR_LEFT;
    }
    if (cell->scrub != nullptr &&
        cell->scrubW == width &&
        cell->scrubH == height &&
        cell->scrubDir == dir &&
        fabsf(cell->scrubRatio - ratio) < 0.02f)
    {
        return;
    }
    if (cell->scrub != nullptr)
    {
        DeleteObject(cell->scrub);
        cell->scrub = nullptr;
    }
    cell->scrub = ComposeTransDemoFrame(cell->kind, dir, ratio, width, height);
    cell->scrubW = cell->scrub != nullptr ? width : 0;
    cell->scrubH = cell->scrub != nullptr ? height : 0;
    cell->scrubRatio = ratio;
    cell->scrubDir = dir;
}

static void ClearTransLibHover(HWND hwnd)
{
    if (g_transLibHover < 0)
    {
        return;
    }
    g_transLibHover = -1;
    g_transLibHoverRatio = 0.0f;
    if (hwnd != nullptr)
    {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

static int g_transStressStep = -1;
static int g_transStressSlideA = -1;
static int g_transStressSlideB = -1;
static int g_transStressPhase = 0; // 0 mirror, 1 span, 2 done
static bool g_transStressGoingToB = true;

struct TransStressCase
{
    int kind;
    int dir;
};

static const TransStressCase g_transStressCases[] = {
    { SLIDE_TRANSITION_NONE, SLIDE_TRANSITION_DIR_LEFT },
    { SLIDE_TRANSITION_CROSSFADE, SLIDE_TRANSITION_DIR_LEFT },
    { SLIDE_TRANSITION_FADE_BLACK, SLIDE_TRANSITION_DIR_LEFT },
    { SLIDE_TRANSITION_PUSH, SLIDE_TRANSITION_DIR_LEFT },
    { SLIDE_TRANSITION_PUSH, SLIDE_TRANSITION_DIR_RIGHT },
    { SLIDE_TRANSITION_PUSH, SLIDE_TRANSITION_DIR_UP },
    { SLIDE_TRANSITION_PUSH, SLIDE_TRANSITION_DIR_DOWN },
    { SLIDE_TRANSITION_WIPE, SLIDE_TRANSITION_DIR_LEFT },
    { SLIDE_TRANSITION_WIPE, SLIDE_TRANSITION_DIR_RIGHT },
    { SLIDE_TRANSITION_WIPE, SLIDE_TRANSITION_DIR_UP },
    { SLIDE_TRANSITION_WIPE, SLIDE_TRANSITION_DIR_DOWN },
    { SLIDE_TRANSITION_COVER, SLIDE_TRANSITION_DIR_LEFT },
    { SLIDE_TRANSITION_COVER, SLIDE_TRANSITION_DIR_RIGHT },
    { SLIDE_TRANSITION_COVER, SLIDE_TRANSITION_DIR_UP },
    { SLIDE_TRANSITION_COVER, SLIDE_TRANSITION_DIR_DOWN },
};
static const int g_transStressCaseCount = (int)(sizeof(g_transStressCases) / sizeof(g_transStressCases[0]));

static bool ImportDemoSlide(const wchar_t* fileName, int* outIndex)
{
    wchar_t module[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    wchar_t* slash = wcsrchr(module, L'\\');
    if (slash != nullptr)
    {
        slash[1] = L'\0';
    }
    wchar_t path[MAX_PATH] = {};
    lstrcpynW(path, module, MAX_PATH);
    lstrcatW(path, L"Assets\\");
    lstrcatW(path, fileName);
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    wchar_t stored[MAX_PATH] = {};
    if (!Document_ImportAsset(path, stored, MAX_PATH))
    {
        // No project / import folder yet — use absolute path as the stored media key.
        lstrcpynW(stored, path, MAX_PATH);
    }
    if (!Renderer_AddFullscreenMediaSlide(stored, false, false))
    {
        return false;
    }
    if (outIndex != nullptr)
    {
        *outIndex = Renderer_CurrentSlide();
    }
    return true;
}

static void StopTransitionStress()
{
    g_transStressStep = -1;
    g_transStressPhase = 2;
    if (g_programWindow != nullptr)
    {
        KillTimer(g_programWindow, ID_TRANS_STRESS_TIMER);
    }
}

static void TickTransitionStress()
{
    if (g_transStressStep < 0 || g_transStressSlideA < 0 || g_transStressSlideB < 0)
    {
        StopTransitionStress();
        return;
    }
    if (SlideXfadeActive())
    {
        return;
    }

    if (g_transStressGoingToB)
    {
        if (g_transStressStep >= g_transStressCaseCount)
        {
            if (g_transStressPhase == 0)
            {
                g_transStressPhase = 1;
                g_transStressStep = 0;
                ManageScreens_SetScreenLayout(ManageScreens_ActiveScreen(), SCREEN_LAYOUT_SPAN);
            }
            else
            {
                StopTransitionStress();
                MessageBoxA(
                    g_programWindow,
                    "Transition stress finished (Mirror + Span with A/B).",
                    "Transitions",
                    MB_OK | MB_ICONINFORMATION
                );
                return;
            }
        }

        const TransStressCase& cse = g_transStressCases[g_transStressStep];
        Renderer_SetSlideTransition(g_transStressSlideB, cse.kind);
        Renderer_SetSlideTransitionDir(g_transStressSlideB, cse.dir);
        Renderer_SetSlideTransitionMs(g_transStressSlideB, 600);
        Renderer_SetSlideTransition(g_transStressSlideA, cse.kind);
        Renderer_SetSlideTransitionDir(g_transStressSlideA, cse.dir);
        Renderer_SetSlideTransitionMs(g_transStressSlideA, 600);
        SelectSlide(g_transStressSlideA);
        SelectSlide(g_transStressSlideB);
        g_transStressGoingToB = false;
        return;
    }

    SelectSlide(g_transStressSlideA);
    g_transStressGoingToB = true;
    g_transStressStep += 1;
}

static void StartTransitionStress()
{
    if (g_programWindow == nullptr)
    {
        return;
    }
    StopTransitionStress();

    while (ManageScreens_GetScreenOutputCount(ManageScreens_ActiveScreen()) < 2)
    {
        if (ManageScreens_AddVirtualDisplay(5) < 0)
        {
            break;
        }
    }
    ManageScreens_SetOutputsEnabled(true);
    ManageScreens_SetScreenLayout(ManageScreens_ActiveScreen(), SCREEN_LAYOUT_MIRROR);

    g_transStressSlideA = -1;
    g_transStressSlideB = -1;
    if (!ImportDemoSlide(L"transition_demo_A.jpg", &g_transStressSlideA) ||
        !ImportDemoSlide(L"transition_demo_B.jpg", &g_transStressSlideB))
    {
        MessageBoxA(
            g_programWindow,
            "Could not import Assets\\transition_demo_A.jpg / B.jpg for stress test.",
            "Transitions",
            MB_OK | MB_ICONWARNING
        );
        return;
    }

    RefreshSlideLists();
    g_transStressPhase = 0;
    g_transStressStep = 0;
    g_transStressGoingToB = true;
    g_liveArmed = true;
    SetTimer(g_programWindow, ID_TRANS_STRESS_TIMER, 750, nullptr);
    TickTransitionStress();
}

static void RefreshTransitionTray()
{
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
    if (g_transGenie != nullptr)
    {
        InvalidateRect(g_transGenie, nullptr, FALSE);
    }
}

static void ApplySlideTransition(int slide, int transition)
{
    if (slide < 0)
    {
        return;
    }
    if (transition == TRANS_GENIE_DEFAULT_KIND)
    {
        if (!Renderer_SlideUsesTransitionOverride(slide))
        {
            g_transCurrent = Renderer_EffectiveSlideTransition(slide);
            g_transDir = Renderer_EffectiveSlideTransitionDir(slide);
            g_transMs = Renderer_EffectiveSlideTransitionMs(slide);
            RefreshTransitionTray();
            return;
        }
        TransRememberOnce();
        Renderer_ClearSlideTransitionOverride(slide);
        Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
        g_transCurrent = Renderer_EffectiveSlideTransition(slide);
        g_transDir = Renderer_EffectiveSlideTransitionDir(slide);
        g_transMs = Renderer_EffectiveSlideTransitionMs(slide);
        RefreshTransitionTray();
        if (g_programMiddle != nullptr)
        {
            InvalidateRect(g_programMiddle, nullptr, FALSE);
        }
        return;
    }

    const int next = transition;
    const bool typeDiff =
        !Renderer_SlideUsesTransitionOverride(slide) ||
        Renderer_GetSlideTransition(slide) != next;
    if (typeDiff)
    {
        TransRememberOnce();
        if (!Renderer_SetSlideTransition(slide, next))
        {
            return;
        }
        Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    }
    g_transCurrent = next;
    RefreshTransitionTray();
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
}

static void ApplySlideTransitionDir(int slide, int direction)
{
    if (slide < 0)
    {
        return;
    }
    const bool dirDiff =
        !Renderer_SlideUsesTransitionOverride(slide) ||
        Renderer_GetSlideTransitionDir(slide) != direction;
    if (dirDiff)
    {
        TransRememberOnce();
        if (!Renderer_SetSlideTransitionDir(slide, direction))
        {
            return;
        }
        Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    }
    g_transDir = direction;
    RefreshTransitionTray();
}

static void ApplyTransitionToAll()
{
    const int count = Renderer_SlideCount();
    if (count < 1)
    {
        return;
    }
    bool any = false;
    for (int i = 0; i < count; ++i)
    {
        const bool typeDiff =
            !Renderer_SlideUsesTransitionOverride(i) ||
            Renderer_GetSlideTransition(i) != g_transCurrent;
        const bool dirDiff =
            !Renderer_SlideUsesTransitionOverride(i) ||
            Renderer_GetSlideTransitionDir(i) != g_transDir;
        const bool msDiff =
            !Renderer_SlideUsesTransitionOverride(i) ||
            Renderer_GetSlideTransitionMs(i) != g_transMs;
        if (!typeDiff && !dirDiff && !msDiff)
        {
            continue;
        }
        if (!any)
        {
            TransRememberOnce();
            any = true;
        }
        Renderer_SetSlideTransition(i, g_transCurrent);
        Renderer_SetSlideTransitionDir(i, g_transDir);
        Renderer_SetSlideTransitionMs(i, g_transMs);
    }
    if (any)
    {
        Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    }
    RefreshTransitionTray();
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
}

static void NudgeTransitionMs(int delta)
{
    if (g_transAnchorSlide < 0)
    {
        return;
    }
    int next = g_transMs + delta;
    if (next < TRANS_MS_MIN)
    {
        next = TRANS_MS_MIN;
    }
    if (next > TRANS_MS_MAX)
    {
        next = TRANS_MS_MAX;
    }
    next = ((next + 50) / 100) * 100;
    if (next == g_transMs && Renderer_SlideUsesTransitionOverride(g_transAnchorSlide))
    {
        return;
    }
    TransRememberOnce();
    if (!Renderer_SetSlideTransitionMs(g_transAnchorSlide, next))
    {
        return;
    }
    g_transMs = Renderer_GetSlideTransitionMs(g_transAnchorSlide);
    Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    RefreshTransitionTray();
}

static int TransGenieHeadH(int clientH)
{
    if (clientH <= TRANS_GENIE_HEAD)
    {
        return clientH;
    }
    return TRANS_GENIE_HEAD;
}

static void TransitionGeniePinRect(const RECT* client, RECT* pin)
{
    if (client == nullptr || pin == nullptr)
    {
        return;
    }
    const int head = TransGenieHeadH(client->bottom - client->top);
    pin->right = client->right - 6;
    pin->left = pin->right - TRANS_PIN_W;
    pin->top = client->top + 4;
    pin->bottom = client->top + head - 4;
    if (pin->bottom < pin->top + 8)
    {
        pin->bottom = pin->top + 8;
    }
}

static int TransitionGenieHit(int x, int y, const RECT* client)
{
    if (client == nullptr)
    {
        return -1;
    }
    POINT pt = { x, y };
    RECT pin = {};
    TransitionGeniePinRect(client, &pin);
    if (PtInRect(&pin, pt))
    {
        const int mid = (pin.top + pin.bottom) / 2;
        return (y < mid) ? 10 : 11;
    }
    const int head = TransGenieHeadH(client->bottom - client->top);
    if (y < client->top + head)
    {
        return -1;
    }
    const int typeBottom = client->top + head + TRANS_GENIE_ROW * TRANS_GENIE_TYPE_ROWS;
    if (y < typeBottom)
    {
        const int row = (y - (client->top + head)) / TRANS_GENIE_ROW;
        if (row < 0 || row >= TRANS_GENIE_TYPE_ROWS)
        {
            return -1;
        }
        return row;
    }
    const int dirBottom = typeBottom + TRANS_GENIE_DIR_H;
    if (y < dirBottom)
    {
        return TRANS_HIT_DIR0;
    }
    if (y < client->bottom)
    {
        return TRANS_HIT_APPLY;
    }
    return -1;
}

static void DrawDurationPinwheel(HDC hdc, const RECT* pin, int ms, int hover)
{
    if (hdc == nullptr || pin == nullptr || pin->right <= pin->left)
    {
        return;
    }
    int tenths = (ms + 50) / 100;
    if (tenths < 1)
    {
        tenths = 1;
    }
    char text[16] = {};
    wsprintfA(text, "%d.%ds", tenths / 10, tenths % 10);

    RECT label = *pin;
    label.right = pin->right - 22;
    SetTextColor(hdc, PROGRAM_COLOR_INK);
    DrawTextA(hdc, text, -1, &label, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const int cx = pin->right - 11;
    const int cy = (pin->top + pin->bottom) / 2;
    const int radius = 9;
    HBRUSH wheel = CreateSolidBrush(hover >= 10 ? PROGRAM_COLOR_WELL_LIGHT : PROGRAM_COLOR_WELL_DARK);
    HPEN ring = CreatePen(PS_SOLID, 1, PROGRAM_COLOR_WELL_LIGHT);
    HPEN oldPen = (HPEN)SelectObject(hdc, ring);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, wheel);
    Ellipse(hdc, cx - radius, cy - radius, cx + radius + 1, cy + radius + 1);

    static const int kTickX[8] = { 0, 5, 7, 5, 0, -5, -7, -5 };
    static const int kTickY[8] = { -7, -5, 0, 5, 7, 5, 0, -5 };
    for (int i = 0; i < 8; ++i)
    {
        const int x0 = cx + ((kTickX[i] * 5) / 8);
        const int y0 = cy + ((kTickY[i] * 5) / 8);
        const int x1 = cx + kTickX[i];
        const int y1 = cy + kTickY[i];
        MoveToEx(hdc, x0, y0, nullptr);
        LineTo(hdc, x1, y1);
    }

    HPEN arrow = CreatePen(PS_SOLID, 1, hover == 10 ? PROGRAM_COLOR_SELECT : PROGRAM_COLOR_INK);
    SelectObject(hdc, arrow);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    POINT up[3] = { { cx, cy - 5 }, { cx - 3, cy - 1 }, { cx + 3, cy - 1 } };
    Polygon(hdc, up, 3);
    DeleteObject(arrow);

    arrow = CreatePen(PS_SOLID, 1, hover == 11 ? PROGRAM_COLOR_SELECT : PROGRAM_COLOR_INK);
    HBRUSH arrowFill = CreateSolidBrush(hover == 11 ? PROGRAM_COLOR_SELECT : PROGRAM_COLOR_INK);
    SelectObject(hdc, arrow);
    SelectObject(hdc, arrowFill);
    POINT down[3] = { { cx, cy + 5 }, { cx - 3, cy + 1 }, { cx + 3, cy + 1 } };
    Polygon(hdc, down, 3);
    POINT upFill[3] = { { cx, cy - 5 }, { cx - 3, cy - 1 }, { cx + 3, cy - 1 } };
    HBRUSH upBrush = CreateSolidBrush(hover == 10 ? PROGRAM_COLOR_SELECT : PROGRAM_COLOR_INK);
    SelectObject(hdc, upBrush);
    Polygon(hdc, upFill, 3);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(upBrush);
    DeleteObject(arrowFill);
    DeleteObject(arrow);
    DeleteObject(ring);
    DeleteObject(wheel);
}

LRESULT CALLBACK TransitionGenieProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_TIMER:
            if (wParam == ID_TRANS_GENIE_TIMER)
            {
                if (g_transStep < TRANS_GENIE_STEPS)
                {
                    g_transStep += 1;
                }
                const int height = 4 + MulDiv(TRANS_GENIE_H - 4, g_transStep, TRANS_GENIE_STEPS);
                RECT card = g_transAnchorCard;
                int width = card.right - card.left;
                if (width < TRANS_GENIE_W)
                {
                    width = TRANS_GENIE_W;
                }
                PlaceTransitionGenie(width, height);
                InvalidateRect(hwnd, nullptr, FALSE);
                if (g_transStep >= TRANS_GENIE_STEPS)
                {
                    KillTimer(hwnd, ID_TRANS_GENIE_TIMER);
                }
                return 0;
            }
            break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HBRUSH fill = CreateSolidBrush(PROGRAM_COLOR_WELL);
            FillRect(hdc, &client, fill);
            DeleteObject(fill);

            const int headH = TransGenieHeadH(client.bottom - client.top);
            RECT head = { client.left + 1, client.top + 1, client.right - 1, client.top + headH };
            if (head.bottom > client.top + 1)
            {
                HBRUSH forehead = CreateSolidBrush(PROGRAM_COLOR_TOP);
                FillRect(hdc, &head, forehead);
                DeleteObject(forehead);
            }

            HFONT font = ProgramUiFont();
            HFONT oldFont = (HFONT)SelectObject(hdc, font);
            SetBkMode(hdc, TRANSPARENT);
            RECT title = head;
            title.left += 8;
            title.right -= (TRANS_PIN_W + 8);
            SetTextColor(hdc, PROGRAM_COLOR_SELECT);
            DrawTextA(hdc, "Transitions", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            RECT pin = {};
            TransitionGeniePinRect(&client, &pin);
            DrawDurationPinwheel(hdc, &pin, g_transMs, g_transHover);

            HPEN rule = CreatePen(PS_SOLID, 1, PROGRAM_COLOR_WELL_LIGHT);
            HPEN oldPen = (HPEN)SelectObject(hdc, rule);
            if (headH > 4)
            {
                MoveToEx(hdc, client.left + 1, client.top + headH, nullptr);
                LineTo(hdc, client.right - 1, client.top + headH);
            }

            const int bodyTop = client.top + headH;
            const int typeValues[TRANS_GENIE_TYPE_ROWS] = {
                TRANS_GENIE_DEFAULT_KIND,
                SLIDE_TRANSITION_NONE,
                SLIDE_TRANSITION_CROSSFADE,
                SLIDE_TRANSITION_FADE_BLACK,
                SLIDE_TRANSITION_PUSH,
                SLIDE_TRANSITION_WIPE,
                SLIDE_TRANSITION_COVER
            };
            const char* typeLabels[TRANS_GENIE_TYPE_ROWS] = {
                "Default", "None", "Crossfade", "Fade black", "Push", "Wipe", "Cover"
            };
            const bool usesOverride = Renderer_SlideUsesTransitionOverride(g_transAnchorSlide);
            for (int i = 0; i < TRANS_GENIE_TYPE_ROWS; ++i)
            {
                RECT row = {
                    client.left + 1,
                    bodyTop + i * TRANS_GENIE_ROW,
                    client.right - 1,
                    bodyTop + (i + 1) * TRANS_GENIE_ROW
                };
                if (g_transHover == i)
                {
                    HBRUSH hover = CreateSolidBrush(PROGRAM_COLOR_WELL_LIGHT);
                    FillRect(hdc, &row, hover);
                    DeleteObject(hover);
                }
                const bool on = (typeValues[i] == TRANS_GENIE_DEFAULT_KIND)
                    ? !usesOverride
                    : (usesOverride && g_transCurrent == typeValues[i]);
                char text[48] = {};
                if (on)
                {
                    wsprintfA(text, "  %s  *", typeLabels[i]);
                }
                else
                {
                    wsprintfA(text, "  %s", typeLabels[i]);
                }
                SetTextColor(hdc, PROGRAM_COLOR_INK);
                DrawTextA(hdc, text, -1, &row, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            const int dirTop = bodyTop + TRANS_GENIE_TYPE_ROWS * TRANS_GENIE_ROW;
            RECT dirRow = {
                client.left + 1,
                dirTop,
                client.right - 1,
                dirTop + TRANS_GENIE_DIR_H
            };
            const bool dirEnabled = Renderer_TransitionNeedsDir(g_transCurrent);
            if (dirEnabled && g_transHover == TRANS_HIT_DIR0)
            {
                HBRUSH hover = CreateSolidBrush(PROGRAM_COLOR_WELL_LIGHT);
                FillRect(hdc, &dirRow, hover);
                DeleteObject(hover);
            }
            char dirText[48] = {};
            if (dirEnabled)
            {
                wsprintfA(dirText, "  %s", TransDirName(g_transDir));
            }
            else
            {
                wsprintfA(dirText, "  Direction");
            }
            SetTextColor(hdc, dirEnabled ? PROGRAM_COLOR_INK : PROGRAM_COLOR_WELL_LIGHT);
            DrawTextA(hdc, dirText, -1, &dirRow, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT apply = {
                client.left + 1,
                dirTop + TRANS_GENIE_DIR_H,
                client.right - 1,
                client.bottom - 1
            };
            if (g_transHover == TRANS_HIT_APPLY)
            {
                HBRUSH hover = CreateSolidBrush(PROGRAM_COLOR_WELL_LIGHT);
                FillRect(hdc, &apply, hover);
                DeleteObject(hover);
            }
            SetTextColor(hdc, PROGRAM_COLOR_SELECT);
            DrawTextA(hdc, "  Apply all", -1, &apply, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, client.left, client.top, client.right, client.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(rule);
            SelectObject(hdc, oldFont);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int hit = TransitionGenieHit(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &client);
            if (hit != g_transHover)
            {
                g_transHover = hit;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int hit = TransitionGenieHit(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &client);
            static const int kTypeValues[TRANS_GENIE_TYPE_ROWS] = {
                TRANS_GENIE_DEFAULT_KIND,
                SLIDE_TRANSITION_NONE,
                SLIDE_TRANSITION_CROSSFADE,
                SLIDE_TRANSITION_FADE_BLACK,
                SLIDE_TRANSITION_PUSH,
                SLIDE_TRANSITION_WIPE,
                SLIDE_TRANSITION_COVER
            };
            if (hit >= 0 && hit < TRANS_GENIE_TYPE_ROWS)
            {
                ApplySlideTransition(g_transAnchorSlide, kTypeValues[hit]);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else if (hit == TRANS_HIT_APPLY)
            {
                ApplyTransitionToAll();
            }
            else if (hit == TRANS_HIT_DIR0)
            {
                if (Renderer_TransitionNeedsDir(g_transCurrent))
                {
                    const int head = TransGenieHeadH(client.bottom - client.top);
                    const int dirTop = client.top + head + TRANS_GENIE_ROW * TRANS_GENIE_TYPE_ROWS;
                    POINT origin = { client.left, dirTop + TRANS_GENIE_DIR_H };
                    ClientToScreen(hwnd, &origin);
                    g_transDirMenuOpen = true;
                    const int dir = PickTransDir(hwnd, origin.x, origin.y, g_transDir);
                    g_transDirMenuOpen = false;
                    if (dir >= 0)
                    {
                        ApplySlideTransitionDir(g_transAnchorSlide, dir);
                    }
                }
            }
            else if (hit == 10)
            {
                NudgeTransitionMs(TRANS_MS_STEP);
            }
            else if (hit == 11)
            {
                NudgeTransitionMs(-TRANS_MS_STEP);
            }
            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            const short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            NudgeTransitionMs(delta > 0 ? TRANS_MS_STEP : -TRANS_MS_STEP);
            return 0;
        }

        case WM_KILLFOCUS:
            if (!g_transDirMenuOpen)
            {
                CloseTransitionGenie();
            }
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                CloseTransitionGenie();
                return 0;
            }
            if (wParam == VK_UP || wParam == VK_RIGHT)
            {
                NudgeTransitionMs(TRANS_MS_STEP);
                return 0;
            }
            if (wParam == VK_DOWN || wParam == VK_LEFT)
            {
                NudgeTransitionMs(-TRANS_MS_STEP);
                return 0;
            }
            break;

        case WM_DESTROY:
            KillTimer(hwnd, ID_TRANS_GENIE_TIMER);
            if (g_transGenie == hwnd)
            {
                g_transGenie = nullptr;
            }
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static bool g_transClosing = false;

static void CloseTransitionGenie()
{
    if (g_transClosing)
    {
        return;
    }
    g_transClosing = true;
    HWND hwnd = g_transGenie;
    g_transGenie = nullptr;
    g_transHover = -1;
    g_transStep = 0;
    g_transRemembered = false;
    if (hwnd != nullptr && IsWindow(hwnd))
    {
        DestroyWindow(hwnd);
    }
    g_transClosing = false;
}

static void ShowTransitionGenie(int slide)
{
    if (slide < 0)
    {
        return;
    }
    CloseTransitionGenie();
    CaptureSlideCardScreen(g_programMiddle, slide, &g_transAnchorCard);
    if (g_transAnchorCard.right <= g_transAnchorCard.left)
    {
        return;
    }
    g_transAnchorSlide = slide;
    g_transCurrent = Renderer_EffectiveSlideTransition(slide);
    g_transDir = Renderer_EffectiveSlideTransitionDir(slide);
    g_transMs = Renderer_EffectiveSlideTransitionMs(slide);
    g_transRemembered = false;
    g_transHover = -1;
    g_transStep = 0;

    RECT work = {};
    HMONITOR monitor = MonitorFromRect(&g_transAnchorCard, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoA(monitor, &info))
    {
        work = info.rcWork;
    }
    else
    {
        work = g_transAnchorCard;
        work.bottom += TRANS_GENIE_H + 8;
    }
    g_transOpenUp = (g_transAnchorCard.bottom + TRANS_GENIE_H + 8 > work.bottom) &&
        (g_transAnchorCard.top - TRANS_GENIE_H - 8 >= work.top);

    HWND owner = g_programWindow != nullptr ? g_programWindow : g_programMiddle;
    g_transGenie = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        "SlideTransitionGenie",
        "",
        WS_POPUP,
        0,
        0,
        TRANS_GENIE_W,
        4,
        owner,
        nullptr,
        g_instance,
        nullptr
    );
    if (g_transGenie == nullptr)
    {
        return;
    }
    PlaceTransitionGenie(TRANS_GENIE_W, 4);
    SetTimer(g_transGenie, ID_TRANS_GENIE_TIMER, 16, nullptr);
    ShowWindow(g_transGenie, SW_SHOW);
    SetForegroundWindow(g_transGenie);
}

// Slide thumbnails, drawn once at their on-screen size and kept in memory.
// Composing them from the full-size images on every repaint stalled the UI
// thread for the length of a crossfade, so the fade showed only its last frame.
struct ProgramSlideThumb
{
    HBITMAP frame;
    unsigned generation;
    int width;
    int height;
    HBITMAP dib;
};
static ProgramSlideThumb g_programSlideThumbs[PROGRAM_SLIDE_MAX] = {};

static void BlitProgramSlideThumb(HDC hdc, int index, int x, int y, int width, int height)
{
    if (hdc == nullptr || width < 1 || height < 1)
    {
        return;
    }
    if (index < 0 || index >= PROGRAM_SLIDE_MAX)
    {
        Renderer_BlitSlideAt(hdc, index, x, y, width, height);
        return;
    }

    HBITMAP frame = Renderer_SlideFrame(index, nullptr, nullptr);
    const unsigned generation = Renderer_FrameGeneration(index);
    ProgramSlideThumb* thumb = &g_programSlideThumbs[index];
    const bool fresh =
        thumb->dib != nullptr &&
        thumb->frame == frame &&
        thumb->generation == generation &&
        thumb->width == width &&
        thumb->height == height;

    if (!fresh)
    {
        if (thumb->dib == nullptr || thumb->width != width || thumb->height != height)
        {
            if (thumb->dib != nullptr)
            {
                DeleteObject(thumb->dib);
                thumb->dib = nullptr;
            }
            void* bits = nullptr;
            thumb->dib = CreateXfadeDib(width, height, &bits);
            thumb->width = thumb->dib != nullptr ? width : 0;
            thumb->height = thumb->dib != nullptr ? height : 0;
        }
        if (thumb->dib == nullptr)
        {
            Renderer_BlitSlideAt(hdc, index, x, y, width, height);
            return;
        }
        HDC mem = CreateCompatibleDC(hdc);
        if (mem == nullptr)
        {
            Renderer_BlitSlideAt(hdc, index, x, y, width, height);
            return;
        }
        HBITMAP old = (HBITMAP)SelectObject(mem, thumb->dib);
        Renderer_BlitSlideAt(mem, index, 0, 0, width, height);
        SelectObject(mem, old);
        DeleteDC(mem);
        thumb->frame = frame;
        thumb->generation = generation;
    }

    HDC mem = CreateCompatibleDC(hdc);
    if (mem == nullptr)
    {
        Renderer_BlitSlideAt(hdc, index, x, y, width, height);
        return;
    }
    HBITMAP old = (HBITMAP)SelectObject(mem, thumb->dib);
    BitBlt(hdc, x, y, width, height, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
}

void PaintProgramSlides(HDC hdc, const RECT* client)
{
    if (hdc == nullptr || client == nullptr || !App_Ready())
    {
        return;
    }

    RECT cells[PROGRAM_SLIDE_MAX] = {};
    const int count = ProgramSlideCount();
    LayoutProgramSlideCells(client, cells, count);
    const int current = Renderer_CurrentSlide();
    HFONT font = ProgramUiFont();
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < count; ++i)
    {
        if (g_programSlideDragging && i == g_programSlideHeld)
        {
            continue;
        }

        int sourceW = 0;
        int sourceH = 0;
        Renderer_GetSlideSize(i, &sourceW, &sourceH);
        RECT thumb = {};
        RECT chin = {};
        FitProgramSlideCard(&cells[i], sourceW, sourceH, &thumb, &chin);

        const bool liveContent = App_IsLive() && i == current;
        const bool liveBackground = g_backgroundActive && i == Renderer_ArmedBackground();
        const bool live = liveContent || liveBackground;
        const bool selected = g_slideSelected[i] || (!live && i == current && !g_slideBoxSelecting);
        if (live || selected)
        {
            RECT ring = thumb;
            InflateRect(&ring, 4, 4);
            SceneRound(
                hdc,
                &ring,
                (float)PROGRAM_SLIDE_RADIUS + 4.0f,
                LOOK_COLOR_SURFACE,
                live ? PROGRAM_COLOR_LIVE_INK : LOOK_COLOR_ACCENT,
                2.0f
            );
        }
        if (thumb.right > thumb.left && thumb.bottom > thumb.top)
        {
            BlitProgramSlideThumb(
                hdc,
                i,
                thumb.left,
                thumb.top,
                thumb.right - thumb.left,
                thumb.bottom - thumb.top
            );
            SceneMaskCorners(hdc, &thumb, (float)PROGRAM_SLIDE_RADIUS, LOOK_COLOR_SURFACE);
        }
        if (live)
        {
            PaintProgramLivePill(hdc, thumb.left + 6, thumb.top + 6);
        }

        char number[8] = {};
        wsprintfA(number, "%d", i + 1);
        const char* tag = "BG";
        if (!Renderer_IsBackgroundSlide(i))
        {
            // Only label real per-slide overrides. Inheriting slides follow the
            // Slide/BG renderer default with no chin tag.
            tag = "";
            if (Renderer_SlideUsesTransitionOverride(i))
            {
                const int kind = Renderer_GetSlideTransition(i);
                if (kind != SLIDE_TRANSITION_NONE)
                {
                    tag = Renderer_TransitionName(kind);
                }
            }
        }
        RECT caption = { chin.left + 2, chin.top + 4, chin.right - 2, chin.bottom };
        ProgramPanelText(
            hdc, number, caption, PROGRAM_FONT_CAPTION_BOLD,
            selected || live ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE
        );
        ProgramPanelText(
            hdc, tag, caption, PROGRAM_FONT_CAPTION, LOOK_COLOR_INK_DIM,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE
        );
    }

    if (g_programSlideDragging && count > 0)
    {
        int slot = g_programSlideDrop;
        if (slot < 0)
        {
            slot = 0;
        }
        if (slot > count)
        {
            slot = count;
        }
        int x = 0;
        int top = 0;
        int bottom = 0;
        if (slot >= count)
        {
            x = cells[count - 1].right;
            top = cells[count - 1].top;
            bottom = cells[count - 1].bottom;
        }
        else
        {
            x = cells[slot].left;
            top = cells[slot].top;
            bottom = cells[slot].bottom;
        }
        DrawProgramSlideInsertMark(hdc, x, top, bottom);
    }

    if (g_slideBoxSelecting)
    {
        const int left = g_slideBoxStart.x < g_slideBoxEnd.x ? g_slideBoxStart.x : g_slideBoxEnd.x;
        const int top = g_slideBoxStart.y < g_slideBoxEnd.y ? g_slideBoxStart.y : g_slideBoxEnd.y;
        const int right = (g_slideBoxStart.x > g_slideBoxEnd.x ? g_slideBoxStart.x : g_slideBoxEnd.x);
        const int bottom = (g_slideBoxStart.y > g_slideBoxEnd.y ? g_slideBoxStart.y : g_slideBoxEnd.y);
        HPEN pen = CreatePen(PS_DOT, 1, PROGRAM_COLOR_SELECT);
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, left, top, right, bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    SelectObject(hdc, oldFont);
}

static void LayoutTransportRow(
    bool show, HWND scrub, HWND back, HWND play, HWND fwd, HWND current, HWND duration,
    int x, int y, int width)
{
    HWND parts[] = { scrub, back, play, fwd, current, duration };
    if (!show)
    {
        for (HWND part : parts)
        {
            if (part != nullptr)
            {
                ShowWindow(part, SW_HIDE);
            }
        }
        return;
    }

    const int btn = 30;
    const int gap = 6;
    const int groupW = (btn * 3) + (gap * 2);
    const int rowY = y + PROGRAM_SCRUB_HIT_H + 2;
    const int btnY = rowY + ((PROGRAM_TRANSPORT_H - btn) / 2);
    const int groupX = x + ((width - groupW) / 2);
    const int timeW = max(0, ((width - groupW) / 2) - 4);
    if (scrub != nullptr)
    {
        MoveWindow(scrub, x, y, width, PROGRAM_SCRUB_HIT_H, TRUE);
    }
    if (back != nullptr)
    {
        MoveWindow(back, groupX, btnY, btn, btn, TRUE);
    }
    if (play != nullptr)
    {
        MoveWindow(play, groupX + btn + gap, btnY, btn, btn, TRUE);
    }
    if (fwd != nullptr)
    {
        MoveWindow(fwd, groupX + ((btn + gap) * 2), btnY, btn, btn, TRUE);
    }
    if (current != nullptr)
    {
        MoveWindow(current, x, rowY, timeW, PROGRAM_TRANSPORT_H, TRUE);
    }
    if (duration != nullptr)
    {
        MoveWindow(duration, x + width - timeW, rowY, timeW, PROGRAM_TRANSPORT_H, TRUE);
    }
    for (HWND part : parts)
    {
        if (part != nullptr)
        {
            ShowWindow(part, SW_SHOW);
        }
    }
}

void LayoutProgramPreview()
{
    if (g_programLeft == nullptr || g_programPreview == nullptr)
    {
        return;
    }

    RECT rc = {};
    GetClientRect(g_programLeft, &rc);
    int availW = rc.right - (PROGRAM_PREVIEW_PAD * 2);
    if (availW < 8 || rc.bottom < 24)
    {
        MoveWindow(g_programPreview, PROGRAM_PREVIEW_PAD, PROGRAM_PREVIEW_PAD, 8, 8, TRUE);
        if (g_programMirror != nullptr)
        {
            MoveWindow(g_programMirror, PROGRAM_PREVIEW_PAD, PROGRAM_PREVIEW_PAD + 14, 8, PROGRAM_MIRROR_H + PROGRAM_MIRROR_DROP, TRUE);
        }
        return;
    }

    int sourceW = 16;
    int sourceH = 9;
    if (!ManageScreens_GetScreenCanvas(PreviewScreenIndex(), &sourceW, &sourceH, nullptr) || sourceW < 1 || sourceH < 1)
    {
        if (App_Ready())
        {
            Renderer_GetSlideSize(Renderer_CurrentSlide(), &sourceW, &sourceH);
        }
    }
    if (sourceW < 1)
    {
        sourceW = 16;
    }
    if (sourceH < 1)
    {
        sourceH = 9;
    }
    if (g_previewSlot < 0 && PreviewSplitBySeat())
    {
        const int seats = ManageScreens_GetScreenOutputCount(PreviewScreenIndex());
        if (seats > 1)
        {
            sourceW = 16 * seats;
            sourceH = 9;
        }
    }

    int width = availW;
    int height = MulDiv(width, sourceH, sourceW);
    int maxH = (rc.bottom * 2) / 5;
    if (maxH < 48)
    {
        maxH = 48;
    }
    const int mirrorBudget = PROGRAM_MIRROR_H + 14 + PROGRAM_SEG_H + 10 +
        (g_mediaPanel != MEDIA_PANEL_NONE ? (PROGRAM_TRANSPORT_BLOCK_H + 8) : 0);
    if (maxH + mirrorBudget + 40 > rc.bottom)
    {
        maxH = rc.bottom - mirrorBudget - 40;
        if (maxH < 48)
        {
            maxH = 48;
        }
    }
    if (height > maxH)
    {
        height = maxH;
        width = MulDiv(height, sourceW, sourceH);
    }

    const int previewX = PROGRAM_PREVIEW_PAD + ((availW - width) / 2);
    MoveWindow(
        g_programPreview,
        previewX,
        PROGRAM_PREVIEW_PAD,
        width,
        height,
        TRUE
    );
    SetWindowRgn(g_programPreview, CreateRoundRectRgn(0, 0, width + 1, height + 1, 16, 16), TRUE);

    int y = PROGRAM_PREVIEW_PAD + height + 10;
    if (g_programMirror != nullptr)
    {
        MoveWindow(
            g_programMirror,
            PROGRAM_PREVIEW_PAD,
            y,
            availW,
            PROGRAM_MIRROR_H + PROGRAM_MIRROR_DROP,
            TRUE
        );
        ShowWindow(g_programMirror, SW_SHOW);
        y += PROGRAM_MIRROR_H + 10;
    }

    const int segX = PROGRAM_PREVIEW_PAD + ((availW - PROGRAM_SEG_W) / 2);
    const int halfW = (PROGRAM_SEG_W - 8) / 2;
    const int halfH = PROGRAM_SEG_H - 8;
    if (g_programVideoBtn != nullptr)
    {
        MoveWindow(g_programVideoBtn, segX + 4, y + 4, halfW, halfH, TRUE);
        ShowWindow(g_programVideoBtn, SW_SHOW);
    }
    if (g_programAudioBtn != nullptr)
    {
        MoveWindow(g_programAudioBtn, segX + 4 + halfW, y + 4, halfW, halfH, TRUE);
        ShowWindow(g_programAudioBtn, SW_SHOW);
    }
    y += PROGRAM_SEG_H + 8;

    const bool showVideo = g_mediaPanel == MEDIA_PANEL_VIDEO;
    const bool showAudio = g_mediaPanel == MEDIA_PANEL_AUDIO;
    LayoutTransportRow(
        showVideo, g_programVideoScrub, g_programVideoBack, g_programVideoPlay, g_programVideoFwd,
        g_programVideoTime, g_programVideoDuration, PROGRAM_PREVIEW_PAD, y, availW
    );
    LayoutTransportRow(
        showAudio, g_programAudioScrub, g_programAudioBack, g_programAudioPlay, g_programAudioFwd,
        g_programAudioLabel, g_programAudioDuration, PROGRAM_PREVIEW_PAD, y, availW
    );
    if (showVideo || showAudio)
    {
        UpdateMediaTransportUi();
    }

    // The project list is drawn on this pane at a Y that follows the preview.
    // Moving the preview only invalidates the strip it uncovered, so the old
    // names stay on screen under the new ones.
    InvalidateRect(g_programLeft, nullptr, FALSE);
}

void RefreshAudioLists()
{
    if (g_audioPlaylists == nullptr || g_audioTracks == nullptr)
    {
        return;
    }

    g_audioListsRefreshing = true;

    AudioPlaylist playlists[AUDIO_PLAYLIST_MAX] = {};
    const int playlistCount = Audio_ListPlaylists(playlists, AUDIO_PLAYLIST_MAX);
    const int previousSel = static_cast<int>(SendMessageA(g_audioPlaylists, LB_GETCURSEL, 0, 0));
    SendMessageA(g_audioPlaylists, LB_RESETCONTENT, 0, 0);

    int selectIndex = 0;
    const int activeId = Audio_ActivePlaylistId();
    for (int i = 0; i < playlistCount; ++i)
    {
        const int index = static_cast<int>(SendMessageA(
            g_audioPlaylists,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(playlists[i].name)
        ));
        SendMessageA(g_audioPlaylists, LB_SETITEMDATA, index, playlists[i].id);
        if (playlists[i].id == activeId)
        {
            selectIndex = index;
        }
    }
    if (playlistCount > 0)
    {
        if (previousSel >= 0 && previousSel < playlistCount && activeId <= 0)
        {
            selectIndex = previousSel;
        }
        SendMessageA(g_audioPlaylists, LB_SETCURSEL, selectIndex, 0);
        const int id = static_cast<int>(SendMessageA(g_audioPlaylists, LB_GETITEMDATA, selectIndex, 0));
        Audio_SetActivePlaylistId(id);
    }

    AudioTrack tracks[AUDIO_TRACK_MAX] = {};
    const int trackCount = Audio_ListTracks(Audio_ActivePlaylistId(), tracks, AUDIO_TRACK_MAX);
    SendMessageA(g_audioTracks, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < trackCount; ++i)
    {
        const int index = static_cast<int>(SendMessageA(
            g_audioTracks,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(tracks[i].name)
        ));
        SendMessageA(g_audioTracks, LB_SETITEMDATA, index, tracks[i].id);
    }

    g_audioListsRefreshing = false;
    if (g_programRight != nullptr)
    {
        InvalidateRect(g_programRight, nullptr, FALSE);
    }
}

void LayoutAudioRail()
{
    if (g_programRight == nullptr || g_audioTracks == nullptr || g_audioPlaylists == nullptr)
    {
        return;
    }

    RECT rc = {};
    GetClientRect(g_programRight, &rc);
    const int inset = PROGRAM_LIST_INSET;
    const int btnH = 32;
    const int halfGap = PROGRAM_GAP / 2;
    const int width = rc.right - (inset * 2);
    if (width < 40 || rc.bottom < 120)
    {
        return;
    }

    int topH = Audio_SplitTop();
    const int minTop = PROGRAM_PANEL_HEAD + 48 + halfGap;
    const int minBottom = PROGRAM_PANEL_HEAD + 48 + btnH + 26 + 20 + halfGap;
    if (topH < minTop)
    {
        topH = minTop;
    }
    if (topH > rc.bottom - minBottom)
    {
        topH = rc.bottom - minBottom;
    }
    g_audioRailSplitY = topH;

    MoveWindow(
        g_audioTracks,
        inset,
        PROGRAM_PANEL_HEAD,
        width,
        topH - halfGap - PROGRAM_PANEL_HEAD - inset,
        TRUE
    );

    const int bottomTop = topH + halfGap;
    const int btnTop = rc.bottom - 10 - btnH;
    const int modeH = 28;
    const int modeTop = btnTop - 8 - modeH;
    const int modeGap = 6;
    const int modeW = (width - modeGap) / 2;
    MoveWindow(
        g_audioPlaylists,
        inset,
        bottomTop + PROGRAM_PANEL_HEAD,
        width,
        modeTop - 8 - (bottomTop + PROGRAM_PANEL_HEAD),
        TRUE
    );
    if (g_audioLoopBtn != nullptr)
    {
        MoveWindow(g_audioLoopBtn, inset, modeTop, modeW, modeH, TRUE);
    }
    if (g_audioShuffleBtn != nullptr)
    {
        MoveWindow(g_audioShuffleBtn, inset + modeW + modeGap, modeTop, width - modeW - modeGap, modeH, TRUE);
    }
    if (g_audioNewPlaylist != nullptr)
    {
        MoveWindow(g_audioNewPlaylist, 10, btnTop, rc.right - 20, btnH, TRUE);
    }
    InvalidateRect(g_programRight, nullptr, FALSE);
}

static WNDPROC g_audioListProc = nullptr;

static void HandleAudioDrop(HWND list, HDROP drop)
{
    int playlistId = Audio_ActivePlaylistId();
    if (list == g_audioPlaylists)
    {
        POINT pt = {};
        DragQueryPoint(drop, &pt);
        const int index = static_cast<int>(SendMessageA(
            g_audioPlaylists,
            LB_ITEMFROMPOINT,
            0,
            MAKELPARAM(pt.x, pt.y)
        ));
        if (HIWORD(index) == 0)
        {
            const int id = static_cast<int>(SendMessageA(g_audioPlaylists, LB_GETITEMDATA, LOWORD(index), 0));
            if (id > 0)
            {
                playlistId = id;
                Audio_SetActivePlaylistId(id);
                SendMessageA(g_audioPlaylists, LB_SETCURSEL, LOWORD(index), 0);
            }
        }
    }

    if (playlistId <= 0)
    {
        playlistId = Audio_AddPlaylist("Playlist 1");
        Audio_SetActivePlaylistId(playlistId);
    }

    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    bool added = false;
    for (UINT i = 0; i < count; ++i)
    {
        wchar_t path[MAX_PATH] = {};
        if (DragQueryFileW(drop, i, path, MAX_PATH) > 0 && Audio_IsAudioExtension(path))
        {
            if (Audio_AddTrackFromFile(playlistId, path))
            {
                added = true;
            }
        }
    }
    DragFinish(drop);
    if (added)
    {
        RefreshAudioLists();
    }
}

static void PaintAudioRow(HWND list, HDC hdc, const RECT* row, int index, bool selected)
{
    HBRUSH fill = CreateSolidBrush(LOOK_COLOR_SURFACE);
    FillRect(hdc, row, fill);
    DeleteObject(fill);
    if (selected)
    {
        SceneRound(hdc, row, 6.0f, LOOK_COLOR_RAISED);
    }

    char text[256] = {};
    SendMessageA(list, LB_GETTEXT, index, reinterpret_cast<LPARAM>(text));
    const int trackId = static_cast<int>(SendMessageA(list, LB_GETITEMDATA, index, 0));
    const bool playing = list == g_audioTracks && trackId > 0 && trackId == Audio_PlayingTrackId();
    RECT textRc = *row;
    textRc.left += 8;
    textRc.right -= 6;
    if (playing)
    {
        SvgIcon_Draw(hdc, "play", textRc.left + 5, (row->top + row->bottom) / 2, 11, LOOK_COLOR_ACCENT);
        textRc.left += 16;
    }
    HFONT old = (HFONT)SelectObject(hdc, ProgramUiFont());
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, selected || playing ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
    DrawTextA(hdc, text, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static void PaintAudioList(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    HBRUSH back = CreateSolidBrush(LOOK_COLOR_SURFACE);
    FillRect(hdc, &client, back);
    DeleteObject(back);

    const int count = static_cast<int>(SendMessageA(hwnd, LB_GETCOUNT, 0, 0));
    if (count <= 0)
    {
        return;
    }

    const int top = static_cast<int>(SendMessageA(hwnd, LB_GETTOPINDEX, 0, 0));
    const int itemH = PROGRAM_AUDIO_ROW;
    for (int i = top; i < count; ++i)
    {
        RECT row = {
            client.left,
            client.top + ((i - top) * itemH),
            client.right,
            client.top + ((i - top + 1) * itemH)
        };
        if (row.top >= client.bottom)
        {
            break;
        }
        PaintAudioRow(hwnd, hdc, &row, i, SendMessageA(hwnd, LB_GETSEL, i, 0) > 0);
    }
}

static void PaintProgramMirrorItem(const DRAWITEMSTRUCT* draw)
{
    HBRUSH back = CreateSolidBrush(LOOK_COLOR_RAISED);
    FillRect(draw->hDC, &draw->rcItem, back);
    DeleteObject(back);
    if (draw->itemID == static_cast<UINT>(-1))
    {
        return;
    }
    const bool hot = (draw->itemState & ODS_SELECTED) != 0;
    if (hot)
    {
        RECT pill = draw->rcItem;
        InflateRect(&pill, -3, -1);
        SceneRound(draw->hDC, &pill, 4.0f, LOOK_COLOR_RAISED_HI);
    }
    char text[160] = {};
    SendMessageA(draw->hwndItem, CB_GETLBTEXT, draw->itemID, reinterpret_cast<LPARAM>(text));
    RECT textRc = draw->rcItem;
    textRc.left += 10;
    textRc.right -= 8;
    HFONT old = (HFONT)SelectObject(draw->hDC, ProgramUiFont());
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, hot ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
    DrawTextA(draw->hDC, text, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(draw->hDC, old);
}

static WNDPROC g_programMirrorProc = nullptr;
static bool g_programMirrorHover = false;

static void PaintProgramMirrorFace(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int w = client.right;
    const int h = client.bottom;
    if (w <= 0 || h <= 0)
    {
        return;
    }
    HDC memory = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memory, bitmap);

    HBRUSH back = CreateSolidBrush(LOOK_COLOR_SURFACE);
    FillRect(memory, &client, back);
    DeleteObject(back);
    const bool open = SendMessageA(hwnd, CB_GETDROPPEDSTATE, 0, 0) != 0;
    const bool focused = GetFocus() == hwnd;
    SceneRound(
        memory,
        &client,
        6.0f,
        open || g_programMirrorHover ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED,
        focused ? RGB(0x6A, 0x6A, 0x6A) : PROGRAM_COLOR_CONTROL_EDGE
    );

    char text[160] = {};
    const int sel = static_cast<int>(SendMessageA(hwnd, CB_GETCURSEL, 0, 0));
    if (sel >= 0)
    {
        SendMessageA(hwnd, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(text));
    }
    RECT textRc = { 10, 0, w - 28, h };
    HFONT old = (HFONT)SelectObject(memory, ProgramUiFont());
    SetBkMode(memory, TRANSPARENT);
    SetTextColor(memory, LOOK_COLOR_INK_BRIGHT);
    DrawTextA(memory, text, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(memory, old);

    {
        Gdiplus::Graphics g(memory);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen pen(
            Gdiplus::Color(255, GetRValue(LOOK_COLOR_INK_DIM), GetGValue(LOOK_COLOR_INK_DIM), GetBValue(LOOK_COLOR_INK_DIM)),
            1.6f
        );
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        const float cx = (float)w - 15.0f;
        const float cy = (float)h / 2.0f;
        const Gdiplus::PointF chevron[3] = {
            Gdiplus::PointF(cx - 4.0f, cy - 2.0f),
            Gdiplus::PointF(cx, cy + 2.0f),
            Gdiplus::PointF(cx + 4.0f, cy - 2.0f),
        };
        g.DrawLines(&pen, chevron, 3);
    }

    BitBlt(hdc, 0, 0, w, h, memory, 0, 0, SRCCOPY);
    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
}

LRESULT CALLBACK ProgramMirrorProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            PaintProgramMirrorFace(hwnd, hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_MOUSEMOVE:
            if (!g_programMirrorHover)
            {
                g_programMirrorHover = true;
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;

        case WM_MOUSELEAVE:
            g_programMirrorHover = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            break;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case CB_SETCURSEL:
        {
            const LRESULT result = CallWindowProcA(g_programMirrorProc, hwnd, uMsg, wParam, lParam);
            InvalidateRect(hwnd, nullptr, FALSE);
            return result;
        }
    }
    return CallWindowProcA(g_programMirrorProc, hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK AudioListProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            PaintAudioList(hwnd, hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_PRINTCLIENT:
            PaintAudioList(hwnd, (HDC)wParam);
            return 0;

        case WM_DROPFILES:
            HandleAudioDrop(hwnd, reinterpret_cast<HDROP>(wParam));
            return 0;

        case WM_RBUTTONUP:
            if (hwnd == g_audioTracks)
            {
                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                const int hit = static_cast<int>(SendMessageA(
                    hwnd,
                    LB_ITEMFROMPOINT,
                    0,
                    MAKELPARAM(x, y)
                ));
                if (HIWORD(hit) == 0)
                {
                    const int index = LOWORD(hit);
                    if (SendMessageA(hwnd, LB_GETSEL, index, 0) <= 0)
                    {
                        SendMessageA(hwnd, LB_SETSEL, FALSE, -1);
                        SendMessageA(hwnd, LB_SETSEL, TRUE, index);
                    }
                }
                POINT pt = { x, y };
                ClientToScreen(hwnd, &pt);
                ShowEditContextMenu(
                    g_programWindow != nullptr ? g_programWindow : GetParent(hwnd),
                    pt.x,
                    pt.y,
                    CTX_TRACKS
                );
                return 0;
            }
            break;

        case WM_CONTEXTMENU:
            if (hwnd == g_audioTracks)
            {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (pt.x == -1 && pt.y == -1)
                {
                    RECT rc = {};
                    GetWindowRect(hwnd, &rc);
                    pt.x = rc.left + 8;
                    pt.y = rc.top + 8;
                }
                ShowEditContextMenu(
                    g_programWindow != nullptr ? g_programWindow : GetParent(hwnd),
                    pt.x,
                    pt.y,
                    CTX_TRACKS
                );
                return 0;
            }
            break;
    }

    if (g_audioListProc != nullptr)
    {
        return CallWindowProcA(g_audioListProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void CreateAudioRail(HWND parent)
{
    EnsureProgramFont();
    g_audioTracks = CreateWindowExA(
        0,
        "LISTBOX",
        "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_EXTENDEDSEL,
        0, 0, 100, 100,
        parent, (HMENU)ID_AUDIO_TRACKS, g_instance, nullptr
    );
    g_audioPlaylists = CreateWindowExA(
        0,
        "LISTBOX",
        "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
        0, 0, 100, 100,
        parent, (HMENU)ID_AUDIO_PLAYLISTS, g_instance, nullptr
    );
    g_audioNewPlaylist = CreateWindowExA(
        0, "MediaChromeButton", "+ New playlist",
        WS_CHILD | WS_VISIBLE,
        0, 0, 100, 32,
        parent, (HMENU)ID_AUDIO_NEW_PLAYLIST, g_instance, nullptr
    );
    g_audioLoopBtn = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 60, 28,
        parent, (HMENU)ID_AUDIO_LOOP, g_instance, nullptr
    );
    g_audioShuffleBtn = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 60, 28,
        parent, (HMENU)ID_AUDIO_SHUFFLE, g_instance, nullptr
    );

    if (g_programFont != nullptr)
    {
        if (g_audioTracks != nullptr)
        {
            SendMessageA(g_audioTracks, WM_SETFONT, (WPARAM)g_programFont, TRUE);
        }
        if (g_audioPlaylists != nullptr)
        {
            SendMessageA(g_audioPlaylists, WM_SETFONT, (WPARAM)g_programFont, TRUE);
        }
    }

    if (g_audioTracks != nullptr)
    {
        g_audioListProc = (WNDPROC)SetWindowLongPtrA(
            g_audioTracks,
            GWLP_WNDPROC,
            (LONG_PTR)AudioListProc
        );
        SetWindowTheme(g_audioTracks, L"DarkMode_Explorer", nullptr);
        DragAcceptFiles(g_audioTracks, TRUE);
    }
    if (g_audioPlaylists != nullptr)
    {
        SetWindowLongPtrA(g_audioPlaylists, GWLP_WNDPROC, (LONG_PTR)AudioListProc);
        SetWindowTheme(g_audioPlaylists, L"DarkMode_Explorer", nullptr);
        DragAcceptFiles(g_audioPlaylists, TRUE);
    }

    RefreshAudioLists();
    LayoutAudioRail();
}

int ProgramPreviewDividerY()
{
    if (g_programLeft == nullptr)
    {
        return 0;
    }

    HWND bottom = nullptr;
    if (g_mediaPanel == MEDIA_PANEL_VIDEO && g_programVideoTime != nullptr && IsWindowVisible(g_programVideoTime))
    {
        bottom = g_programVideoTime;
    }
    else if (g_mediaPanel == MEDIA_PANEL_AUDIO && g_programAudioLabel != nullptr && IsWindowVisible(g_programAudioLabel))
    {
        bottom = g_programAudioLabel;
    }
    else if (g_programAudioBtn != nullptr && IsWindowVisible(g_programAudioBtn))
    {
        // The segmented control's frame sits 4px outside the buttons.
        RECT edge = {};
        GetWindowRect(g_programAudioBtn, &edge);
        MapWindowPoints(nullptr, g_programLeft, reinterpret_cast<POINT*>(&edge), 2);
        return edge.bottom + 4 + 10;
    }
    else if (g_programMirror != nullptr && IsWindowVisible(g_programMirror))
    {
        bottom = g_programMirror;
    }
    else if (g_programPreview != nullptr)
    {
        bottom = g_programPreview;
    }
    if (bottom == nullptr)
    {
        return 0;
    }

    RECT edge = {};
    GetWindowRect(bottom, &edge);
    MapWindowPoints(nullptr, g_programLeft, reinterpret_cast<POINT*>(&edge), 2);
    return edge.bottom + 10;
}

int ProgramProjectListTop()
{
    const int dividerY = ProgramPreviewDividerY();
    if (dividerY <= 0)
    {
        return PROGRAM_PANEL_HEAD;
    }
    return dividerY + PROGRAM_GAP + PROGRAM_PANEL_HEAD;
}

static int ProgramLeftLibTop(const RECT* client)
{
    if (client == nullptr)
    {
        return 0;
    }
    int libH = g_leftLibHeight;
    if (libH < PROGRAM_LIB_MIN)
    {
        libH = PROGRAM_LIB_MIN;
    }
    const int dividerY = ProgramPreviewDividerY();
    const int avail = client->bottom - dividerY - PROGRAM_GAP;
    if (libH > avail - PROGRAM_PROJECTS_MIN)
    {
        libH = avail - PROGRAM_PROJECTS_MIN;
    }
    if (libH < PROGRAM_LIB_MIN)
    {
        libH = PROGRAM_LIB_MIN;
    }
    return client->bottom - libH;
}

static int ProgramProjectsBottom(const RECT* client)
{
    return ProgramLeftLibTop(client) - PROGRAM_GAP;
}

static void EnsureProjectPlaylistSchema()
{
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return;
    }
    char* error = nullptr;
    sqlite3_exec(
        db,
        "CREATE TABLE IF NOT EXISTS project_playlists ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "sort_order INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE IF NOT EXISTS project_playlist_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "playlist_id INTEGER NOT NULL, "
        "project_path TEXT NOT NULL, "
        "sort_order INTEGER NOT NULL DEFAULT 0);",
        nullptr,
        nullptr,
        &error
    );
    sqlite3_free(error);
}

static void RefreshLeftLibPlaylists()
{
    EnsureProjectPlaylistSchema();
    g_leftLibPlaylistCount = 0;
    g_leftLibItemCount = 0;
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT id, name FROM project_playlists ORDER BY sort_order ASC, id ASC;",
            -1,
            &statement,
            nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(statement) == SQLITE_ROW && g_leftLibPlaylistCount < LEFT_LIB_PLAYLIST_MAX)
        {
            LeftLibPlaylist* row = &g_leftLibPlaylists[g_leftLibPlaylistCount++];
            row->id = sqlite3_column_int(statement, 0);
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
            lstrcpynA(row->name, name != nullptr ? name : "Playlist", 128);
        }
        sqlite3_finalize(statement);
    }
    if (g_leftLibPlaylistCount <= 0)
    {
        g_leftLibPlaylistSel = -1;
        return;
    }
    if (g_leftLibPlaylistSel < 0 || g_leftLibPlaylistSel >= g_leftLibPlaylistCount)
    {
        g_leftLibPlaylistSel = 0;
    }
    const int playlistId = g_leftLibPlaylists[g_leftLibPlaylistSel].id;
    if (sqlite3_prepare_v2(
            db,
            "SELECT project_path FROM project_playlist_items WHERE playlist_id = ?1 "
            "ORDER BY sort_order ASC, id ASC;",
            -1,
            &statement,
            nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int(statement, 1, playlistId);
        while (sqlite3_step(statement) == SQLITE_ROW && g_leftLibItemCount < LEFT_LIB_ITEM_MAX)
        {
            LeftLibPlaylistItem* item = &g_leftLibItems[g_leftLibItemCount++];
            const char* path = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
            lstrcpynA(item->path, path != nullptr ? path : "", MAX_PATH);
            const char* slash = strrchr(item->path, '\\');
            const char* name = slash != nullptr ? slash + 1 : item->path;
            lstrcpynA(item->name, name, 128);
            char* dot = strrchr(item->name, '.');
            if (dot != nullptr)
            {
                *dot = '\0';
            }
        }
        sqlite3_finalize(statement);
    }
}

static void FreeLeftLibMediaThumbs()
{
    for (int i = 0; i < g_leftLibMediaCount; ++i)
    {
        if (g_leftLibMedia[i].thumb != nullptr)
        {
            DeleteObject(g_leftLibMedia[i].thumb);
            g_leftLibMedia[i].thumb = nullptr;
        }
        if (g_leftLibMedia[i].scrub != nullptr)
        {
            DeleteObject(g_leftLibMedia[i].scrub);
            g_leftLibMedia[i].scrub = nullptr;
        }
    }
}

static void EnsureMediaLibrarySchema()
{
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return;
    }
    char* error = nullptr;
    sqlite3_exec(
        db,
        "CREATE TABLE IF NOT EXISTS media_library ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "stored TEXT NOT NULL UNIQUE, "
        "is_video INTEGER NOT NULL DEFAULT 0, "
        "sort_order INTEGER NOT NULL DEFAULT 0);",
        nullptr,
        nullptr,
        &error
    );
    sqlite3_free(error);
}

static HBITMAP LeftLibBitmapFromImage(Gdiplus::Image* image, int width, int height)
{
    if (image == nullptr || width < 1 || height < 1)
    {
        return nullptr;
    }
    void* bits = nullptr;
    HBITMAP dib = CreateXfadeDib(width, height, &bits);
    if (dib == nullptr || bits == nullptr)
    {
        return nullptr;
    }
    Gdiplus::Bitmap canvas(width, height, width * 4, PixelFormat32bppRGB, static_cast<BYTE*>(bits));
    Gdiplus::Graphics g(&canvas);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.Clear(Gdiplus::Color(255, 20, 20, 20));
    const int srcW = image->GetWidth();
    const int srcH = image->GetHeight();
    if (srcW > 0 && srcH > 0)
    {
        const float scale = min((float)width / (float)srcW, (float)height / (float)srcH);
        const float dw = srcW * scale;
        const float dh = srcH * scale;
        g.DrawImage(image, (width - dw) * 0.5f, (height - dh) * 0.5f, dw, dh);
    }
    return dib;
}

static HBITMAP LeftLibLoadImageThumb(const wchar_t* path, int width, int height)
{
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(path, FALSE);
    if (bitmap == nullptr || bitmap->GetLastStatus() != Gdiplus::Ok)
    {
        delete bitmap;
        return nullptr;
    }
    HBITMAP dib = LeftLibBitmapFromImage(bitmap, width, height);
    delete bitmap;
    return dib;
}

static HBITMAP LeftLibLoadShellThumb(const wchar_t* path, int width, int height)
{
    IShellItem* item = nullptr;
    if (FAILED(SHCreateItemFromParsingName(path, nullptr, IID_PPV_ARGS(&item))) || item == nullptr)
    {
        return nullptr;
    }
    IShellItemImageFactory* factory = nullptr;
    const HRESULT queried = item->QueryInterface(IID_PPV_ARGS(&factory));
    item->Release();
    if (FAILED(queried) || factory == nullptr)
    {
        return nullptr;
    }
    SIZE size = { max(width, 160), max(height, 90) };
    HBITMAP hbmp = nullptr;
    HRESULT hr = factory->GetImage(size, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &hbmp);
    if (FAILED(hr) || hbmp == nullptr)
    {
        hr = factory->GetImage(size, SIIGBF_RESIZETOFIT | SIIGBF_BIGGERSIZEOK, &hbmp);
    }
    factory->Release();
    if (FAILED(hr) || hbmp == nullptr)
    {
        return nullptr;
    }
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromHBITMAP(hbmp, nullptr);
    DeleteObject(hbmp);
    if (bitmap == nullptr || bitmap->GetLastStatus() != Gdiplus::Ok)
    {
        delete bitmap;
        return nullptr;
    }
    HBITMAP dib = LeftLibBitmapFromImage(bitmap, width, height);
    delete bitmap;
    return dib;
}

static void EnsureLeftLibMediaThumb(LeftLibMediaItem* item, int width, int height)
{
    if (item == nullptr || width < 1 || height < 1)
    {
        return;
    }
    if (item->thumb != nullptr && item->thumbW == width && item->thumbH == height)
    {
        return;
    }
    if (item->thumb != nullptr)
    {
        DeleteObject(item->thumb);
        item->thumb = nullptr;
    }
    wchar_t wide[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, item->path, -1, wide, MAX_PATH);
    if (item->video)
    {
        item->thumb = LeftLibLoadShellThumb(wide, width, height);
    }
    else
    {
        item->thumb = LeftLibLoadImageThumb(wide, width, height);
        if (item->thumb == nullptr)
        {
            item->thumb = LeftLibLoadShellThumb(wide, width, height);
        }
    }
    item->thumbW = item->thumb != nullptr ? width : 0;
    item->thumbH = item->thumb != nullptr ? height : 0;
}

static int LeftLibMediaCardH(int thumbW)
{
    const int thumbH = MulDiv(thumbW, 9, 16);
    return thumbH + PROGRAM_SLIDE_CHIN + 8;
}

static int LeftLibMediaCols(const RECT* body)
{
    if (body == nullptr)
    {
        return 1;
    }
    const int inner = body->right - body->left;
    if (inner < LEFT_LIB_CARD_W)
    {
        return 1;
    }
    int cols = (inner + LEFT_LIB_CARD_GAP) / (LEFT_LIB_CARD_W + LEFT_LIB_CARD_GAP);
    return cols < 1 ? 1 : cols;
}

static int LeftLibMediaPitch()
{
    return LeftLibMediaCardH(LEFT_LIB_CARD_W) + LEFT_LIB_CARD_GAP;
}

static void LeftLibMediaCardRect(const RECT* body, int index, RECT* outThumb, RECT* outCard)
{
    const int cols = LeftLibMediaCols(body);
    const int col = index % cols;
    const int row = index / cols;
    const int cardW = LEFT_LIB_CARD_W;
    const int thumbH = MulDiv(cardW, 9, 16);
    const int cardH = LeftLibMediaCardH(cardW);
    const int left = body->left + col * (cardW + LEFT_LIB_CARD_GAP);
    const int top = body->top - g_leftLibMediaScroll + row * LeftLibMediaPitch();
    if (outCard != nullptr)
    {
        SetRect(outCard, left, top, left + cardW, top + cardH - 4);
    }
    if (outThumb != nullptr)
    {
        SetRect(outThumb, left, top, left + cardW, top + thumbH);
    }
}

static void RefreshLeftLibMedia()
{
    FreeLeftLibMediaThumbs();
    g_leftLibMediaCount = 0;
    g_leftLibMediaSel = -1;
    g_leftLibMediaHover = -1;
    g_leftLibMediaScroll = 0;
    EnsureMediaLibrarySchema();
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT stored, is_video FROM media_library ORDER BY sort_order ASC, id ASC;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return;
    }
    while (sqlite3_step(statement) == SQLITE_ROW && g_leftLibMediaCount < LEFT_LIB_MEDIA_MAX)
    {
        const char* stored = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        if (stored == nullptr || stored[0] == '\0')
        {
            continue;
        }
        wchar_t wideStored[MAX_PATH] = {};
        MultiByteToWideChar(CP_UTF8, 0, stored, -1, wideStored, MAX_PATH);
        wchar_t full[MAX_PATH] = {};
        if (!Document_ResolveAsset(wideStored, full, MAX_PATH))
        {
            continue;
        }
        LeftLibMediaItem* item = &g_leftLibMedia[g_leftLibMediaCount++];
        ZeroMemory(item, sizeof(*item));
        lstrcpynA(item->stored, stored, MAX_PATH);
        WideCharToMultiByte(CP_UTF8, 0, full, -1, item->path, MAX_PATH, nullptr, nullptr);
        item->video = sqlite3_column_int(statement, 1) != 0;
        const char* slash = strrchr(item->path, '\\');
        lstrcpynA(item->name, slash != nullptr ? slash + 1 : item->path, 128);
    }
    sqlite3_finalize(statement);
}

static bool LeftLibEnsureMf()
{
    static bool ready = false;
    static bool failed = false;
    if (ready)
    {
        return true;
    }
    if (failed)
    {
        return false;
    }
    if (FAILED(MFStartup(MF_VERSION)))
    {
        failed = true;
        return false;
    }
    ready = true;
    return true;
}

static bool LeftLibSetReaderRgb(IMFSourceReader* reader, const GUID& subtype)
{
    if (reader == nullptr)
    {
        return false;
    }
    IMFMediaType* want = nullptr;
    if (FAILED(MFCreateMediaType(&want)) || want == nullptr)
    {
        return false;
    }
    want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    want->SetGUID(MF_MT_SUBTYPE, subtype);
    const HRESULT typed = reader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        nullptr,
        want
    );
    want->Release();
    return SUCCEEDED(typed);
}

static HBITMAP LeftLibLoadVideoFrameAt(const wchar_t* path, float ratio, int width, int height)
{
    if (path == nullptr || path[0] == L'\0' || width < 1 || height < 1 || !LeftLibEnsureMf())
    {
        return nullptr;
    }

    IMFAttributes* attrs = nullptr;
    MFCreateAttributes(&attrs, 2);
    if (attrs != nullptr)
    {
        attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }

    IMFSourceReader* reader = nullptr;
    HRESULT opened = MFCreateSourceReaderFromURL(path, attrs, &reader);
    if (attrs != nullptr)
    {
        attrs->Release();
    }
    if (FAILED(opened) || reader == nullptr)
    {
        return nullptr;
    }

    reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    if (!LeftLibSetReaderRgb(reader, MFVideoFormat_RGB32) &&
        !LeftLibSetReaderRgb(reader, MFVideoFormat_RGB24))
    {
        reader->Release();
        return nullptr;
    }

    IMFMediaType* current = nullptr;
    if (FAILED(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &current)) ||
        current == nullptr)
    {
        reader->Release();
        return nullptr;
    }

    UINT32 frameW = 0;
    UINT32 frameH = 0;
    const HRESULT sized = MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &frameW, &frameH);
    UINT32 strideAttr = 0;
    const bool haveStride = SUCCEEDED(current->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideAttr));
    GUID subtype = {};
    current->GetGUID(MF_MT_SUBTYPE, &subtype);
    current->Release();
    if (FAILED(sized) || frameW < 2 || frameH < 2)
    {
        reader->Release();
        return nullptr;
    }
    const UINT32 bpp = (subtype == MFVideoFormat_RGB24) ? 3u : 4u;

    if (ratio < 0.0f)
    {
        ratio = 0.0f;
    }
    if (ratio > 0.999f)
    {
        ratio = 0.999f;
    }
    LONGLONG duration = 0;
    PROPVARIANT dur = {};
    if (SUCCEEDED(reader->GetPresentationAttribute(
            (DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &dur)) &&
        dur.vt == VT_UI8)
    {
        duration = (LONGLONG)dur.uhVal.QuadPart;
    }
    PropVariantClear(&dur);

    PROPVARIANT pos = {};
    PropVariantInit(&pos);
    pos.vt = VT_I8;
    pos.hVal.QuadPart = duration > 0 ? (LONGLONG)((double)duration * (double)ratio) : 0;
    reader->SetCurrentPosition(GUID_NULL, pos);
    PropVariantClear(&pos);

    IMFSample* sample = nullptr;
    for (int attempt = 0; attempt < 48 && sample == nullptr; ++attempt)
    {
        DWORD flags = 0;
        IMFSample* next = nullptr;
        if (FAILED(reader->ReadSample(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, nullptr, &next)))
        {
            break;
        }
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
        {
            if (next != nullptr)
            {
                next->Release();
            }
            break;
        }
        if (next != nullptr)
        {
            sample = next;
        }
    }
    reader->Release();
    if (sample == nullptr)
    {
        return nullptr;
    }

    IMFMediaBuffer* buffer = nullptr;
    if (FAILED(sample->ConvertToContiguousBuffer(&buffer)) || buffer == nullptr)
    {
        sample->Release();
        return nullptr;
    }
    sample->Release();

    BYTE* data = nullptr;
    DWORD maxLen = 0;
    DWORD curLen = 0;
    if (FAILED(buffer->Lock(&data, &maxLen, &curLen)) || data == nullptr || curLen == 0)
    {
        buffer->Release();
        return nullptr;
    }

    INT stride = haveStride ? (INT)strideAttr : (INT)(frameW * bpp);
    if (stride == 0)
    {
        stride = (INT)(frameW * bpp);
    }
    const UINT32 absStride = stride < 0 ? (UINT32)(-stride) : (UINT32)stride;
    const BYTE* firstRow = (stride < 0) ? (data + ((frameH - 1) * absStride)) : data;
    const INT copyStride = (stride < 0) ? -(INT)absStride : (INT)absStride;

    Gdiplus::Bitmap frameBmp((INT)frameW, (INT)frameH, PixelFormat32bppRGB);
    HBITMAP dib = nullptr;
    if (frameBmp.GetLastStatus() == Gdiplus::Ok)
    {
        Gdiplus::BitmapData locked = {};
        Gdiplus::Rect area(0, 0, (INT)frameW, (INT)frameH);
        if (frameBmp.LockBits(&area, Gdiplus::ImageLockModeWrite, PixelFormat32bppRGB, &locked) == Gdiplus::Ok)
        {
            const BYTE* src = firstRow;
            BYTE* dst = static_cast<BYTE*>(locked.Scan0);
            for (UINT32 row = 0; row < frameH; ++row)
            {
                if (bpp == 4)
                {
                    memcpy(dst, src, (size_t)frameW * 4);
                }
                else
                {
                    for (UINT32 col = 0; col < frameW; ++col)
                    {
                        dst[col * 4 + 0] = src[col * 3 + 0];
                        dst[col * 4 + 1] = src[col * 3 + 1];
                        dst[col * 4 + 2] = src[col * 3 + 2];
                        dst[col * 4 + 3] = 255;
                    }
                }
                src += copyStride;
                dst += locked.Stride;
            }
            frameBmp.UnlockBits(&locked);
            dib = LeftLibBitmapFromImage(&frameBmp, width, height);
        }
    }
    buffer->Unlock();
    buffer->Release();
    return dib;
}

static void LeftLibUpdateHoverScrub(int index, float ratio, int thumbW, int thumbH)
{
    if (index < 0 || index >= g_leftLibMediaCount)
    {
        return;
    }
    LeftLibMediaItem* item = &g_leftLibMedia[index];
    if (!item->video)
    {
        return;
    }
    if (item->scrub != nullptr && item->scrubW == thumbW && item->scrubH == thumbH &&
        fabsf(item->scrubRatio - ratio) < 0.03f)
    {
        return;
    }
    if (item->scrub != nullptr)
    {
        DeleteObject(item->scrub);
        item->scrub = nullptr;
    }
    wchar_t wide[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, item->path, -1, wide, MAX_PATH);
    item->scrub = LeftLibLoadVideoFrameAt(wide, ratio, thumbW, thumbH);
    if (item->scrub == nullptr)
    {
        item->scrub = LeftLibLoadShellThumb(wide, thumbW, thumbH);
    }
    item->scrubW = item->scrub != nullptr ? thumbW : 0;
    item->scrubH = item->scrub != nullptr ? thumbH : 0;
    item->scrubRatio = ratio;
}

static bool ScrollLeftLibMedia(HWND hwnd, int wheelClicks)
{
    if (hwnd == nullptr || g_leftLibTab != LEFT_LIB_MEDIA || g_leftLibMediaCount <= 0)
    {
        return false;
    }
    const int cols = LeftLibMediaCols(&g_leftLibMediaBody);
    const int rows = (g_leftLibMediaCount + cols - 1) / cols;
    const int pitch = LeftLibMediaPitch();
    const int content = rows > 0 ? rows * pitch - LEFT_LIB_CARD_GAP : 0;
    const int view = g_leftLibMediaBody.bottom - g_leftLibMediaBody.top;
    int maxScroll = content > view ? content - view : 0;
    const int before = g_leftLibMediaScroll;
    g_leftLibMediaScroll += -wheelClicks * max(pitch / 2, 24);
    if (g_leftLibMediaScroll < 0)
    {
        g_leftLibMediaScroll = 0;
    }
    if (g_leftLibMediaScroll > maxScroll)
    {
        g_leftLibMediaScroll = maxScroll;
    }
    if (g_leftLibMediaScroll != before)
    {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    return true;
}

static void ClearLeftLibMediaHover(HWND hwnd)
{
    if (g_leftLibMediaHover < 0)
    {
        return;
    }
    g_leftLibMediaHover = -1;
    g_leftLibMediaHoverRatio = 0.0f;
    if (hwnd != nullptr)
    {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

static bool ImportLeftLibMedia(const wchar_t* source)
{
    if (source == nullptr || source[0] == L'\0')
    {
        return false;
    }
    if (!IsImageExtension(source) && !IsVideoExtension(source))
    {
        return false;
    }
    wchar_t stored[MAX_PATH] = {};
    if (!Document_ImportAsset(source, stored, MAX_PATH))
    {
        return false;
    }
    EnsureMediaLibrarySchema();
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return false;
    }
    char storedA[MAX_PATH] = {};
    WideCharToMultiByte(CP_UTF8, 0, stored, -1, storedA, MAX_PATH, nullptr, nullptr);
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "INSERT OR IGNORE INTO media_library(stored, is_video, sort_order) VALUES(?1, ?2, "
            "(SELECT COALESCE(MAX(sort_order), 0) + 1 FROM media_library));",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return false;
    }
    sqlite3_bind_text(statement, 1, storedA, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, IsVideoExtension(source) ? 1 : 0);
    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (ok)
    {
        RefreshLeftLibMedia();
    }
    return ok;
}

static void RefreshLeftLib()
{
    RefreshLeftLibMedia();
    RefreshLeftLibPlaylists();
}

struct SearchImportHit
{
    wchar_t path[MAX_PATH];
    char label[MAX_PATH];
    HBITMAP thumb;
    int thumbW;
    int thumbH;
    bool selected;
    bool video;
};

static SearchImportHit g_searchHits[SEARCH_IMPORT_MAX] = {};
static int g_searchHitCount = 0;
static wchar_t g_searchKnown[SEARCH_KNOWN_MAX][MAX_PATH] = {};
static int g_searchKnownCount = 0;
static HWND g_searchWindow = nullptr;
static int g_searchScroll = 0;
static int g_searchHot = SEARCH_HOT_NONE;
static bool g_searchBusy = false;
static char g_searchStatus[180] = {};
static char g_searchFolder[MAX_PATH] = {};

static bool SearchPathHasPrefix(const wchar_t* path, const wchar_t* folder)
{
    const int count = lstrlenW(folder);
    if (path == nullptr || folder == nullptr || count <= 0)
    {
        return false;
    }
    return CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, path, count, folder, count) == CSTR_EQUAL;
}

static void SearchTrimSlash(wchar_t* path)
{
    if (path == nullptr)
    {
        return;
    }
    int count = lstrlenW(path);
    while (count > 0 && (path[count - 1] == L'\\' || path[count - 1] == L'/'))
    {
        if (count == 3 && path[1] == L':')
        {
            break;
        }
        path[--count] = L'\0';
    }
}

static void LoadKnownMediaPaths()
{
    g_searchKnownCount = 0;
    EnsureMediaLibrarySchema();
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT stored FROM media_library;", -1, &statement, nullptr) != SQLITE_OK)
    {
        return;
    }
    while (sqlite3_step(statement) == SQLITE_ROW && g_searchKnownCount < SEARCH_KNOWN_MAX)
    {
        const char* stored = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        if (stored == nullptr || stored[0] == '\0')
        {
            continue;
        }
        wchar_t wideStored[MAX_PATH] = {};
        MultiByteToWideChar(CP_UTF8, 0, stored, -1, wideStored, MAX_PATH);
        wchar_t full[MAX_PATH] = {};
        if (!Document_ResolveAsset(wideStored, full, MAX_PATH))
        {
            continue;
        }
        lstrcpynW(g_searchKnown[g_searchKnownCount], full, MAX_PATH);
        g_searchKnownCount += 1;
    }
    sqlite3_finalize(statement);
}

static bool MediaAlreadyKnown(const wchar_t* fullPath)
{
    if (fullPath == nullptr || fullPath[0] == L'\0')
    {
        return true;
    }
    for (int i = 0; i < g_searchKnownCount; ++i)
    {
        if (lstrcmpiW(g_searchKnown[i], fullPath) == 0)
        {
            return true;
        }
    }
    return false;
}

static void SearchRememberHit(const wchar_t* assets, const wchar_t* fullPath)
{
    if (g_searchHitCount >= SEARCH_IMPORT_MAX || fullPath == nullptr)
    {
        return;
    }
    SearchImportHit* hit = &g_searchHits[g_searchHitCount];
    ZeroMemory(hit, sizeof(*hit));
    lstrcpynW(hit->path, fullPath, MAX_PATH);
    const wchar_t* show = fullPath;
    const int assetsLen = lstrlenW(assets);
    if (SearchPathHasPrefix(fullPath, assets) &&
        (fullPath[assetsLen] == L'\\' || fullPath[assetsLen] == L'/'))
    {
        show = fullPath + assetsLen + 1;
    }
    WideCharToMultiByte(CP_ACP, 0, show, -1, hit->label, MAX_PATH, nullptr, nullptr);
    if (hit->label[0] == '\0')
    {
        lstrcpynA(hit->label, "media", MAX_PATH);
    }
    hit->video = IsVideoExtension(fullPath);
    hit->selected = true;
    g_searchHitCount += 1;
}

static void ScanAssetsFolder(const wchar_t* assets, const wchar_t* folder, int depth)
{
    if (folder == nullptr || folder[0] == L'\0' || depth > 6 || g_searchHitCount >= SEARCH_IMPORT_MAX)
    {
        return;
    }
    wchar_t pattern[MAX_PATH] = {};
    lstrcpynW(pattern, folder, MAX_PATH);
    const int len = lstrlenW(pattern);
    if (len <= 0 || len + 2 >= MAX_PATH)
    {
        return;
    }
    if (pattern[len - 1] != L'\\')
    {
        pattern[len] = L'\\';
        pattern[len + 1] = L'\0';
    }
    lstrcatW(pattern, L"*");

    WIN32_FIND_DATAW data = {};
    HANDLE find = FindFirstFileW(pattern, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if (data.cFileName[0] == L'.' &&
            (data.cFileName[1] == L'\0' || (data.cFileName[1] == L'.' && data.cFileName[2] == L'\0')))
        {
            continue;
        }
        wchar_t full[MAX_PATH] = {};
        lstrcpynW(full, folder, MAX_PATH);
        const int folderLen = lstrlenW(full);
        if (folderLen <= 0 || folderLen + 1 + lstrlenW(data.cFileName) >= MAX_PATH)
        {
            continue;
        }
        if (full[folderLen - 1] != L'\\')
        {
            full[folderLen] = L'\\';
            full[folderLen + 1] = L'\0';
        }
        lstrcatW(full, data.cFileName);
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
            {
                ScanAssetsFolder(assets, full, depth + 1);
            }
            continue;
        }
        if (!IsImageExtension(full) && !IsVideoExtension(full))
        {
            continue;
        }
        if (MediaAlreadyKnown(full))
        {
            continue;
        }
        SearchRememberHit(assets, full);
    }
    while (FindNextFileW(find, &data) && g_searchHitCount < SEARCH_IMPORT_MAX);
    FindClose(find);
}

static bool ReadUserAssetsDir(wchar_t* out, int outChars)
{
    if (out == nullptr || outChars < 2)
    {
        return false;
    }
    out[0] = L'\0';
    char assets[MAX_PATH] = {};
    Document_GetAssetsDir(assets, MAX_PATH);
    if (assets[0] == '\0')
    {
        return false;
    }
    if (MultiByteToWideChar(CP_ACP, 0, assets, -1, out, outChars) <= 0)
    {
        out[0] = L'\0';
        return false;
    }
    SearchTrimSlash(out);
    return out[0] != L'\0';
}

static void FreeSearchThumbs()
{
    for (int i = 0; i < g_searchHitCount; ++i)
    {
        if (g_searchHits[i].thumb != nullptr)
        {
            DeleteObject(g_searchHits[i].thumb);
            g_searchHits[i].thumb = nullptr;
        }
    }
}

static void ScanUnlistedAssets()
{
    FreeSearchThumbs();
    g_searchHitCount = 0;
    g_searchScroll = 0;
    wchar_t assets[MAX_PATH] = {};
    if (!ReadUserAssetsDir(assets, MAX_PATH))
    {
        return;
    }
    LoadKnownMediaPaths();
    ScanAssetsFolder(assets, assets, 0);
}

static bool AddExistingMediaToLibrary(const wchar_t* fullPath)
{
    if (fullPath == nullptr || fullPath[0] == L'\0')
    {
        return false;
    }
    if (!IsImageExtension(fullPath) && !IsVideoExtension(fullPath))
    {
        return false;
    }
    wchar_t assets[MAX_PATH] = {};
    if (!ReadUserAssetsDir(assets, MAX_PATH))
    {
        return false;
    }

    wchar_t storedW[MAX_PATH] = {};
    const int assetsLen = lstrlenW(assets);
    const bool inRoot = SearchPathHasPrefix(fullPath, assets) &&
        (fullPath[assetsLen] == L'\\' || fullPath[assetsLen] == L'/') &&
        wcschr(fullPath + assetsLen + 1, L'\\') == nullptr &&
        wcschr(fullPath + assetsLen + 1, L'/') == nullptr;
    if (inRoot)
    {
        lstrcpynW(storedW, fullPath + assetsLen + 1, MAX_PATH);
    }
    else
    {
        lstrcpynW(storedW, fullPath, MAX_PATH);
    }

    char storedA[MAX_PATH * 4] = {};
    if (WideCharToMultiByte(CP_UTF8, 0, storedW, -1, storedA, (int)sizeof(storedA), nullptr, nullptr) <= 0)
    {
        return false;
    }

    EnsureMediaLibrarySchema();
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "INSERT OR IGNORE INTO media_library(stored, is_video, sort_order) VALUES(?1, ?2, "
            "(SELECT COALESCE(MAX(sort_order), 0) + 1 FROM media_library));",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return false;
    }
    sqlite3_bind_text(statement, 1, storedA, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, IsVideoExtension(fullPath) ? 1 : 0);
    const bool stepped = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return stepped && sqlite3_changes(db) > 0;
}

static int SearchThumbH()
{
    return MulDiv(SEARCH_CARD_W - 12, 9, 16);
}

static int SearchCardH()
{
    return 12 + SearchThumbH() + 28;
}

static int SearchGridTop()
{
    return SEARCH_TITLE_H + SEARCH_HEAD_H;
}

static int SearchCols(int clientW)
{
    const int inner = clientW - SEARCH_PAD * 2;
    int cols = (inner + SEARCH_GAP) / (SEARCH_CARD_W + SEARCH_GAP);
    return cols < 1 ? 1 : cols;
}

static void SearchCardRect(int index, int clientW, RECT* card, RECT* thumb)
{
    const int cols = SearchCols(clientW);
    const int col = index % cols;
    const int row = index / cols;
    const int x = SEARCH_PAD + col * (SEARCH_CARD_W + SEARCH_GAP);
    const int y = SearchGridTop() + row * (SearchCardH() + SEARCH_GAP) - g_searchScroll;
    if (card != nullptr)
    {
        SetRect(card, x, y, x + SEARCH_CARD_W, y + SearchCardH());
    }
    if (thumb != nullptr && card != nullptr)
    {
        SetRect(thumb, card->left + 6, card->top + 6, card->right - 6, card->top + 6 + SearchThumbH());
    }
}

static int SearchMaxScroll(int clientW, int clientH)
{
    if (g_searchHitCount <= 0)
    {
        return 0;
    }
    const int cols = SearchCols(clientW);
    const int rows = (g_searchHitCount + cols - 1) / cols;
    const int content = rows * (SearchCardH() + SEARCH_GAP) - SEARCH_GAP;
    const int view = clientH - SearchGridTop() - SEARCH_FOOT_H;
    return content > view ? content - view : 0;
}

static void SearchClampScroll(int clientW, int clientH)
{
    const int maxScroll = SearchMaxScroll(clientW, clientH);
    if (g_searchScroll < 0)
    {
        g_searchScroll = 0;
    }
    if (g_searchScroll > maxScroll)
    {
        g_searchScroll = maxScroll;
    }
}

static void SearchFooterButtons(int clientW, int clientH, RECT* closeBtn, RECT* importBtn)
{
    const int top = clientH - SEARCH_FOOT_H + 16;
    SetRect(importBtn, clientW - SEARCH_PAD - 108, top, clientW - SEARCH_PAD, top + 32);
    SetRect(closeBtn, importBtn->left - 10 - 88, top, importBtn->left - 10, top + 32);
}

static void SearchCaptionClose(int clientW, RECT* closeRc)
{
    SetRect(closeRc, clientW - PROGRAM_CAPTION_W, 0, clientW, SEARCH_TITLE_H);
}

static int SearchHitAt(int clientW, int clientH, POINT pt)
{
    RECT grid = { SEARCH_PAD, SearchGridTop(), clientW - SEARCH_PAD, clientH - SEARCH_FOOT_H };
    if (!PtInRect(&grid, pt))
    {
        return -1;
    }
    for (int i = 0; i < g_searchHitCount; ++i)
    {
        RECT card = {};
        SearchCardRect(i, clientW, &card, nullptr);
        RECT vis = {};
        if (IntersectRect(&vis, &card, &grid) && PtInRect(&card, pt))
        {
            return i;
        }
    }
    return -1;
}

static int SearchHotAt(int clientW, int clientH, POINT pt)
{
    RECT cap = {};
    SearchCaptionClose(clientW, &cap);
    if (PtInRect(&cap, pt))
    {
        return SEARCH_HOT_CAPTION;
    }
    RECT closeBtn = {};
    RECT importBtn = {};
    SearchFooterButtons(clientW, clientH, &closeBtn, &importBtn);
    if (PtInRect(&importBtn, pt))
    {
        return SEARCH_HOT_IMPORT;
    }
    if (PtInRect(&closeBtn, pt))
    {
        return SEARCH_HOT_CLOSE;
    }
    const int hit = SearchHitAt(clientW, clientH, pt);
    return hit >= 0 ? hit : SEARCH_HOT_NONE;
}

static void SearchEnsureThumb(SearchImportHit* hit)
{
    const int width = SEARCH_CARD_W - 12;
    const int height = SearchThumbH();
    if (hit == nullptr || width < 1 || height < 1)
    {
        return;
    }
    if (hit->thumb != nullptr && hit->thumbW == width && hit->thumbH == height)
    {
        return;
    }
    if (hit->thumb != nullptr)
    {
        DeleteObject(hit->thumb);
        hit->thumb = nullptr;
    }
    if (hit->video)
    {
        hit->thumb = LeftLibLoadShellThumb(hit->path, width, height);
    }
    else
    {
        hit->thumb = LeftLibLoadImageThumb(hit->path, width, height);
        if (hit->thumb == nullptr)
        {
            hit->thumb = LeftLibLoadShellThumb(hit->path, width, height);
        }
    }
    hit->thumbW = hit->thumb != nullptr ? width : 0;
    hit->thumbH = hit->thumb != nullptr ? height : 0;
}

static void SearchDescribeResults()
{
    if (g_searchBusy)
    {
        lstrcpyA(g_searchStatus, "Searching...");
        return;
    }
    if (g_searchHitCount <= 0)
    {
        lstrcpyA(g_searchStatus, "No new pictures or videos. The media tab already has these files.");
    }
    else if (g_searchHitCount == 1)
    {
        lstrcpyA(g_searchStatus, "1 file is not in the media tab yet.");
    }
    else
    {
        wsprintfA(g_searchStatus, "%d files are not in the media tab yet.", g_searchHitCount);
    }
}

static void BeginSearchImportScan(HWND hwnd)
{
    FreeSearchThumbs();
    g_searchHitCount = 0;
    g_searchScroll = 0;
    g_searchBusy = true;
    g_searchHot = SEARCH_HOT_NONE;
    lstrcpyA(g_searchStatus, "Searching...");
    if (hwnd != nullptr)
    {
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
        SetTimer(hwnd, SEARCH_SCAN_TIMER, 30, nullptr);
    }
}

static void NoteMediaLibraryChanged()
{
    g_leftLibTab = LEFT_LIB_MEDIA;
    RefreshLeftLibMedia();
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
}

static bool TakeExplorerMedia(const wchar_t* path, int* added)
{
    if (path == nullptr || path[0] == L'\0')
    {
        return false;
    }
    if (!IsImageExtension(path) && !IsVideoExtension(path))
    {
        return false;
    }
    if (!ImportLeftLibMedia(path))
    {
        return false;
    }
    sqlite3* db = Store_Db();
    if (added != nullptr && db != nullptr && sqlite3_changes(db) > 0)
    {
        *added += 1;
    }
    return true;
}

static void ImportMediaFromExplorer(HWND owner)
{
    wchar_t* buffer = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(wchar_t) * 32768));
    if (buffer == nullptr)
    {
        return;
    }
    wchar_t assets[MAX_PATH] = {};
    ReadUserAssetsDir(assets, MAX_PATH);

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = 32768;
    dialog.lpstrFilter =
        L"Pictures and video\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.mp4;*.mov;*.wmv;*.avi;*.mkv;*.m4v;*.webm\0"
        L"All files\0*.*\0";
    dialog.lpstrTitle = L"Import";
    dialog.lpstrInitialDir = assets[0] != L'\0' ? assets : nullptr;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER |
        OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog) != TRUE)
    {
        HeapFree(GetProcessHeap(), 0, buffer);
        return;
    }

    int added = 0;
    int seen = 0;
    wchar_t* cursor = buffer;
    wchar_t* next = cursor + lstrlenW(cursor) + 1;
    if (next[0] == L'\0')
    {
        if (TakeExplorerMedia(cursor, &added))
        {
            seen += 1;
        }
    }
    else
    {
        const wchar_t* dir = cursor;
        cursor = next;
        while (cursor[0] != L'\0')
        {
            wchar_t full[MAX_PATH] = {};
            lstrcpynW(full, dir, MAX_PATH);
            int len = lstrlenW(full);
            if (len > 0 && full[len - 1] != L'\\' && len + 1 < MAX_PATH)
            {
                full[len] = L'\\';
                full[len + 1] = L'\0';
                len += 1;
            }
            if (len + lstrlenW(cursor) < MAX_PATH)
            {
                lstrcatW(full, cursor);
                if (TakeExplorerMedia(full, &added))
                {
                    seen += 1;
                }
            }
            cursor += lstrlenW(cursor) + 1;
        }
    }
    HeapFree(GetProcessHeap(), 0, buffer);

    if (added > 0)
    {
        NoteMediaLibraryChanged();
        return;
    }
    MessageBoxW(
        owner,
        seen > 0 ? L"Those files are already in the media tab." : L"Those files could not be added to the media tab.",
        L"Import",
        MB_OK | MB_ICONINFORMATION
    );
}

static void ImportSelectedSearchHits(HWND hwnd)
{
    if (g_searchBusy)
    {
        return;
    }
    int wanted = 0;
    int added = 0;
    for (int i = 0; i < g_searchHitCount; ++i)
    {
        if (!g_searchHits[i].selected)
        {
            continue;
        }
        wanted += 1;
        if (AddExistingMediaToLibrary(g_searchHits[i].path))
        {
            added += 1;
        }
    }
    if (wanted <= 0)
    {
        lstrcpyA(g_searchStatus, "Select the files to add to the media tab.");
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
    if (added > 0)
    {
        NoteMediaLibraryChanged();
    }
    ScanUnlistedAssets();
    if (added == 1)
    {
        lstrcpyA(g_searchStatus, "Added 1 file to the media tab.");
    }
    else if (added > 1)
    {
        wsprintfA(g_searchStatus, "Added %d files to the media tab.", added);
    }
    else
    {
        SearchDescribeResults();
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

static void PaintSearchCheck(HDC hdc, RECT rc)
{
    SceneRound(hdc, &rc, 8.0f, LOOK_COLOR_ACCENT);
    HFONT old = (HFONT)SelectObject(hdc, ProgramCaptionGlyphFont());
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, LOOK_COLOR_ACCENT_INK);
    DrawTextW(hdc, L"\xE73E", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static void PaintSearchImport(HWND hwnd, HDC target)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int width = client.right;
    const int height = client.bottom;
    if (width < 2 || height < 2)
    {
        return;
    }

    HDC hdc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(hdc, bitmap);
    SetBkMode(hdc, TRANSPARENT);

    RECT all = { 0, 0, width, height };
    HBRUSH base = CreateSolidBrush(LOOK_COLOR_BASE);
    FillRect(hdc, &all, base);
    DeleteObject(base);

    RECT titleRc = { 18, 0, width - PROGRAM_CAPTION_W, SEARCH_TITLE_H };
    PrefText(hdc, "Search to Import", titleRc, PrefFont(13, FW_SEMIBOLD),
             GetActiveWindow() == hwnd ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT closeRc = {};
    SearchCaptionClose(width, &closeRc);
    if (g_searchHot == SEARCH_HOT_CAPTION)
    {
        HBRUSH red = CreateSolidBrush(RGB(0xC4, 0x2B, 0x1C));
        FillRect(hdc, &closeRc, red);
        DeleteObject(red);
    }
    {
        HFONT old = (HFONT)SelectObject(hdc, ProgramCaptionGlyphFont());
        SetTextColor(hdc, g_searchHot == SEARCH_HOT_CAPTION ? RGB(255, 255, 255) : LOOK_COLOR_INK_DIM);
        DrawTextW(hdc, L"\xE8BB", -1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, old);
    }

    RECT pathRc = { SEARCH_PAD, SEARCH_TITLE_H + 6, width - SEARCH_PAD, SEARCH_TITLE_H + 26 };
    PrefText(hdc, g_searchFolder, pathRc, PrefFont(12, FW_NORMAL), LOOK_COLOR_INK_DIM,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
    RECT hintRc = { SEARCH_PAD, SEARCH_TITLE_H + 28, width - SEARCH_PAD, SEARCH_TITLE_H + SEARCH_HEAD_H - 6 };
    PrefText(hdc, g_searchStatus[0] != '\0' ? g_searchStatus : "Searching...", hintRc, PrefFont(13, FW_NORMAL),
             LOOK_COLOR_INK, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT grid = { 0, SearchGridTop(), width, height - SEARCH_FOOT_H };
    if (g_searchBusy || g_searchHitCount <= 0)
    {
        const char* message = g_searchBusy ? "Searching the assets folder..."
                                           : "Nothing new to import.";
        PrefText(hdc, message, grid, PrefFont(16, FW_NORMAL), LOOK_COLOR_INK_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    else
    {
        HRGN clip = CreateRectRgn(grid.left, grid.top, grid.right, grid.bottom);
        SelectClipRgn(hdc, clip);
        const int thumbW = SEARCH_CARD_W - 12;
        const int thumbH = SearchThumbH();
        for (int i = 0; i < g_searchHitCount; ++i)
        {
            RECT card = {};
            RECT thumb = {};
            SearchCardRect(i, width, &card, &thumb);
            RECT vis = {};
            if (!IntersectRect(&vis, &card, &grid))
            {
                continue;
            }
            SearchEnsureThumb(&g_searchHits[i]);
            const bool hot = g_searchHot == i;
            COLORREF fill = g_searchHits[i].selected ? LOOK_COLOR_RAISED : LOOK_COLOR_SUNKEN;
            if (hot && !g_searchHits[i].selected)
            {
                fill = LOOK_COLOR_RAISED;
            }
            SceneRound(
                hdc,
                &card,
                10.0f,
                fill,
                g_searchHits[i].selected ? LOOK_COLOR_ACCENT : PROGRAM_COLOR_CONTROL_EDGE,
                g_searchHits[i].selected ? 1.5f : 1.0f
            );
            if (g_searchHits[i].thumb != nullptr)
            {
                HDC mem = CreateCompatibleDC(hdc);
                HBITMAP old = (HBITMAP)SelectObject(mem, g_searchHits[i].thumb);
                BitBlt(hdc, thumb.left, thumb.top, thumbW, thumbH, mem, 0, 0, SRCCOPY);
                SelectObject(mem, old);
                DeleteDC(mem);
            }
            RECT chin = { card.left + 8, thumb.bottom + 2, card.right - 8, card.bottom - 4 };
            PrefText(hdc, g_searchHits[i].label, chin, PrefFont(12, FW_NORMAL), LOOK_COLOR_INK,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (g_searchHits[i].selected)
            {
                RECT mark = { card.right - 26, card.top + 8, card.right - 8, card.top + 26 };
                PaintSearchCheck(hdc, mark);
            }
        }
        SelectClipRgn(hdc, nullptr);
        DeleteObject(clip);
    }

    RECT foot = { 0, height - SEARCH_FOOT_H, width, height };
    HBRUSH surface = CreateSolidBrush(LOOK_COLOR_SURFACE);
    FillRect(hdc, &foot, surface);
    DeleteObject(surface);
    RECT footLine = { 0, foot.top, width, foot.top + 1 };
    HBRUSH line = CreateSolidBrush(PROGRAM_COLOR_LINE);
    FillRect(hdc, &footLine, line);
    DeleteObject(line);

    RECT closeBtn = {};
    RECT importBtn = {};
    SearchFooterButtons(width, height, &closeBtn, &importBtn);
    PaintPrefButton(hdc, &closeBtn, "Close", false, g_searchHot == SEARCH_HOT_CLOSE);
    PaintPrefButton(hdc, &importBtn, "Import", true, g_searchHot == SEARCH_HOT_IMPORT && !g_searchBusy && g_searchHitCount > 0);

    BitBlt(target, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
}

LRESULT CALLBACK SearchImportProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_NCCALCSIZE:
            if (wParam)
            {
                return 0;
            }
            break;

        case WM_NCHITTEST:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT client = {};
            GetClientRect(hwnd, &client);
            RECT closeRc = {};
            SearchCaptionClose(client.right, &closeRc);
            if (pt.y >= 0 && pt.y < SEARCH_TITLE_H && !PtInRect(&closeRc, pt))
            {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            PaintSearchImport(hwnd, hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_TIMER:
            if (wParam == SEARCH_SCAN_TIMER)
            {
                KillTimer(hwnd, SEARCH_SCAN_TIMER);
                if (g_searchWindow == hwnd)
                {
                    ScanUnlistedAssets();
                    g_searchBusy = false;
                    SearchDescribeResults();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            break;

        case WM_MOUSEMOVE:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int hot = SearchHotAt(client.right, client.bottom, pt);
            if (hot != g_searchHot)
            {
                g_searchHot = hot;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&track);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (g_searchHot != SEARCH_HOT_NONE)
            {
                g_searchHot = SEARCH_HOT_NONE;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && g_searchHot != SEARCH_HOT_NONE)
            {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
            break;

        case WM_LBUTTONDOWN:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int hot = SearchHotAt(client.right, client.bottom, pt);
            if (hot == SEARCH_HOT_CAPTION || hot == SEARCH_HOT_CLOSE)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            if (hot == SEARCH_HOT_IMPORT)
            {
                ImportSelectedSearchHits(hwnd);
                return 0;
            }
            if (hot >= 0 && hot < g_searchHitCount)
            {
                g_searchHits[hot].selected = !g_searchHits[hot].selected;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int clicks = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            g_searchScroll += -clicks * 72;
            SearchClampScroll(client.right, client.bottom);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_ACTIVATE:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, SEARCH_SCAN_TIMER);
            FreeSearchThumbs();
            g_searchHitCount = 0;
            g_searchBusy = false;
            g_searchWindow = nullptr;
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void ShowSearchToImport(HWND owner)
{
    wchar_t assets[MAX_PATH] = {};
    if (!ReadUserAssetsDir(assets, MAX_PATH))
    {
        MessageBoxA(owner, "Set an assets folder in Preferences first.", "Search to Import", MB_OK | MB_ICONWARNING);
        return;
    }
    WideCharToMultiByte(CP_ACP, 0, assets, -1, g_searchFolder, MAX_PATH, nullptr, nullptr);

    if (g_searchWindow != nullptr)
    {
        BeginSearchImportScan(g_searchWindow);
        SetForegroundWindow(g_searchWindow);
        return;
    }

    RECT ownerRc = {};
    if (owner == nullptr || !GetWindowRect(owner, &ownerRc))
    {
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &ownerRc, 0);
    }
    const int x = ownerRc.left + ((ownerRc.right - ownerRc.left) - SEARCH_W) / 2;
    const int y = ownerRc.top + ((ownerRc.bottom - ownerRc.top) - SEARCH_H) / 2;
    g_searchBusy = true;
    g_searchHitCount = 0;
    g_searchScroll = 0;
    lstrcpyA(g_searchStatus, "Searching...");
    g_searchWindow = CreateWindowExA(
        0,
        "SearchImportWindow",
        "Search to Import",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x > 0 ? x : 0,
        y > 0 ? y : 0,
        SEARCH_W,
        SEARCH_H,
        owner,
        nullptr,
        g_instance,
        nullptr
    );
    if (g_searchWindow == nullptr)
    {
        g_searchBusy = false;
        return;
    }
    ApplySceneTitleBar(g_searchWindow);
    const DWORD round = 2;
    DwmSetWindowAttribute(g_searchWindow, 33, &round, sizeof(round));
    SetWindowPos(g_searchWindow, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    ShowWindow(g_searchWindow, SW_SHOW);
    BeginSearchImportScan(g_searchWindow);
}
static int AddLeftLibPlaylist(const char* name)
{
    EnsureProjectPlaylistSchema();
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return 0;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "INSERT INTO project_playlists(name, sort_order) VALUES(?1, "
            "(SELECT COALESCE(MAX(sort_order), 0) + 1 FROM project_playlists));",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(statement, 1, name != nullptr ? name : "Playlist", -1, SQLITE_TRANSIENT);
    const int ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!ok)
    {
        return 0;
    }
    RefreshLeftLibPlaylists();
    g_leftLibPlaylistSel = g_leftLibPlaylistCount - 1;
    return g_leftLibPlaylistCount > 0 ? g_leftLibPlaylists[g_leftLibPlaylistSel].id : 0;
}

static bool AddCurrentProjectToLeftLibPlaylist()
{
    if (g_leftLibPlaylistSel < 0 || g_leftLibPlaylistSel >= g_leftLibPlaylistCount)
    {
        return false;
    }
    char path[MAX_PATH] = {};
    Document_Path(path, MAX_PATH);
    if (path[0] == '\0')
    {
        return false;
    }
    EnsureProjectPlaylistSchema();
    sqlite3* db = Store_Db();
    if (db == nullptr)
    {
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "INSERT INTO project_playlist_items(playlist_id, project_path, sort_order) VALUES(?1, ?2, "
            "(SELECT COALESCE(MAX(sort_order), 0) + 1 FROM project_playlist_items WHERE playlist_id = ?1));",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return false;
    }
    sqlite3_bind_int(statement, 1, g_leftLibPlaylists[g_leftLibPlaylistSel].id);
    sqlite3_bind_text(statement, 2, path, -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (ok)
    {
        RefreshLeftLibPlaylists();
    }
    return ok;
}

void DrawProgramProjectInsertMark(HDC hdc, int left, int right, int y)
{
    HPEN pen = CreatePen(PS_SOLID, 2, PROGRAM_COLOR_SELECT);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, left, y, nullptr);
    LineTo(hdc, right, y);
    MoveToEx(hdc, left, y - 5, nullptr);
    LineTo(hdc, left, y + 5);
    MoveToEx(hdc, right, y - 5, nullptr);
    LineTo(hdc, right, y + 5);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void PaintProgramLibrary(HDC hdc, const RECT* client)
{
    if (hdc == nullptr || client == nullptr)
    {
        return;
    }
    const int libTop = ProgramLeftLibTop(client);
    if (libTop <= 0 || libTop >= client->bottom)
    {
        return;
    }
    RECT panel = { client->left, libTop, client->right, client->bottom };
    SceneRound(hdc, &panel, (float)PROGRAM_PANEL_RADIUS, LOOK_COLOR_SURFACE);

    const char* tabs[3] = { "Media", "Playlists", "Transitions" };
    const int tabGap = 4;
    const int tabW = (client->right - PROGRAM_LIST_INSET * 2 - tabGap * 2) / 3;
    HFONT font = ProgramUiFont();
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < 3; ++i)
    {
        RECT tab = {
            PROGRAM_LIST_INSET + i * (tabW + tabGap),
            libTop + 8,
            PROGRAM_LIST_INSET + i * (tabW + tabGap) + tabW,
            libTop + 8 + PROGRAM_LIB_TAB_H
        };
        const bool on = g_leftLibTab == i;
        SceneRound(hdc, &tab, 6.0f, on ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
        SetTextColor(hdc, on ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
        DrawTextA(hdc, tabs[i], -1, &tab, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    const int bodyTop = libTop + 8 + PROGRAM_LIB_TAB_H + 8;
    const int rowLeft = PROGRAM_LIST_INSET;
    const int rowRight = client->right - PROGRAM_LIST_INSET;

    if (g_leftLibTab == LEFT_LIB_MEDIA)
    {
        SetRect(&g_leftLibMediaBody, rowLeft, bodyTop, rowRight, client->bottom - 6);
        if (g_leftLibMediaCount == 0)
        {
            RECT empty = { rowLeft + 8, bodyTop, rowRight, bodyTop + 48 };
            SetTextColor(hdc, LOOK_COLOR_INK_DIM);
            DrawTextA(
                hdc,
                "Drop photos and videos here",
                -1,
                &empty,
                DT_LEFT | DT_WORDBREAK | DT_NOPREFIX
            );
        }
        HRGN clip = CreateRectRgn(
            g_leftLibMediaBody.left,
            g_leftLibMediaBody.top,
            g_leftLibMediaBody.right,
            g_leftLibMediaBody.bottom
        );
        SelectClipRgn(hdc, clip);
        for (int i = 0; i < g_leftLibMediaCount; ++i)
        {
            RECT thumb = {};
            RECT card = {};
            LeftLibMediaCardRect(&g_leftLibMediaBody, i, &thumb, &card);
            if (card.bottom < g_leftLibMediaBody.top || card.top > g_leftLibMediaBody.bottom)
            {
                continue;
            }
            const int thumbW = thumb.right - thumb.left;
            const int thumbH = thumb.bottom - thumb.top;
            EnsureLeftLibMediaThumb(&g_leftLibMedia[i], thumbW, thumbH);
            SceneRound(hdc, &card, 8.0f, i == g_leftLibMediaSel ? LOOK_COLOR_RAISED : LOOK_COLOR_SUNKEN);
            HBITMAP frame = g_leftLibMedia[i].thumb;
            if (i == g_leftLibMediaHover && g_leftLibMedia[i].scrub != nullptr)
            {
                frame = g_leftLibMedia[i].scrub;
            }
            if (frame != nullptr)
            {
                HDC mem = CreateCompatibleDC(hdc);
                HBITMAP old = (HBITMAP)SelectObject(mem, frame);
                BitBlt(hdc, thumb.left, thumb.top, thumbW, thumbH, mem, 0, 0, SRCCOPY);
                SelectObject(mem, old);
                DeleteDC(mem);
            }
            if (i == g_leftLibMediaHover)
            {
                const int barX = thumb.left + (int)((thumb.right - thumb.left - 2) * g_leftLibMediaHoverRatio);
                RECT bar = { barX, thumb.top, barX + 2, thumb.bottom };
                HBRUSH mark = CreateSolidBrush(RGB(255, 255, 255));
                FillRect(hdc, &bar, mark);
                DeleteObject(mark);
            }
            RECT chin = { card.left + 6, thumb.bottom, card.right - 6, card.bottom };
            SetTextColor(hdc, i == g_leftLibMediaSel ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
            DrawTextA(hdc, g_leftLibMedia[i].name, -1, &chin, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        SelectClipRgn(hdc, nullptr);
        DeleteObject(clip);
    }
    else if (g_leftLibTab == LEFT_LIB_PLAYLISTS)
    {
        RECT add = { rowLeft, bodyTop, rowRight, bodyTop + 26 };
        SceneRound(hdc, &add, 6.0f, LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
        SetTextColor(hdc, LOOK_COLOR_INK_BRIGHT);
        DrawTextA(hdc, "+ New playlist", -1, &add, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT addProj = { rowLeft, bodyTop + 32, rowRight, bodyTop + 58 };
        SceneRound(hdc, &addProj, 6.0f, LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
        DrawTextA(hdc, "+ Add open project", -1, &addProj, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        int y = bodyTop + 66;
        for (int i = 0; i < g_leftLibPlaylistCount; ++i)
        {
            RECT row = { rowLeft, y, rowRight, y + PROGRAM_PROJECT_ROW };
            if (row.top >= client->bottom)
            {
                break;
            }
            if (i == g_leftLibPlaylistSel)
            {
                SceneRound(hdc, &row, 6.0f, LOOK_COLOR_RAISED);
            }
            SetTextColor(hdc, i == g_leftLibPlaylistSel ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
            RECT text = row;
            text.left += 8;
            DrawTextA(hdc, g_leftLibPlaylists[i].name, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            y += PROGRAM_PROJECT_ROW;
        }
        y += 6;
        for (int i = 0; i < g_leftLibItemCount; ++i)
        {
            RECT row = { rowLeft + 10, y, rowRight, y + PROGRAM_PROJECT_ROW };
            if (row.top >= client->bottom)
            {
                break;
            }
            SetTextColor(hdc, LOOK_COLOR_INK_DIM);
            DrawTextA(hdc, g_leftLibItems[i].name, -1, &row, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            y += PROGRAM_PROJECT_ROW;
        }
    }
    else
    {
        EnsureTransGrid();
        int selKind = g_transCurrent;
        int selDir = g_transDir;
        int selMs = g_transMs;
        if (g_transDefaultPanel == nullptr && g_transGenie == nullptr)
        {
            SyncTransUiFromSelection();
            selKind = g_transCurrent;
            selDir = g_transDir;
            selMs = g_transMs;
        }
        else
        {
            const int slide = FirstTransitionTargetSlide();
            if (slide >= 0)
            {
                selKind = Renderer_EffectiveSlideTransition(slide);
                selDir = Renderer_EffectiveSlideTransitionDir(slide);
                selMs = Renderer_EffectiveSlideTransitionMs(slide);
            }
        }

        const int footTop = client->bottom - 40;
        SetRect(&g_transLibBody, rowLeft, bodyTop, rowRight, footTop - 8);
        const bool editingDefault = g_transDefaultPanel != nullptr;
        // Footer layout / Versions visibility follows the selection, not the fan-out editor.
        const int savedKind = g_transCurrent;
        g_transCurrent = selKind;
        TransLibFooter foot = {};
        LayoutTransLibFooter(rowLeft, rowRight, footTop, client->bottom - 6, &foot);
        g_transCurrent = savedKind;
        g_transFootSlide = foot.slide;
        g_transFootBg = foot.bg;
        const bool slideOn = editingDefault && g_transDefaultEdit == TRANS_DEFAULT_SLIDE;
        const bool bgOn = editingDefault && g_transDefaultEdit == TRANS_DEFAULT_BG;
        SceneRound(hdc, &foot.slide, 6.0f, slideOn ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED,
                   slideOn ? LOOK_COLOR_ACCENT : PROGRAM_COLOR_CONTROL_EDGE);
        SceneRound(hdc, &foot.bg, 6.0f, bgOn ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED,
                   bgOn ? LOOK_COLOR_ACCENT : PROGRAM_COLOR_CONTROL_EDGE);
        SetTextColor(hdc, slideOn ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
        DrawTextA(hdc, "Slide", -1, &foot.slide, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SetTextColor(hdc, bgOn ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
        DrawTextA(hdc, "BG", -1, &foot.bg, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        // While the fan-out is open, hide the selection grid/timing so it doesn't read as a second column.
        if (!editingDefault)
        {
            if (foot.versionsOn)
            {
                SceneRound(hdc, &foot.versions, 6.0f, LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
                SetTextColor(hdc, LOOK_COLOR_INK_BRIGHT);
                DrawTextA(hdc, TransDirName(selDir), -1, &foot.versions, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            SceneRound(hdc, &foot.minus, 6.0f, LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
            SceneRound(hdc, &foot.plus, 6.0f, LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
            SetTextColor(hdc, LOOK_COLOR_INK_BRIGHT);
            DrawTextA(hdc, "-", -1, &foot.minus, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            DrawTextA(hdc, "+", -1, &foot.plus, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            char msText[24] = {};
            const int tenths = (selMs + 50) / 100;
            wsprintfA(msText, "%d.%ds", tenths / 10, tenths % 10);
            SetTextColor(hdc, LOOK_COLOR_INK);
            DrawTextA(hdc, msText, -1, &foot.ms, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            const int innerW = g_transLibBody.right - g_transLibBody.left;
            const int cols = 2;
            const int gap = 6;
            const int cardW = (innerW - gap) / cols;
            const int thumbH = MulDiv(cardW, 9, 16);
            const int cardH = thumbH + 22;
            HRGN clip = CreateRectRgn(
                g_transLibBody.left,
                g_transLibBody.top,
                g_transLibBody.right,
                g_transLibBody.bottom
            );
            SelectClipRgn(hdc, clip);
            for (int i = 0; i < TRANS_GRID_MAX; ++i)
            {
                const int col = i % cols;
                const int row = i / cols;
                const int x = g_transLibBody.left + col * (cardW + gap);
                const int y = g_transLibBody.top - g_transLibScroll + row * (cardH + gap);
                RECT card = { x, y, x + cardW, y + cardH };
                RECT thumb = { x, y, x + cardW, y + thumbH };
                if (card.bottom < g_transLibBody.top || card.top > g_transLibBody.bottom)
                {
                    continue;
                }
                EnsureTransGridThumb(i, cardW, thumbH, selDir);
                const bool on = g_transGrid[i].kind == selKind;
                SceneRound(
                    hdc,
                    &card,
                    8.0f,
                    on ? LOOK_COLOR_RAISED : LOOK_COLOR_SUNKEN,
                    on ? LOOK_COLOR_ACCENT : PROGRAM_COLOR_CONTROL_EDGE
                );
                HBITMAP frame = g_transGrid[i].thumb;
                if (i == g_transLibHover && g_transGrid[i].scrub != nullptr)
                {
                    frame = g_transGrid[i].scrub;
                }
                if (frame != nullptr)
                {
                    HDC mem = CreateCompatibleDC(hdc);
                    HBITMAP old = (HBITMAP)SelectObject(mem, frame);
                    BitBlt(hdc, thumb.left, thumb.top, cardW, thumbH, mem, 0, 0, SRCCOPY);
                    SelectObject(mem, old);
                    DeleteDC(mem);
                }
                if (i == g_transLibHover)
                {
                    const int barX = thumb.left + (int)((thumb.right - thumb.left - 2) * g_transLibHoverRatio);
                    RECT bar = { barX, thumb.top, barX + 2, thumb.bottom };
                    HBRUSH mark = CreateSolidBrush(RGB(255, 255, 255));
                    FillRect(hdc, &bar, mark);
                    DeleteObject(mark);
                }
                RECT chin = { card.left + 4, thumb.bottom, card.right - 4, card.bottom };
                SetTextColor(hdc, on ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
                DrawTextA(hdc, g_transGrid[i].label, -1, &chin, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            SelectClipRgn(hdc, nullptr);
            DeleteObject(clip);
        }
        else
        {
            SetTextColor(hdc, LOOK_COLOR_INK_DIM);
            DrawTextA(
                hdc,
                g_transDefaultEdit == TRANS_DEFAULT_BG ? "Editing BG renderer default" : "Editing slide renderer default",
                -1,
                &g_transLibBody,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX
            );
        }
    }

    SelectObject(hdc, oldFont);
}

void PaintProgramProjects(HDC hdc, const RECT* client)
{
    if (hdc == nullptr || client == nullptr)
    {
        return;
    }

    const int dividerY = ProgramPreviewDividerY();
    if (dividerY <= 0 || dividerY >= client->bottom)
    {
        return;
    }

    const int projectsBottom = ProgramProjectsBottom(client);
    if (projectsBottom <= dividerY + PROGRAM_GAP)
    {
        return;
    }

    RECT panel = { client->left, dividerY + PROGRAM_GAP, client->right, projectsBottom };
    PaintProgramPanelHead(hdc, &panel, "PROJECTS", g_programProjectCount);

    char currentPath[MAX_PATH] = {};
    Document_Path(currentPath, MAX_PATH);
    const int listTop = ProgramProjectListTop();
    const int rowLeft = PROGRAM_LIST_INSET;
    const int rowRight = client->right - PROGRAM_LIST_INSET;
    HFONT font = ProgramUiFont();
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    if (g_programProjectCount == 0)
    {
        RECT empty = {
            rowLeft + 8,
            listTop,
            rowRight,
            listTop + PROGRAM_PROJECT_ROW
        };
        SetTextColor(hdc, LOOK_COLOR_INK_DIM);
        DrawTextA(hdc, "None saved yet", -1, &empty, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        return;
    }

    for (int i = 0; i < g_programProjectCount; ++i)
    {
        RECT row = {
            rowLeft,
            listTop + (i * PROGRAM_PROJECT_ROW),
            rowRight,
            listTop + ((i + 1) * PROGRAM_PROJECT_ROW)
        };
        if (row.top >= projectsBottom)
        {
            break;
        }

        const bool held = g_programProjectDragging && i == g_programProjectHeld;
        if (!held)
        {
            const bool selected =
                currentPath[0] != '\0' &&
                lstrcmpiA(currentPath, g_programProjects[i].path) == 0;
            if (selected)
            {
                SceneRound(hdc, &row, 6.0f, LOOK_COLOR_RAISED);
            }

            SetTextColor(hdc, selected ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
            RECT text = row;
            text.left += 8;
            text.right -= 4;
            DrawTextA(
                hdc,
                g_programProjects[i].name,
                -1,
                &text,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS
            );
        }
    }

    if (g_programProjectDragging)
    {
        const int lineY = listTop + (g_programProjectDrop * PROGRAM_PROJECT_ROW);
        if (lineY < client->bottom)
        {
            DrawProgramProjectInsertMark(hdc, rowLeft, rowRight, lineY);
        }
    }

    SelectObject(hdc, oldFont);
}

int HitTestProgramProject(HWND hwnd, int x, int y)
{
    if (hwnd == nullptr || g_programProjectCount <= 0)
    {
        return -1;
    }

    RECT client = {};
    GetClientRect(hwnd, &client);
    if (x >= client.right - PROGRAM_SPLIT)
    {
        return -1;
    }

    const int listTop = ProgramProjectListTop();
    const int projectsBottom = ProgramProjectsBottom(&client);
    if (y < listTop || y >= projectsBottom)
    {
        return -1;
    }

    const int index = (y - listTop) / PROGRAM_PROJECT_ROW;
    if (index < 0 || index >= g_programProjectCount)
    {
        return -1;
    }
    return index;
}

static bool HitLeftLibSplit(HWND hwnd, int y)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int libTop = ProgramLeftLibTop(&client);
    return y >= libTop - 4 && y <= libTop + 4;
}

static int HitLeftLibTab(HWND hwnd, int x, int y)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int libTop = ProgramLeftLibTop(&client);
    const int tabGap = 4;
    const int tabW = (client.right - PROGRAM_LIST_INSET * 2 - tabGap * 2) / 3;
    for (int i = 0; i < 3; ++i)
    {
        RECT tab = {
            PROGRAM_LIST_INSET + i * (tabW + tabGap),
            libTop + 8,
            PROGRAM_LIST_INSET + i * (tabW + tabGap) + tabW,
            libTop + 8 + PROGRAM_LIB_TAB_H
        };
        POINT pt = { x, y };
        if (PtInRect(&tab, pt))
        {
            return i;
        }
    }
    return -1;
}

static int HitLeftLibMediaCard(int x, int y)
{
    POINT pt = { x, y };
    if (!PtInRect(&g_leftLibMediaBody, pt))
    {
        return -1;
    }
    for (int i = 0; i < g_leftLibMediaCount; ++i)
    {
        RECT card = {};
        LeftLibMediaCardRect(&g_leftLibMediaBody, i, nullptr, &card);
        if (PtInRect(&card, pt))
        {
            return i;
        }
    }
    return -1;
}

static HBITMAP CloneBitmapToSize(HBITMAP src, int srcW, int srcH, int destW, int destH)
{
    if (src == nullptr || srcW < 1 || srcH < 1 || destW < 1 || destH < 1)
    {
        return nullptr;
    }
    void* bits = nullptr;
    HBITMAP dib = CreateXfadeDib(destW, destH, &bits);
    if (dib == nullptr || bits == nullptr)
    {
        if (dib != nullptr)
        {
            DeleteObject(dib);
        }
        return nullptr;
    }
    if (!GrabBitmap32(src, srcW, srcH, destW, destH, static_cast<unsigned char*>(bits)))
    {
        DeleteObject(dib);
        return nullptr;
    }
    return dib;
}

static void ShowLibraryMediaLive(int index)
{
    if (index < 0 || index >= g_leftLibMediaCount)
    {
        return;
    }
    LeftLibMediaItem* item = &g_leftLibMedia[index];
    wchar_t wide[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, item->path, -1, wide, MAX_PATH);
    if (wide[0] == L'\0')
    {
        return;
    }

    const int frameW = 960;
    const int frameH = 540;
    wchar_t videoPath[MAX_PATH] = {};
    HBITMAP neu = nullptr;
    int neuW = frameW;
    int neuH = frameH;
    const int transKind = Renderer_DefaultBgTransition();
    const int transMs = Renderer_DefaultBgTransitionMs();
    const bool wantFade = transKind != SLIDE_TRANSITION_NONE && transMs > 0;
    if (item->video)
    {
        lstrcpynW(videoPath, wide, MAX_PATH);
        if (wantFade)
        {
            // Destination look for the transition — library thumb, not MF.
            if (item->thumb != nullptr && item->thumbW > 0 && item->thumbH > 0)
            {
                neu = CloneBitmapToSize(item->thumb, item->thumbW, item->thumbH, frameW, frameH);
            }
            if (neu == nullptr)
            {
                neu = LeftLibLoadShellThumb(wide, frameW, frameH);
            }
            if (neu == nullptr)
            {
                neu = MakeBlankBgHold(frameW, frameH);
            }
        }
        else if (g_liveBgFrame != nullptr && g_liveBgW > 0 && g_liveBgH > 0)
        {
            neu = DuplicateBgBitmap(g_liveBgFrame, g_liveBgW, g_liveBgH);
            neuW = g_liveBgW;
            neuH = g_liveBgH;
        }
        else
        {
            neu = MakeBlankBgHold(frameW, frameH);
        }
    }
    else
    {
        // Still images: reuse library thumb — shell reloads hitch every click.
        if (item->thumb != nullptr && item->thumbW > 0 && item->thumbH > 0)
        {
            neu = CloneBitmapToSize(item->thumb, item->thumbW, item->thumbH, frameW, frameH);
        }
        if (neu == nullptr)
        {
            neu = LeftLibLoadImageThumb(wide, frameW, frameH);
            if (neu == nullptr)
            {
                neu = LeftLibLoadShellThumb(wide, frameW, frameH);
            }
        }
    }
    if (neu == nullptr)
    {
        return;
    }

    // Media-tab items are live backgrounds — same global BG transition as BG slides.
    Renderer_ArmBackground(-1);
    g_videoBackgroundCleared = false;
    BeginBackgroundTransition(
        neu,
        neuW,
        neuH,
        -1,
        videoPath,
        transKind,
        Renderer_DefaultBgTransitionDir(),
        transMs
    );
    if (item->video && g_mediaPanel != MEDIA_PANEL_VIDEO)
    {
        g_mediaPanel = MEDIA_PANEL_VIDEO;
        LayoutProgramPreview();
    }
    UpdateMediaTransportUi();
    if (g_programMiddle != nullptr)
    {
        InvalidateRect(g_programMiddle, nullptr, FALSE);
    }
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
}

static void DropLibraryMediaOnDeck(int index)
{
    if (index < 0 || index >= g_leftLibMediaCount)
    {
        return;
    }
    wchar_t stored[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, g_leftLibMedia[index].stored, -1, stored, MAX_PATH);
    if (stored[0] == L'\0')
    {
        return;
    }
    const int previous = Renderer_CurrentSlide();
    History_Remember();
    // Same default as Explorer drops in Show view: land as a BG slide, do not select/arm it.
    if (!Renderer_AddFullscreenMediaSlide(stored, g_leftLibMedia[index].video, true))
    {
        return;
    }
    if (previous >= 0)
    {
        Renderer_SelectSlide(previous);
        HighlightSlide(previous);
    }
    Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    RefreshSlideLists();
    PresentCurrent();
}

static bool LibraryMediaOverDeck(HWND from, int x, int y)
{
    if (from == nullptr || g_programMiddle == nullptr)
    {
        return false;
    }
    POINT pt = { x, y };
    ClientToScreen(from, &pt);
    RECT deck = {};
    GetWindowRect(g_programMiddle, &deck);
    return PtInRect(&deck, pt) != FALSE;
}

static void ClickLeftLib(HWND hwnd, int x, int y)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int libTop = ProgramLeftLibTop(&client);
    const int tab = HitLeftLibTab(hwnd, x, y);
    if (tab >= 0)
    {
        if (tab != LEFT_LIB_TRANSITIONS)
        {
            CloseTransDefaultPanel();
            ClearTransLibHover(nullptr);
        }
        if (tab != LEFT_LIB_MEDIA)
        {
            ClearLeftLibMediaHover(nullptr);
        }
        g_leftLibTab = tab;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    const int bodyTop = libTop + 8 + PROGRAM_LIB_TAB_H + 8;
    const int rowLeft = PROGRAM_LIST_INSET;
    const int rowRight = client.right - PROGRAM_LIST_INSET;
    POINT pt = { x, y };

    if (g_leftLibTab == LEFT_LIB_MEDIA)
    {
        for (int i = 0; i < g_leftLibMediaCount; ++i)
        {
            RECT thumb = {};
            RECT card = {};
            LeftLibMediaCardRect(&g_leftLibMediaBody, i, &thumb, &card);
            if (PtInRect(&card, pt))
            {
                g_leftLibMediaSel = i;
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }
        return;
    }

    if (g_leftLibTab == LEFT_LIB_PLAYLISTS)
    {
        RECT add = { rowLeft, bodyTop, rowRight, bodyTop + 26 };
        RECT addProj = { rowLeft, bodyTop + 32, rowRight, bodyTop + 58 };
        if (PtInRect(&add, pt))
        {
            char name[64] = {};
            wsprintfA(name, "Show %d", g_leftLibPlaylistCount + 1);
            AddLeftLibPlaylist(name);
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        if (PtInRect(&addProj, pt))
        {
            if (!AddCurrentProjectToLeftLibPlaylist())
            {
                MessageBoxA(
                    g_programWindow != nullptr ? g_programWindow : hwnd,
                    "Open a project first, then add it to the selected playlist.",
                    "Playlists",
                    MB_OK | MB_ICONINFORMATION
                );
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        int yRow = bodyTop + 66;
        for (int i = 0; i < g_leftLibPlaylistCount; ++i)
        {
            RECT row = { rowLeft, yRow, rowRight, yRow + PROGRAM_PROJECT_ROW };
            if (PtInRect(&row, pt))
            {
                g_leftLibPlaylistSel = i;
                RefreshLeftLibPlaylists();
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            yRow += PROGRAM_PROJECT_ROW;
        }
        yRow += 6;
        for (int i = 0; i < g_leftLibItemCount; ++i)
        {
            RECT row = { rowLeft + 10, yRow, rowRight, yRow + PROGRAM_PROJECT_ROW };
            if (PtInRect(&row, pt) && g_leftLibItems[i].path[0] != '\0')
            {
                Document_Open(g_leftLibItems[i].path);
                RefreshAfterDocumentChange();
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
            yRow += PROGRAM_PROJECT_ROW;
        }
        return;
    }

    EnsureTransGrid();
    const int footTop = client.bottom - 40;
    TransLibFooter foot = {};
    LayoutTransLibFooter(rowLeft, rowRight, footTop, client.bottom - 6, &foot);
    g_transFootSlide = foot.slide;
    g_transFootBg = foot.bg;
    if (PtInRect(&foot.slide, pt))
    {
        ToggleTransDefaultPanel(TRANS_DEFAULT_SLIDE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
    if (PtInRect(&foot.bg, pt))
    {
        ToggleTransDefaultPanel(TRANS_DEFAULT_BG);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
    if (g_transDefaultPanel != nullptr)
    {
        return;
    }
    int selKind = g_transCurrent;
    int selDir = g_transDir;
    int selMs = g_transMs;
    if (g_transDefaultPanel == nullptr)
    {
        SyncTransUiFromSelection();
        selKind = g_transCurrent;
        selDir = g_transDir;
        selMs = g_transMs;
    }
    else
    {
        const int slide = FirstTransitionTargetSlide();
        if (slide >= 0)
        {
            selKind = Renderer_EffectiveSlideTransition(slide);
            selDir = Renderer_EffectiveSlideTransitionDir(slide);
            selMs = Renderer_EffectiveSlideTransitionMs(slide);
        }
    }
    if (foot.versionsOn && PtInRect(&foot.versions, pt))
    {
        POINT origin = { foot.versions.left, foot.versions.bottom };
        ClientToScreen(hwnd, &origin);
        const int dir = PickTransDir(hwnd, origin.x, origin.y, selDir);
        if (dir >= 0)
        {
            ApplyTransitionToSelection(selKind, dir, selMs, false, true, false);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
    if (PtInRect(&foot.minus, pt) || PtInRect(&foot.plus, pt))
    {
        int next = selMs + (PtInRect(&foot.plus, pt) ? TRANS_MS_STEP : -TRANS_MS_STEP);
        if (next < TRANS_MS_MIN)
        {
            next = TRANS_MS_MIN;
        }
        if (next > TRANS_MS_MAX)
        {
            next = TRANS_MS_MAX;
        }
        next = ((next + 50) / 100) * 100;
        ApplyTransitionToSelection(selKind, selDir, next, false, false, true);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    const int innerW = rowRight - rowLeft;
    const int cols = 2;
    const int gap = 6;
    const int cardW = (innerW - gap) / cols;
    const int thumbH = MulDiv(cardW, 9, 16);
    const int cardH = thumbH + 22;
    const int gridTop = bodyTop;
    for (int i = 0; i < TRANS_GRID_MAX; ++i)
    {
        const int col = i % cols;
        const int row = i / cols;
        const int x = rowLeft + col * (cardW + gap);
        const int yCard = gridTop - g_transLibScroll + row * (cardH + gap);
        RECT card = { x, yCard, x + cardW, yCard + cardH };
        if (!PtInRect(&card, pt))
        {
            continue;
        }
        if (pt.y < bodyTop || pt.y >= footTop - 8)
        {
            return;
        }
        const int kind = g_transGrid[i].kind;
        const int dir = Renderer_TransitionNeedsDir(kind) ? selDir : SLIDE_TRANSITION_DIR_LEFT;
        ApplyTransitionToSelection(kind, dir, selMs, true, true, true);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
}

void ApplyProgramProjectDrop()
{
    if (g_programProjectHeld < 0 || g_programProjectDrop < 0)
    {
        return;
    }

    int insert = g_programProjectDrop;
    if (insert > g_programProjectHeld)
    {
        insert -= 1;
    }
    if (insert == g_programProjectHeld)
    {
        return;
    }

    StoreProject remaining[STORE_PROJECT_MAX] = {};
    int remainingCount = 0;
    const StoreProject held = g_programProjects[g_programProjectHeld];
    for (int i = 0; i < g_programProjectCount; ++i)
    {
        if (i != g_programProjectHeld)
        {
            remaining[remainingCount] = g_programProjects[i];
            remainingCount += 1;
        }
    }

    int read = 0;
    for (int i = 0; i < g_programProjectCount; ++i)
    {
        if (i == insert)
        {
            g_programProjects[i] = held;
        }
        else if (read < remainingCount)
        {
            g_programProjects[i] = remaining[read];
            read += 1;
        }
    }
    Store_SetProjectOrder(g_programProjects, g_programProjectCount);
}

int ProgramProjectDropFromY(int y)
{
    if (g_programProjectCount <= 1)
    {
        return 0;
    }

    int slot = (y - ProgramProjectListTop() + (PROGRAM_PROJECT_ROW / 2)) / PROGRAM_PROJECT_ROW;
    if (slot < 0)
    {
        slot = 0;
    }
    if (slot > g_programProjectCount)
    {
        slot = g_programProjectCount;
    }
    return slot;
}

void OpenProgramProject(HWND owner, int index);

void FinishProgramProjectPointer(bool openIfClick)
{
    if (g_programProjectHeld < 0)
    {
        return;
    }

    if (g_programProjectDragging)
    {
        ApplyProgramProjectDrop();
    }
    else if (openIfClick)
    {
        OpenProgramProject(g_programWindow, g_programProjectHeld);
    }

    g_programProjectHeld = -1;
    g_programProjectDrop = -1;
    g_programProjectDragging = false;
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
}

void OpenProgramProject(HWND owner, int index)
{
    if (index < 0 || index >= g_programProjectCount)
    {
        return;
    }
    if (!Document_Open(g_programProjects[index].path))
    {
        MessageBoxA(
            owner,
            "That project file is missing or could not be opened.",
            "Projects",
            MB_OK | MB_ICONWARNING
        );
        return;
    }
    History_Clear();
    if (g_liveBgHeld)
    {
        Renderer_ArmBackground(-1);
    }
    RefreshAfterDocumentChange();
}

void LayoutProgramSections(HWND hwnd);

void DrawProgramShowIcon(HDC hdc, int cx, int cy, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, cx - 18, cy - 14, cx + 18, cy + 10, 6, 6);
    MoveToEx(hdc, cx - 8, cy + 12, nullptr);
    LineTo(hdc, cx + 8, cy + 12);
    MoveToEx(hdc, cx, cy + 12, nullptr);
    LineTo(hdc, cx, cy + 16);
    MoveToEx(hdc, cx - 10, cy + 16, nullptr);
    LineTo(hdc, cx + 10, cy + 16);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawProgramEditIcon(HDC hdc, int cx, int cy, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, cx - 16, cy - 16, cx + 10, cy + 16, 4, 4);
    POINT pencil[5] = {
        { cx + 6, cy + 12 },
        { cx + 18, cy - 8 },
        { cx + 12, cy - 14 },
        { cx, cy + 6 },
        { cx + 6, cy + 12 }
    };
    Polyline(hdc, pencil, 5);
    MoveToEx(hdc, cx + 2, cy + 8, nullptr);
    LineTo(hdc, cx + 8, cy + 14);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void DrawMediaVideoIcon(HDC hdc, int cx, int cy, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, cx - 11, cy - 8, cx + 5, cy + 8, 3, 3);
    POINT play[3] = {
        { cx + 2, cy - 5 },
        { cx + 12, cy },
        { cx + 2, cy + 5 }
    };
    SelectObject(hdc, brush);
    Polygon(hdc, play, 3);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void DrawMediaPlayIcon(HDC hdc, int cx, int cy, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    // Triangle nudged right by 1px so it looks optically centered.
    POINT play[3] = {
        { cx - 4, cy - 7 },
        { cx + 6, cy },
        { cx - 4, cy + 7 }
    };
    Polygon(hdc, play, 3);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void DrawMediaPauseIcon(HDC hdc, int cx, int cy, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    RECT left = { cx - 6, cy - 7, cx - 1, cy + 7 };
    RECT right = { cx + 1, cy - 7, cx + 6, cy + 7 };
    FillRect(hdc, &left, brush);
    FillRect(hdc, &right, brush);
    DeleteObject(brush);
}

static void DrawMediaAudioIcon(HDC hdc, int cx, int cy, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    POINT speaker[5] = {
        { cx - 8, cy - 4 },
        { cx - 3, cy - 4 },
        { cx + 3, cy - 9 },
        { cx + 3, cy + 9 },
        { cx - 3, cy + 4 }
    };
    Polygon(hdc, speaker, 5);
    RECT body = { cx - 10, cy - 4, cx - 3, cy + 4 };
    Rectangle(hdc, body.left, body.top, body.right, body.bottom);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Arc(hdc, cx + 4, cy - 6, cx + 14, cy + 6, cx + 8, cy - 5, cx + 8, cy + 5);
    Arc(hdc, cx + 6, cy - 10, cx + 18, cy + 10, cx + 10, cy - 8, cx + 10, cy + 8);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

#define QUICK_SLIDE_MAX 32
#define QUICK_SLIDE_TEXT 1024
#define QUICK_SLIDES_W 420
#define QUICK_SLIDES_H 280

static bool QuickLineBlank(const char* line, int len)
{
    for (int i = 0; i < len; ++i)
    {
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
        {
            return false;
        }
    }
    return true;
}

static int SplitQuickSlides(const char* text, char blocks[][QUICK_SLIDE_TEXT], int maxBlocks)
{
    if (text == nullptr || blocks == nullptr || maxBlocks <= 0)
    {
        return 0;
    }

    int count = 0;
    char current[QUICK_SLIDE_TEXT] = {};
    int currentLen = 0;
    bool have = false;
    const char* cursor = text;
    while (*cursor != '\0' && count < maxBlocks)
    {
        const char* start = cursor;
        while (*cursor != '\0' && *cursor != '\n')
        {
            cursor += 1;
        }
        int len = static_cast<int>(cursor - start);
        if (len > 0 && start[len - 1] == '\r')
        {
            len -= 1;
        }
        if (QuickLineBlank(start, len))
        {
            if (have)
            {
                lstrcpynA(blocks[count], current, QUICK_SLIDE_TEXT);
                count += 1;
                current[0] = '\0';
                currentLen = 0;
                have = false;
            }
        }
        else
        {
            if (have && currentLen + 1 < QUICK_SLIDE_TEXT)
            {
                current[currentLen] = '\n';
                currentLen += 1;
                current[currentLen] = '\0';
            }
            const int room = QUICK_SLIDE_TEXT - 1 - currentLen;
            if (room > 0)
            {
                const int copy = len < room ? len : room;
                memcpy(current + currentLen, start, static_cast<size_t>(copy));
                currentLen += copy;
                current[currentLen] = '\0';
            }
            have = true;
        }
        if (*cursor == '\n')
        {
            cursor += 1;
        }
    }
    if (have && count < maxBlocks)
    {
        lstrcpynA(blocks[count], current, QUICK_SLIDE_TEXT);
        count += 1;
    }
    return count;
}

static bool ApplyQuickSlideText(int slideIndex, const char* text)
{
    if (!Renderer_SetSlideSlotText(slideIndex, "body", text))
    {
        return false;
    }
    return true;
}

static void AddQuickSlidesFromText(HWND owner)
{
    if (!Document_HasProject())
    {
        MessageBoxA(owner, "Open or create a project first.", "Quick Slides", MB_OK | MB_ICONINFORMATION);
        return;
    }

    char raw[8192] = {};
    if (g_quickSlidesEdit != nullptr)
    {
        GetWindowTextA(g_quickSlidesEdit, raw, sizeof(raw));
    }
    char blocks[QUICK_SLIDE_MAX][QUICK_SLIDE_TEXT] = {};
    const int count = SplitQuickSlides(raw, blocks, QUICK_SLIDE_MAX);
    if (count <= 0)
    {
        MessageBoxA(owner, "Type some text first.\n\nA blank line starts a new slide.", "Quick Slides", MB_OK | MB_ICONINFORMATION);
        return;
    }

    History_Remember();
    int added = 0;
    int failed = 0;
    for (int i = 0; i < count; ++i)
    {
        if (!Document_AddSlide())
        {
            failed += count - i;
            break;
        }
        const int index = Renderer_SlideCount() - 1;
        if (ApplyQuickSlideText(index, blocks[i]))
        {
            added += 1;
        }
        else
        {
            failed += 1;
        }
    }

    if (added > 0)
    {
        Document_Save();
        RefreshAfterDocumentChange();
        EditView_OpenSlide(Renderer_SlideCount() - 1);
        SetWindowTextA(g_quickSlidesEdit, "");
    }
    if (failed > 0)
    {
        MessageBoxA(
            owner,
            added > 0 ? "Some slides could not be added." : "Could not add those slides.",
            "Quick Slides",
            MB_OK | MB_ICONWARNING
        );
    }
}

void PlaceQuickSlidesWindow()
{
    if (g_quickSlidesWindow == nullptr)
    {
        return;
    }

    HWND anchor = g_programWindow != nullptr ? g_programWindow : g_quickSlidesWindow;
    HMONITOR monitor = MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoA(monitor, &info))
    {
        return;
    }

    const int width = QUICK_SLIDES_W;
    const int height = QUICK_SLIDES_H;
    const int x = info.rcWork.right - width - 16;
    const int y = info.rcWork.bottom - height - 16;
    SetWindowPos(g_quickSlidesWindow, HWND_TOP, x, y, width, height, SWP_NOACTIVATE);
}

void HideQuickSlides()
{
    if (g_quickSlidesWindow != nullptr)
    {
        ShowWindow(g_quickSlidesWindow, SW_HIDE);
    }
    if (g_programQuickSlidesBtn != nullptr)
    {
        InvalidateRect(g_programQuickSlidesBtn, nullptr, FALSE);
    }
}

static void LayoutQuickSlidesClient(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int pad = 12;
    const int buttonH = 28;
    const int hintH = 20;
    const int buttonW = 100;
    if (g_quickSlidesEdit != nullptr)
    {
        MoveWindow(
            g_quickSlidesEdit,
            pad,
            pad + hintH,
            client.right - (pad * 2),
            client.bottom - pad - hintH - buttonH - pad,
            TRUE
        );
    }
    HWND add = GetDlgItem(hwnd, ID_QUICK_SLIDES_ADD);
    if (add != nullptr)
    {
        MoveWindow(
            add,
            client.right - pad - buttonW,
            client.bottom - pad - buttonH,
            buttonW,
            buttonH,
            TRUE
        );
    }
}

static LRESULT CALLBACK QuickSlidesProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HBRUSH fill = CreateSolidBrush(PROGRAM_COLOR_FRAME);
            FillRect(hdc, &client, fill);
            DeleteObject(fill);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, PROGRAM_COLOR_INK);
            HFONT old = (HFONT)SelectObject(hdc, ProgramUiFont());
            RECT hint = { 12, 8, client.right - 12, 28 };
            DrawTextA(hdc, "Blank line starts a new slide.", -1, &hint, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(hdc, old);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_SIZE:
            LayoutQuickSlidesClient(hwnd);
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_QUICK_SLIDES_ADD)
            {
                AddQuickSlidesFromText(hwnd);
                return 0;
            }
            break;

        case WM_CTLCOLOREDIT:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkColor(hdc, PROGRAM_COLOR_WELL_DARK);
            SetTextColor(hdc, PROGRAM_COLOR_SELECT);
            static HBRUSH brush = nullptr;
            if (brush == nullptr)
            {
                brush = CreateSolidBrush(PROGRAM_COLOR_WELL_DARK);
            }
            return reinterpret_cast<LRESULT>(brush);
        }

        case WM_CLOSE:
            HideQuickSlides();
            return 0;

        case WM_DESTROY:
            g_quickSlidesEdit = nullptr;
            g_quickSlidesWindow = nullptr;
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void ShowQuickSlides()
{
    if (g_quickSlidesWindow != nullptr && IsWindowVisible(g_quickSlidesWindow))
    {
        HideQuickSlides();
        return;
    }

    if (g_quickSlidesWindow == nullptr)
    {
        g_quickSlidesWindow = CreateWindowExA(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
            "QuickSlidesWindow",
            "Quick Slides",
            WS_POPUP | WS_CAPTION | WS_SYSMENU,
            0,
            0,
            QUICK_SLIDES_W,
            QUICK_SLIDES_H,
            g_programWindow,
            nullptr,
            g_instance,
            nullptr
        );
        if (g_quickSlidesWindow == nullptr)
        {
            return;
        }

        g_quickSlidesEdit = CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            12,
            32,
            380,
            180,
            g_quickSlidesWindow,
            (HMENU)ID_QUICK_SLIDES_EDIT,
            g_instance,
            nullptr
        );
        CreateWindowExA(
            0,
            "BUTTON",
            "Add slides",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            300,
            220,
            100,
            28,
            g_quickSlidesWindow,
            (HMENU)ID_QUICK_SLIDES_ADD,
            g_instance,
            nullptr
        );
        if (g_programFont != nullptr)
        {
            SendMessageA(g_quickSlidesEdit, WM_SETFONT, (WPARAM)g_programFont, TRUE);
            SendMessageA(GetDlgItem(g_quickSlidesWindow, ID_QUICK_SLIDES_ADD), WM_SETFONT, (WPARAM)g_programFont, TRUE);
        }
        LayoutQuickSlidesClient(g_quickSlidesWindow);
    }

    PlaceQuickSlidesWindow();
    ShowWindow(g_quickSlidesWindow, SW_SHOW);
    SetForegroundWindow(g_quickSlidesWindow);
    if (g_quickSlidesEdit != nullptr)
    {
        SetFocus(g_quickSlidesEdit);
    }
    if (g_programQuickSlidesBtn != nullptr)
    {
        InvalidateRect(g_programQuickSlidesBtn, nullptr, FALSE);
    }
}

#define TEMPLATE_THUMB_W 144
#define TEMPLATE_THUMB_H 81
#define TEMPLATE_CELL_GAP 10
#define TEMPLATE_NAME_H 18
#define TEMPLATE_PANEL_COLS 3
#define TEMPLATE_PANEL_PAD 10
#define TEMPLATE_PANEL_HEAD 28
#define TEMPLATE_PANEL_MAX_ROWS 4
#define TEMPLATE_PANEL_EMPTY_H 48

struct TemplateThumb
{
    char id[64];
    char name[128];
    char path[MAX_PATH];
    FILETIME written;
    HBITMAP bitmap;
};

static int TransDefaultPanelWidth()
{
    const int libW = g_transLibBody.right - g_transLibBody.left;
    const int gridW = TRANS_DEFAULT_PANEL_PAD * 2 +
        TRANS_DEFAULT_PANEL_COLS * TRANS_DEFAULT_PANEL_THUMB_W +
        (TRANS_DEFAULT_PANEL_COLS - 1) * TRANS_DEFAULT_PANEL_GAP;
    if (libW > gridW)
    {
        return libW;
    }
    return gridW;
}

static int TransDefaultPanelGridH()
{
    const int rows = (TRANS_GRID_MAX + TRANS_DEFAULT_PANEL_COLS - 1) / TRANS_DEFAULT_PANEL_COLS;
    return rows * TRANS_DEFAULT_PANEL_CARD_H + (rows - 1) * TRANS_DEFAULT_PANEL_GAP;
}

static int TransDefaultPanelHeight()
{
    return TRANS_DEFAULT_PANEL_HEAD + TRANS_DEFAULT_PANEL_PAD * 2 +
        TransDefaultPanelGridH() + TRANS_DEFAULT_PANEL_FOOT;
}

static RECT TransDefaultPanelCell(int index)
{
    const int col = index % TRANS_DEFAULT_PANEL_COLS;
    const int row = index / TRANS_DEFAULT_PANEL_COLS;
    const int x = TRANS_DEFAULT_PANEL_PAD + col * (TRANS_DEFAULT_PANEL_THUMB_W + TRANS_DEFAULT_PANEL_GAP);
    const int y = TRANS_DEFAULT_PANEL_HEAD + TRANS_DEFAULT_PANEL_PAD +
        row * (TRANS_DEFAULT_PANEL_CARD_H + TRANS_DEFAULT_PANEL_GAP) - g_transDefaultScroll;
    RECT cell = { x, y, x + TRANS_DEFAULT_PANEL_THUMB_W, y + TRANS_DEFAULT_PANEL_CARD_H };
    return cell;
}

static void LayoutTransDefaultPanelFooter(const RECT* client, TransLibFooter* out)
{
    const int top = client->bottom - TRANS_DEFAULT_PANEL_FOOT + 4;
    const int bottom = client->bottom - 4;
    LayoutTransLibFooter(
        client->left + TRANS_DEFAULT_PANEL_PAD,
        client->right - TRANS_DEFAULT_PANEL_PAD,
        top,
        bottom,
        out
    );
    // Slide/BG buttons are unused in the fan-out; reclaim that strip for dir/ms.
    out->slide = {};
    out->bg = {};
    out->versionsOn = Renderer_TransitionNeedsDir(g_transCurrent);
    const int gap = 4;
    const int verW = out->versionsOn ? 68 : 0;
    const int step = 24;
    int x = client->left + TRANS_DEFAULT_PANEL_PAD;
    if (out->versionsOn)
    {
        SetRect(&out->versions, x, top, x + verW, bottom);
        x += verW + gap;
    }
    else
    {
        SetRect(&out->versions, 0, 0, 0, 0);
    }
    SetRect(&out->minus, x, top, x + step, bottom);
    SetRect(&out->plus, client->right - TRANS_DEFAULT_PANEL_PAD - step, top, client->right - TRANS_DEFAULT_PANEL_PAD, bottom);
    SetRect(&out->ms, out->minus.right + 2, top, out->plus.left - 2, bottom);
}

static int TransDefaultPanelHit(int x, int y, const RECT* client, TransLibFooter* footOut)
{
    TransLibFooter foot = {};
    LayoutTransDefaultPanelFooter(client, &foot);
    if (footOut != nullptr)
    {
        *footOut = foot;
    }
    POINT pt = { x, y };
    if (foot.versionsOn && PtInRect(&foot.versions, pt))
    {
        return 100;
    }
    if (PtInRect(&foot.minus, pt))
    {
        return 101;
    }
    if (PtInRect(&foot.plus, pt))
    {
        return 102;
    }
    for (int i = 0; i < TRANS_GRID_MAX; ++i)
    {
        RECT cell = TransDefaultPanelCell(i);
        if (PtInRect(&cell, pt))
        {
            return i;
        }
    }
    return -1;
}

static void CloseTransDefaultPanel()
{
    if (g_transDefaultClosing)
    {
        return;
    }
    g_transDefaultClosing = true;
    HWND hwnd = g_transDefaultPanel;
    g_transDefaultPanel = nullptr;
    g_transDefaultHover = -1;
    g_transDefaultPressed = -1;
    g_transDefaultEdit = TRANS_DEFAULT_NONE;
    if (hwnd != nullptr && IsWindow(hwnd))
    {
        DestroyWindow(hwnd);
    }
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
    g_transDefaultClosing = false;
}

static void ShowTransDefaultPanel(int target)
{
    CloseTransDefaultPanel();
    if (target != TRANS_DEFAULT_SLIDE && target != TRANS_DEFAULT_BG)
    {
        return;
    }
    if (g_programLeft == nullptr)
    {
        return;
    }

    EnsureTransGrid();
    g_transDefaultEdit = target;
    LoadTransitionDefaultsIntoUi();
    g_transDefaultScroll = 0;
    g_transDefaultHover = -1;
    g_transDefaultPressed = -1;

    RECT anchor = (target == TRANS_DEFAULT_BG) ? g_transFootBg : g_transFootSlide;
    // Prefer covering the transitions body, same strip as the left-lib grid.
    RECT body = g_transLibBody;
    if (body.right <= body.left || body.bottom <= body.top)
    {
        body = anchor;
    }
    POINT bodyOrigin = { body.left, body.top };
    ClientToScreen(g_programLeft, &bodyOrigin);
    POINT bodyBottomLeft = { body.left, body.bottom };
    ClientToScreen(g_programLeft, &bodyBottomLeft);
    POINT topLeft = { anchor.left, anchor.top };
    ClientToScreen(g_programLeft, &topLeft);

    const int width = TransDefaultPanelWidth();
    const int height = TransDefaultPanelHeight();
    int x = bodyOrigin.x;
    // Sit just above the Slide/BG footer so the fan-out owns the body.
    int y = bodyBottomLeft.y - height;
    if (y > bodyOrigin.y)
    {
        y = bodyOrigin.y;
    }
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    RECT seed = { bodyOrigin.x, bodyOrigin.y, bodyOrigin.x + width, bodyOrigin.y + height };
    if (GetMonitorInfoA(MonitorFromRect(&seed, MONITOR_DEFAULTTONEAREST), &info))
    {
        if (x + width > info.rcWork.right)
        {
            x = max((int)info.rcWork.left, (int)info.rcWork.right - width);
        }
        if (y < info.rcWork.top)
        {
            y = info.rcWork.top;
        }
        if (y + height > info.rcWork.bottom)
        {
            y = max((int)info.rcWork.top, (int)info.rcWork.bottom - height);
        }
    }

    g_transDefaultPanel = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        "TransDefaultPanel",
        "",
        WS_POPUP,
        x,
        y,
        width,
        height,
        g_programWindow != nullptr ? g_programWindow : g_programLeft,
        nullptr,
        g_instance,
        nullptr
    );
    if (g_transDefaultPanel == nullptr)
    {
        g_transDefaultEdit = TRANS_DEFAULT_NONE;
        return;
    }
    ShowWindow(g_transDefaultPanel, SW_SHOW);
    SetForegroundWindow(g_transDefaultPanel);
    if (g_programLeft != nullptr)
    {
        InvalidateRect(g_programLeft, nullptr, FALSE);
    }
}

static void ToggleTransDefaultPanel(int target)
{
    if (g_transDefaultPanel != nullptr && g_transDefaultEdit == target)
    {
        CloseTransDefaultPanel();
        return;
    }
    if (g_transDefaultDismissTick != 0 && GetTickCount() - g_transDefaultDismissTick < 500)
    {
        g_transDefaultDismissTick = 0;
        return;
    }
    ShowTransDefaultPanel(target);
}

static LRESULT CALLBACK TransDefaultPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const int hit = TransDefaultPanelHit(x, y, &client, nullptr);
            float ratio = g_transDefaultHoverRatio;
            if (hit >= 0 && hit < TRANS_GRID_MAX)
            {
                const RECT cell = TransDefaultPanelCell(hit);
                RECT thumb = {
                    cell.left,
                    cell.top,
                    cell.right,
                    cell.top + TRANS_DEFAULT_PANEL_THUMB_H
                };
                POINT pt = { x, y };
                if (PtInRect(&thumb, pt))
                {
                    const int span = thumb.right - thumb.left;
                    ratio = span > 1 ? (float)(x - thumb.left) / (float)span : 0.0f;
                    if (ratio < 0.0f)
                    {
                        ratio = 0.0f;
                    }
                    if (ratio > 1.0f)
                    {
                        ratio = 1.0f;
                    }
                    UpdateTransHoverScrub(
                        hit,
                        ratio,
                        TRANS_DEFAULT_PANEL_THUMB_W,
                        TRANS_DEFAULT_PANEL_THUMB_H,
                        g_transDir
                    );
                }
            }
            if (hit != g_transDefaultHover || fabsf(ratio - g_transDefaultHoverRatio) > 0.005f)
            {
                g_transDefaultHover = hit;
                g_transDefaultHoverRatio = (hit >= 0 && hit < TRANS_GRID_MAX) ? ratio : 0.0f;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT track = {};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            TrackMouseEvent(&track);
            return 0;
        }

        case WM_MOUSELEAVE:
            g_transDefaultHover = -1;
            g_transDefaultHoverRatio = 0.0f;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            g_transDefaultPressed = TransDefaultPanelHit(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &client, nullptr);
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONUP:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            TransLibFooter foot = {};
            const int pressed = g_transDefaultPressed;
            g_transDefaultPressed = -1;
            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
            const int hit = TransDefaultPanelHit(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &client, &foot);
            InvalidateRect(hwnd, nullptr, FALSE);
            if (pressed < 0 || pressed != hit)
            {
                return 0;
            }
            if (hit >= 0 && hit < TRANS_GRID_MAX)
            {
                g_transCurrent = g_transGrid[hit].kind;
                if (!Renderer_TransitionNeedsDir(g_transCurrent))
                {
                    g_transDir = SLIDE_TRANSITION_DIR_LEFT;
                }
                CommitTransitionChoice();
                return 0;
            }
            if (hit == 100 && foot.versionsOn)
            {
                POINT origin = { foot.versions.left, foot.versions.bottom };
                ClientToScreen(hwnd, &origin);
                g_transDefaultDirMenuOpen = true;
                const int dir = PickTransDir(hwnd, origin.x, origin.y, g_transDir);
                g_transDefaultDirMenuOpen = false;
                if (dir >= 0)
                {
                    g_transDir = dir;
                    CommitTransitionChoice();
                }
                return 0;
            }
            if (hit == 101 || hit == 102)
            {
                int next = g_transMs + (hit == 102 ? TRANS_MS_STEP : -TRANS_MS_STEP);
                if (next < TRANS_MS_MIN)
                {
                    next = TRANS_MS_MIN;
                }
                if (next > TRANS_MS_MAX)
                {
                    next = TRANS_MS_MAX;
                }
                next = ((next + 50) / 100) * 100;
                g_transMs = next;
                CommitTransitionChoice();
                return 0;
            }
            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            int next = g_transMs + (GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? TRANS_MS_STEP : -TRANS_MS_STEP);
            if (next < TRANS_MS_MIN)
            {
                next = TRANS_MS_MIN;
            }
            if (next > TRANS_MS_MAX)
            {
                next = TRANS_MS_MAX;
            }
            next = ((next + 50) / 100) * 100;
            if (next != g_transMs)
            {
                g_transMs = next;
                CommitTransitionChoice();
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                CloseTransDefaultPanel();
                return 0;
            }
            break;

        case WM_KILLFOCUS:
            if (!g_transDefaultDirMenuOpen)
            {
                POINT cursor = {};
                GetCursorPos(&cursor);
                HWND under = WindowFromPoint(cursor);
                if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && under == g_programLeft)
                {
                    POINT local = cursor;
                    ScreenToClient(g_programLeft, &local);
                    if (PtInRect(&g_transFootSlide, local) || PtInRect(&g_transFootBg, local))
                    {
                        g_transDefaultDismissTick = GetTickCount();
                    }
                }
                CloseTransDefaultPanel();
            }
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HDC memory = CreateCompatibleDC(hdc);
            HBITMAP canvas = CreateCompatibleBitmap(hdc, max(1, (int)client.right), max(1, (int)client.bottom));
            HBITMAP oldCanvas = (HBITMAP)SelectObject(memory, canvas);

            HBRUSH back = CreateSolidBrush(PROGRAM_COLOR_WELL);
            FillRect(memory, &client, back);
            DeleteObject(back);
            RECT head = { client.left, client.top, client.right, client.top + TRANS_DEFAULT_PANEL_HEAD };
            HBRUSH headFill = CreateSolidBrush(PROGRAM_COLOR_TOP);
            FillRect(memory, &head, headFill);
            DeleteObject(headFill);

            HFONT oldFont = (HFONT)SelectObject(memory, ProgramUiFont());
            SetBkMode(memory, TRANSPARENT);
            RECT title = head;
            title.left += 10;
            title.right -= 10;
            SetTextColor(memory, PROGRAM_COLOR_SELECT);
            DrawTextA(
                memory,
                g_transDefaultEdit == TRANS_DEFAULT_BG ? "BG default" : "Slide default",
                -1,
                &title,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX
            );
            SetTextColor(memory, RGB(0x80, 0x80, 0x80));
            DrawTextA(memory, "Renderer global", -1, &title, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            HPEN rule = CreatePen(PS_SOLID, 1, PROGRAM_COLOR_WELL_LIGHT);
            HPEN oldPen = (HPEN)SelectObject(memory, rule);
            MoveToEx(memory, client.left, client.top + TRANS_DEFAULT_PANEL_HEAD, nullptr);
            LineTo(memory, client.right, client.top + TRANS_DEFAULT_PANEL_HEAD);

            EnsureTransGrid();
            const int bodyBottom = client.bottom - TRANS_DEFAULT_PANEL_FOOT;
            const int saved = SaveDC(memory);
            IntersectClipRect(memory, client.left, client.top + TRANS_DEFAULT_PANEL_HEAD + 1, client.right, bodyBottom);
            for (int i = 0; i < TRANS_GRID_MAX; ++i)
            {
                const RECT cell = TransDefaultPanelCell(i);
                if (cell.bottom < TRANS_DEFAULT_PANEL_HEAD || cell.top > bodyBottom)
                {
                    continue;
                }
                EnsureTransGridThumb(
                    i,
                    TRANS_DEFAULT_PANEL_THUMB_W,
                    TRANS_DEFAULT_PANEL_THUMB_H,
                    g_transDir
                );
                RECT thumb = { cell.left, cell.top, cell.right, cell.top + TRANS_DEFAULT_PANEL_THUMB_H };
                const bool on = g_transGrid[i].kind == g_transCurrent;
                const bool hot = i == g_transDefaultHover || i == g_transDefaultPressed;
                SceneRound(
                    memory,
                    &cell,
                    8.0f,
                    on || hot ? LOOK_COLOR_RAISED : LOOK_COLOR_SUNKEN,
                    on ? LOOK_COLOR_ACCENT : PROGRAM_COLOR_CONTROL_EDGE
                );
                HBITMAP frame = g_transGrid[i].thumb;
                if (i == g_transDefaultHover && g_transGrid[i].scrub != nullptr)
                {
                    frame = g_transGrid[i].scrub;
                }
                if (frame != nullptr)
                {
                    HDC thumbDc = CreateCompatibleDC(memory);
                    HBITMAP oldThumb = (HBITMAP)SelectObject(thumbDc, frame);
                    BitBlt(
                        memory,
                        thumb.left,
                        thumb.top,
                        TRANS_DEFAULT_PANEL_THUMB_W,
                        TRANS_DEFAULT_PANEL_THUMB_H,
                        thumbDc,
                        0,
                        0,
                        SRCCOPY
                    );
                    SelectObject(thumbDc, oldThumb);
                    DeleteDC(thumbDc);
                }
                if (i == g_transDefaultHover)
                {
                    const int barX =
                        thumb.left +
                        (int)((thumb.right - thumb.left - 2) * g_transDefaultHoverRatio);
                    RECT bar = { barX, thumb.top, barX + 2, thumb.bottom };
                    HBRUSH mark = CreateSolidBrush(RGB(255, 255, 255));
                    FillRect(memory, &bar, mark);
                    DeleteObject(mark);
                }
                RECT chin = { cell.left + 2, thumb.bottom, cell.right - 2, cell.bottom };
                SetTextColor(memory, on || hot ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
                DrawTextA(memory, g_transGrid[i].label, -1, &chin, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            RestoreDC(memory, saved);

            TransLibFooter foot = {};
            LayoutTransDefaultPanelFooter(&client, &foot);
            if (foot.versionsOn)
            {
                SceneRound(memory, &foot.versions, 6.0f, LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
                SetTextColor(memory, LOOK_COLOR_INK_BRIGHT);
                DrawTextA(memory, TransDirName(g_transDir), -1, &foot.versions, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            SceneRound(memory, &foot.minus, 6.0f, LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
            SceneRound(memory, &foot.plus, 6.0f, LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
            SetTextColor(memory, LOOK_COLOR_INK_BRIGHT);
            DrawTextA(memory, "-", -1, &foot.minus, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            DrawTextA(memory, "+", -1, &foot.plus, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            char msText[24] = {};
            const int tenths = (g_transMs + 50) / 100;
            wsprintfA(msText, "%d.%ds", tenths / 10, tenths % 10);
            SetTextColor(memory, LOOK_COLOR_INK);
            DrawTextA(memory, msText, -1, &foot.ms, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            HBRUSH oldBrush = (HBRUSH)SelectObject(memory, GetStockObject(NULL_BRUSH));
            Rectangle(memory, client.left, client.top, client.right, client.bottom);
            SelectObject(memory, oldBrush);
            SelectObject(memory, oldPen);
            DeleteObject(rule);
            SelectObject(memory, oldFont);

            BitBlt(hdc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
            SelectObject(memory, oldCanvas);
            DeleteObject(canvas);
            DeleteDC(memory);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_DESTROY:
            if (g_transDefaultPanel == hwnd)
            {
                g_transDefaultPanel = nullptr;
                g_transDefaultEdit = TRANS_DEFAULT_NONE;
            }
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static TemplateThumb g_templateThumbs[STORE_TEMPLATE_MAX] = {};
static int g_templateThumbCount = 0;
static int g_templatePanelScroll = 0;
static int g_templatePanelHover = -1;
static int g_templatePanelPressed = -1;
static DWORD g_templatePanelDismissTick = 0;
static bool g_templatePanelClosing = false;

static FILETIME TemplateFileTime(const char* path)
{
    FILETIME written = {};
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (path != nullptr && GetFileAttributesExA(path, GetFileExInfoStandard, &data))
    {
        written = data.ftLastWriteTime;
    }
    return written;
}

static int TemplatePanelRows(int count)
{
    return (count + TEMPLATE_PANEL_COLS - 1) / TEMPLATE_PANEL_COLS;
}

static int TemplatePanelRowsHeight(int rows)
{
    if (rows <= 0)
    {
        return 0;
    }
    return rows * (TEMPLATE_THUMB_H + TEMPLATE_NAME_H) + (rows - 1) * TEMPLATE_CELL_GAP;
}

static int TemplatePanelWidth()
{
    return TEMPLATE_PANEL_PAD * 2 +
        TEMPLATE_PANEL_COLS * TEMPLATE_THUMB_W +
        (TEMPLATE_PANEL_COLS - 1) * TEMPLATE_CELL_GAP;
}

static int TemplatePanelHeight()
{
    if (g_templateThumbCount <= 0)
    {
        return TEMPLATE_PANEL_HEAD + TEMPLATE_PANEL_EMPTY_H;
    }
    const int rows = min(TemplatePanelRows(g_templateThumbCount), TEMPLATE_PANEL_MAX_ROWS);
    return TEMPLATE_PANEL_HEAD + TEMPLATE_PANEL_PAD * 2 + TemplatePanelRowsHeight(rows);
}

static void ClampTemplatePanelScroll()
{
    const int visible = TemplatePanelRowsHeight(min(TemplatePanelRows(g_templateThumbCount), TEMPLATE_PANEL_MAX_ROWS));
    const int maxScroll = max(0, TemplatePanelRowsHeight(TemplatePanelRows(g_templateThumbCount)) - visible);
    g_templatePanelScroll = max(0, min(g_templatePanelScroll, maxScroll));
}

static RECT TemplatePanelCell(int index)
{
    const int col = index % TEMPLATE_PANEL_COLS;
    const int row = index / TEMPLATE_PANEL_COLS;
    const int x = TEMPLATE_PANEL_PAD + col * (TEMPLATE_THUMB_W + TEMPLATE_CELL_GAP);
    const int y = TEMPLATE_PANEL_HEAD + TEMPLATE_PANEL_PAD +
        row * (TEMPLATE_THUMB_H + TEMPLATE_NAME_H + TEMPLATE_CELL_GAP) - g_templatePanelScroll;
    RECT cell = { x, y, x + TEMPLATE_THUMB_W, y + TEMPLATE_THUMB_H + TEMPLATE_NAME_H };
    return cell;
}

// The Template Maker is a separate program, so the catalog is re-read every
// time the panel opens. Thumbnails are only redrawn for templates whose file
// changed.
static void RefreshTemplateThumbs()
{
    Template_RefreshCatalog();
    static TemplateThumb previous[STORE_TEMPLATE_MAX];
    const int previousCount = g_templateThumbCount;
    memcpy(previous, g_templateThumbs, sizeof(TemplateThumb) * previousCount);
    bool* reused = static_cast<bool*>(calloc(previousCount > 0 ? previousCount : 1, sizeof(bool)));

    int count = 0;
    const int catalog = Template_CatalogCount();
    for (int i = 0; i < catalog && count < STORE_TEMPLATE_MAX; ++i)
    {
        StoreTemplate item = {};
        if (!Template_CatalogGet(i, &item))
        {
            continue;
        }
        TemplateThumb* thumb = &g_templateThumbs[count];
        ZeroMemory(thumb, sizeof(*thumb));
        lstrcpynA(thumb->id, item.id, sizeof(thumb->id));
        lstrcpynA(thumb->name, item.name, sizeof(thumb->name));
        lstrcpynA(thumb->path, item.path, sizeof(thumb->path));
        thumb->written = TemplateFileTime(item.path);
        for (int j = 0; j < previousCount; ++j)
        {
            if (!reused[j] &&
                previous[j].bitmap != nullptr &&
                lstrcmpA(previous[j].id, thumb->id) == 0 &&
                lstrcmpiA(previous[j].path, thumb->path) == 0 &&
                CompareFileTime(&previous[j].written, &thumb->written) == 0)
            {
                thumb->bitmap = previous[j].bitmap;
                reused[j] = true;
                break;
            }
        }
        if (thumb->bitmap == nullptr)
        {
            thumb->bitmap = Renderer_RenderTemplateThumb(thumb->path, TEMPLATE_THUMB_W, TEMPLATE_THUMB_H);
        }
        count += 1;
    }
    for (int j = 0; j < previousCount; ++j)
    {
        if (!reused[j] && previous[j].bitmap != nullptr)
        {
            DeleteObject(previous[j].bitmap);
        }
    }
    free(reused);
    g_templateThumbCount = count;
    g_templatePanelHover = -1;
    g_templatePanelPressed = -1;
    ClampTemplatePanelScroll();
}

static void FreeTemplateThumbs()
{
    for (int i = 0; i < g_templateThumbCount; ++i)
    {
        if (g_templateThumbs[i].bitmap != nullptr)
        {
            DeleteObject(g_templateThumbs[i].bitmap);
        }
    }
    ZeroMemory(g_templateThumbs, sizeof(g_templateThumbs));
    g_templateThumbCount = 0;
}

static int TemplatePanelHit(int x, int y)
{
    if (y < TEMPLATE_PANEL_HEAD)
    {
        return -1;
    }
    POINT pt = { x, y };
    for (int i = 0; i < g_templateThumbCount; ++i)
    {
        RECT cell = TemplatePanelCell(i);
        if (PtInRect(&cell, pt))
        {
            return i;
        }
    }
    return -1;
}

// Show view restyles the selected slides; Edit view restyles the whole show.
// Either way it is one undo step.
static void ApplyTemplateFromPanel(int item)
{
    if (item < 0 || item >= g_templateThumbCount || !Document_HasProject())
    {
        return;
    }
    int indices[PROGRAM_SLIDE_MAX] = {};
    int count = 0;
    if (g_programView == PROGRAM_VIEW_EDIT)
    {
        count = ProgramSlideCount();
        for (int i = 0; i < count; ++i)
        {
            indices[i] = i;
        }
    }
    else
    {
        count = SelectedSlideIndices(indices, PROGRAM_SLIDE_MAX);
        const int current = Renderer_CurrentSlide();
        if (count <= 0 && current >= 0 && current < ProgramSlideCount())
        {
            indices[count++] = current;
        }
    }
    if (count <= 0)
    {
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    if (GetFileAttributesA(g_templateThumbs[item].path) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxA(
            g_programWindow != nullptr ? g_programWindow : g_mainWindow,
            "That template's file is missing.",
            "Templates",
            MB_OK | MB_ICONWARNING
        );
        RefreshTemplateThumbs();
        return;
    }

    EditView_ClearSelection();
    History_Remember();
    if (!Renderer_ApplyTemplate(g_templateThumbs[item].path, indices, count))
    {
        return;
    }
    EventLog("UI Template applied");
    Document_SaveOrWarn(g_programWindow != nullptr ? g_programWindow : g_mainWindow);
    RefreshSlideLists();
}

static void CloseTemplatePanel()
{
    if (g_templatePanelClosing)
    {
        return;
    }
    g_templatePanelClosing = true;
    HWND hwnd = g_templatePanel;
    g_templatePanel = nullptr;
    g_templatePanelHover = -1;
    g_templatePanelPressed = -1;
    if (hwnd != nullptr && IsWindow(hwnd))
    {
        DestroyWindow(hwnd);
    }
    if (g_programTemplatesBtn != nullptr)
    {
        InvalidateRect(g_programTemplatesBtn, nullptr, FALSE);
    }
    g_templatePanelClosing = false;
}

static void ShowTemplatePanel()
{
    CloseTemplatePanel();
    if (g_programTemplatesBtn == nullptr)
    {
        return;
    }
    RefreshTemplateThumbs();
    g_templatePanelScroll = 0;

    RECT button = {};
    GetWindowRect(g_programTemplatesBtn, &button);
    const int width = TemplatePanelWidth();
    const int height = TemplatePanelHeight();
    int x = button.left;
    int y = button.bottom + 2;
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoA(MonitorFromRect(&button, MONITOR_DEFAULTTONEAREST), &info))
    {
        if (x + width > info.rcWork.right)
        {
            x = max((int)info.rcWork.left, (int)info.rcWork.right - width);
        }
        if (y + height > info.rcWork.bottom && button.top - 2 - height >= info.rcWork.top)
        {
            y = button.top - 2 - height;
        }
    }

    g_templatePanel = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        "TemplatePanel",
        "",
        WS_POPUP,
        x,
        y,
        width,
        height,
        g_programWindow,
        nullptr,
        g_instance,
        nullptr
    );
    if (g_templatePanel == nullptr)
    {
        return;
    }
    ShowWindow(g_templatePanel, SW_SHOW);
    SetForegroundWindow(g_templatePanel);
    InvalidateRect(g_programTemplatesBtn, nullptr, FALSE);
}

// Pressing the button while the panel is open takes focus from the panel,
// which already closes it, so that same click must not reopen it.
static void ToggleTemplatePanel()
{
    if (g_templatePanel != nullptr)
    {
        CloseTemplatePanel();
        return;
    }
    if (g_templatePanelDismissTick != 0 && GetTickCount() - g_templatePanelDismissTick < 500)
    {
        g_templatePanelDismissTick = 0;
        return;
    }
    ShowTemplatePanel();
}

LRESULT CALLBACK TemplatePanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
        {
            const int hit = TemplatePanelHit(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (hit != g_templatePanelHover)
            {
                g_templatePanelHover = hit;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT track = {};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            TrackMouseEvent(&track);
            return 0;
        }

        case WM_MOUSELEAVE:
            g_templatePanelHover = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            g_templatePanelPressed = TemplatePanelHit(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONUP:
        {
            const int pressed = g_templatePanelPressed;
            g_templatePanelPressed = -1;
            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            if (pressed >= 0 && pressed == TemplatePanelHit(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
            {
                CloseTemplatePanel();
                ApplyTemplateFromPanel(pressed);
                if (g_programMiddle != nullptr && g_programView == PROGRAM_VIEW_SHOW)
                {
                    SetFocus(g_programMiddle);
                }
            }
            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            g_templatePanelScroll -= GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA *
                (TEMPLATE_THUMB_H + TEMPLATE_NAME_H + TEMPLATE_CELL_GAP);
            ClampTemplatePanelScroll();
            POINT cursor = {};
            GetCursorPos(&cursor);
            ScreenToClient(hwnd, &cursor);
            g_templatePanelHover = TemplatePanelHit(cursor.x, cursor.y);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                CloseTemplatePanel();
                return 0;
            }
            break;

        case WM_KILLFOCUS:
        {
            POINT cursor = {};
            GetCursorPos(&cursor);
            if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && WindowFromPoint(cursor) == g_programTemplatesBtn)
            {
                g_templatePanelDismissTick = GetTickCount();
            }
            CloseTemplatePanel();
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HDC memory = CreateCompatibleDC(hdc);
            HBITMAP canvas = CreateCompatibleBitmap(hdc, max(1, (int)client.right), max(1, (int)client.bottom));
            HBITMAP oldCanvas = (HBITMAP)SelectObject(memory, canvas);

            HBRUSH back = CreateSolidBrush(PROGRAM_COLOR_WELL);
            FillRect(memory, &client, back);
            DeleteObject(back);
            RECT head = { client.left, client.top, client.right, client.top + TEMPLATE_PANEL_HEAD };
            HBRUSH headFill = CreateSolidBrush(PROGRAM_COLOR_TOP);
            FillRect(memory, &head, headFill);
            DeleteObject(headFill);

            HFONT oldFont = (HFONT)SelectObject(memory, ProgramUiFont());
            SetBkMode(memory, TRANSPARENT);
            RECT title = head;
            title.left += 10;
            title.right -= 10;
            SetTextColor(memory, PROGRAM_COLOR_SELECT);
            DrawTextA(memory, "Templates", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SetTextColor(memory, RGB(0x80, 0x80, 0x80));
            DrawTextA(
                memory,
                g_programView == PROGRAM_VIEW_EDIT ? "Applies to the whole show" : "Applies to selected slides",
                -1,
                &title,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX
            );

            HPEN rule = CreatePen(PS_SOLID, 1, PROGRAM_COLOR_WELL_LIGHT);
            HPEN oldPen = (HPEN)SelectObject(memory, rule);
            MoveToEx(memory, client.left, client.top + TEMPLATE_PANEL_HEAD, nullptr);
            LineTo(memory, client.right, client.top + TEMPLATE_PANEL_HEAD);

            RECT body = { client.left, client.top + TEMPLATE_PANEL_HEAD + 1, client.right, client.bottom };
            if (g_templateThumbCount == 0)
            {
                SetTextColor(memory, RGB(0x80, 0x80, 0x80));
                DrawTextA(memory, "No templates yet. Make one in Template Maker.", -1, &body,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            const int saved = SaveDC(memory);
            IntersectClipRect(memory, body.left, body.top, body.right, body.bottom);
            HDC thumbDc = CreateCompatibleDC(hdc);
            for (int i = 0; i < g_templateThumbCount; ++i)
            {
                const RECT cell = TemplatePanelCell(i);
                if (cell.bottom < body.top || cell.top > body.bottom)
                {
                    continue;
                }
                RECT thumbRc = { cell.left, cell.top, cell.right, cell.top + TEMPLATE_THUMB_H };
                if (g_templateThumbs[i].bitmap != nullptr)
                {
                    HBITMAP oldThumb = (HBITMAP)SelectObject(thumbDc, g_templateThumbs[i].bitmap);
                    BitBlt(memory, thumbRc.left, thumbRc.top, TEMPLATE_THUMB_W, TEMPLATE_THUMB_H, thumbDc, 0, 0, SRCCOPY);
                    SelectObject(thumbDc, oldThumb);
                }
                else
                {
                    HBRUSH empty = CreateSolidBrush(PROGRAM_COLOR_WELL_DARK);
                    FillRect(memory, &thumbRc, empty);
                    DeleteObject(empty);
                }
                const bool hot = i == g_templatePanelHover || i == g_templatePanelPressed;
                HBRUSH edge = CreateSolidBrush(hot ? PROGRAM_COLOR_SELECT : PROGRAM_COLOR_WELL_LIGHT);
                FrameRect(memory, &thumbRc, edge);
                DeleteObject(edge);

                RECT nameRc = { cell.left, thumbRc.bottom + 2, cell.right, cell.bottom };
                SetTextColor(memory, hot ? PROGRAM_COLOR_SELECT : PROGRAM_COLOR_INK);
                DrawTextA(memory, g_templateThumbs[i].name, -1, &nameRc,
                    DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
            DeleteDC(thumbDc);
            RestoreDC(memory, saved);

            HBRUSH oldBrush = (HBRUSH)SelectObject(memory, GetStockObject(NULL_BRUSH));
            Rectangle(memory, client.left, client.top, client.right, client.bottom);
            SelectObject(memory, oldBrush);
            SelectObject(memory, oldPen);
            DeleteObject(rule);
            SelectObject(memory, oldFont);
            BitBlt(hdc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
            SelectObject(memory, oldCanvas);
            DeleteObject(canvas);
            DeleteDC(memory);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_DESTROY:
            if (hwnd == g_templatePanel)
            {
                g_templatePanel = nullptr;
            }
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void LayoutProgramModeButtons()
{
    if (g_programTop == nullptr)
    {
        return;
    }

    RECT rc = {};
    GetClientRect(g_programTop, &rc);
    const int pad = 12;
    const int gap = PROGRAM_TILE_GAP;
    const int y = (rc.bottom - PROGRAM_MODE_BTN_H) / 2;
    if (g_programShowButton != nullptr)
    {
        MoveWindow(g_programShowButton, pad, y, PROGRAM_MODE_BTN_W, PROGRAM_MODE_BTN_H, TRUE);
    }
    if (g_programEditButton != nullptr)
    {
        MoveWindow(
            g_programEditButton,
            pad + PROGRAM_MODE_BTN_W + gap,
            y,
            PROGRAM_MODE_BTN_W,
            PROGRAM_MODE_BTN_H,
            TRUE
        );
    }

    const int tileStep = PROGRAM_TILE_W + PROGRAM_TILE_GAP;
    const int tileY = (rc.bottom - PROGRAM_TILE_H) / 2;
    const int modeRight = pad + (PROGRAM_MODE_BTN_W * 2) + gap;
    const int buttonsX = modeRight + PROGRAM_TOP_SEP_SPACE;
    const int clearRight = buttonsX + (tileStep * 3) - gap;
    g_programTopSep[0] = modeRight + (PROGRAM_TOP_SEP_SPACE / 2);
    g_programTopSep[1] = g_programView == PROGRAM_VIEW_SHOW ? clearRight + (PROGRAM_TOP_SEP_SPACE / 2) : -1;
    if (g_programClearSlideBtn != nullptr)
    {
        const int clearX = buttonsX;
        MoveWindow(g_programClearSlideBtn, clearX, tileY, PROGRAM_TILE_W, PROGRAM_TILE_H, TRUE);
        if (g_programClearBgBtn != nullptr)
        {
            MoveWindow(g_programClearBgBtn, clearX + tileStep, tileY, PROGRAM_TILE_W, PROGRAM_TILE_H, TRUE);
        }
        if (g_programClearAudioBtn != nullptr)
        {
            MoveWindow(g_programClearAudioBtn, clearX + tileStep * 2, tileY, PROGRAM_TILE_W, PROGRAM_TILE_H, TRUE);
        }
    }
    if (g_programQuickSlidesBtn != nullptr)
    {
        MoveWindow(g_programQuickSlidesBtn, buttonsX + tileStep, tileY, PROGRAM_TILE_W, PROGRAM_TILE_H, TRUE);
    }
    const int importSize = 56;
    const int importX = rc.right - pad - importSize;
    if (g_importButton != nullptr)
    {
        const int importY = (rc.bottom - importSize) / 2;
        MoveWindow(g_importButton, importX, importY, importSize, importSize, TRUE);
    }
    if (g_programTemplatesBtn != nullptr)
    {
        const int templatesX = g_programView == PROGRAM_VIEW_EDIT
            ? buttonsX
            : clearRight + PROGRAM_TOP_SEP_SPACE;
        MoveWindow(g_programTemplatesBtn, templatesX, tileY, PROGRAM_TILE_W, PROGRAM_TILE_H, TRUE);
    }
    if (g_programScenesBtn != nullptr)
    {
        const int scenesRight = g_importButtonShown != 0 ? importX - gap : rc.right - pad;
        const int scenesX = scenesRight - PROGRAM_SCENES_TILE_W;
        MoveWindow(g_programScenesBtn, scenesX, tileY, PROGRAM_SCENES_TILE_W, PROGRAM_TILE_H, TRUE);
        if (g_programDisplaysBtn != nullptr)
        {
            MoveWindow(g_programDisplaysBtn, scenesX - tileStep, tileY, PROGRAM_TILE_W, PROGRAM_TILE_H, TRUE);
        }
    }
    InvalidateRect(g_programTop, nullptr, FALSE);
}

void SetProgramView(int view)
{
    g_programView = view;
    const bool showLive = view == PROGRAM_VIEW_SHOW;
    if (g_programLeft != nullptr)
    {
        ShowWindow(g_programLeft, showLive ? SW_SHOW : SW_HIDE);
    }
    if (g_programPreview != nullptr)
    {
        ShowWindow(g_programPreview, showLive ? SW_SHOW : SW_HIDE);
    }
    if (g_programMirror != nullptr)
    {
        ShowWindow(g_programMirror, showLive ? SW_SHOW : SW_HIDE);
    }
    if (g_programVideoBtn != nullptr)
    {
        ShowWindow(g_programVideoBtn, showLive ? SW_SHOW : SW_HIDE);
    }
    if (g_programAudioBtn != nullptr)
    {
        ShowWindow(g_programAudioBtn, showLive ? SW_SHOW : SW_HIDE);
    }
    if (g_programClearSlideBtn != nullptr)
    {
        ShowWindow(g_programClearSlideBtn, showLive ? SW_SHOW : SW_HIDE);
    }
    if (g_programClearBgBtn != nullptr)
    {
        ShowWindow(g_programClearBgBtn, showLive ? SW_SHOW : SW_HIDE);
    }
    if (g_programClearAudioBtn != nullptr)
    {
        ShowWindow(g_programClearAudioBtn, showLive ? SW_SHOW : SW_HIDE);
    }
    if (g_programQuickSlidesBtn != nullptr)
    {
        ShowWindow(g_programQuickSlidesBtn, showLive ? SW_HIDE : SW_SHOW);
    }
    if (showLive)
    {
        HideQuickSlides();
    }
    if (g_programMiddle != nullptr)
    {
        ShowWindow(g_programMiddle, showLive ? SW_SHOW : SW_HIDE);
    }
    if (g_programRight != nullptr)
    {
        ShowWindow(g_programRight, showLive ? SW_SHOW : SW_HIDE);
        if (g_audioTracks != nullptr)
        {
            ShowWindow(g_audioTracks, showLive ? SW_SHOW : SW_HIDE);
        }
        if (g_audioPlaylists != nullptr)
        {
            ShowWindow(g_audioPlaylists, showLive ? SW_SHOW : SW_HIDE);
        }
        if (g_audioNewPlaylist != nullptr)
        {
            ShowWindow(g_audioNewPlaylist, showLive ? SW_SHOW : SW_HIDE);
        }
    }
    EditView_Show(!showLive);
    if (g_programShowButton != nullptr)
    {
        InvalidateRect(g_programShowButton, nullptr, FALSE);
    }
    if (g_programEditButton != nullptr)
    {
        InvalidateRect(g_programEditButton, nullptr, FALSE);
    }
    LayoutProgramSections(g_programWindow);
    UpdateDocumentMenus();
}

HWND CreateProgramModeButton(HWND parent, int view)
{
    HWND button = CreateWindowExA(
        0,
        "ProgramModeButton",
        "",
        WS_CHILD | WS_VISIBLE,
        0,
        0,
        PROGRAM_MODE_BTN_W,
        PROGRAM_MODE_BTN_H,
        parent,
        nullptr,
        g_instance,
        nullptr
    );
    SetWindowLongPtrA(button, GWLP_USERDATA, view);
    return button;
}

LRESULT CALLBACK ProgramModeButtonProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
        {
            if (g_programModeHover != hwnd)
            {
                g_programModeHover = hwnd;
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (g_programModeHover == hwnd)
            {
                g_programModeHover = nullptr;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONUP:
            SetProgramView((int)GetWindowLongPtrA(hwnd, GWLP_USERDATA));
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int view = (int)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            const bool selected = g_programView == view;
            const bool hover = g_programModeHover == hwnd;
            const COLORREF ink = selected ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK;
            PaintProgramTileBack(hdc, &client, selected ? PROGRAM_TILE_ON : (hover ? PROGRAM_TILE_HOVER : PROGRAM_TILE_IDLE));

            const int iconY = client.top + PROGRAM_TILE_ICON_Y;
            const int iconX = (client.left + client.right) / 2;
            if (!SvgIcon_Draw(hdc, view == PROGRAM_VIEW_SHOW ? "show" : "edit", iconX, iconY, PROGRAM_TILE_ICON, ink))
            {
                if (view == PROGRAM_VIEW_SHOW)
                {
                    DrawProgramShowIcon(hdc, iconX, iconY, ink);
                }
                else
                {
                    DrawProgramEditIcon(hdc, iconX, iconY, ink);
                }
            }
            PaintProgramTileLabel(hdc, &client, view == PROGRAM_VIEW_SHOW ? "Show" : "Edit", ink);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void LayoutProgramSections(HWND hwnd)
{
    if (hwnd == nullptr)
    {
        hwnd = g_programWindow;
    }
    if (hwnd == nullptr)
    {
        return;
    }

    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const int height = rc.bottom;
    if (rc.right <= 0 || height <= 0)
    {
        return;
    }

    const int topY = PROGRAM_TITLE_H;
    const int topH = PROGRAM_TOP_HEIGHT;
    const int bodyY = topY + topH;
    int bodyH = height - bodyY;
    if (bodyH < 0)
    {
        bodyH = 0;
    }
    const int panelH = bodyH > PROGRAM_MARGIN ? bodyH - PROGRAM_MARGIN : 0;

    int middleWidth = rc.right - g_programLeftWidth - g_programRightWidth - (PROGRAM_GAP * 2) - (PROGRAM_MARGIN * 2);
    if (middleWidth < 0)
    {
        middleWidth = 0;
    }

    if (g_programTop != nullptr)
    {
        MoveWindow(g_programTop, 0, topY, rc.right, topH, TRUE);
        LayoutProgramModeButtons();
    }

    EditView_Layout(0, bodyY, rc.right, bodyH);

    if (g_programView == PROGRAM_VIEW_SHOW)
    {
        if (g_programLeft != nullptr)
        {
            MoveWindow(g_programLeft, PROGRAM_MARGIN, bodyY, g_programLeftWidth, panelH, TRUE);
        }

        if (g_programRight != nullptr)
        {
            MoveWindow(
                g_programRight,
                rc.right - PROGRAM_MARGIN - g_programRightWidth,
                bodyY,
                g_programRightWidth,
                panelH,
                TRUE
            );
            LayoutAudioRail();
        }

        if (g_programMiddle != nullptr)
        {
            MoveWindow(
                g_programMiddle,
                PROGRAM_MARGIN + g_programLeftWidth + PROGRAM_GAP,
                bodyY,
                middleWidth,
                panelH,
                TRUE
            );
            SetWindowRgn(g_programMiddle, nullptr, TRUE);
            UpdateProgramMiddleScroll(g_programMiddle);
        }

        LayoutProgramPreview();
    }
}

LRESULT CALLBACK ProgramSectionProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
        case WM_CTLCOLORSTATIC:
            if (hwnd == g_programLeft)
            {
                const HWND label = (HWND)lParam;
                if (label == g_programVideoTime || label == g_programVideoDuration ||
                    label == g_programAudioLabel || label == g_programAudioDuration)
                {
                    HDC hdc = (HDC)wParam;
                    SetBkColor(hdc, LOOK_COLOR_SURFACE);
                    SetTextColor(hdc, LOOK_COLOR_INK_DIM);
                    static HBRUSH brush = nullptr;
                    if (brush == nullptr)
                    {
                        brush = CreateSolidBrush(LOOK_COLOR_SURFACE);
                    }
                    return (LRESULT)brush;
                }
            }
            break;

        case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            if (measure == nullptr)
            {
                break;
            }
            if (measure->CtlID == ID_AUDIO_TRACKS || measure->CtlID == ID_AUDIO_PLAYLISTS)
            {
                measure->itemHeight = PROGRAM_AUDIO_ROW;
                return TRUE;
            }
            if (measure->CtlID == ID_SCREEN_MIRROR)
            {
                measure->itemHeight = measure->itemID == static_cast<UINT>(-1) ? PROGRAM_MIRROR_H - 6 : 28;
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (draw == nullptr || draw->hwndItem == nullptr)
            {
                break;
            }
            if (draw->CtlID == ID_SCREEN_MIRROR)
            {
                if ((draw->itemState & ODS_COMBOBOXEDIT) != 0)
                {
                    InvalidateRect(draw->hwndItem, nullptr, FALSE);
                    return TRUE;
                }
                PaintProgramMirrorItem(draw);
                return TRUE;
            }
            if (draw->CtlID != ID_AUDIO_TRACKS && draw->CtlID != ID_AUDIO_PLAYLISTS)
            {
                break;
            }
            if (draw->itemID == static_cast<UINT>(-1))
            {
                HBRUSH fill = CreateSolidBrush(LOOK_COLOR_SURFACE);
                FillRect(draw->hDC, &draw->rcItem, fill);
                DeleteObject(fill);
                return TRUE;
            }
            PaintAudioRow(draw->hwndItem, draw->hDC, &draw->rcItem, static_cast<int>(draw->itemID),
                          (draw->itemState & ODS_SELECTED) != 0);
            return TRUE;
        }

        case WM_DROPFILES:
        {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            bool imported = false;
            for (UINT i = 0; i < count; ++i)
            {
                wchar_t path[MAX_PATH] = {};
                if (DragQueryFileW(drop, i, path, MAX_PATH) == 0)
                {
                    continue;
                }
                if (hwnd == g_programLeft && g_programView == PROGRAM_VIEW_SHOW && g_leftLibTab == LEFT_LIB_MEDIA)
                {
                    if (ImportLeftLibMedia(path))
                    {
                        imported = true;
                    }
                }
                else
                {
                    ImportDroppedMedia(g_programWindow != nullptr ? g_programWindow : hwnd, path);
                }
            }
            DragFinish(drop);
            if (imported && hwnd == g_programLeft)
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            if (hwnd == g_programMiddle)
            {
                UpdateProgramMiddleScroll(hwnd);
            }
            return 0;

        case WM_VSCROLL:
            if (hwnd == g_programMiddle)
            {
                const int code = LOWORD(wParam);
                int delta = 0;
                RECT client = {};
                GetClientRect(hwnd, &client);
                switch (code)
                {
                    case SB_LINEUP:
                        delta = -32;
                        break;
                    case SB_LINEDOWN:
                        delta = 32;
                        break;
                    case SB_PAGEUP:
                        delta = -client.bottom;
                        break;
                    case SB_PAGEDOWN:
                        delta = client.bottom;
                        break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION:
                    {
                        SCROLLINFO info = {};
                        info.cbSize = sizeof(info);
                        info.fMask = SIF_TRACKPOS;
                        GetScrollInfo(hwnd, SB_VERT, &info);
                        g_programSlideScroll = info.nTrackPos;
                        SetScrollPos(hwnd, SB_VERT, g_programSlideScroll, TRUE);
                        InvalidateRect(hwnd, nullptr, FALSE);
                        return 0;
                    }
                    default:
                        return 0;
                }
                ScrollProgramMiddle(hwnd, delta);
                return 0;
            }
            break;

        case WM_MOUSEWHEEL:
            if (hwnd == g_programMiddle)
            {
                const int delta = -GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 48;
                ScrollProgramMiddle(hwnd, delta);
                return 0;
            }
            if (hwnd == g_programLeft && g_leftLibTab == LEFT_LIB_MEDIA &&
                ScrollLeftLibMedia(hwnd, GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA))
            {
                return 0;
            }
            if (hwnd == g_programLeft && g_leftLibTab == LEFT_LIB_TRANSITIONS)
            {
                const int innerW = g_transLibBody.right - g_transLibBody.left;
                const int cardW = innerW > 20 ? (innerW - 6) / 2 : 80;
                const int thumbH = MulDiv(cardW, 9, 16);
                const int cardH = thumbH + 22;
                const int rows = (TRANS_GRID_MAX + 1) / 2;
                const int content = rows * (cardH + 6);
                const int view = g_transLibBody.bottom - g_transLibBody.top;
                int maxScroll = content > view ? content - view : 0;
                g_transLibScroll += -GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * max(cardH / 2, 24);
                if (g_transLibScroll < 0)
                {
                    g_transLibScroll = 0;
                }
                if (g_transLibScroll > maxScroll)
                {
                    g_transLibScroll = maxScroll;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;

        case WM_SETCURSOR:
            if (hwnd == g_programLeft || hwnd == g_programRight)
            {
                POINT pt = {};
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                RECT client = {};
                GetClientRect(hwnd, &client);
                const bool leftEdge = hwnd == g_programLeft && pt.x >= client.right - PROGRAM_SPLIT;
                const bool rightEdge = hwnd == g_programRight && pt.x <= PROGRAM_SPLIT;
                if (leftEdge || rightEdge)
                {
                    SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                    return TRUE;
                }
                if (hwnd == g_programRight)
                {
                    const int splitY = Audio_SplitTop();
                    if (pt.x > PROGRAM_SPLIT && pt.y >= splitY - 4 && pt.y <= splitY + 4)
                    {
                        SetCursor(LoadCursor(nullptr, IDC_SIZENS));
                        return TRUE;
                    }
                }
                if (hwnd == g_programLeft && pt.x < client.right - PROGRAM_SPLIT && HitLeftLibSplit(hwnd, pt.y))
                {
                    SetCursor(LoadCursor(nullptr, IDC_SIZENS));
                    return TRUE;
                }
                if (hwnd == g_programLeft && (g_programProjectDragging || g_leftLibMediaDragging))
                {
                    SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                    return TRUE;
                }
            }
            break;

        case WM_LBUTTONDOWN:
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            RECT client = {};
            GetClientRect(hwnd, &client);
            if (hwnd == g_programMiddle)
            {
                const int slide = HitTestProgramSlide(hwnd, x, y);
                g_slidePointerCtrl = (wParam & MK_CONTROL) != 0;
                g_slidePointerShift = (wParam & MK_SHIFT) != 0;
                if (slide >= 0)
                {
                    g_programSlideHeld = slide;
                    g_programSlideDrop = slide;
                    g_programSlideDownX = x;
                    g_programSlideDownY = y;
                    g_programSlideDragging = false;
                    SetCapture(hwnd);
                }
                else
                {
                    g_slideBoxSelecting = true;
                    g_slideBoxStart.x = x;
                    g_slideBoxStart.y = y;
                    g_slideBoxEnd = g_slideBoxStart;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (hwnd == g_programLeft && x < client.right - PROGRAM_SPLIT)
            {
                if (HitLeftLibSplit(hwnd, y))
                {
                    g_leftLibSplitDrag = 1;
                    g_leftLibSplitStartY = y;
                    g_leftLibSplitStartH = g_leftLibHeight;
                    SetCapture(hwnd);
                    return 0;
                }
                RECT leftClient = {};
                GetClientRect(hwnd, &leftClient);
                if (y >= ProgramLeftLibTop(&leftClient))
                {
                    if (g_leftLibTab == LEFT_LIB_MEDIA)
                    {
                        const int media = HitLeftLibMediaCard(x, y);
                        if (media >= 0)
                        {
                            g_leftLibMediaSel = media;
                            g_leftLibMediaHeld = media;
                            g_leftLibMediaDownX = x;
                            g_leftLibMediaDownY = y;
                            g_leftLibMediaDragging = false;
                            SetCapture(hwnd);
                            InvalidateRect(hwnd, nullptr, FALSE);
                            return 0;
                        }
                    }
                    ClickLeftLib(hwnd, x, y);
                    return 0;
                }
                const int project = HitTestProgramProject(hwnd, x, y);
                if (project >= 0)
                {
                    g_programProjectHeld = project;
                    g_programProjectDrop = project;
                    g_programProjectDownY = y;
                    g_programProjectDragging = false;
                    SetCapture(hwnd);
                    return 0;
                }
            }
            if ((hwnd == g_programLeft && x >= client.right - PROGRAM_SPLIT) ||
                (hwnd == g_programRight && x <= PROGRAM_SPLIT))
            {
                POINT pt = { x, GET_Y_LPARAM(lParam) };
                ClientToScreen(hwnd, &pt);
                ScreenToClient(g_programWindow, &pt);
                g_programSplit = hwnd == g_programLeft ? 1 : 2;
                g_programSplitStartX = pt.x;
                g_programSplitStartWidth = hwnd == g_programLeft ? g_programLeftWidth : g_programRightWidth;
                SetCapture(hwnd);
                return 0;
            }
            if (hwnd == g_programRight && x > PROGRAM_SPLIT)
            {
                const int splitY = Audio_SplitTop();
                const int y = GET_Y_LPARAM(lParam);
                if (y >= splitY - 4 && y <= splitY + 4)
                {
                    g_audioSplitDrag = 1;
                    g_audioSplitStartY = y;
                    g_audioSplitStartTop = Audio_SplitTop();
                    SetCapture(hwnd);
                    return 0;
                }
            }
            break;
        }

        case WM_MOUSEMOVE:
            if (g_slideBoxSelecting && (wParam & MK_LBUTTON) != 0)
            {
                g_slideBoxEnd.x = GET_X_LPARAM(lParam);
                g_slideBoxEnd.y = GET_Y_LPARAM(lParam);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_programSlideHeld >= 0 && (wParam & MK_LBUTTON) != 0)
            {
                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                int travelX = x - g_programSlideDownX;
                int travelY = y - g_programSlideDownY;
                if (travelX < 0)
                {
                    travelX = -travelX;
                }
                if (travelY < 0)
                {
                    travelY = -travelY;
                }
                const bool allowReorder =
                    !g_slidePointerCtrl &&
                    !g_slidePointerShift &&
                    ProgramSlideCount() > 1;
                if (!g_programSlideDragging && allowReorder && (travelX > 6 || travelY > 6))
                {
                    g_programSlideDragging = true;
                    SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                if (g_programSlideDragging)
                {
                    const int target = ProgramSlideDropFromPoint(hwnd, x, y);
                    if (target != g_programSlideDrop)
                    {
                        g_programSlideDrop = target;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
            if (g_leftLibMediaHeld >= 0 && (wParam & MK_LBUTTON) != 0)
            {
                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                int travelX = x - g_leftLibMediaDownX;
                int travelY = y - g_leftLibMediaDownY;
                if (travelX < 0)
                {
                    travelX = -travelX;
                }
                if (travelY < 0)
                {
                    travelY = -travelY;
                }
                if (!g_leftLibMediaDragging && (travelX > 6 || travelY > 6))
                {
                    g_leftLibMediaDragging = true;
                    SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                }
                return 0;
            }
            if (g_programProjectHeld >= 0 && (wParam & MK_LBUTTON) != 0)
            {
                const int y = GET_Y_LPARAM(lParam);
                int travel = y - g_programProjectDownY;
                if (travel < 0)
                {
                    travel = -travel;
                }
                if (!g_programProjectDragging && travel > 6)
                {
                    g_programProjectDragging = true;
                    SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                if (g_programProjectDragging)
                {
                    const int target = ProgramProjectDropFromY(y);
                    if (target != g_programProjectDrop)
                    {
                        g_programProjectDrop = target;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
            if (g_programSplit != 0 && (wParam & MK_LBUTTON) != 0)
            {
                RECT client = {};
                GetClientRect(g_programWindow, &client);
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ClientToScreen(hwnd, &pt);
                ScreenToClient(g_programWindow, &pt);
                if (g_programSplit == 1)
                {
                    g_programLeftWidth = ClampProgramWall(
                        g_programSplitStartWidth + (pt.x - g_programSplitStartX),
                        g_programRightWidth,
                        client.right
                    );
                }
                else
                {
                    g_programRightWidth = ClampProgramWall(
                        g_programSplitStartWidth - (pt.x - g_programSplitStartX),
                        g_programLeftWidth,
                        client.right
                    );
                }
                LayoutProgramSections(g_programWindow);
                return 0;
            }
            if (g_audioSplitDrag != 0 && (wParam & MK_LBUTTON) != 0)
            {
                const int y = GET_Y_LPARAM(lParam);
                Audio_SaveSplit(g_audioSplitStartTop + (y - g_audioSplitStartY));
                LayoutAudioRail();
                return 0;
            }
            if (g_leftLibSplitDrag != 0 && (wParam & MK_LBUTTON) != 0)
            {
                RECT client = {};
                GetClientRect(hwnd, &client);
                const int y = GET_Y_LPARAM(lParam);
                int next = g_leftLibSplitStartH - (y - g_leftLibSplitStartY);
                if (next < PROGRAM_LIB_MIN)
                {
                    next = PROGRAM_LIB_MIN;
                }
                const int dividerY = ProgramPreviewDividerY();
                const int maxH = client.bottom - dividerY - PROGRAM_GAP - PROGRAM_PROJECTS_MIN;
                if (next > maxH)
                {
                    next = maxH;
                }
                g_leftLibHeight = next;
                Store_SetInt("program.left_lib_height", g_leftLibHeight);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (hwnd == g_programLeft && g_leftLibTab == LEFT_LIB_MEDIA && (wParam & MK_LBUTTON) == 0)
            {
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);

                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                POINT pt = { x, y };
                int hover = -1;
                float ratio = 0.0f;
                for (int i = 0; i < g_leftLibMediaCount; ++i)
                {
                    RECT thumb = {};
                    LeftLibMediaCardRect(&g_leftLibMediaBody, i, &thumb, nullptr);
                    if (!PtInRect(&g_leftLibMediaBody, pt) || !PtInRect(&thumb, pt))
                    {
                        continue;
                    }
                    hover = i;
                    const int span = thumb.right - thumb.left;
                    ratio = span > 1 ? (float)(x - thumb.left) / (float)span : 0.0f;
                    if (ratio < 0.0f)
                    {
                        ratio = 0.0f;
                    }
                    if (ratio > 1.0f)
                    {
                        ratio = 1.0f;
                    }
                    const int thumbW = thumb.right - thumb.left;
                    const int thumbH = thumb.bottom - thumb.top;
                    LeftLibUpdateHoverScrub(i, ratio, thumbW, thumbH);
                    break;
                }
                if (hover != g_leftLibMediaHover || fabsf(ratio - g_leftLibMediaHoverRatio) > 0.005f)
                {
                    g_leftLibMediaHover = hover;
                    g_leftLibMediaHoverRatio = ratio;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (hwnd == g_programLeft &&
                g_leftLibTab == LEFT_LIB_TRANSITIONS &&
                g_transDefaultPanel == nullptr &&
                (wParam & MK_LBUTTON) == 0)
            {
                TRACKMOUSEEVENT track = {};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);

                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                POINT pt = { x, y };
                int hover = -1;
                float ratio = 0.0f;
                const int innerW = g_transLibBody.right - g_transLibBody.left;
                if (innerW > 20 && PtInRect(&g_transLibBody, pt))
                {
                    const int cols = 2;
                    const int gap = 6;
                    const int cardW = (innerW - gap) / cols;
                    const int thumbH = MulDiv(cardW, 9, 16);
                    const int cardH = thumbH + 22;
                    for (int i = 0; i < TRANS_GRID_MAX; ++i)
                    {
                        const int col = i % cols;
                        const int row = i / cols;
                        const int cardX = g_transLibBody.left + col * (cardW + gap);
                        const int cardY = g_transLibBody.top - g_transLibScroll + row * (cardH + gap);
                        RECT thumb = { cardX, cardY, cardX + cardW, cardY + thumbH };
                        if (!PtInRect(&thumb, pt))
                        {
                            continue;
                        }
                        hover = i;
                        const int span = thumb.right - thumb.left;
                        ratio = span > 1 ? (float)(x - thumb.left) / (float)span : 0.0f;
                        if (ratio < 0.0f)
                        {
                            ratio = 0.0f;
                        }
                        if (ratio > 1.0f)
                        {
                            ratio = 1.0f;
                        }
                        UpdateTransHoverScrub(i, ratio, cardW, thumbH, g_transDir);
                        break;
                    }
                }
                if (hover != g_transLibHover || fabsf(ratio - g_transLibHoverRatio) > 0.005f)
                {
                    g_transLibHover = hover;
                    g_transLibHoverRatio = ratio;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            break;

        case WM_MOUSELEAVE:
            if (hwnd == g_programLeft)
            {
                ClearLeftLibMediaHover(hwnd);
                ClearTransLibHover(hwnd);
                return 0;
            }
            break;

        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
            if (g_slideBoxSelecting || g_programSlideHeld >= 0)
            {
                FinishProgramSlidePointer(uMsg == WM_LBUTTONUP);
                if (uMsg == WM_LBUTTONUP)
                {
                    ReleaseCapture();
                }
                return 0;
            }
            if (g_programProjectHeld >= 0)
            {
                FinishProgramProjectPointer(uMsg == WM_LBUTTONUP);
                if (uMsg == WM_LBUTTONUP)
                {
                    ReleaseCapture();
                }
                return 0;
            }
            if (g_leftLibMediaHeld >= 0)
            {
                const int held = g_leftLibMediaHeld;
                const bool dragged = g_leftLibMediaDragging;
                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                g_leftLibMediaHeld = -1;
                g_leftLibMediaDragging = false;
                if (uMsg == WM_LBUTTONUP)
                {
                    if (dragged && LibraryMediaOverDeck(hwnd, x, y))
                    {
                        DropLibraryMediaOnDeck(held);
                    }
                    else if (!dragged)
                    {
                        ShowLibraryMediaLive(held);
                    }
                    ReleaseCapture();
                }
                return 0;
            }
            if (g_programSplit != 0)
            {
                g_programSplit = 0;
                if (uMsg == WM_LBUTTONUP)
                {
                    ReleaseCapture();
                }
                SaveProgramWalls();
                return 0;
            }
            if (g_audioSplitDrag != 0)
            {
                g_audioSplitDrag = 0;
                if (uMsg == WM_LBUTTONUP)
                {
                    ReleaseCapture();
                }
                return 0;
            }
            if (g_leftLibSplitDrag != 0)
            {
                g_leftLibSplitDrag = 0;
                if (uMsg == WM_LBUTTONUP)
                {
                    ReleaseCapture();
                }
                return 0;
            }
            break;

        case WM_RBUTTONUP:
            if (hwnd == g_programMiddle)
            {
                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                const int slide = HitTestProgramSlide(hwnd, x, y);
                if (slide >= 0 && !g_slideSelected[slide])
                {
                    // Highlight only. SelectSlide() would put it on the live
                    // screens, which a right-click must never do.
                    ClearSlideSelection();
                    g_slideSelected[slide] = true;
                    g_slideSelectAnchor = slide;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                POINT pt = { x, y };
                ClientToScreen(hwnd, &pt);
                ShowEditContextMenu(g_programWindow != nullptr ? g_programWindow : hwnd, pt.x, pt.y, CTX_SLIDES);
                return 0;
            }
            break;

        case WM_CONTEXTMENU:
            if (hwnd == g_programMiddle)
            {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (pt.x == -1 && pt.y == -1)
                {
                    RECT rc = {};
                    GetWindowRect(hwnd, &rc);
                    pt.x = rc.left + 8;
                    pt.y = rc.top + 8;
                }
                ShowEditContextMenu(g_programWindow != nullptr ? g_programWindow : hwnd, pt.x, pt.y, CTX_SLIDES);
                return 0;
            }
            break;

        case WM_COMMAND:
            if (hwnd == g_programLeft &&
                LOWORD(wParam) == ID_SCREEN_MIRROR &&
                (HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == CBN_CLOSEUP))
            {
                if (HIWORD(wParam) == CBN_SELCHANGE)
                {
                    ApplyPreviewComboChange();
                }
                else
                {
                    InvalidateRect(g_programLeft, nullptr, FALSE);
                    if (g_programMiddle != nullptr)
                    {
                        SetFocus(g_programMiddle);
                    }
                }
                return 0;
            }
            if (HandleContextMenuCommand(LOWORD(wParam)))
            {
                return 0;
            }
            if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == ID_AUDIO_TRACKS)
            {
                if (!g_audioListsRefreshing)
                {
                    const bool multiKey =
                        (GetKeyState(VK_CONTROL) & 0x8000) != 0 ||
                        (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    const int selCount = static_cast<int>(SendMessageA(g_audioTracks, LB_GETSELCOUNT, 0, 0));
                    if (!multiKey && selCount <= 1)
                    {
                        PlaySelectedAudioTrack();
                    }
                }
                return 0;
            }
            if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == ID_AUDIO_PLAYLISTS && g_audioPlaylists != nullptr)
            {
                const int sel = static_cast<int>(SendMessageA(g_audioPlaylists, LB_GETCURSEL, 0, 0));
                if (sel >= 0)
                {
                    const int id = static_cast<int>(SendMessageA(g_audioPlaylists, LB_GETITEMDATA, sel, 0));
                    if (id > 0 && id != Audio_ActivePlaylistId())
                    {
                        Audio_SetActivePlaylistId(id);
                        AudioTrack tracks[AUDIO_TRACK_MAX] = {};
                        const int trackCount = Audio_ListTracks(id, tracks, AUDIO_TRACK_MAX);
                        SendMessageA(g_audioTracks, LB_RESETCONTENT, 0, 0);
                        for (int i = 0; i < trackCount; ++i)
                        {
                            const int index = static_cast<int>(SendMessageA(
                                g_audioTracks,
                                LB_ADDSTRING,
                                0,
                                reinterpret_cast<LPARAM>(tracks[i].name)
                            ));
                            SendMessageA(g_audioTracks, LB_SETITEMDATA, index, tracks[i].id);
                        }
                    }
                }
                return 0;
            }
            break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            const LONG kind = GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            const int width = client.right;
            const int height = client.bottom;
            HDC target = hdc;
            HDC memory = nullptr;
            HBITMAP bitmap = nullptr;
            HBITMAP oldBitmap = nullptr;
            if (width > 0 && height > 0)
            {
                memory = CreateCompatibleDC(hdc);
                bitmap = CreateCompatibleBitmap(hdc, width, height);
                oldBitmap = (HBITMAP)SelectObject(memory, bitmap);
                target = memory;
            }

            HBRUSH fill = CreateSolidBrush(LOOK_COLOR_BASE);
            FillRect(target, &client, fill);
            DeleteObject(fill);

            if (kind == 2)
            {
                HBRUSH line = CreateSolidBrush(PROGRAM_COLOR_LINE);
                for (int sep : g_programTopSep)
                {
                    if (sep >= 0)
                    {
                        RECT mark = { sep, (height - 32) / 2, sep + 1, ((height - 32) / 2) + 32 };
                        FillRect(target, &mark, line);
                    }
                }
                DeleteObject(line);
            }
            else if (kind == 1)
            {
                SceneRound(target, &client, (float)PROGRAM_PANEL_RADIUS, LOOK_COLOR_SURFACE);
                PaintProgramSlides(target, &client);
            }
            else if (hwnd == g_programRight)
            {
                const int split = g_audioRailSplitY > 0 ? g_audioRailSplitY : height / 2;
                RECT tracks = { 0, 0, width, split - (PROGRAM_GAP / 2) };
                RECT playlists = { 0, split + (PROGRAM_GAP / 2), width, height };
                SceneRound(target, &tracks, (float)PROGRAM_PANEL_RADIUS, LOOK_COLOR_SURFACE);
                SceneRound(target, &playlists, (float)PROGRAM_PANEL_RADIUS, LOOK_COLOR_SURFACE);
                const int trackCount = g_audioTracks != nullptr
                    ? static_cast<int>(SendMessageA(g_audioTracks, LB_GETCOUNT, 0, 0))
                    : 0;
                PaintProgramPanelHead(target, &tracks, "TRACKS", trackCount);
                PaintProgramPanelHead(target, &playlists, "PLAYLISTS", -1);
            }
            else
            {
                const int dividerY = ProgramPreviewDividerY();
                RECT clientRc = { 0, 0, width, height };
                if (dividerY > 0 && dividerY < height)
                {
                    RECT media = { 0, 0, width, dividerY };
                    const int projectsBottom = ProgramProjectsBottom(&clientRc);
                    RECT projects = { 0, dividerY + PROGRAM_GAP, width, projectsBottom };
                    SceneRound(target, &media, (float)PROGRAM_PANEL_RADIUS, LOOK_COLOR_SURFACE);
                    if (projects.bottom > projects.top)
                    {
                        SceneRound(target, &projects, (float)PROGRAM_PANEL_RADIUS, LOOK_COLOR_SURFACE);
                    }
                }
                else
                {
                    SceneRound(target, &client, (float)PROGRAM_PANEL_RADIUS, LOOK_COLOR_SURFACE);
                }
                if (g_programVideoBtn != nullptr && g_programAudioBtn != nullptr && IsWindowVisible(g_programVideoBtn))
                {
                    RECT video = {};
                    RECT audio = {};
                    GetWindowRect(g_programVideoBtn, &video);
                    GetWindowRect(g_programAudioBtn, &audio);
                    RECT seg = { video.left, video.top, audio.right, audio.bottom };
                    MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&seg), 2);
                    InflateRect(&seg, 4, 4);
                    SceneRound(target, &seg, 6.0f, LOOK_COLOR_SUNKEN, RGB(0x33, 0x33, 0x33));
                }
                PaintProgramProjects(target, &client);
                PaintProgramLibrary(target, &client);
            }

            if (memory != nullptr)
            {
                BitBlt(hdc, 0, 0, width, height, memory, 0, 0, SRCCOPY);
                SelectObject(memory, oldBitmap);
                DeleteObject(bitmap);
                DeleteDC(memory);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

HWND CreateProgramSection(HWND parent, int kind)
{
    HWND section = CreateWindowExA(
        0,
        "ProgramSectionWindow",
        "",
        WS_CHILD | WS_CLIPCHILDREN,
        0,
        0,
        PROGRAM_SIDE_WIDTH,
        100,
        parent,
        nullptr,
        g_instance,
        nullptr
    );
    SetWindowLongPtrA(section, GWLP_USERDATA, kind);
    if (kind == 1)
    {
        const LONG style = GetWindowLongA(section, GWL_STYLE);
        SetWindowLongA(section, GWL_STYLE, style | WS_VSCROLL);
        SetWindowPos(
            section,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED
        );
        ShowScrollBar(section, SB_VERT, FALSE);
    }
    return section;
}

static void EnableDarkPopupMenus()
{
    static bool done = false;
    if (done)
    {
        return;
    }
    done = true;
    // Ordinal 135 is SetPreferredAppMode (2 = force dark) and 136 is
    // FlushMenuThemes. Neither is exported by name.
    HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (uxtheme == nullptr)
    {
        return;
    }
    typedef int (WINAPI *SetPreferredAppModeFn)(int);
    typedef void (WINAPI *FlushMenuThemesFn)();
    SetPreferredAppModeFn setMode = (SetPreferredAppModeFn)GetProcAddress(uxtheme, MAKEINTRESOURCEA(135));
    FlushMenuThemesFn flush = (FlushMenuThemesFn)GetProcAddress(uxtheme, MAKEINTRESOURCEA(136));
    if (setMode != nullptr)
    {
        setMode(2);
    }
    if (flush != nullptr)
    {
        flush();
    }
}

static int g_titleMenuHot = -1;
static int g_titleMenuOpen = -1;
// Set while a popup is showing to open a different menu once it closes.
static int g_titleMenuNext = -1;
static bool g_titleMenuKeys = false;
static HMENU g_titleMenuSelMenu = nullptr;
static bool g_titleMenuSelPopup = false;
static HHOOK g_titleMenuHook = nullptr;
static ULONGLONG g_titleMenuClosedTick = 0;
static int g_titleMenuClosedIndex = -1;
// Windows sends a mouse move when a popup opens; hovering only counts once the cursor leaves this point.
static POINT g_titleMenuMouse = {};
static int g_captionHot = HTNOWHERE;
static int g_captionDown = HTNOWHERE;
static char g_titleShown[MAX_PATH] = {};

static HFONT ProgramTitleFont(bool menu)
{
    static HFONT fonts[2] = {};
    const int which = menu ? 1 : 0;
    if (fonts[which] == nullptr)
    {
        fonts[which] = CreateFontA(menu ? -13 : -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
    return fonts[which] != nullptr ? fonts[which] : ProgramUiFont();
}

static HFONT ProgramCaptionGlyphFont()
{
    static HFONT font = nullptr;
    if (font == nullptr)
    {
        font = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    }
    return font;
}

static void ProgramTitleText(HWND hwnd, char* out, int outChars)
{
    out[0] = '\0';
    if (Document_HasProject())
    {
        Document_Name(out, outChars);
    }
    if (out[0] == '\0')
    {
        GetWindowTextA(hwnd, out, outChars);
    }
}

static int ProgramTitleMenuRects(HWND hwnd, RECT* rects, int max)
{
    if (g_programMenuBar == nullptr)
    {
        return 0;
    }
    int count = GetMenuItemCount(g_programMenuBar);
    if (count > max)
    {
        count = max;
    }
    HDC hdc = GetDC(hwnd);
    HFONT old = (HFONT)SelectObject(hdc, ProgramTitleFont(true));
    int x = 12 + 16 + 10;
    for (int i = 0; i < count; ++i)
    {
        wchar_t text[64] = {};
        GetMenuStringW(g_programMenuBar, i, text, 64, MF_BYPOSITION);
        RECT measure = {};
        DrawTextW(hdc, text, -1, &measure, DT_CALCRECT | DT_SINGLELINE);
        const int w = measure.right + 20;
        rects[i].left = x;
        rects[i].top = 5;
        rects[i].right = x + w;
        rects[i].bottom = PROGRAM_TITLE_H - 5;
        x += w + 2;
    }
    SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
    return count;
}

static int ProgramTitleMenuAt(HWND hwnd, POINT client)
{
    RECT rects[PROGRAM_TITLE_MENU_MAX] = {};
    const int count = ProgramTitleMenuRects(hwnd, rects, PROGRAM_TITLE_MENU_MAX);
    for (int i = 0; i < count; ++i)
    {
        if (PtInRect(&rects[i], client))
        {
            return i;
        }
    }
    return -1;
}

static void ProgramCaptionRect(HWND hwnd, int hit, RECT* out)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int slot = hit == HTCLOSE ? 1 : (hit == HTMAXBUTTON ? 2 : 3);
    out->left = client.right - slot * PROGRAM_CAPTION_W;
    out->top = 0;
    out->right = out->left + PROGRAM_CAPTION_W;
    out->bottom = PROGRAM_TITLE_H;
}

static int ProgramCaptionAt(HWND hwnd, POINT client)
{
    static const int hits[3] = { HTMINBUTTON, HTMAXBUTTON, HTCLOSE };
    for (int i = 0; i < 3; ++i)
    {
        RECT rc = {};
        ProgramCaptionRect(hwnd, hits[i], &rc);
        if (PtInRect(&rc, client))
        {
            return hits[i];
        }
    }
    return HTNOWHERE;
}

static void InvalidateProgramTitle(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    RECT strip = { 0, 0, client.right, PROGRAM_TITLE_H };
    InvalidateRect(hwnd, &strip, FALSE);
}

static void PaintProgramCaptionButton(HDC hdc, HWND hwnd, int hit)
{
    RECT rc = {};
    ProgramCaptionRect(hwnd, hit, &rc);
    const bool hot = g_captionHot == hit;
    const bool down = hot && g_captionDown == hit;
    if (hot)
    {
        COLORREF fill = down ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED;
        if (hit == HTCLOSE)
        {
            fill = down ? RGB(0xA3, 0x26, 0x1A) : RGB(0xC4, 0x2B, 0x1C);
        }
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
    }
    const wchar_t* glyph = L"\xE921";
    if (hit == HTMAXBUTTON)
    {
        glyph = IsZoomed(hwnd) ? L"\xE923" : L"\xE922";
    }
    else if (hit == HTCLOSE)
    {
        glyph = L"\xE8BB";
    }
    COLORREF ink = GetActiveWindow() == hwnd ? LOOK_COLOR_INK : LOOK_COLOR_INK_DIM;
    if (hot)
    {
        ink = hit == HTCLOSE ? RGB(0xFF, 0xFF, 0xFF) : LOOK_COLOR_INK_BRIGHT;
    }
    HFONT old = (HFONT)SelectObject(hdc, ProgramCaptionGlyphFont());
    SetTextColor(hdc, ink);
    DrawTextW(hdc, glyph, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static void PaintProgramTitle(HWND hwnd, HDC target)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int w = client.right;
    const int h = PROGRAM_TITLE_H;
    if (w <= 0)
    {
        return;
    }
    HDC hdc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, w, h);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(hdc, bitmap);
    RECT strip = { 0, 0, w, h };
    HBRUSH back = CreateSolidBrush(LOOK_COLOR_BASE);
    FillRect(hdc, &strip, back);
    DeleteObject(back);
    SetBkMode(hdc, TRANSPARENT);

    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::GraphicsPath mark;
        const float top = (float)((h - 16) / 2);
        mark.AddArc(12.0f, top, 8.0f, 8.0f, 180.0f, 90.0f);
        mark.AddArc(20.0f, top, 8.0f, 8.0f, 270.0f, 90.0f);
        mark.AddArc(20.0f, top + 8.0f, 8.0f, 8.0f, 0.0f, 90.0f);
        mark.AddArc(12.0f, top + 8.0f, 8.0f, 8.0f, 90.0f, 90.0f);
        mark.CloseFigure();
        Gdiplus::LinearGradientBrush fill(
            Gdiplus::PointF(12.0f, top),
            Gdiplus::PointF(28.0f, top + 16.0f),
            Gdiplus::Color(255, 0x5A, 0x5A, 0x5A),
            Gdiplus::Color(255, 0x3A, 0x3A, 0x3A)
        );
        g.FillPath(&fill, &mark);
    }

    RECT rects[PROGRAM_TITLE_MENU_MAX] = {};
    const int count = ProgramTitleMenuRects(hwnd, rects, PROGRAM_TITLE_MENU_MAX);
    int menuRight = 38;
    HFONT old = (HFONT)SelectObject(hdc, ProgramTitleFont(true));
    for (int i = 0; i < count; ++i)
    {
        const bool on = i == g_titleMenuOpen || (g_titleMenuOpen < 0 && i == g_titleMenuHot);
        if (on)
        {
            SceneRound(hdc, &rects[i], 5.0f, LOOK_COLOR_RAISED);
        }
        wchar_t text[64] = {};
        GetMenuStringW(g_programMenuBar, i, text, 64, MF_BYPOSITION);
        SetTextColor(hdc, on ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK);
        DrawTextW(hdc, text, -1, &rects[i],
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | (g_titleMenuKeys ? 0 : DT_HIDEPREFIX));
        menuRight = rects[i].right;
    }

    char title[MAX_PATH] = {};
    ProgramTitleText(hwnd, title, MAX_PATH);
    lstrcpynA(g_titleShown, title, MAX_PATH);
    SelectObject(hdc, ProgramTitleFont(false));
    SIZE size = {};
    GetTextExtentPoint32A(hdc, title, lstrlenA(title), &size);
    const int limitLeft = menuRight + 16;
    const int limitRight = w - PROGRAM_CAPTION_W * 3 - 16;
    int left = (w - size.cx) / 2;
    if (left < limitLeft)
    {
        left = limitLeft;
    }
    RECT titleRc = { left, 0, left + size.cx, h };
    if (titleRc.right > limitRight)
    {
        titleRc.right = limitRight;
    }
    if (titleRc.right > titleRc.left)
    {
        SetTextColor(hdc, LOOK_COLOR_INK_DIM);
        DrawTextA(hdc, title, -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    SelectObject(hdc, old);

    PaintProgramCaptionButton(hdc, hwnd, HTMINBUTTON);
    PaintProgramCaptionButton(hdc, hwnd, HTMAXBUTTON);
    PaintProgramCaptionButton(hdc, hwnd, HTCLOSE);

    BitBlt(target, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
}

static LRESULT CALLBACK ProgramTitleMenuHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == MSGF_MENU && g_programWindow != nullptr && g_programMenuBar != nullptr)
    {
        MSG* msg = reinterpret_cast<MSG*>(lParam);
        if (msg->message == WM_MOUSEMOVE || msg->message == WM_LBUTTONDOWN)
        {
            POINT pt = msg->pt;
            ScreenToClient(g_programWindow, &pt);
            const int over = pt.y >= 0 && pt.y < PROGRAM_TITLE_H ? ProgramTitleMenuAt(g_programWindow, pt) : -1;
            const bool moved = msg->pt.x != g_titleMenuMouse.x || msg->pt.y != g_titleMenuMouse.y;
            if (msg->message == WM_MOUSEMOVE && moved && over >= 0 && over != g_titleMenuOpen)
            {
                g_titleMenuNext = over;
                g_titleMenuKeys = false;
                EndMenu();
                return TRUE;
            }
            if (msg->message == WM_LBUTTONDOWN && over >= 0 && over == g_titleMenuOpen)
            {
                g_titleMenuNext = -1;
                EndMenu();
                return TRUE;
            }
        }
        else if (msg->message == WM_KEYDOWN && (msg->wParam == VK_LEFT || msg->wParam == VK_RIGHT))
        {
            const int count = GetMenuItemCount(g_programMenuBar);
            const HMENU top = GetSubMenu(g_programMenuBar, g_titleMenuOpen);
            const bool inTop = g_titleMenuSelMenu == nullptr || g_titleMenuSelMenu == top;
            if (count > 1 && msg->wParam == VK_LEFT && inTop)
            {
                g_titleMenuNext = (g_titleMenuOpen + count - 1) % count;
                g_titleMenuKeys = true;
                EndMenu();
                return TRUE;
            }
            if (count > 1 && msg->wParam == VK_RIGHT && !g_titleMenuSelPopup)
            {
                g_titleMenuNext = (g_titleMenuOpen + 1) % count;
                g_titleMenuKeys = true;
                EndMenu();
                return TRUE;
            }
        }
    }
    return CallNextHookEx(g_titleMenuHook, code, wParam, lParam);
}

static void OpenProgramTitleMenu(HWND hwnd, int index, bool keys)
{
    if (g_programMenuBar == nullptr || g_titleMenuOpen >= 0)
    {
        return;
    }
    g_titleMenuKeys = keys;
    g_titleMenuNext = index;
    while (g_titleMenuNext >= 0)
    {
        const int i = g_titleMenuNext;
        g_titleMenuNext = -1;
        RECT rects[PROGRAM_TITLE_MENU_MAX] = {};
        const int count = ProgramTitleMenuRects(hwnd, rects, PROGRAM_TITLE_MENU_MAX);
        HMENU popup = i < count ? GetSubMenu(g_programMenuBar, i) : nullptr;
        if (popup == nullptr)
        {
            break;
        }
        g_titleMenuOpen = i;
        g_titleMenuSelMenu = nullptr;
        g_titleMenuSelPopup = false;
        InvalidateProgramTitle(hwnd);
        UpdateWindow(hwnd);

        RECT item = rects[i];
        MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&item), 2);
        TPMPARAMS params = {};
        params.cbSize = sizeof(params);
        params.rcExclude = item;
        GetCursorPos(&g_titleMenuMouse);
        g_titleMenuHook = SetWindowsHookExA(WH_MSGFILTER, ProgramTitleMenuHook, nullptr, GetCurrentThreadId());
        if (g_titleMenuKeys)
        {
            PostMessageA(hwnd, WM_KEYDOWN, VK_DOWN, 0);
        }
        TrackPopupMenuEx(popup, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
                         item.left, item.bottom + 2, hwnd, &params);
        if (g_titleMenuHook != nullptr)
        {
            UnhookWindowsHookEx(g_titleMenuHook);
            g_titleMenuHook = nullptr;
        }
        g_titleMenuClosedIndex = i;
        g_titleMenuClosedTick = GetTickCount64();
    }
    g_titleMenuOpen = -1;
    g_titleMenuKeys = false;
    POINT cursor = {};
    GetCursorPos(&cursor);
    ScreenToClient(hwnd, &cursor);
    g_titleMenuHot = cursor.y >= 0 && cursor.y < PROGRAM_TITLE_H ? ProgramTitleMenuAt(hwnd, cursor) : -1;
    InvalidateProgramTitle(hwnd);
}

static int ProgramTitleMnemonic(wchar_t key)
{
    if (g_programMenuBar == nullptr)
    {
        return -1;
    }
    const wchar_t wanted = (wchar_t)(UINT_PTR)CharUpperW((LPWSTR)(UINT_PTR)key);
    const int count = GetMenuItemCount(g_programMenuBar);
    for (int i = 0; i < count; ++i)
    {
        wchar_t text[64] = {};
        GetMenuStringW(g_programMenuBar, i, text, 64, MF_BYPOSITION);
        const wchar_t* amp = wcschr(text, L'&');
        if (amp != nullptr && amp[1] != L'\0' &&
            (wchar_t)(UINT_PTR)CharUpperW((LPWSTR)(UINT_PTR)amp[1]) == wanted)
        {
            return i;
        }
    }
    return -1;
}

static void SetProgramCaptionHot(HWND hwnd, int hit)
{
    if (g_captionHot != hit)
    {
        g_captionHot = hit;
        InvalidateProgramTitle(hwnd);
    }
}

LRESULT CALLBACK ProgramWindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
        case WM_NCCALCSIZE:
            if (wParam)
            {
                // Keep the side and bottom resize borders but give the caption to the client area.
                NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                const int top = params->rgrc[0].top;
                const LRESULT result = DefWindowProcA(hwnd, uMsg, wParam, lParam);
                params->rgrc[0].top = top;
                if (IsZoomed(hwnd))
                {
                    params->rgrc[0].top += GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                }
                return result;
            }
            break;

        case WM_NCHITTEST:
        {
            const LRESULT hit = DefWindowProcA(hwnd, uMsg, wParam, lParam);
            if (hit != HTCLIENT)
            {
                return hit;
            }
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            if (pt.y >= PROGRAM_TITLE_H)
            {
                return HTCLIENT;
            }
            if (!IsZoomed(hwnd))
            {
                const int edge = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                if (pt.y < edge)
                {
                    RECT client = {};
                    GetClientRect(hwnd, &client);
                    if (pt.x < edge * 2)
                    {
                        return HTTOPLEFT;
                    }
                    if (pt.x >= client.right - edge * 2)
                    {
                        return HTTOPRIGHT;
                    }
                    return HTTOP;
                }
            }
            const int caption = ProgramCaptionAt(hwnd, pt);
            if (caption != HTNOWHERE)
            {
                return caption;
            }
            if (ProgramTitleMenuAt(hwnd, pt) >= 0)
            {
                return HTCLIENT;
            }
            return HTCAPTION;
        }

        // Windows would draw classic caption buttons for these hit codes, so they are handled here.
        case WM_NCMOUSEMOVE:
            if (wParam == HTMINBUTTON || wParam == HTMAXBUTTON || wParam == HTCLOSE)
            {
                SetProgramCaptionHot(hwnd, (int)wParam);
                TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE | TME_NONCLIENT, hwnd, 0 };
                TrackMouseEvent(&track);
                return 0;
            }
            SetProgramCaptionHot(hwnd, HTNOWHERE);
            if (g_titleMenuHot >= 0)
            {
                g_titleMenuHot = -1;
                InvalidateProgramTitle(hwnd);
            }
            break;

        case WM_NCMOUSELEAVE:
            SetProgramCaptionHot(hwnd, HTNOWHERE);
            g_captionDown = HTNOWHERE;
            break;

        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
            if (wParam == HTMINBUTTON || wParam == HTMAXBUTTON || wParam == HTCLOSE)
            {
                g_captionDown = (int)wParam;
                SetProgramCaptionHot(hwnd, (int)wParam);
                InvalidateProgramTitle(hwnd);
                return 0;
            }
            break;

        case WM_NCLBUTTONUP:
            if (wParam == HTMINBUTTON || wParam == HTMAXBUTTON || wParam == HTCLOSE)
            {
                const int down = g_captionDown;
                g_captionDown = HTNOWHERE;
                InvalidateProgramTitle(hwnd);
                if (down == (int)wParam)
                {
                    WPARAM command = SC_CLOSE;
                    if (down == HTMINBUTTON)
                    {
                        command = SC_MINIMIZE;
                    }
                    else if (down == HTMAXBUTTON)
                    {
                        command = IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE;
                    }
                    SetProgramCaptionHot(hwnd, HTNOWHERE);
                    PostMessageA(hwnd, WM_SYSCOMMAND, command, 0);
                }
                return 0;
            }
            g_captionDown = HTNOWHERE;
            break;

        case WM_MOUSEMOVE:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int hot = pt.y < PROGRAM_TITLE_H ? ProgramTitleMenuAt(hwnd, pt) : -1;
            SetProgramCaptionHot(hwnd, HTNOWHERE);
            if (hot != g_titleMenuHot)
            {
                g_titleMenuHot = hot;
                InvalidateProgramTitle(hwnd);
            }
            if (hot >= 0)
            {
                TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&track);
            }
            break;
        }

        case WM_MOUSELEAVE:
            if (g_titleMenuHot >= 0 && g_titleMenuOpen < 0)
            {
                g_titleMenuHot = -1;
                InvalidateProgramTitle(hwnd);
            }
            return 0;

        case WM_LBUTTONDOWN:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int index = pt.y < PROGRAM_TITLE_H ? ProgramTitleMenuAt(hwnd, pt) : -1;
            if (index >= 0)
            {
                const bool justClosed = index == g_titleMenuClosedIndex &&
                    GetTickCount64() - g_titleMenuClosedTick < 250;
                if (!justClosed)
                {
                    OpenProgramTitleMenu(hwnd, index, false);
                }
                return 0;
            }
            break;
        }

        case WM_MENUSELECT:
            g_titleMenuSelMenu = reinterpret_cast<HMENU>(lParam);
            g_titleMenuSelPopup = HIWORD(wParam) != 0xFFFF && (HIWORD(wParam) & MF_POPUP) != 0;
            break;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU && g_programMenuBar != nullptr && lParam != ' ')
            {
                const int index = lParam == 0 ? 0 : ProgramTitleMnemonic((wchar_t)lParam);
                if (index >= 0)
                {
                    OpenProgramTitleMenu(hwnd, index, true);
                }
                else
                {
                    MessageBeep(0);
                }
                return 0;
            }
            break;

        case WM_ACTIVATE:
            InvalidateProgramTitle(hwnd);
            break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            if (ps.rcPaint.top < PROGRAM_TITLE_H)
            {
                PaintProgramTitle(hwnd, hdc);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
            InvalidateProgramTitle(hwnd);
            LayoutProgramSections(hwnd);
            return 0;

        case WM_KEYDOWN:
            if (wParam == 'T' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000))
            {
                StartTransitionStress();
                return 0;
            }
            break;

        case WM_MOUSEWHEEL:
            if (g_programView == PROGRAM_VIEW_SHOW)
            {
                POINT screen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (g_programLeft != nullptr && g_leftLibTab == LEFT_LIB_MEDIA)
                {
                    POINT leftPt = screen;
                    ScreenToClient(g_programLeft, &leftPt);
                    RECT leftClient = {};
                    GetClientRect(g_programLeft, &leftClient);
                    if (PtInRect(&leftClient, leftPt) &&
                        (g_leftLibMediaBody.right > g_leftLibMediaBody.left
                             ? PtInRect(&g_leftLibMediaBody, leftPt)
                             : true) &&
                        ScrollLeftLibMedia(g_programLeft, GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA))
                    {
                        return 0;
                    }
                }
                if (g_programLeft != nullptr && g_leftLibTab == LEFT_LIB_TRANSITIONS)
                {
                    POINT leftPt = screen;
                    ScreenToClient(g_programLeft, &leftPt);
                    RECT leftClient = {};
                    GetClientRect(g_programLeft, &leftClient);
                    if (PtInRect(&leftClient, leftPt))
                    {
                        SendMessageA(g_programLeft, WM_MOUSEWHEEL, wParam, lParam);
                        return 0;
                    }
                }
                if (g_programMiddle != nullptr)
                {
                    POINT midPt = screen;
                    ScreenToClient(g_programMiddle, &midPt);
                    RECT client = {};
                    GetClientRect(g_programMiddle, &client);
                    if (PtInRect(&client, midPt))
                    {
                        const int delta = -GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 48;
                        ScrollProgramMiddle(g_programMiddle, delta);
                        return 0;
                    }
                }
            }
            break;

        case WM_SHOWWINDOW:
            if (wParam)
            {
                LayoutProgramSections(hwnd);
            }
            return 0;

        case WM_COMMAND:
            if (HandleContextMenuCommand(LOWORD(wParam)))
            {
                return 0;
            }
            if (HandleAppMenuCommand(hwnd, LOWORD(wParam)))
            {
                return 0;
            }
            if (LOWORD(wParam) == IDM_FILE_EXIT)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            if (HandleScreensCommand(hwnd, LOWORD(wParam)))
            {
                return 0;
            }
            if (LOWORD(wParam) == ID_SCREEN_MIRROR && HIWORD(wParam) == CBN_SELCHANGE)
            {
                ApplyPreviewComboChange();
                return 0;
            }
            break;

        case WM_IMPORT_READY:
            ImportOnReady(static_cast<int>(wParam), lParam != 0);
            return 0;

        case WM_TIMER:
            if (wParam == ID_MEDIA_TIMER)
            {
                ImportUiTick();
                if (g_mediaPanel != MEDIA_PANEL_NONE)
                {
                    UpdateMediaTransportUi();
                }
                char title[MAX_PATH] = {};
                ProgramTitleText(hwnd, title, MAX_PATH);
                if (lstrcmpA(title, g_titleShown) != 0)
                {
                    InvalidateProgramTitle(hwnd);
                }
                Audio_TickAdvance();
                static int shownPlayingTrack = 0;
                const int playingTrack = Audio_PlayingTrackId();
                if (playingTrack != shownPlayingTrack)
                {
                    shownPlayingTrack = playingTrack;
                    if (g_audioTracks != nullptr)
                    {
                        InvalidateRect(g_audioTracks, nullptr, FALSE);
                    }
                }
                static int preloadTicks = 0;
                if (++preloadTicks >= 10)
                {
                    preloadTicks = 0;
                    SyncVideoPreload();
                }
                return 0;
            }
            if (wParam == ID_XFADE_TIMER)
            {
                TickSlideXfade();
                TickBackgroundXfade();
                if (!SlideXfadeActive() && !g_bgXfadeOn && g_programWindow != nullptr)
                {
                    KillTimer(hwnd, ID_XFADE_TIMER);
                }
                return 0;
            }
            if (wParam == ID_TRANS_STRESS_TIMER)
            {
                TickTransitionStress();
                return 0;
            }
            if (wParam == ID_LIVE_SAVE_TIMER)
            {
                KillTimer(hwnd, ID_LIVE_SAVE_TIMER);
                Document_Save();
                return 0;
            }
            break;

        case WM_DROPFILES:
        {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < count; ++i)
            {
                wchar_t path[MAX_PATH] = {};
                if (DragQueryFileW(drop, i, path, MAX_PATH) > 0)
                {
                    ImportDroppedMedia(hwnd, path);
                }
            }
            DragFinish(drop);
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            g_importButton = nullptr;
            if (g_importWindow != nullptr)
            {
                HWND importWindow = g_importWindow;
                g_importWindow = nullptr;
                DestroyWindow(importWindow);
            }
            ImportShutdown();
            KillTimer(hwnd, ID_MEDIA_TIMER);
            KillTimer(hwnd, ID_XFADE_TIMER);
            KillTimer(hwnd, ID_TRANS_STRESS_TIMER);
            KillTimer(hwnd, ID_LIVE_SAVE_TIMER);
            CloseTransitionGenie();
            CloseTemplatePanel();
            FreeTemplateThumbs();
            StopSlideXfade();
            Document_Save();
            SetThreadExecutionState(ES_CONTINUOUS);
            g_programWindow = nullptr;
            if (g_programMenuBar != nullptr)
            {
                DestroyMenu(g_programMenuBar);
                g_programMenuBar = nullptr;
            }
            g_titleMenuHot = -1;
            g_captionHot = HTNOWHERE;
            g_captionDown = HTNOWHERE;
            g_programTop = nullptr;
            g_programLeft = nullptr;
            g_programPreview = nullptr;
            g_programMirror = nullptr;
            g_programVideoBtn = nullptr;
            g_programAudioBtn = nullptr;
            g_programClearSlideBtn = nullptr;
            g_programClearBgBtn = nullptr;
            g_programClearAudioBtn = nullptr;
            g_programQuickSlidesBtn = nullptr;
            g_programTemplatesBtn = nullptr;
            g_programScenesBtn = nullptr;
            g_programDisplaysBtn = nullptr;
            if (g_quickSlidesWindow != nullptr)
            {
                DestroyWindow(g_quickSlidesWindow);
                g_quickSlidesWindow = nullptr;
                g_quickSlidesEdit = nullptr;
            }
            g_programVideoPlay = nullptr;
            g_programVideoScrub = nullptr;
            g_programVideoTime = nullptr;
            g_programAudioPlay = nullptr;
            g_programAudioScrub = nullptr;
            g_programAudioLabel = nullptr;
            g_programVideoBack = nullptr;
            g_programVideoFwd = nullptr;
            g_programVideoDuration = nullptr;
            g_programAudioBack = nullptr;
            g_programAudioFwd = nullptr;
            g_programAudioDuration = nullptr;
            g_programMiddle = nullptr;
            g_programRight = nullptr;
            g_audioTracks = nullptr;
            g_audioPlaylists = nullptr;
            g_audioNewPlaylist = nullptr;
            g_audioLoopBtn = nullptr;
            g_audioShuffleBtn = nullptr;
            EditView_Destroy();
            ReleaseLiveBackground();
            g_backgroundActive = false;
            g_programShowButton = nullptr;
            g_programEditButton = nullptr;
            g_importButton = nullptr;
            g_importWindow = nullptr;
            g_programModeHover = nullptr;
            g_programView = PROGRAM_VIEW_SHOW;
            g_mediaPanel = MEDIA_PANEL_VIDEO;
            g_programSlideScroll = 0;
            g_programSlideHeld = -1;
            g_programSlideDrop = -1;
            g_programSlideDragging = false;
            g_programProjectHeld = -1;
            g_programProjectDrop = -1;
            g_programProjectDragging = false;
            if (g_mainWindow != nullptr)
            {
                DestroyWindow(g_mainWindow);
            }
            return 0;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void ShowProgramView()
{
    if (g_programWindow != nullptr)
    {
        LoadProgramProjects();
        RefreshLeftLib();
        if (g_programLeft != nullptr)
        {
            InvalidateRect(g_programLeft, nullptr, FALSE);
        }
        ShowWindow(g_programWindow, SW_SHOW);
        SetForegroundWindow(g_programWindow);
        ManageScreens_Restore(g_programWindow, g_instance);
        RefreshProgramScreenMirror();
        PresentCurrent();
        return;
    }

    EnsureProgramFont();
    EnableDarkPopupMenus();
    g_programWindow = CreateWindowExA(
        WS_EX_APPWINDOW,
        "ProgramWindow",
        "Canvas You",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        820,
        nullptr,
        nullptr,
        g_instance,
        nullptr
    );

    if (g_programWindow == nullptr)
    {
        return;
    }
    ApplySceneTitleBar(g_programWindow);

    DragAcceptFiles(g_programWindow, TRUE);
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    SetTimer(g_programWindow, ID_MEDIA_TIMER, 100, nullptr);

    g_programMenuBar = CreateProgramMenu();
    SetWindowPos(g_programWindow, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    UpdateDocumentMenus();
    g_programTop = CreateProgramSection(g_programWindow, 2);
    g_programShowButton = CreateProgramModeButton(g_programTop, PROGRAM_VIEW_SHOW);
    g_programEditButton = CreateProgramModeButton(g_programTop, PROGRAM_VIEW_EDIT);
    g_importButton = CreateWindowExA(
        0,
        "ProgramImportButton",
        "",
        WS_CHILD,
        0,
        0,
        56,
        56,
        g_programTop,
        nullptr,
        g_instance,
        nullptr
    );
    g_importButtonShown = 0;
    g_programLeft = CreateProgramSection(g_programWindow, 0);
    g_programMiddle = CreateProgramSection(g_programWindow, 1);
    SetWindowTheme(g_programMiddle, L"DarkMode_Explorer", nullptr);
    g_programRight = CreateProgramSection(g_programWindow, 0);
    CreateAudioRail(g_programRight);
    EditView_Create(g_programWindow, g_instance);
    DragAcceptFiles(g_programLeft, TRUE);
    DragAcceptFiles(g_programMiddle, TRUE);
    g_programPreview = CreateWindowExA(
        0,
        "HostPreviewWindow",
        "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        PROGRAM_PREVIEW_PAD,
        PROGRAM_PREVIEW_PAD,
        160,
        90,
        g_programLeft,
        nullptr,
        g_instance,
        nullptr
    );
    g_programMirror = CreateWindowExA(
        0,
        "COMBOBOX",
        "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_HASSTRINGS | CBS_OWNERDRAWFIXED,
        PROGRAM_PREVIEW_PAD,
        PROGRAM_PREVIEW_PAD + 96,
        160,
        200,
        g_programLeft,
        (HMENU)ID_SCREEN_MIRROR,
        g_instance,
        nullptr
    );
    if (g_programMirror != nullptr && g_programFont != nullptr)
    {
        SendMessageA(g_programMirror, WM_SETFONT, (WPARAM)g_programFont, TRUE);
    }
    if (g_programMirror != nullptr)
    {
        g_programMirrorProc = (WNDPROC)SetWindowLongPtrA(g_programMirror, GWLP_WNDPROC, (LONG_PTR)ProgramMirrorProc);
        COMBOBOXINFO info = {};
        info.cbSize = sizeof(info);
        if (GetComboBoxInfo(g_programMirror, &info) && info.hwndList != nullptr)
        {
            SetWindowTheme(info.hwndList, L"DarkMode_Explorer", nullptr);
        }
    }
    g_programVideoBtn = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, PROGRAM_MEDIA_BTN, PROGRAM_MEDIA_BTN,
        g_programLeft, (HMENU)ID_MEDIA_VIDEO_BTN, g_instance, nullptr
    );
    g_programAudioBtn = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, PROGRAM_MEDIA_BTN, PROGRAM_MEDIA_BTN,
        g_programLeft, (HMENU)ID_MEDIA_AUDIO_BTN, g_instance, nullptr
    );
    g_programClearSlideBtn = CreateWindowExA(
        0, "MediaChromeButton", "Clear slide",
        WS_CHILD | WS_VISIBLE,
        0, 0, PROGRAM_TILE_W, PROGRAM_TILE_H,
        g_programTop, (HMENU)ID_MEDIA_CLEAR_SLIDE, g_instance, nullptr
    );
    g_programClearBgBtn = CreateWindowExA(
        0, "MediaChromeButton", "Clear bg",
        WS_CHILD | WS_VISIBLE,
        0, 0, PROGRAM_TILE_W, PROGRAM_TILE_H,
        g_programTop, (HMENU)ID_MEDIA_CLEAR_BG, g_instance, nullptr
    );
    g_programClearAudioBtn = CreateWindowExA(
        0, "MediaChromeButton", "Clear audio",
        WS_CHILD | WS_VISIBLE,
        0, 0, PROGRAM_TILE_W, PROGRAM_TILE_H,
        g_programTop, (HMENU)ID_AUDIO_CLEAR, g_instance, nullptr
    );
    g_programQuickSlidesBtn = CreateWindowExA(
        0, "MediaChromeButton", "Quick Slides",
        WS_CHILD,
        0, 0, PROGRAM_TILE_W, PROGRAM_TILE_H,
        g_programTop, (HMENU)ID_QUICK_SLIDES, g_instance, nullptr
    );
    g_programTemplatesBtn = CreateWindowExA(
        0, "MediaChromeButton", "Templates",
        WS_CHILD | WS_VISIBLE,
        0, 0, PROGRAM_TILE_W, PROGRAM_TILE_H,
        g_programTop, (HMENU)ID_TEMPLATES_BTN, g_instance, nullptr
    );
    g_programScenesBtn = CreateWindowExA(
        0, "MediaChromeButton", "Scenes",
        WS_CHILD | WS_VISIBLE,
        0, 0, PROGRAM_TILE_W, PROGRAM_TILE_H,
        g_programTop, (HMENU)ID_SCENES_BTN, g_instance, nullptr
    );
    g_programDisplaysBtn = CreateWindowExA(
        0, "MediaChromeButton", "Displays",
        WS_CHILD | WS_VISIBLE,
        0, 0, PROGRAM_TILE_W, PROGRAM_TILE_H,
        g_programTop, (HMENU)ID_DISPLAYS_BTN, g_instance, nullptr
    );
    g_programVideoPlay = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD,
        0, 0, 28, PROGRAM_TRANSPORT_H,
        g_programLeft, (HMENU)ID_MEDIA_VIDEO_PLAY, g_instance, nullptr
    );
    g_programVideoScrub = CreateWindowExA(
        0, "MediaScrubBar", "",
        WS_CHILD,
        0, 0, 100, PROGRAM_SCRUB_H,
        g_programLeft, (HMENU)ID_MEDIA_VIDEO_SCRUB, g_instance, nullptr
    );
    g_programVideoTime = CreateWindowExA(
        0, "STATIC", "0:00",
        WS_CHILD | SS_LEFT | SS_CENTERIMAGE,
        0, 0, 78, PROGRAM_TRANSPORT_H,
        g_programLeft, nullptr, g_instance, nullptr
    );
    g_programVideoDuration = CreateWindowExA(
        0, "STATIC", "0:00",
        WS_CHILD | SS_RIGHT | SS_CENTERIMAGE,
        0, 0, 78, PROGRAM_TRANSPORT_H,
        g_programLeft, nullptr, g_instance, nullptr
    );
    g_programVideoBack = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD,
        0, 0, 28, 28,
        g_programLeft, (HMENU)ID_MEDIA_VIDEO_BACK, g_instance, nullptr
    );
    g_programVideoFwd = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD,
        0, 0, 28, 28,
        g_programLeft, (HMENU)ID_MEDIA_VIDEO_FWD, g_instance, nullptr
    );
    g_programAudioPlay = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD,
        0, 0, 28, PROGRAM_TRANSPORT_H,
        g_programLeft, (HMENU)ID_MEDIA_AUDIO_PLAY, g_instance, nullptr
    );
    g_programAudioScrub = CreateWindowExA(
        0, "MediaScrubBar", "",
        WS_CHILD,
        0, 0, 100, PROGRAM_SCRUB_H,
        g_programLeft, (HMENU)ID_MEDIA_AUDIO_SCRUB, g_instance, nullptr
    );
    g_programAudioLabel = CreateWindowExA(
        0, "STATIC", "0:00",
        WS_CHILD | SS_LEFT | SS_CENTERIMAGE,
        0, 0, 78, PROGRAM_TRANSPORT_H,
        g_programLeft, nullptr, g_instance, nullptr
    );
    g_programAudioDuration = CreateWindowExA(
        0, "STATIC", "0:00",
        WS_CHILD | SS_RIGHT | SS_CENTERIMAGE,
        0, 0, 78, PROGRAM_TRANSPORT_H,
        g_programLeft, nullptr, g_instance, nullptr
    );
    g_programAudioBack = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD,
        0, 0, 28, 28,
        g_programLeft, (HMENU)ID_MEDIA_AUDIO_BACK, g_instance, nullptr
    );
    g_programAudioFwd = CreateWindowExA(
        0, "MediaChromeButton", "",
        WS_CHILD,
        0, 0, 28, 28,
        g_programLeft, (HMENU)ID_MEDIA_AUDIO_FWD, g_instance, nullptr
    );
    if (g_programFont != nullptr)
    {
        SendMessageA(g_programVideoTime, WM_SETFONT, (WPARAM)g_programFont, TRUE);
        SendMessageA(g_programVideoDuration, WM_SETFONT, (WPARAM)g_programFont, TRUE);
        SendMessageA(g_programAudioLabel, WM_SETFONT, (WPARAM)g_programFont, TRUE);
        SendMessageA(g_programAudioDuration, WM_SETFONT, (WPARAM)g_programFont, TRUE);
    }
    LayoutProgramSections(g_programWindow);
    LoadProgramProjects();
    RefreshLeftLib();
    RefreshProgramScreenMirror();
    ShowWindow(g_programTop, SW_SHOW);
    ShowWindow(g_programShowButton, SW_SHOW);
    ShowWindow(g_programEditButton, SW_SHOW);
    ShowWindow(g_programLeft, SW_SHOW);
    ShowWindow(g_programPreview, SW_SHOW);
    ShowWindow(g_programMirror, SW_SHOW);
    ShowWindow(g_programVideoBtn, SW_SHOW);
    ShowWindow(g_programAudioBtn, SW_SHOW);
    ShowWindow(g_programClearSlideBtn, SW_SHOW);
    ShowWindow(g_programClearBgBtn, SW_SHOW);
    ShowWindow(g_programClearAudioBtn, SW_SHOW);
    ShowWindow(g_programMiddle, SW_SHOW);
    ShowWindow(g_programRight, SW_SHOW);
    EditView_Show(false);
    ShowWindow(g_programWindow, SW_SHOW);
    LayoutProgramSections(g_programWindow);
    UpdateWindow(g_programWindow);
    ManageScreens_Restore(g_programWindow, g_instance);
    RefreshProgramScreenMirror();
    PresentCurrent();
}

LRESULT CALLBACK HostPreviewProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            // Passive surface: LiveOutput_Push owns content. Paint only restores
            // black / GDI fallback using the same engine flags as fullscreen.
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            if (App_OutputCleared() || App_LiveHold())
            {
            HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &client, fill);
            DeleteObject(fill);
            }
            else if (!GpuOwnsOutputs() && App_Ready())
            {
                LiveOutput_PaintHwnd(hwnd);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}


static bool g_looksLoaded = false;
static void EnsureLookState();

static void LookStoreBegin()
{
    if (Store_Ready())
    {
        sqlite3_exec(Store_Db(), "BEGIN;", nullptr, nullptr, nullptr);
    }
}

static void LookStoreCommit()
{
    if (Store_Ready())
    {
        sqlite3_exec(Store_Db(), "COMMIT;", nullptr, nullptr, nullptr);
    }
}

static void ResetLookScene(int look)
{
    if (look < 0 || look >= LOOK_MAX)
    {
        return;
    }
    for (int screen = 0; screen < LOOK_MAX_SCREENS; ++screen)
    {
        g_lookBgMode[look][screen] = 0;
        g_lookSlideMode[look][screen] = 1;
    }
    for (int display = 0; display < LOOK_MAX_DISPLAYS; ++display)
    {
        g_lookBgOn[look][display] = true;
        g_lookSlideOn[look][display] = true;
    }
}

static void SaveLookMeta()
{
    if (!Store_Ready())
    {
        return;
    }
    Store_SetInt("looks.count", g_lookCount);
    Store_SetInt("looks.view", g_lookIndex);
    Store_SetInt("looks.applied", g_appliedLook);
    for (int look = 0; look < g_lookCount; ++look)
    {
        char key[32] = {};
        wsprintfA(key, "looks.%d.name", look);
        Store_SetString(key, g_lookNames[look]);
    }
}

static void SaveLookScene(int look)
{
    if (!Store_Ready() || look < 0 || look >= g_lookCount)
    {
        return;
    }
    for (int screen = 0; screen < LOOK_MAX_SCREENS; ++screen)
    {
        char key[64] = {};
        wsprintfA(key, "looks.%d.bgMode.%d", look, screen);
        Store_SetInt(key, g_lookBgMode[look][screen]);
        wsprintfA(key, "looks.%d.slMode.%d", look, screen);
        Store_SetInt(key, g_lookSlideMode[look][screen]);
    }
    for (int display = 0; display < LOOK_MAX_DISPLAYS; ++display)
    {
        char key[64] = {};
        wsprintfA(key, "looks.%d.bgOn.%d", look, display);
        Store_SetInt(key, g_lookBgOn[look][display] ? 1 : 0);
        wsprintfA(key, "looks.%d.slOn.%d", look, display);
        Store_SetInt(key, g_lookSlideOn[look][display] ? 1 : 0);
    }
}

static void SaveLookState()
{
    if (!Store_Ready())
    {
        return;
    }
    LookStoreBegin();
    SaveLookMeta();
    for (int look = 0; look < g_lookCount; ++look)
    {
        SaveLookScene(look);
    }
    LookStoreCommit();
}

static void SaveLookEdit()
{
    if (!Store_Ready())
    {
        return;
    }
    LookStoreBegin();
    SaveLookMeta();
    SaveLookScene(g_lookIndex);
    LookStoreCommit();
}

static void LoadLookState()
{
    EnsureLookState();
    if (g_looksLoaded || !Store_Ready())
    {
        return;
    }

    Renderer_LoadTransitionDefaults();
    Renderer_NormalizeInheritedTransitions();
    g_lookCount = Store_GetInt("looks.count", 0);
    if (g_lookCount < 0)
    {
        g_lookCount = 0;
    }
    if (g_lookCount > LOOK_MAX)
    {
        g_lookCount = LOOK_MAX;
    }
    g_lookIndex = Store_GetInt("looks.view", 0);
    g_appliedLook = Store_GetInt("looks.applied", g_lookIndex);
    if (g_lookIndex < 0 || g_lookIndex >= g_lookCount)
    {
        g_lookIndex = 0;
    }
    if (g_appliedLook < 0 || g_appliedLook >= g_lookCount)
    {
        g_appliedLook = g_lookIndex;
    }
    for (int look = 0; look < g_lookCount; ++look)
    {
        char key[64] = {};
        char fallback[LOOK_NAME_MAX] = {};
        wsprintfA(key, "looks.%d.name", look);
        wsprintfA(fallback, "Scene %d", look + 1);
        Store_GetString(key, g_lookNames[look], LOOK_NAME_MAX, fallback);
        if (g_lookNames[look][0] == '\0')
        {
            lstrcpynA(g_lookNames[look], fallback, LOOK_NAME_MAX);
        }
        for (int screen = 0; screen < LOOK_MAX_SCREENS; ++screen)
        {
            wsprintfA(key, "looks.%d.bgMode.%d", look, screen);
            g_lookBgMode[look][screen] = Store_GetInt(key, g_lookBgMode[look][screen]);
            wsprintfA(key, "looks.%d.slMode.%d", look, screen);
            g_lookSlideMode[look][screen] = Store_GetInt(key, g_lookSlideMode[look][screen]);
        }
        for (int display = 0; display < LOOK_MAX_DISPLAYS; ++display)
        {
            wsprintfA(key, "looks.%d.bgOn.%d", look, display);
            g_lookBgOn[look][display] = Store_GetInt(key, g_lookBgOn[look][display] ? 1 : 0) != 0;
            wsprintfA(key, "looks.%d.slOn.%d", look, display);
            g_lookSlideOn[look][display] = Store_GetInt(key, g_lookSlideOn[look][display] ? 1 : 0) != 0;
        }
    }
    g_looksLoaded = true;
}

static void RefreshScenesUi()
{
    if (g_scenesWindow != nullptr && IsWindow(g_scenesWindow))
    {
        RefreshScenesWindow();
    }
    if (g_programScenesBtn != nullptr)
    {
        InvalidateRect(g_programScenesBtn, nullptr, FALSE);
    }
}

static void ApplyLook(int index)
{
    EnsureLookState();
    LoadLookState();
    if (index < 0 || index >= g_lookCount)
    {
        return;
    }
    g_appliedLook = index;
    if (Store_Ready())
    {
        Store_SetInt("looks.applied", g_appliedLook);
    }
    RefreshScenesUi();
    PresentCurrent();
}

static void ApplyLiveLook()
{
    ApplyLook(g_lookIndex);
}

static void EnsureLookState()
{
    if (g_lookStateReady)
    {
        return;
    }
    for (int look = 0; look < LOOK_MAX; ++look)
    {
        ResetLookScene(look);
    }
    g_lookStateReady = true;
}

static void DefaultLookName(char* out, int outChars)
{
    for (int n = g_lookCount + 1; n <= g_lookCount + LOOK_MAX + 1; ++n)
    {
        wsprintfA(out, "Scene %d", n);
        bool taken = false;
        for (int look = 0; look < g_lookCount && !taken; ++look)
        {
            taken = lstrcmpiA(g_lookNames[look], out) == 0;
        }
        if (!taken)
        {
            return;
        }
    }
    out[outChars - 1] = '\0';
}

static int AddLookScene()
{
    EnsureLookState();
    LoadLookState();
    if (g_lookCount >= LOOK_MAX)
    {
        return -1;
    }
    const int look = g_lookCount;
    ResetLookScene(look);
    DefaultLookName(g_lookNames[look], LOOK_NAME_MAX);
    g_lookCount += 1;
    g_lookIndex = look;
    const bool firstScene = g_lookCount == 1;
    if (firstScene)
    {
        g_appliedLook = look;
    }
    LookStoreBegin();
    SaveLookMeta();
    SaveLookScene(look);
    LookStoreCommit();
    RefreshScenesUi();
    if (firstScene)
    {
        PresentCurrent();
    }
    return look;
}

static void DeleteLookScene(int look)
{
    if (look < 0 || look >= g_lookCount)
    {
        return;
    }
    const bool wasLive = look == g_appliedLook;
    for (int i = look; i + 1 < g_lookCount; ++i)
    {
        lstrcpynA(g_lookNames[i], g_lookNames[i + 1], LOOK_NAME_MAX);
        memcpy(g_lookBgMode[i], g_lookBgMode[i + 1], sizeof(g_lookBgMode[i]));
        memcpy(g_lookSlideMode[i], g_lookSlideMode[i + 1], sizeof(g_lookSlideMode[i]));
        memcpy(g_lookBgOn[i], g_lookBgOn[i + 1], sizeof(g_lookBgOn[i]));
        memcpy(g_lookSlideOn[i], g_lookSlideOn[i + 1], sizeof(g_lookSlideOn[i]));
    }
    g_lookCount -= 1;
    g_lookNames[g_lookCount][0] = '\0';
    ResetLookScene(g_lookCount);
    if (g_appliedLook > look || (wasLive && g_appliedLook > 0))
    {
        g_appliedLook -= 1;
    }
    if (g_lookIndex > look || (g_lookIndex == look && g_lookIndex > 0))
    {
        g_lookIndex -= 1;
    }
    if (g_lookCount == 0)
    {
        g_lookIndex = 0;
        g_appliedLook = 0;
    }
    SaveLookState();
    RefreshScenesUi();
    if (wasLive)
    {
        PresentCurrent();
    }
}

static void RenameLookScene(int look, const char* name)
{
    if (look < 0 || look >= g_lookCount || name == nullptr)
    {
        return;
    }
    char trimmed[LOOK_NAME_MAX] = {};
    const char* start = name;
    while (*start == ' ' || *start == '\t')
    {
        ++start;
    }
    lstrcpynA(trimmed, start, LOOK_NAME_MAX);
    int len = lstrlenA(trimmed);
    while (len > 0 && (trimmed[len - 1] == ' ' || trimmed[len - 1] == '\t'))
    {
        trimmed[--len] = '\0';
    }
    if (trimmed[0] == '\0')
    {
        return;
    }
    lstrcpynA(g_lookNames[look], trimmed, LOOK_NAME_MAX);
    if (Store_Ready())
    {
        char key[32] = {};
        wsprintfA(key, "looks.%d.name", look);
        Store_SetString(key, g_lookNames[look]);
    }
    RefreshScenesUi();
}

static void RebuildLookLayout()
{
    EnsureLookState();
    ZeroMemory(g_lookDisplays, sizeof(g_lookDisplays));
    g_lookDisplayCount = 0;
    g_lookScreenCount = ManageScreens_ScreenCount();
    if (g_lookScreenCount < 0)
    {
        g_lookScreenCount = 0;
    }
    if (g_lookScreenCount > LOOK_MAX_SCREENS)
    {
        g_lookScreenCount = LOOK_MAX_SCREENS;
    }

    int store = 0;
    for (int screen = 0; screen < g_lookScreenCount; ++screen)
    {
        const int slots = ManageScreens_GetScreenOutputCount(screen);
        if (slots <= 0)
        {
            if (g_lookDisplayCount >= LOOK_MAX_DISPLAYS || store >= LOOK_MAX_DISPLAYS)
            {
                break;
            }
            LookDisplay* item = &g_lookDisplays[g_lookDisplayCount];
            item->screen = screen;
            item->slot = -1;
            item->ghost = true;
            item->store = store;
            g_lookDisplayCount += 1;
            store += 1;
            continue;
        }

        if (slots > 1 && ManageScreens_GetScreenLayout(screen) == SCREEN_LAYOUT_MIRROR)
        {
            if (g_lookDisplayCount >= LOOK_MAX_DISPLAYS || store >= LOOK_MAX_DISPLAYS)
            {
                break;
            }
            LookDisplay* item = &g_lookDisplays[g_lookDisplayCount];
            item->screen = screen;
            item->slot = 0;
            item->ghost = false;
            item->mirror = true;
            item->store = store;
            g_lookDisplayCount += 1;
            store += slots;
            continue;
        }

        for (int slot = 0; slot < slots && g_lookDisplayCount < LOOK_MAX_DISPLAYS && store < LOOK_MAX_DISPLAYS; ++slot)
        {
            LookDisplay* item = &g_lookDisplays[g_lookDisplayCount];
            item->screen = screen;
            item->slot = slot;
            item->ghost = false;
            item->store = store;
            g_lookDisplayCount += 1;
            store += 1;
        }
    }
}

static void SetLookColumnSwitch(int look, int column, bool background, bool on)
{
    if (look < 0 || look >= g_lookCount || column < 0 || column >= g_lookDisplayCount)
    {
        return;
    }
    const LookDisplay* item = &g_lookDisplays[column];
    int count = 1;
    if (item->mirror)
    {
        count = ManageScreens_GetScreenOutputCount(item->screen);
    }
    for (int k = 0; k < count && item->store + k < LOOK_MAX_DISPLAYS; ++k)
    {
        if (background)
        {
            g_lookBgOn[look][item->store + k] = on;
        }
        else
        {
            g_lookSlideOn[look][item->store + k] = on;
        }
    }
}

#define LOOK_CAP_TOP (LOOK_CARD_PAD + LOOK_CARD_HEAD)
#define LOOK_THUMB_TOP (LOOK_CAP_TOP + LOOK_CAP_H)
#define LOOK_CELL_TOP (LOOK_THUMB_TOP + LOOK_THUMB_H + LOOK_CELL_GAP)
#define LOOK_CARD_H (LOOK_CELL_TOP + 2 * LOOK_CELL_H + LOOK_CARD_PAD)
#define LOOK_MODE_KEY_W 30
#define LOOK_SPAN_CARD_MIN_W (LOOK_CARD_PAD * 2 + LOOK_MODE_KEY_W + 8 + LOOK_SEG_SPAN_W + LOOK_SEG_MIRROR_W + 6)
#define LOOK_PLACE_W 320
#define LOOK_PLACE_H 180

enum
{
    SCENE_FONT_TITLE,
    SCENE_FONT_BODY,
    SCENE_FONT_BODY_BOLD,
    SCENE_FONT_LABEL,
    SCENE_FONT_SMALL,
    SCENE_FONT_SMALL_BOLD,
    SCENE_FONT_PILL,
    SCENE_FONT_COUNT
};

enum
{
    SCENE_HIT_NONE,
    SCENE_HIT_PLUS,
    SCENE_HIT_MINUS,
    SCENE_HIT_ROW,
    SCENE_HIT_APPLY,
    SCENE_HIT_MODE,
    SCENE_HIT_CHECK
};

struct SceneHit
{
    int kind;
    // Row: scene. Mode: screen. Check: display column.
    int index;
    // 0 = Background, 1 = Slide.
    int row;
    // Mode only: 0 = Span, 1 = Mirror.
    int value;
};

struct SceneCard
{
    int screen;
    int first;
    int count;
    int left;
    int width;
    int colsLeft;
};

// Cards are laid out in their own coordinates, starting at (0, 0), and shown
// through `view` scrolled by g_lookScrollX / g_lookScrollY.
struct ScenesLayout
{
    RECT rail;
    RECT main;
    RECT plus;
    RECT minus;
    RECT apply;
    RECT view;
    int labelsLeft;
    int contentW;
    int contentH;
    SceneCard cards[LOOK_MAX_SCREENS];
    int cardCount;
};

// Changes when what is live changes; the thumbnails are rebuilt from it.
struct SceneLiveKey
{
    int bgOn;
    HBITMAP bgFrame;
    HBITMAP bgStill;
    int slideOn;
    int slide;
    HBITMAP slideFrame;
};

static HFONT g_sceneFonts[SCENE_FONT_COUNT] = {};
static SceneHit g_scenePress = {};
static SceneLiveKey g_sceneLiveKey = {};
static bool g_sceneLiveCached = false;
static HBITMAP g_sceneBgSmall = nullptr;
static HBITMAP g_sceneSlideSmall = nullptr;
static HBITMAP g_scenePlaceBg = nullptr;
static HBITMAP g_scenePlaceSlide = nullptr;

static HFONT MakeSceneFont(int height, int weight, DWORD quality)
{
    return CreateFontA(
        -height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        quality, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
    );
}

static void EnsureSceneFont()
{
    if (g_sceneFont != nullptr)
    {
        return;
    }
    g_sceneFonts[SCENE_FONT_TITLE] = MakeSceneFont(22, FW_SEMIBOLD, CLEARTYPE_QUALITY);
    g_sceneFonts[SCENE_FONT_BODY] = MakeSceneFont(13, FW_NORMAL, CLEARTYPE_QUALITY);
    g_sceneFonts[SCENE_FONT_BODY_BOLD] = MakeSceneFont(13, FW_SEMIBOLD, CLEARTYPE_QUALITY);
    g_sceneFonts[SCENE_FONT_LABEL] = MakeSceneFont(12, FW_NORMAL, CLEARTYPE_QUALITY);
    g_sceneFonts[SCENE_FONT_SMALL] = MakeSceneFont(11, FW_NORMAL, CLEARTYPE_QUALITY);
    g_sceneFonts[SCENE_FONT_SMALL_BOLD] = MakeSceneFont(11, FW_SEMIBOLD, CLEARTYPE_QUALITY);
    g_sceneFonts[SCENE_FONT_PILL] = MakeSceneFont(10, FW_BOLD, CLEARTYPE_QUALITY);
    g_sceneFont = g_sceneFonts[SCENE_FONT_BODY];
}

static HFONT SceneFont(int which)
{
    EnsureSceneFont();
    return g_sceneFonts[which] != nullptr ? g_sceneFonts[which] : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

static COLORREF SceneMix(COLORREF over, COLORREF under, int alpha)
{
    const int inv = 255 - alpha;
    return RGB(
        (GetRValue(over) * alpha + GetRValue(under) * inv) / 255,
        (GetGValue(over) * alpha + GetGValue(under) * inv) / 255,
        (GetBValue(over) * alpha + GetBValue(under) * inv) / 255
    );
}

static Gdiplus::Color SceneGdiColor(COLORREF color)
{
    return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

static void SceneRoundPath(Gdiplus::GraphicsPath* path, float x, float y, float w, float h, float r)
{
    if (r * 2.0f > w)
    {
        r = w / 2.0f;
    }
    if (r * 2.0f > h)
    {
        r = h / 2.0f;
    }
    path->StartFigure();
    if (r <= 0.0f)
    {
        path->AddRectangle(Gdiplus::RectF(x, y, w, h));
        return;
    }
    const float d = r * 2.0f;
    path->AddArc(x, y, d, d, 180.0f, 90.0f);
    path->AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path->AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path->AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path->CloseFigure();
}

static void SceneRound(HDC hdc, const RECT* rc, float radius, COLORREF fill, COLORREF border, float borderW)
{
    float x = (float)rc->left;
    float y = (float)rc->top;
    float w = (float)(rc->right - rc->left);
    float h = (float)(rc->bottom - rc->top);
    if (w <= 0.0f || h <= 0.0f)
    {
        return;
    }
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    if (border != CLR_INVALID && borderW > 0.0f)
    {
        Gdiplus::GraphicsPath outer;
        SceneRoundPath(&outer, x, y, w, h, radius);
        Gdiplus::SolidBrush edge(SceneGdiColor(border));
        g.FillPath(&edge, &outer);
        x += borderW;
        y += borderW;
        w -= borderW * 2.0f;
        h -= borderW * 2.0f;
        radius -= borderW;
        if (w <= 0.0f || h <= 0.0f)
        {
            return;
        }
    }
    Gdiplus::GraphicsPath path;
    SceneRoundPath(&path, x, y, w, h, radius);
    Gdiplus::SolidBrush brush(SceneGdiColor(fill));
    g.FillPath(&brush, &path);
}

// Rounds a thumbnail's corners by painting the card colour outside them.
static void SceneMaskCorners(HDC hdc, const RECT* rc, float radius, COLORREF outside)
{
    const float x = (float)rc->left;
    const float y = (float)rc->top;
    const float w = (float)(rc->right - rc->left);
    const float h = (float)(rc->bottom - rc->top);
    if (w <= 0.0f || h <= 0.0f)
    {
        return;
    }
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    Gdiplus::GraphicsPath corners(Gdiplus::FillModeAlternate);
    corners.AddRectangle(Gdiplus::RectF(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f));
    SceneRoundPath(&corners, x, y, w, h, radius);
    Gdiplus::SolidBrush brush(SceneGdiColor(outside));
    g.FillPath(&brush, &corners);
    Gdiplus::GraphicsPath edge;
    SceneRoundPath(&edge, x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, radius - 0.5f);
    Gdiplus::Pen pen(Gdiplus::Color(20, 255, 255, 255), 1.0f);
    g.DrawPath(&pen, &edge);
}

static void SceneText(HDC hdc, const char* text, RECT rc, int font, COLORREF color, UINT flags)
{
    HFONT old = (HFONT)SelectObject(hdc, SceneFont(font));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rc, flags | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static int SceneTextWidth(HDC hdc, const char* text, int font)
{
    HFONT old = (HFONT)SelectObject(hdc, SceneFont(font));
    SIZE size = {};
    GetTextExtentPoint32A(hdc, text, lstrlenA(text), &size);
    SelectObject(hdc, old);
    return size.cx;
}

static int ScenePillWidth(HDC hdc, const char* text)
{
    SetTextCharacterExtra(hdc, 1);
    const int width = SceneTextWidth(hdc, text, SCENE_FONT_PILL) + 14;
    SetTextCharacterExtra(hdc, 0);
    return width;
}

static void ScenePaintLivePill(HDC hdc, int left, int centerY, COLORREF behind)
{
    const int width = ScenePillWidth(hdc, "LIVE");
    RECT pill = { left, centerY - 9, left + width, centerY + 9 };
    SceneRound(hdc, &pill, 4.0f, SceneMix(RGB(110, 198, 110), behind, 41));
    SetTextCharacterExtra(hdc, 1);
    RECT text = pill;
    text.left += 1;
    SceneText(hdc, "LIVE", text, SCENE_FONT_PILL, LOOK_COLOR_LIVE_INK, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SetTextCharacterExtra(hdc, 0);
}

static void ScenePaintCheck(HDC hdc, const RECT* box, bool on, bool pressed)
{
    if (!on)
    {
        SceneRound(hdc, box, 5.0f, pressed ? LOOK_COLOR_RAISED : LOOK_COLOR_SUNKEN, RGB(0x5A, 0x5A, 0x5A), 1.5f);
        return;
    }
    SceneRound(hdc, box, 5.0f, pressed ? RGB(0xC8, 0xC8, 0xC8) : LOOK_COLOR_ACCENT);
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(SceneGdiColor(LOOK_COLOR_ACCENT_INK), 2.0f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    const float x = (float)box->left;
    const float y = (float)box->top;
    const Gdiplus::PointF tick[3] = {
        Gdiplus::PointF(x + 5.0f, y + 9.5f),
        Gdiplus::PointF(x + 8.0f, y + 12.5f),
        Gdiplus::PointF(x + 13.5f, y + 6.0f),
    };
    g.DrawLines(&pen, tick, 3);
}

static void ScenePaintRailButton(HDC hdc, const RECT* rc, bool plus, bool pressed)
{
    SceneRound(hdc, rc, 6.0f, pressed ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED, RGB(0x45, 0x45, 0x45));
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    Gdiplus::Pen pen(SceneGdiColor(LOOK_COLOR_INK_BRIGHT), 1.5f);
    const float cx = (rc->left + rc->right) / 2.0f;
    const float cy = (rc->top + rc->bottom) / 2.0f;
    g.DrawLine(&pen, cx - 5.0f, cy, cx + 5.0f, cy);
    if (plus)
    {
        g.DrawLine(&pen, cx, cy - 5.0f, cx, cy + 5.0f);
    }
}

static HBITMAP SceneCreateDib(int width, int height, unsigned char** bits)
{
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* raw = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &raw, nullptr, 0);
    if (bits != nullptr)
    {
        *bits = bmp != nullptr ? static_cast<unsigned char*>(raw) : nullptr;
    }
    return bmp;
}

// The caller deletes the returned wrapper; it reads the DIB's pixels in place.
// Every frame here is a top-down DIB; GetObject reports a positive height for
// those too, so it can't be used to tell the row order.
static Gdiplus::Bitmap* SceneWrapDib(HBITMAP bmp)
{
    DIBSECTION section = {};
    if (bmp == nullptr ||
        GetObject(bmp, sizeof(section), &section) != sizeof(section) ||
        section.dsBm.bmBits == nullptr ||
        section.dsBm.bmBitsPixel != 32)
    {
        return nullptr;
    }
    const int width = section.dsBm.bmWidth;
    const int height = section.dsBm.bmHeight < 0 ? -section.dsBm.bmHeight : section.dsBm.bmHeight;
    if (width < 1 || height < 1)
    {
        return nullptr;
    }
    BYTE* scan0 = static_cast<BYTE*>(section.dsBm.bmBits);
    return new Gdiplus::Bitmap(width, height, section.dsBm.bmWidthBytes, PixelFormat32bppPARGB, scan0);
}

static HBITMAP SceneShrink(HBITMAP source)
{
    Gdiplus::Bitmap* image = SceneWrapDib(source);
    if (image == nullptr)
    {
        return nullptr;
    }
    const int srcW = (int)image->GetWidth();
    const int srcH = (int)image->GetHeight();
    const int width = srcW > 640 ? 640 : srcW;
    int height = MulDiv(srcH, width, srcW);
    if (height < 1)
    {
        height = 1;
    }
    unsigned char* bits = nullptr;
    HBITMAP shrunk = SceneCreateDib(width, height, &bits);
    if (shrunk != nullptr && bits != nullptr)
    {
        ZeroMemory(bits, (size_t)width * height * 4);
        Gdiplus::Bitmap target(width, height, width * 4, PixelFormat32bppPARGB, bits);
        Gdiplus::Graphics g(&target);
        g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::ImageAttributes wrap;
        wrap.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        g.DrawImage(image, Gdiplus::Rect(0, 0, width, height), 0, 0, srcW, srcH, Gdiplus::UnitPixel, &wrap);
    }
    delete image;
    return shrunk;
}

// `src` is in compose coordinates, as LookOutputRects returns it.
static void SceneDrawCrop(HDC dest, const RECT* rc, HBITMAP bmp, const RECT* src, int composeW, int composeH)
{
    Gdiplus::Bitmap* image = SceneWrapDib(bmp);
    if (image == nullptr)
    {
        return;
    }
    const int bmpW = (int)image->GetWidth();
    const int bmpH = (int)image->GetHeight();
    RECT crop = {};
    MapRectToCompose(src, composeW, composeH, bmpW, bmpH, &crop);
    crop.left = max(0L, crop.left);
    crop.top = max(0L, crop.top);
    crop.right = min((LONG)bmpW, crop.right);
    crop.bottom = min((LONG)bmpH, crop.bottom);
    if (crop.right > crop.left && crop.bottom > crop.top)
    {
        Gdiplus::Graphics g(dest);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::ImageAttributes wrap;
        wrap.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        g.DrawImage(
            image,
            Gdiplus::Rect(rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top),
            crop.left,
            crop.top,
            crop.right - crop.left,
            crop.bottom - crop.top,
            Gdiplus::UnitPixel,
            &wrap
        );
    }
    delete image;
}

// Stand-ins shown when nothing is live, so the crops still read: a gradient
// for the background and a faint word for the slide.
static void EnsureScenePlaceholders()
{
    const int w = LOOK_PLACE_W;
    const int h = LOOK_PLACE_H;
    if (g_scenePlaceBg == nullptr)
    {
        unsigned char* bits = nullptr;
        g_scenePlaceBg = SceneCreateDib(w, h, &bits);
        for (int y = 0; bits != nullptr && y < h; ++y)
        {
            const float v = (float)y / (float)(h - 1);
            for (int x = 0; x < w; ++x)
            {
                const float t = (float)x / (float)(w - 1);
                const float light = 1.0f - (0.32f * v) + (0.08f * sinf((t * 9.4f) + (v * 3.1f)));
                const float r = (0x48 + ((0xB0 - 0x48) * t)) * light;
                const float g = (0x2C + ((0x30 - 0x2C) * t)) * light;
                const float b = (0x8C + ((0x6A - 0x8C) * t)) * light;
                unsigned char* px = bits + (((y * w) + x) * 4);
                px[0] = (unsigned char)max(0.0f, min(255.0f, b));
                px[1] = (unsigned char)max(0.0f, min(255.0f, g));
                px[2] = (unsigned char)max(0.0f, min(255.0f, r));
                px[3] = 255;
            }
        }
    }
    if (g_scenePlaceSlide == nullptr)
    {
        unsigned char* bits = nullptr;
        g_scenePlaceSlide = SceneCreateDib(w, h, &bits);
        if (bits != nullptr)
        {
            ZeroMemory(bits, (size_t)w * h * 4);
            HDC dc = CreateCompatibleDC(nullptr);
            HBITMAP oldBmp = (HBITMAP)SelectObject(dc, g_scenePlaceSlide);
            HFONT font = MakeSceneFont(64, FW_SEMIBOLD, ANTIALIASED_QUALITY);
            HFONT oldFont = (HFONT)SelectObject(dc, font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            RECT rc = { 0, 0, w, h };
            DrawTextA(dc, "Slide", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, oldFont);
            DeleteObject(font);
            SelectObject(dc, oldBmp);
            DeleteDC(dc);
            GdiFlush();
            for (int i = 0; i < w * h; ++i)
            {
                unsigned char* px = bits + (i * 4);
                const int level = max(px[0], max(px[1], px[2]));
                const unsigned char a = (unsigned char)((level * 150) / 255);
                px[0] = a;
                px[1] = a;
                px[2] = a;
                px[3] = a;
            }
        }
    }
}

static void SceneReadLiveKey(SceneLiveKey* key)
{
    ZeroMemory(key, sizeof(*key));
    key->bgOn = BackgroundLayerOn() ? 1 : 0;
    if (key->bgOn)
    {
        key->bgFrame = g_liveBgFrame;
        key->bgStill = g_liveBgStill;
    }
    key->slideOn = g_liveArmed ? 1 : 0;
    if (key->slideOn)
    {
        key->slide = Renderer_CurrentSlide();
        key->slideFrame = Renderer_SlideFrame(key->slide, nullptr, nullptr);
    }
}

static void SceneDropLiveCache()
{
    if (g_sceneBgSmall != nullptr)
    {
        DeleteObject(g_sceneBgSmall);
        g_sceneBgSmall = nullptr;
    }
    if (g_sceneSlideSmall != nullptr)
    {
        DeleteObject(g_sceneSlideSmall);
        g_sceneSlideSmall = nullptr;
    }
    g_sceneLiveCached = false;
}

static void SceneEnsureLiveCache()
{
    SceneLiveKey key = {};
    SceneReadLiveKey(&key);
    if (g_sceneLiveCached && memcmp(&key, &g_sceneLiveKey, sizeof(key)) == 0)
    {
        return;
    }
    SceneDropLiveCache();
    if (key.bgOn)
    {
        g_sceneBgSmall = SceneShrink(key.bgStill != nullptr ? key.bgStill : key.bgFrame);
    }
    if (key.slideOn)
    {
        g_sceneSlideSmall = SceneShrink(key.slideFrame);
    }
    memcpy(&g_sceneLiveKey, &key, sizeof(key));
    g_sceneLiveCached = true;
}

static void ScenesNoteLive()
{
    if (g_scenesWindow == nullptr || !IsWindow(g_scenesWindow))
    {
        return;
    }
    SceneLiveKey key = {};
    SceneReadLiveKey(&key);
    if (g_sceneLiveCached && memcmp(&key, &g_sceneLiveKey, sizeof(key)) == 0)
    {
        return;
    }
    InvalidateRect(g_scenesWindow, nullptr, FALSE);
}

static const char* LayoutModeName(int mode)
{
    if (mode == 1)
    {
        return "Span";
    }
    if (mode == 2)
    {
        return "Mirror";
    }
    return "Single";
}

static void MeasureScenes(const RECT* client, ScenesLayout* layout)
{
    ZeroMemory(layout, sizeof(*layout));
    layout->rail.left = client->left + LOOK_GAP;
    layout->rail.top = client->top + LOOK_GAP;
    layout->rail.right = layout->rail.left + LOOK_RAIL_W;
    layout->rail.bottom = max(client->bottom - LOOK_GAP, layout->rail.top + LOOK_HEADER_H);
    layout->main.left = layout->rail.right + LOOK_GAP;
    layout->main.top = layout->rail.top;
    layout->main.right = max(client->right - LOOK_GAP, layout->main.left + 2 * LOOK_MAIN_PAD_X);
    layout->main.bottom = layout->rail.bottom;

    const int buttonTop = layout->rail.top + (LOOK_HEADER_H - LOOK_BTN) / 2;
    layout->minus.right = layout->rail.right - 10;
    layout->minus.left = layout->minus.right - LOOK_BTN;
    layout->minus.top = buttonTop;
    layout->minus.bottom = buttonTop + LOOK_BTN;
    layout->plus.right = layout->minus.left - 6;
    layout->plus.left = layout->plus.right - LOOK_BTN;
    layout->plus.top = buttonTop;
    layout->plus.bottom = buttonTop + LOOK_BTN;

    const int headTop = layout->main.top + LOOK_MAIN_PAD_Y;
    layout->apply.right = layout->main.right - LOOK_MAIN_PAD_X;
    layout->apply.left = layout->apply.right - 76;
    layout->apply.top = headTop + (LOOK_TITLE_H - 32) / 2;
    layout->apply.bottom = layout->apply.top + 32;

    layout->labelsLeft = layout->main.left + LOOK_MAIN_PAD_X;
    layout->view.left = layout->labelsLeft + LOOK_LABEL_W;
    layout->view.top = headTop + LOOK_TITLE_H + 16;
    layout->view.right = max(layout->main.right - LOOK_MAIN_PAD_X, layout->view.left);
    layout->view.bottom = max(layout->main.bottom - LOOK_MAIN_PAD_Y, layout->view.top);

    int x = 0;
    for (int d = 0; d < g_lookDisplayCount && layout->cardCount < LOOK_MAX_SCREENS;)
    {
        SceneCard* card = &layout->cards[layout->cardCount];
        layout->cardCount += 1;
        card->screen = g_lookDisplays[d].screen;
        card->first = d;
        while (d < g_lookDisplayCount && g_lookDisplays[d].screen == card->screen)
        {
            card->count += 1;
            d += 1;
        }
        const int colsW = (card->count * LOOK_COL_W) + ((card->count - 1) * LOOK_COL_GAP);
        card->width = colsW + (LOOK_CARD_PAD * 2);
        if (ManageScreens_GetScreenLayout(card->screen) == SCREEN_LAYOUT_SPAN && card->width < LOOK_SPAN_CARD_MIN_W)
        {
            card->width = LOOK_SPAN_CARD_MIN_W;
        }
        card->left = x;
        card->colsLeft = x + ((card->width - colsW) / 2);
        x += card->width + LOOK_CARD_GAP;
    }
    layout->contentW = layout->cardCount > 0 ? x - LOOK_CARD_GAP : 0;
    layout->contentH = LOOK_CARD_H + LOOK_LEGEND_GAP + LOOK_LEGEND_H;
}

static int SceneColumnLeft(const SceneCard* card, int display)
{
    return card->colsLeft + ((display - card->first) * (LOOK_COL_W + LOOK_COL_GAP));
}

static void SceneSegRects(const SceneCard* card, int row, RECT* outer, RECT* spanRc, RECT* mirrorRc)
{
    const int top = LOOK_CARD_PAD + 26 + (row * (LOOK_SEG_H + 4));
    const int left = card->left + LOOK_CARD_PAD + LOOK_MODE_KEY_W + 8;
    spanRc->left = left + 2;
    spanRc->top = top + 2;
    spanRc->right = spanRc->left + LOOK_SEG_SPAN_W;
    spanRc->bottom = top + LOOK_SEG_H - 2;
    mirrorRc->left = spanRc->right + 2;
    mirrorRc->top = spanRc->top;
    mirrorRc->right = mirrorRc->left + LOOK_SEG_MIRROR_W;
    mirrorRc->bottom = spanRc->bottom;
    outer->left = left;
    outer->top = top;
    outer->right = mirrorRc->right + 2;
    outer->bottom = top + LOOK_SEG_H;
}

static void SceneCheckRect(const SceneCard* card, int display, int row, RECT* out)
{
    out->left = SceneColumnLeft(card, display) + ((LOOK_COL_W - LOOK_CHECK) / 2);
    out->top = LOOK_CELL_TOP + (row * LOOK_CELL_H) + ((LOOK_CELL_H - LOOK_CHECK) / 2);
    out->right = out->left + LOOK_CHECK;
    out->bottom = out->top + LOOK_CHECK;
}

static void SceneRowRect(const ScenesLayout* layout, int look, RECT* out)
{
    out->left = layout->rail.left + 6;
    out->right = layout->rail.right - 6;
    out->top = layout->rail.top + LOOK_HEADER_H + (look * LOOK_ROW_H);
    out->bottom = out->top + LOOK_ROW_H;
}

static bool ScenePressed(int kind, int index, int row, int value)
{
    return g_scenePress.kind == kind &&
        g_scenePress.index == index &&
        g_scenePress.row == row &&
        g_scenePress.value == value;
}

static bool SceneSameHit(const SceneHit* a, const SceneHit* b)
{
    return a->kind == b->kind && a->index == b->index && a->row == b->row && a->value == b->value;
}

static SceneHit SceneHitTest(HWND hwnd, int x, int y)
{
    SceneHit hit = {};
    RECT client = {};
    GetClientRect(hwnd, &client);
    ScenesLayout layout = {};
    MeasureScenes(&client, &layout);
    const POINT pt = { x, y };
    if (PtInRect(&layout.plus, pt))
    {
        hit.kind = SCENE_HIT_PLUS;
        return hit;
    }
    if (PtInRect(&layout.minus, pt))
    {
        hit.kind = SCENE_HIT_MINUS;
        return hit;
    }
    if (PtInRect(&layout.rail, pt))
    {
        for (int i = 0; i < g_lookCount; ++i)
        {
            RECT row = {};
            SceneRowRect(&layout, i, &row);
            if (row.bottom > layout.rail.bottom)
            {
                break;
            }
            if (PtInRect(&row, pt))
            {
                hit.kind = SCENE_HIT_ROW;
                hit.index = i;
                break;
            }
        }
        return hit;
    }
    if (g_lookCount <= 0)
    {
        return hit;
    }
    if (PtInRect(&layout.apply, pt))
    {
        hit.kind = SCENE_HIT_APPLY;
        return hit;
    }
    if (!PtInRect(&layout.view, pt))
    {
        return hit;
    }
    const POINT at = { x - layout.view.left + g_lookScrollX, y - layout.view.top + g_lookScrollY };
    for (int c = 0; c < layout.cardCount; ++c)
    {
        const SceneCard* card = &layout.cards[c];
        if (at.x < card->left || at.x >= card->left + card->width)
        {
            continue;
        }
        if (ManageScreens_GetScreenLayout(card->screen) == SCREEN_LAYOUT_SPAN)
        {
            for (int row = 0; row < 2; ++row)
            {
                RECT outer = {};
                RECT spanRc = {};
                RECT mirrorRc = {};
                SceneSegRects(card, row, &outer, &spanRc, &mirrorRc);
                if (PtInRect(&spanRc, at) || PtInRect(&mirrorRc, at))
                {
                    hit.kind = SCENE_HIT_MODE;
                    hit.index = card->screen;
                    hit.row = row;
                    hit.value = PtInRect(&mirrorRc, at) ? 1 : 0;
                    return hit;
                }
            }
        }
        for (int d = card->first; d < card->first + card->count; ++d)
        {
            if (g_lookDisplays[d].ghost)
            {
                continue;
            }
            for (int row = 0; row < 2; ++row)
            {
                RECT box = {};
                SceneCheckRect(card, d, row, &box);
                InflateRect(&box, 10, 10);
                if (PtInRect(&box, at))
                {
                    hit.kind = SCENE_HIT_CHECK;
                    hit.index = d;
                    hit.row = row;
                    return hit;
                }
            }
        }
        break;
    }
    return hit;
}

static void UpdateLookScroll(HWND hwnd)
{
    if (hwnd == nullptr)
    {
        return;
    }
    // Measure as if neither bar were showing, then take room for the bars the
    // content needs; a bar that is up now would otherwise keep itself up.
    RECT client = {};
    GetClientRect(hwnd, &client);
    const LONG_PTR style = GetWindowLongPtrA(hwnd, GWL_STYLE);
    const int barW = GetSystemMetrics(SM_CXVSCROLL);
    const int barH = GetSystemMetrics(SM_CYHSCROLL);
    if ((style & WS_VSCROLL) != 0)
    {
        client.right += barW;
    }
    if ((style & WS_HSCROLL) != 0)
    {
        client.bottom += barH;
    }
    ScenesLayout layout = {};
    MeasureScenes(&client, &layout);

    int viewW = layout.view.right - layout.view.left;
    int viewH = layout.view.bottom - layout.view.top;
    bool needX = layout.contentW > viewW;
    bool needY = layout.contentH > viewH;
    if (needX)
    {
        viewH -= barH;
        needY = layout.contentH > viewH;
    }
    if (needY)
    {
        viewW -= barW;
        if (!needX && layout.contentW > viewW)
        {
            viewH -= barH;
        }
    }
    viewW = max(1, viewW);
    viewH = max(1, viewH);
    const int maxX = layout.contentW > viewW ? layout.contentW - viewW : 0;
    const int maxY = layout.contentH > viewH ? layout.contentH - viewH : 0;
    g_lookScrollX = max(0, min(g_lookScrollX, maxX));
    g_lookScrollY = max(0, min(g_lookScrollY, maxY));

    SCROLLINFO info = {};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = layout.contentW > 0 ? layout.contentW - 1 : 0;
    info.nPage = viewW > 0 ? viewW : 1;
    info.nPos = g_lookScrollX;
    SetScrollInfo(hwnd, SB_HORZ, &info, TRUE);

    info.nMax = layout.contentH > 0 ? layout.contentH - 1 : 0;
    info.nPage = viewH > 0 ? viewH : 1;
    info.nPos = g_lookScrollY;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
}

static void LookScrollBy(HWND hwnd, int bar, int delta)
{
    if (delta == 0)
    {
        return;
    }
    if (bar == SB_HORZ)
    {
        g_lookScrollX += delta;
    }
    else
    {
        g_lookScrollY += delta;
    }
    UpdateLookScroll(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
}

static int LookScrollTrack(HWND hwnd, int bar, WPARAM wParam)
{
    SCROLLINFO info = {};
    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;
    GetScrollInfo(hwnd, bar, &info);
    int pos = info.nPos;
    switch (LOWORD(wParam))
    {
        case SB_LINELEFT:
            pos -= 32;
            break;
        case SB_LINERIGHT:
            pos += 32;
            break;
        case SB_PAGELEFT:
            pos -= (int)info.nPage;
            break;
        case SB_PAGERIGHT:
            pos += (int)info.nPage;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            pos = info.nTrackPos;
            break;
        default:
            return 0;
    }
    const int current = (bar == SB_HORZ) ? g_lookScrollX : g_lookScrollY;
    return pos - current;
}

void RefreshScenesWindow()
{
    if (g_scenesWindow == nullptr || !IsWindow(g_scenesWindow))
    {
        return;
    }
    EnsureLookState();
    if (g_lookIndex < 0 || g_lookIndex >= g_lookCount)
    {
        g_lookIndex = 0;
    }
    RebuildLookLayout();
    SceneDropLiveCache();
    UpdateLookScroll(g_scenesWindow);
    InvalidateRect(g_scenesWindow, nullptr, FALSE);
}

static void PaintSceneRail(HDC hdc, const ScenesLayout* layout)
{
    RECT head = { layout->rail.left + 14, layout->rail.top, layout->plus.left - 8, layout->rail.top + LOOK_HEADER_H };
    SetTextCharacterExtra(hdc, 1);
    SceneText(hdc, "SCENES", head, SCENE_FONT_SMALL, LOOK_COLOR_INK_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SetTextCharacterExtra(hdc, 0);
    ScenePaintRailButton(hdc, &layout->plus, true, ScenePressed(SCENE_HIT_PLUS, 0, 0, 0));
    ScenePaintRailButton(hdc, &layout->minus, false, ScenePressed(SCENE_HIT_MINUS, 0, 0, 0));

    if (g_lookCount == 0)
    {
        RECT hint = {};
        SceneRowRect(layout, 0, &hint);
        hint.left += 10;
        SceneText(hdc, "Press + to make a scene", hint, SCENE_FONT_BODY, LOOK_COLOR_INK_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return;
    }
    for (int i = 0; i < g_lookCount; ++i)
    {
        RECT row = {};
        SceneRowRect(layout, i, &row);
        if (row.bottom > layout->rail.bottom)
        {
            break;
        }
        const bool selected = i == g_lookIndex;
        const COLORREF behind = selected ? LOOK_COLOR_RAISED : LOOK_COLOR_SURFACE;
        if (selected)
        {
            SceneRound(hdc, &row, 7.0f, LOOK_COLOR_RAISED);
        }
        int textRight = row.right - 10;
        if (i == g_appliedLook)
        {
            const int pillW = ScenePillWidth(hdc, "LIVE");
            const int pillLeft = row.right - 10 - pillW;
            ScenePaintLivePill(hdc, pillLeft, (row.top + row.bottom) / 2, behind);
            textRight = pillLeft - 8;
        }
        if (i != g_sceneNameEditing)
        {
            RECT text = { row.left + 10, row.top, textRight, row.bottom };
            SceneText(
                hdc,
                g_lookNames[i],
                text,
                SCENE_FONT_BODY,
                selected ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS
            );
        }
    }
}

static void PaintSceneHeader(HDC hdc, const ScenesLayout* layout)
{
    const int headTop = layout->main.top + LOOK_MAIN_PAD_Y;
    const int left = layout->main.left + LOOK_MAIN_PAD_X;
    const char* name = g_lookNames[g_lookIndex];
    const bool live = g_lookIndex == g_appliedLook;
    const int pillSpace = live ? ScenePillWidth(hdc, "LIVE") + 12 : 0;
    const int roomW = layout->apply.left - 16 - left - pillSpace;
    const int titleW = max(0, min(SceneTextWidth(hdc, name, SCENE_FONT_TITLE), roomW));
    RECT title = { left, headTop, left + titleW, headTop + 30 };
    SceneText(hdc, name, title, SCENE_FONT_TITLE, LOOK_COLOR_INK_BRIGHT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (live)
    {
        ScenePaintLivePill(hdc, title.right + 12, headTop + 16, LOOK_COLOR_SURFACE);
    }
    RECT sub = { left, headTop + 33, layout->apply.left - 16, headTop + LOOK_TITLE_H };
    SceneText(
        hdc,
        "Double-click a scene to rename it. Changes save as you make them.",
        sub,
        SCENE_FONT_LABEL,
        LOOK_COLOR_INK_DIM,
        DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS
    );

    const bool pressed = ScenePressed(SCENE_HIT_APPLY, 0, 0, 0);
    SceneRound(hdc, &layout->apply, 6.0f, pressed ? RGB(0xC4, 0xC4, 0xC4) : LOOK_COLOR_ACCENT);
    SceneText(hdc, "Apply", layout->apply, SCENE_FONT_BODY_BOLD, LOOK_COLOR_ACCENT_INK, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void ScenePaintThumb(HDC hdc, const RECT* rc, int display, int composeW, int composeH, bool bgLive, bool slideLive)
{
    const LookDisplay* item = &g_lookDisplays[display];
    if (item->ghost)
    {
        HBRUSH empty = CreateSolidBrush(RGB(0x16, 0x16, 0x16));
        FillRect(hdc, rc, empty);
        DeleteObject(empty);
        SceneMaskCorners(hdc, rc, 6.0f, LOOK_COLOR_SUNKEN);
        return;
    }
    FillRect(hdc, rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    RECT videoSrc = {};
    RECT overlaySrc = {};
    bool bgOn = true;
    bool slideOn = true;
    LookOutputRects(g_lookIndex, item->screen, item->mirror ? 0 : item->slot, composeW, composeH, &videoSrc, &overlaySrc, &bgOn, &slideOn);
    if (bgOn)
    {
        SceneDrawCrop(hdc, rc, bgLive ? g_sceneBgSmall : g_scenePlaceBg, &videoSrc, composeW, composeH);
    }
    if (slideOn)
    {
        SceneDrawCrop(hdc, rc, slideLive ? g_sceneSlideSmall : g_scenePlaceSlide, &overlaySrc, composeW, composeH);
    }
    SceneMaskCorners(hdc, rc, 6.0f, LOOK_COLOR_SUNKEN);
}

static void PaintSceneCards(HDC hdc, const ScenesLayout* layout, int width, int height)
{
    RECT all = { 0, 0, width, height };
    HBRUSH surface = CreateSolidBrush(LOOK_COLOR_SURFACE);
    FillRect(hdc, &all, surface);
    DeleteObject(surface);

    EnsureScenePlaceholders();
    SceneEnsureLiveCache();
    const bool bgLive = BackgroundLayerOn();
    const bool slideLive = g_liveArmed;
    int composeW = 1920;
    int composeH = 1080;
    ManageScreens_GetScreenCanvas(PreviewScreenIndex(), &composeW, &composeH, nullptr);
    if (composeW < 1 || composeH < 1)
    {
        composeW = 1920;
        composeH = 1080;
    }

    HBRUSH line = CreateSolidBrush(LOOK_COLOR_SURFACE);
    for (int c = 0; c < layout->cardCount; ++c)
    {
        const SceneCard* card = &layout->cards[c];
        const int screen = card->screen;
        const int screenLayout = ManageScreens_GetScreenLayout(screen);
        RECT cardRc = { card->left, 0, card->left + card->width, LOOK_CARD_H };
        SceneRound(hdc, &cardRc, 10.0f, LOOK_COLOR_SUNKEN);

        char screenName[64] = {};
        if (!ManageScreens_GetScreenLabel(screen, screenName, sizeof(screenName)))
        {
            wsprintfA(screenName, "Screen %d", screen + 1);
        }
        const char* chip = LayoutModeName(screenLayout);
        const int innerLeft = card->left + LOOK_CARD_PAD;
        const int innerRight = card->left + card->width - LOOK_CARD_PAD;
        const int chipW = SceneTextWidth(hdc, chip, SCENE_FONT_SMALL) + 14;
        const int nameW = max(0, min(SceneTextWidth(hdc, screenName, SCENE_FONT_BODY_BOLD), innerRight - innerLeft - chipW - 8));
        RECT nameRc = { innerLeft, LOOK_CARD_PAD, innerLeft + nameW, LOOK_CARD_PAD + 20 };
        SceneText(hdc, screenName, nameRc, SCENE_FONT_BODY_BOLD, LOOK_COLOR_INK_BRIGHT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT chipRc = { nameRc.right + 8, LOOK_CARD_PAD + 2, nameRc.right + 8 + chipW, LOOK_CARD_PAD + 19 };
        SceneRound(hdc, &chipRc, 8.5f, RGB(0x34, 0x34, 0x34));
        SceneText(hdc, chip, chipRc, SCENE_FONT_SMALL, LOOK_COLOR_INK_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (screenLayout == SCREEN_LAYOUT_SPAN)
        {
            const char* keys[2] = { "BG", "Slide" };
            for (int row = 0; row < 2; ++row)
            {
                RECT outer = {};
                RECT spanRc = {};
                RECT mirrorRc = {};
                SceneSegRects(card, row, &outer, &spanRc, &mirrorRc);
                RECT keyRc = { innerLeft, outer.top, innerLeft + LOOK_MODE_KEY_W, outer.bottom };
                SceneText(hdc, keys[row], keyRc, SCENE_FONT_SMALL, LOOK_COLOR_INK_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SceneRound(hdc, &outer, 6.0f, LOOK_COLOR_SUNKEN, RGB(0x33, 0x33, 0x33));
                const int mode = row == 0 ? g_lookBgMode[g_lookIndex][screen] : g_lookSlideMode[g_lookIndex][screen];
                const RECT* parts[2] = { &spanRc, &mirrorRc };
                const char* labels[2] = { "Span", "Mirror" };
                for (int value = 0; value < 2; ++value)
                {
                    const bool on = (mode == 0 ? 0 : 1) == value;
                    if (on)
                    {
                        SceneRound(hdc, parts[value], 4.0f, LOOK_COLOR_RAISED_HI);
                    }
                    else if (ScenePressed(SCENE_HIT_MODE, screen, row, value))
                    {
                        SceneRound(hdc, parts[value], 4.0f, LOOK_COLOR_RAISED);
                    }
                    SceneText(
                        hdc,
                        labels[value],
                        *parts[value],
                        SCENE_FONT_SMALL,
                        on ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK_DIM,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE
                    );
                }
            }
        }
        else
        {
            const char* note = "One display";
            if (g_lookDisplays[card->first].ghost)
            {
                note = "No displays yet";
            }
            else if (screenLayout == SCREEN_LAYOUT_MIRROR)
            {
                note = "Every display shows the same picture";
            }
            RECT noteRc = { innerLeft, LOOK_CARD_PAD + 26, innerRight, LOOK_CAP_TOP };
            SceneText(hdc, note, noteRc, SCENE_FONT_SMALL, LOOK_COLOR_INK_FAINT, DT_LEFT | DT_TOP | DT_WORDBREAK);
        }

        for (int d = card->first; d < card->first + card->count; ++d)
        {
            const int colLeft = SceneColumnLeft(card, d);
            char caption[32] = {};
            if (g_lookDisplays[d].ghost)
            {
                lstrcpyA(caption, "No display");
            }
            else if (g_lookDisplays[d].mirror)
            {
                lstrcpyA(caption, "All displays");
            }
            else
            {
                wsprintfA(caption, "Display %d", g_lookDisplays[d].slot + 1);
            }
            RECT capRc = { colLeft, LOOK_CAP_TOP, colLeft + LOOK_COL_W, LOOK_THUMB_TOP };
            SceneText(hdc, caption, capRc, SCENE_FONT_LABEL, LOOK_COLOR_INK, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT thumb = { colLeft, LOOK_THUMB_TOP, colLeft + LOOK_COL_W, LOOK_THUMB_TOP + LOOK_THUMB_H };
            ScenePaintThumb(hdc, &thumb, d, composeW, composeH, bgLive, slideLive);

            const int store = g_lookDisplays[d].store;
            for (int row = 0; row < 2; ++row)
            {
                const int cellTop = LOOK_CELL_TOP + (row * LOOK_CELL_H);
                RECT rule = { colLeft, cellTop, colLeft + LOOK_COL_W, cellTop + 1 };
                FillRect(hdc, &rule, line);
                if (g_lookDisplays[d].ghost)
                {
                    continue;
                }
                const bool on = row == 0 ? g_lookBgOn[g_lookIndex][store] : g_lookSlideOn[g_lookIndex][store];
                RECT box = {};
                SceneCheckRect(card, d, row, &box);
                ScenePaintCheck(hdc, &box, on, ScenePressed(SCENE_HIT_CHECK, d, row, 0));
            }
        }
    }
    DeleteObject(line);
}

static void PaintSceneGrid(HDC hdc, const ScenesLayout* layout)
{
    const int viewW = layout->view.right - layout->view.left;
    const int viewH = layout->view.bottom - layout->view.top;
    if (viewW <= 0 || viewH <= 0)
    {
        return;
    }

    const int top = layout->view.top - g_lookScrollY;
    const int labelsRight = layout->view.left - 8;
    SaveDC(hdc);
    IntersectClipRect(hdc, layout->labelsLeft, layout->view.top, layout->view.right, layout->view.bottom);

    const bool bgLive = BackgroundLayerOn();
    const bool slideLive = g_liveArmed;
    const char* sample = nullptr;
    if (!bgLive && !slideLive)
    {
        sample = "Nothing live, showing a sample";
    }
    else if (!bgLive)
    {
        sample = "No background live, showing a sample";
    }
    else if (!slideLive)
    {
        sample = "No slide live, showing a sample";
    }
    if (sample == nullptr)
    {
        RECT preview = { layout->labelsLeft, top + LOOK_THUMB_TOP, labelsRight, top + LOOK_THUMB_TOP + LOOK_THUMB_H };
        SceneText(hdc, "Preview", preview, SCENE_FONT_BODY, LOOK_COLOR_INK_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    else
    {
        RECT preview = { layout->labelsLeft, top + LOOK_THUMB_TOP + 4, labelsRight, top + LOOK_THUMB_TOP + 22 };
        SceneText(hdc, "Preview", preview, SCENE_FONT_BODY, LOOK_COLOR_INK_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT note = { layout->labelsLeft, preview.bottom + 2, labelsRight, top + LOOK_CELL_TOP };
        SceneText(hdc, sample, note, SCENE_FONT_SMALL, LOOK_COLOR_INK_FAINT, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
    const char* rowNames[2] = { "Background", "Slide" };
    for (int row = 0; row < 2; ++row)
    {
        const int cellTop = top + LOOK_CELL_TOP + (row * LOOK_CELL_H);
        RECT label = { layout->labelsLeft, cellTop, labelsRight, cellTop + LOOK_CELL_H };
        SceneText(hdc, rowNames[row], label, SCENE_FONT_BODY, LOOK_COLOR_INK_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    const char* legend[2][2] = {
        { "Span", " stretches one picture across every display on the screen" },
        { "Mirror", " repeats the whole picture on each display" },
    };
    const int legendTop = top + LOOK_CARD_H + LOOK_LEGEND_GAP;
    int x = layout->labelsLeft;
    for (int i = 0; i < 2; ++i)
    {
        const int boldW = SceneTextWidth(hdc, legend[i][0], SCENE_FONT_SMALL_BOLD);
        const int restW = SceneTextWidth(hdc, legend[i][1], SCENE_FONT_SMALL);
        RECT bold = { x, legendTop, x + boldW, legendTop + LOOK_LEGEND_H };
        SceneText(hdc, legend[i][0], bold, SCENE_FONT_SMALL_BOLD, LOOK_COLOR_INK, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT rest = { bold.right, legendTop, bold.right + restW, legendTop + LOOK_LEGEND_H };
        SceneText(hdc, legend[i][1], rest, SCENE_FONT_SMALL, LOOK_COLOR_INK_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x = rest.right + 18;
    }
    RestoreDC(hdc, -1);

    const int blitW = min(viewW, layout->contentW - g_lookScrollX);
    const int blitH = min(viewH, LOOK_CARD_H - g_lookScrollY);
    if (layout->contentW <= 0 || blitW <= 0 || blitH <= 0)
    {
        return;
    }
    HDC cards = CreateCompatibleDC(hdc);
    HBITMAP canvas = CreateCompatibleBitmap(hdc, layout->contentW, LOOK_CARD_H);
    if (cards == nullptr || canvas == nullptr)
    {
        if (canvas != nullptr)
        {
            DeleteObject(canvas);
        }
        if (cards != nullptr)
        {
            DeleteDC(cards);
        }
        return;
    }
    HBITMAP oldCanvas = (HBITMAP)SelectObject(cards, canvas);
    PaintSceneCards(cards, layout, layout->contentW, LOOK_CARD_H);
    BitBlt(hdc, layout->view.left, layout->view.top, blitW, blitH, cards, g_lookScrollX, g_lookScrollY, SRCCOPY);
    SelectObject(cards, oldCanvas);
    DeleteObject(canvas);
    DeleteDC(cards);
}

static void PaintScenes(HDC hdc, const RECT* client)
{
    EnsureSceneFont();
    ScenesLayout layout = {};
    MeasureScenes(client, &layout);

    HBRUSH base = CreateSolidBrush(LOOK_COLOR_BASE);
    FillRect(hdc, client, base);
    DeleteObject(base);
    SceneRound(hdc, &layout.rail, 10.0f, LOOK_COLOR_SURFACE);
    SceneRound(hdc, &layout.main, 10.0f, LOOK_COLOR_SURFACE);

    PaintSceneRail(hdc, &layout);
    if (g_lookCount == 0)
    {
        SceneText(hdc, "No scenes yet", layout.main, SCENE_FONT_BODY, LOOK_COLOR_INK_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    if (g_lookIndex < 0 || g_lookIndex >= g_lookCount)
    {
        g_lookIndex = 0;
    }
    PaintSceneHeader(hdc, &layout);
    PaintSceneGrid(hdc, &layout);
}

static void ApplySceneTitleBar(HWND hwnd)
{
    // Attribute 20 is dark mode on Windows 10 20H1 and later; 19 is the same
    // switch on earlier Windows 10 builds. Caption and text colours need
    // Windows 11 and are ignored elsewhere.
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
    {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
    COLORREF caption = LOOK_COLOR_BASE;
    DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
    COLORREF text = LOOK_COLOR_INK;
    DwmSetWindowAttribute(hwnd, 36, &text, sizeof(text));
}

static void SetSceneMode(HWND hwnd, int screen, bool background, int mode)
{
    if (g_lookIndex < 0 || g_lookIndex >= g_lookCount || screen < 0 || screen >= g_lookScreenCount)
    {
        return;
    }
    if (ManageScreens_GetScreenLayout(screen) != SCREEN_LAYOUT_SPAN)
    {
        return;
    }
    int* current = background ? &g_lookBgMode[g_lookIndex][screen] : &g_lookSlideMode[g_lookIndex][screen];
    if (*current == mode)
    {
        return;
    }
    *current = mode;
    SaveLookEdit();
    InvalidateRect(hwnd, nullptr, FALSE);
}

static void ToggleSceneCheck(HWND hwnd, int display, bool background)
{
    if (g_lookIndex < 0 || g_lookIndex >= g_lookCount || display < 0 || display >= g_lookDisplayCount)
    {
        return;
    }
    if (g_lookDisplays[display].ghost)
    {
        return;
    }
    const int store = g_lookDisplays[display].store;
    const bool on = background ? g_lookBgOn[g_lookIndex][store] : g_lookSlideOn[g_lookIndex][store];
    SetLookColumnSwitch(g_lookIndex, display, background, !on);
    SaveLookEdit();
    InvalidateRect(hwnd, nullptr, FALSE);
}

static void EndSceneNameEdit(bool commit)
{
    if (g_sceneNameEdit == nullptr)
    {
        return;
    }
    HWND edit = g_sceneNameEdit;
    const int look = g_sceneNameEditing;
    // Cleared first: destroying the edit sends WM_KILLFOCUS back into here.
    g_sceneNameEdit = nullptr;
    g_sceneNameEditing = -1;
    if (commit)
    {
        char text[LOOK_NAME_MAX] = {};
        GetWindowTextA(edit, text, sizeof(text));
        RenameLookScene(look, text);
    }
    DestroyWindow(edit);
    if (g_scenesWindow != nullptr)
    {
        InvalidateRect(g_scenesWindow, nullptr, FALSE);
    }
}

LRESULT CALLBACK SceneNameEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS | CallWindowProcA(g_sceneNameEditProc, hwnd, uMsg, wParam, lParam);

        case WM_KEYDOWN:
            if (wParam == VK_RETURN)
            {
                EndSceneNameEdit(true);
                return 0;
            }
            if (wParam == VK_ESCAPE)
            {
                EndSceneNameEdit(false);
                return 0;
            }
            break;

        case WM_CHAR:
            if (wParam == '\r' || wParam == 27)
            {
                return 0;
            }
            break;

        case WM_KILLFOCUS:
        {
            const LRESULT result = CallWindowProcA(g_sceneNameEditProc, hwnd, uMsg, wParam, lParam);
            EndSceneNameEdit(true);
            return result;
        }
    }
    return CallWindowProcA(g_sceneNameEditProc, hwnd, uMsg, wParam, lParam);
}

static void BeginSceneNameEdit(int look)
{
    EndSceneNameEdit(true);
    if (g_scenesWindow == nullptr || look < 0 || look >= g_lookCount)
    {
        return;
    }
    RECT client = {};
    GetClientRect(g_scenesWindow, &client);
    ScenesLayout layout = {};
    MeasureScenes(&client, &layout);
    RECT row = {};
    SceneRowRect(&layout, look, &row);
    g_sceneNameEdit = CreateWindowExA(
        0, "EDIT", g_lookNames[look],
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        row.left + 8, row.top + 8, (row.right - row.left) - 16, (row.bottom - row.top) - 16,
        g_scenesWindow, (HMENU)ID_SCENE_NAME_EDIT, g_instance, nullptr
    );
    if (g_sceneNameEdit == nullptr)
    {
        return;
    }
    EnsureSceneFont();
    SendMessageA(g_sceneNameEdit, WM_SETFONT, (WPARAM)g_sceneFont, TRUE);
    SendMessageA(g_sceneNameEdit, EM_LIMITTEXT, LOOK_NAME_MAX - 1, 0);
    g_sceneNameEditProc = (WNDPROC)SetWindowLongPtrA(g_sceneNameEdit, GWLP_WNDPROC, (LONG_PTR)SceneNameEditProc);
    g_sceneNameEditing = look;
    SetFocus(g_sceneNameEdit);
    SendMessageA(g_sceneNameEdit, EM_SETSEL, 0, -1);
    InvalidateRect(g_scenesWindow, nullptr, FALSE);
}

static void SelectLookRow(int look)
{
    if (look < 0 || look >= g_lookCount)
    {
        return;
    }
    g_lookIndex = look;
    if (Store_Ready())
    {
        Store_SetInt("looks.view", g_lookIndex);
    }
    RefreshScenesWindow();
}

static void ActivateSceneHit(HWND hwnd, const SceneHit* hit)
{
    switch (hit->kind)
    {
        case SCENE_HIT_PLUS:
        {
            const int look = AddLookScene();
            if (look < 0)
            {
                char text[64] = {};
                wsprintfA(text, "You can have up to %d scenes.", LOOK_MAX);
                MessageBoxA(hwnd, text, "Scenes", MB_OK | MB_ICONINFORMATION);
                return;
            }
            BeginSceneNameEdit(look);
            return;
        }

        case SCENE_HIT_MINUS:
        {
            if (g_lookCount <= 0)
            {
                return;
            }
            char text[LOOK_NAME_MAX + 48] = {};
            wsprintfA(text, "Delete the scene \"%s\"?", g_lookNames[g_lookIndex]);
            if (Store_GetInt("general.confirm_delete", 1) == 0 ||
                MessageBoxA(hwnd, text, "Scenes", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) == IDOK)
            {
                DeleteLookScene(g_lookIndex);
            }
            return;
        }

        case SCENE_HIT_ROW:
            SelectLookRow(hit->index);
            return;

        case SCENE_HIT_APPLY:
            ApplyLiveLook();
            return;

        case SCENE_HIT_MODE:
            SetSceneMode(hwnd, hit->index, hit->row == 0, hit->value);
            return;

        case SCENE_HIT_CHECK:
            ToggleSceneCheck(hwnd, hit->index, hit->row == 0);
            return;
    }
}

LRESULT CALLBACK ScenesProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
            ApplySceneTitleBar(hwnd);
            EnsureSceneFont();
            LoadLookState();
            RebuildLookLayout();
            UpdateLookScroll(hwnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            UpdateLookScroll(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_HSCROLL:
            LookScrollBy(hwnd, SB_HORZ, LookScrollTrack(hwnd, SB_HORZ, wParam));
            return 0;

        case WM_VSCROLL:
            LookScrollBy(hwnd, SB_VERT, LookScrollTrack(hwnd, SB_VERT, wParam));
            return 0;

        case WM_MOUSEWHEEL:
        {
            const int delta = -GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 48;
            if ((GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) != 0)
            {
                LookScrollBy(hwnd, SB_VERT, delta);
            }
            else
            {
                LookScrollBy(hwnd, SB_HORZ, delta);
            }
            return 0;
        }

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT)
            {
                POINT pt = {};
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                const SceneHit hit = SceneHitTest(hwnd, pt.x, pt.y);
                if (hit.kind != SCENE_HIT_NONE && hit.kind != SCENE_HIT_ROW)
                {
                    SetCursor(LoadCursor(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        {
            const SceneHit hit = SceneHitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (hit.kind == SCENE_HIT_ROW && hit.index == g_sceneNameEditing)
            {
                return 0;
            }
            EndSceneNameEdit(true);
            if (uMsg == WM_LBUTTONDBLCLK && hit.kind == SCENE_HIT_ROW)
            {
                SelectLookRow(hit.index);
                BeginSceneNameEdit(hit.index);
                return 0;
            }
            if (hit.kind == SCENE_HIT_NONE)
            {
                return 0;
            }
            SetCapture(hwnd);
            g_scenePress = hit;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONUP:
        {
            if (g_scenePress.kind == SCENE_HIT_NONE)
            {
                return 0;
            }
            const SceneHit press = g_scenePress;
            g_scenePress = {};
            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            const SceneHit hit = SceneHitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (SceneSameHit(&hit, &press))
            {
                ActivateSceneHit(hwnd, &press);
            }
            return 0;
        }

        case WM_CAPTURECHANGED:
            if (g_scenePress.kind != SCENE_HIT_NONE)
            {
                g_scenePress = {};
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_CTLCOLOREDIT:
            if ((HWND)lParam == g_sceneNameEdit)
            {
                static HBRUSH editBrush = CreateSolidBrush(LOOK_COLOR_SUNKEN);
                HDC dc = (HDC)wParam;
                SetTextColor(dc, LOOK_COLOR_INK_BRIGHT);
                SetBkColor(dc, LOOK_COLOR_SUNKEN);
                return (LRESULT)editBrush;
            }
            break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int w = max(1, (int)client.right);
            const int h = max(1, (int)client.bottom);
            HDC memory = CreateCompatibleDC(hdc);
            HBITMAP canvas = CreateCompatibleBitmap(hdc, w, h);
            if (memory != nullptr && canvas != nullptr)
            {
                HBITMAP oldCanvas = (HBITMAP)SelectObject(memory, canvas);
                PaintScenes(memory, &client);
                BitBlt(hdc, 0, 0, w, h, memory, 0, 0, SRCCOPY);
                SelectObject(memory, oldCanvas);
            }
            if (canvas != nullptr)
            {
                DeleteObject(canvas);
            }
            if (memory != nullptr)
            {
                DeleteDC(memory);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            EndSceneNameEdit(true);
            SaveLookState();
            SceneDropLiveCache();
            g_scenePress = {};
            g_scenesWindow = nullptr;
            return 0;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void ShowScenesWindow()
{
    HWND host = ScreensHost();
    if (g_scenesWindow != nullptr && IsWindow(g_scenesWindow))
    {
        if (host != nullptr)
        {
            SetWindowLongPtrA(g_scenesWindow, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(host));
        }
        RefreshScenesWindow();
        ShowWindow(g_scenesWindow, SW_SHOW);
        SetForegroundWindow(g_scenesWindow);
        return;
    }

    HWND owner = host;

    LoadLookState();
    RebuildLookLayout();
    RECT probe = { 0, 0, 4000, 4000 };
    ScenesLayout layout = {};
    MeasureScenes(&probe, &layout);
    const int rows = g_lookCount > 0 ? g_lookCount : 1;
    int clientW = layout.view.left + layout.contentW + LOOK_MAIN_PAD_X + LOOK_GAP;
    int clientH = layout.view.top + layout.contentH + LOOK_MAIN_PAD_Y + LOOK_GAP;
    clientW = max(clientW, 900);
    clientH = max(clientH, max(460, LOOK_GAP + LOOK_HEADER_H + (rows * LOOK_ROW_H) + 12 + LOOK_GAP));
    RECT frame = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);

    RECT work = {};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    const int width = min((int)(frame.right - frame.left), (int)(work.right - work.left) - 40);
    const int height = min((int)(frame.bottom - frame.top), (int)(work.bottom - work.top) - 40);
    const int x = work.left + ((work.right - work.left) - width) / 2;
    const int y = work.top + ((work.bottom - work.top) - height) / 2;

    g_scenesWindow = CreateWindowExA(
        0,
        "ScenesWindow",
        "Scenes",
        WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL | WS_CLIPCHILDREN,
        x,
        y,
        width,
        height,
        owner,
        nullptr,
        g_instance,
        nullptr
    );
    ShowWindow(g_scenesWindow, SW_SHOW);
    UpdateWindow(g_scenesWindow);
}


void RefreshScreensPanel()
{
}

LRESULT CALLBACK BlankScreenProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            CREATESTRUCTA* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            if (!GpuOwnsOutputs())
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
            RECT minClient = { 0, 0, VIRTUAL_VIEW_MIN_W, VIRTUAL_VIEW_MIN_H };
            AdjustWindowRectEx(&minClient, GetWindowLongA(hwnd, GWL_STYLE), FALSE, 0);
            info->ptMinTrackSize.x = minClient.right - minClient.left;
            info->ptMinTrackSize.y = minClient.bottom - minClient.top;
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            if (App_OutputCleared() || App_LiveHold())
            {
                HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(hdc, &client, fill);
                DeleteObject(fill);
            }
            else if (!GpuOwnsOutputs())
            {
                LiveOutput_PaintHwnd(hwnd);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
        {
            const int id = static_cast<int>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
            for (int i = 0; i < BOOT_MAX_SCREENS; ++i)
            {
                if (g_screenWindows[i] == hwnd)
                {
                    g_screenWindows[i] = nullptr;
                }
            }
            if (id > 0)
            {
                ManageScreens_NoteVirtualClosed(id);
            }
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void PaintSplash(HDC hdc, const RECT* client)
{
    RECT fill = *client;
    HBRUSH brush = CreateSolidBrush(RGB(23, 21, 19));
    FillRect(hdc, &fill, brush);
    DeleteObject(brush);

    HFONT font = CreateFontA(
        -46,
        0,
        0,
        0,
        FW_LIGHT,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI"
    );
    HFONT previous = static_cast<HFONT>(SelectObject(hdc, font));
    SetBkMode(hdc, TRANSPARENT);

    const char* canvas = "Canvas";
    const char* you = "You";
    SIZE canvasSize = {};
    SIZE youSize = {};
    GetTextExtentPoint32A(hdc, canvas, 6, &canvasSize);
    GetTextExtentPoint32A(hdc, you, 3, &youSize);
    const int block = canvasSize.cy + youSize.cy - 6;
    const int x = 58;
    int y = (client->bottom - block) / 2;
    if (y < 0)
    {
        y = 0;
    }
    SetTextColor(hdc, RGB(156, 151, 142));
    TextOutA(hdc, x, y, canvas, 6);
    SetTextColor(hdc, RGB(247, 244, 239));
    TextOutA(hdc, x, y + canvasSize.cy - 6, you, 3);

    SelectObject(hdc, previous);
    DeleteObject(font);
}

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg)
    {
        case WM_SIZE:
            LayoutTopButtons(hwnd);
            LayoutHostPreview(hwnd);
            PresentCurrent();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            PaintSplash(hdc, &client);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_TIMER:
            if (wParam == ID_SPLASH_TIMER)
            {
                KillTimer(hwnd, ID_SPLASH_TIMER);
                ShowProgramView();
                if (g_programWindow != nullptr)
                {
                    ShowWindow(hwnd, SW_HIDE);
                }
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, ID_SPLASH_TIMER);
            g_mainWindow = nullptr;
            if (g_programWindow != nullptr)
            {
                DestroyWindow(g_programWindow);
            }
            ManageScreens_Close();
            PostQuitMessage(0);
            return 0;

        case WM_COMMAND:
            if (HandleAppMenuCommand(hwnd, LOWORD(wParam)))
            {
                return 0;
            }
            if (HandleScreensCommand(hwnd, LOWORD(wParam)))
            {
                return 0;
            }
            if (LOWORD(wParam) == IDM_FILE_EXIT)
            {
                DestroyWindow(hwnd);
            }
            return 0;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static bool IsTypingFocus()
{
    if (EditView_IsEditingText())
    {
        return true;
    }
    HWND focus = GetFocus();
    if (focus == nullptr)
    {
        return false;
    }
    char cls[64] = {};
    GetClassNameA(focus, cls, 64);
    return lstrcmpiA(cls, "Edit") == 0 || lstrcmpiA(cls, "ComboBox") == 0;
}

static bool FocusInProgram()
{
    HWND focus = GetFocus();
    if (focus == nullptr)
    {
        return g_programWindow != nullptr && IsWindowVisible(g_programWindow);
    }
    if (g_programWindow != nullptr && (focus == g_programWindow || IsChild(g_programWindow, focus)))
    {
        return true;
    }
    if (g_quickSlidesWindow != nullptr && (focus == g_quickSlidesWindow || IsChild(g_quickSlidesWindow, focus)))
    {
        return true;
    }
    return false;
}

static bool HandleProgramKeys(const MSG* msg)
{
    if (msg == nullptr || (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN))
    {
        return false;
    }
    if (g_programWindow == nullptr || !IsWindowVisible(g_programWindow))
    {
        return false;
    }
    if (!FocusInProgram())
    {
        return false;
    }

    const WPARAM key = msg->wParam;
    const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool typing = IsTypingFocus();

    if (control && key == VK_RETURN &&
        g_quickSlidesWindow != nullptr && IsWindowVisible(g_quickSlidesWindow))
    {
        AddQuickSlidesFromText(g_quickSlidesWindow);
        return true;
    }

    if (key == VK_ESCAPE)
    {
        if (g_quickSlidesWindow != nullptr && IsWindowVisible(g_quickSlidesWindow))
        {
            HideQuickSlides();
            return true;
        }
        if (!typing && EditView_HasSelection())
        {
            EditView_ClearSelection();
            return true;
        }
        if (!typing)
        {
            ClearSlideSelection();
            if (g_programMiddle != nullptr)
            {
                InvalidateRect(g_programMiddle, nullptr, FALSE);
            }
            return true;
        }
        return false;
    }

    if (typing)
    {
        const bool arrows =
            key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN;
        const bool screenMenu = g_programMirror != nullptr && GetFocus() == g_programMirror;
        const bool menuOpen = screenMenu &&
            SendMessage(g_programMirror, CB_GETDROPPEDSTATE, 0, 0) != 0;
        if (!(arrows && screenMenu && !menuOpen))
        {
            return false;
        }
    }

    if (control)
    {
        if (key == 'Z' || key == 'z')
        {
            if (shift)
            {
                if (History_Redo())
                {
                    RestoreAfterHistory();
                }
            }
            else if (History_Undo())
            {
                RestoreAfterHistory();
            }
            UpdateDocumentMenus();
            return true;
        }
        if (key == 'Y' || key == 'y')
        {
            if (History_Redo())
            {
                RestoreAfterHistory();
            }
            UpdateDocumentMenus();
            return true;
        }
        if (key == 'X' || key == 'x')
        {
            ProgramCut();
            UpdateDocumentMenus();
            return true;
        }
        if (key == 'C' || key == 'c')
        {
            ProgramCopy();
            UpdateDocumentMenus();
            return true;
        }
        if (key == 'V' || key == 'v')
        {
            ProgramPaste();
            UpdateDocumentMenus();
            return true;
        }
        if (key == 'D' || key == 'd')
        {
            ProgramDuplicate();
            UpdateDocumentMenus();
            return true;
        }
        if (key == 'A' || key == 'a')
        {
            SelectAllSlides();
            UpdateDocumentMenus();
            return true;
        }
        if (key == 'Q' || key == 'q')
        {
            SetProgramView(PROGRAM_VIEW_EDIT);
            ShowQuickSlides();
            return true;
        }
        return false;
    }

    if (key == VK_TAB)
    {
        SetProgramView(g_programView == PROGRAM_VIEW_SHOW ? PROGRAM_VIEW_EDIT : PROGRAM_VIEW_SHOW);
        return true;
    }

    if (key == VK_DELETE)
    {
        ProgramDelete();
        UpdateDocumentMenus();
        return true;
    }

    if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN)
    {
        if (g_programView == PROGRAM_VIEW_EDIT && EditView_HasSelection())
        {
            return EditView_HandleKey(key, shift, false);
        }
        if (g_programView == PROGRAM_VIEW_EDIT)
        {
            HighlightByArrow(key);
            return true;
        }
        GoLiveByArrow(key);
        return true;
    }

    if (key == VK_SPACE && g_programView == PROGRAM_VIEW_SHOW)
    {
        GoLiveStep(1);
        return true;
    }

    if (key == VK_HOME)
    {
        if (g_programView == PROGRAM_VIEW_EDIT)
        {
            HighlightFirstLast(true);
            return true;
        }
        GoLiveFirstLast(true);
        return true;
    }

    if (key == VK_END)
    {
        if (g_programView == PROGRAM_VIEW_EDIT)
        {
            HighlightFirstLast(false);
            return true;
        }
        GoLiveFirstLast(false);
        return true;
    }

    return false;
}

// ---- Preferences (beta) ---------------------------------------------------------

#define PREFS_W 900
#define PREFS_H 620
#define PREFS_TITLE_H 34
#define PREFS_NAV_W 200
#define PREFS_FOOT_H 61
#define PREFS_PAD_X 28
#define PREFS_ROW_H 66
#define PREFS_PATH_ROW_H 106
#define PREFS_BTN_H 32
#define PREFS_COLOR_DEV RGB(0xB7, 0x9A, 0x5A)

enum
{
    PREF_PAGE_GENERAL,
    PREF_PAGE_APPEARANCE,
    PREF_PAGE_FOLDERS,
    PREF_PAGE_PLAYBACK,
    PREF_PAGE_RECORDING,
    PREF_PAGE_COUNT
};

enum
{
    PREF_KIND_TOGGLE,
    PREF_KIND_DROPDOWN,
    PREF_KIND_SEG,
    PREF_KIND_PATH,
    PREF_KIND_SWATCHES
};

enum
{
    PR_REOPEN,
    PR_CONFIRM_DELETE,
    PR_UI_SIZE,
    PR_ACCENT,
    PR_ICON_PACK,
    PR_DIR_FILES,
    PR_DIR_ASSETS,
    PR_DIR_SCREENS,
    PR_SHOW_REMAINING,
    PR_SKIP_BACK,
    PR_SKIP_FORWARD,
    PR_VIDEO_FIT,
    PR_LOOP,
    PR_REC_ON,
    PR_REC_RATE,
    PR_REC_KEEP,
    PR_REC_PATH,
    PR_COUNT
};

struct PrefRowDef
{
    int page;
    int kind;
    // Under development: drawn greyed with a note and not editable.
    bool dev;
    const char* title;
    const char* desc;
    const char* options;
};

static const char* const PREF_SKIP_OPTIONS = "5 seconds|10 seconds|15 seconds|20 seconds|30 seconds|60 seconds";
static const int g_prefSkipValues[] = { 5, 10, 15, 20, 30, 60 };

static const PrefRowDef g_prefRows[PR_COUNT] = {
    { PREF_PAGE_GENERAL, PREF_KIND_TOGGLE, false, "Reopen last project on startup",
      "Opens the project that was open when the app last closed", nullptr },
    { PREF_PAGE_GENERAL, PREF_KIND_TOGGLE, false, "Confirm before deleting",
      "Asks before deleting more than one slide, or a scene", nullptr },
    { PREF_PAGE_APPEARANCE, PREF_KIND_SEG, true, "Interface size", nullptr, "90%|100%|125%|150%" },
    { PREF_PAGE_APPEARANCE, PREF_KIND_SWATCHES, true, "Accent color", nullptr, nullptr },
    { PREF_PAGE_APPEARANCE, PREF_KIND_DROPDOWN, true, "Icon pack", nullptr, "Default" },
    { PREF_PAGE_FOLDERS, PREF_KIND_PATH, false, "Project files", "Where projects are saved", nullptr },
    { PREF_PAGE_FOLDERS, PREF_KIND_PATH, false, "Assets", "Pictures, videos and audio imported into projects", nullptr },
    { PREF_PAGE_FOLDERS, PREF_KIND_PATH, false, "Screen configuration files", "Saved screen setups", nullptr },
    { PREF_PAGE_PLAYBACK, PREF_KIND_TOGGLE, false, "Show remaining time",
      "Right-hand time shows -0:42 instead of the duration", nullptr },
    { PREF_PAGE_PLAYBACK, PREF_KIND_DROPDOWN, false, "Skip back",
      "Used by the back button under the scrubber", PREF_SKIP_OPTIONS },
    { PREF_PAGE_PLAYBACK, PREF_KIND_DROPDOWN, false, "Skip forward",
      "Used by the forward button under the scrubber", PREF_SKIP_OPTIONS },
    { PREF_PAGE_PLAYBACK, PREF_KIND_SEG, false, "Default video fit",
      "How new full-screen videos fill the screen", "Stretch|Fit|Fill" },
    { PREF_PAGE_PLAYBACK, PREF_KIND_TOGGLE, true, "Loop videos by default", nullptr, nullptr },
    { PREF_PAGE_RECORDING, PREF_KIND_TOGGLE, false, "Record sessions",
      "Saves the app window while Canvas You is open", nullptr },
    { PREF_PAGE_RECORDING, PREF_KIND_DROPDOWN, false, "Capture rate",
      "How often a changed frame is saved", "Every 100 ms|Every 250 ms|Every 500 ms|Every 1 second" },
    { PREF_PAGE_RECORDING, PREF_KIND_DROPDOWN, false, "Keep sessions",
      "Older session folders are deleted", "Last 3 sessions|Last 5 sessions|Last 10 sessions" },
    { PREF_PAGE_RECORDING, PREF_KIND_PATH, false, "Save to",
      "Folder where each session is written", nullptr },
};

static const char* const g_prefPageNames[PREF_PAGE_COUNT] = {
    "General", "Appearance", "Folders", "Playback", "Recording"
};
static const char* const g_prefPageIcons[PREF_PAGE_COUNT] = {
    "pref_general", "pref_appearance", "pref_folders", "pref_playback", "pref_recording"
};
static const COLORREF g_prefSwatches[6] = {
    RGB(0x7A, 0x7A, 0x7A), RGB(0x4F, 0x6B, 0x80), RGB(0x5B, 0x7A, 0x5E),
    RGB(0x8A, 0x74, 0x45), RGB(0x8A, 0x4E, 0x4E), RGB(0x6E, 0x64, 0x88)
};

#define PREF_HOT_NONE 0
#define PREF_HOT_CLOSE 1
#define PREF_HOT_CANCEL 2
#define PREF_HOT_SAVE 3
#define PREF_HOT_NAV 100
#define PREF_HOT_BROWSE 200

HWND g_prefsBetaWindow = nullptr;
static int g_prefPage = PREF_PAGE_PLAYBACK;
static int g_prefValue[PR_COUNT] = {};
static char g_prefPath[PR_COUNT][MAX_PATH] = {};
static HWND g_prefEdit[PR_COUNT] = {};
static RECT g_prefCtl[PR_COUNT] = {};
static RECT g_prefSeg[PR_COUNT][6] = {};
static RECT g_prefBrowse[PR_COUNT] = {};
static RECT g_prefRowRc[PR_COUNT] = {};
static int g_prefHot = PREF_HOT_NONE;

static HFONT PrefFont(int px, int weight)
{
    static HFONT fonts[8] = {};
    static int keys[8] = {};
    const int key = px * 1000 + weight;
    for (int i = 0; i < 8; ++i)
    {
        if (fonts[i] != nullptr && keys[i] == key)
        {
            return fonts[i];
        }
        if (fonts[i] == nullptr)
        {
            fonts[i] = CreateFontA(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            keys[i] = key;
            return fonts[i];
        }
    }
    return ProgramUiFont();
}

static void PrefText(HDC hdc, const char* text, RECT rc, HFONT font, COLORREF color, UINT flags)
{
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rc, flags | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static int PrefTextWidth(HDC hdc, const char* text, HFONT font)
{
    HFONT old = (HFONT)SelectObject(hdc, font);
    SIZE size = {};
    GetTextExtentPoint32A(hdc, text, lstrlenA(text), &size);
    SelectObject(hdc, old);
    return size.cx;
}

// Copies option `index` of a '|' separated list.
static bool PrefOption(const char* options, int index, char* out, int outChars)
{
    out[0] = '\0';
    if (options == nullptr)
    {
        return false;
    }
    const char* p = options;
    for (int i = 0; i < index; ++i)
    {
        p = strchr(p, '|');
        if (p == nullptr)
        {
            return false;
        }
        p += 1;
    }
    const char* end = strchr(p, '|');
    int length = end != nullptr ? (int)(end - p) : lstrlenA(p);
    if (length >= outChars)
    {
        length = outChars - 1;
    }
    memcpy(out, p, length);
    out[length] = '\0';
    return true;
}

static int PrefOptionCount(const char* options)
{
    if (options == nullptr)
    {
        return 0;
    }
    int count = 1;
    for (const char* p = options; *p != '\0'; ++p)
    {
        if (*p == '|')
        {
            count += 1;
        }
    }
    return count;
}

static int PrefSkipIndex(int seconds)
{
    int best = 0;
    for (int i = 0; i < (int)(sizeof(g_prefSkipValues) / sizeof(g_prefSkipValues[0])); ++i)
    {
        if (abs(g_prefSkipValues[i] - seconds) < abs(g_prefSkipValues[best] - seconds))
        {
            best = i;
        }
    }
    return best;
}

static void LoadPrefValues()
{
    g_prefValue[PR_REOPEN] = Store_GetInt("general.reopen_last", 1) != 0;
    g_prefValue[PR_CONFIRM_DELETE] = Store_GetInt("general.confirm_delete", 1) != 0;
    g_prefValue[PR_UI_SIZE] = 1;
    g_prefValue[PR_ACCENT] = 0;
    g_prefValue[PR_ICON_PACK] = 0;
    Document_GetFilesDir(g_prefPath[PR_DIR_FILES], MAX_PATH);
    Document_GetAssetsDir(g_prefPath[PR_DIR_ASSETS], MAX_PATH);
    Document_GetScreensDir(g_prefPath[PR_DIR_SCREENS], MAX_PATH);
    g_prefValue[PR_SHOW_REMAINING] = g_showRemaining ? 1 : 0;
    g_prefValue[PR_SKIP_BACK] = PrefSkipIndex(SkipBackSeconds());
    g_prefValue[PR_SKIP_FORWARD] = PrefSkipIndex(SkipForwardSeconds());
    int fit = Store_GetInt("playback.video_fit", IMAGE_STRETCH);
    g_prefValue[PR_VIDEO_FIT] = (fit == IMAGE_FIT || fit == IMAGE_FILL) ? fit : IMAGE_STRETCH;
    // Videos always loop today.
    g_prefValue[PR_LOOP] = 1;

    char folder[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, folder, MAX_PATH);
    char* slash = strrchr(folder, '\\');
    if (slash != nullptr)
    {
        slash[1] = '\0';
    }
    lstrcatA(folder, "screenshots");
    Store_GetString("recording.path", g_prefPath[PR_REC_PATH], MAX_PATH, folder);

    char onText[8] = {};
    Store_GetString("recording.on", onText, sizeof(onText), "");
    if (onText[0] == '\0')
    {
        const DWORD attrs = GetFileAttributesA(g_prefPath[PR_REC_PATH]);
        g_prefValue[PR_REC_ON] = attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    else
    {
        g_prefValue[PR_REC_ON] = atoi(onText) != 0;
    }
    int rate = Store_GetInt("recording.rate", 0);
    if (rate < 0 || rate > 3)
    {
        rate = 0;
    }
    g_prefValue[PR_REC_RATE] = rate;
    int keep = Store_GetInt("recording.keep", 1);
    if (keep < 0 || keep > 2)
    {
        keep = 1;
    }
    g_prefValue[PR_REC_KEEP] = keep;
}

static void SavePrefValues()
{
    Store_SetInt("general.reopen_last", g_prefValue[PR_REOPEN] ? 1 : 0);
    Store_SetInt("general.confirm_delete", g_prefValue[PR_CONFIRM_DELETE] ? 1 : 0);

    const int dirs[3] = { PR_DIR_FILES, PR_DIR_ASSETS, PR_DIR_SCREENS };
    for (int i = 0; i < 3; ++i)
    {
        if (g_prefEdit[dirs[i]] != nullptr)
        {
            GetWindowTextA(g_prefEdit[dirs[i]], g_prefPath[dirs[i]], MAX_PATH);
        }
    }
    Document_SetFilesDir(g_prefPath[PR_DIR_FILES]);
    Document_SetAssetsDir(g_prefPath[PR_DIR_ASSETS]);
    Document_SetScreensDir(g_prefPath[PR_DIR_SCREENS]);
    Document_EnsureFolders();

    g_showRemaining = g_prefValue[PR_SHOW_REMAINING] != 0;
    Store_SetInt("playback.show_remaining", g_showRemaining ? 1 : 0);
    Store_SetInt("playback.skip_back", g_prefSkipValues[g_prefValue[PR_SKIP_BACK]]);
    Store_SetInt("playback.skip_forward", g_prefSkipValues[g_prefValue[PR_SKIP_FORWARD]]);
    Store_SetInt("playback.video_fit", g_prefValue[PR_VIDEO_FIT]);

    if (g_prefEdit[PR_REC_PATH] != nullptr)
    {
        GetWindowTextA(g_prefEdit[PR_REC_PATH], g_prefPath[PR_REC_PATH], MAX_PATH);
    }
    Store_SetInt("recording.on", g_prefValue[PR_REC_ON] ? 1 : 0);
    Store_SetInt("recording.rate", g_prefValue[PR_REC_RATE]);
    Store_SetInt("recording.keep", g_prefValue[PR_REC_KEEP]);
    Store_SetString("recording.path", g_prefPath[PR_REC_PATH]);
    if (g_prefValue[PR_REC_ON] && g_prefPath[PR_REC_PATH][0] != '\0')
    {
        SHCreateDirectoryExA(nullptr, g_prefPath[PR_REC_PATH], nullptr);
    }
    HANDLE reload = OpenEventA(EVENT_MODIFY_STATE, FALSE, RECORDER_RELOAD_EVENT);
    if (reload != nullptr)
    {
        SetEvent(reload);
        CloseHandle(reload);
    }

    UpdateMediaTransportUi();
    HWND skipButtons[4] = { g_programVideoBack, g_programVideoFwd, g_programAudioBack, g_programAudioFwd };
    for (int i = 0; i < 4; ++i)
    {
        if (skipButtons[i] != nullptr)
        {
            InvalidateRect(skipButtons[i], nullptr, FALSE);
        }
    }
}

static void LayoutPrefs(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    const int right = PREFS_W - PREFS_PAD_X;
    int y = PREFS_TITLE_H + 22 + 27 + 14;
    for (int i = 0; i < PR_COUNT; ++i)
    {
        const PrefRowDef* row = &g_prefRows[i];
        SetRectEmpty(&g_prefRowRc[i]);
        SetRectEmpty(&g_prefCtl[i]);
        SetRectEmpty(&g_prefBrowse[i]);
        const bool shown = row->page == g_prefPage;
        if (g_prefEdit[i] != nullptr)
        {
            ShowWindow(g_prefEdit[i], shown ? SW_SHOW : SW_HIDE);
        }
        if (!shown)
        {
            continue;
        }
        const int h = row->kind == PREF_KIND_PATH ? PREFS_PATH_ROW_H : PREFS_ROW_H;
        SetRect(&g_prefRowRc[i], PREFS_NAV_W + PREFS_PAD_X, y, right, y + h);
        const int cy = y + PREFS_ROW_H / 2;
        switch (row->kind)
        {
            case PREF_KIND_TOGGLE:
                SetRect(&g_prefCtl[i], right - 40, cy - 11, right, cy + 11);
                break;
            case PREF_KIND_DROPDOWN:
            {
                int w = 110;
                const int count = PrefOptionCount(row->options);
                for (int o = 0; o < count; ++o)
                {
                    char text[64] = {};
                    PrefOption(row->options, o, text, sizeof(text));
                    const int need = PrefTextWidth(hdc, text, PrefFont(13, FW_NORMAL)) + 44;
                    if (need > w)
                    {
                        w = need;
                    }
                }
                SetRect(&g_prefCtl[i], right - w, cy - 16, right, cy + 16);
                break;
            }
            case PREF_KIND_SEG:
            {
                const int count = PrefOptionCount(row->options);
                int widths[6] = {};
                int total = 3 * 2 + (count - 1) * 2;
                for (int o = 0; o < count && o < 6; ++o)
                {
                    char text[32] = {};
                    PrefOption(row->options, o, text, sizeof(text));
                    widths[o] = PrefTextWidth(hdc, text, PrefFont(13, FW_NORMAL)) + 28;
                    total += widths[o];
                }
                SetRect(&g_prefCtl[i], right - total, cy - 16, right, cy + 16);
                int x = g_prefCtl[i].left + 3;
                for (int o = 0; o < count && o < 6; ++o)
                {
                    SetRect(&g_prefSeg[i][o], x, g_prefCtl[i].top + 3, x + widths[o], g_prefCtl[i].bottom - 3);
                    x += widths[o] + 2;
                }
                break;
            }
            case PREF_KIND_SWATCHES:
            {
                const int total = 6 * 22 + 5 * 8;
                SetRect(&g_prefCtl[i], right - total, cy - 11, right, cy + 11);
                for (int o = 0; o < 6; ++o)
                {
                    const int x = g_prefCtl[i].left + o * 30;
                    SetRect(&g_prefSeg[i][o], x, cy - 11, x + 22, cy + 11);
                }
                break;
            }
            case PREF_KIND_PATH:
            {
                const int top = y + 13 + 40 + 8;
                const int browseW = 92;
                SetRect(&g_prefBrowse[i], right - browseW, top, right, top + PREFS_BTN_H);
                SetRect(&g_prefCtl[i], PREFS_NAV_W + PREFS_PAD_X, top, right - browseW - 8, top + PREFS_BTN_H);
                if (g_prefEdit[i] != nullptr)
                {
                    MoveWindow(g_prefEdit[i], g_prefCtl[i].left + 10, top + 8,
                               g_prefCtl[i].right - g_prefCtl[i].left - 20, 17, TRUE);
                }
                break;
            }
        }
        y += h;
    }
    ReleaseDC(hwnd, hdc);
}

static void PaintPrefToggle(HDC hdc, const RECT* rc, bool on, bool disabled)
{
    COLORREF track = on ? LOOK_COLOR_ACCENT : RGB(0x4A, 0x4A, 0x4A);
    COLORREF knob = on ? LOOK_COLOR_ACCENT_INK : RGB(0xBD, 0xBD, 0xBD);
    if (disabled)
    {
        track = on ? RGB(0x48, 0x48, 0x48) : RGB(0x33, 0x33, 0x33);
        knob = on ? RGB(0x2A, 0x2A, 0x2A) : RGB(0x55, 0x55, 0x55);
    }
    SceneRound(hdc, rc, 11.0f, track);
    const int x = on ? rc->left + 21 : rc->left + 3;
    RECT dot = { x, rc->top + 3, x + 16, rc->top + 19 };
    SceneRound(hdc, &dot, 8.0f, knob);
}

static void PaintPrefChevron(HDC hdc, int cx, int cy, COLORREF color)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)), 1.6f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    Gdiplus::PointF points[3] = {
        Gdiplus::PointF(cx - 4.5f, cy - 2.0f),
        Gdiplus::PointF((float)cx, cy + 2.5f),
        Gdiplus::PointF(cx + 4.5f, cy - 2.0f)
    };
    g.DrawLines(&pen, points, 3);
}

static void PaintPrefButton(HDC hdc, const RECT* rc, const char* text, bool primary, bool hot)
{
    if (primary)
    {
        SceneRound(hdc, rc, 6.0f, hot ? RGB(0xFF, 0xFF, 0xFF) : LOOK_COLOR_ACCENT);
        PrefText(hdc, text, *rc, PrefFont(13, FW_SEMIBOLD), LOOK_COLOR_ACCENT_INK, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    SceneRound(hdc, rc, 6.0f, hot ? LOOK_COLOR_RAISED_HI : LOOK_COLOR_RAISED, PROGRAM_COLOR_CONTROL_EDGE);
    PrefText(hdc, text, *rc, PrefFont(13, FW_NORMAL), LOOK_COLOR_INK_BRIGHT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void PrefsFooterButtons(RECT* cancel, RECT* save)
{
    const int top = PREFS_H - PREFS_FOOT_H + 1 + 14;
    SetRect(save, PREFS_W - 20 - 62, top, PREFS_W - 20, top + PREFS_BTN_H);
    SetRect(cancel, save->left - 8 - 72, top, save->left - 8, top + PREFS_BTN_H);
}

static void PrefsNavRect(int page, RECT* out)
{
    const int top = PREFS_TITLE_H + 8 + page * 37;
    SetRect(out, 10, top, PREFS_NAV_W - 10, top + 35);
}

static void PrefsCloseRect(RECT* out)
{
    SetRect(out, PREFS_W - PROGRAM_CAPTION_W, 0, PREFS_W, PREFS_TITLE_H);
}

static void PaintPrefRow(HDC hdc, int i, bool last)
{
    const PrefRowDef* row = &g_prefRows[i];
    const RECT* rc = &g_prefRowRc[i];
    const bool dev = row->dev;
    RECT title = { rc->left, rc->top + 13, rc->right - 200, rc->top + 33 };
    if (row->kind == PREF_KIND_PATH)
    {
        title.right = rc->right;
    }
    PrefText(hdc, row->title, title, PrefFont(14, FW_NORMAL), dev ? LOOK_COLOR_INK_FAINT : LOOK_COLOR_INK_BRIGHT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT desc = { rc->left, rc->top + 34, title.right, rc->top + 52 };
    if (dev)
    {
        const int dotY = (desc.top + desc.bottom) / 2;
        RECT dot = { desc.left, dotY - 3, desc.left + 6, dotY + 3 };
        SceneRound(hdc, &dot, 3.0f, PREFS_COLOR_DEV);
        desc.left += 12;
        PrefText(hdc, "This feature is under development", desc, PrefFont(12, FW_NORMAL), PREFS_COLOR_DEV,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    else if (row->desc != nullptr)
    {
        PrefText(hdc, row->desc, desc, PrefFont(12, FW_NORMAL), LOOK_COLOR_INK_DIM,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    const RECT* ctl = &g_prefCtl[i];
    switch (row->kind)
    {
        case PREF_KIND_TOGGLE:
            PaintPrefToggle(hdc, ctl, g_prefValue[i] != 0, dev);
            break;
        case PREF_KIND_DROPDOWN:
        {
            SceneRound(hdc, ctl, 6.0f, dev ? RGB(0x32, 0x32, 0x32) : LOOK_COLOR_RAISED,
                       dev ? RGB(0x3A, 0x3A, 0x3A) : PROGRAM_COLOR_CONTROL_EDGE);
            char text[64] = {};
            PrefOption(row->options, g_prefValue[i], text, sizeof(text));
            RECT textRc = { ctl->left + 11, ctl->top, ctl->right - 26, ctl->bottom };
            PrefText(hdc, text, textRc, PrefFont(13, FW_NORMAL), dev ? LOOK_COLOR_INK_FAINT : LOOK_COLOR_INK_BRIGHT,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            PaintPrefChevron(hdc, ctl->right - 15, (ctl->top + ctl->bottom) / 2,
                             dev ? LOOK_COLOR_INK_FAINT : LOOK_COLOR_INK_DIM);
            break;
        }
        case PREF_KIND_SEG:
        {
            SceneRound(hdc, ctl, 6.0f, LOOK_COLOR_SUNKEN, RGB(0x33, 0x33, 0x33));
            const int count = PrefOptionCount(row->options);
            for (int o = 0; o < count && o < 6; ++o)
            {
                const bool on = g_prefValue[i] == o;
                if (on)
                {
                    SceneRound(hdc, &g_prefSeg[i][o], 4.0f, dev ? RGB(0x30, 0x30, 0x30) : LOOK_COLOR_RAISED_HI);
                }
                char text[32] = {};
                PrefOption(row->options, o, text, sizeof(text));
                COLORREF ink = on ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK_DIM;
                if (dev)
                {
                    ink = on ? LOOK_COLOR_INK_DIM : LOOK_COLOR_INK_FAINT;
                }
                PrefText(hdc, text, g_prefSeg[i][o], PrefFont(13, FW_NORMAL), ink, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            break;
        }
        case PREF_KIND_SWATCHES:
            for (int o = 0; o < 6; ++o)
            {
                RECT dot = g_prefSeg[i][o];
                const COLORREF c = g_prefSwatches[o];
                // Greyed: blend each swatch halfway toward the panel colour.
                const COLORREF dim = RGB((GetRValue(c) + 0x2E) / 2, (GetGValue(c) + 0x2E) / 2, (GetBValue(c) + 0x2E) / 2);
                if (g_prefValue[i] == o)
                {
                    RECT ring = dot;
                    InflateRect(&ring, 3, 3);
                    SceneRound(hdc, &ring, 14.0f, RGB(0x6A, 0x6A, 0x6A));
                    InflateRect(&ring, -2, -2);
                    SceneRound(hdc, &ring, 12.0f, LOOK_COLOR_SURFACE);
                }
                SceneRound(hdc, &dot, 11.0f, dev ? dim : c);
            }
            break;
        case PREF_KIND_PATH:
        {
            SceneRound(hdc, ctl, 6.0f, LOOK_COLOR_SUNKEN, RGB(0x33, 0x33, 0x33));
            if (g_prefEdit[i] == nullptr)
            {
                RECT textRc = { ctl->left + 10, ctl->top, ctl->right - 10, ctl->bottom };
                PrefText(hdc, g_prefPath[i], textRc, PrefFont(13, FW_NORMAL), dev ? LOOK_COLOR_INK_FAINT : LOOK_COLOR_INK,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
            }
            if (dev)
            {
                SceneRound(hdc, &g_prefBrowse[i], 6.0f, RGB(0x32, 0x32, 0x32), RGB(0x3A, 0x3A, 0x3A));
                PrefText(hdc, "Browse...", g_prefBrowse[i], PrefFont(13, FW_NORMAL), LOOK_COLOR_INK_FAINT,
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            else
            {
                PaintPrefButton(hdc, &g_prefBrowse[i], "Browse...", false, g_prefHot == PREF_HOT_BROWSE + i);
            }
            break;
        }
    }

    if (!last)
    {
        RECT line = { rc->left, rc->bottom - 1, rc->right, rc->bottom };
        HBRUSH brush = CreateSolidBrush(PROGRAM_COLOR_LINE);
        FillRect(hdc, &line, brush);
        DeleteObject(brush);
    }
}

static void PaintPrefs(HWND hwnd, HDC target)
{
    HDC hdc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, PREFS_W, PREFS_H);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(hdc, bitmap);
    SetBkMode(hdc, TRANSPARENT);

    RECT all = { 0, 0, PREFS_W, PREFS_H };
    HBRUSH base = CreateSolidBrush(LOOK_COLOR_BASE);
    FillRect(hdc, &all, base);
    DeleteObject(base);

    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath mark;
        const float top = (PREFS_TITLE_H - 16) / 2.0f;
        mark.AddArc(12.0f, top, 8.0f, 8.0f, 180.0f, 90.0f);
        mark.AddArc(20.0f, top, 8.0f, 8.0f, 270.0f, 90.0f);
        mark.AddArc(20.0f, top + 8.0f, 8.0f, 8.0f, 0.0f, 90.0f);
        mark.AddArc(12.0f, top + 8.0f, 8.0f, 8.0f, 90.0f, 90.0f);
        mark.CloseFigure();
        Gdiplus::LinearGradientBrush fill(
            Gdiplus::PointF(12.0f, top),
            Gdiplus::PointF(28.0f, top + 16.0f),
            Gdiplus::Color(255, 0x5A, 0x5A, 0x5A),
            Gdiplus::Color(255, 0x3A, 0x3A, 0x3A)
        );
        g.FillPath(&fill, &mark);
    }
    RECT titleRc = { 38, 0, PREFS_W - PROGRAM_CAPTION_W, PREFS_TITLE_H };
    PrefText(hdc, "Preferences", titleRc, PrefFont(12, FW_NORMAL),
             GetActiveWindow() == hwnd ? LOOK_COLOR_INK : LOOK_COLOR_INK_DIM, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT closeRc = {};
    PrefsCloseRect(&closeRc);
    if (g_prefHot == PREF_HOT_CLOSE)
    {
        HBRUSH red = CreateSolidBrush(RGB(0xC4, 0x2B, 0x1C));
        FillRect(hdc, &closeRc, red);
        DeleteObject(red);
    }
    {
        HFONT old = (HFONT)SelectObject(hdc, ProgramCaptionGlyphFont());
        SetTextColor(hdc, g_prefHot == PREF_HOT_CLOSE ? RGB(0xFF, 0xFF, 0xFF) : LOOK_COLOR_INK_DIM);
        DrawTextW(hdc, L"\xE8BB", -1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, old);
    }

    for (int page = 0; page < PREF_PAGE_COUNT; ++page)
    {
        RECT item = {};
        PrefsNavRect(page, &item);
        const bool on = page == g_prefPage;
        if (on)
        {
            SceneRound(hdc, &item, 7.0f, LOOK_COLOR_RAISED);
            RECT bar = { item.left, item.top + 9, item.left + 3, item.bottom - 9 };
            SceneRound(hdc, &bar, 1.5f, LOOK_COLOR_ACCENT);
        }
        else if (g_prefHot == PREF_HOT_NAV + page)
        {
            SceneRound(hdc, &item, 7.0f, RGB(0x30, 0x30, 0x30));
        }
        SvgIcon_Draw(hdc, g_prefPageIcons[page], item.left + 12 + 8, (item.top + item.bottom) / 2, 17,
                     on ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK_DIM);
        RECT text = { item.left + 12 + 17 + 10, item.top, item.right - 8, item.bottom };
        PrefText(hdc, g_prefPageNames[page], text, PrefFont(13, FW_NORMAL), on ? LOOK_COLOR_INK_BRIGHT : LOOK_COLOR_INK,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    RECT content = { PREFS_NAV_W, PREFS_TITLE_H, PREFS_W + 12, PREFS_H };
    SceneRound(hdc, &content, 12.0f, LOOK_COLOR_SURFACE);
    RECT h1 = { PREFS_NAV_W + PREFS_PAD_X, PREFS_TITLE_H + 22, PREFS_W - PREFS_PAD_X, PREFS_TITLE_H + 22 + 27 };
    PrefText(hdc, g_prefPageNames[g_prefPage], h1, PrefFont(20, FW_SEMIBOLD), LOOK_COLOR_INK_BRIGHT,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int lastRow = -1;
    for (int i = 0; i < PR_COUNT; ++i)
    {
        if (g_prefRows[i].page == g_prefPage)
        {
            lastRow = i;
        }
    }
    for (int i = 0; i < PR_COUNT; ++i)
    {
        if (g_prefRows[i].page == g_prefPage)
        {
            PaintPrefRow(hdc, i, i == lastRow);
        }
    }

    RECT foot = { 0, PREFS_H - PREFS_FOOT_H, PREFS_W, PREFS_H };
    HBRUSH surface = CreateSolidBrush(LOOK_COLOR_SURFACE);
    FillRect(hdc, &foot, surface);
    DeleteObject(surface);
    RECT footLine = { 0, foot.top, PREFS_W, foot.top + 1 };
    HBRUSH line = CreateSolidBrush(PROGRAM_COLOR_LINE);
    FillRect(hdc, &footLine, line);
    DeleteObject(line);
    RECT cancel = {};
    RECT save = {};
    PrefsFooterButtons(&cancel, &save);
    PaintPrefButton(hdc, &cancel, "Cancel", false, g_prefHot == PREF_HOT_CANCEL);
    PaintPrefButton(hdc, &save, "Save", true, g_prefHot == PREF_HOT_SAVE);

    BitBlt(target, 0, 0, PREFS_W, PREFS_H, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
}

static int PrefsHotAt(POINT pt)
{
    RECT rc = {};
    PrefsCloseRect(&rc);
    if (PtInRect(&rc, pt))
    {
        return PREF_HOT_CLOSE;
    }
    RECT cancel = {};
    RECT save = {};
    PrefsFooterButtons(&cancel, &save);
    if (PtInRect(&cancel, pt))
    {
        return PREF_HOT_CANCEL;
    }
    if (PtInRect(&save, pt))
    {
        return PREF_HOT_SAVE;
    }
    for (int page = 0; page < PREF_PAGE_COUNT; ++page)
    {
        PrefsNavRect(page, &rc);
        if (PtInRect(&rc, pt))
        {
            return PREF_HOT_NAV + page;
        }
    }
    for (int i = 0; i < PR_COUNT; ++i)
    {
        if (g_prefRows[i].page == g_prefPage && !g_prefRows[i].dev && PtInRect(&g_prefBrowse[i], pt))
        {
            return PREF_HOT_BROWSE + i;
        }
    }
    return PREF_HOT_NONE;
}

static void ShowPrefDropdown(HWND hwnd, int i)
{
    const PrefRowDef* row = &g_prefRows[i];
    HMENU menu = CreatePopupMenu();
    const int count = PrefOptionCount(row->options);
    for (int o = 0; o < count; ++o)
    {
        char text[64] = {};
        PrefOption(row->options, o, text, sizeof(text));
        AppendMenuA(menu, MF_STRING | (g_prefValue[i] == o ? MF_CHECKED : 0), o + 1, text);
    }
    POINT pt = { g_prefCtl[i].left, g_prefCtl[i].bottom + 2 };
    ClientToScreen(hwnd, &pt);
    const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
                                   pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
    if (cmd > 0)
    {
        g_prefValue[i] = cmd - 1;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

static void ClickPrefs(HWND hwnd, POINT pt)
{
    const int hot = PrefsHotAt(pt);
    if (hot == PREF_HOT_CLOSE || hot == PREF_HOT_CANCEL)
    {
        DestroyWindow(hwnd);
        return;
    }
    if (hot == PREF_HOT_SAVE)
    {
        SavePrefValues();
        DestroyWindow(hwnd);
        return;
    }
    if (hot >= PREF_HOT_NAV && hot < PREF_HOT_NAV + PREF_PAGE_COUNT)
    {
        g_prefPage = hot - PREF_HOT_NAV;
        LayoutPrefs(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
    if (hot >= PREF_HOT_BROWSE && hot < PREF_HOT_BROWSE + PR_COUNT)
    {
        const int i = hot - PREF_HOT_BROWSE;
        char path[MAX_PATH] = {};
        if (BrowseForFolder(hwnd, path, MAX_PATH))
        {
            lstrcpynA(g_prefPath[i], path, MAX_PATH);
            if (g_prefEdit[i] != nullptr)
            {
                SetWindowTextA(g_prefEdit[i], path);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }
    for (int i = 0; i < PR_COUNT; ++i)
    {
        const PrefRowDef* row = &g_prefRows[i];
        if (row->page != g_prefPage || row->dev)
        {
            continue;
        }
        if (row->kind == PREF_KIND_TOGGLE && PtInRect(&g_prefCtl[i], pt))
        {
            g_prefValue[i] = !g_prefValue[i];
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        if (row->kind == PREF_KIND_DROPDOWN && PtInRect(&g_prefCtl[i], pt))
        {
            ShowPrefDropdown(hwnd, i);
            return;
        }
        if (row->kind == PREF_KIND_SEG)
        {
            const int count = PrefOptionCount(row->options);
            for (int o = 0; o < count && o < 6; ++o)
            {
                if (PtInRect(&g_prefSeg[i][o], pt))
                {
                    g_prefValue[i] = o;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return;
                }
            }
        }
        if (row->kind == PREF_KIND_PATH && g_prefEdit[i] != nullptr && PtInRect(&g_prefCtl[i], pt))
        {
            SetFocus(g_prefEdit[i]);
            return;
        }
    }
}

LRESULT CALLBACK PreferencesBetaProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_NCCALCSIZE:
            if (wParam)
            {
                return 0;
            }
            break;

        case WM_NCHITTEST:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            RECT closeRc = {};
            PrefsCloseRect(&closeRc);
            if (pt.y >= 0 && pt.y < PREFS_TITLE_H && !PtInRect(&closeRc, pt))
            {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintPrefs(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_ACTIVATE:
            InvalidateRect(hwnd, nullptr, FALSE);
            break;

        case WM_MOUSEMOVE:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int hot = PrefsHotAt(pt);
            if (hot != g_prefHot)
            {
                g_prefHot = hot;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&track);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (g_prefHot != PREF_HOT_NONE)
            {
                g_prefHot = PREF_HOT_NONE;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN:
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClickPrefs(hwnd, pt);
            return 0;
        }

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT)
            {
                POINT pt = {};
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                bool hand = PrefsHotAt(pt) != PREF_HOT_NONE;
                for (int i = 0; i < PR_COUNT && !hand; ++i)
                {
                    const PrefRowDef* row = &g_prefRows[i];
                    if (row->page == g_prefPage && !row->dev && row->kind != PREF_KIND_PATH &&
                        PtInRect(&g_prefCtl[i], pt))
                    {
                        hand = true;
                    }
                }
                SetCursor(LoadCursor(nullptr, hand ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_CTLCOLOREDIT:
        {
            static HBRUSH sunken = nullptr;
            if (sunken == nullptr)
            {
                sunken = CreateSolidBrush(LOOK_COLOR_SUNKEN);
            }
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, LOOK_COLOR_SUNKEN);
            SetTextColor(hdc, LOOK_COLOR_INK);
            return (LRESULT)sunken;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            for (int i = 0; i < PR_COUNT; ++i)
            {
                g_prefEdit[i] = nullptr;
            }
            g_prefHot = PREF_HOT_NONE;
            g_prefsBetaWindow = nullptr;
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void ShowPreferencesBeta(HWND owner)
{
    if (g_prefsBetaWindow != nullptr)
    {
        SetForegroundWindow(g_prefsBetaWindow);
        return;
    }
    EnableDarkPopupMenus();
    LoadPrefValues();

    RECT ownerRc = {};
    if (owner == nullptr || !GetWindowRect(owner, &ownerRc))
    {
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &ownerRc, 0);
    }
    const int x = ownerRc.left + ((ownerRc.right - ownerRc.left) - PREFS_W) / 2;
    const int y = ownerRc.top + ((ownerRc.bottom - ownerRc.top) - PREFS_H) / 2;
    g_prefsBetaWindow = CreateWindowExA(
        0,
        "PreferencesBetaWindow",
        "Preferences",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x > 0 ? x : 0,
        y > 0 ? y : 0,
        PREFS_W,
        PREFS_H,
        owner,
        nullptr,
        g_instance,
        nullptr
    );
    if (g_prefsBetaWindow == nullptr)
    {
        return;
    }
    ApplySceneTitleBar(g_prefsBetaWindow);
    // Windows 11 rounds the corners of a window whose frame is drawn by the app only when asked.
    const DWORD round = 2;
    DwmSetWindowAttribute(g_prefsBetaWindow, 33, &round, sizeof(round));

    const int dirs[4] = { PR_DIR_FILES, PR_DIR_ASSETS, PR_DIR_SCREENS, PR_REC_PATH };
    for (int d = 0; d < 4; ++d)
    {
        const int i = dirs[d];
        g_prefEdit[i] = CreateWindowExA(0, "EDIT", g_prefPath[i], WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP,
                                        0, 0, 10, 10, g_prefsBetaWindow, nullptr, g_instance, nullptr);
        SendMessageA(g_prefEdit[i], WM_SETFONT, (WPARAM)PrefFont(13, FW_NORMAL), FALSE);
    }
    SetWindowPos(g_prefsBetaWindow, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    LayoutPrefs(g_prefsBetaWindow);
    ShowWindow(g_prefsBetaWindow, SW_SHOW);
    UpdateWindow(g_prefsBetaWindow);
}

int RunApp(HINSTANCE hInstance, int nCmdShow)
{
    g_instance = hInstance;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    App_OnStart();

    WNDCLASSA mainClass = {};
    mainClass.lpfnWndProc = WindowProc;
    mainClass.hInstance = hInstance;
    mainClass.lpszClassName = "MyCppWindow";
    mainClass.hbrBackground = CreateSolidBrush(RGB(23, 21, 19));
    mainClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    mainClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&mainClass);


    WNDCLASSA scenesClass = {};
    scenesClass.lpfnWndProc = ScenesProc;
    scenesClass.hInstance = hInstance;
    scenesClass.lpszClassName = "ScenesWindow";
    scenesClass.hbrBackground = CreateSolidBrush(PROGRAM_COLOR_FRAME);
    scenesClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    scenesClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    RegisterClassA(&scenesClass);


    WNDCLASSA screenClass = {};
    screenClass.lpfnWndProc = BlankScreenProc;
    screenClass.hInstance = hInstance;
    screenClass.lpszClassName = "BlankScreenWindow";
    screenClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    screenClass.style = CS_HREDRAW | CS_VREDRAW;
    screenClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&screenClass);

    WNDCLASSA hostClass = {};
    hostClass.lpfnWndProc = HostPreviewProc;
    hostClass.hInstance = hInstance;
    hostClass.lpszClassName = "HostPreviewWindow";
    hostClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&hostClass);

    WNDCLASSA programClass = {};
    programClass.lpfnWndProc = ProgramWindowProc;
    programClass.hInstance = hInstance;
    programClass.lpszClassName = "ProgramWindow";
    programClass.hbrBackground = CreateSolidBrush(LOOK_COLOR_BASE);
    programClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    programClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&programClass);

    WNDCLASSA programSectionClass = {};
    programSectionClass.lpfnWndProc = ProgramSectionProc;
    programSectionClass.hInstance = hInstance;
    programSectionClass.lpszClassName = "ProgramSectionWindow";
    programSectionClass.hbrBackground = CreateSolidBrush(LOOK_COLOR_BASE);
    programSectionClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    programSectionClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&programSectionClass);

    WNDCLASSA programModeClass = {};
    programModeClass.lpfnWndProc = ProgramModeButtonProc;
    programModeClass.hInstance = hInstance;
    programModeClass.lpszClassName = "ProgramModeButton";
    programModeClass.hbrBackground = CreateSolidBrush(PROGRAM_COLOR_TOP);
    programModeClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    programModeClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&programModeClass);

    WNDCLASSA importButtonClass = {};
    importButtonClass.lpfnWndProc = ImportButtonProc;
    importButtonClass.hInstance = hInstance;
    importButtonClass.lpszClassName = "ProgramImportButton";
    importButtonClass.hbrBackground = CreateSolidBrush(PROGRAM_COLOR_TOP);
    importButtonClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    importButtonClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&importButtonClass);

    WNDCLASSA importProgressClass = {};
    importProgressClass.lpfnWndProc = ImportProgressProc;
    importProgressClass.hInstance = hInstance;
    importProgressClass.lpszClassName = "ImportProgressWindow";
    importProgressClass.hbrBackground = CreateSolidBrush(PROGRAM_COLOR_FRAME);
    importProgressClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    importProgressClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    importProgressClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&importProgressClass);

    WNDCLASSA quickSlidesClass = {};
    quickSlidesClass.lpfnWndProc = QuickSlidesProc;
    quickSlidesClass.hInstance = hInstance;
    quickSlidesClass.lpszClassName = "QuickSlidesWindow";
    quickSlidesClass.hbrBackground = CreateSolidBrush(PROGRAM_COLOR_FRAME);
    quickSlidesClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    quickSlidesClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    quickSlidesClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&quickSlidesClass);

    WNDCLASSA mediaBtnClass = {};
    mediaBtnClass.lpfnWndProc = MediaChromeButtonProc;
    mediaBtnClass.hInstance = hInstance;
    mediaBtnClass.lpszClassName = "MediaChromeButton";
    mediaBtnClass.hbrBackground = CreateSolidBrush(PROGRAM_COLOR_WELL_DARK);
    mediaBtnClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    mediaBtnClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&mediaBtnClass);

    WNDCLASSA scrubClass = {};
    scrubClass.lpfnWndProc = MediaScrubProc;
    scrubClass.hInstance = hInstance;
    scrubClass.lpszClassName = "MediaScrubBar";
    scrubClass.hbrBackground = CreateSolidBrush(PROGRAM_COLOR_WELL_DARK);
    scrubClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    scrubClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&scrubClass);

    WNDCLASSA templatePanelClass = {};
    templatePanelClass.lpfnWndProc = TemplatePanelProc;
    templatePanelClass.hInstance = hInstance;
    templatePanelClass.lpszClassName = "TemplatePanel";
    templatePanelClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    RegisterClassA(&templatePanelClass);

    WNDCLASSA transDefaultPanelClass = {};
    transDefaultPanelClass.lpfnWndProc = TransDefaultPanelProc;
    transDefaultPanelClass.hInstance = hInstance;
    transDefaultPanelClass.lpszClassName = "TransDefaultPanel";
    transDefaultPanelClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    RegisterClassA(&transDefaultPanelClass);

    WNDCLASSA nameClass = {};
    nameClass.lpfnWndProc = ProjectNameProc;
    nameClass.hInstance = hInstance;
    nameClass.lpszClassName = "ProjectNameDialog";
    nameClass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    nameClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&nameClass);

    WNDCLASSA prefsClass = {};
    prefsClass.lpfnWndProc = PreferencesProc;
    prefsClass.hInstance = hInstance;
    prefsClass.lpszClassName = "PreferencesWindow";
    prefsClass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    prefsClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&prefsClass);

    WNDCLASSA searchImportClass = {};
    searchImportClass.lpfnWndProc = SearchImportProc;
    searchImportClass.hInstance = hInstance;
    searchImportClass.lpszClassName = "SearchImportWindow";
    searchImportClass.hbrBackground = CreateSolidBrush(LOOK_COLOR_BASE);
    searchImportClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    searchImportClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&searchImportClass);

    WNDCLASSA prefsBetaClass = {};
    prefsBetaClass.lpfnWndProc = PreferencesBetaProc;
    prefsBetaClass.hInstance = hInstance;
    prefsBetaClass.lpszClassName = "PreferencesBetaWindow";
    prefsBetaClass.hbrBackground = CreateSolidBrush(LOOK_COLOR_BASE);
    prefsBetaClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&prefsBetaClass);

    WNDCLASSA transGenieClass = {};
    transGenieClass.lpfnWndProc = TransitionGenieProc;
    transGenieClass.hInstance = hInstance;
    transGenieClass.lpszClassName = "SlideTransitionGenie";
    transGenieClass.hbrBackground = CreateSolidBrush(PROGRAM_COLOR_WELL);
    transGenieClass.hCursor = LoadCursor(nullptr, IDC_HAND);
    transGenieClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&transGenieClass);

    EditView_Register(hInstance);

    const int splashW = 500;
    const int splashH = 340;
    const int splashX = (GetSystemMetrics(SM_CXSCREEN) - splashW) / 2;
    const int splashY = (GetSystemMetrics(SM_CYSCREEN) - splashH) / 2;
    g_mainWindow = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        "MyCppWindow",
        "Canvas You",
        WS_POPUP,
        splashX,
        splashY,
        splashW,
        splashH,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (g_mainWindow == nullptr)
    {
        return 0;
    }

    const int corner = 1;
    DwmSetWindowAttribute(g_mainWindow, 33, &corner, sizeof(corner));

    ShowWindow(g_mainWindow, SW_SHOW);
    UpdateWindow(g_mainWindow);
    const DWORD splashShown = GetTickCount();
    if (App_Ready())
    {
        // Gpu.exe is started by Initializer before Window; retry connect briefly.
        if (!GpuClient_Connect())
        {
            for (int i = 0; i < 20 && !GpuClient_Connect(); ++i)
            {
                Sleep(100);
            }
        }
        SyncVideoPreload();
    }
    ClearLiveOutputs();
    const DWORD splashElapsed = GetTickCount() - splashShown;
    const DWORD splashRemain = splashElapsed >= 1500 ? 1 : 1500 - splashElapsed;
    SetTimer(g_mainWindow, ID_SPLASH_TIMER, splashRemain, nullptr);
    (void)nCmdShow;

    MSG msg = {};

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (HandleProgramKeys(&msg))
        {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GpuClient_Stop();
    GpuClient_Disconnect();
    Audio_Shutdown();
    App_Shutdown();
    CoUninitialize();
    return 0;
}
