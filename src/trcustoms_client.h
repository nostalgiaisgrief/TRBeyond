/* Read-only TRCustoms catalogue and image requests. Workers never touch GL or
   the active menu. Their results are consumed after their thread signals. */
#include "../third_party/cJSON/cJSON.h"
#include <winhttp.h>
#define COBJMACROS
#include <wincodec.h>

static const cJSON *catalog_field(const cJSON *object,const char *key){
    return cJSON_GetObjectItemCaseSensitive(object,key);
}
static const char *catalog_string(const cJSON *object,const char *key){
    const cJSON *field=catalog_field(object,key);
    return cJSON_IsString(field)?field->valuestring:"";
}
typedef struct {
    HANDLE thread;
    int page; /* Zero identifies an artwork request. */
    char url[1024];
    cJSON *record;
    unsigned char *pixels;
    UINT w,h;
} CatalogRequest;

/* Only these two HTTPS origins and resource types are needed for browsing. */
static unsigned char *catalog_get(const CatalogRequest *job,size_t *length){
    const char *prefix=job->page?"https://trcustoms.org/api/levels/?":"https://data.trcustoms.org/media/";
    if(strncmp(job->url,prefix,strlen(prefix)))return NULL;
    wchar_t path[1024];
    const char *resource=strchr(job->url+8,'/');
    if(!resource || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,resource,-1,path,1024))return NULL;
    HINTERNET session=WinHttpOpen(L"Tomb Raider Beyond",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,NULL,NULL,0);
    if(!session)return NULL;
    WinHttpSetTimeouts(session,5000,5000,8000,10000);
    HINTERNET host=WinHttpConnect(session,job->page?L"trcustoms.org":L"data.trcustoms.org",443,0);
    HINTERNET request=host?WinHttpOpenRequest(host,L"GET",path,NULL,NULL,NULL,WINHTTP_FLAG_SECURE):NULL;
    unsigned char *data=NULL;
    DWORD status=0,size=sizeof status,redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if(request && WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof redirect) &&
       WinHttpSendRequest(request,NULL,0,NULL,0,0,0) && WinHttpReceiveResponse(request,NULL) &&
       WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&status,&size,NULL) && status==200){
        size_t used=0,capacity=0,limit=job->page?4*1024*1024:12*1024*1024;
        unsigned char chunk[16384];DWORD count;
        while(WinHttpReadData(request,chunk,sizeof chunk,&count)){
            if(!count){if(data){data[used]=0;*length=used;}break;}
            if(count>limit-used)break;
            if(used+count+1>capacity){
                capacity=(used+count+1)*2;
                unsigned char *larger=realloc(data,capacity);
                if(!larger)break;
                data=larger;
            }
            memcpy(data+used,chunk,count);used+=count;
        }
    }
    if(request)WinHttpCloseHandle(request);
    if(host)WinHttpCloseHandle(host);
    WinHttpCloseHandle(session);
    if(!*length){free(data);return NULL;}
    return data;
}
static void catalog_decode(CatalogRequest *job,unsigned char *data,size_t bytes){
    HRESULT init=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if(FAILED(init))return;
    IWICImagingFactory *factory=NULL; IWICStream *stream=NULL;
    IWICBitmapDecoder *decoder=NULL;IWICBitmapFrameDecode *frame=NULL;
    IWICBitmapScaler *scaler=NULL;IWICFormatConverter *converter=NULL;
    UINT w=0,h=0;
    if(SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void**)&factory)) &&
       SUCCEEDED(IWICImagingFactory_CreateStream(factory,&stream)) &&
       SUCCEEDED(IWICStream_InitializeFromMemory(stream,data,(DWORD)bytes)) &&
       SUCCEEDED(IWICImagingFactory_CreateDecoderFromStream(factory,(IStream*)stream,NULL,WICDecodeMetadataCacheOnDemand,&decoder)) &&
       SUCCEEDED(IWICBitmapDecoder_GetFrame(decoder,0,&frame)) && SUCCEEDED(IWICBitmapFrameDecode_GetSize(frame,&w,&h)) &&
       w && h && (uint64_t)w*h<=40000000 && SUCCEEDED(IWICImagingFactory_CreateBitmapScaler(factory,&scaler))){
        double factor=fmin(1.0,512.0/fmax(w,h));
        job->w=(UINT)fmax(1,w*factor);job->h=(UINT)fmax(1,h*factor);
        UINT stride=job->w*4,n=stride*job->h;
        if(SUCCEEDED(IWICBitmapScaler_Initialize(scaler,(IWICBitmapSource*)frame,job->w,job->h,WICBitmapInterpolationModeFant)) &&
           SUCCEEDED(IWICImagingFactory_CreateFormatConverter(factory,&converter)) &&
           SUCCEEDED(IWICFormatConverter_Initialize(converter,(IWICBitmapSource*)scaler,&GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom))){
            job->pixels=malloc(n);
            if(job->pixels && FAILED(IWICFormatConverter_CopyPixels(converter,NULL,stride,n,job->pixels))){free(job->pixels);job->pixels=NULL;}
        }
    }
    if(converter)IWICFormatConverter_Release(converter);
    if(scaler)IWICBitmapScaler_Release(scaler);
    if(frame)IWICBitmapFrameDecode_Release(frame);
    if(decoder)IWICBitmapDecoder_Release(decoder);
    if(stream)IWICStream_Release(stream);
    if(factory)IWICImagingFactory_Release(factory);
    CoUninitialize();
}
static DWORD WINAPI catalog_worker(void *arg){
    CatalogRequest *job=arg;size_t length=0;
    unsigned char *data=catalog_get(job,&length);
    if(data){
        if(job->page)job->record=cJSON_ParseWithLength((const char*)data,length);
        else catalog_decode(job,data,length);
        free(data);
    }
    return 0;
}
static void catalog_start(CatalogRequest *job){
    job->thread=CreateThread(NULL,0,catalog_worker,job,0,NULL);
}
static int catalog_finished(CatalogRequest *job){
    if(!job->thread)return 1;
    if(WaitForSingleObject(job->thread,0)!=WAIT_OBJECT_0)return 0;
    CloseHandle(job->thread);job->thread=NULL;return 1;
}
