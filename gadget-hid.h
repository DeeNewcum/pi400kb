#include <linux/usb/ch9.h>
#include <usbg/usbg.h>
#include <usbg/function/hid.h>

//#define KEYBOARD_VID             0x04d9
//#define KEYBOARD_PID             0x0007
//#define KEYBOARD_DEV             "/dev/input/by-id/usb-_Raspberry_Pi_Internal_Keyboard-event-kbd"
#define KEYBOARD_HID_REPORT_SIZE 8

//#define MOUSE_VID                0x093a
//#define MOUSE_PID                0x2510
//#define MOUSE_DEV                "/dev/input/by-id/usb-PixArt_USB_Optical_Mouse-event-mouse"
#define MOUSE_HID_REPORT_SIZE    4

struct hid_buf {
    uint8_t report_id;
    unsigned char data[64];
}  __attribute__ ((aligned (1)));

// see https://wiki.osdev.org/USB_Human_Interface_Devices#USB_keyboard
struct keyboard_hid_buf {
    uint8_t report_id;
    uint8_t modifier;
    uint8_t reserved;       // always zero
    uint8_t keycode[6];     // Note: This limits the keyboard to 6KRO (which the Raspberry Pi 400 fits into).
                            //       See https://www.devever.net/~hl/usbnkro
}  __attribute__ ((aligned (1)));


int initUSB();
int cleanupUSB();


/**
 * Modifier masks - used for the first byte in the HID report.
 */
#define KEY_MOD_LCTRL  0x01
#define KEY_MOD_LSHIFT 0x02
#define KEY_MOD_LALT   0x04
#define KEY_MOD_LMETA  0x08     // Raspberry
#define KEY_MOD_RCTRL  0x10
#define KEY_MOD_RSHIFT 0x20
#define KEY_MOD_RALT   0x40
#define KEY_MOD_RMETA  0x80

/**
 * Scan codes - last N slots in the HID report (usually 6).
 * 0x00 if no key pressed.
 * 
 * If more than N keys are pressed, the HID reports 
 * KEY_ERR_OVF in all slots to indicate this condition.
 * 
 * See https://usb.org/sites/default/files/hut1_2.pdf pages 83 - 94.
 */
#define KEYCODE_NONE 0x00 // No key pressed
#define KEYCODE_ERR_OVF 0x01 //  Keyboard Error Roll Over - used for all slots if too many keys are pressed ("Phantom key")
// 0x02 //  Keyboard POST Fail
// 0x03 //  Keyboard Error Undefined
#define KEYCODE_A 0x04 // Keyboard a and A
#define KEYCODE_B 0x05 // Keyboard b and B
#define KEYCODE_C 0x06 // Keyboard c and C
#define KEYCODE_D 0x07 // Keyboard d and D
#define KEYCODE_E 0x08 // Keyboard e and E
#define KEYCODE_F 0x09 // Keyboard f and F
#define KEYCODE_G 0x0a // Keyboard g and G
#define KEYCODE_H 0x0b // Keyboard h and H
#define KEYCODE_I 0x0c // Keyboard i and I
#define KEYCODE_J 0x0d // Keyboard j and J
#define KEYCODE_K 0x0e // Keyboard k and K
#define KEYCODE_L 0x0f // Keyboard l and L
#define KEYCODE_M 0x10 // Keyboard m and M
#define KEYCODE_N 0x11 // Keyboard n and N
#define KEYCODE_O 0x12 // Keyboard o and O
#define KEYCODE_P 0x13 // Keyboard p and P
#define KEYCODE_Q 0x14 // Keyboard q and Q
#define KEYCODE_R 0x15 // Keyboard r and R
#define KEYCODE_S 0x16 // Keyboard s and S
#define KEYCODE_T 0x17 // Keyboard t and T
#define KEYCODE_U 0x18 // Keyboard u and U
#define KEYCODE_V 0x19 // Keyboard v and V
#define KEYCODE_W 0x1a // Keyboard w and W
#define KEYCODE_X 0x1b // Keyboard x and X
#define KEYCODE_Y 0x1c // Keyboard y and Y
#define KEYCODE_Z 0x1d // Keyboard z and Z

#define KEYCODE_1 0x1e // Keyboard 1 and !
#define KEYCODE_2 0x1f // Keyboard 2 and @
#define KEYCODE_3 0x20 // Keyboard 3 and #
#define KEYCODE_4 0x21 // Keyboard 4 and $
#define KEYCODE_5 0x22 // Keyboard 5 and %
#define KEYCODE_6 0x23 // Keyboard 6 and ^
#define KEYCODE_7 0x24 // Keyboard 7 and &
#define KEYCODE_8 0x25 // Keyboard 8 and *
#define KEYCODE_9 0x26 // Keyboard 9 and (
#define KEYCODE_0 0x27 // Keyboard 0 and )

#define KEYCODE_ENTER 0x28 // Keyboard Return (ENTER)
#define KEYCODE_ESC 0x29 // Keyboard ESCAPE
#define KEYCODE_BACKSPACE 0x2a // Keyboard DELETE (Backspace)
#define KEYCODE_TAB 0x2b // Keyboard Tab
#define KEYCODE_SPACE 0x2c // Keyboard Spacebar
#define KEYCODE_MINUS 0x2d // Keyboard - and _
#define KEYCODE_EQUAL 0x2e // Keyboard = and +
#define KEYCODE_LEFTBRACE 0x2f // Keyboard [ and {
#define KEYCODE_RIGHTBRACE 0x30 // Keyboard ] and }
#define KEYCODE_BACKSLASH 0x31 // Keyboard \ and |
#define KEYCODE_HASHTILDE 0x32 // Keyboard Non-US # and ~
#define KEYCODE_SEMICOLON 0x33 // Keyboard ; and :
#define KEYCODE_APOSTROPHE 0x34 // Keyboard ' and "
#define KEYCODE_GRAVE 0x35 // Keyboard ` and ~
#define KEYCODE_COMMA 0x36 // Keyboard , and <
#define KEYCODE_DOT 0x37 // Keyboard . and >
#define KEYCODE_SLASH 0x38 // Keyboard / and ?
#define KEYCODE_CAPSLOCK 0x39 // Keyboard Caps Lock

#define KEYCODE_F1 0x3a // Keyboard F1
#define KEYCODE_F2 0x3b // Keyboard F2
#define KEYCODE_F3 0x3c // Keyboard F3
#define KEYCODE_F4 0x3d // Keyboard F4
#define KEYCODE_F5 0x3e // Keyboard F5
#define KEYCODE_F6 0x3f // Keyboard F6
#define KEYCODE_F7 0x40 // Keyboard F7
#define KEYCODE_F8 0x41 // Keyboard F8
#define KEYCODE_F9 0x42 // Keyboard F9
#define KEYCODE_F10 0x43 // Keyboard F10
#define KEYCODE_F11 0x44 // Keyboard F11
#define KEYCODE_F12 0x45 // Keyboard F12

#define KEYCODE_SYSRQ 0x46 // Keyboard Print Screen
#define KEYCODE_SCROLLLOCK 0x47 // Keyboard Scroll Lock
#define KEYCODE_PAUSE 0x48 // Keyboard Pause
#define KEYCODE_INSERT 0x49 // Keyboard Insert
#define KEYCODE_HOME 0x4a // Keyboard Home
#define KEYCODE_PAGEUP 0x4b // Keyboard Page Up
#define KEYCODE_DELETE 0x4c // Keyboard Delete Forward
#define KEYCODE_END 0x4d // Keyboard End
#define KEYCODE_PAGEDOWN 0x4e // Keyboard Page Down
#define KEYCODE_RIGHT 0x4f // Keyboard Right Arrow
#define KEYCODE_LEFT 0x50 // Keyboard Left Arrow
#define KEYCODE_DOWN 0x51 // Keyboard Down Arrow
#define KEYCODE_UP 0x52 // Keyboard Up Arrow

#define KEYCODE_NUMLOCK 0x53 // Keyboard Num Lock and Clear
#define KEYCODE_KPSLASH 0x54 // Keypad /
#define KEYCODE_KPASTERISK 0x55 // Keypad *
#define KEYCODE_KPMINUS 0x56 // Keypad -
#define KEYCODE_KPPLUS 0x57 // Keypad +
#define KEYCODE_KPENTER 0x58 // Keypad ENTER
#define KEYCODE_KP1 0x59 // Keypad 1 and End
#define KEYCODE_KP2 0x5a // Keypad 2 and Down Arrow
#define KEYCODE_KP3 0x5b // Keypad 3 and PageDn
#define KEYCODE_KP4 0x5c // Keypad 4 and Left Arrow
#define KEYCODE_KP5 0x5d // Keypad 5
#define KEYCODE_KP6 0x5e // Keypad 6 and Right Arrow
#define KEYCODE_KP7 0x5f // Keypad 7 and Home
#define KEYCODE_KP8 0x60 // Keypad 8 and Up Arrow
#define KEYCODE_KP9 0x61 // Keypad 9 and Page Up
#define KEYCODE_KP0 0x62 // Keypad 0 and Insert
#define KEYCODE_KPDOT 0x63 // Keypad . and Delete

#define KEYCODE_102ND 0x64 // Keyboard Non-US \ and |
#define KEYCODE_COMPOSE 0x65 // Keyboard Application
#define KEYCODE_POWER 0x66 // Keyboard Power
#define KEYCODE_KPEQUAL 0x67 // Keypad =

#define KEYCODE_F13 0x68 // Keyboard F13
#define KEYCODE_F14 0x69 // Keyboard F14
#define KEYCODE_F15 0x6a // Keyboard F15
#define KEYCODE_F16 0x6b // Keyboard F16
#define KEYCODE_F17 0x6c // Keyboard F17
#define KEYCODE_F18 0x6d // Keyboard F18
#define KEYCODE_F19 0x6e // Keyboard F19
#define KEYCODE_F20 0x6f // Keyboard F20
#define KEYCODE_F21 0x70 // Keyboard F21
#define KEYCODE_F22 0x71 // Keyboard F22
#define KEYCODE_F23 0x72 // Keyboard F23
#define KEYCODE_F24 0x73 // Keyboard F24

#define KEYCODE_OPEN 0x74 // Keyboard Execute
#define KEYCODE_HELP 0x75 // Keyboard Help
#define KEYCODE_PROPS 0x76 // Keyboard Menu
#define KEYCODE_FRONT 0x77 // Keyboard Select
#define KEYCODE_STOP 0x78 // Keyboard Stop
#define KEYCODE_AGAIN 0x79 // Keyboard Again
#define KEYCODE_UNDO 0x7a // Keyboard Undo
#define KEYCODE_CUT 0x7b // Keyboard Cut
#define KEYCODE_COPY 0x7c // Keyboard Copy
#define KEYCODE_PASTE 0x7d // Keyboard Paste
#define KEYCODE_FIND 0x7e // Keyboard Find
#define KEYCODE_MUTE 0x7f // Keyboard Mute
#define KEYCODE_VOLUMEUP 0x80 // Keyboard Volume Up
#define KEYCODE_VOLUMEDOWN 0x81 // Keyboard Volume Down
// 0x82  Keyboard Locking Caps Lock
// 0x83  Keyboard Locking Num Lock
// 0x84  Keyboard Locking Scroll Lock
#define KEYCODE_KPCOMMA 0x85 // Keypad Comma
// 0x86  Keypad Equal Sign
#define KEYCODE_RO 0x87 // Keyboard International1
#define KEYCODE_KATAKANAHIRAGANA 0x88 // Keyboard International2
#define KEYCODE_YEN 0x89 // Keyboard International3
#define KEYCODE_HENKAN 0x8a // Keyboard International4
#define KEYCODE_MUHENKAN 0x8b // Keyboard International5
#define KEYCODE_KPJPCOMMA 0x8c // Keyboard International6
// 0x8d  Keyboard International7
// 0x8e  Keyboard International8
// 0x8f  Keyboard International9
#define KEYCODE_HANGEUL 0x90 // Keyboard LANG1
#define KEYCODE_HANJA 0x91 // Keyboard LANG2
#define KEYCODE_KATAKANA 0x92 // Keyboard LANG3
#define KEYCODE_HIRAGANA 0x93 // Keyboard LANG4
#define KEYCODE_ZENKAKUHANKAKU 0x94 // Keyboard LANG5

#define KEYCODE_MEDIA_PLAYPAUSE 0xe8
#define KEYCODE_MEDIA_STOPCD 0xe9
#define KEYCODE_MEDIA_PREVIOUSSONG 0xea
#define KEYCODE_MEDIA_NEXTSONG 0xeb
#define KEYCODE_MEDIA_EJECTCD 0xec
#define KEYCODE_MEDIA_VOLUMEUP 0xed
#define KEYCODE_MEDIA_VOLUMEDOWN 0xee
#define KEYCODE_MEDIA_MUTE 0xef
#define KEYCODE_MEDIA_WWW 0xf0
#define KEYCODE_MEDIA_BACK 0xf1
#define KEYCODE_MEDIA_FORWARD 0xf2
#define KEYCODE_MEDIA_STOP 0xf3
#define KEYCODE_MEDIA_FIND 0xf4
#define KEYCODE_MEDIA_SCROLLUP 0xf5
#define KEYCODE_MEDIA_SCROLLDOWN 0xf6
#define KEYCODE_MEDIA_EDIT 0xf7
#define KEYCODE_MEDIA_SLEEP 0xf8
#define KEYCODE_MEDIA_COFFEE 0xf9
#define KEYCODE_MEDIA_REFRESH 0xfa
#define KEYCODE_MEDIA_CALC 0xfb
