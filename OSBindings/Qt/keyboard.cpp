#include "keyboard.h"

#include <QDebug>
#include <QGuiApplication>

// Qt is the worst.
//
// Assume your keyboard has a key labelled both . and >, as they do on US and UK keyboards. Call it the dot key.
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
// You can't. Qt is the worst. SDL doesn't have this problem, including in X11, but I'm not sure I want the extra
// dependency. I may need to reassess.

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
	qDebug() << "F1 " << XKeysymToKeycode(QX11Info::display(), XK_Escape);
#endif
}

std::optional<Inputs::Keyboard::Key> KeyboardMapper::keyForEvent(QKeyEvent *event) {
	// Workaround for X11: assume PC-esque mapping.

#ifdef HAS_X11
	if(QGuiApplication::platformName() == QLatin1String("xcb")) {
#define BIND(code, key) 	case code:	return Inputs::Keyboard::Key::key;

	switch(event->nativeScanCode()) {	/* TODO */
			default: qDebug() << "Unmapped" << event->nativeScanCode(); return {};

			BIND(XK_Escape, Escape);
			BIND(XK_F1, F1);	BIND(XK_F2, F2);	BIND(XK_F3, F3);	BIND(XK_F4, F4);	BIND(XK_F5, F5);
			BIND(XK_F6, F6);	BIND(XK_F7, F7);	BIND(XK_F8, F8);	BIND(XK_F9, F9);	BIND(XK_F10, F10);
			BIND(XK_F11, F11);	BIND(XK_F12, F12);
			BIND(XK_Sys_Req, PrintScreen);
			BIND(XK_Scroll_Lock, ScrollLock);
			BIND(XK_Pause, Pause);

			BIND(XK_grave, BackTick);
			BIND(XK_1, k1);	BIND(XK_2, k2);	BIND(XK_3, k3);	BIND(XK_4, k4);	BIND(XK_5, k5);
			BIND(XK_6, k6);	BIND(XK_7, k7);	BIND(XK_8, k8);	BIND(XK_9, k9);	BIND(XK_0, k0);
			BIND(XK_minus, Hyphen);
			BIND(XK_equal, Equals);
			BIND(XK_BackSpace, Backspace);

			BIND(XK_Tab, Tab);
			BIND(XK_Q, Q);	BIND(XK_W, W);	BIND(XK_E, E);	BIND(XK_R, R);	BIND(XK_T, T);
			BIND(XK_Y, Y);	BIND(XK_U, U);	BIND(XK_I, I);	BIND(XK_O, O);	BIND(XK_P, P);
			BIND(XK_bracketleft, OpenSquareBracket);
			BIND(XK_bracketright, CloseSquareBracket);
			BIND(XK_backslash, Backslash);

			BIND(XK_Caps_Lock, CapsLock);
			BIND(XK_A, A);	BIND(XK_S, S);	BIND(XK_D, D);	BIND(XK_F, F);	BIND(XK_G, G);
			BIND(XK_H, H);	BIND(XK_J, J);	BIND(XK_K, K);	BIND(XK_L, L);
			BIND(XK_semicolon, Semicolon);
			BIND(XK_apostrophe, Quote);
			BIND(XK_Return, Enter);

			BIND(XK_Shift_L, LeftShift);
			BIND(XK_Z, Z);	BIND(XK_X, X);	BIND(XK_C, C);	BIND(XK_V, V);
			BIND(XK_B, B);	BIND(XK_N, N);	BIND(XK_M, M);
			BIND(XK_comma, Comma);
			BIND(XK_period, FullStop);
			BIND(XK_slash, ForwardSlash);
			BIND(XK_Shift_R, RightShift);

			BIND(XK_Control_L, LeftControl);
			BIND(XK_Control_R, RightControl);
			BIND(XK_Alt_L, LeftOption);
			BIND(XK_Alt_R, RightOption);
			BIND(XK_Meta_L, LeftMeta);
			BIND(XK_Meta_R, RightMeta);
			BIND(XK_space, Space);

			BIND(XK_Left, Left);	BIND(XK_Right, Right);	BIND(XK_Up, Up);	BIND(XK_Down, Down);

			BIND(XK_Insert, Insert);
			BIND(XK_Delete, Delete);
			BIND(XK_Home, Home);
			BIND(XK_End, End);

			BIND(XK_Num_Lock, NumLock);

			BIND(XK_KP_Divide, KeypadSlash);
			BIND(XK_KP_Multiply, KeypadAsterisk);
			BIND(XK_KP_Delete, KeypadDelete);
			BIND(XK_KP_7, Keypad7);	BIND(XK_KP_8, Keypad8);	BIND(XK_KP_9, Keypad9);	BIND(XK_KP_Add, KeypadPlus);
			BIND(XK_KP_4, Keypad4);	BIND(XK_KP_5, Keypad5);	BIND(XK_KP_6, Keypad6);	BIND(XK_KP_Subtract, KeypadMinus);
			BIND(XK_KP_1, Keypad1);	BIND(XK_KP_2, Keypad2);	BIND(XK_KP_3, Keypad3);	BIND(XK_KP_Enter, KeypadEnter);
			BIND(XK_KP_0, Keypad0);
			BIND(XK_KP_Decimal, KeypadDecimalPoint);
			BIND(XK_KP_Equal, KeypadEquals);

			BIND(XK_Help, Help);
		}

#undef BIND
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
