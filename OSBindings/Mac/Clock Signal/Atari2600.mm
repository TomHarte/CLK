//
//  Atari2600.m
//  ElectrEm
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import "Atari2600.h"
#import "Atari2600.hpp"
#import <OpenGL/gl3.h>

const char *vertexShader =
	"#version 150\n"
	"\n"
	"in vec2 position;\n"
	"\n"
	"out vec4 colour;\n"
	"\n"
	"void main (void)\n"
	"{\n"
		"colour = vec4(1.0, 1.0, 1.0, 1.0);\n"
		"gl_Position = vec4(position.x * 2.0 - 1.0, 1.0 - position.y * 2.0, 0.0, 1.0);\n"
	"}\n";

const char *fragmentShader =
	"#version 150\n"
	"\n"
	"in vec4 colour;\n"
	"out vec4 fragColour;\n"
	"\n"
	"void main(void)\n"
	"{\n"
		"fragColour = colour;\n"
	"}\n";

@interface CSAtari2600 (Callbacks)
- (void)crtDidEndFrame:(CRTFrame *)frame;
@end

struct Atari2600CRTDelegate: public Outputs::CRT::CRTDelegate {
	CSAtari2600 *atari;
	void crt_did_end_frame(Outputs::CRT *crt, CRTFrame *frame) { [atari crtDidEndFrame:frame]; }
};

@implementation CSAtari2600 {
	Atari2600::Machine _atari2600;
	Atari2600CRTDelegate _crtDelegate;

	dispatch_queue_t _serialDispatchQueue;
	CRTFrame *_queuedFrame;

	GLuint _vertexShader, _fragmentShader;
	GLuint _shaderProgram;

	GLuint _arrayBuffer, _vertexArray;
}

- (void)crtDidEndFrame:(CRTFrame *)frame {

	dispatch_async(dispatch_get_main_queue(), ^{
		if(_queuedFrame) {
			dispatch_async(_serialDispatchQueue, ^{
				_atari2600.get_crt()->return_frame();
			});
		}
		_queuedFrame = frame;

		[self.delegate atari2600NeedsRedraw:self];
	});
}

- (GLuint)compileShader:(const char *)source type:(GLenum)type
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	return shader;
}

- (void)draw {

	if(!_shaderProgram)
	{
		_shaderProgram = glCreateProgram();
		_vertexShader = [self compileShader:vertexShader type:GL_VERTEX_SHADER];
		_fragmentShader = [self compileShader:fragmentShader type:GL_FRAGMENT_SHADER];

		glAttachShader(_shaderProgram, _vertexShader);
		glAttachShader(_shaderProgram, _fragmentShader);
		glLinkProgram(_shaderProgram);

		glGenVertexArrays(1, &_vertexArray);
		glBindVertexArray(_vertexArray);
		glGenBuffers(1, &_arrayBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, _arrayBuffer);

		GLushort vertices[] = {
			0, 0,
			32767, 32767,
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	}

	if(_queuedFrame)
	{
		glClearColor(1.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(_shaderProgram);

		GLint position = glGetAttribLocation(_shaderProgram, "position");

		glBufferData(GL_ARRAY_BUFFER, _queuedFrame->number_of_runs * sizeof(GLushort) * 8, _queuedFrame->runs, GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(position);
		glVertexAttribPointer(position, 2, GL_UNSIGNED_SHORT, GL_TRUE, 4 * sizeof(GLushort), (void *)0);

		glDrawArrays(GL_LINES, 0, _queuedFrame->number_of_runs*2);
	}
}

- (void)runForNumberOfCycles:(int)cycles {
	dispatch_async(_serialDispatchQueue, ^{
		_atari2600.run_for_cycles(cycles);
	});
}

- (void)setROM:(NSData *)rom {
	dispatch_async(_serialDispatchQueue, ^{
		_atari2600.set_rom(rom.length, (const uint8_t *)rom.bytes);
	});
}

- (instancetype)init {
	self = [super init];

	if (self) {
		_crtDelegate.atari = self;
		_atari2600.get_crt()->set_delegate(&_crtDelegate);
		_serialDispatchQueue = dispatch_queue_create("Atari 2600 queue", DISPATCH_QUEUE_SERIAL);
	}

	return self;
}

@end
