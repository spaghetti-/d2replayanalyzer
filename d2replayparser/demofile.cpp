#include "demofile.h"
#include "snappy/snappy.h"
#include <assert.h>

uint32 ReadVarInt32( const std::string& buf, size_t& index )
{
	uint32 b;
	int count = 0;
	uint32 result = 0;

	do
	{
		if ( count == 5 )
		{
			// If we get here it means that the fifth bit had its
			// high bit set, which implies corrupt data.
			assert( 0 );
			return result;
		}
		else if ( index >= buf.size() )
		{
			assert( 0 );
			return result;
		}

		b = buf[ index++ ];
		result |= ( b & 0x7F ) << ( 7 * count );
		++count;
	} while ( b & 0x80 );

	return result;
}

CDemoFile::CDemoFile() :
	m_fileBufferPos( 0 )
{
}

CDemoFile::~CDemoFile()
{
	Close();
}

bool CDemoFile::IsDone()
{
	return m_fileBufferPos >= m_fileBuffer.size();
}

EDemoCommands CDemoFile::ReadMessageType( int *pTick, bool *pbCompressed )
{
	uint32 Cmd = ReadVarInt32( m_fileBuffer, m_fileBufferPos );

	if( pbCompressed )
		*pbCompressed = !!( Cmd & DEM_IsCompressed );

	Cmd = ( Cmd & ~DEM_IsCompressed );

	int Tick = ReadVarInt32( m_fileBuffer, m_fileBufferPos );
	if( pTick )
		*pTick = Tick;

	if( m_fileBufferPos >= m_fileBuffer.size() )
		return DEM_Error;

	return ( EDemoCommands )Cmd;
}

bool CDemoFile::ReadMessage( IDemoMessage *pMsg, bool bCompressed, int *pSize, int *pUncompressedSize )
{
	int Size = ReadVarInt32( m_fileBuffer, m_fileBufferPos );

	if( pSize )
	{
		*pSize = Size;
	}
	if( pUncompressedSize )
	{
		*pUncompressedSize = 0;
	}

	if( m_fileBufferPos + Size > m_fileBuffer.size() )
	{
		assert( 0 );
		return false;
	}

	if( pMsg )
	{
		const char *parseBuffer = &m_fileBuffer[ m_fileBufferPos ];
		m_fileBufferPos += Size;

		if( bCompressed )
		{
			if ( snappy::IsValidCompressedBuffer( parseBuffer, Size ) )
			{
				size_t uDecompressedLen;

				if ( snappy::GetUncompressedLength( parseBuffer, Size, &uDecompressedLen ) )
				{
					if( pUncompressedSize )
					{
						*pUncompressedSize = uDecompressedLen;
					}

					m_parseBufferSnappy.resize( uDecompressedLen );
					char *parseBufferUncompressed = &m_parseBufferSnappy[ 0 ];

					if ( snappy::RawUncompress( parseBuffer, Size, parseBufferUncompressed ) )
					{
						if ( pMsg->GetProtoMsg().ParseFromArray( parseBufferUncompressed, uDecompressedLen ) )
						{
							return true;
						}
					}
				}
			}

			assert( 0 );
			fprintf( stderr, "CDemoFile::ReadMessage() snappy::RawUncompress failed.\n" );
			return false;
		}

		return pMsg->GetProtoMsg().ParseFromArray( parseBuffer, Size );
	}
	else
	{
		m_fileBufferPos += Size;
		return true;
	}
}

bool CDemoFile::Open( const char *name )
{
	Close();

	FILE *fp = fopen( name, "rb" );
	if( fp )
	{
		size_t Length;
		protodemoheader_t DotaDemoHeader;

		fseek( fp, 0, SEEK_END );
		Length = ftell( fp );
		fseek( fp, 0, SEEK_SET );

		if( Length < sizeof( DotaDemoHeader ) )
		{
			fprintf( stderr, "CDemoFile::Open: file too small. %s.\n", name );
			return false;
		}

		fread( &DotaDemoHeader, 1, sizeof( DotaDemoHeader ), fp );
		Length -= sizeof( DotaDemoHeader );

		if( strcmp( DotaDemoHeader.demofilestamp, PROTODEMO_HEADER_ID ) )
		{
			fprintf( stderr, "CDemoFile::Open: demofilestamp doesn't match. %s.\n", name );
			return false;
		}

		m_fileBuffer.resize( Length );
		fread( &m_fileBuffer[ 0 ], 1, Length, fp );

		fclose( fp );
		fp = NULL;
	}

	if ( !m_fileBuffer.size() )
	{
		fprintf( stderr, "CDemoFile::Open: couldn't open file %s.\n", name );
		Close();
		return false;
	}

	m_fileBufferPos = 0;
	m_szFileName = name;
	return true;
}

void CDemoFile::Close()
{
	m_szFileName.clear();

	m_fileBufferPos = 0;
	m_fileBuffer.clear();

	m_parseBufferSnappy.clear();
}
