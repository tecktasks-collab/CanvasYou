#include "boot.h"
#include "store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGRAM_WALL_DEFAULT 240
#define PROGRAM_WALL_MIN 80

static void TicketPath(char* out, int outChars, const char* root)
{
    lstrcpynA(out, root, outChars);
    const int len = lstrlenA(out);
    if (len > 0 && out[len - 1] != '\\' && len + 1 < outChars)
    {
        out[len] = '\\';
        out[len + 1] = '\0';
    }
    lstrcatA(out, "boot.ticket");
}

void Boot_Defaults(BootTicket* ticket)
{
    if (ticket == nullptr)
    {
        return;
    }

    ZeroMemory(ticket, sizeof(*ticket));
    ticket->leftWidth = PROGRAM_WALL_DEFAULT;
    ticket->rightWidth = PROGRAM_WALL_DEFAULT;
    ticket->screenCount = 0;
}

bool Boot_Write(const BootTicket* ticket)
{
    if (ticket == nullptr || ticket->dataRoot[0] == '\0')
    {
        return false;
    }

    char path[MAX_PATH] = {};
    TicketPath(path, MAX_PATH, ticket->dataRoot);

    char text[4096] = {};
    sprintf_s(
        text,
        sizeof(text),
        "root=%s\r\nleft=%d\r\nright=%d\r\ndocument=%s\r\nscreens=%d\r\n",
        ticket->dataRoot,
        ticket->leftWidth,
        ticket->rightWidth,
        ticket->document,
        ticket->screenCount
    );

    for (int i = 0; i < ticket->screenCount && i < BOOT_MAX_SCREENS; ++i)
    {
        char line[96] = {};
        sprintf_s(line, sizeof(line), "screen%d=%s\r\n", i, ticket->screens[i]);
        if (lstrlenA(text) + lstrlenA(line) < static_cast<int>(sizeof(text)))
        {
            lstrcatA(text, line);
        }
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD written = 0;
    const DWORD bytes = static_cast<DWORD>(lstrlenA(text));
    const BOOL ok = WriteFile(file, text, bytes, &written, nullptr);
    CloseHandle(file);
    return ok == TRUE && written == bytes;
}

bool Boot_Read(BootTicket* ticket)
{
    if (ticket == nullptr)
    {
        return false;
    }

    Boot_Defaults(ticket);

    char root[MAX_PATH] = {};
    if (!Store_ResolveRoot(root, MAX_PATH))
    {
        return false;
    }

    char path[MAX_PATH] = {};
    TicketPath(path, MAX_PATH, root);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    char text[4096] = {};
    DWORD read = 0;
    ReadFile(file, text, sizeof(text) - 1, &read, nullptr);
    CloseHandle(file);

    lstrcpynA(ticket->dataRoot, root, MAX_PATH);
    char* line = text;
    while (line != nullptr && line[0] != '\0')
    {
        char* end = strstr(line, "\r\n");
        if (end != nullptr)
        {
            *end = '\0';
        }

        if (strncmp(line, "root=", 5) == 0)
        {
            lstrcpynA(ticket->dataRoot, line + 5, MAX_PATH);
        }
        else if (strncmp(line, "left=", 5) == 0)
        {
            ticket->leftWidth = atoi(line + 5);
        }
        else if (strncmp(line, "right=", 6) == 0)
        {
            ticket->rightWidth = atoi(line + 6);
        }
        else if (strncmp(line, "document=", 9) == 0)
        {
            lstrcpynA(ticket->document, line + 9, MAX_PATH);
        }
        else if (strncmp(line, "screens=", 8) == 0)
        {
            ticket->screenCount = atoi(line + 8);
        }
        else if (strncmp(line, "screen", 6) == 0)
        {
            const char* digits = line + 6;
            const int index = atoi(digits);
            const char* eq = strchr(line, '=');
            if (eq != nullptr && index >= 0 && index < BOOT_MAX_SCREENS)
            {
                lstrcpynA(ticket->screens[index], eq + 1, 64);
            }
        }

        if (end == nullptr)
        {
            break;
        }
        line = end + 2;
    }

    if (ticket->leftWidth < PROGRAM_WALL_MIN)
    {
        ticket->leftWidth = PROGRAM_WALL_MIN;
    }
    if (ticket->rightWidth < PROGRAM_WALL_MIN)
    {
        ticket->rightWidth = PROGRAM_WALL_MIN;
    }
    if (ticket->screenCount < 0)
    {
        ticket->screenCount = 0;
    }
    if (ticket->screenCount > BOOT_MAX_SCREENS)
    {
        ticket->screenCount = BOOT_MAX_SCREENS;
    }

    return ticket->dataRoot[0] != '\0';
}
