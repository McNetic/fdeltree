/*
 * fdeltree.cpp 
 *
 * A simple, fast recursive file deletion utitlity
 *
 * (C) 2015 Nicolai Ehemann (en@enlightened.de)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "time.h"

#define VERSION     "0.2"
#define VERDATE     "2015-07-15"
#define VERYEARS    "2015"

//#define DEBUG
#define MAX_LONG_PATH 32767

#define SUCCESS                          0
#define ERROR_ARGUMENT_MISSING          -1
#define ERROR_INVALID_ARGUMENT          -2
#define ERROR_PATH_NORMALIZATION_FAILED -3
#define ERROR_PATH_ATTRS_UNREADABLE     -4
#define ERROR_PATH_NO_DIRECTORY         -5
#define ERROR_DIRECTORY_REMOVAL_FAILED  -6
#define ERROR_FILE_READONLY             -7
#define ERROR_FILE_REMOVAL_FAILED       -8

#define OPT_NONE    0x00000000
#define OPT_VERBOSE 0x00000001
#define OPT_FORCE   0x00000002

#ifdef DEBUG
#define DB(x, y) _ftprintf(stderr, x, y)
#else
#define DB(x, y)
#endif

DWORD DeleteDirectory(const TCHAR* sPath, DWORD options = OPT_NONE);

int _cdecl usage(FILE* outstream = stderr) {
    return _ftprintf(outstream, TEXT("Usage: fdeltree [options] <directory>\noptions:\n")
                                TEXT("  /h  -  print this help\n  /v  -  verbose (print statistics every few seconds)\n")
                                TEXT("  /f  -  force deleting of read-only files\n")
                                );
}

int _tmain(int argc, _TCHAR* argv[]) {
    TCHAR sTmp[MAX_LONG_PATH] = _T(""); 
    TCHAR sDir[MAX_LONG_PATH] = _T("");
    DWORD fileAttrs;
    DWORD options = OPT_NONE;
    int dirArg = 1;
    bool fileArgFound = false;

    _tprintf(_T("fdeltree v%s (%s)\n(C) %s Nicolai Ehemann (en@enlightened.de)\n"), _T(VERSION), _T(VERDATE), _T(VERYEARS));
    if (2 > argc) {
        usage();
        return ERROR_ARGUMENT_MISSING;
    } else {
        while (dirArg < argc) {
            if (0 == _tcscmp(argv[dirArg], _T("/h")) || 0 == _tcscmp(argv[dirArg], _T("/H"))) {
                _tprintf(_T("A simple, fast recursive file deletion utility\n"));
                usage(stdout);
                return SUCCESS;
            } else if (0 == _tcscmp(argv[dirArg], _T("/v")) || 0 == _tcscmp(argv[dirArg], _T("/V"))) {
                options |= OPT_VERBOSE;
            } else if (0 == _tcscmp(argv[dirArg], _T("/f")) || 0 == _tcscmp(argv[dirArg], _T("/F"))) {
                options |= OPT_FORCE;
            } else if (dirArg + 1 != argc) {
                usage(stderr);
                return ERROR_INVALID_ARGUMENT;
            } else {
                fileArgFound = true;
            }
            dirArg++;
        }
    }

    if (!fileArgFound) {
        usage();
        return ERROR_ARGUMENT_MISSING;
    } else if (0 == GetFullPathName(argv[argc - 1], MAX_LONG_PATH, sTmp, NULL)) 
    {
        _ftprintf(stderr, TEXT("Error: path normalization failed\n"));
        return ERROR_PATH_NORMALIZATION_FAILED;
    }
    else {
        if (0 != _tcsncmp(sTmp, _T("\\\\?\\"), _tcslen(_T("\\\\?\\")))) {
            _tcscat_s(sDir, _T("\\\\?\\"));
        }
        _tcscat_s(sDir, sTmp);
        if (INVALID_FILE_ATTRIBUTES == (fileAttrs = GetFileAttributes(sDir))) {
            _ftprintf(stderr, TEXT("Error: unable to get directory attributes (does the directory exist?)\n"));
            return ERROR_PATH_ATTRS_UNREADABLE;
        } else if (!(fileAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
            _ftprintf(stderr, TEXT("Error: argument is not a directory\n"));
            return ERROR_PATH_NO_DIRECTORY;
        } else {
            return DeleteDirectory(sDir, options);
        }
    }
}

static TCHAR* getLastErrorText(TCHAR *pBuf, ULONG bufSize){
    DWORD retSize;
    LPTSTR pTemp=NULL;

    if (bufSize < 16) {
        if (bufSize > 0) {
            pBuf[0]='\0';
        }
        return(pBuf);
    }
    retSize=FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ARGUMENT_ARRAY, NULL, GetLastError(), LANG_NEUTRAL, (LPTSTR)&pTemp, 0, NULL);
    if (!retSize || pTemp == NULL) {
        pBuf[0]='\0';
    }
    else {
        pTemp[_tcslen(pTemp)-2]='\0'; //remove cr and newline character
        swprintf_s(pBuf, bufSize, _T("%.*s (0x%04x)"), bufSize-16, pTemp, GetLastError());
        LocalFree((HLOCAL)pTemp);
    }
    return(pBuf);
}

BOOL IsDots(const TCHAR* str) {
    if(_tcscmp(str,_T(".")) && _tcscmp(str,_T(".."))) {
        return FALSE;
    }
    return TRUE;
}

DWORD DeleteDirectory(const TCHAR* sPath, DWORD options) {
    std::stack<TCHAR*> dirStack;
    TCHAR *sCurDir, *sNewDir;
    TCHAR sNextFile[MAX_LONG_PATH];
    HANDLE hFind;
    WIN32_FIND_DATA FindFileData;
    DWORD curlen, len, retval, count, nextCount;
    time_t begin, curtime;

    len = _tcslen(sPath) + _tcslen(_T("\\")) + 1;
    sCurDir = new TCHAR[len];
    _tcscpy_s(sCurDir, len, sPath);
    _tcscat_s(sCurDir, len, _T("\\"));
    dirStack.push(sCurDir);
    count = 0;
    nextCount = 100;
    time(&begin);

    retval = SUCCESS;
    /* PSEUDO CODE ALGORITHM
    GET ITEM FROM STACK
      FIND FIRST CHILD
        IF NONE -> DELETE CURRENT, POP AS CURRENT
        ELSE -> PUSH CURRENT, ITERATE OVER CONTENTS
            -> DELETE FILES
            -> PUSH DIRECTORIES
    */
    while (!dirStack.empty()) {
        if (options & OPT_VERBOSE && count > nextCount) {
            time(&curtime);
            curtime -= begin;
            if (1 > curtime) {
                curtime = 1;
            }
            _tprintf(_T("\rdeleted %lu files/directories in %I64d seconds (~ %I64d files/minute)"), count, curtime, ((count * 60) / curtime));
            nextCount = count + 2500;
        }
        sCurDir = dirStack.top();
        _tcscpy_s(sNextFile, MAX_LONG_PATH, sCurDir);
        curlen = _tcslen(sNextFile);
        _tcscat_s(sNextFile, MAX_LONG_PATH, _T("*"));

        hFind = FindFirstFileEx(sNextFile, FindExInfoBasic, &FindFileData, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
        if (hFind == INVALID_HANDLE_VALUE || (IsDots(FindFileData.cFileName) && (!FindNextFile(hFind, &FindFileData) || (IsDots(FindFileData.cFileName) && !FindNextFile(hFind, &FindFileData))))) {
            // nothing (or only ./..) found
            FindClose(hFind);
            dirStack.pop();
            sCurDir[_tcslen(sCurDir) - 1] = 0;
            DB(_T("D DIR %s\n"), sCurDir);
            if (!RemoveDirectory(sCurDir)) {
                TCHAR msg[256];
                retval = ERROR_DIRECTORY_REMOVAL_FAILED;
                getLastErrorText(msg, 256);
                _ftprintf(stderr, TEXT("Error: removal of directory '%s' failed (%s)\n"), sCurDir, msg);
            }
            count++;
            delete sCurDir;
        } else {
            do {
                if( IsDots(FindFileData.cFileName) ) {
                    continue;
                }    

                sNextFile[curlen] = 0;
                _tcscpy_s(sNextFile + curlen, MAX_LONG_PATH - (curlen), FindFileData.cFileName);
                if((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    DB(_T("F DIR %s\n"), sNextFile);
                    _tcscat_s(sNextFile, MAX_LONG_PATH, _T("\\"));
                    len = _tcslen(sNextFile) + 1;
                    sNewDir = new TCHAR[len];
                    _tcscpy_s(sNewDir, len, sNextFile);
                    dirStack.push(sNewDir);
                } else {
                    DB(_T("D FIL %s\n"), sNextFile);
                    if(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
                        if (options & OPT_FORCE) {
                            _wchmod(sNextFile, _S_IWRITE); // change read-only file mode
                        } else {
                            retval = ERROR_FILE_READONLY;
                            _ftprintf(stderr, TEXT("Error: file '%s' is set ready-only\n"), sNextFile);
                            break;
                        }
                    }
                    if(!DeleteFile(sNextFile)) {
                        TCHAR msg[256];
                        retval = ERROR_FILE_REMOVAL_FAILED;
                        getLastErrorText(msg, 256);
                        _ftprintf(stderr, TEXT("Error: removal of file '%s' failed (%s)\n"), sNextFile, msg);
                        break;
                    }
                    count++;
                }
            } while( FindNextFile(hFind,&FindFileData) );
            FindClose(hFind);
        }
        if (SUCCESS != retval) {
            while (!dirStack.empty()) {
                sCurDir = dirStack.top();
                delete sCurDir;
                dirStack.pop();
            }
            break;
        }
    }
    if (options & OPT_VERBOSE) {
        _tprintf(_T("\n"));
    }

    return retval;
}
