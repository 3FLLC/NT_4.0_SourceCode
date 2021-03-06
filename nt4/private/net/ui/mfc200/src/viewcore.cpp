// This is a part of the Microsoft Foundation Classes C++ library. 
// Copyright (C) 1992 Microsoft Corporation 
// All rights reserved. 
//  
// This source code is only intended as a supplement to the 
// Microsoft Foundation Classes Reference and Microsoft 
// QuickHelp and/or WinHelp documentation provided with the library. 
// See these sources for detailed information regarding the 
// Microsoft Foundation Classes product. 

#include "stdafx.h"

#ifdef AFX_CORE2_SEG
#pragma code_seg(AFX_CORE2_SEG)
#endif

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CView

// IMPLEMENT_DYNAMIC for CView is in winfrm.cpp for .OBJ granularity reasons

BEGIN_MESSAGE_MAP(CView, CWnd)
	//{{AFX_MSG_MAP(CView)
	ON_WM_PAINT()
	ON_WM_MOUSEACTIVATE()
	ON_WM_CREATE()
	ON_WM_DESTROY()

	// Standard commands for split pane
	ON_COMMAND_EX(ID_WINDOW_SPLIT, OnSplitCmd)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_SPLIT, OnUpdateSplitCmd)

	// Standard commands for next pane
	ON_UPDATE_COMMAND_UI(ID_NEXT_PANE, OnUpdateNextPaneMenu)
	ON_COMMAND_EX(ID_NEXT_PANE, OnNextPaneCmd)
	ON_UPDATE_COMMAND_UI(ID_PREV_PANE, OnUpdateNextPaneMenu)
	ON_COMMAND_EX(ID_PREV_PANE, OnNextPaneCmd)
	//}}AFX_MSG_MAP
	// special command for Initial Update
	ON_MESSAGE_VOID(WM_INITIALUPDATE, OnInitialUpdate)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CView construction/destruction

CView::CView()
{
	m_pDocument = NULL;
}

CView::~CView()
{
	if (m_pDocument != NULL)
		m_pDocument->RemoveView(this);
}

/////////////////////////////////////////////////////////////////////////////
// CView second phase construction - bind to document

BOOL CView::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(cs.style & WS_CHILD);
	if (cs.lpszClass == NULL)
		cs.lpszClass = _afxWndFrameOrView;  // COLOR_WINDOW background
	return TRUE;
}

int CView::OnCreate(LPCREATESTRUCT lpcs)
{
	if (CWnd::OnCreate(lpcs) == -1)
		return -1;

	// if ok, wire in the current document
	ASSERT(m_pDocument == NULL);
	CCreateContext* pContext;
	pContext = (CCreateContext*)_AfxGetPtrFromFarPtr(lpcs->lpCreateParams);

	// A pane MUST be created in a given context !
	ASSERT(pContext != NULL);
	if (pContext->m_pCurrentDoc != NULL)
	{
		pContext->m_pCurrentDoc->AddView(this);
		ASSERT(m_pDocument != NULL);
	}
	else
	{
		TRACE0("Warning: Creating a pane with no CDocument\n");
	}

	return 0;   // ok
}

void CView::OnDestroy()
{
	CFrameWnd* pFrame = GetParentFrame();
	if (pFrame != NULL && pFrame->GetActiveView() == this)
		pFrame->SetActiveView(NULL);    // deactivate during death
	CWnd::OnDestroy();
}

// self destruction
void CView::PostNcDestroy()
{
	// default for views is to allocate them on the heap
	//  the default post-cleanup is to 'delete this'.
	// never explicitly call 'delete' on a view
	delete this;
}

/////////////////////////////////////////////////////////////////////////////
// Command routing

BOOL CView::OnCmdMsg(UINT nID, int nCode, void* pExtra,
	AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// first pump through pane
	if (CWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;

	// then pump through document
	BOOL bHandled = FALSE;
	if (m_pDocument != NULL)
	{
		// special state for saving view of 
		CView*  pOldRoutingView = CCmdTarget::pRoutingView;
		CCmdTarget::pRoutingView = this;
		bHandled = m_pDocument->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		CCmdTarget::pRoutingView = pOldRoutingView;
	}

	return bHandled;
}

/////////////////////////////////////////////////////////////////////////////
// CView drawing support

void CView::OnPaint()
{
	// standard paint routine
	CPaintDC dc(this);
	OnPrepareDC(&dc);
	OnDraw(&dc);
}

void CView::OnInitialUpdate()
{
	OnUpdate(NULL, 0, NULL);        // initial update
}

void CView::OnUpdate(CView* pSender, LPARAM /*lHint*/, CObject* /*pHint*/)
{
	ASSERT(pSender != this);

	// invalidate the entire pane, erase background too
	Invalidate(TRUE);
}

void CView::OnPrint(CDC* pDC, CPrintInfo*)
{
	ASSERT_VALID(pDC);

	// Override and set printing variables based on page number
	OnDraw(pDC);                    // Call Draw
}

/////////////////////////////////////////////////////////////////////////////
// CView selection support

BOOL CView::IsSelected(const CObject* pDocItem) const
{
	ASSERT_VALID(pDocItem);

	return FALSE;       // not selected as far as I know
}

void CView::OnActivateView(BOOL bActivate, CView* pActivateView, CView*)
{
	if (bActivate)
	{
		ASSERT(pActivateView == this);
		SetFocus(); // take the focus if the view/pane is active
	}
}

int CView::OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message)
{
	int maRet = CWnd::OnMouseActivate(pDesktopWnd, nHitTest, message);
	if (maRet == MA_NOACTIVATE || maRet == MA_NOACTIVATEANDEAT)
		return maRet;   // frame does not want to activate

	// eat it if this will cause activation
	CFrameWnd* pParentFrame = GetParentFrame();
	ASSERT(pParentFrame == pDesktopWnd || pDesktopWnd->IsChild(pParentFrame));
	pParentFrame->SetActiveView(this); // activate or re-activate us

	return maRet;
}



/////////////////////////////////////////////////////////////////////////////
// CView splitting commands

IMPLEMENT_DYNAMIC(CSplitterWnd, CWnd)   // for swap tuning

static CSplitterWnd* GetParentSplitter(const CWnd* pWnd, BOOL bAnyState)
{
	CSplitterWnd* pSplitter = (CSplitterWnd*)pWnd->GetParent();
	if (!pSplitter->IsKindOf(RUNTIME_CLASS(CSplitterWnd)))
		return NULL;        // not a splitter
	if (!bAnyState)
	{
		// ignore splitters in minimized (iconic) windows
		while ((pWnd = pWnd->GetParent()) != NULL)
			if (pWnd->IsIconic())
				return NULL;
	}
	return pSplitter;
}

CScrollBar* CView::GetScrollBarCtrl(int nBar) const
{
	ASSERT(nBar == SB_HORZ || nBar == SB_VERT);
	if (GetStyle() & ((nBar == SB_HORZ) ? WS_HSCROLL : WS_VSCROLL))
	{
		// it has a regular windows style scrollbar (no control)
		return NULL;
	}

	CWnd* pParent = GetParentSplitter(this, TRUE);
	if (pParent == NULL)
		return NULL;            // no splitter

	UINT nID = _AfxGetDlgCtrlID(m_hWnd);
	if (nID < AFX_IDW_PANE_FIRST || nID > AFX_IDW_PANE_LAST)
		return NULL;            // not a standard pane ID

	// appropriate PANE id - look for sibling (splitter, or just frame)
	UINT nIDScroll;
	if (nBar == SB_HORZ)
		nIDScroll = AFX_IDW_HSCROLL_FIRST + (nID - AFX_IDW_PANE_FIRST) % 16;
	else
		nIDScroll = AFX_IDW_VSCROLL_FIRST + (nID - AFX_IDW_PANE_FIRST) / 16;

	// return shared scroll bars that are immediate children of splitter
	return (CScrollBar*)pParent->GetDlgItem(nIDScroll);
}


void CView::OnUpdateSplitCmd(CCmdUI* pCmdUI)
{
	CSplitterWnd* pSplitter = GetParentSplitter(this, FALSE);
	pCmdUI->Enable(pSplitter != NULL);
}

BOOL CView::OnSplitCmd(UINT)
{
	CSplitterWnd* pSplitter = GetParentSplitter(this, FALSE);
	if (pSplitter == NULL)
		return FALSE;

	pSplitter->DoKeyboardSplit();
	return TRUE;    // attempted at least
}

void CView::OnUpdateNextPaneMenu(CCmdUI* pCmdUI)
{
	ASSERT(pCmdUI->m_nID == ID_NEXT_PANE ||
		pCmdUI->m_nID == ID_PREV_PANE);
	CSplitterWnd* pSplitter = GetParentSplitter(this, FALSE);
	pCmdUI->Enable(pSplitter != NULL &&
		pSplitter->CanActivateNext(pCmdUI->m_nID == ID_PREV_PANE));
}

BOOL CView::OnNextPaneCmd(UINT nID)
{
	CSplitterWnd* pSplitter = GetParentSplitter(this, FALSE);
	if (pSplitter == NULL)
		return FALSE;

	ASSERT(nID == ID_NEXT_PANE || nID == ID_PREV_PANE);
	pSplitter->ActivateNext(nID == ID_PREV_PANE);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// Printing support virtual functions (others in viewpr.cpp)

void CView::OnPrepareDC(CDC* pDC, CPrintInfo* pInfo)
{
	ASSERT_VALID(pDC);

	// Default to one page printing if doc length not known
	if (pInfo != NULL)
		pInfo->m_bContinuePrinting = 
			(pInfo->GetMaxPage() != 0xffff || (pInfo->m_nCurPage == 1));
}


BOOL CView::OnPreparePrinting(CPrintInfo*)
{
	// Do print DC initialization here
	// override and call DoPreparePrinting (in viewprnt.cpp)

	return TRUE;
}

void CView::OnBeginPrinting(CDC* pDC, CPrintInfo*)
{
	ASSERT_VALID(pDC);
	// Do printing initialization here
}

void CView::OnEndPrinting(CDC* pDC, CPrintInfo*)
{
	ASSERT_VALID(pDC);
	// Do printing cleanup here
}


// OnEndPrintPreview is here for swap tuning reasons
//  (see viewprev.cpp for complete preview mode implementation)
void CView::OnEndPrintPreview(CDC* pDC, CPrintInfo* pInfo, 
			POINT, CPreviewView* pView)
{
	ASSERT_VALID(pDC);
	ASSERT_VALID(pView);

	if (pView->m_pPrintView != NULL)
		pView->m_pPrintView->OnEndPrinting(pDC, pInfo);

	CFrameWnd* pParent = (CFrameWnd*) AfxGetApp()->m_pMainWnd;
	ASSERT(pParent->IsKindOf(RUNTIME_CLASS(CFrameWnd)));

	// restore the old main window
	pParent->OnSetPreviewMode(FALSE, pView->m_pPreviewState);

	// Force active view back to old one
	pParent->SetActiveView(pView->m_pPreviewState->pViewActiveOld);
	if (pParent != GetParentFrame())
		OnActivateView(TRUE, this, this);   // re-activate view in real frame
	pView->DestroyWindow();     // destroy preview view
			// C++ object will be deleted in PostNcDestroy

	// restore main frame layout and idle message
	pParent->RecalcLayout();
	pParent->SendMessage(WM_SETMESSAGESTRING, (WPARAM)AFX_IDS_IDLEMESSAGE, 0L);
}



/////////////////////////////////////////////////////////////////////////////
// CView diagnostics

#ifdef _DEBUG
void CView::Dump(CDumpContext& dc) const
{
	CWnd::Dump(dc);
	if (m_pDocument != NULL)
		AFX_DUMP1(dc, "\nwith document ", m_pDocument);
	else
		AFX_DUMP0(dc, "\nwith no document");
}

void CView::AssertValid() const
{
	CWnd::AssertValid();
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
