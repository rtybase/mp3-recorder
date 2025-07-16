// mp3_stream_GUIDlg.h : header file
//

#if !defined(AFX_MP3_STREAM_GUIDLG_H__095E8773_6637_4F87_AFDF_C80CCA16E397__INCLUDED_)
#define AFX_MP3_STREAM_GUIDLG_H__095E8773_6637_4F87_AFDF_C80CCA16E397__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CMp3_stream_GUIDlg dialog

class CMp3_stream_GUIDlg : public CDialog
{
// Construction
public:
	CMp3_stream_GUIDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CMp3_stream_GUIDlg)
	enum { IDD = IDD_MP3_STREAM_GUI_DIALOG };
	CStatic	m_stVolum;
	CSliderCtrl	m_sldVolume;
	CButton	m_btCancel;
	CButton	m_btOK;
	CComboBox	m_cbInputLine;
	CComboBox	m_cbDriver;
	CComboBox	m_cbStereoMode;
	CComboBox	m_cbSampleRate;
	CComboBox	m_cbBitRate;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMp3_stream_GUIDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CMp3_stream_GUIDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	virtual void OnCancel();
	virtual void OnOK();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDestroy();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnSelchangeCombosounddriver();
	afx_msg void OnClose();
	afx_msg void OnReleasedCaptureSlidersoundVolume(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnStaticSave();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnStaticLoad();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	CBrush* m_pDialogBkBrush;
	CBrush* m_pStaticBkBrush;
	CFont m_cfBold;
	CFont m_cfBoldUnderlined;

	void *m_mp3Wr;

	CString m_strDriver;
	CString m_strLine;

	bool m_bSTATICLOAD;
	bool m_bSTATICSAVE;
	bool m_bSTATICHELP;
	volatile bool m_isRecording;

	void chageFontStyle(int ControlID, bool setUnderline = false);
	bool isOverControl(int ControlID, CPoint &point);
	void start();
	void stop();
	bool validate();
	unsigned int getBitRate();
	unsigned int getSimpleRate();
	void getStereoMode(CString &str);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MP3_STREAM_GUIDLG_H__095E8773_6637_4F87_AFDF_C80CCA16E397__INCLUDED_)
