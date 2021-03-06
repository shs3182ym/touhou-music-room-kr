/********************************************************************************
*                                                                               *
*                        F i l e   S t a t i s t i c s                          *
*                                                                               *
*********************************************************************************
* Copyright (C) 2005,2010 by Jeroen van der Zijp.   All Rights Reserved.        *
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
#include "FXStat.h"
#include "FXFile.h"



/*
  Notes:
  - Find out stuff about files and directories.
*/


using namespace FX;

/*******************************************************************************/

namespace FX {


// Return true if it is a hidden file (note: Windows-only attribute)
FXbool FXStat::isHidden() const {
  return (modeFlags&FXIO::Hidden)!=0;
  }

// Return true if it is a regular file
FXbool FXStat::isFile() const {
  return (modeFlags&FXIO::File)!=0;
  }

// Return true if it is a link
FXbool FXStat::isLink() const {
  return (modeFlags&FXIO::SymLink)!=0;
  }

// Return true if character device
FXbool FXStat::isCharacter() const {
  return (modeFlags&FXIO::Character)!=0;
  }

// Return true if block device
FXbool FXStat::isBlock() const {
  return (modeFlags&FXIO::Block)!=0;
  }

// Return true if socket device
FXbool FXStat::isSocket() const {
  return (modeFlags&FXIO::Socket)!=0;
  }

// Return true if fifo device
FXbool FXStat::isFifo() const {
  return (modeFlags&FXIO::Fifo)!=0;
  }

// Return true if input path is a directory
FXbool FXStat::isDirectory() const {
  return (modeFlags&FXIO::Directory)!=0;
  }

// Return true if file is readable
FXbool FXStat::isReadable() const {
  return (modeFlags&(FXIO::OtherRead|FXIO::GroupRead|FXIO::OwnerRead))!=0;
  }

// Return true if file is writable
FXbool FXStat::isWritable() const {
  return (modeFlags&(FXIO::OtherWrite|FXIO::GroupWrite|FXIO::OwnerWrite))!=0;
  }

// Return true if file is executable
FXbool FXStat::isExecutable() const {
  return (modeFlags&(FXIO::OtherExec|FXIO::GroupExec|FXIO::OwnerExec))!=0;
  }

// Return true if owner has read-write-execute permissions
FXbool FXStat::isOwnerReadWriteExecute() const {
  return (modeFlags&FXIO::OwnerExec) && (modeFlags&FXIO::OwnerWrite) && (modeFlags&FXIO::OwnerRead);
  }

// Return true if owner has read permissions
FXbool FXStat::isOwnerReadable() const {
  return (modeFlags&FXIO::OwnerRead)!=0;
  }

// Return true if owner has write permissions
FXbool FXStat::isOwnerWritable() const {
  return (modeFlags&FXIO::OwnerWrite)!=0;
  }

// Return true if owner has execute permissions
FXbool FXStat::isOwnerExecutable() const {
  return (modeFlags&FXIO::OwnerExec)!=0;
  }

// Return true if group has read-write-execute permissions
FXbool FXStat::isGroupReadWriteExecute() const {
  return (modeFlags&FXIO::GroupExec) && (modeFlags&FXIO::GroupWrite) && (modeFlags&FXIO::GroupRead);
  }

// Return true if group has read permissions
FXbool FXStat::isGroupReadable() const {
  return (modeFlags&FXIO::GroupRead)!=0;
  }

// Return true if group has write permissions
FXbool FXStat::isGroupWritable() const {
  return (modeFlags&FXIO::GroupWrite)!=0;
  }

// Return true if group has execute permissions
FXbool FXStat::isGroupExecutable() const {
  return (modeFlags&FXIO::GroupExec)!=0;
  }

// Return true if others have read-write-execute permissions
FXbool FXStat::isOtherReadWriteExecute() const {
  return (modeFlags&FXIO::OtherExec) && (modeFlags&FXIO::OtherWrite) && (modeFlags&FXIO::OtherRead);
  }

// Return true if others have read permissions
FXbool FXStat::isOtherReadable() const {
  return (modeFlags&FXIO::OtherRead)!=0;
  }

// Return true if others have write permissions
FXbool FXStat::isOtherWritable() const {
  return (modeFlags&FXIO::OtherWrite)!=0;
  }

// Return true if others have execute permissions
FXbool FXStat::isOtherExecutable() const {
  return (modeFlags&FXIO::OtherExec)!=0;
  }

// Return true if the file sets the user id on execution
FXbool FXStat::isSetUid() const {
  return (modeFlags&FXIO::SetUser)!=0;
  }

// Return true if the file sets the group id on execution
FXbool FXStat::isSetGid() const {
  return (modeFlags&FXIO::SetGroup)!=0;
  }

// Return true if the file has the sticky bit set
FXbool FXStat::isSetSticky() const {
  return (modeFlags&FXIO::Sticky)!=0;
  }


#ifdef WIN32

// Convert 100ns since 01/01/1601 to ns since 01/01/1970
static inline FXTime fxfiletime(FXTime ft){
#if defined(__CYGWIN__) || defined(__MINGW32__) || defined(__SC__) || defined(__BCPLUSPLUS__)
  return (ft-116444736000000000LL)*100LL;
#else
  return (ft-116444736000000000L)*100L;
#endif
  }

#endif


// Get statistics of given file
FXbool FXStat::statFile(const FXString& file,FXStat& info){
  FXbool result=false;
  info.modeFlags=0;
  info.userNumber=0;
  info.groupNumber=0;
  info.linkCount=0;
  info.createTime=0;
  info.accessTime=0;
  info.modifyTime=0;
  info.fileVolume=0;
  info.fileIndex=0;
  info.fileSize=0;
  if(!file.empty()){
#ifdef WIN32
#ifdef UNICODE
    TCHAR buffer[MAXPATHLEN];
    HANDLE hfile;
    utf2ncs(buffer,MAXPATHLEN,file.text(),file.length()+1);
    if((hfile=::CreateFile(buffer,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL))!=INVALID_HANDLE_VALUE){
//    if((hfile=::CreateFile(buffer,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL))!=INVALID_HANDLE_VALUE){
      BY_HANDLE_FILE_INFORMATION data;
      if(::GetFileInformationByHandle(hfile,&data)){
        SHFILEINFO sfi;
        info.modeFlags=0777;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN) info.modeFlags|=FXIO::Hidden;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) info.modeFlags|=FXIO::Directory;
        else info.modeFlags|=FXIO::File;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_READONLY) info.modeFlags&=~(FXIO::OwnerWrite|FXIO::GroupWrite|FXIO::OtherWrite);
        if(::SHGetFileInfoW(buffer,0,&sfi,sizeof(SHFILEINFO),SHGFI_EXETYPE)==0) info.modeFlags&=~(FXIO::OwnerExec|FXIO::GroupExec|FXIO::OtherExec);
        info.userNumber=0;
        info.groupNumber=0;
        info.linkCount=data.nNumberOfLinks;
        info.accessTime=fxfiletime(*((FXTime*)&data.ftLastAccessTime));
        info.modifyTime=fxfiletime(*((FXTime*)&data.ftLastWriteTime));
        info.createTime=fxfiletime(*((FXTime*)&data.ftCreationTime));
        info.fileVolume=data.dwVolumeSerialNumber;
        info.fileIndex=(((FXulong)data.nFileIndexHigh)<<32)|((FXulong)data.nFileIndexLow);
        info.fileSize=(((FXlong)data.nFileSizeHigh)<<32)|((FXlong)data.nFileSizeLow);
        result=true;
        }
      ::CloseHandle(hfile);
      }
#else
    HANDLE hfile;
    if((hfile=::CreateFile(file.text(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL))!=INVALID_HANDLE_VALUE){
//    if((hfile=::CreateFile(file.text(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL))!=INVALID_HANDLE_VALUE){
      BY_HANDLE_FILE_INFORMATION data;
      if(::GetFileInformationByHandle(hfile,&data)){
        SHFILEINFO sfi;
        info.modeFlags=0777;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN) info.modeFlags|=FXIO::Hidden;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) info.modeFlags|=FXIO::Directory;
        else info.modeFlags|=FXIO::File;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_READONLY) info.modeFlags&=~(FXIO::OwnerWrite|FXIO::GroupWrite|FXIO::OtherWrite);
        if(::SHGetFileInfo(file.text(),0,&sfi,sizeof(SHFILEINFO),SHGFI_EXETYPE)==0) info.modeFlags&=~(FXIO::OwnerExec|FXIO::GroupExec|FXIO::OtherExec);
        info.userNumber=0;
        info.groupNumber=0;
        info.linkCount=data.nNumberOfLinks;
        info.accessTime=fxfiletime(*((FXTime*)&data.ftLastAccessTime));
        info.modifyTime=fxfiletime(*((FXTime*)&data.ftLastWriteTime));
        info.createTime=fxfiletime(*((FXTime*)&data.ftCreationTime));
        info.fileVolume=data.dwVolumeSerialNumber;
        info.fileIndex=(((FXulong)data.nFileIndexHigh)<<32)|((FXulong)data.nFileIndexLow);
        info.fileSize=(((FXlong)data.nFileSizeHigh)<<32)|((FXlong)data.nFileSizeLow);
        result=true;
        }
      ::CloseHandle(hfile);
      }
#endif
#else
    const FXTime seconds=1000000000;
    struct stat data;
    if(::stat(file.text(),&data)==0){
      info.modeFlags=(data.st_mode&0777);
      if(S_ISDIR(data.st_mode)) info.modeFlags|=FXIO::Directory;
      if(S_ISREG(data.st_mode)) info.modeFlags|=FXIO::File;
      if(S_ISLNK(data.st_mode)) info.modeFlags|=FXIO::SymLink;
      if(S_ISCHR(data.st_mode)) info.modeFlags|=FXIO::Character;
      if(S_ISBLK(data.st_mode)) info.modeFlags|=FXIO::Block;
      if(S_ISFIFO(data.st_mode)) info.modeFlags|=FXIO::Fifo;
      if(S_ISSOCK(data.st_mode)) info.modeFlags|=FXIO::Socket;
      if(data.st_mode&S_ISUID) info.modeFlags|=FXIO::SetUser;
      if(data.st_mode&S_ISGID) info.modeFlags|=FXIO::SetGroup;
      if(data.st_mode&S_ISVTX) info.modeFlags|=FXIO::Sticky;
      info.userNumber=data.st_uid;
      info.groupNumber=data.st_gid;
      info.linkCount=data.st_nlink;
      info.accessTime=data.st_atime*seconds;
      info.modifyTime=data.st_mtime*seconds;
      info.createTime=data.st_ctime*seconds;
      info.fileVolume=(FXlong)data.st_dev;
      info.fileIndex=(FXlong)data.st_ino;
      info.fileSize=(FXlong)data.st_size;
      result=true;
      }
#endif
    }
  return result;
  }


// Get statistice of the linked file
FXbool FXStat::statLink(const FXString& file,FXStat& info){
  FXbool result=false;
  info.modeFlags=0;
  info.userNumber=0;
  info.groupNumber=0;
  info.linkCount=0;
  info.createTime=0;
  info.accessTime=0;
  info.modifyTime=0;
  info.fileVolume=0;
  info.fileIndex=0;
  info.fileSize=0;
  if(!file.empty()){
#ifdef WIN32
#ifdef UNICODE
    TCHAR buffer[MAXPATHLEN];
    HANDLE hfile;
    utf2ncs(buffer,MAXPATHLEN,file.text(),file.length()+1);
    if((hfile=::CreateFile(buffer,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL))!=INVALID_HANDLE_VALUE){
//    if((hfile=::CreateFile(buffer,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL))!=INVALID_HANDLE_VALUE){
      BY_HANDLE_FILE_INFORMATION data;
      if(::GetFileInformationByHandle(hfile,&data)){
        SHFILEINFO sfi;
        info.modeFlags=0777;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN) info.modeFlags|=FXIO::Hidden;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) info.modeFlags|=FXIO::Directory;
        else info.modeFlags|=FXIO::File;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_READONLY) info.modeFlags&=~(FXIO::OwnerWrite|FXIO::GroupWrite|FXIO::OtherWrite);
        if(::SHGetFileInfoW(buffer,0,&sfi,sizeof(SHFILEINFO),SHGFI_EXETYPE)==0) info.modeFlags&=~(FXIO::OwnerExec|FXIO::GroupExec|FXIO::OtherExec);
        info.userNumber=0;
        info.groupNumber=0;
        info.linkCount=data.nNumberOfLinks;
        info.accessTime=fxfiletime(*((FXTime*)&data.ftLastAccessTime));
        info.modifyTime=fxfiletime(*((FXTime*)&data.ftLastWriteTime));
        info.createTime=fxfiletime(*((FXTime*)&data.ftCreationTime));
        info.fileVolume=data.dwVolumeSerialNumber;
        info.fileIndex=(((FXulong)data.nFileIndexHigh)<<32)|((FXulong)data.nFileIndexLow);
        info.fileSize=(((FXlong)data.nFileSizeHigh)<<32)|((FXlong)data.nFileSizeLow);
        result=true;
        }
      ::CloseHandle(hfile);
      }
#else
    HANDLE hfile;
    if((hfile=::CreateFile(file.text(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL))!=INVALID_HANDLE_VALUE){
//    if((hfile=::CreateFile(file.text(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,NULL))!=INVALID_HANDLE_VALUE){
      BY_HANDLE_FILE_INFORMATION data;
      if(::GetFileInformationByHandle(hfile,&data)){
        SHFILEINFO sfi;
        info.modeFlags=0777;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN) info.modeFlags|=FXIO::Hidden;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) info.modeFlags|=FXIO::Directory;
        else info.modeFlags|=FXIO::File;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_READONLY) info.modeFlags&=~(FXIO::OwnerWrite|FXIO::GroupWrite|FXIO::OtherWrite);
        if(::SHGetFileInfo(file.text(),0,&sfi,sizeof(SHFILEINFO),SHGFI_EXETYPE)==0) info.modeFlags&=~(FXIO::OwnerExec|FXIO::GroupExec|FXIO::OtherExec);
        info.userNumber=0;
        info.groupNumber=0;
        info.linkCount=data.nNumberOfLinks;
        info.accessTime=fxfiletime(*((FXTime*)&data.ftLastAccessTime));
        info.modifyTime=fxfiletime(*((FXTime*)&data.ftLastWriteTime));
        info.createTime=fxfiletime(*((FXTime*)&data.ftCreationTime));
        info.fileVolume=data.dwVolumeSerialNumber;
        info.fileIndex=(((FXulong)data.nFileIndexHigh)<<32)|((FXulong)data.nFileIndexLow);
        info.fileSize=(((FXlong)data.nFileSizeHigh)<<32)|((FXlong)data.nFileSizeLow);
        result=true;
        }
      ::CloseHandle(hfile);
      }
#endif
#else
    const FXTime seconds=1000000000;
    struct stat data;
    if(::lstat(file.text(),&data)==0){
      info.modeFlags=(data.st_mode&0777);
      if(S_ISDIR(data.st_mode)) info.modeFlags|=FXIO::Directory;
      if(S_ISREG(data.st_mode)) info.modeFlags|=FXIO::File;
      if(S_ISLNK(data.st_mode)) info.modeFlags|=FXIO::SymLink;
      if(S_ISCHR(data.st_mode)) info.modeFlags|=FXIO::Character;
      if(S_ISBLK(data.st_mode)) info.modeFlags|=FXIO::Block;
      if(S_ISFIFO(data.st_mode)) info.modeFlags|=FXIO::Fifo;
      if(S_ISSOCK(data.st_mode)) info.modeFlags|=FXIO::Socket;
      if(data.st_mode&S_ISUID) info.modeFlags|=FXIO::SetUser;
      if(data.st_mode&S_ISGID) info.modeFlags|=FXIO::SetGroup;
      if(data.st_mode&S_ISVTX) info.modeFlags|=FXIO::Sticky;
      info.userNumber=data.st_uid;
      info.groupNumber=data.st_gid;
      info.linkCount=data.st_nlink;
      info.accessTime=data.st_atime*seconds;
      info.modifyTime=data.st_mtime*seconds;
      info.createTime=data.st_ctime*seconds;
      info.fileVolume=(FXlong)data.st_dev;
      info.fileIndex=(FXlong)data.st_ino;
      info.fileSize=(FXlong)data.st_size;
      result=true;
      }
#endif
    }
  return result;
  }


// Get statistice of the already open file
FXbool FXStat::stat(const FXFile& file,FXStat& info){
  info.modeFlags=0;
  info.userNumber=0;
  info.groupNumber=0;
  info.linkCount=0;
  info.createTime=0;
  info.accessTime=0;
  info.modifyTime=0;
  info.fileVolume=0;
  info.fileIndex=0;
  info.fileSize=0;
#ifdef WIN32
  BY_HANDLE_FILE_INFORMATION data;
  if(::GetFileInformationByHandle(file.handle(),&data)){
    info.modeFlags=0777;
    if(data.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN) info.modeFlags|=FXIO::Hidden;
    if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) info.modeFlags|=FXIO::Directory;
    else info.modeFlags|=FXIO::File;
    if(data.dwFileAttributes&FILE_ATTRIBUTE_READONLY) info.modeFlags&=~(FXIO::OwnerWrite|FXIO::GroupWrite|FXIO::OtherWrite);
    info.userNumber=0;
    info.groupNumber=0;
    info.linkCount=data.nNumberOfLinks;
    info.accessTime=fxfiletime(*((FXTime*)&data.ftLastAccessTime));
    info.modifyTime=fxfiletime(*((FXTime*)&data.ftLastWriteTime));
    info.createTime=fxfiletime(*((FXTime*)&data.ftCreationTime));
    info.fileVolume=data.dwVolumeSerialNumber;
    info.fileIndex=(((FXulong)data.nFileIndexHigh)<<32)|((FXulong)data.nFileIndexLow);
    info.fileSize=(((FXulong)data.nFileSizeHigh)<<32)|((FXulong)data.nFileSizeLow);
    return true;
    }
#else
  const FXTime seconds=1000000000;
  struct stat data;
  if(::fstat(file.handle(),&data)==0){
    info.modeFlags=(data.st_mode&0777);
    if(S_ISDIR(data.st_mode)) info.modeFlags|=FXIO::Directory;
    if(S_ISREG(data.st_mode)) info.modeFlags|=FXIO::File;
    if(S_ISLNK(data.st_mode)) info.modeFlags|=FXIO::SymLink;
    if(S_ISCHR(data.st_mode)) info.modeFlags|=FXIO::Character;
    if(S_ISBLK(data.st_mode)) info.modeFlags|=FXIO::Block;
    if(S_ISFIFO(data.st_mode)) info.modeFlags|=FXIO::Fifo;
    if(S_ISSOCK(data.st_mode)) info.modeFlags|=FXIO::Socket;
    if(data.st_mode&S_ISUID) info.modeFlags|=FXIO::SetUser;
    if(data.st_mode&S_ISGID) info.modeFlags|=FXIO::SetGroup;
    if(data.st_mode&S_ISVTX) info.modeFlags|=FXIO::Sticky;
    info.userNumber=data.st_uid;
    info.groupNumber=data.st_gid;
    info.linkCount=data.st_nlink;
    info.accessTime=data.st_atime*seconds;
    info.modifyTime=data.st_mtime*seconds;
    info.createTime=data.st_ctime*seconds;
    info.fileVolume=(FXlong)data.st_dev;
    info.fileIndex=(FXlong)data.st_ino;
    info.fileSize=(FXlong)data.st_size;
    return true;
    }
#endif
  return false;
  }


// Return file mode flags
FXuint FXStat::mode(const FXString& file){
  FXStat data;
  statFile(file,data);
  return data.mode();
  }


// Change the mode flags for this file
FXbool FXStat::mode(const FXString& file,FXuint perm){
#ifdef WIN32
  return false; // FIXME Unimplemented yet
#else
  FXuint bits=perm&0777;
  if(perm&FXIO::SetUser) bits|=S_ISUID;
  if(perm&FXIO::SetGroup) bits|=S_ISGID;
  if(perm&FXIO::Sticky) bits|=S_ISVTX;
  return !file.empty() && ::chmod(file.text(),bits)==0;
#endif
  }


// Return true if file exists
FXbool FXStat::exists(const FXString& file){
  if(!file.empty()){
#ifdef WIN32
#ifdef UNICODE
    FXnchar name[MAXPATHLEN];
    utf2ncs(name,MAXPATHLEN,file.text(),file.length()+1);
    return ::GetFileAttributesW(name)!=0xffffffff;
#else
    return ::GetFileAttributesA(file.text())!=0xffffffff;
#endif
#else
    struct stat status;
    return ::stat(file.text(),&status)==0;
#endif
    }
  return false;
  }


// Get file size
FXlong FXStat::size(const FXString& file){
  FXStat data;
  statFile(file,data);
  return data.size();
  }


// Return file volume number
FXlong FXStat::volume(const FXString& file){
  FXStat data;
  statFile(file,data);
  return data.volume();
  }


// Return file index number
FXlong FXStat::index(const FXString& file){
  FXStat data;
  statFile(file,data);
  return data.index();
  }


// Return number of links to file
FXuint FXStat::links(const FXString& file){
  FXStat data;
  statFile(file,data);
  return data.links();
  }


// Return time file was last modified
FXTime FXStat::modified(const FXString& file){
  FXStat data;
  statFile(file,data);
  return data.modified();
  }


// Return time file was last accessed
FXTime FXStat::accessed(const FXString& file){
  FXStat data;
  statFile(file,data);
  return data.accessed();
  }


// Return time when created
FXTime FXStat::created(const FXString& file){
  FXStat data;
  statFile(file,data);
  return data.created();
  }


// Return true if file is hidden
FXbool FXStat::isHidden(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isHidden();
  }


// Check if file represents a file
FXbool FXStat::isFile(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isFile();
  }


// Check if file represents a link
FXbool FXStat::isLink(const FXString& file){
  FXStat data;
  return statLink(file,data) && data.isLink();
  }


// Check if file represents a directory
FXbool FXStat::isDirectory(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isDirectory();
  }


// Return true if file is readable
FXbool FXStat::isReadable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isReadable();
  }


// Return true if file is writable
FXbool FXStat::isWritable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isWritable();
  }


// Return true if file is executable
FXbool FXStat::isExecutable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isExecutable();
  }


// Check if owner has full permissions
FXbool FXStat::isOwnerReadWriteExecute(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isOwnerReadWriteExecute();
  }


// Check if owner can read
FXbool FXStat::isOwnerReadable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isOwnerReadable();
  }


// Check if owner can write
FXbool FXStat::isOwnerWritable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isOwnerWritable();
  }


// Check if owner can execute
FXbool FXStat::isOwnerExecutable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isOwnerExecutable();
  }


// Check if group has full permissions
FXbool FXStat::isGroupReadWriteExecute(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isGroupReadWriteExecute();
  }


// Check if group can read
FXbool FXStat::isGroupReadable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isGroupReadable();
  }


// Check if group can write
FXbool FXStat::isGroupWritable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isGroupWritable();
  }


// Check if group can execute
FXbool FXStat::isGroupExecutable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isGroupExecutable();
  }


// Check if everybody has full permissions
FXbool FXStat::isOtherReadWriteExecute(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isOtherReadWriteExecute();
  }


// Check if everybody can read
FXbool FXStat::isOtherReadable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isOtherReadable();
  }


// Check if everybody can write
FXbool FXStat::isOtherWritable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isOtherWritable();
  }


// Check if everybody can execute
FXbool FXStat::isOtherExecutable(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isOtherExecutable();
  }


// Test if suid bit set
FXbool FXStat::isSetUid(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isSetUid();
  }


// Test if sgid bit set
FXbool FXStat::isSetGid(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isSetGid();
  }


// Test if sticky bit set
FXbool FXStat::isSetSticky(const FXString& file){
  FXStat data;
  return statFile(file,data) && data.isSetSticky();
  }


#if 0
FXulong FXStat::getTotalDiskSpace(const FXString& path){
#ifdef WIN32
#else
  struct statvfs64 info;
  if(statvfs64(path.text(),&info)==0){
    if(info.f_frsize)
      return info.f_blocks*info.f_frsize;
    else
      return info.f_blocks*info.f_bsize;
    }
#endif
  return 0;
  }

FXulong FXStat::getAvailableDiskSpace(const FXString& path){
#ifdef WIN32
#else
  struct statvfs64 info;
  if(statvfs64(path.text(),&info)==0){
    if(info.f_frsize)
      return info.f_bavail*info.f_frsize;
    else
      return info.f_bavail*info.f_bsize;
    }
#endif
  return 0;
  }
#endif

}

