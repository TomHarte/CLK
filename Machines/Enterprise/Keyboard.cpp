//
//  Keyboard.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Keyboard.hpp"

using namespace Enterprise;

uint16_t KeyboardMapper::mapped_key_for_key(Inputs::Keyboard::Key key) const {
#define BIND(source, dest)	case Inputs::Keyboard::Key::source:	return uint16_t(Key::dest)
	switch(key) {
		default: break;

		BIND(k1, k1);	BIND(k2, k2);	BIND(k3, k3);	BIND(k4, k4);	BIND(k5, k5);
		BIND(k6, k6);	BIND(k7, k7);	BIND(k8, k8);	BIND(k9, k9);	BIND(k0, k0);

		BIND(Q, Q);	BIND(W, W);	BIND(E, E);	BIND(R, R);	BIND(T, T);
		BIND(Y, Y);	BIND(U, U);	BIND(I, I);	BIND(O, O);	BIND(P, P);

		BIND(A, A);	BIND(S, S);	BIND(D, D);	BIND(F, F);	BIND(G, G);
		BIND(H, H);	BIND(J, J);	BIND(K, K);	BIND(L, L);

		BIND(Z, Z);	BIND(X, X);	BIND(C, C);	BIND(V, V);
		BIND(B, B);	BIND(N, N);	BIND(M, M);

		BIND(Space, Space);	BIND(Enter, Enter);

		BIND(LeftShift, LeftShift);
		BIND(RightShift, RightShift);
		BIND(LeftOption, Option);
		BIND(RightOption, Option);
		BIND(LeftControl, Control);
		BIND(RightControl, Control);

		BIND(Left, Left);
		BIND(Right, Right);
		BIND(Up, Up);
		BIND(Down, Down);
	}
#undef BIND

	return MachineTypes::MappedKeyboardMachine::KeyNotMapped;
}
