/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "StdAfx.h"
#include "resource.h"

#include "TeamGeneric.h"
#include "EditParameter.h"
#include "WorldBuilder.h"

#include "Common/AsciiString.h"
#include "Common/Dict.h"
#include "Common/WellKnownKeys.h"

static const UINT s_allControls[][2] =
{
	{ IDC_SCRIPT_PREFIX1,		IDC_TeamGeneric_Script1,	},
	{ IDC_SCRIPT_PREFIX2,		IDC_TeamGeneric_Script2,	},
	{ IDC_SCRIPT_PREFIX3,		IDC_TeamGeneric_Script3,	},
	{ IDC_SCRIPT_PREFIX4,		IDC_TeamGeneric_Script4,	},
	{ IDC_SCRIPT_PREFIX5,		IDC_TeamGeneric_Script5,	},
	{ IDC_SCRIPT_PREFIX6,		IDC_TeamGeneric_Script6,	},
	{ IDC_SCRIPT_PREFIX7,		IDC_TeamGeneric_Script7,	},
	{ IDC_SCRIPT_PREFIX8,		IDC_TeamGeneric_Script8,	},
	{ IDC_SCRIPT_PREFIX9,		IDC_TeamGeneric_Script9,	},
	{ IDC_SCRIPT_PREFIX10,	IDC_TeamGeneric_Script10, },
	{ IDC_SCRIPT_PREFIX11,	IDC_TeamGeneric_Script11, },
	{ IDC_SCRIPT_PREFIX12,	IDC_TeamGeneric_Script12, },
	{ IDC_SCRIPT_PREFIX13,	IDC_TeamGeneric_Script13, },
	{ IDC_SCRIPT_PREFIX14,	IDC_TeamGeneric_Script14, },
	{ IDC_SCRIPT_PREFIX15,	IDC_TeamGeneric_Script15, },
	{ IDC_SCRIPT_PREFIX16,	IDC_TeamGeneric_Script16, },
	{ IDC_SCRIPT_PREFIX17,	IDC_TeamGeneric_Script17,	},
	{ IDC_SCRIPT_PREFIX18,	IDC_TeamGeneric_Script18,	},
	{ IDC_SCRIPT_PREFIX19,	IDC_TeamGeneric_Script19,	},
	{ IDC_SCRIPT_PREFIX20,	IDC_TeamGeneric_Script20,	},
	{ IDC_SCRIPT_PREFIX21,	IDC_TeamGeneric_Script21,	},
	{ IDC_SCRIPT_PREFIX22,	IDC_TeamGeneric_Script22,	},
	{ IDC_SCRIPT_PREFIX23,	IDC_TeamGeneric_Script23,	},
	{ IDC_SCRIPT_PREFIX24,	IDC_TeamGeneric_Script24,	},
	{ IDC_SCRIPT_PREFIX25,	IDC_TeamGeneric_Script25,	},
	{ IDC_SCRIPT_PREFIX26,	IDC_TeamGeneric_Script26, },
	{ IDC_SCRIPT_PREFIX27,	IDC_TeamGeneric_Script27, },
	{ IDC_SCRIPT_PREFIX28,	IDC_TeamGeneric_Script28, },
	{ IDC_SCRIPT_PREFIX29,	IDC_TeamGeneric_Script29, },
	{ IDC_SCRIPT_PREFIX30,	IDC_TeamGeneric_Script30, },
	{ IDC_SCRIPT_PREFIX31,	IDC_TeamGeneric_Script31, },
	{ IDC_SCRIPT_PREFIX32,	IDC_TeamGeneric_Script32, },
	{ IDC_SCRIPT_PREFIX33,	IDC_TeamGeneric_Script33,	},
	{ IDC_SCRIPT_PREFIX34,	IDC_TeamGeneric_Script34,	},
	{ IDC_SCRIPT_PREFIX35,	IDC_TeamGeneric_Script35,	},
	{ IDC_SCRIPT_PREFIX36,	IDC_TeamGeneric_Script36,	},
	{ IDC_SCRIPT_PREFIX37,	IDC_TeamGeneric_Script37,	},
	{ IDC_SCRIPT_PREFIX38,	IDC_TeamGeneric_Script38,	},
	{ IDC_SCRIPT_PREFIX39,	IDC_TeamGeneric_Script39,	},
	{ IDC_SCRIPT_PREFIX40,	IDC_TeamGeneric_Script40,	},
	{ IDC_SCRIPT_PREFIX41,	IDC_TeamGeneric_Script41,	},
	{ IDC_SCRIPT_PREFIX42,	IDC_TeamGeneric_Script42, },
	{ IDC_SCRIPT_PREFIX43,	IDC_TeamGeneric_Script43, },
	{ IDC_SCRIPT_PREFIX44,	IDC_TeamGeneric_Script44, },
	{ IDC_SCRIPT_PREFIX45,	IDC_TeamGeneric_Script45, },
	{ IDC_SCRIPT_PREFIX46,	IDC_TeamGeneric_Script46, },
	{ IDC_SCRIPT_PREFIX47,	IDC_TeamGeneric_Script47, },
	{ IDC_SCRIPT_PREFIX48,	IDC_TeamGeneric_Script48, },
	{ IDC_SCRIPT_PREFIX49,	IDC_TeamGeneric_Script49,	},
	{ IDC_SCRIPT_PREFIX50,	IDC_TeamGeneric_Script50,	},
	{ IDC_SCRIPT_PREFIX51,	IDC_TeamGeneric_Script51,	},
	{ IDC_SCRIPT_PREFIX52,	IDC_TeamGeneric_Script52,	},
	{ IDC_SCRIPT_PREFIX53,	IDC_TeamGeneric_Script53,	},
	{ IDC_SCRIPT_PREFIX54,	IDC_TeamGeneric_Script54,	},
	{ IDC_SCRIPT_PREFIX55,	IDC_TeamGeneric_Script55,	},
	{ IDC_SCRIPT_PREFIX56,	IDC_TeamGeneric_Script56,	},
	{ IDC_SCRIPT_PREFIX57,	IDC_TeamGeneric_Script57,	},
	{ IDC_SCRIPT_PREFIX58,	IDC_TeamGeneric_Script58, },
	{ IDC_SCRIPT_PREFIX59,	IDC_TeamGeneric_Script59, },
	{ IDC_SCRIPT_PREFIX60,	IDC_TeamGeneric_Script60, },
	{ IDC_SCRIPT_PREFIX61,	IDC_TeamGeneric_Script61, },
	{ IDC_SCRIPT_PREFIX62,	IDC_TeamGeneric_Script62, },
	{ IDC_SCRIPT_PREFIX63,	IDC_TeamGeneric_Script63, },
	{ IDC_SCRIPT_PREFIX64,	IDC_TeamGeneric_Script64, },
	{ 0,0, },
};

TeamGeneric::TeamGeneric() : CPropertyPage(TeamGeneric::IDD)
{

}

BOOL TeamGeneric::OnInitDialog()
{
	CPropertyPage::OnInitDialog();
	// Fill all the combo boxes with the scripts we have.
	_fillComboBoxesWithScripts();

	// Set up the dialog as appropriate.
	_dictToScripts();

	SCROLLINFO si = {};
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_RANGE | SIF_PAGE;
	si.nMin = 0;
	si.nMax = 1850;
	si.nPage = 311;

	SetScrollInfo(SB_VERT, &si);
	return FALSE;
}

void TeamGeneric::_fillComboBoxesWithScripts()
{
	int i = 0;
	while (s_allControls[i][1]) {
		CComboBox *pCombo = (CComboBox*) GetDlgItem(s_allControls[i][1]);
		if (!pCombo) {
			continue;
		}

		// Load all the scripts, then add the NONE string.
		EditParameter::loadScripts(pCombo, true);
		pCombo->InsertString(0, NONE_STRING);
		++i;
	}
}

void TeamGeneric::_dictToScripts()
{
	CWnd *pText = nullptr;
	CComboBox *pCombo = nullptr;

	if (!m_teamDict) {
		return;
	}

	int i = 0;
	while (s_allControls[i][1]) {

		pText = GetDlgItem(s_allControls[i][0]);
		if (!pText) {
			continue;
		}

		pCombo = (CComboBox*) GetDlgItem(s_allControls[i][1]);
		if (!pCombo) {
			continue;
		}

		Bool exists;
		AsciiString scriptString;
		AsciiString keyName;
		keyName.format("%s%d", TheNameKeyGenerator->keyToName(TheKey_teamGenericScriptHook).str(), i);
		scriptString = m_teamDict->getAsciiString(NAMEKEY(keyName), &exists);

		pText->ShowWindow(SW_SHOW);
		pCombo->ShowWindow(SW_SHOW);

		if (exists) {
			Int selNdx = pCombo->FindStringExact(-1, scriptString.str());
			if (selNdx == LB_ERR) {
				pCombo->SetCurSel(0);
			} else {
				pCombo->SetCurSel(selNdx);
			}
		} else {
			break;
		}

		++i;
	}

	if (!s_allControls[i][1]) {
		// We filled everything.
		return;
	}

	if (!pCombo) {
		// We filled nothing, or there was an error.
		return;
	}

	pCombo->SetCurSel(0);

	++i;
	while (s_allControls[i][1]) {
		pText = GetDlgItem(s_allControls[i][0]);
		if (!pText) {
			continue;
		}

		pCombo = (CComboBox*) GetDlgItem(s_allControls[i][1]);
		if (!pCombo) {
			continue;
		}

		pText->ShowWindow(SW_HIDE);
		pCombo->ShowWindow(SW_HIDE);
		++i;
	}
}

void TeamGeneric::_scriptsToDict()
{
	if (!m_teamDict) {
		return;
	}

	CWnd *pText = nullptr;
	CComboBox *pCombo = nullptr;

	int scriptNum = 0;

	int i = 0;
	while (s_allControls[i][1]) {

		pText = GetDlgItem(s_allControls[i][0]);
		if (!pText) {
			continue;
		}

		pCombo = (CComboBox*) GetDlgItem(s_allControls[i][1]);
		if (!pCombo) {
			continue;
		}

		// i should always be incremented, so just do it here.
		++i;

		AsciiString keyName;
		keyName.format("%s%d", TheNameKeyGenerator->keyToName(TheKey_teamGenericScriptHook).str(), scriptNum);

		int curSel = pCombo->GetCurSel();
		if (curSel == CB_ERR || curSel == 0) {
			if (m_teamDict->known(NAMEKEY(keyName), Dict::DICT_ASCIISTRING)) {
				// remove it if we know it.
				m_teamDict->remove(NAMEKEY(keyName));
			}

			continue;
		}

		CString cstr;
		pCombo->GetLBText(curSel, cstr);

		AsciiString scriptString = static_cast<LPCSTR>(cstr);
		m_teamDict->setAsciiString(NAMEKEY(keyName), scriptString);
		++scriptNum;
	}

	for ( ; s_allControls[scriptNum][1]; ++scriptNum ) {
		AsciiString keyName;
		keyName.format("%s%d", TheNameKeyGenerator->keyToName(TheKey_teamGenericScriptHook).str(), scriptNum);

		if (m_teamDict->known(NAMEKEY(keyName), Dict::DICT_ASCIISTRING))  {
			m_teamDict->remove(NAMEKEY(keyName));
		}
	}
}

void TeamGeneric::OnScriptAdjust()
{
	_scriptsToDict();
	_dictToScripts();
}

void TeamGeneric::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	int curPos = GetScrollPos(SB_VERT);
	int newPos = curPos;

	switch (nSBCode)
	{
	case SB_LINEUP:      newPos -= 20; break;
	case SB_LINEDOWN:    newPos += 20; break;
	case SB_PAGEUP:      newPos -= 100; break;
	case SB_PAGEDOWN:    newPos += 100; break;
	case SB_THUMBTRACK:  newPos = nPos; break;
	}

	SetScrollPos(SB_VERT, newPos);
	ScrollWindow(0, curPos - newPos);
}

BEGIN_MESSAGE_MAP(TeamGeneric, CPropertyPage)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script1, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script2, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script3, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script4, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script5, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script6, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script7, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script8, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script9, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script10, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script11, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script12, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script13, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script14, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script15, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script16, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script17, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script18, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script19, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script20, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script21, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script22, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script23, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script24, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script25, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script26, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script27, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script28, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script29, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script30, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script31, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script32, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script33, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script34, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script35, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script36, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script37, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script38, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script39, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script40, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script41, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script42, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script43, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script44, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script45, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script46, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script47, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script48, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script49, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script50, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script51, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script52, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script53, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script54, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script55, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script56, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script57, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script58, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script59, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script60, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script61, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script62, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script63, OnScriptAdjust)
	ON_CBN_SELCHANGE(IDC_TeamGeneric_Script64, OnScriptAdjust)

	ON_WM_VSCROLL()
END_MESSAGE_MAP()
