#include "document.h"
#include "renderer.h"
#include "store.h"

#include <string.h>
#include <wchar.h>

static char g_filesDir[MAX_PATH] = {};
static char g_assetsDir[MAX_PATH] = {};
static char g_screensDir[MAX_PATH] = {};
static char g_templatesDir[MAX_PATH] = {};
static char g_projectPath[MAX_PATH] = {};
static char g_projectName[128] = {};

static void JoinPath(char* out, int outChars, const char* folder, const char* name)
{
    lstrcpynA(out, folder, outChars);
    const int len = lstrlenA(out);
    if (len > 0 && out[len - 1] != '\\' && len + 1 < outChars)
    {
        out[len] = '\\';
        out[len + 1] = '\0';
    }
    lstrcatA(out, name);
}

static void JoinPathW(wchar_t* out, int outChars, const wchar_t* folder, const wchar_t* name)
{
    lstrcpynW(out, folder, outChars);
    const int len = lstrlenW(out);
    if (len > 0 && out[len - 1] != L'\\' && len + 1 < outChars)
    {
        out[len] = L'\\';
        out[len + 1] = L'\0';
    }
    lstrcatW(out, name);
}

static const wchar_t* WideFileName(const wchar_t* path)
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

static bool FolderToWide(wchar_t* out, int outChars)
{
    char assets[MAX_PATH] = {};
    Document_GetAssetsDir(assets, MAX_PATH);
    if (assets[0] == '\0')
    {
        return false;
    }
    return MultiByteToWideChar(CP_ACP, 0, assets, -1, out, outChars) > 0;
}

static void EnsureFolder(const char* path)
{
    if (path != nullptr && path[0] != '\0')
    {
        CreateDirectoryA(path, nullptr);
    }
}

static void DefaultUnderRoot(char* out, int outChars, const char* folder)
{
    char root[MAX_PATH] = {};
    if (Store_Ready())
    {
        Store_Root(root, MAX_PATH);
    }
    if (root[0] == '\0')
    {
        Store_ResolveRoot(root, MAX_PATH);
    }
    if (root[0] == '\0')
    {
        out[0] = '\0';
        return;
    }
    JoinPath(out, outChars, root, folder);
}

static void LoadPath(char* dest, int destChars, const char* key, const char* folder)
{
    dest[0] = '\0';
    if (Store_Ready())
    {
        Store_GetString(key, dest, destChars, "");
    }
    if (dest[0] == '\0')
    {
        DefaultUnderRoot(dest, destChars, folder);
    }
}

static void RememberPath(char* dest, int destChars, const char* key, const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return;
    }
    lstrcpynA(dest, path, destChars);
    if (Store_Ready())
    {
        Store_SetString(key, dest);
    }
}

void Document_GetFilesDir(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    if (g_filesDir[0] == '\0')
    {
        LoadPath(g_filesDir, MAX_PATH, "paths.files", "Projects");
    }
    lstrcpynA(out, g_filesDir, outChars);
}

void Document_GetAssetsDir(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    if (g_assetsDir[0] == '\0')
    {
        LoadPath(g_assetsDir, MAX_PATH, "paths.assets", "media");
    }
    lstrcpynA(out, g_assetsDir, outChars);
}

void Document_GetScreensDir(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    if (g_screensDir[0] == '\0')
    {
        LoadPath(g_screensDir, MAX_PATH, "paths.screens", "Screens");
    }
    lstrcpynA(out, g_screensDir, outChars);
}

void Document_GetTemplatesDir(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    if (g_templatesDir[0] == '\0')
    {
        LoadPath(g_templatesDir, MAX_PATH, "paths.templates", "Templates");
    }
    lstrcpynA(out, g_templatesDir, outChars);
}

void Document_SetFilesDir(const char* path)
{
    RememberPath(g_filesDir, MAX_PATH, "paths.files", path);
}

void Document_SetAssetsDir(const char* path)
{
    RememberPath(g_assetsDir, MAX_PATH, "paths.assets", path);
}

void Document_SetScreensDir(const char* path)
{
    RememberPath(g_screensDir, MAX_PATH, "paths.screens", path);
}

void Document_SetTemplatesDir(const char* path)
{
    RememberPath(g_templatesDir, MAX_PATH, "paths.templates", path);
}

void Document_EnsureFolders()
{
    char files[MAX_PATH] = {};
    char assets[MAX_PATH] = {};
    char screens[MAX_PATH] = {};
    char templates[MAX_PATH] = {};
    Document_GetFilesDir(files, MAX_PATH);
    Document_GetAssetsDir(assets, MAX_PATH);
    Document_GetScreensDir(screens, MAX_PATH);
    Document_GetTemplatesDir(templates, MAX_PATH);
    EnsureFolder(files);
    EnsureFolder(assets);
    EnsureFolder(screens);
    EnsureFolder(templates);
}

bool Document_HasProject()
{
    return g_projectPath[0] != '\0';
}

void Document_Name(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    lstrcpynA(out, g_projectName, outChars);
}

void Document_Path(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    lstrcpynA(out, g_projectPath, outChars);
}

static void RememberCurrentProject()
{
    if (g_projectPath[0] == '\0' || g_projectName[0] == '\0')
    {
        return;
    }
    Store_RememberProject(g_projectName, g_projectPath);
    if (Store_Ready())
    {
        Store_SetString("last.document", g_projectPath);
    }
}

static bool SanitizeName(const char* in, char* out, int outChars)
{
    if (in == nullptr || out == nullptr || outChars <= 1)
    {
        return false;
    }

    while (*in == ' ' || *in == '\t')
    {
        in += 1;
    }

    int written = 0;
    for (; *in != '\0' && written + 1 < outChars; ++in)
    {
        const char ch = *in;
        if (ch == '\\' || ch == '/' || ch == ':' || ch == '*' || ch == '?' ||
            ch == '"' || ch == '<' || ch == '>' || ch == '|')
        {
            continue;
        }
        out[written] = ch;
        written += 1;
    }
    while (written > 0 && out[written - 1] == ' ')
    {
        written -= 1;
    }
    out[written] = '\0';
    return written > 0;
}

bool Document_NewProject(const char* name)
{
    char safe[128] = {};
    if (!SanitizeName(name, safe, sizeof(safe)))
    {
        return false;
    }

    Document_EnsureFolders();
    char files[MAX_PATH] = {};
    Document_GetFilesDir(files, MAX_PATH);
    if (files[0] == '\0')
    {
        return false;
    }

    char folder[MAX_PATH] = {};
    JoinPath(folder, MAX_PATH, files, safe);
    if (GetFileAttributesA(folder) != INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    if (!CreateDirectoryA(folder, nullptr))
    {
        return false;
    }

    char fileName[160] = {};
    lstrcpynA(fileName, safe, sizeof(fileName));
    lstrcatA(fileName, ".json");
    char path[MAX_PATH] = {};
    JoinPath(path, MAX_PATH, folder, fileName);

    if (!Renderer_NewDocument())
    {
        return false;
    }
    if (!Renderer_SaveDocument(path, safe))
    {
        return false;
    }

    lstrcpynA(g_projectPath, path, MAX_PATH);
    lstrcpynA(g_projectName, safe, sizeof(g_projectName));
    RememberCurrentProject();
    return true;
}

bool Document_AddSlide()
{
    if (!Document_HasProject())
    {
        return false;
    }
    if (!Renderer_AddBlankSlide())
    {
        return false;
    }
    return Document_Save();
}

static void NameFromPath(const char* path, char* out, int outChars)
{
    const char* slash = strrchr(path, '\\');
    const char* name = slash != nullptr ? slash + 1 : path;
    lstrcpynA(out, name, outChars);
    char* dot = strrchr(out, '.');
    if (dot != nullptr)
    {
        *dot = '\0';
    }
}

bool Document_Open(const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    char name[128] = {};
    if (!Renderer_LoadDocument(path, name, sizeof(name)))
    {
        return false;
    }
    if (name[0] == '\0')
    {
        NameFromPath(path, name, sizeof(name));
    }

    lstrcpynA(g_projectPath, path, MAX_PATH);
    lstrcpynA(g_projectName, name, sizeof(g_projectName));
    RememberCurrentProject();
    return true;
}

bool Document_Save()
{
    if (!Document_HasProject())
    {
        return true;
    }
    if (!Renderer_SaveDocument(g_projectPath, g_projectName))
    {
        return false;
    }
    RememberCurrentProject();
    return true;
}

bool Document_SaveOrWarn(HWND owner)
{
    if (!Document_HasProject())
    {
        return true;
    }
    if (Document_Save())
    {
        return true;
    }
    MessageBoxA(
        owner,
        "Could not save the new slide order to the project file.",
        "Save",
        MB_OK | MB_ICONWARNING
    );
    return false;
}

int Document_PlanImport(
    const wchar_t* source,
    wchar_t* outName,
    int nameChars,
    wchar_t* outDest,
    int destChars
)
{
    if (source == nullptr || source[0] == L'\0' || outName == nullptr || nameChars < 2 ||
        outDest == nullptr || destChars < 2)
    {
        return 0;
    }

    Document_EnsureFolders();
    wchar_t assets[MAX_PATH] = {};
    if (!FolderToWide(assets, MAX_PATH))
    {
        return 0;
    }

    const wchar_t* base = WideFileName(source);
    if (base[0] == L'\0')
    {
        return 0;
    }

    wchar_t stem[MAX_PATH] = {};
    wchar_t ext[64] = {};
    const wchar_t* dot = wcsrchr(base, L'.');
    if (dot != nullptr && dot != base)
    {
        const int stemChars = static_cast<int>(dot - base) + 1;
        lstrcpynW(stem, base, stemChars < MAX_PATH ? stemChars : MAX_PATH);
        lstrcpynW(ext, dot, 64);
    }
    else
    {
        lstrcpynW(stem, base, MAX_PATH);
    }

    wchar_t candidate[MAX_PATH] = {};
    lstrcpynW(candidate, base, MAX_PATH);
    for (int n = 0; n < 1000; ++n)
    {
        if (n > 0)
        {
            wsprintfW(candidate, L"%s-%d%s", stem, n + 1, ext);
        }

        wchar_t dest[MAX_PATH] = {};
        JoinPathW(dest, MAX_PATH, assets, candidate);
        if (lstrcmpiW(source, dest) == 0)
        {
            lstrcpynW(outName, candidate, nameChars);
            lstrcpynW(outDest, dest, destChars);
            return 2;
        }

        if (GetFileAttributesW(dest) == INVALID_FILE_ATTRIBUTES)
        {
            lstrcpynW(outName, candidate, nameChars);
            lstrcpynW(outDest, dest, destChars);
            return 1;
        }
    }
    return 0;
}

bool Document_ImportAsset(const wchar_t* source, wchar_t* outName, int outChars)
{
    wchar_t dest[MAX_PATH] = {};
    const int plan = Document_PlanImport(source, outName, outChars, dest, MAX_PATH);
    if (plan == 0)
    {
        return false;
    }
    if (plan == 2)
    {
        return true;
    }
    return CopyFileW(source, dest, TRUE) != FALSE;
}

bool Document_ResolveAsset(const wchar_t* stored, wchar_t* outPath, int outChars)
{
    if (stored == nullptr || stored[0] == L'\0' || outPath == nullptr || outChars < 2)
    {
        return false;
    }

    if (wcschr(stored, L'\\') != nullptr || wcschr(stored, L'/') != nullptr ||
        (stored[1] == L':' && ((stored[0] >= L'A' && stored[0] <= L'Z') ||
                               (stored[0] >= L'a' && stored[0] <= L'z'))))
    {
        lstrcpynW(outPath, stored, outChars);
        return true;
    }

    wchar_t assets[MAX_PATH] = {};
    if (!FolderToWide(assets, MAX_PATH))
    {
        return false;
    }
    JoinPathW(outPath, outChars, assets, stored);
    return true;
}
