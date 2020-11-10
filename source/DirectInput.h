#pragma once


namespace DIMouse
{
	HRESULT DirectInputInit( HWND hDlg );
	void DirectInputUninit( HWND hDlg );
	HRESULT ReadImmediateData( long* pX=NULL, long* pY=NULL );
};
