#include "store.h"
#include "sqlite3.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

static sqlite3* g_db = nullptr;
static char g_root[MAX_PATH] = {};

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

static bool ReadDataRootOverride(char* out, int outChars)
{
    char exe[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exe, MAX_PATH) == 0)
    {
        return false;
    }

    char* slash = strrchr(exe, '\\');
    if (slash == nullptr)
    {
        return false;
    }
    slash[1] = '\0';
    lstrcatA(exe, "data_root.txt");

    HANDLE file = CreateFileA(exe, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    char line[MAX_PATH] = {};
    DWORD read = 0;
    const BOOL ok = ReadFile(file, line, sizeof(line) - 1, &read, nullptr);
    CloseHandle(file);
    if (!ok || read == 0)
    {
        return false;
    }

    while (read > 0 && (line[read - 1] == '\n' || line[read - 1] == '\r' || line[read - 1] == ' '))
    {
        line[read - 1] = '\0';
        read -= 1;
    }
    if (line[0] == '\0')
    {
        return false;
    }

    lstrcpynA(out, line, outChars);
    return true;
}

static bool DefaultRoot(char* out, int outChars)
{
    char appData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
    {
        return false;
    }

    JoinPath(out, outChars, appData, "PresentationSoftwareDev");
    return true;
}

static void EnsureFolder(const char* path)
{
    CreateDirectoryA(path, nullptr);
}

static void SyncTemplateFiles(const char* folder);

static bool Exec(const char* sql)
{
    if (g_db == nullptr)
    {
        return false;
    }

    char* error = nullptr;
    const int result = sqlite3_exec(g_db, sql, nullptr, nullptr, &error);
    sqlite3_free(error);
    return result == SQLITE_OK;
}

static void BackfillProjectOrderIfNeeded()
{
    if (g_db == nullptr)
    {
        return;
    }

    sqlite3_stmt* check = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "SELECT COUNT(*), MIN(sort_order), MAX(sort_order) FROM projects;",
            -1,
            &check,
            nullptr) != SQLITE_OK)
    {
        return;
    }

    int count = 0;
    int minOrder = 0;
    int maxOrder = 0;
    if (sqlite3_step(check) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check, 0);
        minOrder = sqlite3_column_int(check, 1);
        maxOrder = sqlite3_column_int(check, 2);
    }
    sqlite3_finalize(check);
    if (count <= 1 || minOrder != 0 || maxOrder != 0)
    {
        return;
    }

    sqlite3_stmt* list = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "SELECT path FROM projects ORDER BY updated DESC, name ASC;",
            -1,
            &list,
            nullptr) != SQLITE_OK)
    {
        return;
    }

    sqlite3_stmt* update = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "UPDATE projects SET sort_order = ?1 WHERE path = ?2;",
            -1,
            &update,
            nullptr) != SQLITE_OK)
    {
        sqlite3_finalize(list);
        return;
    }

    int order = 0;
    while (sqlite3_step(list) == SQLITE_ROW)
    {
        const char* path = reinterpret_cast<const char*>(sqlite3_column_text(list, 0));
        sqlite3_reset(update);
        sqlite3_clear_bindings(update);
        sqlite3_bind_int(update, 1, order);
        sqlite3_bind_text(update, 2, path != nullptr ? path : "", -1, SQLITE_TRANSIENT);
        sqlite3_step(update);
        order += 1;
    }

    sqlite3_finalize(update);
    sqlite3_finalize(list);
}

bool Store_ResolveRoot(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return false;
    }

    out[0] = '\0';
    if (ReadDataRootOverride(out, outChars))
    {
        return true;
    }
    return DefaultRoot(out, outChars);
}

bool Store_Open()
{
    char root[MAX_PATH] = {};
    if (!Store_ResolveRoot(root, MAX_PATH))
    {
        return false;
    }
    return Store_OpenAt(root);
}

bool Store_OpenAt(const char* root)
{
    Store_Close();
    if (root == nullptr || root[0] == '\0')
    {
        return false;
    }

    lstrcpynA(g_root, root, MAX_PATH);
    EnsureFolder(g_root);

    char media[MAX_PATH] = {};
    JoinPath(media, MAX_PATH, g_root, "media");
    EnsureFolder(media);

    char dbPath[MAX_PATH] = {};
    JoinPath(dbPath, MAX_PATH, g_root, "app.sqlite");

    if (sqlite3_open(dbPath, &g_db) != SQLITE_OK)
    {
        Store_Close();
        return false;
    }

    if (!Exec("CREATE TABLE IF NOT EXISTS prefs (key TEXT PRIMARY KEY, value TEXT NOT NULL);"))
    {
        Store_Close();
        return false;
    }

    if (!Exec(
            "CREATE TABLE IF NOT EXISTS projects ("
            "path TEXT PRIMARY KEY, "
            "name TEXT NOT NULL, "
            "updated INTEGER NOT NULL, "
            "sort_order INTEGER NOT NULL DEFAULT 0);"))
    {
        Store_Close();
        return false;
    }
    Exec("ALTER TABLE projects ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0;");
    BackfillProjectOrderIfNeeded();

    if (!Exec(
            "CREATE TABLE IF NOT EXISTS screens ("
            "sort_order INTEGER PRIMARY KEY, "
            "name TEXT NOT NULL, "
            "layout_mode INTEGER NOT NULL DEFAULT 0, "
            "output_count INTEGER NOT NULL DEFAULT 0, "
            "canvas_w INTEGER NOT NULL DEFAULT 1920, "
            "canvas_h INTEGER NOT NULL DEFAULT 1080, "
            "refresh_hz INTEGER NOT NULL DEFAULT 60);"))
    {
        Store_Close();
        return false;
    }
    Exec("ALTER TABLE screens ADD COLUMN canvas_w INTEGER NOT NULL DEFAULT 1920;");
    Exec("ALTER TABLE screens ADD COLUMN canvas_h INTEGER NOT NULL DEFAULT 1080;");
    Exec("ALTER TABLE screens ADD COLUMN refresh_hz INTEGER NOT NULL DEFAULT 60;");
    if (!Exec(
            "CREATE TABLE IF NOT EXISTS screen_outputs ("
            "sort_order INTEGER NOT NULL, "
            "slot INTEGER NOT NULL, "
            "device TEXT NOT NULL DEFAULT '', "
            "PRIMARY KEY (sort_order, slot));"))
    {
        Store_Close();
        return false;
    }
    if (!Exec(
            "CREATE TABLE IF NOT EXISTS virtual_displays ("
            "id INTEGER PRIMARY KEY, "
            "mode TEXT NOT NULL);"))
    {
        Store_Close();
        return false;
    }

    if (!Exec(
            "CREATE TABLE IF NOT EXISTS templates ("
            "id TEXT PRIMARY KEY, "
            "name TEXT NOT NULL, "
            "path TEXT NOT NULL, "
            "layout TEXT, "
            "updated INTEGER NOT NULL, "
            "sort_order INTEGER NOT NULL DEFAULT 0);"))
    {
        Store_Close();
        return false;
    }

    char templates[MAX_PATH] = {};
    JoinPath(templates, MAX_PATH, g_root, "Templates");
    EnsureFolder(templates);
    SyncTemplateFiles(templates);

    return true;
}

void Store_Close()
{
    if (g_db != nullptr)
    {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}

bool Store_Ready()
{
    return g_db != nullptr;
}

sqlite3* Store_Db()
{
    return g_db;
}

void Store_Root(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    lstrcpynA(out, g_root, outChars);
}

int Store_GetInt(const char* key, int fallback)
{
    char text[32] = {};
    Store_GetString(key, text, sizeof(text), "");
    if (text[0] == '\0')
    {
        return fallback;
    }
    return atoi(text);
}

void Store_SetInt(const char* key, int value)
{
    char text[32] = {};
    wsprintfA(text, "%d", value);
    Store_SetString(key, text);
}

void Store_GetString(const char* key, char* out, int outChars, const char* fallback)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    out[0] = '\0';
    if (g_db == nullptr || key == nullptr)
    {
        if (fallback != nullptr)
        {
            lstrcpynA(out, fallback, outChars);
        }
        return;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_db, "SELECT value FROM prefs WHERE key = ?1;", -1, &statement, nullptr) != SQLITE_OK)
    {
        if (fallback != nullptr)
        {
            lstrcpynA(out, fallback, outChars);
        }
        return;
    }

    sqlite3_bind_text(statement, 1, key, -1, SQLITE_STATIC);
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        const char* value = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        if (value != nullptr)
        {
            lstrcpynA(out, value, outChars);
        }
    }
    else if (fallback != nullptr)
    {
        lstrcpynA(out, fallback, outChars);
    }

    sqlite3_finalize(statement);
}

void Store_SetString(const char* key, const char* value)
{
    if (g_db == nullptr || key == nullptr || value == nullptr)
    {
        return;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "INSERT INTO prefs(key, value) VALUES(?1, ?2) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return;
    }

    sqlite3_bind_text(statement, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, value, -1, SQLITE_STATIC);
    sqlite3_step(statement);
    sqlite3_finalize(statement);
}

bool Store_RememberProject(const char* name, const char* path)
{
    if (g_db == nullptr || name == nullptr || name[0] == '\0' || path == nullptr || path[0] == '\0')
    {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "INSERT INTO projects(path, name, updated, sort_order) "
            "VALUES(?1, ?2, ?3, COALESCE((SELECT MAX(sort_order) + 1 FROM projects), 0)) "
            "ON CONFLICT(path) DO UPDATE SET name = excluded.name, updated = excluded.updated;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(statement, 1, path, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 3, static_cast<sqlite3_int64>(time(nullptr)));
    const int result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    return result == SQLITE_DONE;
}

int Store_ListProjects(StoreProject* out, int max)
{
    if (out == nullptr || max <= 0 || g_db == nullptr)
    {
        return 0;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "SELECT name, path FROM projects ORDER BY sort_order ASC, name ASC;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return 0;
    }

    int count = 0;
    while (count < max && sqlite3_step(statement) == SQLITE_ROW)
    {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        const char* path = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        lstrcpynA(out[count].name, name != nullptr ? name : "", sizeof(out[count].name));
        lstrcpynA(out[count].path, path != nullptr ? path : "", sizeof(out[count].path));
        count += 1;
    }

    sqlite3_finalize(statement);
    return count;
}

bool Store_SetProjectOrder(const StoreProject* items, int count)
{
    if (g_db == nullptr || items == nullptr || count < 0)
    {
        return false;
    }

    if (!Exec("BEGIN;"))
    {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "UPDATE projects SET sort_order = ?1 WHERE path = ?2;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        Exec("ROLLBACK;");
        return false;
    }

    bool ok = true;
    for (int i = 0; i < count; ++i)
    {
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
        sqlite3_bind_int(statement, 1, i);
        sqlite3_bind_text(statement, 2, items[i].path, -1, SQLITE_STATIC);
        if (sqlite3_step(statement) != SQLITE_DONE)
        {
            ok = false;
            break;
        }
    }

    sqlite3_finalize(statement);
    if (!ok)
    {
        Exec("ROLLBACK;");
        return false;
    }
    return Exec("COMMIT;");
}

int Store_LoadScreens(StoreScreen* out, int max, int* activeOut)
{
    if (activeOut != nullptr)
    {
        *activeOut = 0;
    }
    if (out == nullptr || max <= 0 || g_db == nullptr)
    {
        return 0;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "SELECT sort_order, name, layout_mode, output_count, canvas_w, canvas_h, refresh_hz FROM screens ORDER BY sort_order ASC;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return 0;
    }

    int count = 0;
    while (count < max && sqlite3_step(statement) == SQLITE_ROW)
    {
        ZeroMemory(&out[count], sizeof(out[count]));
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        lstrcpynA(out[count].name, name != nullptr ? name : "", sizeof(out[count].name));
        out[count].layoutMode = sqlite3_column_int(statement, 2);
        out[count].outputCount = sqlite3_column_int(statement, 3);
        out[count].canvasW = sqlite3_column_int(statement, 4);
        out[count].canvasH = sqlite3_column_int(statement, 5);
        out[count].refreshHz = sqlite3_column_int(statement, 6);
        if (out[count].outputCount < 0)
        {
            out[count].outputCount = 0;
        }
        if (out[count].outputCount > STORE_SCREEN_OUTPUT_MAX)
        {
            out[count].outputCount = STORE_SCREEN_OUTPUT_MAX;
        }
        if (out[count].canvasW <= 0)
        {
            out[count].canvasW = 1920;
        }
        if (out[count].canvasH <= 0)
        {
            out[count].canvasH = 1080;
        }
        if (out[count].refreshHz <= 0)
        {
            out[count].refreshHz = 60;
        }
        count += 1;
    }
    sqlite3_finalize(statement);

    sqlite3_stmt* outputs = nullptr;
    if (count > 0 && sqlite3_prepare_v2(
            g_db,
            "SELECT sort_order, slot, device FROM screen_outputs;",
            -1,
            &outputs,
            nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(outputs) == SQLITE_ROW)
        {
            const int order = sqlite3_column_int(outputs, 0);
            const int slot = sqlite3_column_int(outputs, 1);
            const char* device = reinterpret_cast<const char*>(sqlite3_column_text(outputs, 2));
            if (order >= 0 && order < count && slot >= 0 && slot < STORE_SCREEN_OUTPUT_MAX)
            {
                lstrcpynA(out[order].devices[slot], device != nullptr ? device : "", 32);
            }
        }
        sqlite3_finalize(outputs);
    }

    if (activeOut != nullptr)
    {
        int active = Store_GetInt("screens.active", 0);
        if (active < 0 || active >= count)
        {
            active = 0;
        }
        *activeOut = active;
    }
    return count;
}

bool Store_SaveScreens(const StoreScreen* items, int count, int active)
{
    if (g_db == nullptr || (count > 0 && items == nullptr) || count < 0)
    {
        return false;
    }
    if (count > STORE_SCREEN_MAX)
    {
        count = STORE_SCREEN_MAX;
    }

    if (!Exec("BEGIN;"))
    {
        return false;
    }
    if (!Exec("DELETE FROM screen_outputs;") || !Exec("DELETE FROM screens;"))
    {
        Exec("ROLLBACK;");
        return false;
    }

    sqlite3_stmt* screen = nullptr;
    sqlite3_stmt* output = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "INSERT INTO screens(sort_order, name, layout_mode, output_count, canvas_w, canvas_h, refresh_hz) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7);",
            -1,
            &screen,
            nullptr) != SQLITE_OK)
    {
        Exec("ROLLBACK;");
        return false;
    }
    if (sqlite3_prepare_v2(
            g_db,
            "INSERT INTO screen_outputs(sort_order, slot, device) VALUES(?1, ?2, ?3);",
            -1,
            &output,
            nullptr) != SQLITE_OK)
    {
        sqlite3_finalize(screen);
        Exec("ROLLBACK;");
        return false;
    }

    bool ok = true;
    for (int i = 0; i < count && ok; ++i)
    {
        sqlite3_reset(screen);
        sqlite3_clear_bindings(screen);
        sqlite3_bind_int(screen, 1, i);
        sqlite3_bind_text(screen, 2, items[i].name[0] != '\0' ? items[i].name : "Screen", -1, SQLITE_STATIC);
        sqlite3_bind_int(screen, 3, items[i].layoutMode);
        sqlite3_bind_int(screen, 4, items[i].outputCount);
        sqlite3_bind_int(screen, 5, items[i].canvasW > 0 ? items[i].canvasW : 1920);
        sqlite3_bind_int(screen, 6, items[i].canvasH > 0 ? items[i].canvasH : 1080);
        sqlite3_bind_int(screen, 7, items[i].refreshHz > 0 ? items[i].refreshHz : 60);
        if (sqlite3_step(screen) != SQLITE_DONE)
        {
            ok = false;
            break;
        }

        const int slots = items[i].outputCount < STORE_SCREEN_OUTPUT_MAX
            ? items[i].outputCount
            : STORE_SCREEN_OUTPUT_MAX;
        for (int slot = 0; slot < slots; ++slot)
        {
            sqlite3_reset(output);
            sqlite3_clear_bindings(output);
            sqlite3_bind_int(output, 1, i);
            sqlite3_bind_int(output, 2, slot);
            sqlite3_bind_text(output, 3, items[i].devices[slot], -1, SQLITE_STATIC);
            if (sqlite3_step(output) != SQLITE_DONE)
            {
                ok = false;
                break;
            }
        }
    }

    sqlite3_finalize(screen);
    sqlite3_finalize(output);
    if (!ok)
    {
        Exec("ROLLBACK;");
        return false;
    }
    if (!Exec("COMMIT;"))
    {
        return false;
    }

    if (active < 0 || active >= count)
    {
        active = 0;
    }
    Store_SetInt("screens.active", active);
    return true;
}

int Store_LoadVirtuals(StoreVirtual* out, int max)
{
    if (out == nullptr || max <= 0 || g_db == nullptr)
    {
        return 0;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "SELECT id, mode FROM virtual_displays ORDER BY id ASC;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return 0;
    }

    int count = 0;
    while (count < max && sqlite3_step(statement) == SQLITE_ROW)
    {
        ZeroMemory(&out[count], sizeof(out[count]));
        out[count].id = sqlite3_column_int(statement, 0);
        const char* mode = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        lstrcpynA(out[count].mode, mode != nullptr ? mode : "1080p60", sizeof(out[count].mode));
        count += 1;
    }
    sqlite3_finalize(statement);
    return count;
}

bool Store_SaveVirtuals(const StoreVirtual* items, int count)
{
    if (g_db == nullptr || (count > 0 && items == nullptr) || count < 0)
    {
        return false;
    }
    if (count > STORE_VIRTUAL_MAX)
    {
        count = STORE_VIRTUAL_MAX;
    }

    if (!Exec("BEGIN;"))
    {
        return false;
    }
    if (!Exec("DELETE FROM virtual_displays;"))
    {
        Exec("ROLLBACK;");
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "INSERT INTO virtual_displays(id, mode) VALUES(?1, ?2);",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        Exec("ROLLBACK;");
        return false;
    }

    bool ok = true;
    for (int i = 0; i < count && ok; ++i)
    {
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
        sqlite3_bind_int(statement, 1, items[i].id > 0 ? items[i].id : i + 1);
        sqlite3_bind_text(
            statement,
            2,
            items[i].mode[0] != '\0' ? items[i].mode : "1080p60",
            -1,
            SQLITE_STATIC
        );
        if (sqlite3_step(statement) != SQLITE_DONE)
        {
            ok = false;
        }
    }
    sqlite3_finalize(statement);
    if (!ok)
    {
        Exec("ROLLBACK;");
        return false;
    }
    return Exec("COMMIT;");
}

void Store_TemplatesDir(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    out[0] = '\0';
    if (g_root[0] == '\0')
    {
        return;
    }
    JoinPath(out, outChars, g_root, "Templates");
}

static void ExtractJsonStringField(const char* json, const char* key, char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }
    out[0] = '\0';
    if (json == nullptr || key == nullptr || key[0] == '\0')
    {
        return;
    }

    char quoted[80] = {};
    quoted[0] = '"';
    lstrcpynA(quoted + 1, key, 76);
    lstrcatA(quoted, "\"");
    const char* cursor = json;
    while ((cursor = strstr(cursor, quoted)) != nullptr)
    {
        cursor += lstrlenA(quoted);
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
        {
            cursor += 1;
        }
        if (*cursor != ':')
        {
            continue;
        }
        cursor += 1;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
        {
            cursor += 1;
        }
        if (*cursor != '"')
        {
            return;
        }
        cursor += 1;
        int written = 0;
        while (*cursor != '\0' && *cursor != '"')
        {
            char ch = *cursor;
            cursor += 1;
            if (ch == '\\' && *cursor != '\0')
            {
                ch = *cursor;
                cursor += 1;
            }
            if (written + 1 < outChars)
            {
                out[written] = ch;
                written += 1;
            }
        }
        out[written] = '\0';
        return;
    }
}

// Template files are UTF-8; names are shown with the ANSI GDI calls. Older files
// with raw ANSI bytes don't decode as UTF-8 and are kept as-is.
static void TextFromFile(char* text, int textChars)
{
    bool ascii = true;
    for (const char* cursor = text; *cursor != '\0'; ++cursor)
    {
        if (static_cast<unsigned char>(*cursor) >= 0x80)
        {
            ascii = false;
            break;
        }
    }
    wchar_t wide[512] = {};
    if (ascii || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, 512) == 0)
    {
        return;
    }
    if (WideCharToMultiByte(CP_ACP, 0, wide, -1, text, textChars, nullptr, nullptr) == 0)
    {
        text[textChars - 1] = '\0';
    }
}

static bool ReadFileText(const char* path, char** out)
{
    if (out == nullptr)
    {
        return false;
    }
    *out = nullptr;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    const DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 2 * 1024 * 1024)
    {
        CloseHandle(file);
        return false;
    }

    char* text = static_cast<char*>(malloc(static_cast<size_t>(size) + 1));
    if (text == nullptr)
    {
        CloseHandle(file);
        return false;
    }

    DWORD read = 0;
    const BOOL ok = ReadFile(file, text, size, &read, nullptr);
    CloseHandle(file);
    if (!ok)
    {
        free(text);
        return false;
    }
    text[read] = '\0';
    *out = text;
    return true;
}

static void FileStem(const char* fileName, char* out, int outChars)
{
    lstrcpynA(out, fileName != nullptr ? fileName : "", outChars);
    char* dot = strrchr(out, '.');
    if (dot != nullptr)
    {
        *dot = '\0';
    }
}

static void SyncTemplateFiles(const char* folder)
{
    if (g_db == nullptr || folder == nullptr || folder[0] == '\0')
    {
        return;
    }

    char spec[MAX_PATH] = {};
    JoinPath(spec, MAX_PATH, folder, "*.json");

    WIN32_FIND_DATAA data = {};
    HANDLE find = FindFirstFileA(spec, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            continue;
        }

        char path[MAX_PATH] = {};
        JoinPath(path, MAX_PATH, folder, data.cFileName);

        char* text = nullptr;
        if (!ReadFileText(path, &text) || text == nullptr)
        {
            continue;
        }

        char id[64] = {};
        char name[128 * 3] = {};
        ExtractJsonStringField(text, "id", id, sizeof(id));
        ExtractJsonStringField(text, "name", name, sizeof(name));
        TextFromFile(name, sizeof(name));
        if (id[0] == '\0')
        {
            FileStem(data.cFileName, id, sizeof(id));
        }
        if (name[0] == '\0')
        {
            lstrcpynA(name, id, sizeof(name));
        }
        Store_UpsertTemplate(id, name, path, text);
        free(text);
    } while (FindNextFileA(find, &data));

    FindClose(find);
}

int Store_ListTemplates(StoreTemplate* out, int max)
{
    if (out == nullptr || max <= 0 || g_db == nullptr)
    {
        return 0;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "SELECT id, name, path FROM templates ORDER BY sort_order ASC, name ASC;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return 0;
    }

    int count = 0;
    while (count < max && sqlite3_step(statement) == SQLITE_ROW)
    {
        ZeroMemory(&out[count], sizeof(out[count]));
        const char* id = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        const char* path = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        lstrcpynA(out[count].id, id != nullptr ? id : "", sizeof(out[count].id));
        lstrcpynA(out[count].name, name != nullptr ? name : "", sizeof(out[count].name));
        lstrcpynA(out[count].path, path != nullptr ? path : "", sizeof(out[count].path));
        count += 1;
    }
    sqlite3_finalize(statement);
    return count;
}

bool Store_UpsertTemplate(const char* id, const char* name, const char* path, const char* layout)
{
    if (g_db == nullptr || id == nullptr || id[0] == '\0' || name == nullptr || name[0] == '\0' || path == nullptr || path[0] == '\0')
    {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "INSERT INTO templates(id, name, path, layout, updated, sort_order) "
            "VALUES(?1, ?2, ?3, ?4, ?5, COALESCE((SELECT MAX(sort_order) + 1 FROM templates), 0)) "
            "ON CONFLICT(id) DO UPDATE SET "
            "name = excluded.name, "
            "path = excluded.path, "
            "layout = COALESCE(excluded.layout, templates.layout), "
            "updated = excluded.updated;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, path, -1, SQLITE_STATIC);
    if (layout != nullptr)
    {
        sqlite3_bind_text(statement, 4, layout, -1, SQLITE_STATIC);
    }
    else
    {
        sqlite3_bind_null(statement, 4);
    }
    sqlite3_bind_int64(statement, 5, static_cast<sqlite3_int64>(time(nullptr)));
    const int result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    return result == SQLITE_DONE;
}

bool Store_DeleteTemplate(const char* id)
{
    if (g_db == nullptr || id == nullptr || id[0] == '\0')
    {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_db, "DELETE FROM templates WHERE id = ?1;", -1, &statement, nullptr) != SQLITE_OK)
    {
        return false;
    }
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    const int result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    return result == SQLITE_DONE;
}

bool Store_GetTemplate(const char* id, StoreTemplate* out)
{
    if (g_db == nullptr || id == nullptr || id[0] == '\0' || out == nullptr)
    {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            g_db,
            "SELECT id, name, path FROM templates WHERE id = ?1;",
            -1,
            &statement,
            nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    bool found = false;
    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        ZeroMemory(out, sizeof(*out));
        const char* foundId = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        const char* path = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        lstrcpynA(out->id, foundId != nullptr ? foundId : "", sizeof(out->id));
        lstrcpynA(out->name, name != nullptr ? name : "", sizeof(out->name));
        lstrcpynA(out->path, path != nullptr ? path : "", sizeof(out->path));
        found = true;
    }
    sqlite3_finalize(statement);
    return found;
}
