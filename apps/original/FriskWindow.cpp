// ---------------------------------------------------------------------------
//                   Copyright Joe Drago 2012.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "FriskWindow.h"
#include "SettingsWindow.h"
#include "resource.h"
#include "FindInSearchWindow.h"
#include <Commdlg.h>
#include <ShellAPI.h>
#include <Shlwapi.h>
#include <assert.h>
#include <Windowsx.h>
#include <shlobj.h>

static FriskWindow *sWindow = NULL;

// ------------------------------------------------------------------------------------------------
// Helpers

static bool hasWindowText(HWND ctrl)
{
    int len = GetWindowTextLength(ctrl);
    return (len > 0);
}

bool ctrlIsChecked(HWND ctrl)
{
    return (SendMessage(ctrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

void checkCtrl(HWND ctrl, bool checked)
{
    PostMessage(ctrl, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

std::string getWindowText(HWND ctrl)
{
    std::string str;
    int len = GetWindowTextLength(ctrl);
    if(len > 0)
    {
        str.resize(len+1);
        GetWindowText(ctrl, &str[0], len+1);
        str.resize(len);
    }
    return str;
}

void setWindowText(HWND ctrl, const std::string &s)
{
    SetWindowText(ctrl, s.c_str());
}

static void comboClear(HWND ctrl)
{
    SendMessage(ctrl, CB_RESETCONTENT, 0, 0);
}

void comboSet(HWND ctrl, StringList &list)
{
    comboClear(ctrl);
    for(StringList::iterator it = list.begin(); it != list.end(); ++it)
    {
        SendMessage(ctrl, CB_ADDSTRING, 0, (LPARAM)it->c_str());
    }
    SendMessage(ctrl, CB_SETCURSEL, 0, 0);
    SendMessage(ctrl, CB_SETEDITSEL, 0, MAKEWORD(-1, -1));
}

static void comboLRU(HWND ctrl, StringList &list, unsigned int maxRecent)
{
    std::string chosen = getWindowText(ctrl);

    for(StringList::iterator it = list.begin(); it != list.end(); ++it)
    {
        int a = it->compare(chosen);
        if(!it->compare(chosen))
        {
            list.erase(it);
            break;
        }
    }

    while(list.size() > maxRecent)
        list.pop_back();

    list.insert(list.begin(), chosen);
    comboSet(ctrl, list);
}

static void split(const std::string &orig, const char *delims, StringList &output)
{
    output.clear();
    std::string workBuffer = orig;
    char *rawString = &workBuffer[0];
    for(char *token = strtok(rawString, delims); token != NULL; token = strtok(NULL, delims))
    {
        if(token[0])
            output.push_back(std::string(token));
    }
}

// ------------------------------------------------------------------------------------------------

FriskWindow::FriskWindow(HINSTANCE instance)
: instance_(instance)
, dialog_((HWND)INVALID_HANDLE_VALUE)
, context_(NULL)
, running_(false)
, closing_(false)
, pFindSearchWindow_(NULL)
, keypressHook_(NULL)
{
    sWindow = this;

    HDC dc = GetDC(NULL);
    font_ = CreateFont(-MulDiv(8, GetDeviceCaps(dc, LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, "Courier New");
    ReleaseDC(NULL, dc);
}

FriskWindow::~FriskWindow()
{
    DeleteObject(font_);
    delete context_;
	if (keypressHook_)
		UnhookWindowsHookEx(keypressHook_);
    sWindow = NULL;
	
}

// ------------------------------------------------------------------------------------------------

void FriskWindow::outputClear()
{
    CHARRANGE charRange;
    charRange.cpMin = 0;
    charRange.cpMax = -1;
    SendMessage(outputCtrl_, EM_EXSETSEL, 0, (LPARAM)&charRange);
    SendMessage(outputCtrl_, EM_REPLACESEL, FALSE, (LPARAM)"");
}

void FriskWindow::outputUpdatePos()
{
    RECT clientRect;
    RECT outputRect;

    GetClientRect(dialog_, &clientRect);
    GetWindowRect(outputCtrl_, &outputRect);
    POINT outputPos;
    outputPos.x = outputRect.left;
    outputPos.y = outputRect.top;
    ScreenToClient(dialog_, &outputPos);

    //outputRect.right = 5;
    MoveWindow(outputCtrl_, outputPos.x, /*outputPos.y */ 0, clientRect.right - outputPos.x + 2, clientRect.bottom - outputPos.y + 2, TRUE);
}

void FriskWindow::outputUpdateColors()
{
    SendMessage(outputCtrl_, EM_SETBKGNDCOLOR, 0, (LPARAM)config_->backgroundColor_);
    InvalidateRect(outputCtrl_, NULL, TRUE);
}

int FriskWindow::flagsFromControls()
{
	int flags = config_->flags_;
    flags &= ~(
		  SF_RECURSIVE 
		| SF_FILESPEC_REGEXES 
		| SF_FILESPEC_CASE_SENSITIVE 
		| SF_MATCH_REGEXES
		| SF_MATCH_CASE_SENSITIVE
		| SF_BACKUP);
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_RECURSIVE)))      flags |= SF_RECURSIVE;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_FILESPEC_REGEX))) flags |= SF_FILESPEC_REGEXES;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_FILESPEC_CASE)))  flags |= SF_FILESPEC_CASE_SENSITIVE;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_MATCH_REGEXES)))  flags |= SF_MATCH_REGEXES;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_MATCH_CASE)))     flags |= SF_MATCH_CASE_SENSITIVE;
    if(ctrlIsChecked(GetDlgItem(dialog_, IDC_BACKUP)))         flags |= SF_BACKUP;

	return flags;
}

void FriskWindow::flagsToControls(int flags)
{
    checkCtrl(GetDlgItem(dialog_, IDC_RECURSIVE),      0 != (flags & SF_RECURSIVE));
    checkCtrl(GetDlgItem(dialog_, IDC_FILESPEC_REGEX), 0 != (flags & SF_FILESPEC_REGEXES));
    checkCtrl(GetDlgItem(dialog_, IDC_FILESPEC_CASE),  0 != (flags & SF_FILESPEC_CASE_SENSITIVE));
    checkCtrl(GetDlgItem(dialog_, IDC_MATCH_REGEXES),  0 != (flags & SF_MATCH_REGEXES));
    checkCtrl(GetDlgItem(dialog_, IDC_MATCH_CASE),     0 != (flags & SF_MATCH_CASE_SENSITIVE));
    checkCtrl(GetDlgItem(dialog_, IDC_BACKUP),         0 != (flags & SF_BACKUP));
}

void FriskWindow::configToControls()
{
    comboSet(pathCtrl_, config_->paths_);
    comboSet(matchCtrl_, config_->matches_);
    comboSet(filespecCtrl_, config_->filespecs_);
    comboSet(replaceCtrl_, config_->replaces_);
	comboSet(backupExtCtrl_, config_->backupExtensions_);
    comboSet(fileSizesCtrl_, config_->fileSizes_);
	flagsToControls(config_->flags_);
}

void FriskWindow::controlsToConfig()
{
    comboLRU(pathCtrl_, config_->paths_, 10);
    comboLRU(matchCtrl_, config_->matches_, 10);
    comboLRU(filespecCtrl_, config_->filespecs_, 10);
    comboLRU(replaceCtrl_, config_->replaces_, 10);
    comboLRU(backupExtCtrl_, config_->backupExtensions_, 10);
    comboLRU(fileSizesCtrl_, config_->fileSizes_, 10);
	config_->flags_ = flagsFromControls();
}

void FriskWindow::windowToConfig()
{
    RECT windowRect;
    GetWindowRect(dialog_, &windowRect);
    config_->windowX_ = windowRect.left;
    config_->windowY_ = windowRect.top;
    config_->windowW_ = windowRect.right - windowRect.left;
    config_->windowH_ = windowRect.bottom - windowRect.top;
}

void FriskWindow::updateState(const std::string &progress)
{
    if(running_)
    {
        if(progress.empty())
            setWindowText(stateCtrl_, "Frisking, please wait...");
        else
            setWindowText(stateCtrl_, progress);
		ShowWindow(GetDlgItem(dialog_, IDC_STOP), SW_SHOW);
    }
    else
    {
        setWindowText(stateCtrl_, "");
		ShowWindow(GetDlgItem(dialog_, IDC_STOP), SW_HIDE);
    }
    InvalidateRect(stateCtrl_, NULL, TRUE);
}

// ------------------------------------------------------------------------------------------------

INT_PTR FriskWindow::onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
    dialog_            = hDlg;
    context_           = new SearchContext(dialog_);
    config_            = &(context_->config());
    outputCtrl_        = GetDlgItem(hDlg, IDC_OUTPUT);
    pathCtrl_          = GetDlgItem(hDlg, IDC_PATH);
    filespecCtrl_      = GetDlgItem(hDlg, IDC_FILESPEC);
    matchCtrl_         = GetDlgItem(hDlg, IDC_MATCH);
    stateCtrl_         = GetDlgItem(hDlg, IDC_STATE);
    replaceCtrl_       = GetDlgItem(hDlg, IDC_REPLACE);
	backupExtCtrl_     = GetDlgItem(hDlg, IDC_BACKUP_EXT);
    fileSizesCtrl_     = GetDlgItem(hDlg, IDC_FILESIZE);
	savedSearchesCtrl_ = GetDlgItem(hDlg, IDC_SAVEDSEARCHES);


    SendMessage(pathCtrl_,          WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(filespecCtrl_,      WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(matchCtrl_,         WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(stateCtrl_,         WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(replaceCtrl_,       WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(backupExtCtrl_,     WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(savedSearchesCtrl_, WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));

    SendMessage(GetDlgItem(dialog_, IDC_RECURSIVE),      WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(GetDlgItem(dialog_, IDC_FILESPEC_REGEX), WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(GetDlgItem(dialog_, IDC_FILESPEC_CASE),  WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(GetDlgItem(dialog_, IDC_MATCH_REGEXES),  WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));
    SendMessage(GetDlgItem(dialog_, IDC_MATCH_CASE),     WM_SETFONT, (WPARAM)font_, MAKEWORD(TRUE, 0));

    bool shouldMaximize = (config_->windowMaximized_ != 0);

    HDC dc = GetDC(NULL);
    outputUpdateColors();
    SendMessage(outputCtrl_, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS);
    SendMessage(outputCtrl_, EM_LIMITTEXT, 0x7FFFFFFE, 0);
    CHARRANGE charRange;
    charRange.cpMin = -1;
    charRange.cpMax = -1;
    SendMessage(outputCtrl_, EM_EXSETSEL, 0, (LPARAM)&charRange);
    ReleaseDC(NULL, dc);

    configToControls();
    updateState();
	updateSavedSearchControl();

    HICON hIcon;
    hIcon = (HICON)LoadImage(instance_,
        MAKEINTRESOURCE(IDI_FRISK),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        0);
    if(hIcon)
    {
        SendMessage(dialog_, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    RECT windowRect;
    GetWindowRect(dialog_, &windowRect);
    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;
    if(config_->windowW_)
        width = config_->windowW_;
    if(config_->windowH_)
        height = config_->windowH_;
    MoveWindow(dialog_, config_->windowX_, config_->windowY_, width, height, FALSE);
    if(shouldMaximize)
        ShowWindow(dialog_, SW_MAXIMIZE);

	// get a hook to the Keyboard messages to capture the ESC key so it doesn't close our app
	keypressHook_ = SetWindowsHookEx(WH_KEYBOARD, KeypressHook, instance_, GetCurrentThreadId() );

    return TRUE;
}

static void splitRtfHighlights(const char *rawLine, HighlightList &highlights, StringList &l)
{
	const char* c = rawLine;
	for(HighlightList::iterator it = highlights.begin(); it != highlights.end(); it++)
	{
		Highlight &highlight = *it;
		std::string pre(c, rawLine + it->offset);
		l.push_back(pre);
		std::string p(rawLine + it->offset, it->count);
		l.push_back(p);
		c = rawLine + it->offset + it->count;
	}
	if(*c)
		l.push_back(std::string(c));
}

std::string FriskWindow::rtfHighlight(const char *rawLine, HighlightList &highlights, int count)
{
	StringList pieces;
	splitRtfHighlights(rawLine, highlights, pieces);

	char header[1024];
	sprintf(header, "{\\rtf1{\\fonttbl{\\f0\\fnil\\fcharset0 Courier New;}}{\\colortbl ;\\red%d\\green%d\\blue%d;\\red%d\\green%d\\blue%d;}\\fs%d\\cf1 ",
		(int)GetRValue(config_->textColor_),
		(int)GetGValue(config_->textColor_),
		(int)GetBValue(config_->textColor_),
		(int)GetRValue(config_->highlightColor_),
		(int)GetGValue(config_->highlightColor_),
		(int)GetBValue(config_->highlightColor_),
		config_->textSize_ * 2
		);

	std::string rtf = header;
	const char * colorTexts[2] = 
	{
		"\\cf1 ",
		"\\cf2 ",
	};
	int currentColor = 0;
	for(StringList::iterator it = pieces.begin(); it != pieces.end(); ++it)
	{
		replaceAll(*it, "\\", "\\\\");
		replaceAll(*it, "{", "\\{");
		replaceAll(*it, "}", "\\}");
		replaceAll(*it, "\n", "\\line ");
		rtf += colorTexts[currentColor];
		currentColor ^= 1;
		rtf += *it;
	}
	rtf += "}";
	return rtf;
}

bool FriskWindow::ensureSavedSearchNameExists()
{
	bool ret = true;
	std::string name = getWindowText(savedSearchesCtrl_);

	if(name.empty())
		ret = false;
	// TODO: check for only spaces?

	if(!ret)
	{
		MessageBox(dialog_, "Please type in or choose a saved search name first.", "Error", MB_OK);
	}
	return ret;
}

void FriskWindow::deleteCurrentSavedSearch()
{
	std::string name = getWindowText(savedSearchesCtrl_);
	for(SavedSearchList::iterator it = config_->savedSearches_.begin(); it != config_->savedSearches_.end(); ++it)
	{
		if(it->name == name)
		{
			config_->savedSearches_.erase(it);
			break;
		}
	}
}

void FriskWindow::updateSavedSearchControl()
{
	comboClear(savedSearchesCtrl_);

	for(SavedSearchList::iterator it = config_->savedSearches_.begin(); it != config_->savedSearches_.end(); ++it)
    {
        SendMessage(savedSearchesCtrl_, CB_ADDSTRING, 0, (LPARAM)it->name.c_str());
    }
}


INT_PTR FriskWindow::onPoke(WPARAM wParam, LPARAM lParam)
{
	PokeData *pokeData = (PokeData *)lParam;

    if(wParam != context_->searchID())
	{
		delete pokeData;
        return FALSE;
	}

    updateState(pokeData->progress);

    if(pokeData->text.empty())
	{
		delete pokeData;
        return TRUE;
	}

    // Disable redrawing briefly
    SendMessage(outputCtrl_, WM_SETREDRAW, FALSE, 0);

    // Stash off the previous caret and scroll positions
    CHARRANGE prevRange;
    SendMessage(outputCtrl_, EM_EXGETSEL, 0, (LPARAM)&prevRange);
    POINT prevScrollPos;
    SendMessage(outputCtrl_, EM_GETSCROLLPOS, 0, (LPARAM)&prevScrollPos);

    // Find out where the last character position is (for appending)
    GETTEXTLENGTHEX textLengthEx;
    textLengthEx.codepage = CP_ACP;
    textLengthEx.flags = GTL_NUMCHARS;
    int textLength = SendMessage(outputCtrl_, EM_GETTEXTLENGTHEX, (WPARAM)&textLengthEx, 0);

    // Move the caret to the end of the text
    CHARRANGE charRange;
    charRange.cpMin = textLength;
    charRange.cpMax = textLength;
    SendMessage(outputCtrl_, EM_EXSETSEL, 0, (LPARAM)&charRange);

    // Append the incoming text
	std::string rtfText = rtfHighlight(pokeData->text.c_str(), pokeData->highlights, 0);
    SendMessage(outputCtrl_, EM_REPLACESEL, FALSE, (LPARAM)rtfText.c_str());
    delete pokeData;

    // Move the caret/selection back to where it was, and scroll to the previous view
    SendMessage(outputCtrl_, EM_EXSETSEL, 0, (LPARAM)&prevRange);
    SendMessage(outputCtrl_, EM_SETSCROLLPOS, 0, (LPARAM)&prevScrollPos);

    // Reenable redrawing and invalidate the window's contents
    SendMessage(outputCtrl_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(outputCtrl_, NULL, TRUE);
    return TRUE;
}

INT_PTR FriskWindow::onState(WPARAM wParam, LPARAM lParam)
{
    running_ = (wParam != 0);
    updateState();
    return TRUE;
}

INT_PTR FriskWindow::onNotify(WPARAM wParam, LPARAM lParam)
{
    NMHDR *nmhdr = (NMHDR *)lParam;
    if(nmhdr->idFrom == IDC_OUTPUT)
    {
        switch(nmhdr->code)
        {
        case EN_MSGFILTER:
            {
                MSGFILTER *filter = (MSGFILTER *)lParam;
                if(filter->msg == WM_LBUTTONDBLCLK)
                {
                    onDoubleClickOutput();
                }
				
            }
            break;

        };
    }

    return TRUE;
}

INT_PTR FriskWindow::onMove(WPARAM wParam, LPARAM lParam)
{
    if(!closing_)
        windowToConfig();
    return TRUE;
}

INT_PTR FriskWindow::onSize(WPARAM wParam, LPARAM lParam)
{
    outputUpdatePos();
    if(wParam == SIZE_MAXIMIZED)
    {
        config_->windowMaximized_ = 1;
        return TRUE;
    }

    if(!closing_)
        config_->windowMaximized_ = 0;

    if(wParam == SIZE_MINIMIZED)
    {
        // Not interesting
    }
    else
    {
        windowToConfig();
    }
    return TRUE;
}

INT_PTR FriskWindow::onShow(WPARAM wParam, LPARAM lParam)
{
    outputUpdatePos();
    return TRUE;
}

void FriskWindow::onCancel()
{
    closing_ = true;
    SendMessage(dialog_, WM_SYSCOMMAND, SC_RESTORE, 0);
    EndDialog(dialog_, IDCANCEL);
}

void FriskWindow::search(int extraFlags)
{
    if(!hasWindowText(matchCtrl_)
    || !hasWindowText(pathCtrl_)
    || !hasWindowText(filespecCtrl_))
    {
        MessageBox(dialog_, "Please fill out What, Where, and Which.", "Not so fast!", MB_OK);
        return;
    }

	ShowWindow(GetDlgItem(dialog_, IDC_STOP), SW_SHOW);

    controlsToConfig();
    config_->save();

    context_->stop();
    outputClear();

    SearchParams params;
    params.flags = config_->flags_ | extraFlags;
    params.match = config_->matches_[0];
    params.replace = config_->replaces_[0];
	params.backupExtension = config_->backupExtensions_[0];
    split(config_->paths_[0], ";", params.paths);
    split(config_->filespecs_[0], ";", params.filespecs);
    params.maxFileSize = atoi(config_->fileSizes_[0].c_str());
    if(params.maxFileSize < 0)
        params.maxFileSize = 0;
    context_->search(params);

    updateState();
}

void FriskWindow::onSearch()
{
    search(0);
}

void FriskWindow::onReplace()
{
    if(IDYES == MessageBox(dialog_, "Are you SURE you want to perform a Replace in Files?", "Confirmation", MB_YESNO))
        search(SF_REPLACE);
}

void FriskWindow::onSettings()
{
    SettingsWindow settings(instance_, dialog_, config_);
    if(settings.show())
    {
        outputUpdateColors();
        config_->save();
    }
}

void FriskWindow::onBrowse()
{
    std::string initialPath = getWindowText(pathCtrl_);
    char filename[MAX_PATH] = "\r";
    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = dialog_;
    ofn.hInstance = instance_;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "Folders\0*.nosuchextensionevar\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = initialPath.c_str();
    ofn.Flags = OFN_HIDEREADONLY | OFN_NOVALIDATE | OFN_PATHMUSTEXIST | OFN_READONLY;
    if(GetOpenFileName(&ofn))
    {
        std::string fn = filename;
        if(!fn.empty())
        {
            if(fn[fn.length() - 1] == '\r')
            {
                fn.resize(fn.length() - 1);
            }
            if(fn[fn.length() - 1] == '\\')
            {
                fn.resize(fn.length() - 1);
            }
            setWindowText(pathCtrl_, fn);
        }
    }
}

void FriskWindow::onStop()
{
	context_->stop();
}

void FriskWindow::onLoad()
{
	if(!ensureSavedSearchNameExists())
		return;

	std::string name = getWindowText(savedSearchesCtrl_);
	for(SavedSearchList::iterator it = config_->savedSearches_.begin(); it != config_->savedSearches_.end(); ++it)
	{
		if(it->name == name)
		{
			setWindowText(savedSearchesCtrl_, it->name);
			setWindowText(matchCtrl_, it->match);
			setWindowText(pathCtrl_, it->path);
			setWindowText(filespecCtrl_, it->filespec);
			setWindowText(fileSizesCtrl_, it->fileSize);
			setWindowText(replaceCtrl_, it->replace);
			setWindowText(backupExtCtrl_, it->backupExtension);
			flagsToControls(it->flags);

			SendMessage(dialog_, WM_SETFOCUS, (WPARAM)matchCtrl_, 0);
			return;
		}
	}

	MessageBox(dialog_, "No saved search with that name.", "Error", MB_OK);
}

void FriskWindow::onSave()
{
	if(!ensureSavedSearchNameExists())
		return;

	if(MessageBox(dialog_, "Are you sure you want to save this search?", "Confirmation", MB_YESNO) != IDYES)
		return;

	deleteCurrentSavedSearch();

	SavedSearch savedSearch;
	savedSearch.name = getWindowText(savedSearchesCtrl_);
	savedSearch.match = getWindowText(matchCtrl_);
	savedSearch.path = getWindowText(pathCtrl_);
	savedSearch.filespec = getWindowText(filespecCtrl_);
	savedSearch.fileSize = getWindowText(fileSizesCtrl_);
	savedSearch.replace = getWindowText(replaceCtrl_);
	savedSearch.backupExtension = getWindowText(backupExtCtrl_);
	savedSearch.flags = flagsFromControls();
	config_->savedSearches_.push_back(savedSearch);

	updateSavedSearchControl();
}

void FriskWindow::onDelete()
{
	if(!ensureSavedSearchNameExists())
		return;

	if(MessageBox(dialog_, "Are you sure you want to delete this search?", "Confirmation", MB_YESNO) != IDYES)
		return;

	deleteCurrentSavedSearch();
	setWindowText(savedSearchesCtrl_, "");
	updateSavedSearchControl();
}

void FriskWindow::onSavedSearch(WPARAM wParam, LPARAM lParam)
{
    if(HIWORD(wParam) == CBN_DBLCLK)
    {
        onLoad();
    }
}

// gets the executable we should use to open this file
// based on our configuration, may potentially redirect to another file type
bool FriskWindow::getAssociatedExecutableForFile(const std::string &filename, std::string &exe_out)
{
	char exeBufferOut[MAX_PATH] = {0};
	const char *pszFileQuery = filename.c_str();
	
	if (!config_->extRedirections_.empty())
	{	// if we have file extension redirects, see if this file type is in our map 
		// and redirect it to another extension type if so
		const char *ext = PathFindExtension(pszFileQuery);
		if (ext)
		{
			ExtensionRedirection &redirect = config_->extRedirections_[ext];
			if (redirect.redirectExt.size())
				pszFileQuery = redirect.redirectExt.c_str();
		}
	}
	

	DWORD bufLen = MAX_PATH;
	if (S_OK == AssocQueryString(ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_EXECUTABLE, 
									pszFileQuery, "open", exeBufferOut, &bufLen))
	{
		exe_out = exeBufferOut;
		return true;
	}

	if (pszFileQuery == filename.c_str())
	{	// if AssocQueryString failed for whatever reason, try FindExecutable
		// but only if we didn't redirect 
		HINSTANCE ret = FindExecutable(filename.c_str(), NULL, exeBufferOut);
		if (exeBufferOut[0] != 0)
		{
			exe_out = exeBufferOut;
			return true;
		}
	}
	

	return false;
}

// gets the command to run on the file and puts it into the cmd_out parameter
void FriskWindow::getFileOpenCommand(const SearchEntry *searchEntry, const std::string &cmdTemplate, std::string &cmd_out)
{
	cmd_out = cmdTemplate;
	char lineBuffer[32];

	sprintf(lineBuffer, "%d", searchEntry->line_);

	if (config_->cmdFrisksChoice_ != 0 || cmd_out.empty())
	{	// empty command, try and find the right executable and configure the command to go to the right line in the file
		std::string exeName;

		if (getAssociatedExecutableForFile(searchEntry->filename_, exeName))
		{
			const char *exename = PathFindFileName(exeName.c_str());
			const char *pszFormat = NULL;

			// todo: offer a way to configure the executable to command 

			if (!stricmp(exename, "textpad.exe"))
			{
				pszFormat = "!EXE! \"!FILENAME!\"(!LINE!,0)";
			}
			else if (!stricmp(exename, "editplus.exe"))
			{
				pszFormat = "!EXE! \"!FILENAME!\" -cursor !LINE!:0";
			}
			else
			{	// unknown exe, just try and run it with the filename as the first parameter
				pszFormat = "!EXE! \"!FILENAME!\"";
			}

			assert(pszFormat);

			cmd_out = pszFormat;
			replaceAll(cmd_out, "!EXE!", exeName.c_str());
			replaceAll(cmd_out, "!FILENAME!", searchEntry->filename_.c_str());
			replaceAll(cmd_out, "!LINE!", lineBuffer);
			return;
		}

		// fell through, we couldn't find an executable for this, just open with notepad
		cmd_out = SearchConfig::getNotepadCmd();
	}
	
	// 
	replaceAll(cmd_out, "!LINE!", lineBuffer);
	replaceAll(cmd_out, "!FILENAME!", searchEntry->filename_.c_str());
}

void FriskWindow::runCommandOnSearchEntry(const SearchEntry *pEntry)
{
	//std::string cmd = "c:\\vim\\vim73\\gvim.exe --remote-silent +!LINE! +zz \"!FILENAME!\"";
	std::string cmd;

	getFileOpenCommand(pEntry, config_->cmdTemplate_, cmd);

	if (cmd.size() > 0)
	{
		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		if(CreateProcess(NULL, (char *)cmd.c_str(), NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
}


void FriskWindow::onDoubleClickOutput()
{
    CHARRANGE charRange;
    SendMessage(outputCtrl_, EM_EXGETSEL, 0, (LPARAM)&charRange);

    int offset = charRange.cpMin;

    context_->lock();
	const SearchEntry *entry = getSearchEntryByOffset(offset);
    
    if(entry)
    {
        runCommandOnSearchEntry(entry);
    }
    context_->unlock();
}

INT_PTR FriskWindow::onHotkey(WPARAM wParam, LPARAM lParam)
{
	if (!pFindSearchWindow_)
	{
		pFindSearchWindow_ = new FindInSearchWindow(instance_, dialog_, outputCtrl_, config_);
		if (!pFindSearchWindow_)
			return TRUE;
	}

	pFindSearchWindow_->show();
	

	return TRUE;
}

INT_PTR FriskWindow::onFocus(WPARAM wParam, LPARAM lParam)
{	
	// RegisterHotKey is system wide, so register/unregister with focus/unfocusing
	if (wParam > WA_INACTIVE)
	{
		RegisterHotKey(dialog_, 1, MOD_CONTROL, 'F');
	}
	else
	{
		UnregisterHotKey(dialog_, 1);
	}
	return TRUE;
}

bool FriskWindow::isActive()
{
	return (GetActiveWindow() == dialog_);
}

// ------------------------------------------------------------------------------------------------

static INT_PTR CALLBACK FriskProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        processMessageH(WM_INITDIALOG, onInitDialog);
        processMessage(WM_SEARCHCONTEXT_POKE, onPoke);
        processMessage(WM_SEARCHCONTEXT_STATE, onState);
        processMessage(WM_NOTIFY, onNotify);
        processMessage(WM_MOVE, onMove);
        processMessage(WM_SIZE, onSize);
        processMessage(WM_SHOWWINDOW, onShow);
		processMessage(WM_CONTEXTMENU, onContextMenu);
		processMessage(WM_HOTKEY, onHotkey);
		processMessage(WM_ACTIVATE, onFocus);
		
		
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                processCommand(IDCANCEL, onCancel);
                processCommand(IDC_SEARCH, onSearch);
                processCommand(IDC_DOREPLACE, onReplace);
                processCommand(IDC_SETTINGS, onSettings);
                processCommand(IDC_BROWSE, onBrowse);
				processCommand(IDC_STOP, onStop);
				processCommand(IDC_LOAD, onLoad);
				processCommand(IDC_SAVE, onSave);
				processCommand(IDC_DELETE, onDelete);
                processCommandParams(IDC_SAVEDSEARCHES, onSavedSearch);
            };
    }
    return (INT_PTR)FALSE;
}

/*static*/ LRESULT CALLBACK FriskWindow::KeypressHook(int code, WPARAM wParam, LPARAM lParam)
{  
	if(code == HC_ACTION && ((DWORD)lParam & 0x80000000) == 0)	// if there is an incoming action and a key was pressed
	{
		if (sWindow->isActive())
		{
			switch(wParam)
			{
			// The ESC key is pressed, return that we handled it
			case VK_ESCAPE:
				return TRUE;
			}
		}
	}
	return CallNextHookEx(sWindow->keypressHook_, code, wParam, lParam);
}

// copies the given string to window's clipboard
void FriskWindow::setStringClipboardData(const std::string &str)
{
	if (OpenClipboard(dialog_))
	{
		// Empty the Clipboard. This also has the effect
		// of allowing Windows to free the memory associated
		// with any data that is in the Clipboard
		EmptyClipboard();

		if (!str.empty())
		{
			HGLOBAL hClipboardData;
			hClipboardData = GlobalAlloc(GMEM_DDESHARE, str.size()+1);

			char * pchData;
			pchData = (char*)GlobalLock(hClipboardData);
			strcpy(pchData, LPCSTR(str.c_str()));

			GlobalUnlock(hClipboardData);

			SetClipboardData(CF_TEXT,hClipboardData);
		}
		
		CloseClipboard();
	}
}

// search window popup menu options
enum ESearchWindowContextOptions
{
	ESearchWindowContextOptions_OPEN = 1,
	ESearchWindowContextOptions_OPEN_LOCATION,
	ESearchWindowContextOptions_COPY_SELECTED,
	ESearchWindowContextOptions_COPY_TEXT,
	ESearchWindowContextOptions_COPY_FILENAME,
	ESearchWindowContextOptions_COPY_LINE,
};

void richEditGetSelectionText(HWND hwnd, int min, int max, std::string &selectionOut)
{
	int count = max - min;
	if (count > 0)
	{
		CHARRANGE range;
		selectionOut.reserve(count + 1);

		range.cpMin = min;
		range.cpMax = max;
		// just be sure and select what we had initially
		SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&range);

		// MSDN This message returns the number of characters copied, not including the terminating null character.
		int textLength = SendMessage(hwnd, EM_GETSELTEXT, 0, (WPARAM)selectionOut.c_str());
		// SendMessage copies the string into the buffer, but the std::string doesn't know its been updated
		// force the size update in the std::string
		selectionOut._Eos(textLength);
	}
	
}

// creates a popup menu for the searchWindow
void FriskWindow::popSearchWindowContextMenu(POINT *clientPos)
{
	int pos = SendMessage(outputCtrl_, EM_CHARFROMPOS, 0, (LPARAM)clientPos);

	context_->lock();
	const SearchEntry *entry = getSearchEntryByOffset(pos);
	if (entry)
	{
		HMENU hMenuPopup = CreatePopupMenu();

		AppendMenu(hMenuPopup, MF_STRING, ESearchWindowContextOptions_OPEN, "Open");
		AppendMenu(hMenuPopup, MF_STRING, ESearchWindowContextOptions_OPEN_LOCATION, "Open File Location");
		AppendMenu(hMenuPopup, MF_SEPARATOR, 0, NULL);
		
		CHARRANGE selRange;
		SendMessage(outputCtrl_, EM_EXGETSEL, 0, (LPARAM)&selRange);
		if (selRange.cpMax > selRange.cpMin)
		{	// if the user has something selected, allow copy
			AppendMenu(hMenuPopup, MF_STRING, ESearchWindowContextOptions_COPY_SELECTED, "Copy");
		}

		AppendMenu(hMenuPopup, MF_STRING, ESearchWindowContextOptions_COPY_TEXT, "Copy Text");
		AppendMenu(hMenuPopup, MF_STRING, ESearchWindowContextOptions_COPY_FILENAME, "Copy Filename");
		AppendMenu(hMenuPopup, MF_STRING, ESearchWindowContextOptions_COPY_LINE, "Copy Line");
			

		POINT screenCursorPos; 
		GetCursorPos(&screenCursorPos);

		BOOL ret = TrackPopupMenu(hMenuPopup, 
									TPM_RETURNCMD | TPM_RIGHTBUTTON, 
									screenCursorPos.x, screenCursorPos.y, 0, dialog_, NULL); 

		switch (ret)
		{
			case ESearchWindowContextOptions_OPEN:
				runCommandOnSearchEntry(entry);
				break;
			case ESearchWindowContextOptions_OPEN_LOCATION:
			{
				ITEMIDLIST *pidl = ILCreateFromPath(entry->filename_.c_str());
				if(pidl) 
				{
					SHOpenFolderAndSelectItems(pidl, 0, 0, 0);
					ILFree(pidl);
				}
			} break;

			case ESearchWindowContextOptions_COPY_SELECTED:
			{
				std::string selection;
				richEditGetSelectionText(outputCtrl_, selRange.cpMin, selRange.cpMax, selection);
				setStringClipboardData(selection);

			} break;

			case ESearchWindowContextOptions_COPY_TEXT:
			{
				setStringClipboardData(entry->match_);
			} break;

			case ESearchWindowContextOptions_COPY_FILENAME:
			{
				setStringClipboardData(entry->filename_);
			} break;
			
			case ESearchWindowContextOptions_COPY_LINE:
			{
				setStringClipboardData(entry->filename_ + entry->match_);
			} break;
		}
	}
	context_->unlock();
}

// Creates a pop up window and handles its input
INT_PTR FriskWindow::onContextMenu(WPARAM wParam, LPARAM lParam)
{
	RECT rc;			// client area of window 
	POINT pt;			// location of mouse click 

	// Get the bounding rectangle of the client area. 
	GetCursorPos(&pt);

	GetClientRect(context_->getWindow(), &rc); 
	ScreenToClient(context_->getWindow(), &pt); 

	// If the position is in the client area, display a shortcut menu. 

	if (PtInRect(&rc, pt))
	{
		popSearchWindowContextMenu(&pt);
	}
	
	return TRUE;
}

// returns a SearchEntry based on the character offset
// context_->lock(); should be called before 
const SearchEntry* FriskWindow::getSearchEntryByOffset(int offset)
{
	SearchList &list = context_->list();
	const SearchEntry *entry = NULL;
	for(SearchList::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if(it->offset_ > offset)
		{
			return &(*it);
		}
	}
	return NULL;
}

void FriskWindow::show()
{
	DialogBox(instance_, MAKEINTRESOURCE(IDD_FRISK), NULL, FriskProc);
}
