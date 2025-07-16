// mp3_stream_GUI.h : main header file for the MP3_STREAM_GUI application
//

#if !defined(AFX_MP3_STREAM_GUI_H__25699999_CB90_4C77_87B4_9DC8DE9FBDC9__INCLUDED_)
#define AFX_MP3_STREAM_GUI_H__25699999_CB90_4C77_87B4_9DC8DE9FBDC9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CMp3_stream_GUIApp:
// See mp3_stream_GUI.cpp for the implementation of this class
//

class CMp3_stream_GUIApp : public CWinApp
{
public:
	CMp3_stream_GUIApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMp3_stream_GUIApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CMp3_stream_GUIApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MP3_STREAM_GUI_H__25699999_CB90_4C77_87B4_9DC8DE9FBDC9__INCLUDED_)
