
#pragma once
#include "defines.h"

namespace C3D
{
    enum Keys : u16
    {
        /** @brief The Backspace key. */
        KeyBackspace = 0x08,
        /** @brief The Enter key. */
        KeyEnter = 0x0d,
        /** @brief The Tab key. */
        KeyTab = 0x09,
        /** @brief The Shift key. */
        KeyShift = 0x10,
        /** @brief The Control/Ctrl key. */
        KeyControl = 0x11,

        /** @brief The Pause key. */
        KeyPause = 0x13,
        /** @brief The Caps Lock key. */
        KeyCapslock = 0x14,

        /** @brief The Escape key. */
        KeyEscape = 0x1b,

        KeyConvert    = 0x1c,
        KeyNonConvert = 0x1d,
        KeyAccept     = 0x1e,
        KeyModeChange = 0x1f,

        /** @brief The SpaceBar key. */
        KeySpace    = 0x20,
        KeyPageUp   = 0x21,
        KeyPageDown = 0x22,
        /** @brief The End key. */
        KeyEnd = 0x23,
        /** @brief The Home key. */
        KeyHome = 0x24,
        /** @brief The Left Arrow key. */
        KeyArrowLeft = 0x25,
        /** @brief The Up Arrow key. */
        KeyArrowUp = 0x26,
        /** @brief The Right Arrow key. */
        KeyArrowRight = 0x27,
        /** @brief The Down Arrow key. */
        KeyArrowDown = 0x28,
        KeySelect    = 0x29,
        KeyPrint     = 0x2a,
        KeyExecute   = 0x2b,
        KeySnapshot  = 0x2c,
        /** @brief The Insert key. */
        KeyInsert = 0x2d,
        /** @brief The Delete key. */
        KeyDelete = 0x2e,
        KeyHelp   = 0x2f,
        /** @brief The 0 Key */
        Key0 = 0x30,
        /** @brief The 1 Key */
        Key1 = 0x31,
        /** @brief The 2 Key */
        Key2 = 0x32,
        /** @brief The 3 Key */
        Key3 = 0x33,
        /** @brief The 4 Key */
        Key4 = 0x34,
        /** @brief The 5 Key */
        Key5 = 0x35,
        /** @brief The 6 Key */
        Key6 = 0x36,
        /** @brief The 7 Key */
        Key7 = 0x37,
        /** @brief The 8 Key */
        Key8 = 0x38,
        /** @brief The 9 Key */
        Key9 = 0x39,
        /** @brief The A key. */
        KeyA = 'A',
        /** @brief The B key. */
        KeyB = 'B',
        /** @brief The C key. */
        KeyC = 'C',
        /** @brief The D key. */
        KeyD = 'D',
        /** @brief The E key. */
        KeyE = 'E',
        /** @brief The F key. */
        KeyF = 'F',
        /** @brief The G key. */
        KeyG = 'G',
        /** @brief The H key. */
        KeyH = 'H',
        /** @brief The I key. */
        KeyI = 'I',
        /** @brief The J key. */
        KeyJ = 'J',
        /** @brief The K key. */
        KeyK = 'K',
        /** @brief The L key. */
        KeyL = 'L',
        /** @brief The M key. */
        KeyM = 'M',
        /** @brief The N key. */
        KeyN = 'N',
        /** @brief The O key. */
        KeyO = 'O',
        /** @brief The P key. */
        KeyP = 'P',
        /** @brief The Q key. */
        KeyQ = 'Q',
        /** @brief The R key. */
        KeyR = 'R',
        /** @brief The S key. */
        KeyS = 'S',
        /** @brief The T key. */
        KeyT = 'T',
        /** @brief The U key. */
        KeyU = 'U',
        /** @brief The V key. */
        KeyV = 'V',
        /** @brief The W key. */
        KeyW = 'W',
        /** @brief The X key. */
        KeyX = 'X',
        /** @brief The Y key. */
        KeyY = 'Y',
        /** @brief The Z key. */
        KeyZ = 'Z',

        /** @brief The Left Windows/Super key. */
        KeyLSuper = 0x5b,
        /** @brief The Right Windows/Super key. */
        KeyRSuper = 0x5c,
        KeyApps   = 0x5d,
        /** @brief The Sleep key. */
        KeySleep = 0x5f,
        /** @brief The NumberPad 0 key. */
        KeyNumpad0 = 0x60,
        /** @brief The NumberPad 1 key. */
        KeyNumpad1 = 0x61,
        /** @brief The NumberPad 2 key. */
        KeyNumpad2 = 0x62,
        /** @brief The NumberPad 3 key. */
        KeyNumpad3 = 0x63,
        /** @brief The NumberPad 4 key. */
        KeyNumpad4 = 0x64,
        /** @brief The NumberPad 5 key. */
        KeyNumpad5 = 0x65,
        /** @brief The NumberPad 6 key. */
        KeyNumpad6 = 0x66,
        /** @brief The NumberPad 7 key. */
        KeyNumpad7 = 0x67,
        /** @brief The NumberPad 8 key. */
        KeyNumpad8 = 0x68,
        /** @brief The NumberPad 9 key. */
        KeyNumpad9 = 0x69,
        /** @brief The NumberPad Multiply key. */
        KeyMultiply = 0x6a,
        /** @brief The NumberPad Add key. */
        KeyAdd = 0x6b,
        /** @brief The NumberPad Separator key. */
        KeySeparator = 0x6c,
        /** @brief The NumberPad Subtract key. */
        KeySubtract = 0x6d,
        /** @brief The NumberPad Decimal key. */
        KeyDecimal = 0x6e,
        /** @brief The NumberPad Divide key. */
        KeyDivide = 0x6f,

        /** @brief The F1 key. */
        KeyF1 = 0x70,
        /** @brief The F2 key. */
        KeyF2 = 0x71,
        /** @brief The F3 key. */
        KeyF3 = 0x72,
        /** @brief The F4 key. */
        KeyF4 = 0x73,
        /** @brief The F5 key. */
        KeyF5 = 0x74,
        /** @brief The F6 key. */
        KeyF6 = 0x75,
        /** @brief The F7 key. */
        KeyF7 = 0x76,
        /** @brief The F8 key. */
        KeyF8 = 0x77,
        /** @brief The F9 key. */
        KeyF9 = 0x78,
        /** @brief The F10 key. */
        KeyF10 = 0x79,
        /** @brief The F11 key. */
        KeyF11 = 0x7a,
        /** @brief The F12 key. */
        KeyF12 = 0x7b,
        /** @brief The F13 key. */
        KeyF13 = 0x7c,
        /** @brief The F14 key. */
        KeyF14 = 0x7d,
        /** @brief The F15 key. */
        KeyF15 = 0x7e,
        /** @brief The F16 key. */
        KeyF16 = 0x7f,
        /** @brief The F17 key. */
        KeyF17 = 0x80,
        /** @brief The F18 key. */
        KeyF18 = 0x81,
        /** @brief The F19 key. */
        KeyF19 = 0x82,
        /** @brief The F20 key. */
        KeyF20 = 0x83,
        /** @brief The F21 key. */
        KeyF21 = 0x84,
        /** @brief The F22 key. */
        KeyF22 = 0x85,
        /** @brief The F23 key. */
        KeyF23 = 0x86,
        /** @brief The F24 key. */
        KeyF24 = 0x87,

        /** @brief The Number Lock key. */
        KeyNumLock = 0x90,

        /** @brief The Scroll Lock key. */
        KeyScroll = 0x91,

        /** @brief The NumberPad Equal key. */
        KeyNumpadEqual = 0x92,

        /** @brief The Left Shift key. */
        KeyLShift = 0xa0,
        /** @brief The Right Shift key. */
        KeyRShift = 0xa1,
        /** @brief The Left Control key. */
        KeyLControl = 0xa2,
        /** @brief The Right Control key. */
        KeyRControl = 0xa3,
        /** @brief The Left Alt key. */
        KeyLAlt = 0xa4,
        /** @brief The Right Alt key. */
        KeyRAlt = 0xa5,

        /** @brief The Semicolon key. */
        KeySemicolon = 0xba,
        /** @brief The Equals (=) key. */
        KeyEquals = 0xbb,
        /** @brief The Comma key. */
        KeyComma = 0xbc,
        /** @brief The minus (-) key. */
        KeyMinus = 0xbd,
        /** @brief The Period key. */
        KeyPeriod = 0xbe,
        /** @brief The Slash key.. */
        KeySlash = 0xbf,
        /** @brief The Grave key. */
        KeyGrave = 0xc0,
        /** @brief The Open Square Bracket ([) and Open Curly Bracket ({) key. */
        KeyOpenSquareBracket = 0xdb,
        /** @brief The Backwards Slash (\) and Vertical Pipe (|) key. */
        KeyBackwordsSlash = 0xdc,
        /** @brief The Close Square Bracket (]) and Close Curly Bracket (}) key.*/
        KeyCloseSquareBracket = 0xdd,
        /** @brief The Apostrophe (') and Quote (") key. */
        KeyApostrophe = 0xde,

        MaxKeys,
    };
}