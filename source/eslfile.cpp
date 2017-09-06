
/*****************************************************************************
                   Entis Standard Library implementations
                                                    last update 2003/04/17
 ----------------------------------------------------------------------------

	In this file, the file object classes source codes.

	Copyright (C) 1998-2003 Leshade Entis.  All rights reserved.

 ****************************************************************************/


// Include esl.h
//////////////////////////////////////////////////////////////////////////////

#include	"xerisa.h"
#include	"esl.h"


/*****************************************************************************
                           ファイル抽象クラス
 ****************************************************************************/

// 構築関数
//////////////////////////////////////////////////////////////////////////////
ESLFileObject::ESLFileObject( void )
	: m_nAttribute( 0 )
{
    //::InitializeCriticalSection( &m_cs ) ;
}

// 消滅関数
//////////////////////////////////////////////////////////////////////////////
ESLFileObject::~ESLFileObject( void )
{
    //::DeleteCriticalSection( &m_cs ) ;
}

// スレッド排他アクセス
//////////////////////////////////////////////////////////////////////////////
void ESLFileObject::Lock( void ) const
{
    //::EnterCriticalSection( (CRITICAL_SECTION*) &m_cs ) ;
}

void ESLFileObject::Unlock( void ) const
{
    //::LeaveCriticalSection( (CRITICAL_SECTION*) &m_cs ) ;
}

// ファイルの終端を現在の位置に設定する
//////////////////////////////////////////////////////////////////////////////
ESLError ESLFileObject::SetEndOfFile( void )
{
	return	eslErrGeneral ;
}

// ファイルの長さを取得
//////////////////////////////////////////////////////////////////////////////
UINT64 ESLFileObject::GetLargeLength( void ) const
{
	return	GetLength( ) ;
}

// ファイルポインタを移動
//////////////////////////////////////////////////////////////////////////////
UINT64 ESLFileObject::SeekLarge
	( INT64 nOffsetPos, ESLFileObject::SeekOrigin fSeekFrom )
{
	return	Seek( (long int) nOffsetPos, fSeekFrom ) ;
}

// ファイルポインタを取得
//////////////////////////////////////////////////////////////////////////////
UINT64 ESLFileObject::GetLargePosition( void ) const
{
	return	GetPosition( ) ;
}


/*****************************************************************************
                           メモリファイルクラス
 ****************************************************************************/

// 構築関数
//////////////////////////////////////////////////////////////////////////////
EMemoryFile::EMemoryFile( void )
	: m_ptrMemory( NULL )
{
}

// 消滅関数
//////////////////////////////////////////////////////////////////////////////
EMemoryFile::~EMemoryFile( void )
{
	Delete( ) ;
}

// 読み書き可能なメモリファイルを作成する
//////////////////////////////////////////////////////////////////////////////
ESLError EMemoryFile::Create( unsigned long int nLength )
{
	//
	// 現在のメモリファイルを解放する
	//
	Delete( ) ;
	//
	// メモリを確保する
	//
	Lock( ) ;
	m_nLength = 0 ;
	m_nPosition = 0 ;
	m_nBufferSize = nLength ;
	m_ptrMemory = ::eslHeapAllocate( NULL, m_nBufferSize, 0 ) ;
	m_nAttribute = (modeRead | modeCreate) ;
	Unlock( ) ;

	return	eslErrSuccess ;
}

// 読み込み専用のメモリファイルオブジェクトを作成する
//////////////////////////////////////////////////////////////////////////////
ESLError EMemoryFile::Open
	( const void * ptrMemory, unsigned long int nLength )
{
	//
	// 現在のメモリファイルを解放する
	//
	Delete( ) ;
	//
	// メモリを関連付ける
	//
	ESLAssert( ptrMemory != NULL ) ;
	Lock( ) ;
	m_nBufferSize = m_nLength = nLength ;
	m_nPosition = 0 ;
	m_ptrMemory = (void*) ptrMemory ;
	m_nAttribute = modeRead ;
	Unlock( ) ;

	return	eslErrSuccess ;
}

// メモリファイルを解放する
//////////////////////////////////////////////////////////////////////////////
void EMemoryFile::Delete( void )
{
	Lock( ) ;
	if( m_ptrMemory != NULL )
	{
		if( m_nAttribute & modeWrite )
		{
			::eslHeapFree( NULL, m_ptrMemory ) ;
		}
		m_ptrMemory = NULL ;
		m_nAttribute = 0 ;
	}
	Unlock( ) ;
}

// メモリファイルを複製する
//////////////////////////////////////////////////////////////////////////////
ESLFileObject * EMemoryFile::Duplicate( void ) const
{
	ESLAssert( m_ptrMemory != NULL ) ;
	Lock( ) ;
	EMemoryFile *	pMemFile = new EMemoryFile ;
	pMemFile->Create( m_nLength ) ;
	pMemFile->Write( m_ptrMemory, m_nLength ) ;
	pMemFile->Seek( 0, FromBegin ) ;
	Unlock( ) ;
	return	pMemFile ;
}

// メモリファイルからデータを転送する
//////////////////////////////////////////////////////////////////////////////
unsigned long int EMemoryFile::Read
	( void * ptrBuffer, unsigned long int nBytes )
{
	ESLAssert( m_ptrMemory != NULL ) ;
	ESLAssert( m_nAttribute & modeRead ) ;
	Lock( ) ;
	unsigned long int	nReadBytes = nBytes ;
	if ( (nReadBytes + m_nPosition) > m_nLength )
	{
		nReadBytes = m_nLength - m_nPosition ;
	}
	::eslMoveMemory
		( ptrBuffer, (((BYTE*)m_ptrMemory) + m_nPosition), nReadBytes ) ;
	m_nPosition += nReadBytes ;
	Unlock( ) ;
	return	nReadBytes ;
}

// メモリファイルにデータを転送する
//////////////////////////////////////////////////////////////////////////////
unsigned long int EMemoryFile::Write
	( const void * ptrBuffer, unsigned long int nBytes )
{
	ESLAssert( m_ptrMemory != NULL ) ;
	ESLAssert( m_nAttribute & modeWrite ) ;
	Lock( ) ;
	unsigned long int	nWrittenBytes = nBytes ;
	if ( nWrittenBytes + m_nPosition > m_nLength )
	{
		m_nLength = nWrittenBytes + m_nPosition ;
		if ( m_nLength > m_nBufferSize )
		{
			m_nBufferSize =
				(m_nLength + nWrittenBytes + 0xFFF) & (~0xFFF) ;
			m_ptrMemory = ::eslHeapReallocate
				( NULL, m_ptrMemory, m_nBufferSize, 0 ) ;
		}
	}
	::eslMoveMemory
		( (((BYTE*)m_ptrMemory) + m_nPosition), ptrBuffer, nWrittenBytes ) ;
	m_nPosition += nWrittenBytes ;
	Unlock( ) ;
	return	nWrittenBytes ;
}

// メモリファイルの長さを取得する
//////////////////////////////////////////////////////////////////////////////
unsigned long int EMemoryFile::GetLength( void ) const
{
	ESLAssert( m_ptrMemory != NULL ) ;
	return	m_nLength ;
}

// メモリファイルのポインタを移動する
//////////////////////////////////////////////////////////////////////////////
unsigned long int EMemoryFile::Seek
	( long int nOffsetPos, SeekOrigin fSeekFrom )
{
	ESLAssert( m_ptrMemory != NULL ) ;
	Lock( ) ;
	switch ( fSeekFrom )
	{
	case	FromBegin:
		m_nPosition = nOffsetPos ;
		break ;
	case	FromCurrent:
		m_nPosition += nOffsetPos ;
		break ;
	case	FromEnd:
		m_nPosition = m_nLength + nOffsetPos ;
		break ;
	default:
		ESLAssert( false ) ;
		break ;
	}
	if ( (signed long int) m_nPosition < 0 )
	{
		m_nPosition = 0 ;
	}
	else if ( m_nPosition > m_nLength )
	{
		if ( m_nAttribute & modeWrite )
		{
			m_nLength = m_nPosition ;
		}
	}
	Unlock( ) ;
	return	m_nPosition ;
}

// メモリファイルポインタを取得する
//////////////////////////////////////////////////////////////////////////////
unsigned long int EMemoryFile::GetPosition( void ) const
{
	ESLAssert( m_ptrMemory != NULL ) ;
	return	m_nPosition ;
}

// ファイルの終端を現在の位置に設定する
//////////////////////////////////////////////////////////////////////////////
ESLError EMemoryFile::SetEndOfFile( void )
{
	ESLAssert( m_ptrMemory != NULL ) ;
	ESLAssert( m_nAttribute & modeWrite ) ;
	m_nLength = m_nPosition ;
	return	eslErrSuccess ;
}


/*****************************************************************************
                 ストリーミングバッファファイルクラス
 ****************************************************************************/
// 構築関数
//////////////////////////////////////////////////////////////////////////////
EStreamFileBuffer::EStreamFileBuffer( void )
{
	SetAttribute( modeReadWrite ) ;
}

// 消滅関数
//////////////////////////////////////////////////////////////////////////////
EStreamFileBuffer::~EStreamFileBuffer( void )
{
}

// ファイルオブジェクトを複製する
//////////////////////////////////////////////////////////////////////////////
ESLFileObject * EStreamFileBuffer::Duplicate( void ) const
{
	return	new EStreamFileBuffer ;
}

// ファイルから読み込む
//////////////////////////////////////////////////////////////////////////////
unsigned long int EStreamFileBuffer::Read
	( void * ptrBuffer, unsigned long int nBytes )
{
	return	EStreamBuffer::Read( ptrBuffer, nBytes ) ;
}

// ファイルへ書き出す
//////////////////////////////////////////////////////////////////////////////
unsigned long int EStreamFileBuffer::Write
	( const void * ptrBuffer, unsigned long int nBytes )
{
	return	EStreamBuffer::Write( ptrBuffer, nBytes ) ;
}

// ファイルの長さを取得
//////////////////////////////////////////////////////////////////////////////
unsigned long int EStreamFileBuffer::GetLength( void ) const
{
	return	EStreamBuffer::GetLength( ) ;
}

// ファイルポインタを移動
//////////////////////////////////////////////////////////////////////////////
unsigned long int EStreamFileBuffer::Seek
	( long int nOffsetPos, SeekOrigin fSeekFrom )
{
	if ( fSeekFrom == FromEnd )
	{
		nOffsetPos += EStreamBuffer::GetLength( ) ;
	}
	EStreamBuffer::GetBuffer( nOffsetPos ) ;
	EStreamBuffer::Release( nOffsetPos ) ;
	return	0 ;
}

// ファイルポインタを取得
//////////////////////////////////////////////////////////////////////////////
unsigned long int EStreamFileBuffer::GetPosition( void ) const
{
	return	0 ;
}

