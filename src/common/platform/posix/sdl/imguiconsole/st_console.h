/*
 ** st_console.h
 **
 **---------------------------------------------------------------------------
 ** Copyright 2015 Alexey Lysiuk
 ** Copyright 2020 Cacodemon345
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 **
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **---------------------------------------------------------------------------
 **
 */

#include <SDL.h>
#include "imgui.h"
#include "imgui_sdl.h"
#include "v_palette.h"
#include "i_interface.h"
class FConsoleWindow
{
public:
	enum class StartupType
	{
		StartupTypeNormal,
		StartupTypeHexen,
		StartupTypeHeretic,
		StartupTypeStrife,
	};
	static FConsoleWindow& GetInstance();

	static void CreateInstance();
	static bool InstanceExists();
	static void DeleteInstance();

	void Show(bool visible);
	void ShowFatalError(const char* message);

	void AddText(const char* message);
	void AddText(FString message);
	int PickIWad (WadStuff *wads, int numwads, bool showwin, int defaultiwad);

	void SetTitleText();
	void SetProgressBar(bool visible);
	void InitGraphicalMode();
	void DeinitGraphicalMode();
	void SetStartupType(StartupType type);
	StartupType GetStartupType();
	void AddStatusText(const char* message, int colors);
	void AppendStatusLine(FString status);

	// FStartupScreen functionality
	void Progress(int current, int maximum);
	void NetInit(const char* message, int playerCount);
	void NetProgress(int count);
	void NetDone();
	void RunLoop();
	bool NetUserExitRequested();
    
private:
    SDL_Renderer* m_renderer;
	SDL_Window* m_window;
    ImGuiIO& io;
	TArray<FString> m_texts;
	bool ProgBar, m_netinit, m_exitreq, m_error;
	bool m_renderiwadtitle, m_graphicalstartscreen;
	uint32_t m_iwadtitlebgcol, m_iwadtitletextcol;
	int m_netCurPos, m_netMaxPos;
	int m_consolewidth, m_consoleheight;
	FString m_nettext, m_iwadtitle;
	int m_playercount;
	int m_iwadselect;
	bool m_netprogflash;
	float m_maxscroll;
	unsigned int m_errorframe;
	StartupType m_startuptype = StartupType::StartupTypeNormal;
	SDL_Texture* m_hexenpic = nullptr;
	SDL_Texture* m_hexennotchpic = nullptr;
	SDL_Texture* m_hexennetnotchpic = nullptr;
	SDL_Texture* m_strifepics[4 + 2 + 1];
	SDL_Texture* m_strifestartuppic = nullptr;
	SDL_Texture* m_vgatextpic = nullptr;
	SDL_Texture* m_loadingpic = nullptr;
	uint8_t m_textloadingscreen[4000] = { 0 };
	TArray<FString> m_statustexts;
	TArray<uint8_t> m_statuscolors;
	FString m_statusline;
	int m_vgatextheight;
	unsigned int m_thermx, m_thermy, m_thermwidth, m_thermheight;
	struct
	{
		WadStuff *wads;
		int numwads, defaultiwad, currentiwad, curbackend;
		bool lightsload, brightmapsload, widescreenload, noautoload;
		TArray<const char *> wadnames;
	} m_iwadparams;

    void AddText(const PalEntry& color, const char* message);
	void RunImguiRenderLoop();
	void RunHexenSubLoop();
	void RunHereticSubLoop();
	static void GetANSITextColors(uint8_t attrib, SDL_Color& fgcol, SDL_Color& bgcol);
	void RunStrifeSubLoop();
	SDL_Texture* CreateTextureFromFont(const uint8_t* font);

	FConsoleWindow();	
	~FConsoleWindow();
};

