
// week2.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// Cweek2App:
// See week2.cpp for the implementation of this class
//

class Cweek2App : public CWinApp
{
public:
	Cweek2App();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern Cweek2App theApp;
