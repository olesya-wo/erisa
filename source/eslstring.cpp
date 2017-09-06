
/*****************************************************************************
                   Entis Standard Library declarations
 ----------------------------------------------------------------------------
          Copyright (c) 1998-2003 Leshade Entis. All rights reserved.
 ****************************************************************************/


// Include esl.h
//////////////////////////////////////////////////////////////////////////////

#include	"xerisa.h"
#include	"esl.h"

/*****************************************************************************
                    ストリーミングバッファクラス
 ****************************************************************************/

// 消滅関数
//////////////////////////////////////////////////////////////////////////////
EStreamBuffer::EBuffer::~EBuffer( void )
{
	if ( m_ptrBuf != NULL )
	{
		::eslHeapFree( NULL, m_ptrBuf ) ;
	}
}

// バッファ取得
//////////////////////////////////////////////////////////////////////////////
EPtrBuffer EStreamBuffer::EBuffer::GetBuffer( unsigned int nLength )
{
	if ( nLength > m_nLength )
	{
		nLength = m_nLength ;
	}
	return	EPtrBuffer( m_ptrBuf + m_nBeginPos, nLength ) ;
}

// バッファ解放
//////////////////////////////////////////////////////////////////////////////
void EStreamBuffer::EBuffer::Release( unsigned int nLength )
{
	if ( nLength > m_nLength )
	{
		nLength = m_nLength ;
	}
	m_nBeginPos += nLength ;
	m_nLength -= nLength ;
}

// 出力のためにバッファ確保
//////////////////////////////////////////////////////////////////////////////
void * EStreamBuffer::EBuffer::PutBuffer( unsigned int nLength )
{
	if ( (m_nLength + nLength) > m_nBufLimit )
	{
		//
		// メモリを再確保
		//
		m_nBufLimit = ((m_nLength + nLength + 0xFFF) & (~0xFFF)) ;
		BYTE *	ptrBuf =
			(BYTE*) ::eslHeapAllocate( NULL, m_nBufLimit, 0 ) ;
		if ( m_ptrBuf != NULL )
		{
			::eslMoveMemory( ptrBuf, m_ptrBuf + m_nBeginPos, m_nLength ) ;
			::eslHeapFree( NULL, m_ptrBuf ) ;
		}
		m_ptrBuf = ptrBuf ;
		m_nBeginPos = 0 ;
	}
	else
	{
		//
		// メモリの再確保無しにバッファを確保
		//
		if ( (m_nBeginPos > 0) && (m_nLength > 0) )
		{
			::eslMoveMemory( m_ptrBuf, m_ptrBuf + m_nBeginPos, m_nLength ) ;
		}
		m_nBeginPos = 0 ;
	}
	//
	return	(m_ptrBuf + m_nBeginPos + m_nLength) ;
}

// メモリ確定
//////////////////////////////////////////////////////////////////////////////
void EStreamBuffer::EBuffer::Flush( unsigned int nLength )
{
	if ( m_nBeginPos + m_nLength + nLength > m_nBufLimit )
	{
		nLength = m_nBufLimit - (m_nBeginPos + m_nLength) ;
	}
	m_nLength += nLength ;
}

// 構築関数
//////////////////////////////////////////////////////////////////////////////
EStreamBuffer::EStreamBuffer( void )
	: m_pFirstBuf( NULL ), m_pLastBuf( NULL ), m_nLength( 0 )
{
}

EStreamBuffer::EStreamBuffer( const void * ptrBuffer, unsigned int nLength )
	: m_pFirstBuf( NULL ), m_pLastBuf( NULL ), m_nLength( 0 )
{
	Write( ptrBuffer, nLength ) ;
}

EStreamBuffer::EStreamBuffer( const EPtrBuffer & ptrbuf )
	: m_pFirstBuf( NULL ), m_pLastBuf( NULL ), m_nLength( 0 )
{
	Write( ptrbuf, ptrbuf.GetLength() ) ;
}

// 消滅関数
//////////////////////////////////////////////////////////////////////////////
EStreamBuffer::~EStreamBuffer( void )
{
	Delete( ) ;
}

// リソースを解放
//////////////////////////////////////////////////////////////////////////////
void EStreamBuffer::Delete( void )
{
	EBuffer *	pLastBuf = m_pFirstBuf ;
	while ( pLastBuf != NULL )
	{
		EBuffer *	pNextBuf = pLastBuf->GetNextBuffer( ) ;
		delete	pLastBuf ;
		pLastBuf = pNextBuf ;
	}
	m_pFirstBuf = NULL ;
	m_pLastBuf = NULL ;
	m_nLength = 0 ;
}

// バッファの長さを取得
//////////////////////////////////////////////////////////////////////////////
unsigned int EStreamBuffer::GetLength( void ) const
{
	return	m_nLength ;
}

// バッファから取り出すためのメモリを確保
//////////////////////////////////////////////////////////////////////////////
EPtrBuffer EStreamBuffer::GetBuffer( unsigned int nLength )
{
	//
	// 取り出すバッファの長さを計算
	//
	if ( m_pFirstBuf == NULL )
	{
		return	EPtrBuffer( NULL, 0 ) ;
	}
	if ( (nLength == 0) || (nLength > m_nLength) )
	{
		nLength = m_nLength ;
	}
	//
	// 先頭のバッファから取り出す
	//
	if ( m_pFirstBuf->GetLength() >= nLength )
	{
		return	m_pFirstBuf->GetBuffer( nLength ) ;
	}
	//
	// バッファを結合する
	//
	EBuffer *	pBuf = new EBuffer ;
	void *	ptrDst = pBuf->PutBuffer( nLength ) ;
	unsigned int	nSize = Read( ptrDst, nLength ) ;
	m_nLength += nSize ;
	pBuf->Flush( nSize ) ;
	pBuf->SetNextBuffer( m_pFirstBuf ) ;
	m_pFirstBuf = pBuf ;
	if ( m_pLastBuf == NULL )
	{
		m_pLastBuf = pBuf ;
	}
	//
	return	m_pFirstBuf->GetBuffer( nLength ) ;
}

// GetBuffer によってロックされたメモリを解放
//////////////////////////////////////////////////////////////////////////////
void EStreamBuffer::Release( unsigned int nLength )
{
	if ( nLength != 0 )
	{
		ESLAssert( m_pFirstBuf != NULL ) ;
		m_pFirstBuf->Release( nLength ) ;
		m_nLength -= nLength ;
	}
}

// バッファからデータを読み出す
//////////////////////////////////////////////////////////////////////////////
unsigned int EStreamBuffer::Read( void * ptrBuffer, unsigned int nLength )
{
	BYTE *	ptrDst = (BYTE*) ptrBuffer ;
	unsigned int	i = 0 ;
	while ( i < nLength )
	{
		if ( m_pFirstBuf == NULL )
		{
			break ;
		}
		EPtrBuffer	ptrbuf =
			m_pFirstBuf->GetBuffer( m_pFirstBuf->GetLength() ) ;
		unsigned int	nCopyLen = ptrbuf.GetLength( ) ;
		if ( i + nCopyLen > nLength )
		{
			nCopyLen = nLength - i ;
		}
		::eslMoveMemory( ptrDst + i, ptrbuf, nCopyLen ) ;
		m_pFirstBuf->Release( nCopyLen ) ;
		m_nLength -= nCopyLen ;
		i += nCopyLen ;
		//
		if ( m_pFirstBuf->GetLength() == 0 )
		{
			EBuffer *	pNext = m_pFirstBuf->GetNextBuffer( ) ;
			delete	m_pFirstBuf ;
			m_pFirstBuf = pNext ;
			if ( pNext == NULL )
			{
				m_pLastBuf = NULL ;
			}
		}
	}
	return	i ;
}

// バッファへ書き出すためのメモリを確保
//////////////////////////////////////////////////////////////////////////////
void * EStreamBuffer::PutBuffer( unsigned int nLength )
{
	if ( m_pLastBuf != NULL )
	{
		if ( m_pLastBuf->GetMaxWritableLength() >= nLength )
		{
			return	m_pLastBuf->PutBuffer( nLength ) ;
		}
	}
	EBuffer *	pBuf = new EBuffer ;
	void *	ptrPut = pBuf->PutBuffer( nLength ) ;
	if ( m_pLastBuf == NULL )
	{
		ESLAssert( m_pFirstBuf == NULL ) ;
		m_pFirstBuf = m_pLastBuf = pBuf ;
	}
	else
	{
		m_pLastBuf->SetNextBuffer( pBuf ) ;
		m_pLastBuf = pBuf ;
	}
	return	ptrPut ;
}

// PutBuffer によってロックされたメモリを確定
//////////////////////////////////////////////////////////////////////////////
void EStreamBuffer::Flush( unsigned int nLength )
{
	ESLAssert( m_pLastBuf != NULL ) ;
	m_pLastBuf->Flush( nLength ) ;
	m_nLength += nLength ;
}

// バッファへデータを書き込む
//////////////////////////////////////////////////////////////////////////////
unsigned int EStreamBuffer::Write
	( const void * ptrBuffer, unsigned int nLength )
{
	::eslMoveMemory( PutBuffer( nLength ), ptrBuffer, nLength ) ;
	Flush( nLength ) ;
	return	nLength ;
}

// バッファの一部を参照
//////////////////////////////////////////////////////////////////////////////
void * EStreamBuffer::ModifyBuffer
	( unsigned int nPosition, unsigned int nLength )
{
	//
	// バッファの位置を検索
	//
	EBuffer *	pBuffer = m_pFirstBuf ;
	while ( pBuffer != NULL )
	{
		if ( nPosition < pBuffer->GetLength() )
			break ;
		nPosition -= pBuffer->GetLength( ) ;
		pBuffer = pBuffer->GetNextBuffer( ) ;
	}
	if ( pBuffer == NULL )
	{
		return	NULL ;
	}
	//
	// バッファの結合
	//
	int	nFixed = 0 ;
	int	nLeftBytes = nPosition + nLength - pBuffer->GetLength() ;
	if ( nLeftBytes > 0 )
	{
		BYTE *	pbytBuf = (BYTE*) pBuffer->PutBuffer( nLeftBytes ) ;
		while ( nFixed < nLeftBytes )
		{
			EBuffer *	pNextBuf = pBuffer->GetNextBuffer( ) ;
			if ( pNextBuf == NULL )
			{
				break ;
			}
			int	nBufLen = pNextBuf->GetLength( ) ;
			if ( nBufLen > (nLeftBytes - nFixed) )
			{
				nBufLen = nLeftBytes - nFixed ;
			}
			//
			EPtrBuffer	ptrbuf = pNextBuf->GetBuffer( nBufLen ) ;
			::eslMoveMemory( pbytBuf, ptrbuf.GetBuffer(), ptrbuf.GetLength() ) ;
			pbytBuf += ptrbuf.GetLength( ) ;
			nFixed += ptrbuf.GetLength( ) ;
			pNextBuf->Release( nBufLen ) ;
			//
			if ( pNextBuf->GetLength() == 0 )
			{
				pBuffer->SetNextBuffer( pNextBuf->GetNextBuffer() ) ;
				delete	pNextBuf ;
				if ( pBuffer->GetNextBuffer() == NULL )
				{
					m_pLastBuf = pBuffer ;
				}
			}
		}
		pBuffer->Flush( nFixed ) ;
	}
	//
	if ( nPosition + nLength <= pBuffer->GetLength() )
	{
		return	pBuffer->ModifyBuffer( nPosition ) ;
	}
	return	NULL ;
}

// ファイルの終端まで読み込む
//////////////////////////////////////////////////////////////////////////////
ESLError EStreamBuffer::ReadFromFile( ESLFileObject & file )
{
	const DWORD	dwBlockSize = 0x2000 ;
	DWORD	dwReadBytes ;
	do
	{
		void *	pbuf = PutBuffer( dwBlockSize ) ;
		dwReadBytes = file.Read( pbuf, dwBlockSize ) ;
		Flush( dwReadBytes ) ;
	}
	while ( dwReadBytes == dwBlockSize ) ;
	return	eslErrSuccess ;
}

// バッファの内容をファイルに書き出す
//////////////////////////////////////////////////////////////////////////////
ESLError EStreamBuffer::WriteToFile( ESLFileObject & file )
{
	const DWORD	dwBlockSize = 0x2000 ;
	for ( ; ; )
	{
		EPtrBuffer	ptrbuf = GetBuffer( dwBlockSize ) ;
		if ( ptrbuf.GetLength() == 0 )
		{
			break ;
		}
		if ( file.Write
			( ptrbuf, ptrbuf.GetLength() ) < ptrbuf.GetLength() )
		{
			return	eslErrGeneral ;
		}
		Release( ptrbuf.GetLength() ) ;
	}
	return	eslErrSuccess ;
}

