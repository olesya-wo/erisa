#define	STRICT	1
#include "xerisa.h"


// EPreloadBuffer конструктор
//////////////////////////////////////////////////////////////////////////////
MIODynamicPlayer::EPreloadBuffer::EPreloadBuffer( DWORD dwLength )
{
	m_ptrBuffer = (BYTE*) ::eslHeapAllocate( NULL, dwLength, 0 ) ;
	m_nKeySample = 0 ;
	EMemoryFile::Open( m_ptrBuffer, dwLength ) ;
}
// EPreloadBuffer деструктор
//////////////////////////////////////////////////////////////////////////////
MIODynamicPlayer::EPreloadBuffer::~EPreloadBuffer( void )
{
	::eslHeapFree( NULL, m_ptrBuffer ) ;
}
// Конструктор
//////////////////////////////////////////////////////////////////////////////
MIODynamicPlayer::MIODynamicPlayer( void )
{
	m_pmioc = NULL ;
	m_pmiod = NULL ;
	m_hQueueFull = NULL ;
	m_hQueueNotEmpty = NULL ;
	m_hQueueSpace = NULL ;
    //::InitializeCriticalSection( &m_cs ) ;
}
// Деструктор
//////////////////////////////////////////////////////////////////////////////
MIODynamicPlayer::~MIODynamicPlayer( void )
{
	Close( ) ;
    //::DeleteCriticalSection( &m_cs ) ;
}


// MIO открытие
//////////////////////////////////////////////////////////////////////////////
ESLError MIODynamicPlayer::Open(ESLFileObject * pFile)
{
	Close( ) ;
	//
	// ERIファイルを開く（ストリームレコードまで開く）
	//////////////////////////////////////////////////////////////////////////
	if ( m_erif.Open( pFile, m_erif.otOpenStream ) )
	{
        ESLTrace( "[MIODynamicPlayer::Open] Can't open file!\n" ) ;
		return	ESLErrorMsg( "ERI ファイルを開けませんでした。" ) ;
	}
	//
	// 展開オブジェクトを初期化する
	//////////////////////////////////////////////////////////////////////////
	if ( !(m_erif.m_fdwReadMask & ERIFile::rmSoundInfo) )
	{
        ESLTrace( "[MIODynamicPlayer::Open] Sound not found!\n" ) ;
		return	ESLErrorMsg( "ファイルに音声情報が含まれていません。" ) ;
	}
	m_pmiod = CreateMIODecoder( ) ;
	if ( m_pmiod->Initialize( m_erif.m_MIOInfHdr ) )
	{
        ESLTrace( "[MIODynamicPlayer::Open] MIO decoder initialisation fail!\n" ) ;
		return	ESLErrorMsg( "MIO デコーダの初期化に失敗しました。" ) ;
	}
	m_pmioc = new ERISADecodeContext( 0x10000 ) ;
	//
	// 先読みバッファ配列を確保
	//////////////////////////////////////////////////////////////////////////
    m_queueSound.SetLimit( 2 ) ;
	//
	// スレッドを生成
	//////////////////////////////////////////////////////////////////////////
	//
    m_nCurrentSample = 0 ;
    EPreloadBuffer *	pBuffer =
        LoadSoundStream( m_nCurrentSample ) ;
    if ( pBuffer != NULL )
    {
        AddPreloadBuffer( pBuffer ) ;
        m_nCurrentSample += pBuffer->m_miodh.dwSampleCount ;
    }

	return	eslErrSuccess ;
}
// MIO закрытие
//////////////////////////////////////////////////////////////////////////////
void MIODynamicPlayer::Close( void )
{
	m_queueSound.RemoveAll( ) ;
	//
	// キーポイント配列をクリアする
	m_listKeySample.RemoveAll( ) ;
	//
	// 展開オブジェクトを削除する
	if ( m_pmiod != NULL )
	{
		m_pmiod->Delete( ) ;
		delete	m_pmiod ;
		m_pmiod = NULL ;
	}
	if ( m_pmioc != NULL )
	{
		delete	m_pmioc ;
		m_pmioc = NULL ;
	}
	//
	// ファイルを閉じる
	m_erif.Close( ) ;
}


// Перейти к указанному образцу и получить данные первого блока
//////////////////////////////////////////////////////////////////////////////
void * MIODynamicPlayer::GetWaveBufferFrom( ULONG nSample, DWORD & dwBytes, DWORD & dwOffsetBytes )
{
    SeekKeySample( nSample, m_nCurrentSample ) ;
	//
	// 先頭のデータを取得して展開する
	//////////////////////////////////////////////////////////////////////////
	EPreloadBuffer *	pBuffer = GetPreloadBuffer( ) ;
	if ( pBuffer == NULL )
	{
		return	NULL ;
	}
	if ( (nSample < pBuffer->m_nKeySample) ||
		(nSample >= pBuffer->m_nKeySample + pBuffer->m_miodh.dwSampleCount) )
	{
		return	NULL ;
	}
	DWORD	dwSampleBytes = GetChannelCount() * GetBitsPerSample() / 8 ;
	dwBytes = pBuffer->m_miodh.dwSampleCount * dwSampleBytes ;
	dwOffsetBytes = (nSample - pBuffer->m_nKeySample) * dwSampleBytes ;
	//
	void *	ptrWaveBuf = AllocateWaveBuffer( dwBytes ) ;
	//
	m_pmioc->AttachInputFile( pBuffer ) ;
	//
	if ( m_pmiod->DecodeSound( *m_pmioc, pBuffer->m_miodh, ptrWaveBuf ) )
	{
		DeleteWaveBuffer( ptrWaveBuf ) ;
		delete	pBuffer ;
		return	NULL ;
	}
	delete	pBuffer ;

	return	ptrWaveBuf ;
}


// Являются ли следующие аудиоданные началом потока?
//////////////////////////////////////////////////////////////////////////////
bool MIODynamicPlayer::IsNextDataRewound( void )
{
    if ( m_queueSound.GetSize() == 0 )
	{
		EPreloadBuffer *	pBuffer =
			LoadSoundStream( m_nCurrentSample ) ;
		if ( pBuffer != NULL )
		{
			AddPreloadBuffer( pBuffer ) ;
			m_nCurrentSample += pBuffer->m_miodh.dwSampleCount ;
		}
	}
	bool	fRewound = false ;
	Lock( ) ;
	if ( m_queueSound.GetSize() != 0 )
	{
		EPreloadBuffer *	pBuffer = m_queueSound.GetAt( 0 ) ;
		if ( (pBuffer != NULL) && (pBuffer->m_nKeySample == 0) )
		{
			fRewound = true ;
		}
	}
	Unlock( ) ;
	//
	return	fRewound ;
}


// Получить следующие данные
//////////////////////////////////////////////////////////////////////////////
void * MIODynamicPlayer::GetNextWaveBuffer( DWORD & dwBytes )
{
	EPreloadBuffer *	pBuffer = GetPreloadBuffer( ) ;
	if ( pBuffer == NULL )
	{
		return	NULL ;
	}
	DWORD	dwSampleBytes = GetChannelCount() * GetBitsPerSample() / 8 ;
	dwBytes = pBuffer->m_miodh.dwSampleCount * dwSampleBytes ;
	//
	void *	ptrWaveBuf = AllocateWaveBuffer( dwBytes ) ;
	//
	m_pmioc->AttachInputFile( pBuffer ) ;
	//
	if ( m_pmiod->DecodeSound( *m_pmioc, pBuffer->m_miodh, ptrWaveBuf ) )
	{
		DeleteWaveBuffer( ptrWaveBuf ) ;
		delete	pBuffer ;
		return	NULL ;
	}
	delete	pBuffer ;

	return	ptrWaveBuf ;
}


// Выделение аудио-буфера
//////////////////////////////////////////////////////////////////////////////
void * MIODynamicPlayer::AllocateWaveBuffer( DWORD dwBytes )
{
	return	::eslHeapAllocate( NULL, dwBytes, 0 ) ;
}
// Освобождение аудио-буфера
//////////////////////////////////////////////////////////////////////////////
void MIODynamicPlayer::DeleteWaveBuffer( void * ptrWaveBuf )
{
	::eslHeapFree( NULL, ptrWaveBuf ) ;
}
// Создание декодера
//////////////////////////////////////////////////////////////////////////////
MIODecoder * MIODynamicPlayer::CreateMIODecoder( void )
{
	return	new MIODecoder ;
}


// ERIFile
//////////////////////////////////////////////////////////////////////////////
const ERIFile & MIODynamicPlayer::GetERIFile( void ) const
{
	return	m_erif ;
}


// Получить количество каналов
//////////////////////////////////////////////////////////////////////////////
DWORD MIODynamicPlayer::GetChannelCount( void ) const
{
	return	m_erif.m_MIOInfHdr.dwChannelCount ;
}
// Получить частоту дискретизации
//////////////////////////////////////////////////////////////////////////////
DWORD MIODynamicPlayer::GetFrequency( void ) const
{
	return	m_erif.m_MIOInfHdr.dwSamplesPerSec ;
}
// Получить bps
//////////////////////////////////////////////////////////////////////////////
DWORD MIODynamicPlayer::GetBitsPerSample( void ) const
{
	return	m_erif.m_MIOInfHdr.dwBitsPerSample ;
}
// Получить общую длину (количество выборок)
//////////////////////////////////////////////////////////////////////////////
DWORD MIODynamicPlayer::GetTotalSampleCount( void ) const
{
	return	m_erif.m_MIOInfHdr.dwAllSampleCount ;
}


// Получить буфер предварительной выборки
//////////////////////////////////////////////////////////////////////////////
MIODynamicPlayer::EPreloadBuffer *MIODynamicPlayer::GetPreloadBuffer( void )
{
    while ( m_queueSound.GetSize() <= 1 )
    {
        EPreloadBuffer *	pBuffer =
            LoadSoundStream( m_nCurrentSample ) ;
        if ( pBuffer == NULL )
        {
            break ;
        }
        AddPreloadBuffer( pBuffer ) ;
        m_nCurrentSample += pBuffer->m_miodh.dwSampleCount ;
    }
	EPreloadBuffer *	pBuffer = NULL ;
	Lock( ) ;
	if ( m_queueSound.GetSize() != 0 )
	{
		pBuffer = m_queueSound.GetAt( 0 ) ;
		m_queueSound.DetachAt( 0 ) ;
		if ( m_queueSound.GetSize() == 0 )
		{
            //::ResetEvent( m_hQueueNotEmpty ) ;
		}
		if ( m_queueSound.GetSize() < m_queueSound.GetLimit() )
		{
            //::SetEvent( m_hQueueSpace ) ;
            //::ResetEvent( m_hQueueFull ) ;
		}
	}
	Unlock( ) ;
	return	pBuffer ;
}
// Добавить в буфер предварительной выборки
//////////////////////////////////////////////////////////////////////////////
void MIODynamicPlayer::AddPreloadBuffer( EPreloadBuffer * pBuffer )
{
	Lock( ) ;
	if ( m_queueSound.GetSize() < m_queueSound.GetLimit() )
	{
		m_queueSound.Add( pBuffer ) ;
		if ( m_queueSound.GetSize() != 0 )
		{
            //::SetEvent( m_hQueueNotEmpty ) ;
		}
		if ( m_queueSound.GetSize() >= m_queueSound.GetLimit() )
		{
            //::ResetEvent( m_hQueueSpace ) ;
            //::SetEvent( m_hQueueFull ) ;
		}
	}
	Unlock( ) ;
}


// Чтение аудиоданных
//////////////////////////////////////////////////////////////////////////////
MIODynamicPlayer::EPreloadBuffer * MIODynamicPlayer::LoadSoundStream( unsigned int & nCurrentSample )
{
	EKeyPoint	keypoint ;
	DWORD	dwRecPosition = m_erif.GetPosition( ) ;
	while ( m_erif.DescendRecord( (const UINT64 *)"SoundStm" ) )
	{
		if ( nCurrentSample == 0 )
		{
			// 1つも音声レコードが無い場合はエラー
			return	NULL ;
		}
		//
		// レコードの終端に到達したら
		// 自動的に先頭に移動
		EKeyPoint *	pKeyPoint = SearchKeySample( nCurrentSample ) ;
		if ( pKeyPoint == NULL )
		{
			keypoint.m_nKeySample = nCurrentSample ;
			keypoint.m_dwRecOffset = dwRecPosition ;
			AddKeySample( keypoint ) ;
		}
		//
		nCurrentSample = 0 ;
		m_erif.Seek( 0, ESLFileObject::FromBegin ) ;
	}
	//
	// 音声データレコードを読み込む
	DWORD	dwDataBytes = m_erif.GetLength( ) ;
	EPreloadBuffer *	pBuffer =
		new EPreloadBuffer( dwDataBytes - sizeof(MIO_DATA_HEADER) ) ;
	//
	pBuffer->m_nKeySample = nCurrentSample ;
	m_erif.Read( &(pBuffer->m_miodh), sizeof(MIO_DATA_HEADER) ) ;
	m_erif.Read( pBuffer->m_ptrBuffer,
					dwDataBytes - sizeof(MIO_DATA_HEADER) ) ;
	//
	m_erif.AscendRecord( ) ;
	//
	// キーポイントの設定
	if ( pBuffer->m_miodh.bytFlags & MIO_LEAD_BLOCK )
	{
		EKeyPoint *	pKeyPoint = SearchKeySample( nCurrentSample ) ;
		if ( pKeyPoint == NULL )
		{
			keypoint.m_nKeySample = nCurrentSample ;
			keypoint.m_dwRecOffset = dwRecPosition ;
			AddKeySample( keypoint ) ;
		}
	}
	return	pBuffer ;
}


// Добавить ключевой кадр
//////////////////////////////////////////////////////////////////////////////
void MIODynamicPlayer::AddKeySample( const MIODynamicPlayer::EKeyPoint & key )
{
	Lock( ) ;
	m_listKeySample.Add( new EKeyPoint( key ) ) ;
	Unlock( ) ;
}
// Поиск заданного ключевого кадра
//////////////////////////////////////////////////////////////////////////////
MIODynamicPlayer::EKeyPoint *MIODynamicPlayer::SearchKeySample( unsigned int nKeySample )
{
	int		iFirst, iMiddle, iEnd ;
	EKeyPoint *	pFoundKey = NULL ;
	//
	Lock( ) ;
	//
	if ( m_listKeySample.GetSize() == 0 )
	{
		Unlock( ) ;
		return	NULL ;
	}
	//
	iFirst = 0 ;
	iMiddle = 0 ;
	iEnd = m_listKeySample.GetSize() - 1 ;
	//
	for ( ; ; )
	{
		if ( iFirst >= iEnd )
		{
			pFoundKey = NULL ;
			ESLAssert( iMiddle < (int) m_listKeySample.GetSize() ) ;
			if ( m_listKeySample[iMiddle].m_nKeySample == nKeySample )
			{
				pFoundKey = m_listKeySample.GetAt( iMiddle ) ;
			}
			else if ( m_listKeySample[iMiddle].m_nKeySample > nKeySample )
			{
				while ( iMiddle > 0 )
				{
					if ( m_listKeySample[-- iMiddle].m_nKeySample <= nKeySample )
					{
						pFoundKey = m_listKeySample.GetAt( iMiddle ) ;
						break ;
					}
				}
			}
			else
			{
				while ( iMiddle < (int) m_listKeySample.GetSize() - 1 )
				{
					if ( m_listKeySample[iMiddle + 1].m_nKeySample == nKeySample )
					{
						pFoundKey = m_listKeySample.GetAt( iMiddle + 1 ) ;
						break ;
					}
					else if ( m_listKeySample[iMiddle + 1].m_nKeySample > nKeySample )
					{
						pFoundKey = m_listKeySample.GetAt( iMiddle ) ;
						break ;
					}
					++ iMiddle ;
				}
			}
			break ;
		}
		//
		iMiddle = (iFirst + iEnd) / 2 ;
		pFoundKey = m_listKeySample.GetAt( iMiddle ) ;
		//
		if ( pFoundKey->m_nKeySample == nKeySample )
		{
			break ;
		}
		if ( pFoundKey->m_nKeySample > nKeySample )
		{
			iEnd = iMiddle - 1 ;
		}
		else
		{
			iFirst = iMiddle + 1 ;
		}
	}
	//
	Unlock( ) ;
	//
	return	pFoundKey ;
}
// Прочитать блок, содержащий указанный семпл
//////////////////////////////////////////////////////////////////////////////
void MIODynamicPlayer::SeekKeySample( ULONG nSample, unsigned int & nCurrentSample )
{
	EPreloadBuffer *	pBuffer ;
	//
	// 既に先読みキューに読み込まれていないか判断
	//////////////////////////////////////////////////////////////////////////
	Lock( ) ;
	//
	unsigned int	iLoaded = 0 ;
	unsigned int	iLeadBlock = 0 ;
	while ( iLoaded < m_queueSound.GetSize() )
	{
		pBuffer = m_queueSound.GetAt( iLoaded ) ;
		if ( pBuffer != NULL )
		{
			if ( pBuffer->m_miodh.bytFlags & MIO_LEAD_BLOCK )
			{
				iLeadBlock = iLoaded ;
			}
			if ( (pBuffer->m_nKeySample <= nSample)
				&& ((nSample - pBuffer->m_nKeySample)
						< pBuffer->m_miodh.dwSampleCount) )
			{
				break ;
			}
		}
		++ iLoaded ;
	}
	//
	// 既に読み込まれている場合にはそこまでシークする
	//
	if ( iLoaded < m_queueSound.GetSize() )
	{
		//
		// 最も近いリードブロックまで破棄する
		unsigned int	i ;
		m_queueSound.RemoveBetween( 0, iLeadBlock ) ;
		//
		// 特定のブロックまでシークする
		for ( i = iLeadBlock; i < iLoaded; i ++ )
		{
			pBuffer = GetPreloadBuffer( ) ;
			if ( pBuffer == NULL )
				break ;
			//
			DWORD	dwSampleBytes =
				GetChannelCount() * GetBitsPerSample() / 8 ;
			DWORD	dwBytes =
				pBuffer->m_miodh.dwSampleCount * dwSampleBytes ;
			//
			void *	ptrWaveBuf = AllocateWaveBuffer( dwBytes ) ;
			//
			m_pmioc->AttachInputFile( pBuffer ) ;
			//
			m_pmiod->DecodeSound( *m_pmioc, pBuffer->m_miodh, ptrWaveBuf ) ;
			//
			DeleteWaveBuffer( ptrWaveBuf ) ;
			delete	pBuffer ;
		}
		Unlock( ) ;
		return ;
	}
	//
	// 既に読み込まれているブロックを破棄する
	//
	m_queueSound.SetSize( 0 ) ;
	Unlock( ) ;
	//
	// リストに指定のサンプルを含むキーが登録されているか検索し、
	// 登録されていない場合には、シークする
	//////////////////////////////////////////////////////////////////////////
	EKeyPoint *	pKeyPoint = SearchKeySample( nSample ) ;
	if ( pKeyPoint == NULL )
	{
		Lock( ) ;
		if ( m_listKeySample.GetSize() > 0 )
		{
			pKeyPoint = m_listKeySample.GetAt( m_listKeySample.GetSize() - 1 ) ;
			m_erif.Seek( pKeyPoint->m_dwRecOffset, ESLFileObject::FromBegin ) ;
			nCurrentSample = pKeyPoint->m_nKeySample ;
		}
		Unlock( ) ;
		//
		// 各レコードを順次読み込む
		for ( ; ; )
		{
			DWORD	dwRecPosition = m_erif.GetPosition( ) ;
			if ( m_erif.DescendRecord( (const UINT64 *)"SoundStm" ) )
			{
				return ;
			}
			//
			MIO_DATA_HEADER	miodh ;
			m_erif.Read( &miodh, sizeof(miodh) ) ;
			m_erif.AscendRecord( ) ;
			//
			// キーポイントの設定
			if ( miodh.bytFlags & MIO_LEAD_BLOCK )
			{
				pKeyPoint = SearchKeySample( nCurrentSample ) ;
				if ( pKeyPoint == NULL )
				{
					EKeyPoint	keypoint ;
					keypoint.m_nKeySample = nCurrentSample ;
					keypoint.m_dwRecOffset = dwRecPosition ;
					AddKeySample( keypoint ) ;
					//
					pKeyPoint =
						m_listKeySample.GetAt( m_listKeySample.GetSize() - 1 ) ;
				}
			}
			//
			// 位置の更新
			if ( (nCurrentSample <= nSample) &&
					((nSample - nCurrentSample) < miodh.dwSampleCount) )
			{
				break ;
			}
			nCurrentSample += miodh.dwSampleCount ;
		}
	}
	//
	// 指定のキーポイントからシークする
	//////////////////////////////////////////////////////////////////////////
	if ( pKeyPoint == NULL )
	{
		return ;
	}
	nCurrentSample = pKeyPoint->m_nKeySample ;
	m_erif.Seek( pKeyPoint->m_dwRecOffset, ESLFileObject::FromBegin ) ;
	//
	for ( ; ; )
	{
		if ( m_erif.DescendRecord( (const UINT64 *)"SoundStm" ) )
		{
			return ;
		}
		//
		// 音声データレコードを読み込む
		DWORD	dwDataBytes = m_erif.GetLength( ) ;
		EPreloadBuffer *	pBuffer =
			new EPreloadBuffer( dwDataBytes - sizeof(MIO_DATA_HEADER) ) ;
		//
		pBuffer->m_nKeySample = nCurrentSample ;
		m_erif.Read( &(pBuffer->m_miodh), sizeof(MIO_DATA_HEADER) ) ;
		m_erif.Read( pBuffer->m_ptrBuffer,
						dwDataBytes - sizeof(MIO_DATA_HEADER) ) ;
		//
		m_erif.AscendRecord( ) ;
		//
		// 位置の更新
		if ( (nCurrentSample <= nSample) &&
			((nSample - nCurrentSample) < pBuffer->m_miodh.dwSampleCount) )
		{
			nCurrentSample += pBuffer->m_miodh.dwSampleCount ;
			AddPreloadBuffer( pBuffer ) ;
			break ;
		}
		nCurrentSample += pBuffer->m_miodh.dwSampleCount ;
		//
		// データを展開して破棄する
		DWORD	dwSampleBytes = GetChannelCount() * GetBitsPerSample() / 8 ;
		DWORD	dwBytes = pBuffer->m_miodh.dwSampleCount * dwSampleBytes ;
		//
		void *	ptrWaveBuf = AllocateWaveBuffer( dwBytes ) ;
		//
		m_pmioc->AttachInputFile( pBuffer ) ;
		//
		m_pmiod->DecodeSound( *m_pmioc, pBuffer->m_miodh, ptrWaveBuf ) ;
		//
		DeleteWaveBuffer( ptrWaveBuf ) ;
		delete	pBuffer ;
	}
}


// Exclusive processing (critical section)
//////////////////////////////////////////////////////////////////////////////
void MIODynamicPlayer::Lock( void )
{
    //::EnterCriticalSection( &m_cs ) ;
}
void MIODynamicPlayer::Unlock( void )
{
    //::LeaveCriticalSection( &m_cs ) ;
}

