
#ifndef FINDINSEARCHWINDOW_H
#define FINDINSEARCHWINDOW_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xstring>
#include <pcre.h>
#include <Richedit.h>

class SearchConfig;


class FindInSearchWindow
{
public:
	FindInSearchWindow(HINSTANCE instance, HWND parent, HWND searchWindow, SearchConfig *config);
	~FindInSearchWindow();
	
	bool show();

	INT_PTR onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam);
	void onFindNext(WPARAM wParam, LPARAM lParam);
	void onClose();
	
	void optionChanged(WPARAM wParam, LPARAM lParam);
	bool isActive();

protected:
	void doFindNext();
	bool findNextMatch();
	void addFindTextToConfig(std::string &addString);
	
	static LRESULT CALLBACK FindInSearchKeypressHook(int code, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK FindInSearchProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT EditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	HINSTANCE instance_;
	HWND dialog_;
	HWND parent_;
	HWND searchWindow_;
	HHOOK keypressHook_;
	WNDPROC defEditProc_;

	SearchConfig *config_;

	char* richEditText_;
	int richEditTextBufferSize_;
	int richEditTextStrLen_;

	std::string searchString_;
	pcre *matchRegex_;
	CHARRANGE lastSearchSelection_;
	int lastSearchFailedPos_;
};

#endif
