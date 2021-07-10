#pragma once
#include "zstring.h"
#include "tarray.h"

class FFont;

struct FNotifyText
{
	int TimeOut;
	int Ticker;
	int PrintLevel;
	FString Text;
};

class FNotifyBufferBase
{
public:
	virtual ~FNotifyBufferBase() = default;
	virtual void AddString(int printlevel, int printflags, FString source) = 0;
	virtual void Shift(int maxlines);
	virtual void Clear();
	virtual void Tick();
	virtual void Draw() = 0;

protected:
	TArray<FNotifyText> Text;
	int Top = 0;
	int TopGoal = 0;
	int LineHeight = 0;
	enum { NEWLINE, APPENDLINE, REPLACELINE } AddType = NEWLINE;

	void AddString(int printlevel, FFont *printFont, const FString &source, int formatwidth, float keeptime, int maxlines);

};





