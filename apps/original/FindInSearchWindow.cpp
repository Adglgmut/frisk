#include "FindInSearchWindow.h"
#include "SettingsWindow.h"
#include "FriskWindow.h"
#include <Commdlg.h>
#include "resource.h"
#include <RichEdit.h>
#include <stdio.h>
#include <assert.h>
#include "pcre.h"


static FindInSearchWindow *sWindow = NULL;

FindInSearchWindow::FindInSearchWindow(HINSTANCE instance, HWND parent, HWND searchWindow, SearchConfig *config)
	: instance_(instance)
	, parent_(parent)
	, config_(config)
	, searchWindow_(searchWindow)
	, richEditTextBufferSize_(0)
	, richEditTextStrLen_(0)
	, richEditText_(NULL)
	, lastSearchFailedPos_(false)
	, dialog_(0)
	, matchRegex_(NULL)
	, keypressHook_(NULL)
{
	sWindow = this;
}

FindInSearchWindow::~FindInSearchWindow()
{
	if(matchRegex_)
		pcre_free(matchRegex_);

	if (keypressHook_)
		UnhookWindowsHookEx(keypressHook_);
	sWindow = NULL;
}


bool FindInSearchWindow::findNextMatch()
{
	if (searchString_.empty())
		return false;

	// See if we have an update to date version of the text in the searchWindow_
	GETTEXTLENGTHEX textLength;
	textLength.codepage = 0;
	textLength.flags = GTL_DEFAULT;
	int size = SendMessage(searchWindow_, EM_GETTEXTLENGTHEX, (LPARAM)&textLength, 0);
	if (!size)
		return false;

	if (richEditTextStrLen_ != size + 1) // this is a weak check, or dirty richEditTextStrLen_ when searchWindow_ changes
	{
		if (size > richEditTextBufferSize_ + 1)
		{	// see if our buffer is big enough
			richEditTextBufferSize_ = size + 1;
			delete richEditText_;
			richEditText_ = new char[richEditTextBufferSize_];
		}

		// null terminate it for now
		richEditTextStrLen_ = 0;
		richEditText_[0] = 0; 

		GETTEXTEX tex;
		tex.cb = size + 1;
		tex.flags = GT_NOHIDDENTEXT;
		tex.codepage = 0;
		tex.lpDefaultChar = NULL;
		tex.lpUsedDefChar = NULL;
		// get the text
		SendMessage(searchWindow_, EM_GETTEXTEX, (LPARAM)&tex, (LPARAM)richEditText_);
		richEditTextStrLen_ = strlen(richEditText_);
		if (!richEditTextStrLen_)
			return false;
	}
	
	// Get the current selection, start searching from this point
	CHARRANGE prevRange;
	SendMessage(searchWindow_, EM_EXGETSEL, 0, (LPARAM)&prevRange);
	
	// see if we should change where we start searching
	int searchOffset = prevRange.cpMax;
	if (searchOffset > richEditTextStrLen_)
		searchOffset = 0;
	if (lastSearchFailedPos_ && lastSearchFailedPos_ == searchOffset)
		searchOffset = 0;

	char *pchFound = NULL;
	char *pchSearchStr = richEditText_ + searchOffset;
	int matchLen = 0;
	if (matchRegex_)
	{
		int rc;
		int ovector[100];
		for (rc = 0; rc < 100; ++rc)
		{
			ovector[rc] = -1;
		}

		if(rc = pcre_exec(matchRegex_, 0, pchSearchStr, strlen(pchSearchStr), 0, 0, ovector, sizeof(ovector)) >= 0)
		{
			// check if we have a capture, only consider the first match 
			if (ovector[2] < 0 || ovector[3] < 0)
			{
				pchFound = pchSearchStr + ovector[0];
				matchLen = ovector[1] - ovector[0];
			}
			else
			{
				pchFound = pchSearchStr + ovector[2];
				matchLen = ovector[3] - ovector[2];
			}
			
		}
	}
	else
	{
		if (config_->findInSearchMatchCase_)
		{
			pchFound = strstr(richEditText_ + searchOffset, searchString_.c_str());
		}
		else
		{
			pchFound = strstri(richEditText_ + searchOffset, searchString_.c_str());
		}

		if (pchFound)
			matchLen = searchString_.size();
	}
	
	
	if (pchFound && matchLen)
	{
		CHARRANGE newSelRange;
		unsigned int selOffset = (size_t)pchFound - (size_t)richEditText_;
		newSelRange.cpMin = selOffset;
		newSelRange.cpMax = selOffset + matchLen;
		SendMessage(searchWindow_, EM_EXSETSEL, 0, (LPARAM)&newSelRange);

		lastSearchFailedPos_ = 0;
		return true;
	}
	else
	{
		lastSearchFailedPos_ = searchOffset;
		return false;
	}
}


bool FindInSearchWindow::isActive()
{
	return (GetActiveWindow() == dialog_);

}



INT_PTR FindInSearchWindow::onInitDialog(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	dialog_ = hDlg;

	checkCtrl(GetDlgItem(dialog_, IDC_FIND_MATCHCASE), !!config_->findInSearchMatchCase_);
	checkCtrl(GetDlgItem(dialog_, IDC_FIND_MATCWHOLEWORDS), !!config_->findInSearchMatchWhole_);
	checkCtrl(GetDlgItem(dialog_, IDC_FIND_REGEX), !!config_->findInSearchRegex_);

	comboSet(GetDlgItem(dialog_, IDC_FIND_TEXT), config_->findInSearchStrings_);
	
	// set the text box to have focus first
	PostMessage(dialog_, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(dialog_, IDC_FIND_TEXT), TRUE);
		
	// get a hook to the Keyboard messages to get around not getting the reteurn and escape WM_KEYDOWN messages to our proc
	keypressHook_ = SetWindowsHookEx(WH_KEYBOARD, FindInSearchKeypressHook, instance_, GetCurrentThreadId() );

	// Intercept the WndProc for the Find text combo box, this is to surpress return beeping for some reason
	defEditProc_ = (WNDPROC)SetWindowLongPtr(GetDlgItem(dialog_, IDC_FIND_TEXT), GWL_WNDPROC, (long)EditProc);

	return TRUE;
}

void FindInSearchWindow::onClose()
{
	EndDialog(dialog_, IDCANCEL);
	dialog_ = 0;
	if (keypressHook_)
	{
		UnhookWindowsHookEx(keypressHook_);
		keypressHook_ = NULL;
	}
}

void FindInSearchWindow::onFindNext(WPARAM wParam, LPARAM lParam)
{
	int msg = HIWORD(wParam);
	if (msg == BN_CLICKED)
	{
		doFindNext();
	}
}

void FindInSearchWindow::addFindTextToConfig(std::string &addString)
{
	if (addString.empty())
		return;

	StringList &list = config_->findInSearchStrings_;
	bool matched = false;
	
	for(StringList::iterator it = list.begin(); it != list.end(); ++it)
	{
		if (it->compare(addString) == 0)
		{
			matched = true;
			break;
		}
	}

	if (!matched)
	{
		list.insert(list.begin(), addString);
		comboSet(GetDlgItem(dialog_, IDC_FIND_TEXT), config_->findInSearchStrings_);
	}
}

void FindInSearchWindow::doFindNext()
{
	searchString_ = getWindowText(GetDlgItem(dialog_, IDC_FIND_TEXT));
	
	if (!searchString_.empty())
	{
		if (config_->findInSearchMatchWhole_ || config_->findInSearchRegex_)
		{
			std::string regexString;

			const char *error;
			int erroffset;
			int flags = 0;
			if (!config_->findInSearchRegex_)
			{
				assert(config_->findInSearchMatchWhole_);
				// not actual regex, but we're going to use it to do findInSearchMatchWhole_
				regexString = searchString_;
				regexString.insert(0, "\\W(");
				regexString.append(")\\W");
			}
			else
			{
				regexString = searchString_;
			}

			if(matchRegex_)
				pcre_free(matchRegex_);

			if(!config_->findInSearchMatchCase_)
				flags |= PCRE_CASELESS;

			matchRegex_ = pcre_compile(regexString.c_str(), flags, &error, &erroffset, NULL);
			if(!matchRegex_)
			{
				MessageBox(parent_, error, "Match Regex Error", MB_OK);
				return;
			}
		}
		else
		{
			if(matchRegex_)
				pcre_free(matchRegex_);
			matchRegex_ = NULL;
		}
				

		if (findNextMatch())
		{
			addFindTextToConfig(searchString_);
		}
		else
		{
			MessageBeep(MB_ICONERROR);
		}
	}
}

void FindInSearchWindow::optionChanged(WPARAM wParam, LPARAM lParam)
{
	// we are going to modify the config directly here
	switch (wParam)
	{
		case IDC_FIND_MATCHCASE:
			config_->findInSearchMatchCase_ = ctrlIsChecked(GetDlgItem(dialog_, IDC_FIND_MATCHCASE));
			break;
		case IDC_FIND_MATCWHOLEWORDS:
			config_->findInSearchMatchWhole_ = ctrlIsChecked(GetDlgItem(dialog_, IDC_FIND_MATCWHOLEWORDS));
			break;
		case IDC_FIND_REGEX:
			config_->findInSearchRegex_ = ctrlIsChecked(GetDlgItem(dialog_, IDC_FIND_REGEX));
			break;
	}
}

// A hack to able me to intercept the return key
/*static*/ LRESULT FindInSearchWindow::EditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!sWindow)
		return FALSE;


	switch (uMsg)
	{
	case WM_KEYDOWN:
		{
			int x= 0;
			if (wParam == VK_RETURN)
			{
				sWindow->doFindNext();
				return TRUE;
			}
		} break;	
	case CB_GETDROPPEDSTATE:
		{
			return TRUE;
		} break;

	default:
		return CallWindowProc(sWindow->defEditProc_, hwnd, uMsg, wParam, lParam);

	}

	return FALSE;
}


/*static*/ LRESULT CALLBACK FindInSearchWindow::FindInSearchKeypressHook(int code, WPARAM wParam, LPARAM lParam)
{  
	if(code == HC_ACTION && ((DWORD)lParam & 0x80000000) == 0)	// if there is an incoming action and a key was pressed
	{
		if (sWindow->isActive())
		{
			switch(wParam)
			{
				case VK_ESCAPE:
					sWindow->onClose();
					break;
				case VK_RETURN:
				{
					HWND focused = GetFocus();
					if (focused == GetDlgItem(sWindow->dialog_, ID_FIND_CLOSE))
					{
						sWindow->onClose();
					}
					else 
					{
						sWindow->doFindNext();
					} 

				} break;
			}
		}

		
	}
	return CallNextHookEx(sWindow->keypressHook_, code, wParam, lParam);
}

/*static*/ INT_PTR CALLBACK FindInSearchWindow::FindInSearchProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		processMessageH(WM_INITDIALOG, onInitDialog);
				
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				processCommandParams(IDC_FIND_MATCHCASE, optionChanged);
				processCommandParams(IDC_FIND_MATCWHOLEWORDS, optionChanged);
				processCommandParams(IDC_FIND_REGEX, optionChanged);
				processCommandParams(ID_FIND_NEXT, onFindNext);
				processCommand(ID_FIND_CLOSE, onClose);
				processCommand(IDCANCEL, onClose);
				processCommand(IDOK, onClose);
			};
		
	}
	return (INT_PTR)FALSE;
}

bool FindInSearchWindow::show()
{
	if (searchWindow_)
	{
		CHARRANGE currentSelection;
		std::string selection;
		
		SendMessage(searchWindow_, EM_EXGETSEL, 0, (LPARAM)&currentSelection);
		if (currentSelection.cpMax > currentSelection.cpMin)
		{
			richEditGetSelectionText(searchWindow_, currentSelection.cpMin, currentSelection.cpMax, selection);
			if (!strstr(selection.c_str(), "\v"))
			{
				addFindTextToConfig(selection);
			}
		}
	}

	if (!dialog_)
	{
		dialog_ = CreateDialog(instance_, MAKEINTRESOURCE(IDD_SEARCHWINDOW_FIND), parent_, FindInSearchProc);
	}
	else
	{
		onInitDialog(dialog_, 0, 0);
	}
	
	
	

	return true;
}


