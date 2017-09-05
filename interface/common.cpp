/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-ui-console - common.c                                     *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2007-2010 Richard42                                     *
 *   Copyright (C) 2008 Ebenblues Nmn Okaygo Tillin9                       *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <minizip/unzip.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "common.h"
#include "plugin.h"
#include "core_interface.h"
#include <SDL2/SDL_keycode.h>
#include "version.h"
#include "cheat.h"

/** global variables **/
int    g_Verbose = 0;

/*********************************************************************************************************
 *  Callback functions from the core
 */

void DebugMessage(int level, const char *message, ...)
{
  char msgbuf[1024];
  va_list args;

  va_start(args, message);
  vsnprintf(msgbuf, 1024, message, args);

  DebugCallback((char*)"GUI", level, msgbuf);

  va_end(args);
}

void DebugCallback(void *Context, int level, const char *message)
{
    if (level == M64MSG_ERROR)
        printf("%s Error: %s\n", (const char *) Context, message);
    else if (level == M64MSG_WARNING)
        printf("%s Warning: %s\n", (const char *) Context, message);
    else if (level == M64MSG_INFO)
        printf("%s: %s\n", (const char *) Context, message);
    else if (level == M64MSG_STATUS)
        printf("%s Status: %s\n", (const char *) Context, message);
    else if (level == M64MSG_VERBOSE)
    {
        if (g_Verbose)
            printf("%s: %s\n", (const char *) Context, message);
    }
    else
        printf("%s Unknown: %s\n", (const char *) Context, message);
}

m64p_error openROM(std::string filename)
{
    unsigned char *ROM_buffer = NULL;
    size_t romlength = 0;
    uint32_t i;

    if ((filename.find(".zip") != std::string::npos) || (filename.find(".ZIP") != std::string::npos))
    {
        unzFile uf = NULL;
        unz_global_info gi;
        unz_file_info file_info;
        uf = unzOpen(filename.c_str());
        if (uf == NULL)
        {
            DebugMessage(M64MSG_ERROR, "couldn't open ZIP file '%s' for reading.", filename.c_str());
            (*CoreShutdown)();
            DetachCoreLib();
            return M64ERR_INVALID_STATE;
        }
        int err;
        int found = 0;
        err = unzGetGlobalInfo(uf,&gi);
        if (err != UNZ_OK)
        {
            DebugMessage(M64MSG_ERROR, "couldn't open ZIP file '%s' for reading.", filename.c_str());
            unzClose(uf);
            (*CoreShutdown)();
            DetachCoreLib();
            return M64ERR_INVALID_STATE;
        }
        for (i=0;i<gi.number_entry;++i)
        {
            char filename_inzip[256];
            unzGetCurrentFileInfo(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);
            if (strstr(filename_inzip, ".n64") != NULL ||
                strstr(filename_inzip, ".N64") != NULL ||
                strstr(filename_inzip, ".z64") != NULL ||
                strstr(filename_inzip, ".Z64") != NULL ||
                strstr(filename_inzip, ".v64") != NULL ||
                strstr(filename_inzip, ".V64") != NULL)
            {
                found = 1;
                break;
            }
            unzGoToNextFile(uf);
        }
        if (found)
        {
            err = unzOpenCurrentFile(uf);
            if (err != UNZ_OK)
            {
                DebugMessage(M64MSG_ERROR, "couldn't open ZIP file '%s' for reading.", filename.c_str());
                unzClose(uf);
                (*CoreShutdown)();
                DetachCoreLib();
                return M64ERR_INVALID_STATE;
            }
            romlength = file_info.uncompressed_size;
            ROM_buffer = (unsigned char *) malloc(romlength);
            err = unzReadCurrentFile(uf, ROM_buffer, romlength);
            if (err < 0)
            {
                DebugMessage(M64MSG_ERROR, "couldn't open ZIP file '%s' for reading.", filename.c_str());
                free(ROM_buffer);
                unzCloseCurrentFile(uf);
                unzClose(uf);
                (*CoreShutdown)();
                DetachCoreLib();
                return M64ERR_INVALID_STATE;
            }
            unzCloseCurrentFile(uf);
        }
        unzClose(uf);
    }
    else
    {
        /* load ROM image */
        FILE *fPtr = fopen(filename.c_str(), "rb");
        if (fPtr == NULL)
        {
            DebugMessage(M64MSG_ERROR, "couldn't open ROM file '%s' for reading.", filename.c_str());
            (*CoreShutdown)();
            DetachCoreLib();
            return M64ERR_INVALID_STATE;
        }

        /* get the length of the ROM, allocate memory buffer, load it from disk */
        fseek(fPtr, 0L, SEEK_END);
        romlength = (size_t)ftell(fPtr);
        fseek(fPtr, 0L, SEEK_SET);
        ROM_buffer = (unsigned char *) malloc(romlength);
        if (fread(ROM_buffer, 1, romlength, fPtr) != romlength)
        {
            DebugMessage(M64MSG_ERROR, "couldn't read %li bytes from ROM image file '%s'.", romlength, filename.c_str());
            free(ROM_buffer);
            fclose(fPtr);
            (*CoreShutdown)();
            DetachCoreLib();
            return M64ERR_INVALID_STATE;
        }
        fclose(fPtr);
    }

    /* Try to load the ROM image into the core */
    if ((*CoreDoCommand)(M64CMD_ROM_OPEN, (int) romlength, ROM_buffer) != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "core failed to open ROM image file '%s'.", filename.c_str());
        free(ROM_buffer);
        (*CoreShutdown)();
        DetachCoreLib();
        return M64ERR_INVALID_STATE;
    }
    free(ROM_buffer); /* the core copies the ROM image, so we can release this buffer immediately */

    /* search for and load plugins */
    m64p_error rval = PluginSearchLoad();
    if (rval != M64ERR_SUCCESS)
    {
        (*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
        (*CoreShutdown)();
        DetachCoreLib();
        return M64ERR_INVALID_STATE;
    }

    int gfx_hle = -1;
    m64p_handle rspHandle;
    if (g_PluginMap[3].libname == "Static Interpreter" && g_PluginMap[0].libname == "Angrylion RDP Plus GFX Plugin")
    {
        (*ConfigOpenSection)("rsp-cxd4", &rspHandle);
        gfx_hle = (*ConfigGetParamBool)(rspHandle, "DisplayListToGraphicsPlugin");
        int temp_value = 0;
        (*ConfigSetParameter)(rspHandle, "DisplayListToGraphicsPlugin", M64TYPE_BOOL, &temp_value);
    }

    /* attach plugins to core */
    for (i = 0; i < 4; i++)
    {
        if ((*CoreAttachPlugin)(g_PluginMap[i].type, g_PluginMap[i].handle) != M64ERR_SUCCESS)
        {
            DebugMessage(M64MSG_ERROR, "core error while attaching %s plugin.", g_PluginMap[i].name);
            (*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
            (*CoreShutdown)();
            DetachCoreLib();
            return M64ERR_INVALID_STATE;
        }
    }
    m64p_rom_header    l_RomHeader;
    /* get the ROM header for the currently loaded ROM image from the core */
    if ((*CoreDoCommand)(M64CMD_ROM_GET_HEADER, sizeof(l_RomHeader), &l_RomHeader) != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_WARNING, "couldn't get ROM header information from core library");
        return M64ERR_INVALID_STATE;
    }

    /* generate section name from ROM's CRC and country code */
    char RomSection[24];
    sprintf(RomSection, "%08X-%08X-C:%X", sl(l_RomHeader.CRC1), sl(l_RomHeader.CRC2), l_RomHeader.Country_code & 0xff);

    /* parse through the cheat INI file and load up any cheat codes found for this ROM */
    ReadCheats(RomSection);
    if (!l_RomFound || l_CheatCodesFound == 0)
    {
        DebugMessage(M64MSG_WARNING, "no cheat codes found for ROM image '%.20s'", l_RomHeader.Name);
    }

    /* run the game */
    (*CoreDoCommand)(M64CMD_EXECUTE, 0, NULL);

    if (gfx_hle != -1)
    {
        (*ConfigSetParameter)(rspHandle, "DisplayListToGraphicsPlugin", M64TYPE_BOOL, &gfx_hle);
    }

    /* detach plugins from core and unload them */
    for (i = 0; i < 4; i++)
        (*CoreDetachPlugin)(g_PluginMap[i].type);
    PluginUnload();

    /* close the ROM image */
    (*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);

    CheatFreeAll();

    return M64ERR_SUCCESS;
}

int QT2SDL2MOD(Qt::KeyboardModifiers modifiers)
{
    int value = 0;
    if (modifiers & Qt::ShiftModifier)
        value |= KMOD_SHIFT;
    if (modifiers & Qt::ControlModifier)
        value |= KMOD_CTRL;
    if (modifiers & Qt::AltModifier)
        value |= KMOD_ALT;
    if (modifiers & Qt::MetaModifier)
        value |= KMOD_GUI;
    return value;
}

int QT2SDL2(int qtKey)
{
    SDL_Scancode returnValue;
    switch (qtKey) {
    case Qt::Key_Escape:
        returnValue = SDL_SCANCODE_ESCAPE;
        break;
    case Qt::Key_Tab:
        returnValue = SDL_SCANCODE_TAB;
        break;
    case Qt::Key_Backspace:
        returnValue = SDL_SCANCODE_BACKSPACE;
        break;
    case Qt::Key_Return:
        returnValue = SDL_SCANCODE_RETURN;
        break;
    case Qt::Key_Enter:
        returnValue = SDL_SCANCODE_KP_ENTER;
        break;
    case Qt::Key_Insert:
        returnValue = SDL_SCANCODE_INSERT;
        break;
    case Qt::Key_Delete:
        returnValue = SDL_SCANCODE_DELETE;
        break;
    case Qt::Key_Pause:
        returnValue = SDL_SCANCODE_PAUSE;
        break;
    case Qt::Key_Print:
        returnValue = SDL_SCANCODE_PRINTSCREEN;
        break;
    case Qt::Key_SysReq:
        returnValue = SDL_SCANCODE_SYSREQ;
        break;
    case Qt::Key_Clear:
        returnValue = SDL_SCANCODE_CLEAR;
        break;
    case Qt::Key_Home:
        returnValue = SDL_SCANCODE_HOME;
        break;
    case Qt::Key_End:
        returnValue = SDL_SCANCODE_END;
        break;
    case Qt::Key_Left:
        returnValue = SDL_SCANCODE_LEFT;
        break;
    case Qt::Key_Right:
        returnValue = SDL_SCANCODE_RIGHT;
        break;
    case Qt::Key_Up:
        returnValue = SDL_SCANCODE_UP;
        break;
    case Qt::Key_Down:
        returnValue = SDL_SCANCODE_DOWN;
        break;
    case Qt::Key_PageUp:
        returnValue = SDL_SCANCODE_PAGEUP;
        break;
    case Qt::Key_PageDown:
        returnValue = SDL_SCANCODE_PAGEDOWN;
        break;
    case Qt::Key_Shift:
        returnValue = SDL_SCANCODE_LSHIFT;
        break;
    case Qt::Key_Control:
        returnValue = SDL_SCANCODE_LCTRL;
        break;
    case Qt::Key_Meta:
        returnValue = SDL_SCANCODE_LGUI;
        break;
    case Qt::Key_Alt:
        returnValue = SDL_SCANCODE_LALT;
        break;
    case Qt::Key_AltGr:
        returnValue = SDL_SCANCODE_RALT;
        break;
    case Qt::Key_CapsLock:
        returnValue = SDL_SCANCODE_CAPSLOCK;
        break;
    case Qt::Key_NumLock:
        returnValue = SDL_SCANCODE_NUMLOCKCLEAR;
        break;
    case Qt::Key_ScrollLock:
        returnValue = SDL_SCANCODE_SCROLLLOCK;
        break;
    case Qt::Key_F1:
        returnValue = SDL_SCANCODE_F1;
        break;
    case Qt::Key_F2:
        returnValue = SDL_SCANCODE_F2;
        break;
    case Qt::Key_F3:
        returnValue = SDL_SCANCODE_F3;
        break;
    case Qt::Key_F4:
        returnValue = SDL_SCANCODE_F4;
        break;
    case Qt::Key_F5:
        returnValue = SDL_SCANCODE_F5;
        break;
    case Qt::Key_F6:
        returnValue = SDL_SCANCODE_F6;
        break;
    case Qt::Key_F7:
        returnValue = SDL_SCANCODE_F7;
        break;
    case Qt::Key_F8:
        returnValue = SDL_SCANCODE_F8;
        break;
    case Qt::Key_F9:
        returnValue = SDL_SCANCODE_F9;
        break;
    case Qt::Key_F10:
        returnValue = SDL_SCANCODE_F10;
        break;
    case Qt::Key_F11:
        returnValue = SDL_SCANCODE_F11;
        break;
    case Qt::Key_F12:
        returnValue = SDL_SCANCODE_F12;
        break;
    case Qt::Key_F13:
        returnValue = SDL_SCANCODE_F13;
        break;
    case Qt::Key_F14:
        returnValue = SDL_SCANCODE_F14;
        break;
    case Qt::Key_F15:
        returnValue = SDL_SCANCODE_F15;
        break;
    case Qt::Key_F16:
        returnValue = SDL_SCANCODE_F16;
        break;
    case Qt::Key_F17:
        returnValue = SDL_SCANCODE_F17;
        break;
    case Qt::Key_F18:
        returnValue = SDL_SCANCODE_F18;
        break;
    case Qt::Key_F19:
        returnValue = SDL_SCANCODE_F19;
        break;
    case Qt::Key_F20:
        returnValue = SDL_SCANCODE_F20;
        break;
    case Qt::Key_F21:
        returnValue = SDL_SCANCODE_F21;
        break;
    case Qt::Key_F22:
        returnValue = SDL_SCANCODE_F22;
        break;
    case Qt::Key_F23:
        returnValue = SDL_SCANCODE_F23;
        break;
    case Qt::Key_F24:
        returnValue = SDL_SCANCODE_F24;
        break;
    case Qt::Key_Space:
        returnValue = SDL_SCANCODE_SPACE;
        break;
    case Qt::Key_0:
        returnValue = SDL_SCANCODE_0;
        break;
    case Qt::Key_1:
        returnValue = SDL_SCANCODE_1;
        break;
    case Qt::Key_2:
        returnValue = SDL_SCANCODE_2;
        break;
    case Qt::Key_3:
        returnValue = SDL_SCANCODE_3;
        break;
    case Qt::Key_4:
        returnValue = SDL_SCANCODE_4;
        break;
    case Qt::Key_5:
        returnValue = SDL_SCANCODE_5;
        break;
    case Qt::Key_6:
        returnValue = SDL_SCANCODE_6;
        break;
    case Qt::Key_7:
        returnValue = SDL_SCANCODE_7;
        break;
    case Qt::Key_8:
        returnValue = SDL_SCANCODE_8;
        break;
    case Qt::Key_9:
        returnValue = SDL_SCANCODE_9;
        break;
    case Qt::Key_A:
        returnValue = SDL_SCANCODE_A;
        break;
    case Qt::Key_B:
        returnValue = SDL_SCANCODE_B;
        break;
    case Qt::Key_C:
        returnValue = SDL_SCANCODE_C;
        break;
    case Qt::Key_D:
        returnValue = SDL_SCANCODE_D;
        break;
    case Qt::Key_E:
        returnValue = SDL_SCANCODE_E;
        break;
    case Qt::Key_F:
        returnValue = SDL_SCANCODE_F;
        break;
    case Qt::Key_G:
        returnValue = SDL_SCANCODE_G;
        break;
    case Qt::Key_H:
        returnValue = SDL_SCANCODE_H;
        break;
    case Qt::Key_I:
        returnValue = SDL_SCANCODE_I;
        break;
    case Qt::Key_J:
        returnValue = SDL_SCANCODE_J;
        break;
    case Qt::Key_K:
        returnValue = SDL_SCANCODE_K;
        break;
    case Qt::Key_L:
        returnValue = SDL_SCANCODE_L;
        break;
    case Qt::Key_M:
        returnValue = SDL_SCANCODE_M;
        break;
    case Qt::Key_N:
        returnValue = SDL_SCANCODE_N;
        break;
    case Qt::Key_O:
        returnValue = SDL_SCANCODE_O;
        break;
    case Qt::Key_P:
        returnValue = SDL_SCANCODE_P;
        break;
    case Qt::Key_Q:
        returnValue = SDL_SCANCODE_Q;
        break;
    case Qt::Key_R:
        returnValue = SDL_SCANCODE_R;
        break;
    case Qt::Key_S:
        returnValue = SDL_SCANCODE_S;
        break;
    case Qt::Key_T:
        returnValue = SDL_SCANCODE_T;
        break;
    case Qt::Key_U:
        returnValue = SDL_SCANCODE_U;
        break;
    case Qt::Key_V:
        returnValue = SDL_SCANCODE_V;
        break;
    case Qt::Key_W:
        returnValue = SDL_SCANCODE_W;
        break;
    case Qt::Key_X:
        returnValue = SDL_SCANCODE_X;
        break;
    case Qt::Key_Y:
        returnValue = SDL_SCANCODE_Y;
        break;
    case Qt::Key_Z:
        returnValue = SDL_SCANCODE_Z;
        break;
    case Qt::Key_BracketLeft:
        returnValue = SDL_SCANCODE_LEFTBRACKET;
        break;
    case Qt::Key_BracketRight:
        returnValue = SDL_SCANCODE_RIGHTBRACKET;
        break;
    case Qt::Key_Minus:
        returnValue = SDL_SCANCODE_MINUS;
        break;
    case Qt::Key_Semicolon:
        returnValue = SDL_SCANCODE_SEMICOLON;
        break;
    case Qt::Key_Slash:
        returnValue = SDL_SCANCODE_SLASH;
        break;
    case Qt::Key_Backslash:
        returnValue = SDL_SCANCODE_BACKSLASH;
        break;
    case Qt::Key_Apostrophe:
        returnValue = SDL_SCANCODE_APOSTROPHE;
        break;
    case Qt::Key_Comma:
        returnValue = SDL_SCANCODE_COMMA;
        break;
    case Qt::Key_Period:
        returnValue = SDL_SCANCODE_PERIOD;
        break;
    case Qt::Key_Equal:
        returnValue = SDL_SCANCODE_EQUALS;
        break;
    case Qt::Key_QuoteLeft:
        returnValue = SDL_SCANCODE_GRAVE;
        break;
    default:
        returnValue = SDL_SCANCODE_UNKNOWN;
        break;
    }

    return returnValue;
}
