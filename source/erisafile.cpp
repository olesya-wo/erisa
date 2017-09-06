
/*****************************************************************************
                         E R I S A - L i b r a r y
													last update 2003/03/07
 -----------------------------------------------------------------------------
	Copyright (C) 2002-2003 Leshade Entis, Entis-soft. All rights reserved.
 *****************************************************************************/


#define	STRICT	1
#include "xerisa.h"
#include <string.h>


/*****************************************************************************
                  EMC (Entis Media Complex) ファイル形式
 *****************************************************************************/

char	EMCFile::m_cDefSignature[8] = { 'E', 'n', 't', 'i', 's', 0x1A, 0, 0 } ; 

// クラス情報
//////////////////////////////////////////////////////////////////////////////
//IMPLEMENT_CLASS_INFO( EMCFile, ESLFileObject )

// 構築関数
//////////////////////////////////////////////////////////////////////////////
EMCFile::EMCFile( void )
{
	m_pFile = NULL ;
	m_pRecord = NULL ;
}

// 消滅関数
//////////////////////////////////////////////////////////////////////////////
EMCFile::~EMCFile( void )
{
	Close( ) ;
}

// ファイルを開く
//////////////////////////////////////////////////////////////////////////////
ESLError EMCFile::Open( ESLFileObject * pfile, const FILE_HEADER * pfhHeader )
{
	//
	// 現在のファイルを閉じる
	//
	Close( ) ;
	//
	// ファイルを関連付けて開く
	//
	ESLAssert( pfile != NULL ) ;
	m_pFile = pfile ;
	SetAttribute( m_pFile->GetAttribute() ) ;
	//
	if ( (GetAttribute() & modeCreateFlag) && (pfhHeader != NULL) )
	{
		//
		// ファイルヘッダを書き出す
		//
		if ( m_pFile->Write
			( pfhHeader, sizeof(FILE_HEADER) ) < sizeof(FILE_HEADER) )
		{
            ESLTrace( "Failed to write the file header.\n" ) ;
			return	eslErrGeneral ;
		}
		//
		// ルートレコードを設定する
		//
		m_pRecord = new RECORD_INFO ;
		m_pRecord->pParent = NULL ;
		m_pRecord->dwWriteFlag = 1 ;
		m_pRecord->qwBasePos = m_pFile->GetLargePosition( ) ;
		m_pRecord->rechdr.nRecordID = 0 ;
		m_pRecord->rechdr.nRecLength = 0 ;
	}
	else
	{
		//
		// ファイルヘッダを読み込む
		//
		SetAttribute( m_pFile->GetAttribute() & ~modeCreateFlag ) ;
		if ( m_pFile->Read( &m_fhHeader, sizeof(FILE_HEADER) ) < sizeof(FILE_HEADER) )
		{
            ESLTrace( "Failed to read the file header.\n" ) ;
			return	eslErrGeneral ;
		}
		if ( (m_fhHeader.dwReserved != 0) ||
				(*((UINT64*)&m_fhHeader.cHeader[0])
					!= *((UINT64*)m_cDefSignature)) )
		{
            ESLTrace( "Invalid file header.\n" ) ;
			return	eslErrGeneral ;
		}
		//
		// ルートレコードを読み込む
		//
		m_pRecord = new RECORD_INFO ;
		m_pRecord->pParent = NULL ;
		m_pRecord->dwWriteFlag = 0 ;
		m_pRecord->qwBasePos = m_pFile->GetLargePosition( ) ;
		m_pRecord->rechdr.nRecordID = 0 ;
		m_pRecord->rechdr.nRecLength =
			m_pFile->GetLargeLength() - m_pRecord->qwBasePos ;
	}
	//
	return	eslErrSuccess ;
}

// ファイルを閉じる
//////////////////////////////////////////////////////////////////////////////
void EMCFile::Close( void )
{
	if ( m_pFile )
	{
		while ( m_pRecord != NULL )
		{
			if ( AscendRecord( ) )
			{
				break ;
			}
		}
	}
	while ( m_pRecord != NULL )
	{
		RECORD_INFO *	pParent = m_pRecord->pParent ;
		delete	m_pRecord ;
		m_pRecord = pParent ;
	}
	m_pFile = NULL ;
}

// EMC ファイルを複製
//////////////////////////////////////////////////////////////////////////////
ESLFileObject * EMCFile::Duplicate( void ) const
{
	EMCFile *	pDupFile = new EMCFile ;
	//
	pDupFile->SetAttribute( GetAttribute() ) ;
	pDupFile->m_pFile = m_pFile->Duplicate( ) ;
	pDupFile->m_pFile->SeekLarge( m_pFile->GetLargePosition(), FromBegin ) ;
	memcpy( &pDupFile->m_fhHeader, &m_fhHeader, sizeof(FILE_HEADER) ) ;
	//
	pDupFile->m_pRecord = NULL ;
	if ( m_pRecord != NULL )
	{
		RECORD_INFO *	pSrcRec = m_pRecord ;
		RECORD_INFO *	pDstRec = new RECORD_INFO ;
		pDupFile->m_pRecord = pDstRec ;
		//
		for ( ; ; )
		{
			pDstRec->dwWriteFlag = pSrcRec->dwWriteFlag ;
			pDstRec->qwBasePos = pSrcRec->qwBasePos ;
			pDstRec->rechdr = pSrcRec->rechdr ;
			//
			if ( pSrcRec->pParent == NULL )
			{
				pDstRec->pParent = NULL ;
				break ;
			}
			else
			{
				pDstRec->pParent = new RECORD_INFO ;
				pSrcRec = pSrcRec->pParent ;
				pDstRec = pDstRec->pParent ;
			}
		}
	}
	//
	return	pDupFile ;
}

// ファイルヘッダ初期化
//////////////////////////////////////////////////////////////////////////////
void EMCFile::SetFileHeader
	( EMCFile::FILE_HEADER & fhHeader, DWORD dwFileID, const char * pszDesc )
{
	static const char * const	pszFileDesc[] =
	{
		"ERISA-Archive file",
		"Entis Rasterized Image format",
		"EGL-3D surface attribute file",
		"EGL-3D model object file",
		"undefined EMC file"
	} ;
	static const DWORD	dwFileIDs[] =
	{
		(DWORD) fidArchive,
		(DWORD) fidRasterizedImage,
		(DWORD) fidEGL3DSurface,
		(DWORD) fidEGL3DModel,
		(DWORD) fidUndefinedEMC
	} ;
	//
	*((UINT64*)fhHeader.cHeader) = *((UINT64*)m_cDefSignature) ;
	fhHeader.dwFileID = dwFileID ;
	fhHeader.dwReserved = 0 ;
	//
	int		i ;
	if ( pszDesc == NULL )
	{
		for ( i = 0; dwFileIDs[i] != (DWORD) fidUndefinedEMC; i ++ )
		{
			if ( dwFileIDs[i] == dwFileID )
			{
				break ;
			}
		}
		pszDesc = pszFileDesc[i] ;
	}
	//
	for ( i = 0; (i < 0x29) && (pszDesc[i] != '\0'); i ++ )
	{
		fhHeader.cFormatDesc[i] = pszDesc[i] ;
	}
	while ( i < 0x30 )
	{
		fhHeader.cFormatDesc[i ++] = '\0' ;
	}
}

// デフォルトファイルシグネチャを取得
//////////////////////////////////////////////////////////////////////////////
void EMCFile::GetFileSignature( char cHeader[8] )
{
	*((UINT64*)cHeader) = *((UINT64*)m_cDefSignature) ;
}

// デフォルトファイルシグネチャを設定
//////////////////////////////////////////////////////////////////////////////
void EMCFile::SetFileSignature( const char cHeader[8] )
{
	*((UINT64*)m_cDefSignature) = *((UINT64*)cHeader) ;
}

// レコードを開く
//////////////////////////////////////////////////////////////////////////////
ESLError EMCFile::DescendRecord( const UINT64 * pRecID )
{
	if ( GetAttribute() & modeCreateFlag )
	{
		//
		// 新規にレコードを生成する
		//
		ESLAssert( pRecID != NULL ) ;
		RECORD_HEADER	rechdr ;
		rechdr.nRecordID = *pRecID ;
		rechdr.nRecLength = 0 ;
		if ( m_pFile->Write( &rechdr, sizeof(rechdr) ) < sizeof(rechdr) )
		{
            ESLTrace( "Failed to write the record header.\n" ) ;
			return	eslErrGeneral ;
		}
		//
		RECORD_INFO *	pRec = new RECORD_INFO ;
		pRec->pParent = m_pRecord ;
		pRec->dwWriteFlag = 1 ;
		pRec->qwBasePos = m_pFile->GetLargePosition( ) ;
		pRec->rechdr = rechdr ;
		m_pRecord = pRec ;
	}
	else
	{
		//
		// 既存のレコードを開く
		//
		RECORD_HEADER	rechdr ;
		for ( ; ; )
		{
			if ( Read( &rechdr, sizeof(rechdr) ) < sizeof(rechdr) ) 
			{
				return	eslErrGeneral ;
			}
/*			if ( (rechdr.nRecLength != 0xFFFFFFFFFFFFFFFF)
					&& (rechdr.nRecLength & 0xFFFFFFFF00000000) )
			{
				return	eslErrGeneral ;
			}
*/			if ( pRecID == NULL )
				break ;
			if ( *pRecID == rechdr.nRecordID )
				break ;
			//
			m_pFile->SeekLarge( rechdr.nRecLength, FromCurrent ) ;
		}
		//
		RECORD_INFO *	pRec = new RECORD_INFO ;
		pRec->pParent = m_pRecord ;
		pRec->dwWriteFlag = 0 ;
		pRec->qwBasePos = m_pFile->GetLargePosition( ) ;
		pRec->rechdr = rechdr ;
		m_pRecord = pRec ;
		//
		if ( rechdr.nRecLength == 0xFFFFFFFFFFFFFFFF )
		{
			pRec->rechdr.nRecLength
				= m_pFile->GetLargeLength() - pRec->qwBasePos ;
		}
	}
	//
	return	eslErrSuccess ;
}

// レコードを閉じる
//////////////////////////////////////////////////////////////////////////////
ESLError EMCFile::AscendRecord( void )
{
	if ( m_pRecord != NULL )
	{
		if ( m_pRecord->dwWriteFlag )
		{
			ESLAssert( GetAttribute() & modeWrite ) ;
			UINT64	nPos = GetLargePosition( ) ;
			if ( nPos > m_pRecord->rechdr.nRecLength )
			{
				m_pRecord->rechdr.nRecLength = nPos ;
			}
			m_pFile->SeekLarge
				( m_pRecord->qwBasePos - sizeof(RECORD_HEADER), FromBegin ) ;
			if ( m_pFile->Write
				( &(m_pRecord->rechdr),
					sizeof(RECORD_HEADER) ) < sizeof(RECORD_HEADER) )
			{
                ESLTrace( "Failed to write the record header.\n" ) ;
			}
		}
		//
		SeekLarge( GetLargeLength(), FromBegin ) ;
		//
		RECORD_INFO *	pRec = m_pRecord ;
		m_pRecord = m_pRecord->pParent ;
		delete	pRec ;
	}
	//
	return	eslErrSuccess ;
}

// データを読み込む
//////////////////////////////////////////////////////////////////////////////
unsigned long int EMCFile::Read
	( void * ptrBuffer, unsigned long int nBytes )
{
	ESLAssert( m_pFile != NULL ) ;
	ESLAssert( m_pRecord != NULL ) ;
	//
	INT64	nPos = m_pFile->GetLargePosition() - m_pRecord->qwBasePos ;
	if ( (UINT64) nPos + nBytes > m_pRecord->rechdr.nRecLength )
	{
		nBytes = (DWORD) (m_pRecord->rechdr.nRecLength - nPos) ;
	}
	//
	if ( (signed long int) nBytes < 0 )
	{
		nBytes = 0 ;
	}
	return	m_pFile->Read( ptrBuffer, nBytes ) ;
}

// データを書き出す
//////////////////////////////////////////////////////////////////////////////
unsigned long int EMCFile::Write
	( const void * ptrBuffer, unsigned long int nBytes )
{
	ESLAssert( m_pFile != NULL ) ;
	ESLAssert( m_pRecord != NULL ) ;
	//
	if ( !(GetAttribute() & modeCreateFlag) )
	{
		INT64	nPos = m_pFile->GetLargePosition() - m_pRecord->qwBasePos ;
		if ( (UINT64) nPos + nBytes > m_pRecord->rechdr.nRecLength )
		{
			nBytes = (DWORD) (m_pRecord->rechdr.nRecLength - nPos) ;
		}
	}
	//
	if ( (signed long int) nBytes < 0 )
	{
		nBytes = 0 ;
	}
	return	m_pFile->Write( ptrBuffer, nBytes ) ;
}

// レコードの長さを取得する
//////////////////////////////////////////////////////////////////////////////
UINT64 EMCFile::GetLargeLength( void ) const
{
	ESLAssert( m_pRecord != NULL ) ;
	return	m_pRecord->rechdr.nRecLength ;
}

unsigned long int EMCFile::GetLength( void ) const
{
	ESLAssert( m_pRecord != NULL ) ;
	return	(unsigned long int) m_pRecord->rechdr.nRecLength ;
}

// ファイルポインタを移動する
//////////////////////////////////////////////////////////////////////////////
UINT64 EMCFile::SeekLarge
	( INT64 nOffsetPos, ESLFileObject::SeekOrigin fSeekFrom )
{
	ESLAssert( m_pFile != NULL ) ;
	ESLAssert( m_pRecord != NULL ) ;
	UINT64	nPos = m_pFile->GetLargePosition() - m_pRecord->qwBasePos ;
	if ( m_pRecord->dwWriteFlag )
	{
		if ( nPos > m_pRecord->rechdr.nRecLength )
		{
			m_pRecord->rechdr.nRecLength = nPos ;
		}
	}
	//
	unsigned long int	nRecLength =
		(unsigned long int) m_pRecord->rechdr.nRecLength ;
	//
	switch ( fSeekFrom )
	{
	case	FromBegin:
	default:
		break ;
	case	FromCurrent:
		nOffsetPos += nPos ;
		break ;
	case	FromEnd:
		nOffsetPos += nRecLength ;
		break ;
	}
	//
	if ( m_pRecord->dwWriteFlag )
	{
		if ( (unsigned long int) nOffsetPos
					> m_pRecord->rechdr.nRecLength )
		{
			m_pRecord->rechdr.nRecLength = nOffsetPos ;
		}
	}
	else
	{
		if ( (unsigned long int) nOffsetPos
					> m_pRecord->rechdr.nRecLength )
		{
			nOffsetPos = (long int) m_pRecord->rechdr.nRecLength ;
		}
	}
	//
	return	m_pFile->SeekLarge
				( nOffsetPos + m_pRecord->qwBasePos, FromBegin )
											- m_pRecord->qwBasePos ;
}

unsigned long int EMCFile::Seek
	( long int nOffsetPos, ESLFileObject::SeekOrigin fSeekFrom )
{
	return	(unsigned long int) EMCFile::SeekLarge( nOffsetPos, fSeekFrom ) ;
}

// ファイルポインタを取得する
//////////////////////////////////////////////////////////////////////////////
UINT64 EMCFile::GetLargePosition( void ) const
{
	ESLAssert( m_pFile != NULL ) ;
	ESLAssert( m_pRecord != NULL ) ;
	return	m_pFile->GetLargePosition() - m_pRecord->qwBasePos ;
}

unsigned long int EMCFile::GetPosition( void ) const
{
	return	(unsigned long int) EMCFile::GetLargePosition( ) ;
}

// ファイルの終端を現在の位置に設定する
//////////////////////////////////////////////////////////////////////////////
ESLError EMCFile::SetEndOfFile( void )
{
	ESLAssert( m_pFile != NULL ) ;
	ESLAssert( m_pRecord != NULL ) ;
	ESLAssert( GetAttribute() & modeWrite ) ;
	m_pRecord->rechdr.nRecLength =
		m_pFile->GetLargePosition() - m_pRecord->qwBasePos ;
	return	m_pFile->SetEndOfFile( ) ;
}


/*****************************************************************************
                          タグ情報解析クラス
 *****************************************************************************/

//
// ETagObject 構築関数
//////////////////////////////////////////////////////////////////////////////
ERIFile::ETagObject::ETagObject( void )
{
}

//
// ETagObject 消滅関数
//////////////////////////////////////////////////////////////////////////////
ERIFile::ETagObject::~ETagObject( void )
{
}

//
// ETagInfo 構築関数
//////////////////////////////////////////////////////////////////////////////
ERIFile::ETagInfo::ETagInfo( void )
{
}

//
// ETagInfo 消滅関数
//////////////////////////////////////////////////////////////////////////////
ERIFile::ETagInfo::~ETagInfo( void )
{
}

//
// タグ情報解析
//////////////////////////////////////////////////////////////////////////////
void ERIFile::ETagInfo::CreateTagInfo( const wchar_t * pwszDesc )
{
	//
	// 現在の情報をクリア
	//
	DeleteContents( ) ;
	//
	// 文字列解析
	//
	if ( pwszDesc == NULL )
	{
		return ;
	}
	if ( pwszDesc[0] != L'#' )
	{
		ETagObject *	pTag = new ETagObject ;
		pTag->m_tag = L"comment" ;
		pTag->m_contents = pwszDesc ;
		m_lstTags.Add( pTag ) ;
		return ;
	}
	//
	while ( *pwszDesc != L'\0' )
	{
		//
		// タグ名を取得
		//
		WORD	wch ;
		ETagObject *	pTag = new ETagObject ;
		int		i = 1 ;
		while ( pwszDesc[i] != L'\0' )
		{
			wch = pwszDesc[i] ;
			if ( wch > L' ' )
				break ;
			++ i ;
		}
		//
		int		iBegin = i ;
		while ( pwszDesc[i] != L'\0' )
		{
			wch = pwszDesc[i] ;
			if ( wch <= L' ' )
				break ;
			++ i ;
		}
		//
		pTag->m_tag = EWideString( pwszDesc + iBegin, i - iBegin ) ;
		//
		// 次の行へ
		//
		while ( pwszDesc[i] != L'\0' )
		{
			wch = pwszDesc[i ++] ;
			if ( (wch == L'\r') && (pwszDesc[i] == L'\n') )
			{
				++ i ;
				break ;
			}
		}
		pwszDesc += i ;
		//
		// タグの内容を取得
		//
		while ( *pwszDesc != L'\0' )
		{
			//
			// １行取得
			//
			iBegin = 0 ;
			if ( pwszDesc[0] == L'#' )
			{
				if ( pwszDesc[1] != L'#' )
					break ;
				iBegin = 1 ;
			}
			i = iBegin ;
			while ( pwszDesc[i] != L'\0' )
			{
				wch = pwszDesc[i ++] ;
				if ( (wch == L'\r') && (pwszDesc[i] == L'\n') )
				{
					++ i ;
					break ;
				}
			}
			//
			// １行追加
			//
			pTag->m_contents += EWideString( pwszDesc + iBegin, i - iBegin ) ;
			//
			pwszDesc += i ;
		}
		//
		pTag->m_contents.TrimRight( ) ;
		//
		// タグを追加
		//
		m_lstTags.Add( pTag ) ;
	}
}

// タグ情報をフォーマット
//////////////////////////////////////////////////////////////////////////////
void ERIFile::ETagInfo::FormatDescription( EWideString & wstrDesc )
{
	wstrDesc.GetBuffer(1)[0] = (wchar_t) 0xFEFF ;
	wstrDesc.ReleaseBuffer( 1 ) ;
	//
	for ( unsigned int i = 0; i < m_lstTags.GetSize(); i ++ )
	{
		ETagObject *	pTag = m_lstTags.GetAt( i ) ;
		ESLAssert( pTag != NULL ) ;
		//
		// タグ名
		//
		wstrDesc += L'#' ;
		wstrDesc += pTag->m_tag ;
		wstrDesc += L"\r\n" ;
		//
		// 内容
		//
		int		iLine = 0 ;
		while ( iLine < (int) pTag->m_contents.GetLength() )
		{
			if ( pTag->m_contents.GetAt(iLine) == L'#' )
			{
				wstrDesc += L'#' ;
			}
			//
			int	nNextLine = pTag->m_contents.Find( L"\r\n", iLine ) ;
			if ( nNextLine < 0 )
				break ;
			//
			nNextLine += 2 ;
			wstrDesc += pTag->m_contents.Middle( iLine, nNextLine - iLine ) ;
			iLine = nNextLine ;
		}
		wstrDesc += pTag->m_contents.Middle( iLine ) ;
		if ( wstrDesc.Right(2) != L"\r\n" )
		{
			wstrDesc += L"\r\n" ;
		}
	}
}

// タグを追加する
//////////////////////////////////////////////////////////////////////////////
void ERIFile::ETagInfo::AddTag
		( TagIndex tagIndex, const wchar_t * pwszContents )
{
	ETagObject *	pTag = new ETagObject ;
	pTag->m_tag = m_pwszTagName[tagIndex] ;
	pTag->m_contents = pwszContents ;
	m_lstTags.Add( pTag ) ;
}

//
// タグ情報のクリア
//////////////////////////////////////////////////////////////////////////////
void ERIFile::ETagInfo::DeleteContents( void )
{
	m_lstTags.RemoveAll( ) ;
}

//
// タグ情報取得
//////////////////////////////////////////////////////////////////////////////
const wchar_t *
	ERIFile::ETagInfo::GetTagContents( const wchar_t * pwszTag ) const
{
	for ( unsigned int i = 0; i < m_lstTags.GetSize(); i ++ )
	{
		ETagObject *	pTag = m_lstTags.GetAt( i ) ;
		ESLAssert( pTag != NULL ) ;
		if ( pTag->m_tag == pwszTag )
		{
			return	pTag->m_contents ;
		}
	}
	return	NULL ;
}

const wchar_t * ERIFile::ETagInfo::GetTagContents( TagIndex tagIndex ) const
{
	return	GetTagContents( ERIFile::m_pwszTagName[tagIndex] ) ;
}


/*****************************************************************************
                        ERI ファイルインターフェース
 *****************************************************************************/

//
// タグ情報文字列
//////////////////////////////////////////////////////////////////////////////
const wchar_t *	ERIFile::m_pwszTagName[ERIFile::tagMax] =
{
	L"title", L"vocal-player", L"composer", L"arranger",
	L"source", L"track", L"release-date", L"genre",
	L"rewind-point", L"hot-spot", L"resolution",
	L"comment", L"words"
} ;

// クラス情報
//////////////////////////////////////////////////////////////////////////////
//IMPLEMENT_CLASS_INFO( ERIFile, EMCFile )

// 構築関数
//////////////////////////////////////////////////////////////////////////////
ERIFile::ERIFile( void )
{
	m_dwSeqLength = 0 ;
	m_pSequence = NULL ;
}

// 消滅関数
//////////////////////////////////////////////////////////////////////////////
ERIFile::~ERIFile( void )
{
	if ( m_pSequence != NULL )
	{
		::eslHeapFree( NULL, m_pSequence ) ;
	}
}

// ERI ファイルを開く
//////////////////////////////////////////////////////////////////////////////
ESLError ERIFile::Open( ESLFileObject * pFile, ERIFile::OpenType type )
{
	//
	// 読み込みフラグをクリア
	//////////////////////////////////////////////////////////////////////////
	m_fdwReadMask = 0 ;
	//
	m_dwSeqLength = 0 ;
	if ( m_pSequence != NULL )
	{
		::eslHeapFree( NULL, m_pSequence ) ;
		m_pSequence = NULL ;
	}
	//
	// EMCヘッダを読み込む
	//////////////////////////////////////////////////////////////////////////
	ESLError	err ;
	err = EMCFile::Open( pFile, NULL ) ;
	if ( err )
		return	err ;
	if ( type == otOpenRoot )
		return	eslErrSuccess ;
	//
	// 情報ヘッダレコードを読み込む
	//////////////////////////////////////////////////////////////////////////
	typedef	const UINT64 *	PCUINT64 ;
	err = DescendRecord( (PCUINT64)"Header  " ) ;
	if ( err )
		return	err ;
	//
	// ファイルヘッダを読み込む
	err = DescendRecord( (PCUINT64)"FileHdr " ) ;
	if ( err )
		return	err ;
	if ( Read( &m_FileHeader,
			sizeof(ERI_FILE_HEADER) ) < sizeof(ERI_FILE_HEADER) )
	{
		return	ESLErrorMsg( "ERI ファイルヘッダを読み込めませんでした。" ) ;
	}
	AscendRecord( ) ;
	//
	m_fdwReadMask |= rmFileHeader ;
	//
	// バージョン情報のチェック
	if ( m_FileHeader.dwVersion > 0x00020100 )
	{
		return	ESLErrorMsg( "ERI ファイルバージョンが不正です。" ) ;
	}
	//
	// 画像情報ヘッダを読み込む
	for ( ; ; )
	{
		if ( DescendRecord( ) )
			break ;
		//
		UINT64	ui64RecID = GetRecordID( ) ;
		if ( ui64RecID == *((PCUINT64)"PrevwInf") )
		{
			//
			// プレビュー画像情報ヘッダ
			if ( Read( &m_PreviewInfo,
					sizeof(ERI_INFO_HEADER) ) == sizeof(ERI_INFO_HEADER) )
			{
				m_fdwReadMask |= rmPreviewInfo ;
			}
		}
		else if ( ui64RecID == *((PCUINT64)"ImageInf") )
		{
			//
			// 画像情報ヘッダ
			if ( Read( &m_InfoHeader,
					sizeof(ERI_INFO_HEADER) ) == sizeof(ERI_INFO_HEADER) )
			{
				m_fdwReadMask |= rmImageInfo ;
			}
		}
		else if ( ui64RecID == *((PCUINT64)"SoundInf") )
		{
			//
			// 音声情報ヘッダ
			if ( Read( &m_MIOInfHdr,
					sizeof(MIO_INFO_HEADER) ) == sizeof(MIO_INFO_HEADER) )
			{
				m_fdwReadMask |= rmSoundInfo ;
			}
		}
		else if ( ui64RecID == *((PCUINT64)"Sequence") )
		{
			//
			// シーケンステーブルレコード
			DWORD	dwBytes ;
			m_dwSeqLength = GetLength() / sizeof(SEQUENCE_DELTA) ;
			dwBytes = m_dwSeqLength * sizeof(SEQUENCE_DELTA) ;
			m_pSequence =
				(SEQUENCE_DELTA*) ::eslHeapAllocate( NULL, dwBytes, 0 ) ;
			//
			if ( Read( m_pSequence, dwBytes ) == dwBytes )
			{
				m_fdwReadMask |= rmSequenceTable ;
			}
		}
		else
		{
			//
			// 著作権情報・コメントなど
			int	nType = -1 ;
			if ( ui64RecID == *((PCUINT64)"cpyright") )
			{
				nType = 0 ;
				m_fdwReadMask |= rmCopyright ;
			}
			else if ( ui64RecID == *((PCUINT64)"descript") )
			{
				nType = 1 ;
				m_fdwReadMask |= rmDescription ;
			}
			if ( nType >= 0 )
			{
				int		nLength = GetLength( ) ;
				EStreamBuffer	bufDesc ;
				char *	pszBuf = (char*) bufDesc.PutBuffer( nLength + 2 ) ;
				Read( pszBuf, nLength ) ;
				//
				if ( (nLength >= 2) &&
					(pszBuf[0] == '\xff') && (pszBuf[1] == '\xfe') )
				{
					wchar_t *	pwszBuf = (wchar_t *) (pszBuf + 2) ;
					int		nWStrLen = (nLength - 2) / 2 ;
					pwszBuf[nWStrLen] = L'\0' ;
				}
				else
				{
					pszBuf[nLength] = '\0' ;
				}
			}
		}
		//
		// 次のレコードへ
		AscendRecord( ) ;
	}
	//
	AscendRecord( ) ;
	//
	// 圧縮オプションのチェック
	if ( !(m_fdwReadMask & rmImageInfo) && !(m_fdwReadMask & rmSoundInfo) )
	{
		return	ESLErrorMsg( "画像情報ヘッダが見つかりませんでした。" ) ;
	}
	if ( type == otReadHeader )
	{
		return	eslErrSuccess ;
	}
	//
	// ストリームレコードを開く
	//////////////////////////////////////////////////////////////////////////
	err = DescendRecord( (PCUINT64)"Stream  " ) ;
	if ( err )
		return	err ;
	if ( type == otOpenStream )
		return	eslErrSuccess ;
	//
	// 画像データレコードを捜索
	for ( ; ; )
	{
		err = DescendRecord( ) ;
		if ( err )
		{
			return	err ;
		}
		UINT64	nRecID = GetRecordID( ) ;
		if ( nRecID == *((PCUINT64)"ImageFrm") )
		{
			break ;
		}
		if ( nRecID == *((PCUINT64)"Palette ") )
		{
			//
			// パレットテーブル読み込み
			::eslFillMemory( m_PaletteTable, 0, sizeof(m_PaletteTable) ) ;
			Read( m_PaletteTable, sizeof(m_PaletteTable) ) ;
			//
			m_fdwReadMask |= rmPaletteTable ;
		}
		AscendRecord( ) ;
	}

	return	eslErrSuccess ;
}

// シーケンステーブルを取得する
//////////////////////////////////////////////////////////////////////////////
const ERIFile::SEQUENCE_DELTA *
	 ERIFile::GetSequenceTable( DWORD * pdwLength ) const
{
	if ( pdwLength != NULL )
	{
		*pdwLength = m_dwSeqLength ;
	}
	return	m_pSequence ;
}


