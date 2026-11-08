#pragma once

#include <windows.h>

enum ImageDisplayMode
{
    IMAGE_STRETCH = 0,
    IMAGE_FIT = 1,
    IMAGE_FILL = 2
};

enum SlideTransition
{
    SLIDE_TRANSITION_NONE = 0,
    SLIDE_TRANSITION_CROSSFADE = 1,
    SLIDE_TRANSITION_FADE_BLACK = 2,
    SLIDE_TRANSITION_PUSH = 3,
    SLIDE_TRANSITION_WIPE = 4,
    SLIDE_TRANSITION_COVER = 5
};

enum SlideTransitionDir
{
    SLIDE_TRANSITION_DIR_LEFT = 0,
    SLIDE_TRANSITION_DIR_RIGHT = 1,
    SLIDE_TRANSITION_DIR_UP = 2,
    SLIDE_TRANSITION_DIR_DOWN = 3
};

bool Renderer_Startup();
void Renderer_Shutdown();

bool Renderer_SelectSlide(int index);
int Renderer_CurrentSlide();
int Renderer_SlideCount();

void Renderer_Blit(HDC hdc, int width, int height);
void Renderer_BlitSlideAt(HDC hdc, int index, int x, int y, int width, int height);
void Renderer_BlitSlideContentAt(HDC hdc, int index, int x, int y, int width, int height);
void Renderer_BlitSlidePreviewAt(HDC hdc, int index, int x, int y, int width, int height);
HBITMAP Renderer_SlideFrame(int index, int* width, int* height);
unsigned Renderer_FrameGeneration(int index);
void Renderer_PresentTo(HWND hwnd);
void Renderer_PresentHostThenDisplays(HWND host, HWND displays[3]);

void Renderer_GetSlide5Text(char* out, int outChars);
void Renderer_SetSlide5Text(const char* text);
void Renderer_SetSlide5BackgroundColor(COLORREF color);
bool Renderer_SetSlide5BackgroundImage(const wchar_t* path);
void Renderer_SetSlide5Font(const char* fontName, int fontSize);
void Renderer_SetSlide5TextColor(COLORREF color);
void Renderer_GetSlide5Font(char* outName, int outChars, int* fontSize);
bool Renderer_SetSlide6Image(const wchar_t* path);
void Renderer_SetSlide5ImageMode(ImageDisplayMode mode);
void Renderer_SetSlide6ImageMode(ImageDisplayMode mode);

void Renderer_BlitAt(HDC hdc, int x, int y, int width, int height);
void Renderer_GetSlideSize(int index, int* width, int* height);

int Renderer_FreeformCount();
bool Renderer_FreeformGet(int index, int* kind, int* x, int* y, int* w, int* h);
float Renderer_FreeformGetRotation(int index);
void Renderer_FreeformSetRotation(int index, float degrees);
int Renderer_FreeformHitTest(int slideX, int slideY);
int Renderer_FreeformHitTestPad(int slideX, int slideY, int pad);
int Renderer_FreeformAddText(int x, int y);
int Renderer_FreeformAddRect(int x, int y, int w, int h);
int Renderer_FreeformAddEllipse(int x, int y, int w, int h);
int Renderer_FreeformAddImage(int x, int y, int w, int h, const wchar_t* path);
void Renderer_FreeformSetFrame(int index, int x, int y, int w, int h);
void Renderer_FreeformDelete(int index);
void Renderer_FreeformGetText(int index, char* out, int outChars);
void Renderer_FreeformSetText(int index, const char* text);
void Renderer_FreeformSetTextLive(int index, const char* text);
bool Renderer_FreeformGetStyle(
    int index,
    char* fontName,
    int nameChars,
    int* fontSize,
    COLORREF* color,
    bool* bold,
    bool* italic,
    bool* underline);
void Renderer_FreeformSetFontName(int index, const char* name);
void Renderer_FreeformSetFontSize(int index, int size);
void Renderer_FreeformSetTextColor(int index, COLORREF color);
void Renderer_FreeformSetFontStyle(int index, bool bold, bool italic, bool underline);
int Renderer_FreeformGetAlign(int index);
void Renderer_FreeformSetAlign(int index, int align);
void Renderer_FreeformBlitLive(HDC hdc, int x, int y, int width, int height);
void Renderer_FreeformRebuild();
void Renderer_FreeformBeginEditorDrag(int hideIndex, int previewW, int previewH);
void Renderer_FreeformEndEditorDrag();
void Renderer_FreeformBlitEditor(HDC hdc, int x, int y, int width, int height, int liveIndex);

struct VideoLayer
{
    wchar_t path[MAX_PATH];
    int x;
    int y;
    int w;
    int h;
    int slideW;
    int slideH;
    ImageDisplayMode mode;
};

bool Renderer_CurrentSlideHasVideo();
int Renderer_GetVideoLayers(VideoLayer* out, int max);
// Every video in the document, background videos first, resolved and unique.
int Renderer_CollectVideoPaths(wchar_t (*out)[MAX_PATH], int max);
bool Renderer_SetSlide8Video(const wchar_t* path);
void Renderer_SetSlide8ImageMode(ImageDisplayMode mode);
int Renderer_FreeformAddVideo(int x, int y, int w, int h, const wchar_t* path);
bool Renderer_AddFullscreenMediaSlide(const wchar_t* storedPath, bool video, bool asBackground);
bool Renderer_IsBackgroundSlide(int index);
int Renderer_GetSlideTransition(int index);
bool Renderer_SetSlideTransition(int index, int transition);
int Renderer_GetSlideTransitionDir(int index);
bool Renderer_SetSlideTransitionDir(int index, int direction);
int Renderer_GetSlideTransitionMs(int index);
bool Renderer_SetSlideTransitionMs(int index, int milliseconds);
bool Renderer_SlideUsesTransitionOverride(int index);
bool Renderer_SetSlideTransitionOverride(int index, bool on);
void Renderer_ClearSlideTransitionOverride(int index);
int Renderer_EffectiveSlideTransition(int index);
int Renderer_EffectiveSlideTransitionDir(int index);
int Renderer_EffectiveSlideTransitionMs(int index);
int Renderer_DefaultSlideTransition();
int Renderer_DefaultSlideTransitionDir();
int Renderer_DefaultSlideTransitionMs();
int Renderer_DefaultBgTransition();
int Renderer_DefaultBgTransitionDir();
int Renderer_DefaultBgTransitionMs();
void Renderer_SetDefaultSlideTransition(int transition, int direction, int milliseconds);
void Renderer_SetDefaultBgTransition(int transition, int direction, int milliseconds);
void Renderer_LoadTransitionDefaults();
void Renderer_NormalizeInheritedTransitions();
bool Renderer_TransitionNeedsDir(int transition);
const char* Renderer_TransitionName(int transition);
const char* Renderer_TransitionDirName(int direction);
void Renderer_ArmBackground(int index);
int Renderer_ArmedBackground();
bool Renderer_BackgroundVideoPath(wchar_t* out, int outChars);
// Caller owns the returned bitmap.
HBITMAP Renderer_RenderBackgroundBed(int index, int* width, int* height);
// Same as the bed, with each video drawn as its poster frame. Caller owns it.
HBITMAP Renderer_RenderBackgroundStill(int index, int* width, int* height);
bool Renderer_UseAsSlide(int index);
bool Renderer_UseAsBackground(int index);
bool Renderer_FreeformIsLocked(int index);
int Renderer_FreeformHitTestAny(int slideX, int slideY);
bool Renderer_FreeformCopy(int index);
bool Renderer_FreeformHasClipboard();
int Renderer_FreeformPaste();
HBITMAP Renderer_ComposeLiveOverlay(int bottomIndex, int topIndex, bool topOn, int* width, int* height);

bool Renderer_NewDocument();
bool Renderer_AddBlankSlide();
bool Renderer_SlideSlotEmpty(int slideIndex, const char* slot);
// Also sizes the box to the text's lines around its vertical center.
bool Renderer_SetSlideSlotText(int slideIndex, const char* slot, const char* text);
bool Renderer_SaveDocument(const char* path, const char* name);
bool Renderer_LoadDocument(const char* path, char* nameOut, int nameChars);
bool Renderer_SaveTemplate(const char* path, const char* id, const char* name);
bool Renderer_LoadTemplate(const char* path, char* nameOut, int nameChars);
// Restyles the given slides from a template file; each keeps the words of its largest text box.
bool Renderer_ApplyTemplate(const char* path, const int* indices, int count);
// Caller owns the returned bitmap.
HBITMAP Renderer_RenderTemplateThumb(const char* path, int width, int height);
bool Renderer_SetEditSlide(int index);
int Renderer_EditSlide();
bool Renderer_MoveSlide(int from, int dropSlot);
bool Renderer_DeleteSlides(const int* indices, int count);
bool Renderer_CopySlides(const int* indices, int count);
bool Renderer_HasSlideClipboard();
bool Renderer_PasteSlides(int insertBefore);
