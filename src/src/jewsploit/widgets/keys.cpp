#include "keys.h"

#include <stdio.h>
#include <windows.h>

namespace
{
	const char* table[256]{};
	bool ready = false;

	void set(int vk, const char* s)
	{
		if (vk >= 0 && vk < 256)
		{
			table[vk] = s;
		}
	}

	void build()
	{
		if (ready)
		{
			return;
		}

		ready = true;

		set(0, "-");

		set(VK_LBUTTON, "LMB");
		set(VK_RBUTTON, "RMB");
		set(VK_MBUTTON, "M3");
		set(VK_XBUTTON1, "M4");
		set(VK_XBUTTON2, "M5");

		set(VK_BACK, "back");
		set(VK_TAB, "tab");
		set(VK_CLEAR, "clear");
		set(VK_RETURN, "enter");
		set(VK_SHIFT, "shift");
		set(VK_CONTROL, "ctrl");
		set(VK_MENU, "alt");
		set(VK_PAUSE, "pause");
		set(VK_CAPITAL, "caps");
		set(VK_ESCAPE, "esc");
		set(VK_SPACE, "space");
		set(VK_PRIOR, "pgup");
		set(VK_NEXT, "pgdn");
		set(VK_END, "end");
		set(VK_HOME, "home");
		set(VK_LEFT, "left");
		set(VK_UP, "up");
		set(VK_RIGHT, "right");
		set(VK_DOWN, "down");
		set(VK_SELECT, "select");
		set(VK_PRINT, "print");
		set(VK_EXECUTE, "exec");
		set(VK_SNAPSHOT, "prtsc");
		set(VK_INSERT, "ins");
		set(VK_DELETE, "del");
		set(VK_HELP, "help");

		set(0x30, "0");
		set(0x31, "1");
		set(0x32, "2");
		set(0x33, "3");
		set(0x34, "4");
		set(0x35, "5");
		set(0x36, "6");
		set(0x37, "7");
		set(0x38, "8");
		set(0x39, "9");

		set(0x41, "a");
		set(0x42, "b");
		set(0x43, "c");
		set(0x44, "d");
		set(0x45, "e");
		set(0x46, "f");
		set(0x47, "g");
		set(0x48, "h");
		set(0x49, "i");
		set(0x4A, "j");
		set(0x4B, "k");
		set(0x4C, "l");
		set(0x4D, "m");
		set(0x4E, "n");
		set(0x4F, "o");
		set(0x50, "p");
		set(0x51, "q");
		set(0x52, "r");
		set(0x53, "s");
		set(0x54, "t");
		set(0x55, "u");
		set(0x56, "v");
		set(0x57, "w");
		set(0x58, "x");
		set(0x59, "y");
		set(0x5A, "z");

		set(VK_LWIN, "lwin");
		set(VK_RWIN, "rwin");
		set(VK_APPS, "menu");
		set(VK_SLEEP, "sleep");

		set(VK_NUMPAD0, "num0");
		set(VK_NUMPAD1, "num1");
		set(VK_NUMPAD2, "num2");
		set(VK_NUMPAD3, "num3");
		set(VK_NUMPAD4, "num4");
		set(VK_NUMPAD5, "num5");
		set(VK_NUMPAD6, "num6");
		set(VK_NUMPAD7, "num7");
		set(VK_NUMPAD8, "num8");
		set(VK_NUMPAD9, "num9");
		set(VK_MULTIPLY, "num*");
		set(VK_ADD, "num+");
		set(VK_SEPARATOR, "numsep");
		set(VK_SUBTRACT, "num-");
		set(VK_DECIMAL, "num.");
		set(VK_DIVIDE, "num/");

		set(VK_F1, "f1");
		set(VK_F2, "f2");
		set(VK_F3, "f3");
		set(VK_F4, "f4");
		set(VK_F5, "f5");
		set(VK_F6, "f6");
		set(VK_F7, "f7");
		set(VK_F8, "f8");
		set(VK_F9, "f9");
		set(VK_F10, "f10");
		set(VK_F11, "f11");
		set(VK_F12, "f12");
		set(VK_F13, "f13");
		set(VK_F14, "f14");
		set(VK_F15, "f15");
		set(VK_F16, "f16");
		set(VK_F17, "f17");
		set(VK_F18, "f18");
		set(VK_F19, "f19");
		set(VK_F20, "f20");
		set(VK_F21, "f21");
		set(VK_F22, "f22");
		set(VK_F23, "f23");
		set(VK_F24, "f24");

		set(VK_NUMLOCK, "numlk");
		set(VK_SCROLL, "scroll");

		set(VK_LSHIFT, "lshift");
		set(VK_RSHIFT, "rshift");
		set(VK_LCONTROL, "lctrl");
		set(VK_RCONTROL, "rctrl");
		set(VK_LMENU, "lalt");
		set(VK_RMENU, "ralt");

		set(VK_BROWSER_BACK, "bbak");
		set(VK_BROWSER_FORWARD, "bfwd");
		set(VK_BROWSER_REFRESH, "bref");
		set(VK_BROWSER_STOP, "bstp");
		set(VK_BROWSER_SEARCH, "bse");
		set(VK_BROWSER_FAVORITES, "bfav");
		set(VK_BROWSER_HOME, "bhome");

		set(VK_VOLUME_MUTE, "mute");
		set(VK_VOLUME_DOWN, "voldn");
		set(VK_VOLUME_UP, "volup");
		set(VK_MEDIA_NEXT_TRACK, "next");
		set(VK_MEDIA_PREV_TRACK, "prev");
		set(VK_MEDIA_STOP, "stop");
		set(VK_MEDIA_PLAY_PAUSE, "play");

		set(VK_OEM_1, ";");
		set(VK_OEM_PLUS, "=");
		set(VK_OEM_COMMA, ",");
		set(VK_OEM_MINUS, "-");
		set(VK_OEM_PERIOD, ".");
		set(VK_OEM_2, "/");
		set(VK_OEM_3, "`");
		set(VK_OEM_4, "[");
		set(VK_OEM_5, "\\");
		set(VK_OEM_6, "]");
		set(VK_OEM_7, "'");

		set(VK_OEM_102, "oem102");
		set(VK_PROCESSKEY, "process");
		set(VK_ATTN, "attn");
		set(VK_CRSEL, "crsel");
		set(VK_EXSEL, "exsel");
		set(VK_EREOF, "ereof");
		set(VK_PLAY, "playk");
		set(VK_ZOOM, "zoom");
		set(VK_NONAME, "noname");
		set(VK_PA1, "pa1");
		set(VK_OEM_CLEAR, "oemclr");
	}
}

void keys::name(int vk, char* out, int out_n)
{
	if (!out || out_n <= 0)
	{
		return;
	}

	build();

	if (vk <= 0)
	{
		snprintf(out, out_n, "-");
		return;
	}

	if (vk < 256 && table[vk])
	{
		snprintf(out, out_n, "%s", table[vk]);
		return;
	}

	snprintf(out, out_n, "k%d", vk);
}