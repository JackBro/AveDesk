// Unzipper.cpp: implementation of the CUnzipper class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Unzipper.h"

#include "zlib\unzip.h"
#include "zlib\iowin32.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#pragma comment(lib,"unzip/zlib/unzipd.lib")

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

const UINT BUFFERSIZE = 20480;

CUnzipper::CUnzipper(const char* szFileName) : m_uzFile(0)
{
	m_szOutputFolder[0] = 0;

	if (szFileName)
	{
		m_uzFile = unzOpen(szFileName);

		if (m_uzFile)
		{
			// set the default output folder
			char* szPath = _strdup(szFileName);

			// strip off extension
			char* p = strrchr(szPath, '.');

			if (p)
				*p = 0;

			strcpy(m_szOutputFolder, szPath);
			free(szPath);
		}
	}
}

CUnzipper::~CUnzipper()
{
	CloseZip();
}

BOOL CUnzipper::CloseZip()
{
	unzCloseCurrentFile(m_uzFile);

	int nRet = unzClose(m_uzFile);
	m_uzFile = NULL;

	return (nRet == UNZ_OK);
}

// simple interface
BOOL CUnzipper::Unzip(BOOL bIgnoreFilePath)
{
	if (!m_uzFile)
		return FALSE;

	return UnzipTo(m_szOutputFolder, bIgnoreFilePath);
}

BOOL CUnzipper::UnzipTo(const char* szFolder, BOOL bIgnoreFilePath)
{
	if (!m_uzFile)
		return FALSE;

	if (!szFolder || !CreateFolderA(szFolder))
		return FALSE;

	if (GetFileCount() == 0)
		return FALSE;

	if (!GotoFirstFile())
		return FALSE;

	// else
	do
	{
		if (!UnzipFile(szFolder, bIgnoreFilePath))
			return FALSE;
	}
	while (GotoNextFile());
	
	return TRUE;
}

BOOL CUnzipper::Unzip(const char* szFileName, const char* szFolder, BOOL bIgnoreFilePath)
{
	CloseZip();

	if (!OpenZip(szFileName))
		return FALSE;

	return UnzipTo(szFolder, bIgnoreFilePath);
}

// extended interface
BOOL CUnzipper::OpenZip(const char* szFileName)
{
	CloseZip();

	if (szFileName)
	{
		m_uzFile = unzOpen(szFileName);

		if (m_uzFile)
		{
			// set the default output folder
			char* szPath = _strdup(szFileName);

			// strip off extension
			char* p = strrchr(szPath, '.');

			if (p)
				*p = 0;

			strcpy(m_szOutputFolder, szPath);
			free(szPath);
		}
	}

	return (m_uzFile != NULL);
}

BOOL CUnzipper::SetOutputFolder(const char* szFolder)
{
	DWORD dwAttrib = GetFileAttributesA(szFolder);

	if (dwAttrib != 0xffffffff && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
		return FALSE;

	strcpy(m_szOutputFolder, szFolder);

	return CreateFolderA(szFolder);
}

int CUnzipper::GetFileCount()
{
	if (!m_uzFile)
		return 0;

	unz_global_info info;

	if (unzGetGlobalInfo(m_uzFile, &info) == UNZ_OK)
	{
		return (int)info.number_entry;
	}

	return 0;
}

BOOL CUnzipper::GetFileInfo(int nFile, UZ_FileInfo& info)
{
	if (!m_uzFile)
		return FALSE;

	if (!GotoFile(nFile))
		return FALSE;

	return GetFileInfo(info);
}

BOOL CUnzipper::UnzipFile(int nFile, const char* szFolder, BOOL bIgnoreFilePath)
{
	if (!m_uzFile)
		return FALSE;

	if (!szFolder)
		szFolder = m_szOutputFolder;

	if (!GotoFile(nFile))
		return FALSE;

	return UnzipFile(szFolder, bIgnoreFilePath);
}

BOOL CUnzipper::GotoFirstFile(const char* szExt)
{
	if (!m_uzFile)
		return FALSE;

	if (!szExt || !strlen(szExt))
		return (unzGoToFirstFile(m_uzFile) == UNZ_OK);

	// else
	if (unzGoToFirstFile(m_uzFile) == UNZ_OK)
	{
		UZ_FileInfo info;

		if (!GetFileInfo(info))
			return FALSE;

		// test extension
		char* pExt = strrchr(info.szFileName, '.');

		if (pExt)
		{
			pExt++;

			if (strcmpi(szExt, pExt) == 0)
				return TRUE;
		}

		return GotoNextFile(szExt);
	}

	return FALSE;
}

BOOL CUnzipper::GotoNextFile(const char* szExt)
{
	if (!m_uzFile)
		return FALSE;

	if (!szExt || !strlen(szExt))
		return (unzGoToNextFile(m_uzFile) == UNZ_OK);

	// else
	UZ_FileInfo info;

	while (unzGoToNextFile(m_uzFile) == UNZ_OK)
	{
		if (!GetFileInfo(info))
			return FALSE;

		// test extension
		char* pExt = strrchr(info.szFileName, '.');

		if (pExt)
		{
			pExt++;

			if (strcmpi(szExt, pExt) == 0)
				return TRUE;
		}
	}

	return FALSE;

}

BOOL CUnzipper::GetFileInfo(UZ_FileInfo& info)
{
	if (!m_uzFile)
		return FALSE;

	unz_file_info uzfi;

	ZeroMemory(&info, sizeof(info));
	ZeroMemory(&uzfi, sizeof(uzfi));

	if (UNZ_OK != unzGetCurrentFileInfo(m_uzFile, &uzfi, info.szFileName, MAX_PATH, NULL, 0, info.szComment, MAX_COMMENT))
		return FALSE;

	// copy across
	info.dwVersion = uzfi.version;	
	info.dwVersionNeeded = uzfi.version_needed;
	info.dwFlags = uzfi.flag;	
	info.dwCompressionMethod = uzfi.compression_method; 
	info.dwDosDate = uzfi.dosDate;  
	info.dwCRC = uzfi.crc;	 
	info.dwCompressedSize = uzfi.compressed_size; 
	info.dwUncompressedSize = uzfi.uncompressed_size;
	info.dwInternalAttrib = uzfi.internal_fa; 
	info.dwExternalAttrib = uzfi.external_fa; 

	// replace filename forward slashes with backslashes
	int nLen = strlen(info.szFileName);

	while (nLen--)
	{
		if (info.szFileName[nLen] == '/')
			info.szFileName[nLen] = '\\';
	}

	// is it a folder?
	info.bFolder = (info.szFileName[strlen(info.szFileName) - 1] == '\\');

	return TRUE;
}

BOOL CUnzipper::UnzipFile(const char* szFolder, BOOL bIgnoreFilePath)
{
	if (!m_uzFile)
		return FALSE;

	if (!szFolder)
		szFolder = m_szOutputFolder;

	if (!CreateFolderA(szFolder))
		return FALSE;

	UZ_FileInfo info;
	GetFileInfo(info);

	// if the item is a folder then simply return 'TRUE'
	if (info.szFileName[strlen(info.szFileName) - 1] == '\\')
		return TRUE;

	// build the output filename
	char szFilePath[MAX_PATH];
	strcpy(szFilePath, szFolder);

	// append backslash
	if (szFilePath[strlen(szFilePath) - 1] != '\\')
		strcat(szFilePath, "\\");

	if (bIgnoreFilePath)
	{
		char* p = strrchr(info.szFileName, '\\');

		if (p)
			strcpy(info.szFileName, p + 1);
	}

	strcat(szFilePath, info.szFileName);

	// open the input and output files
	if (!CreateFilePath(szFilePath))
		return FALSE;

	HANDLE hOutputFile = ::CreateFileA(szFilePath, 
										GENERIC_WRITE,
										0,
										NULL,
										CREATE_ALWAYS,
										FILE_ATTRIBUTE_NORMAL,
										NULL);

	if (INVALID_HANDLE_VALUE == hOutputFile)
		return FALSE;

	if (unzOpenCurrentFile(m_uzFile) != UNZ_OK)
		return FALSE;

	// read the file and output
	int nRet = UNZ_OK;
	char pBuffer[BUFFERSIZE];

	do
	{
		nRet = unzReadCurrentFile(m_uzFile, pBuffer, BUFFERSIZE);

		if (nRet > 0)
		{
			// output
			DWORD dwBytesWritten = 0;

			if (!::WriteFile(hOutputFile, pBuffer, nRet, &dwBytesWritten, NULL) ||
				dwBytesWritten != (DWORD)nRet)
			{
				nRet = UNZ_ERRNO;
				break;
			}
		}
	}
	while (nRet > 0);

	CloseHandle(hOutputFile);
	unzCloseCurrentFile(m_uzFile);

	if (nRet == UNZ_OK)
		SetFileModTime(szFilePath, info.dwDosDate);

	return (nRet == UNZ_OK);
}

BOOL CUnzipper::GotoFile(int nFile)
{
	if (!m_uzFile)
		return FALSE;

	if (nFile < 0 || nFile >= GetFileCount())
		return FALSE;

	GotoFirstFile();

	while (nFile--)
	{
		if (!GotoNextFile())
			return FALSE;
	}

	return TRUE;
}

BOOL CUnzipper::GotoFile(const char* szFileName, BOOL bIgnoreFilePath)
{
	if (!m_uzFile)
		return FALSE;

	// try the simple approach
	if (unzLocateFile(m_uzFile, szFileName, 2) == UNZ_OK)
		return TRUE;

	else if (bIgnoreFilePath)
	{ 
		// brute force way
		if (unzGoToFirstFile(m_uzFile) != UNZ_OK)
			return FALSE;

		UZ_FileInfo info;

		do
		{
			if (!GetFileInfo(info))
				return FALSE;

			// test name
			char* pName = strrchr(info.szFileName, '\\');

			if (pName)
			{
				pName++;

				if (strcmpi(szFileName, pName) == 0)
					return TRUE;
			}
		}
		while (unzGoToNextFile(m_uzFile) == UNZ_OK);
	}

	// else
	return FALSE;
}

BOOL CUnzipper::CreateFolderA(const char* szFolder)
{
	if (!szFolder || !strlen(szFolder))
		return FALSE;

	DWORD dwAttrib = GetFileAttributesA(szFolder);

	// already exists ?
	if (dwAttrib != 0xffffffff)
		return ((dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);

	// recursively create from the top down
	char* szPath = _strdup(szFolder);
	char* p = strrchr(szPath, '\\');

	if (p) 
	{
		// The parent is a dir, not a drive
		*p = '\0';
			
		// if can't create parent
		if (!CreateFolderA(szPath))
		{
			free(szPath);
			return FALSE;
		}
		free(szPath);

		if (!::CreateDirectoryA(szFolder, NULL)) 
			return FALSE;
	}
	
	return TRUE;
}

BOOL CUnzipper::CreateFilePath(const char* szFilePath)
{
	char* szPath = _strdup(szFilePath);
	char* p = strrchr(szPath,'\\');

	BOOL bRes = FALSE;

	if (p)
	{
		*p = '\0';

		bRes = CreateFolderA(szPath);
	}

	free(szPath);

	return bRes;
}

BOOL CUnzipper::SetFileModTime(const char* szFilePath, DWORD dwDosDate)
{
	HANDLE hFile = CreateFileA(szFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (!hFile)
		return FALSE;
	
	FILETIME ftm, ftLocal, ftCreate, ftLastAcc, ftLastWrite;

	BOOL bRes = (GetFileTime(hFile, &ftCreate, &ftLastAcc, &ftLastWrite) == TRUE);

	if (bRes)
		bRes = DosDateTimeToFileTime((WORD)(dwDosDate >> 16), (WORD)dwDosDate, &ftLocal);

	if (bRes)
		bRes = LocalFileTimeToFileTime(&ftLocal, &ftm);

	if (bRes)
		bRes = SetFileTime(hFile, &ftm, &ftLastAcc, &ftm);

	CloseHandle(hFile);

	return bRes;
}

