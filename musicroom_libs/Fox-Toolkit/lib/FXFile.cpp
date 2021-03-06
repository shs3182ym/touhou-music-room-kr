/********************************************************************************
*                                                                               *
*                             F i l e   C l a s s                               *
*                                                                               *
*********************************************************************************
* Copyright (C) 2000,2010 by Jeroen van der Zijp.   All Rights Reserved.        *
*********************************************************************************
* This library is free software; you can redistribute it and/or modify          *
* it under the terms of the GNU Lesser General Public License as published by   *
* the Free Software Foundation; either version 3 of the License, or             *
* (at your option) any later version.                                           *
*                                                                               *
* This library is distributed in the hope that it will be useful,               *
* but WITHOUT ANY WARRANTY; without even the implied warranty of                *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 *
* GNU Lesser General Public License for more details.                           *
*                                                                               *
* You should have received a copy of the GNU Lesser General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>          *
********************************************************************************/
#include "xincs.h"
#include "fxver.h"
#include "fxdefs.h"
#include "fxascii.h"
#include "FXHash.h"
#include "FXStream.h"
#include "FXString.h"
#include "FXPath.h"
#include "FXIO.h"
#include "FXStat.h"
#include "FXFile.h"
#include "FXPipe.h"
#include "FXDir.h"



/*
  Notes:

  - Implemented many functions in terms of FXFile and FXDir
    so we won't have to worry about unicode stuff.
  - Perhaps we should assume FXString contains string encoded in the locale
    of the system [which in case of Windows would mean it contains UTF-16]?
    Because it isn't between 8-bit or 16-bit, but also about utf-8 v.s. other
    encodings..
  - This should be in FXSystem; FXSystem needs to determine the locale, then
    determine the codec needed for that locale, and then use this codec for
    encoding our strings to that locale.
*/

#ifdef WIN32
#define BadHandle INVALID_HANDLE_VALUE
#else
#define BadHandle -1
#endif

#ifdef WIN32
#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif
#endif

using namespace FX;

/*******************************************************************************/

namespace FX {



// Construct file and attach existing handle h
FXFile::FXFile(FXInputHandle h,FXuint m){
  FXIO::open(h,m);
  }


// Construct and open a file
FXFile::FXFile(const FXString& file,FXuint m,FXuint perm){
  open(file,m,perm);
  }


// Open file
FXbool FXFile::open(const FXString& file,FXuint m,FXuint perm){
  if(!file.empty() && !isOpen()){
#ifdef WIN32
    DWORD flags=GENERIC_READ;
    DWORD creation=OPEN_EXISTING;

    // Basic access mode
    switch(m&(ReadOnly|WriteOnly)){
      case ReadOnly: flags=GENERIC_READ; break;
      case WriteOnly: flags=GENERIC_WRITE; break;
      case ReadWrite: flags=GENERIC_READ|GENERIC_WRITE; break;
      }

    // Creation and truncation mode
    switch(m&(Create|Truncate|Exclusive)){
      case Create: creation=OPEN_ALWAYS; break;
      case Truncate: creation=TRUNCATE_EXISTING; break;
      case Create|Truncate: creation=CREATE_ALWAYS; break;
      case Create|Truncate|Exclusive: creation=CREATE_NEW; break;
      }

    // Non-blocking mode
    if(m&NonBlocking){
      // FIXME
      }

    // Do it
#ifdef UNICODE
    FXnchar unifile[MAXPATHLEN];
    utf2ncs(unifile,MAXPATHLEN,file.text(),file.length()+1);
    device=::CreateFileW(unifile,flags,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,creation,FILE_ATTRIBUTE_NORMAL,NULL);
#else
    device=::CreateFileA(file.text(),flags,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,creation,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
    if(device!=BadHandle){
      if(m&Append){                             // Appending
        ::SetFilePointer(device,0,NULL,FILE_END);
        }
      access=(m|OwnHandle);                     // Own handle
      return true;
      }
#else
    FXuint bits=perm&0777;
    FXuint flags=0;

    // Basic access mode
    switch(m&(ReadOnly|WriteOnly)){
      case ReadOnly: flags=O_RDONLY; break;
      case WriteOnly: flags=O_WRONLY; break;
      case ReadWrite: flags=O_RDWR; break;
      }

    // Appending and truncation
    if(m&Append) flags|=O_APPEND;
    if(m&Truncate) flags|=O_TRUNC;

    // Non-blocking mode
    if(m&NonBlocking) flags|=O_NONBLOCK;

    // Change access time
#ifdef O_NOATIME
    if(m&NoAccessTime) flags|=O_NOATIME;
#endif

    // Creation mode
    if(m&Create){
      flags|=O_CREAT;
      if(m&Exclusive) flags|=O_EXCL;
      }

    // Permission bits
    if(perm&FXIO::SetUser) bits|=S_ISUID;
    if(perm&FXIO::SetGroup) bits|=S_ISGID;
    if(perm&FXIO::Sticky) bits|=S_ISVTX;

    // Do it
    device=::open(file.text(),flags,bits);
    if(device!=BadHandle){
      access=(m|OwnHandle);                     // Own handle
      return true;
      }
#endif
    }
  return false;
  }


// Open device with access mode and handle
FXbool FXFile::open(FXInputHandle h,FXuint m){
  return FXIO::open(h,m);
  }


// Return true if serial access only
FXbool FXFile::isSerial() const {
  return false;
  }


// Get position
FXlong FXFile::position() const {
  if(isOpen()){
#ifdef WIN32
    LARGE_INTEGER pos;
    pos.QuadPart=0;
    pos.LowPart=::SetFilePointer(device,0,&pos.HighPart,FILE_CURRENT);
    if(pos.LowPart==INVALID_SET_FILE_POINTER && GetLastError()!=NO_ERROR) pos.QuadPart=-1;
    return pos.QuadPart;
#else
    return ::lseek(device,0,SEEK_CUR);
#endif
    }
  return -1;
  }


// Move to position
FXlong FXFile::position(FXlong offset,FXuint from){
  if(isOpen()){
#ifdef WIN32
    LARGE_INTEGER pos;
    pos.QuadPart=offset;
    pos.LowPart=::SetFilePointer(device,pos.LowPart,&pos.HighPart,from);
    if(pos.LowPart==INVALID_SET_FILE_POINTER && GetLastError()!=NO_ERROR) pos.QuadPart=-1;
    return pos.QuadPart;
#else
    return ::lseek(device,offset,from);
#endif
    }
  return -1;
  }


// Read block
FXival FXFile::readBlock(void* data,FXival count){
  FXival nread=-1;
  if(isOpen()){
#ifdef WIN32
    DWORD nr;
    if(::ReadFile(device,data,(DWORD)count,&nr,NULL)!=0){
      nread=(FXival)nr;
      }
#else
    do{
      nread=::read(device,data,count);
      }
    while(nread<0 && errno==EINTR);
#endif
    }
  return nread;
  }


// Write block
FXival FXFile::writeBlock(const void* data,FXival count){
  FXival nwritten=-1;
  if(isOpen()){
#ifdef WIN32
    DWORD nw;
    if(::WriteFile(device,data,(DWORD)count,&nw,NULL)!=0){
      nwritten=(FXival)nw;
      }
#else
    do{
      nwritten=::write(device,data,count);
      }
    while(nwritten<0 && errno==EINTR);
#endif
    }
  return nwritten;
  }


// Truncate file
FXlong FXFile::truncate(FXlong s){
  if(isOpen()){
#ifdef WIN32
    LARGE_INTEGER oldpos,newpos;
    oldpos.QuadPart=0;
    newpos.QuadPart=s;
    oldpos.LowPart=::SetFilePointer(device,0,&oldpos.HighPart,FILE_CURRENT);
    newpos.LowPart=::SetFilePointer(device,newpos.LowPart,&newpos.HighPart,FILE_BEGIN);
    if((newpos.LowPart==INVALID_SET_FILE_POINTER && GetLastError()!=NO_ERROR) || ::SetEndOfFile(device)==0) newpos.QuadPart=-1;
    ::SetFilePointer(device,oldpos.LowPart,&oldpos.HighPart,FILE_BEGIN);
    return newpos.QuadPart;
#else
    if(::ftruncate(device,s)==0) return s;
#endif
    }
  return -1;
  }


// Flush to disk
FXbool FXFile::flush(){
  if(isOpen()){
#ifdef WIN32
    return ::FlushFileBuffers(device)!=0;
#else
    return ::fsync(device)==0;
#endif
    }
  return false;
  }


// Test if we're at the end
FXbool FXFile::eof(){
  if(isOpen()){
    register FXlong pos=position();
    return 0<=pos && size()<=pos;
    }
  return true;
  }


// Return file size
FXlong FXFile::size(){
  if(isOpen()){
#ifdef WIN32
    ULARGE_INTEGER result;
    result.LowPart=::GetFileSize(device,&result.HighPart);
    return result.QuadPart;
#else
    struct stat data;
    if(::fstat(device,&data)==0) return data.st_size;
#endif
    }
  return -1;
  }


// Close file
FXbool FXFile::close(){
  if(isOpen()){
    if(access&OwnHandle){
#ifdef WIN32
      if(::CloseHandle(device)!=0){
        device=BadHandle;
        access=NoAccess;
        return true;
        }
#else
      if(::close(device)==0){
        device=BadHandle;
        access=NoAccess;
        return true;
        }
#endif
      }
    device=BadHandle;
    access=NoAccess;
    }
  return false;
  }


// Destroy
FXFile::~FXFile(){
  close();
  }



// Create new (empty) file
FXbool FXFile::create(const FXString& file,FXuint perm){
  if(!file.empty()){
#ifdef WIN32
#ifdef UNICODE
    FXnchar buffer[MAXPATHLEN];
    utf2ncs(buffer,MAXPATHLEN,file.text(),file.length()+1);
    FXInputHandle h=::CreateFileW(buffer,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
#else
    FXInputHandle h=::CreateFileA(file.text(),GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
#endif
    if(h!=BadHandle){ ::CloseHandle(h); return true; }
#else
    FXInputHandle h=::open(file.text(),O_CREAT|O_WRONLY|O_TRUNC|O_EXCL,perm);
    if(h!=BadHandle){ ::close(h); return true; }
#endif
    }
  return false;
  }


// Remove a file
FXbool FXFile::remove(const FXString& file){
  if(!file.empty()){
#ifdef WIN32
#ifdef UNICODE
    FXnchar buffer[MAXPATHLEN];
    utf2ncs(buffer,MAXPATHLEN,file.text(),file.length()+1);
    return ::DeleteFileW(buffer)!=0;
#else
    return ::DeleteFileA(file.text())!=0;
#endif
#else
    return ::unlink(file.text())==0;
#endif
    }
  return false;
  }


// Rename file
FXbool FXFile::rename(const FXString& srcfile,const FXString& dstfile){
  if(srcfile!=dstfile){
#ifdef WIN32
#ifdef UNICODE
    FXnchar oldname[MAXPATHLEN],newname[MAXPATHLEN];
    utf2ncs(oldname,MAXPATHLEN,srcfile.text(),srcfile.length()+1);
    utf2ncs(newname,MAXPATHLEN,dstfile.text(),dstfile.length()+1);
    return ::MoveFileExW(oldname,newname,MOVEFILE_REPLACE_EXISTING)!=0;
#else
    return ::MoveFileExA(srcfile.text(),dstfile.text(),MOVEFILE_REPLACE_EXISTING)!=0;
#endif
#else
    return ::rename(srcfile.text(),dstfile.text())==0;
#endif
    }
  return false;
  }


#ifdef WIN32

typedef BOOL (WINAPI *FunctionCreateHardLink)(const TCHAR*,const TCHAR*,LPSECURITY_ATTRIBUTES);

static BOOL WINAPI HelpCreateHardLink(const TCHAR*,const TCHAR*,LPSECURITY_ATTRIBUTES);

static FunctionCreateHardLink MyCreateHardLink=HelpCreateHardLink;


// The first time its called, we're setting the function pointer, so
// subsequent calls will experience no additional overhead whatsoever!
static BOOL WINAPI HelpCreateHardLink(const TCHAR* newname,const TCHAR* oldname,LPSECURITY_ATTRIBUTES sa){
#ifdef UNICODE
  HMODULE hkernel=LoadLibraryW(L"Kernel32");
  if(hkernel){
    MyCreateHardLink=(FunctionCreateHardLink)::GetProcAddress(hkernel,"CreateHardLinkW");
    ::FreeLibrary(hkernel);
    return MyCreateHardLink(newname,oldname,sa);
    }
#else
  HMODULE hkernel=LoadLibraryA("Kernel32");
  if(hkernel){
    MyCreateHardLink=(FunctionCreateHardLink)::GetProcAddress(hkernel,"CreateHardLinkA");
    ::FreeLibrary(hkernel);
    return MyCreateHardLink(newname,oldname,sa);
    }
#endif
  return 0;
  }

#endif


// Link file
FXbool FXFile::link(const FXString& oldfile,const FXString& newfile){
  if(newfile!=oldfile){
#ifdef WIN32
#ifdef UNICODE
    FXnchar oldname[MAXPATHLEN],newname[MAXPATHLEN];
    utf2ncs(oldname,MAXPATHLEN,oldfile.text(),oldfile.length()+1);
    utf2ncs(newname,MAXPATHLEN,newfile.text(),newfile.length()+1);
    return MyCreateHardLink(newname,oldname,NULL)!=0;
#else
    return MyCreateHardLink(newfile.text(),oldfile.text(),NULL)!=0;
#endif
#else
    return ::link(oldfile.text(),newfile.text())==0;
#endif
    }
  return false;
  }


// Read symbolic link
FXString FXFile::symlink(const FXString& file){
  if(!file.empty()){
#ifndef WIN32
    FXchar lnk[MAXPATHLEN+1];
    FXint len=::readlink(file.text(),lnk,MAXPATHLEN);
    if(0<=len){
      return FXString(lnk,len);
      }
#endif
    }
  return FXString::null;
  }


// Symbolic Link file
FXbool FXFile::symlink(const FXString& oldfile,const FXString& newfile){
  if(newfile!=oldfile){
#ifndef WIN32
    return ::symlink(oldfile.text(),newfile.text())==0;
#endif
    }
  return false;
  }


// Return true if files are identical
FXbool FXFile::identical(const FXString& file1,const FXString& file2){
  if(file1!=file2){
    FXStat info1;
    FXStat info2;
    if(FXStat::statFile(file1,info1) && FXStat::statFile(file2,info2)){
      return info1.index()==info2.index() && info1.volume()==info2.volume();
      }
    return false;
    }
  return true;
  }


// Copy srcfile to dstfile, overwriting dstfile if allowed
FXbool FXFile::copy(const FXString& srcfile,const FXString& dstfile,FXbool overwrite){
  if(srcfile!=dstfile){
    FXuchar buffer[4096]; FXival nwritten,nread; FXStat stat;
    FXFile src(srcfile,FXIO::Reading);
    if(src.isOpen()){
      if(FXStat::stat(src,stat)){
        FXFile dst(dstfile,overwrite?FXIO::Writing:FXIO::Writing|FXIO::Exclusive,stat.mode());
        if(dst.isOpen()){
          while(1){
            nread=src.readBlock(buffer,sizeof(buffer));
            if(nread<0) return false;
            if(nread==0) break;
            nwritten=dst.writeBlock(buffer,nread);
            if(nwritten<0) return false;
            }
          return true;
          }
        }
      }
    }
  return false;
  }


// Concatenate srcfile1 and srcfile2 to dstfile, overwriting dstfile if allowed
FXbool FXFile::concat(const FXString& srcfile1,const FXString& srcfile2,const FXString& dstfile,FXbool overwrite){
  FXuchar buffer[4096]; FXival nwritten,nread;
  if(srcfile1!=dstfile && srcfile2!=dstfile){
    FXFile src1(srcfile1,FXIO::Reading);
    if(src1.isOpen()){
      FXFile src2(srcfile2,FXIO::Reading);
      if(src2.isOpen()){
        FXFile dst(dstfile,overwrite?FXIO::Writing:FXIO::Writing|FXIO::Exclusive);
        if(dst.isOpen()){
          while(1){
            nread=src1.readBlock(buffer,sizeof(buffer));
            if(nread<0) return false;
            if(nread==0) break;
            nwritten=dst.writeBlock(buffer,nread);
            if(nwritten<0) return false;
            }
          while(1){
            nread=src2.readBlock(buffer,sizeof(buffer));
            if(nread<0) return false;
            if(nread==0) break;
            nwritten=dst.writeBlock(buffer,nread);
            if(nwritten<0) return false;
            }
          return true;
          }
        }
      }
    }
  return false;
  }

// FIXME use FXFile::identical to keep struct on stack for cycle-test


// Recursively copy files or directories from srcfile to dstfile, overwriting dstfile if allowed
FXbool FXFile::copyFiles(const FXString& srcfile,const FXString& dstfile,FXbool overwrite){
  if(srcfile!=dstfile){
    FXString name,linkname;
    FXStat srcstat;
    FXStat dststat;
    //FXTRACE((100,"FXFile::copyFiles(%s,%s)\n",srcfile.text(),dstfile.text()));
    if(FXStat::statLink(srcfile,srcstat)){

      // Destination is a directory?
      if(FXStat::statLink(dstfile,dststat)){
        if(!dststat.isDirectory()){
          if(!overwrite) return false;
          //FXTRACE((100,"FXFile::remove(%s)\n",dstfile.text()));
          if(!FXFile::remove(dstfile)) return false;
          }
        }

      // Source is a directory
      if(srcstat.isDirectory()){

        // Make destination directory if needed
        if(!dststat.isDirectory()){
          //FXTRACE((100,"FXDir::create(%s)\n",dstfile.text()));

          // Make directory
          if(!FXDir::create(dstfile,srcstat.mode()|FXIO::OwnerWrite)) return false;
          }

        // Open source directory
        FXDir dir(srcfile);

        // Copy source directory
        while(dir.next(name)){

          // Skip '.' and '..'
          if(name[0]=='.' && (name[1]=='\0' || (name[1]=='.' && name[2]=='\0'))) continue;

          // Recurse
          if(!FXFile::copyFiles(srcfile+PATHSEP+name,dstfile+PATHSEP+name,overwrite)) return false;
          }

        // OK
        return true;
        }

      // Source is a file
      if(srcstat.isFile()){
        //FXTRACE((100,"FXFile::copy(%s,%s)\n",srcfile.text(),dstfile.text()));

        // Simply copy
        if(!FXFile::copy(srcfile,dstfile,overwrite)) return false;

        // OK
        return true;
        }

      // Source is symbolic link: make a new one
      if(srcstat.isLink()){
        linkname=FXFile::symlink(srcfile);
        //FXTRACE((100,"symlink(%s,%s)\n",srcfile.text(),dstfile.text()));

        // New symlink to whatever old one referred to
        if(!FXFile::symlink(srcfile,dstfile)) return false;

        // OK
        return true;
        }

      // Source is fifo: make a new one
      if(srcstat.isFifo()){
        //FXTRACE((100,"FXPipe::create(%s)\n",dstfile.text()));

        // Make named pipe
        if(!FXPipe::create(dstfile,srcstat.mode())) return false;

        // OK
        return true;
        }

/*
  // Source is device: make a new one
  if(S_ISBLK(status1.st_mode) || S_ISCHR(status1.st_mode) || S_ISSOCK(status1.st_mode)){
    FXTRACE((100,"mknod(%s)\n",newfile.text()));
    return ::mknod(newfile.text(),status1.st_mode,status1.st_rdev)==0;
    }
*/

      }
    }
  return false;
  }



// Recursively copy or move files or directories from srcfile to dstfile, overwriting dstfile if allowed
FXbool FXFile::moveFiles(const FXString& srcfile,const FXString& dstfile,FXbool overwrite){
  if(srcfile!=dstfile){
    if(FXStat::exists(srcfile)){
      if(FXStat::exists(dstfile)){
        if(!overwrite) return false;
        if(!FXFile::removeFiles(dstfile,true)) return false;
        }
      if(FXDir::rename(srcfile,dstfile)) return true;
      if(FXFile::copyFiles(srcfile,dstfile,overwrite)){
        return FXFile::removeFiles(srcfile,true);
        }
      }
    }
  return false;
  }


// Remove file or directory, recursively if allowed
FXbool FXFile::removeFiles(const FXString& path,FXbool recursive){
  FXStat stat;
  if(FXStat::statLink(path,stat)){
    if(stat.isDirectory()){
      if(recursive){
        FXDir dir(path);
        FXString name;
        while(dir.next(name)){
          if(name[0]=='.' && (name[1]=='\0' || (name[1]=='.' && name[2]=='\0'))) continue;
          if(!FXFile::removeFiles(path+PATHSEP+name,true)) return false;
          }
        }
      return FXDir::remove(path);
      }
    return FXFile::remove(path);
    }
  return false;
  }


}

