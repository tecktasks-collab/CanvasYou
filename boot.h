#pragma once

#include <windows.h>

#define BOOT_MAX_SCREENS 8

struct BootTicket
{
    char dataRoot[MAX_PATH];
    int leftWidth;
    int rightWidth;
    char document[MAX_PATH];
    int screenCount;
    char screens[BOOT_MAX_SCREENS][64];
};

void Boot_Defaults(BootTicket* ticket);
bool Boot_Write(const BootTicket* ticket);
bool Boot_Read(BootTicket* ticket);
