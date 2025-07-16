// mp3_stream_GUIDlg.cpp : implementation file
//

#pragma setlocale(".1251")

#include "stdafx.h"
#include "mp3_stream_GUI.h"
#include "mp3_stream_GUIDlg.h"
#include "INCLUDE/mp3_simple.h"
#include "INCLUDE/waveIN_simple.h"
#include "INCLUDE/threading.h"
#include "FFTransform.h"
#include "SampleIter.h"
#include <locale.h>

#import <msxml4.dll>
using namespace MSXML2;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define SPECSCOPEWIDTH 10

static TCHAR szMP3Filter[] = _T("MP3 files (*.mp3)|*.mp3||");
static TCHAR szConfigFilter[] = _T("Configuration files (*.config)|*.config||");
static TCHAR szConfigFolder[] = _T(".\\Config");
static TCHAR szMusicFolder[] = _T(".\\Music");

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	CStatic	m_stInfo;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	virtual BOOL OnInitDialog();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	CBrush* m_pDialogBkBrush;
	CBrush* m_pStaticBkBrush;
};

class drawThread: public CThread {
private:
	HWND hwnd;
	HDC  HdblDC;
	HBITMAP HdblOldBitmap;
	RECT    drawArea;

	HPEN hpen;
	HBRUSH hbrGreen;
	HBRUSH hbrWhite;

	// buffer related data
	char m_lpData[2 * MAXBUFFLEN];
	DWORD m_dwBytesRecorded;
	bool m_hasNewData;

	FFTransform *m_pFFTransStereo;
	SampleIter *m_pSampleIter;
	ReadWriteMutex wrLock;

	int max_fr;

	void doGraphicalStuff(LPSTR lpData, DWORD dwBytesRecorded) {
		int maxFreq, freqChg, rps = 1;
		int freqAccum[SPECSCOPEWIDTH];
		static int channels[SPECSCOPEWIDTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

		// 2 == 16/8
		m_pSampleIter->SetSampleWorkSet (lpData, dwBytesRecorded, 2, false);
		m_pFFTransStereo->fftTransformMag (*m_pSampleIter, true);
		
		maxFreq = (m_pSampleIter->Count() / 4);
		freqChg = maxFreq / (SPECSCOPEWIDTH+1);
		memset (freqAccum, 0, sizeof (freqAccum));

		int idx;
		int freqIdx    = -2;
		int freqCnt    = 0;
		int accumlator = 0;
		float *pMags  = m_pFFTransStereo->fftGetMagArray();

		for (idx = 0; idx < maxFreq; idx++) {                                    
			// get the max power between freq band areas
			if (pMags[idx] > accumlator) accumlator = (int) pMags[idx];
			freqCnt++;

			if (freqCnt > freqChg) {
				freqCnt = 0;
				freqIdx++;

				// chop off the lower frequencies that have too much power 
				// and over staturate the output
				if (freqIdx >= 0) freqAccum[freqIdx] = accumlator; 

				accumlator = 0;
			}                                                                        
		}

		if (freqIdx < (SPECSCOPEWIDTH-1)) {
			freqIdx++;
			freqAccum[freqIdx] = accumlator;
		}   

		for (idx = 0; idx < SPECSCOPEWIDTH; idx++) {
			if (freqAccum[idx] >= channels[idx]) channels[idx] = freqAccum[idx];
			else {
				channels[idx] -= (128 * rps);
				if (channels[idx] < 0) channels[idx] = 0;
			}
		}

		draw(channels);
	}

protected:
	void draw(int *channels) {
		FillRect(HdblDC,  &drawArea, hbrWhite);

		if (channels != NULL) {
			HPEN holdPen = (HPEN)SelectObject(HdblDC, hpen);
			HBRUSH holdBrush = (HBRUSH)SelectObject(HdblDC, hbrGreen);
			int start = 3 + drawArea.left;

			for (int idx = 0; idx < SPECSCOPEWIDTH; idx++) {
				int y = (drawArea.bottom * channels[idx]) / max_fr;
				int x = start + idx*5;

				MoveToEx(HdblDC, x, drawArea.bottom, NULL);
				Rectangle(HdblDC, x, drawArea.bottom - y, x + 3, drawArea.bottom);
			}

			SelectObject(HdblDC, holdPen);
			SelectObject(HdblDC, holdBrush);
		}

		HDC hdc = GetDC(hwnd);
		SetViewportOrgEx(hdc, 0, 0, NULL);

		BitBlt(hdc, drawArea.left, drawArea.top, drawArea.right - drawArea.left, drawArea.bottom - drawArea.top, HdblDC, drawArea.left, drawArea.top, SRCCOPY);
		ReleaseDC(hwnd, hdc);
	}

	virtual void run() {
		char lpData[2 * MAXBUFFLEN];
		DWORD dwBytesRecorded = 0;
		bool hasNewData = false;

		while(!isInterrupted()) {
			wrLock.lockRead();

			hasNewData = m_hasNewData;
			if (hasNewData) {
				memcpy(lpData, m_lpData, m_dwBytesRecorded);
				dwBytesRecorded = m_dwBytesRecorded;
				m_hasNewData = false;
			}

			wrLock.unlockRead();

			if (hasNewData) doGraphicalStuff(lpData, dwBytesRecorded);
			else ::Sleep(10);
		}

		draw(NULL);
	}

public:
	void useBuffer(LPSTR lpData, DWORD dwBytesRecorded) {
		wrLock.lockWrite();

		memcpy((void *) m_lpData, (void *) lpData, dwBytesRecorded);
		m_dwBytesRecorded = dwBytesRecorded;
		m_hasNewData = true;

		wrLock.unlockWrite();
	}

	drawThread(HWND hwnd): CThread(), wrLock(1) {
		m_hasNewData = false;
		max_fr = (0x7fff)/4;
		this->hwnd = hwnd;

		// prepare the DSP processing objects
		m_pFFTransStereo = new FFTransform (44100, MAXBUFFLEN);
		if (m_pFFTransStereo == NULL) throw "Can't instantiate FFT class.";

		m_pSampleIter = new SampleIter();
		if (m_pSampleIter == NULL) {
			delete m_pFFTransStereo;
			throw "Can't instantiate SimpleIterator class.";
		}

		GetClientRect(this->hwnd, &drawArea);
		HDC dc = GetDC(this->hwnd);
		HdblDC = CreateCompatibleDC(dc);
		HdblOldBitmap = CreateCompatibleBitmap(dc, drawArea.right, drawArea.bottom);
		HdblOldBitmap = (HBITMAP) SelectObject(HdblDC, HdblOldBitmap);
		ReleaseDC(this->hwnd, dc);

		hpen = CreatePen(PS_SOLID, 1, RGB(0, 200, 0));
		hbrWhite = CreateSolidBrush(RGB(255,255,255));
		hbrGreen = CreateSolidBrush(RGB(0, 200, 0));
	}

	virtual ~drawThread() {
	    this->interrupt();
		// wait until thread ends
	    this->join();

		delete m_pFFTransStereo;
		delete m_pSampleIter;

		DeleteObject(hpen);
		DeleteObject(hbrGreen);
		DeleteObject(hbrWhite);     

		DeleteObject(SelectObject (HdblDC, HdblOldBitmap));
		DeleteDC(HdblDC);
		HdblDC = NULL;
		HdblOldBitmap = NULL;
	}
};

//--------------------------------------------------------------------------------
class mp3Writer: public IReceiver {
private:
	CMP3Simple	m_mp3Enc;
	FILE *f;
	drawThread thr;

public:
	mp3Writer(HWND hwnd, const TCHAR *file, unsigned int bitrate = 128, unsigned int finalSimpleRate = 0, 
		STEREOMODE eMode = JOINTSTEREO): thr(hwnd), m_mp3Enc(bitrate, 44100, finalSimpleRate, eMode) {

		if (file == NULL) throw "NULL MP3 file name provided.";

		f = _tfopen(file, _T("wb"));
		if (f == NULL) throw "Can't create MP3 file.";

		thr.start();
	};

	~mp3Writer() {
		fclose(f);
	};

	virtual void ReceiveBuffer(LPSTR lpData, DWORD dwBytesRecorded) {
		BYTE	mp3Out[MAXBUFFLEN];
		DWORD	dwOut = 0;

		memset(mp3Out, 0,sizeof(BYTE) * MAXBUFFLEN);
		m_mp3Enc.Encode((PSHORT) lpData, dwBytesRecorded/2, mp3Out, &dwOut);
		fwrite(mp3Out, dwOut, 1, f);
		thr.useBuffer(lpData, dwBytesRecorded);
	};
};

//--------------------------------------------------------------------------------
CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
	m_pDialogBkBrush = new CBrush(RGB(255, 255, 255));
	m_pStaticBkBrush = new CBrush(RGB(255, 255, 255));
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	DDX_Control(pDX, IDC_STATICINFO, m_stInfo);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
	ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMp3_stream_GUIDlg dialog

CMp3_stream_GUIDlg::CMp3_stream_GUIDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMp3_stream_GUIDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CMp3_stream_GUIDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDI_MAINICON);

	m_pDialogBkBrush = new CBrush(RGB(255, 255, 255));
	m_pStaticBkBrush = new CBrush(RGB(255, 255, 255));
	m_bSTATICLOAD = false;
	m_bSTATICSAVE = false;
	m_bSTATICHELP = false;
	
	m_isRecording = false;
	m_mp3Wr = NULL;
}

void CMp3_stream_GUIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMp3_stream_GUIDlg)
	DDX_Control(pDX, IDC_STATICVOLUME, m_stVolum);
	DDX_Control(pDX, IDC_SLIDERSOUNDVOLUME, m_sldVolume);
	DDX_Control(pDX, IDCANCEL, m_btCancel);
	DDX_Control(pDX, IDOK, m_btOK);
	DDX_Control(pDX, IDC_COMBOINPUTLINE, m_cbInputLine);
	DDX_Control(pDX, IDC_COMBOSOUNDDRIVER, m_cbDriver);
	DDX_Control(pDX, IDC_COMBOSTEREOMODE, m_cbStereoMode);
	DDX_Control(pDX, IDC_COMBOSAMPLERATE, m_cbSampleRate);
	DDX_Control(pDX, IDC_COMBOBITRATE, m_cbBitRate);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CMp3_stream_GUIDlg, CDialog)
	//{{AFX_MSG_MAP(CMp3_stream_GUIDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CTLCOLOR()
	ON_WM_DESTROY()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_CBN_SELCHANGE(IDC_COMBOSOUNDDRIVER, OnSelchangeCombosounddriver)
	ON_WM_CLOSE()
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SLIDERSOUNDVOLUME, OnReleasedCaptureSlidersoundVolume)
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDC_STATICSAVE, OnStaticSave)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_STATICLOAD, OnStaticLoad)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMp3_stream_GUIDlg uses to change font for particular control

void CMp3_stream_GUIDlg::chageFontStyle(int ControlID, bool setUnderline) 
{
	CWnd *wnd = GetDlgItem(ControlID);

	if (wnd != NULL) {
		if (setUnderline) wnd->SetFont(&m_cfBoldUnderlined);
		else wnd->SetFont(&m_cfBold);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CMp3_stream_GUIDlg message handlers

BOOL CMp3_stream_GUIDlg::OnInitDialog()
{
	UINT i = 0;
	HRESULT hResult = S_OK;

	setlocale( LC_ALL, ".866");
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	LOGFONT lf;
	CFont *cfont = GetDlgItem(IDC_STATICLOAD)->GetFont();
	cfont->GetLogFont(&lf);
	lf.lfWeight = FW_BOLD;

	m_cfBold.CreateFontIndirect(&lf);
	lf.lfUnderline = TRUE;
	m_cfBoldUnderlined.CreateFontIndirect(&lf);

	// Set font to BOLD for left "menu"
	chageFontStyle(IDC_STATICLOAD);
	chageFontStyle(IDC_STATICSAVE);
	chageFontStyle(IDC_STATICHELP);
	
	// pre-selelct settings for MP3 combo box
	m_cbStereoMode.SetCurSel(0);
	m_cbBitRate.SetCurSel(11);
	m_cbSampleRate.SetCurSel(0);

	// pre-load waveIN device list
	const vector<CWaveINSimple*>& wInDevices = CWaveINSimple::GetDevices();
	for (i = 0; i < wInDevices.size(); i++) {
		m_cbDriver.AddString(wInDevices[i]->GetName());
	}
	if (m_cbDriver.GetCount() > 0) {
		m_cbDriver.SetCurSel(0);
		OnSelchangeCombosounddriver();
	}

	// set statuses for buttons
	m_btOK.EnableWindow(TRUE);
	m_btCancel.EnableWindow(FALSE);

	m_sldVolume.SetPos(100);
	OnReleasedCaptureSlidersoundVolume(NULL,NULL);

	hResult = CoInitialize(NULL);
	if (FAILED(hResult)) {
		AfxMessageBox( _T("Failed to initialize COM environment.") ); 
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CMp3_stream_GUIDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMp3_stream_GUIDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMp3_stream_GUIDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

// grab bitrate
unsigned int CMp3_stream_GUIDlg::getBitRate() {
	int idx;
	CString str;

	idx = m_cbBitRate.GetCurSel();
	if (idx < 0) return 0;

	m_cbBitRate.GetLBText( idx, str );
	return (unsigned int) atoi(str);
}

// grab sample rate
unsigned int CMp3_stream_GUIDlg::getSimpleRate() {
	int idx;
	CString str;

	idx = m_cbSampleRate.GetCurSel();
	if (idx < 0) return 0;

	m_cbSampleRate.GetLBText( idx, str );
	return (unsigned int) atoi(str);
}

// grab stereo mode
void CMp3_stream_GUIDlg::getStereoMode(CString &str) {
	int idx;

	str.Empty();
	idx = m_cbStereoMode.GetCurSel();
	if (idx < 0) return;
	m_cbStereoMode.GetLBText( idx, str );
}

void CMp3_stream_GUIDlg::start() {
	CString strFile, strFilePattern; 
	unsigned int bitrate, simpleRate; 
	STEREOMODE eMode = JOINTSTEREO;
	CString str;
	GUID guid;

	if (!validate()) return;
	if ((CoCreateGuid(&guid)) != S_OK) return;
	strFilePattern.Format(_T("music{%x-%x-%x-%x}"),guid.Data1,guid.Data2,guid.Data3,guid.Data4);

	bitrate = getBitRate();
	if (bitrate <= 0) return;

	simpleRate = getSimpleRate();
	if (simpleRate <= 0) return;

	getStereoMode(str);
	if (str.CompareNoCase(_T("Stereo")) == 0) eMode = STEREO;
	else if (str.CompareNoCase(_T("Dual Channel")) == 0) eMode = DUALCHANNEL;

	CFileDialog FileDlg(FALSE, ".mp3", strFilePattern, OFN_OVERWRITEPROMPT, szMP3Filter);
	FileDlg.m_ofn.lpstrInitialDir = szMusicFolder;
	if( FileDlg.DoModal() != IDOK ) return;

	strFile = FileDlg.GetFileName();

	try {
		CWaveINSimple& device = CWaveINSimple::GetDevice(m_strDriver);
		CMixer& mixer = device.OpenMixer();
		CMixerLine& mixerline = mixer.GetLine(m_strLine);

		mixerline.UnMute();
		mixerline.SetVolume(m_sldVolume.GetPos());
		mixerline.Select();
		mixer.Close();

		mp3Writer *p = new mp3Writer(::GetDlgItem(GetSafeHwnd(), IDC_STATICIMAGE), strFile, bitrate, simpleRate, eMode);
		if (p == NULL) return;

		device.Start((IReceiver *) p);
		m_mp3Wr = (void *) p;
	}
	catch (const char *err) {
		AfxMessageBox( err ); 
		return;
	}

	m_btOK.EnableWindow(FALSE);
	m_cbStereoMode.EnableWindow(FALSE);
	m_cbBitRate.EnableWindow(FALSE);
	m_cbSampleRate.EnableWindow(FALSE);
	m_cbDriver.EnableWindow(FALSE);
	m_cbInputLine.EnableWindow(FALSE);
	m_sldVolume.EnableWindow(FALSE);

	m_btCancel.EnableWindow(TRUE);
	m_isRecording = true;
}

void CMp3_stream_GUIDlg::stop() {
	try {
		CWaveINSimple& device = CWaveINSimple::GetDevice(m_strDriver);
		device.Stop();

		if (m_mp3Wr != NULL) {
			mp3Writer *p = (mp3Writer*) m_mp3Wr;
			delete p;
			m_mp3Wr = NULL;
		}
	}
	catch (const char *err) {
		AfxMessageBox( err ); 
		return;
	}

	//::Sleep(500);

	m_btOK.EnableWindow(TRUE);
	m_cbStereoMode.EnableWindow(TRUE);
	m_cbBitRate.EnableWindow(TRUE);
	m_cbSampleRate.EnableWindow(TRUE);
	m_cbDriver.EnableWindow(TRUE);
	m_cbInputLine.EnableWindow(TRUE);
	m_sldVolume.EnableWindow(TRUE);

	m_btCancel.EnableWindow(FALSE);
	m_isRecording = false;
}

bool CMp3_stream_GUIDlg::validate() {

	m_strDriver.Empty();
	m_strLine.Empty();

	int idxDriver = m_cbDriver.GetCurSel();
	if (idxDriver < 0) {
		AfxMessageBox( _T("Please select a Sound Device!") ); 
		return false;
	}

	int idxLine = m_cbInputLine.GetCurSel();
	if (idxLine < 0) {
		AfxMessageBox( _T("Please select an Input Line!") ); 
		return false;
	}

	m_cbDriver.GetLBText( idxDriver, m_strDriver );
	m_cbInputLine.GetLBText( idxLine, m_strLine );

	if (m_strDriver.IsEmpty()) {
		AfxMessageBox( _T("Please select a Sound Device!") ); 
		return false;
	}

	if (m_strLine.IsEmpty()) {
		AfxMessageBox( _T("Please select an Input Line!") ); 
		return false;
	}

	return true;
}

void CMp3_stream_GUIDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
//	CDialog::OnCancel();
	stop();
}

void CMp3_stream_GUIDlg::OnOK() 
{
	// TODO: Add extra validation here
//	CDialog::OnOK();
	start();
}

HBRUSH CMp3_stream_GUIDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
//	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	
	// TODO: Change any attributes of the DC here
	
	// TODO: Return a different brush if the default is not desired

	switch (nCtlColor) {
		case CTLCOLOR_STATIC:
              // Set color to green on black and return the background brush.
			pDC->SetTextColor(RGB(0, 101, 202));
			pDC->SetBkColor(RGB(255, 255, 255));
//			switch (pWnd->GetDlgCtrlID()) {
//			case IDC_STATICLOAD:
//			case IDC_STATICSAVE:
//			case IDC_STATICHELP:;
//			}
			return (HBRUSH)(m_pStaticBkBrush->GetSafeHandle());	

		case CTLCOLOR_EDIT:
		case CTLCOLOR_LISTBOX:
			pDC->SetTextColor(RGB(0, 0, 0));
			pDC->SetBkColor(RGB(255, 255, 255));
			return (HBRUSH)(m_pDialogBkBrush->GetSafeHandle());	

		case CTLCOLOR_SCROLLBAR:
		case CTLCOLOR_BTN:
		case CTLCOLOR_DLG:
		case CTLCOLOR_MSGBOX:
			pDC->SetTextColor(RGB(0, 101, 202));
			pDC->SetBkColor(RGB(255, 255, 255));
			return (HBRUSH)(m_pDialogBkBrush->GetSafeHandle());	

		default:
			return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	}

//	return hbr;
}

void CMp3_stream_GUIDlg::OnDestroy() 
{
	CDialog::OnDestroy();
	
	// TODO: Add your message handler code here
//	if (m_isRecording) stop();
	delete m_pStaticBkBrush;
	delete m_pDialogBkBrush;
	CWaveINSimple::CleanUp();
	CoUninitialize();
}

// check if point is over given control
bool CMp3_stream_GUIDlg::isOverControl(int ControlID, CPoint &point) {
	bool ret = false;
	CRect rect;

	CWnd *wnd = GetDlgItem(ControlID);
	if (wnd != NULL) {
		wnd->GetWindowRect(rect);
		ScreenToClient(rect);
		if (rect.PtInRect(point)) ret = true;
	}

	return ret;
}

// mouse is moved above "Save Config", "Load Config" or "Help"
void CMp3_stream_GUIDlg::OnMouseMove(UINT nFlags, CPoint point) 
{
	HCURSOR lhCursor;

	// TODO: Add your message handler code here and/or call default

	if (!m_isRecording) {
		if (isOverControl(IDC_STATICLOAD, point)) {
			chageFontStyle(IDC_STATICLOAD, true);
			lhCursor = AfxGetApp()->LoadCursor(IDC_HANDCURSOR);
			SetCursor(lhCursor);
			m_bSTATICLOAD = true;
		}
		else {
			if (m_bSTATICLOAD) {
				chageFontStyle(IDC_STATICLOAD);
				lhCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
				SetCursor(lhCursor);
				m_bSTATICLOAD = false;
			}
		}

		if (isOverControl(IDC_STATICSAVE, point)) {
			chageFontStyle(IDC_STATICSAVE, true);
			lhCursor = AfxGetApp()->LoadCursor(IDC_HANDCURSOR);
			SetCursor(lhCursor);
			m_bSTATICSAVE = true;
		}
		else {
			if (m_bSTATICSAVE) {
				chageFontStyle(IDC_STATICSAVE);
				lhCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
				SetCursor(lhCursor);
				m_bSTATICSAVE = false;
			}
		}

		if (isOverControl(IDC_STATICHELP, point)) {
			chageFontStyle(IDC_STATICHELP, true);
			lhCursor = AfxGetApp()->LoadCursor(IDC_HANDCURSOR);
			SetCursor(lhCursor);
			m_bSTATICHELP = true;
		}
		else {
			if (m_bSTATICHELP) {
				chageFontStyle(IDC_STATICHELP);
				lhCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
				SetCursor(lhCursor);
				m_bSTATICHELP = false;
			}
		}
	}

	
	CDialog::OnMouseMove(nFlags, point);
}

void CMp3_stream_GUIDlg::OnLButtonDown(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	OnMouseMove(nFlags, point);
	CDialog::OnLButtonDown(nFlags, point);
}

void CMp3_stream_GUIDlg::OnLButtonUp(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	OnMouseMove(nFlags, point);
	CDialog::OnLButtonUp(nFlags, point);

	// User clicked to save configuration
	if (isOverControl(IDC_STATICSAVE, point)) {
		OnStaticSave();
	}
	else if (isOverControl(IDC_STATICLOAD, point)) {
		OnStaticLoad();
	}
	else if (isOverControl(IDC_STATICHELP, point)) {
		if (!m_isRecording) {
			CAboutDlg dlgAbout;
			dlgAbout.DoModal();
		}
	}
}

void CMp3_stream_GUIDlg::OnRButtonDown(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default

	OnMouseMove(nFlags, point);	
	CDialog::OnRButtonDown(nFlags, point);
}

void CMp3_stream_GUIDlg::OnRButtonUp(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	
	OnMouseMove(nFlags, point);	
	CDialog::OnRButtonUp(nFlags, point);
}

void CMp3_stream_GUIDlg::OnSelchangeCombosounddriver() 
{
//	CHAR szName[MIXER_LONG_NAME_CHARS];
	// TODO: Add your control notification handler code here
	m_cbInputLine.ResetContent();

	int idx = m_cbDriver.GetCurSel();
	if (idx < 0) return;

	CString str;
	m_cbDriver.GetLBText( idx, str );

	try {
		CWaveINSimple& device = CWaveINSimple::GetDevice(str);
		CMixer& mixer = device.OpenMixer();
		const vector<CMixerLine*>& mLines = mixer.GetLines();

		for (UINT j = 0; j < mLines.size(); j++) {
//			::CharToOem(mLines[j]->GetName(), szName);
//			m_cbInputLine.AddString(szName);
			m_cbInputLine.AddString(mLines[j]->GetName());
		}

		mixer.Close();

		if (m_cbInputLine.GetCount() > 0) m_cbInputLine.SetCurSel(0);
	}
	catch (const char *err) {
		AfxMessageBox( err ); 
	}
}

void CMp3_stream_GUIDlg::OnClose() 
{
	// TODO: Add your message handler code here and/or call default
	if (m_isRecording) stop();

	CDialog::OnClose();
	CDialog::OnOK();
}

void CMp3_stream_GUIDlg::OnReleasedCaptureSlidersoundVolume(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	int pos = m_sldVolume.GetPos();
	CString str = "";
	str.Format(_T("Volume (%d%%)"),pos);

	m_stVolum.SetWindowText(str);
	if (pResult != NULL) *pResult = 0;
}

BOOL CAboutDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here

	CMP3Simple::LoadLIBS();

	CString str;
	BE_VERSION bVersion;
	beVersion(&bVersion);
	str.Format(_T("Using lame_enc.dll version %u.%02u (%u/%u/%u)\n"
			"LAME_ENC engine %u.%02u (MMX %s)\n"
			"Homepage at %s\n\n"
			"Have you purchased this software? If not, please consider\n"
			"purchasing it for only 9.95 GBP, we would appreciate it if you did.\n"
			"Use your credit card at PayPal to send the money to the support email\n"
			"below, you will then be emailed a license key entitling you to support.\n"
			"Or you can pay by sending a cheque payable 'Netray' to:\n\n"
			"Unit A1B, Staverton Technology Park\n"
			"Herrick Way, Staverton\n"
			"Cheltenham\n"
			"GL51 6TQ\n"
			"United Kingdon\n"
			"\nSupport at: mp3r@send2.me.uk"
			),	
			bVersion.byDLLMajorVersion, bVersion.byDLLMinorVersion,
			bVersion.byDay, bVersion.byMonth, bVersion.wYear,
			bVersion.byMajorVersion, bVersion.byMinorVersion,
			(bVersion.byMMXEnabled ? "enabled": "disabled"), bVersion.zHomepage);

	m_stInfo.SetWindowText(str);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

HBRUSH CAboutDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
//	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	
	// TODO: Change any attributes of the DC here
	
	// TODO: Return a different brush if the default is not desired


	switch (nCtlColor) {
		case CTLCOLOR_STATIC:
              // Set color to green on black and return the background brush.
			pDC->SetTextColor(RGB(0, 101, 202));
			pDC->SetBkColor(RGB(255, 255, 255));
			return (HBRUSH)(m_pStaticBkBrush->GetSafeHandle());	

		case CTLCOLOR_EDIT:
		case CTLCOLOR_LISTBOX:
			pDC->SetTextColor(RGB(0, 0, 0));
			pDC->SetBkColor(RGB(255, 255, 255));
			return (HBRUSH)(m_pDialogBkBrush->GetSafeHandle());	

		case CTLCOLOR_BTN:
		case CTLCOLOR_DLG:
		case CTLCOLOR_MSGBOX:
			pDC->SetTextColor(RGB(0, 101, 202));
			pDC->SetBkColor(RGB(255, 255, 255));
			return (HBRUSH)(m_pDialogBkBrush->GetSafeHandle());	

		case CTLCOLOR_SCROLLBAR:
		default:
			return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	}

//	return hbr;
}

void CMp3_stream_GUIDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
{
	CDialog::OnShowWindow(bShow, nStatus);
	
	// TODO: Add your message handler code here
}

// "Save Config" is hit
void CMp3_stream_GUIDlg::OnStaticSave() 
{
	CString strFile; 
	unsigned int bitrate, simpleRate;
	CString strStereoMode;
	CString tmp = "";

	HRESULT hResult = S_OK;

	// TODO: Add your control notification handler code here
	if (m_isRecording) return;
	if (!validate()) return;
	
	bitrate = getBitRate();
	if (bitrate <= 0) return;

	simpleRate = getSimpleRate();
	if (simpleRate <= 0) return;

	getStereoMode(strStereoMode);

	CFileDialog FileDlg(FALSE, ".config", _T("MyConfig"), OFN_OVERWRITEPROMPT, szConfigFilter);
	FileDlg.m_ofn.lpstrInitialDir = szConfigFolder;
	if( FileDlg.DoModal() != IDOK ) return;

	strFile = FileDlg.GetFileName();

	//Save setting in XML file
	try {
		IXMLDOMDocument2Ptr spDocOutput;
		IXMLDOMElementPtr spElemRoot;
		IXMLDOMElementPtr spElem;
		IXMLDOMProcessingInstructionPtr spXMLDecl;
		IXMLDOMTextPtr spText;

		hResult = spDocOutput.CreateInstance(__uuidof(DOMDocument40));
		if FAILED(hResult) {
			AfxMessageBox( _T("Failed to create XML document instance.\nPlease check if you have MSXML 4.0 installed.") );
			return;
		}

		spDocOutput->async = VARIANT_FALSE;
		spDocOutput->preserveWhiteSpace = VARIANT_TRUE; 

		spXMLDecl = spDocOutput->createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
		spDocOutput->appendChild(spXMLDecl);

		spElemRoot = spDocOutput->createElement("Settings");
		spDocOutput->appendChild(spElemRoot);
		spText = spDocOutput->createTextNode(_T("\r\n"));
		spElemRoot->appendChild(spText);

		// set device name
		spText = spDocOutput->createTextNode(_T("\t"));
		spElemRoot->appendChild(spText);
		spElem = spDocOutput->createElement("Device_Name");
		spElemRoot->appendChild(spElem);
		spText = spDocOutput->createTextNode((LPCSTR) m_strDriver);
		spElem->appendChild(spText);
		spText = spDocOutput->createTextNode(_T("\r\n"));
		spElemRoot->appendChild(spText);

		// set input line
		spText = spDocOutput->createTextNode(_T("\t"));
		spElemRoot->appendChild(spText);
		spElem = spDocOutput->createElement("Device_InputLine");
		spElemRoot->appendChild(spElem);
		spText = spDocOutput->createTextNode((LPCSTR) m_strLine);
		spElem->appendChild(spText);
		spText = spDocOutput->createTextNode(_T("\r\n"));
		spElemRoot->appendChild(spText);

		// set line's volume
		spText = spDocOutput->createTextNode(_T("\t"));
		spElemRoot->appendChild(spText);
		tmp.Format("%d", m_sldVolume.GetPos());
		spElem = spDocOutput->createElement("Device_Volume");
		spElemRoot->appendChild(spElem);
		spText = spDocOutput->createTextNode((LPCSTR) tmp);
		spElem->appendChild(spText);
		spText = spDocOutput->createTextNode(_T("\r\n"));
		spElemRoot->appendChild(spText);

		// set bitrate
		spText = spDocOutput->createTextNode(_T("\t"));
		spElemRoot->appendChild(spText);
		tmp.Empty();
		tmp.Format("%d", bitrate);
		spElem = spDocOutput->createElement("MP3_BitRate");
		spElemRoot->appendChild(spElem);
		spText = spDocOutput->createTextNode((LPCSTR) tmp);
		spElem->appendChild(spText);
		spText = spDocOutput->createTextNode(_T("\r\n"));
		spElemRoot->appendChild(spText);

		// set simpleRate
		spText = spDocOutput->createTextNode(_T("\t"));
		spElemRoot->appendChild(spText);
		tmp.Empty();
		tmp.Format("%d", simpleRate);
		spElem = spDocOutput->createElement("MP3_SampleRate");
		spElemRoot->appendChild(spElem);
		spText = spDocOutput->createTextNode((LPCSTR) tmp);
		spElem->appendChild(spText);
		spText = spDocOutput->createTextNode(_T("\r\n"));
		spElemRoot->appendChild(spText);

		// set stereo mode
		spText = spDocOutput->createTextNode(_T("\t"));
		spElemRoot->appendChild(spText);
		spElem = spDocOutput->createElement("MP3_StereoMode");
		spElemRoot->appendChild(spElem);
		spText = spDocOutput->createTextNode((LPCSTR) strStereoMode);
		spElem->appendChild(spText);
		spText = spDocOutput->createTextNode(_T("\r\n"));
		spElemRoot->appendChild(spText);

		hResult = spDocOutput->save((LPCSTR) strFile);
		if FAILED(hResult) {
			AfxMessageBox( _T("Failed to save document.") );
		}
	}
	catch (_com_error &e) {
		AfxMessageBox( e.ErrorMessage() );
	}
}

// detect vertical scrolling, mainly to detect 
// when scrolling is caused by keyboard
void CMp3_stream_GUIDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	// TODO: Add your message handler code here and/or call default

	// adjust sound volume
	OnReleasedCaptureSlidersoundVolume(NULL, NULL);
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CMp3_stream_GUIDlg::OnStaticLoad() 
{
	CString strFile; 
	CString strBitRate, strSimpleRate;
	CString strStereoMode;
	CString strDevice, strInputLine, strVolume;
	int idx;

	// TODO: Add your control notification handler code here
	if (m_isRecording) return;

	CFileDialog FileDlg(TRUE, ".config", NULL, OFN_OVERWRITEPROMPT, szConfigFilter);
	FileDlg.m_ofn.lpstrInitialDir = szConfigFolder;
	if( FileDlg.DoModal() != IDOK ) return;

	strFile = FileDlg.GetFileName();

	try {
		IXMLDOMDocument2Ptr	ptrDomDocument;
		IXMLDOMElementPtr	ptrDocRoot;
		HRESULT				hResult;

		hResult = ptrDomDocument.CreateInstance(__uuidof(DOMDocument40));
		if FAILED(hResult) {
			AfxMessageBox( _T("Failed to create XML document instance.\nPlease check if you have MSXML 4.0 installed.") );
			return;
		}

		ptrDomDocument->async = VARIANT_FALSE;
		// Load provided XML file
		hResult = ptrDomDocument->load((LPCSTR) strFile);
		if ( hResult != VARIANT_TRUE ) {
			IXMLDOMParseErrorPtr  pError;
			pError = ptrDomDocument->parseError;
			_bstr_t parseError =_bstr_t("Error parsing file.\nAt line ") + 
				_bstr_t(pError->Getline()) + _bstr_t(": ")+ _bstr_t(pError->Getreason());

			AfxMessageBox( parseError );
			return;
		}

		ptrDocRoot = ptrDomDocument->documentElement;
		// basic check to verify if root element is "Settings"
		if ((_wcsicmp((wchar_t *) ptrDocRoot->nodeName,L"Settings")) != 0) {
			_bstr_t parseError =_bstr_t("Illegal root node name \"") + 
				_bstr_t(ptrDocRoot->nodeName) + _bstr_t("\" (should be \"Settings\").");

			AfxMessageBox( parseError );
			return;
		}

		// process nodes of the XML file.
		for (IXMLDOMNodePtr pChild = ptrDocRoot->firstChild; pChild != NULL; pChild = pChild->nextSibling) {
			// comment found, so skip it
			if ((_wcsicmp((wchar_t *) pChild->nodeName,L"#comment")) == 0) continue;

			if ((_wcsicmp((wchar_t *) pChild->nodeName,L"Device_Name")) == 0) {
				strDevice = (LPCSTR) pChild->text;
				strDevice.TrimLeft();
				strDevice.TrimRight();
			}
			else if ((_wcsicmp((wchar_t *) pChild->nodeName,L"Device_InputLine")) == 0) {
				strInputLine = (LPCSTR) pChild->text;
				strInputLine.TrimLeft();
				strInputLine.TrimRight();
			}
			else if ((_wcsicmp((wchar_t *) pChild->nodeName,L"Device_Volume")) == 0) {
				strVolume = (LPCSTR) pChild->text;
				strVolume.TrimLeft();
				strVolume.TrimRight();
			}
			else if ((_wcsicmp((wchar_t *) pChild->nodeName,L"MP3_BitRate")) == 0) {
				strBitRate = (LPCSTR) pChild->text;
				strBitRate.TrimLeft();
				strBitRate.TrimRight();
			}
			else if ((_wcsicmp((wchar_t *) pChild->nodeName,L"MP3_SampleRate")) == 0) {
				strSimpleRate = (LPCSTR) pChild->text;
				strSimpleRate.TrimLeft();
				strSimpleRate.TrimRight();
			}
			else if ((_wcsicmp((wchar_t *) pChild->nodeName,L"MP3_StereoMode")) == 0) {
				strStereoMode = (LPCSTR) pChild->text;
				strStereoMode.TrimLeft();
				strStereoMode.TrimRight();
			}
		}

		ptrDocRoot.Release();
		ptrDomDocument.Release();
	}
	catch (_com_error &e) {
		AfxMessageBox( e.ErrorMessage() );
		return;
	}

	// basic data validation
	if (strDevice.IsEmpty()) {
		AfxMessageBox( _T("Load failed! Empty Device name provided.") );
		return;
	}

	if (strInputLine.IsEmpty()) {
		AfxMessageBox( _T("Load failed! Empty Input Line provided.") );
		return;
	}

	if (strVolume.IsEmpty()) {
		AfxMessageBox( _T("Load failed! Empty Volume level provided.") );
		return;
	}

	if (strBitRate.IsEmpty()) {
		AfxMessageBox( _T("Load failed! Empty Bit rate provided.") );
		return;
	}

	if (strSimpleRate.IsEmpty()) {
		AfxMessageBox( _T("Load failed! Empty Sample rate provided.") );
		return;
	}

	if (strStereoMode.IsEmpty()) {
		AfxMessageBox( _T("Load failed! Empty Stereo mode provided.") );
		return;
	}

	// apply settings
	idx = m_cbDriver.FindStringExact(-1, strDevice);
	if (idx != CB_ERR) {
		m_cbDriver.SetCurSel(idx);
		OnSelchangeCombosounddriver();
	}

	idx = m_cbInputLine.FindStringExact(-1, strInputLine);
	if (idx != CB_ERR) m_cbInputLine.SetCurSel(idx);

	idx = atoi(strVolume);
	if ((idx >= 0) && (idx <= 100)) {
		m_sldVolume.SetPos(idx);
		OnReleasedCaptureSlidersoundVolume(NULL,NULL);
	}

	idx = m_cbBitRate.FindStringExact(-1, strBitRate);
	if (idx != CB_ERR) m_cbBitRate.SetCurSel(idx);

	idx = m_cbSampleRate.FindStringExact(-1, strSimpleRate);
	if (idx != CB_ERR) m_cbSampleRate.SetCurSel(idx);

	idx = m_cbStereoMode.FindStringExact(-1, strStereoMode);
	if (idx != CB_ERR) m_cbStereoMode.SetCurSel(idx);
}
