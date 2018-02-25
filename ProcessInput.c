#include "Global.h"
#include "GlHeaders.h"

#ifndef _WIN32
#include <sys/un.h>
#endif
#include <stdio.h>
#include "ProcessInput.h"
#include "ParseGL.h"
#include "SymbolHash.h"

#define READ_BUFFER_SIZE 1024
#define TOK_COUNT_MAX MAX_GL_PARAMS+1

bool ParseLine(char *Line, int *ArgCountResult, char **ArgResult) {
	char *LastArgPos, *temp;

	*ArgCountResult= 0;
	LastArgPos= NULL;
	temp= strtok(Line, " \t");
	while (temp != NULL && *ArgCountResult < TOK_COUNT_MAX) {
		if (temp > LastArgPos + 1)
			ArgResult[(*ArgCountResult)++]= temp;
		LastArgPos= temp;
		temp= strtok(NULL, " \t");
	}
	ArgResult[*ArgCountResult]= NULL;
	return *ArgCountResult > 0;
}

int ProcessCommand(char **TokenPointers, int TokenCount) {
	const CmdHashEntry *Cmd;
	int Result, GLErr;

	Cmd= GetCmd(TokenPointers[0]);
	if (Cmd != NULL) {
		Result= Cmd->Value(TokenCount-1, TokenPointers+1); // run command
		switch (Result) {
		case 0:
			if (PointsInProgress) // can't check command success
				return 0;
			GLErr= glGetError();
			if (!GLErr) // command was successful
				return 0;
			while (GLErr) {
				fprintf(stderr, "GL error while executing %s: %s.\n", TokenPointers[0], gluErrorString(GLErr));
				GLErr= glGetError();
			}
			break;
		case ERR_PARAMCOUNT:
			fprintf(stderr, "Wrong number of parameters for %s\n", TokenPointers[0]);
			break;
		case ERR_PARAMPARSE:
			fprintf(stderr, "Cannot parse parameters for %s\n", TokenPointers[0]);
			break;
		case ERR_EXEC:
			fprintf(stderr, "Error during execution of %s\n", TokenPointers[0]);
			break;
		default:
			fprintf(stderr, "Unknown result code: %d\n", Result);
		}
		return P_CMD_ERR;
	}
	else {
		fprintf(stderr, "Unknown command: \"%s\".\n", TokenPointers[0]);
		return P_NOT_FOUND;
	}
}

/** Everything from here down is "class LineBuffer".  I had to write my own
 * because I wanted a readline function that wouldn't block, and wouldn't
 * return a partial string.
 *
 * There's only one used in the whole prog, and I see it staying that way, so I
 * 'optimized' a bit.
 *
 * If it ever becomes necessary to make more than one, uncomment and propogate.
 * Also, it will then be necessary to make a constructor function, and call it
 * before each gets used the first time.
 */
//typedef struct LineBuffer_t {
	char ReadBuffer[READ_BUFFER_SIZE];
	char const *StopPos;
	char *Pos;
	char *DataPos;
	char *LineStart;
	#ifndef _WIN32
	int fd;
	#else
	HANDLE fd;
	#endif
//} LineBuffer;

#ifndef _WIN32
void InitLineBuffer(int FileHandle /*, LineBuffer *this */) {
#else
void InitLineBuffer(HANDLE FileHandle /*, LineBuffer *this */) {
#endif
	fd= FileHandle;
	StopPos= ReadBuffer+READ_BUFFER_SIZE;
	Pos= ReadBuffer;
	DataPos= ReadBuffer;
	LineStart= ReadBuffer;
}

void ShiftBuffer(/* LineBuffer *this */) {
	int shift= LineStart - ReadBuffer;
	if (DataPos != LineStart)
		memmove(ReadBuffer, LineStart, DataPos - LineStart);
	DataPos-= shift;
	Pos-= shift;
	LineStart= ReadBuffer;
}

char* ReadLine(/* LineBuffer *this */) {
	int success;
	int red;
	char *Result;

	if (LineStart != ReadBuffer && DataPos - LineStart < 16)
		ShiftBuffer();
	do {
		// do we need more?
		if (Pos == DataPos) {
			// do we need to shift?
			if (DataPos == StopPos) {
				// can we shift?
				if (LineStart != ReadBuffer)
					ShiftBuffer();
				else {
					// Full buffer.  Abort, and reset the buffer pointers.
					LineStart= DataPos= Pos= ReadBuffer;
					fprintf(stderr, "Input line too long\n");
					return NULL;
				}
			}
			#ifndef _WIN32
			red= read(fd, DataPos, StopPos-DataPos);
			if (red <= 0)
				return NULL;
			#else
			// no non-blocking mode, so we need to test the status of the object
			if (WaitForSingleObject(fd, 0) == WAIT_OBJECT_0)
				success= ReadFile(fd, DataPos, StopPos-DataPos, &red, NULL);
			else
				return NULL;
			if (!success || red<=0)
				return NULL;
			#endif
			DataPos+= red;
		}
		while (Pos < DataPos && *Pos != '\n')
			Pos++;
	} while (*Pos != '\n');
	*Pos= '\0';
	if (Pos > LineStart)
		if (Pos[-1] == '\r')
			Pos[-1]= 0;
	Result= LineStart;
	LineStart= ++Pos;
	return Result;
}




