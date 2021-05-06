#include "keyboard.h"

#include <QDebug>
#include <QGuiApplication>

// Qt is the worst.
//
// Assume your keyboard has a key labelled both . and >, as on US and UK keyboards. Call it the dot key.
// Perform the following:
//	1.	press dot key;
//	2.	press shift key;
//	3.	release dot key;
//	4.	release shift key.
//
// Per empirical testing, and key repeat aside, on both macOS and Ubuntu 19.04 that sequence will result in
// _three_ keypress events, but only _two_ key release events. You'll get presses for Qt::Key_Period, Qt::Key_Greater
// and Qt::Key_Shift. You'll get releases only for Qt::Key_Greater and Qt::Key_Shift.
//
// How can you detect at runtime that Key_Greater and Key_Period are the same physical key?
//
// You can't. On Ubuntu they have the same values for QKeyEvent::nativeScanCode(), which are unique to the key,
// but they have different ::nativeVirtualKey()s.
//
// On macOS they have the same ::nativeScanCode() only because on macOS [almost] all keys have the same
// ::nativeScanCode(). So that's not usable. They have the same ::nativeVirtualKey()s, but since that isn't true
// on Ubuntu, that's also not usable.
//
// So how can you track the physical keys on a keyboard via Qt?
//
// You can't. Qt is the worst. SDL doesn't have this problem, including in X11, but I don't want the non-Qt dependency.

#ifdef Q_OS_LINUX
#define HAS_X11
#endif

#ifdef HAS_X11
#include <QX11Info>

#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif

KeyboardMapper::KeyboardMapper() {
#ifdef HAS_X11
	struct DesiredMapping {
		KeySym source;
		Inputs::Keyboard::Key destination;
	};

	using Key = Inputs::Keyboard::Key;
	constexpr DesiredMapping mappings[] = {
		{XK_Escape, Key::Escape},
		{XK_F1, Key::F1},	{XK_F2, Key::F2},	{XK_F3, Key::F3},	{XK_F4, Key::F4},	{XK_F5, Key::F5},
		{XK_F6, Key::F6},	{XK_F7, Key::F7},	{XK_F8, Key::F8},	{XK_F9, Key::F9},	{XK_F10, Key::F10},
		{XK_F11, Key::F11},	{XK_F12, Key::F12},
		{XK_Sys_Req, Key::PrintScreen},
		{XK_Scroll_Lock, Key::ScrollLock},
		{XK_Pause, Key::Pause},

		{XK_grave, Key::BackTick},
		{XK_1, Key::k1},	{XK_2, Key::k2},	{XK_3, Key::k3},	{XK_4, Key::k4},	{XK_5, Key::k5},
		{XK_6, Key::k6},	{XK_7, Key::k7},	{XK_8, Key::k8},	{XK_9, Key::k9},	{XK_0, Key::k0},
		{XK_minus, Key::Hyphen},
		{XK_equal, Key::Equals},
		{XK_BackSpace, Key::Backspace},

		{XK_Tab, Key::Tab},
		{XK_Q, Key::Q},	{XK_W, Key::W},	{XK_E, Key::E},	{XK_R, Key::R},	{XK_T, Key::T},
		{XK_Y, Key::Y},	{XK_U, Key::U},	{XK_I, Key::I},	{XK_O, Key::O},	{XK_P, Key::P},
		{XK_bracketleft, Key::OpenSquareBracket},
		{XK_bracketright, Key::CloseSquareBracket},
		{XK_backslash, Key::Backslash},

		{XK_Caps_Lock, Key::CapsLock},
		{XK_A, Key::A},	{XK_S, Key::S},	{XK_D, Key::D},	{XK_F, Key::F},	{XK_G, Key::G},
		{XK_H, Key::H},	{XK_J, Key::J},	{XK_K, Key::K},	{XK_L, Key::L},
		{XK_semicolon, Key::Semicolon},
		{XK_apostrophe, Key::Quote},
		{XK_Return, Key::Enter},

		{XK_Shift_L, Key::LeftShift},
		{XK_Z, Key::Z},	{XK_X, Key::X},	{XK_C, Key::C},	{XK_V, Key::V},
		{XK_B, Key::B},	{XK_N, Key::N},	{XK_M, Key::M},
		{XK_comma, Key::Comma},
		{XK_period, Key::FullStop},
		{XK_slash, Key::ForwardSlash},
		{XK_Shift_R, Key::RightShift},

		{XK_Control_L, Key::LeftControl},
		{XK_Control_R, Key::RightControl},
		{XK_Alt_L, Key::LeftOption},
		{XK_Alt_R, Key::RightOption},
		{XK_Meta_L, Key::LeftMeta},
		{XK_Meta_R, Key::RightMeta},
		{XK_space, Key::Space},

		{XK_Left, Key::Left},	{XK_Right, Key::Right},	{XK_Up, Key::Up},	{XK_Down, Key::Down},

		{XK_Insert, Key::Insert},
		{XK_Delete, Key::Delete},
		{XK_Home, Key::Home},
		{XK_End, Key::End},

		{XK_Num_Lock, Key::NumLock},

		{XK_KP_Divide, Key::KeypadSlash},
		{XK_KP_Multiply, Key::KeypadAsterisk},
		{XK_KP_Delete, Key::KeypadDelete},
		{XK_KP_7, Key::Keypad7},	{XK_KP_8, Key::Keypad8},	{XK_KP_9, Key::Keypad9},	{XK_KP_Add, Key::KeypadPlus},
		{XK_KP_4, Key::Keypad4},	{XK_KP_5, Key::Keypad5},	{XK_KP_6, Key::Keypad6},	{XK_KP_Subtract, Key::KeypadMinus},
		{XK_KP_1, Key::Keypad1},	{XK_KP_2, Key::Keypad2},	{XK_KP_3, Key::Keypad3},	{XK_KP_Enter, Key::KeypadEnter},
		{XK_KP_0, Key::Keypad0},
		{XK_KP_Decimal, Key::KeypadDecimalPoint},
		{XK_KP_Equal, Key::KeypadEquals},

		{XK_Help, Key::Help},

		{0}
	};

	// Extra level of nonsense here:
	//
	//	(1)	assume a PC-esque keyboard, with a close-to-US/UK layout;
	//	(2)	from there, use any of the X11 KeySyms I'd expect to be achievable from each physical key to
	//		look up the X11 KeyCode;
	//	(3)	henceforth, map from X11 KeyCode to the Inputs::Keyboard::Key.
	const DesiredMapping *mapping = mappings;
	while(mapping->source != 0) {
		const auto code = XKeysymToKeycode(QX11Info::display(), mapping->source);
		keyByKeySym[code] = mapping->destination;
		++mapping;
	}
#endif
}

std::optional<Inputs::Keyboard::Key> KeyboardMapper::keyForEvent(QKeyEvent *event) {
#ifdef HAS_X11
	if(QGuiApplication::platformName() == QLatin1String("xcb")) {
		const auto key = keyByKeySym.find(event->nativeScanCode());
		if(key == keyByKeySym.end()) return std::nullopt;
		return key->second;
	}
#endif

	// Fall back on a limited, faulty adaptation.
#define BIND2(qtKey, clkKey) case Qt::qtKey: return Inputs::Keyboard::Key::clkKey;
#define BIND(key) BIND2(Key_##key, key)

	switch(event->key()) {
		default: return {};

		BIND(Escape);
		BIND(F1);	BIND(F2);	BIND(F3);	BIND(F4);	BIND(F5);	BIND(F6);
		BIND(F7);	BIND(F8);	BIND(F9);	BIND(F10);	BIND(F11);	BIND(F12);
		BIND2(Key_Print, PrintScreen);
		BIND(ScrollLock);	BIND(Pause);

		BIND2(Key_AsciiTilde, BackTick);
		BIND2(Key_1, k1);	BIND2(Key_2, k2);	BIND2(Key_3, k3);	BIND2(Key_4, k4);	BIND2(Key_5, k5);
		BIND2(Key_6, k6);	BIND2(Key_7, k7);	BIND2(Key_8, k8);	BIND2(Key_9, k9);	BIND2(Key_0, k0);
		BIND2(Key_Minus, Hyphen);
		BIND2(Key_Plus, Equals);
		BIND(Backspace);

		BIND(Tab);	BIND(Q);	BIND(W);	BIND(E);	BIND(R);	BIND(T);	BIND(Y);
		BIND(U);	BIND(I);	BIND(O);	BIND(P);
		BIND2(Key_BraceLeft, OpenSquareBracket);
		BIND2(Key_BraceRight, CloseSquareBracket);
		BIND(Backslash);

		BIND(CapsLock);	BIND(A);	BIND(S);	BIND(D);	BIND(F);	BIND(G);
		BIND(H);		BIND(J);	BIND(K);	BIND(L);
		BIND(Semicolon);
		BIND2(Key_Apostrophe, Quote);
		BIND2(Key_QuoteDbl, Quote);
		// TODO: something to hash?
		BIND2(Key_Return, Enter);

		BIND2(Key_Shift, LeftShift);
		BIND(Z);	BIND(X);	BIND(C);	BIND(V);
		BIND(B);	BIND(N);	BIND(M);
		BIND(Comma);
		BIND2(Key_Period, FullStop);
		BIND2(Key_Slash, ForwardSlash);
		// Omitted: right shift.

		BIND2(Key_Control, LeftControl);
		BIND2(Key_Alt, LeftOption);
		BIND2(Key_Meta, LeftMeta);
		BIND(Space);
		BIND2(Key_AltGr, RightOption);

		BIND(Left);	BIND(Right);	BIND(Up);	BIND(Down);

		BIND(Insert); BIND(Home);	BIND(PageUp);	BIND(Delete);	BIND(End);	BIND(PageDown);

		BIND(NumLock);
	}

#undef BIND
#undef BIND2
}
