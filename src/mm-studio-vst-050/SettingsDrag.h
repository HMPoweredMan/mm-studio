// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <stdexcept>
#include "SettingsMidi.h"
#include "PercussionPads.h"
namespace mm5studio {
inline std::filesystem::path settingsMidiCache(){
 std::vector<wchar_t> text(32768);const DWORD n=GetEnvironmentVariableW(L"MM5_STUDIO_MIDI_CACHE",text.data(),DWORD(text.size()));
 if(n&&n<text.size())return std::filesystem::absolute(std::filesystem::path(text.data()));
 const DWORD count=GetTempPathW(DWORD(text.size()),text.data());if(!count||count>=text.size())throw std::runtime_error("Temporary MIDI directory unavailable");
 return std::filesystem::path(text.data())/L"MM Studio"/L"Settings MIDI";
}
inline std::filesystem::path cacheMidi(const std::vector<uint8_t>& bytes,const std::wstring& stem){
 uint64_t hash=14695981039346656037ull;for(auto byte:bytes){hash^=byte;hash*=1099511628211ull;}
 const auto folder=settingsMidiCache();std::filesystem::create_directories(folder);
 wchar_t name[150];std::swprintf(name,150,L"%s-%016llx",stem.c_str(),static_cast<unsigned long long>(hash));
 for(int suffix=0;suffix<1000;suffix++){
  const auto file=folder/(std::wstring(name)+(suffix?L"-"+std::to_wstring(suffix):L"")+L".mid");
  HANDLE handle=CreateFileW(file.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(handle==INVALID_HANDLE_VALUE){if(GetLastError()!=ERROR_FILE_EXISTS)throw std::runtime_error("Could not create settings MIDI file");std::ifstream existing(file,std::ios::binary);std::vector<uint8_t> data((std::istreambuf_iterator<char>(existing)),{});if(data==bytes)return file;continue;}
  DWORD written=0;const bool ok=WriteFile(handle,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size();CloseHandle(handle);if(!ok)throw std::runtime_error("Could not write settings MIDI file");return file;
 }throw std::runtime_error("Settings MIDI filename unavailable");
}
inline std::filesystem::path cacheSettingsMidi(const SettingsSnapshot& s){return cacheMidi(settingsMidi(s),L"MM5 channel "+std::to_wstring(s.channel+1)+L" program "+std::to_wstring(s.program)+L" settings");}
inline std::filesystem::path cachePadMidi(const SettingsSnapshot& s,int index){const auto bytes=padMidi(s,index);if(bytes.empty())throw std::runtime_error("Invalid percussion pad");return cacheMidi(bytes,L"MM5 pad "+std::to_wstring(index)+L" note "+std::to_wstring(padNote(kSongSounds[index])));}
class SettingsFileData final:public IDataObject {
 std::atomic<ULONG> refs{1};std::wstring path;
 bool accepts(FORMATETC* f)const{return f&&f->cfFormat==CF_HDROP&&(f->tymed&TYMED_HGLOBAL)&&f->dwAspect==DVASPECT_CONTENT&&f->lindex==-1;}
public:
 explicit SettingsFileData(std::wstring filename):path(std::move(filename)){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IDataObject){*out=static_cast<IDataObject*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{const auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE GetData(FORMATETC* f,STGMEDIUM* m)override{if(!m)return E_POINTER;if(!accepts(f))return DV_E_FORMATETC;*m={};const SIZE_T size=sizeof(DROPFILES)+(path.size()+2)*sizeof(wchar_t);auto data=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,size);if(!data)return E_OUTOFMEMORY;auto* drop=static_cast<DROPFILES*>(GlobalLock(data));if(!drop){GlobalFree(data);return E_OUTOFMEMORY;}drop->pFiles=sizeof(DROPFILES);drop->fWide=TRUE;std::memcpy(reinterpret_cast<char*>(drop)+sizeof(DROPFILES),path.c_str(),(path.size()+1)*sizeof(wchar_t));GlobalUnlock(data);m->tymed=TYMED_HGLOBAL;m->hGlobal=data;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*,STGMEDIUM*)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* f)override{return accepts(f)?S_OK:DV_E_FORMATETC;}
 HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*,FORMATETC* out)override{if(out)out->ptd=nullptr;return DATA_S_SAMEFORMATETC;}
 HRESULT STDMETHODCALLTYPE SetData(FORMATETC*,STGMEDIUM*,BOOL)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction,IEnumFORMATETC** out)override{if(!out)return E_POINTER;*out=nullptr;if(direction!=DATADIR_GET)return E_NOTIMPL;FORMATETC f{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};return SHCreateStdEnumFmtEtc(1,&f,out);}
 HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*,DWORD,IAdviseSink*,DWORD*)override{return OLE_E_ADVISENOTSUPPORTED;}
 HRESULT STDMETHODCALLTYPE DUnadvise(DWORD)override{return OLE_E_ADVISENOTSUPPORTED;}
 HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**)override{return OLE_E_ADVISENOTSUPPORTED;}
};
class SettingsDropSource final:public IDropSource {
 std::atomic<ULONG> refs{1};
public:
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IDropSource){*out=static_cast<IDropSource*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{const auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape,DWORD keys)override{return escape?DRAGDROP_S_CANCEL:!(keys&MK_LBUTTON)?DRAGDROP_S_DROP:S_OK;}
 HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD)override{return DRAGDROP_S_USEDEFAULTCURSORS;}
};
inline HRESULT dragSettingsFile(const SettingsSnapshot& snapshot,int padIndex=-1){
 const HRESULT ole=OleInitialize(nullptr);if(FAILED(ole))return ole;
 HRESULT result=E_FAIL;try{const auto file=padIndex<0?cacheSettingsMidi(snapshot):cachePadMidi(snapshot,padIndex);auto* data=new SettingsFileData(file.wstring());auto* source=new SettingsDropSource;DWORD effect=0;result=DoDragDrop(data,source,DROPEFFECT_COPY,&effect);data->Release();source->Release();if(result==DRAGDROP_S_DROP&&!(effect&DROPEFFECT_COPY))result=DRAGDROP_S_CANCEL;}catch(...){result=E_FAIL;}
 OleUninitialize();return result;
}
}
