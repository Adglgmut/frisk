// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef FRISKWINDOW_H
#define FRISKWINDOW_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <RichEdit.h>

#include "SearchContext.h"

class FindInSearchWindow;

void comboSet(HWND ctrl, StringList &list);
void comboLRU(HWND ctrl, StringList &list, unsigned int maxRecent);
void richEditGetSelectionText(HWND hwnd, int min, int max, std::string &selectionOut);

class FriskWindow
{
public:
    FriskWindow(HINSTANCE instance);
    ~FriskWindow();

    void show();

    void outputClear();
    void outputUpdatePos();
    void outputUpdateColors();

    void configToControls();
    void controlsToConfig();
    void windowToConfig();
    void updateState(const std::string &progress = "");
	std::string rtfHighlight(const char *rawLine, HighlightList &highlights, int count);

    INT_PTR onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam);
    INT_PTR onPoke(WPARAM wParam, LPARAM lParam);
    INT_PTR onState(WPARAM wParam, LPARAM lParam);
    INT_PTR onNotify(WPARAM wParam, LPARAM lParam);
    INT_PTR onMove(WPARAM wParam, LPARAM lParam);
    INT_PTR onSize(WPARAM wParam, LPARAM lParam);
    INT_PTR onShow(WPARAM wParam, LPARAM lParam);
	INT_PTR onContextMenu(WPARAM wParam, LPARAM lParam);
	

    void search(int extraFlags);
	bool ensureSavedSearchNameExists();
	void deleteCurrentSavedSearch();
	void updateSavedSearchControl();

	int flagsFromControls();
	void flagsToControls(int flags);

    void onCancel();
    void onSearch();
    void onReplace();
    void onDoubleClickOutput();
    void onSettings();
    void onBrowse();
	void onStop();
	void onSave();
	void onLoad();
	void onDelete();
    void onSavedSearch(WPARAM wParam, LPARAM lParam);
	bool isActive();

protected:

	static LRESULT CALLBACK KeypressHook(int code, WPARAM wParam, LPARAM lParam);


	void getFileOpenCommand(const SearchEntry *searchEntry, const std::string &cmdTemplate, std::string &cmd_out);
	void runCommandOnSearchEntry(const SearchEntry *pEntry);

	bool getAssociatedExecutableForFile(const std::string &filename, std::string &exe_out);
	
	// pops up the context menu for the search window at the given location, using the position to get the row
	void popSearchWindowContextMenu(POINT *pt);
	
	// returns a SearchEntry based on the character offset
	const SearchEntry* getSearchEntryByOffset(int offset);

	// copies the given string to window's clipboard
	void setStringClipboardData(const std::string &str);
		
	void openSearchWindow();

    HINSTANCE instance_;
    HFONT font_;
    HWND dialog_;
    HWND outputCtrl_;
    HWND pathCtrl_;
    HWND filespecCtrl_;
    HWND matchCtrl_;
    HWND stateCtrl_;
    HWND replaceCtrl_;
	HWND backupExtCtrl_;
    HWND fileSizesCtrl_;
	HWND savedSearchesCtrl_;
	HHOOK keypressHook_;
	SearchContext *context_;
    SearchConfig *config_;
	FindInSearchWindow *pFindSearchWindow_;
	
    bool running_;
    bool closing_;
	bool findHotkeyRegistered_;
};

#endif
