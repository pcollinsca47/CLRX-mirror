/*
 *  CLRadeonExtender - Unofficial OpenCL Radeon Extensions Library
 *  Copyright (C) 2014-2015 Mateusz Szpakowski
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <CLRX/Config.h>
#include <iostream>
#include <algorithm>
#include <exception>
#include <vector>
#include <utility>
#include <thread>
#include <mutex>
#include <cstring>
#include <string>
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <CLRX/utils/Utilities.h>
#include <CLRX/utils/Containers.h>
#include <CLRX/amdasm/Assembler.h>
#include <CLRX/amdbin/AmdBinaries.h>
#include <CLRX/utils/InputOutput.h>
#include <CLRX/utils/GPUId.h>
#include "CLWrapper.h"

using namespace CLRX;

std::once_flag clrxOnceFlag;
bool useCLRXWrapper = true;
static std::unique_ptr<DynLibrary> amdOclLibrary = nullptr;
cl_uint amdOclNumPlatforms = 0;
CLRXpfn_clGetPlatformIDs amdOclGetPlatformIDs = nullptr;
CLRXpfn_clUnloadCompiler amdOclUnloadCompiler = nullptr;
cl_int clrxWrapperInitStatus = CL_SUCCESS;

std::unique_ptr<CLRXPlatform[]> clrxPlatforms = nullptr;

#ifdef CL_VERSION_1_2
clEnqueueWaitSignalAMD_fn amdOclEnqueueWaitSignalAMD = nullptr;
clEnqueueWriteSignalAMD_fn amdOclEnqueueWriteSignalAMD = nullptr;
clEnqueueMakeBuffersResidentAMD_fn amdOclEnqueueMakeBuffersResidentAMD = nullptr;
#endif
CLRXpfn_clGetExtensionFunctionAddress amdOclGetExtensionFunctionAddress = nullptr;

/* extensions table - entries are sorted in function name's order */
CLRXExtensionEntry clrxExtensionsTable[18] =
{
    { "clCreateEventFromGLsyncKHR", (void*)clrxclCreateEventFromGLsyncKHR },
    { "clCreateFromGLBuffer", (void*)clrxclCreateFromGLBuffer },
    { "clCreateFromGLRenderbuffer", (void*)clrxclCreateFromGLRenderbuffer },
#ifdef CL_VERSION_1_2
    { "clCreateFromGLTexture", (void*)clrxclCreateFromGLTexture },
#else
    { "clCreateFromGLTexture", nullptr },
#endif
    { "clCreateFromGLTexture2D", (void*)clrxclCreateFromGLTexture2D },
    { "clCreateFromGLTexture3D", (void*)clrxclCreateFromGLTexture3D },
    { "clCreateSubDevicesEXT", (void*)clrxclCreateSubDevicesEXT },
    { "clEnqueueAcquireGLObjects", (void*)clrxclEnqueueAcquireGLObjects },
#ifdef CL_VERSION_1_2
    { "clEnqueueMakeBuffersResidentAMD", (void*)clrxclEnqueueMakeBuffersResidentAMD },
#else
    { "clEnqueueMakeBuffersResidentAMD", nullptr },
#endif
    { "clEnqueueReleaseGLObjects", (void*)clrxclEnqueueReleaseGLObjects },
#ifdef CL_VERSION_1_2
    { "clEnqueueWaitSignalAMD", (void*)clrxclEnqueueWaitSignalAMD },
    { "clEnqueueWriteSignalAMD", (void*)clrxclEnqueueWriteSignalAMD },
#else
    { "clEnqueueWaitSignalAMD", nullptr },
    { "clEnqueueWriteSignalAMD", nullptr },
#endif
    { "clGetGLContextInfoKHR", (void*)clrxclGetGLContextInfoKHR },
    { "clGetGLObjectInfo", (void*)clrxclGetGLObjectInfo },
    { "clGetGLTextureInfo", (void*)clrxclGetGLTextureInfo },
    { "clIcdGetPlatformIDsKHR", (void*)clrxclIcdGetPlatformIDsKHR },
    { "clReleaseDeviceEXT", (void*)clrxclReleaseDeviceEXT },
    { "clRetainDeviceEXT", (void*)clrxclRetainDeviceEXT }
};

/* dispatch structure */
const CLRXIcdDispatch clrxDispatchRecord =
{ {
    clrxclGetPlatformIDs,
    clrxclGetPlatformInfo,
    clrxclGetDeviceIDs,
    clrxclGetDeviceInfo,
    clrxclCreateContext,
    clrxclCreateContextFromType,
    clrxclRetainContext,
    clrxclReleaseContext,
    clrxclGetContextInfo,
    clrxclCreateCommandQueue,
    clrxclRetainCommandQueue,
    clrxclReleaseCommandQueue,
    clrxclGetCommandQueueInfo,
    clrxclSetCommandQueueProperty,
    clrxclCreateBuffer,
    clrxclCreateImage2D,
    clrxclCreateImage3D,
    clrxclRetainMemObject,
    clrxclReleaseMemObject,
    clrxclGetSupportedImageFormats,
    clrxclGetMemObjectInfo,
    clrxclGetImageInfo,
    clrxclCreateSampler,
    clrxclRetainSampler,
    clrxclReleaseSampler,
    clrxclGetSamplerInfo,
    clrxclCreateProgramWithSource,
    clrxclCreateProgramWithBinary,
    clrxclRetainProgram,
    clrxclReleaseProgram,
    clrxclBuildProgram,
    clrxclUnloadCompiler,
    clrxclGetProgramInfo,
    clrxclGetProgramBuildInfo,
    clrxclCreateKernel,
    clrxclCreateKernelsInProgram,
    clrxclRetainKernel,
    clrxclReleaseKernel,
    clrxclSetKernelArg,
    clrxclGetKernelInfo,
    clrxclGetKernelWorkGroupInfo,
    clrxclWaitForEvents,
    clrxclGetEventInfo,
    clrxclRetainEvent,
    clrxclReleaseEvent,
    clrxclGetEventProfilingInfo,
    clrxclFlush,
    clrxclFinish,
    clrxclEnqueueReadBuffer,
    clrxclEnqueueWriteBuffer,
    clrxclEnqueueCopyBuffer,
    clrxclEnqueueReadImage,
    clrxclEnqueueWriteImage,
    clrxclEnqueueCopyImage,
    clrxclEnqueueCopyImageToBuffer,
    clrxclEnqueueCopyBufferToImage,
    clrxclEnqueueMapBuffer,
    clrxclEnqueueMapImage,
    clrxclEnqueueUnmapMemObject,
    clrxclEnqueueNDRangeKernel,
    clrxclEnqueueTask,
    clrxclEnqueueNativeKernel,
    clrxclEnqueueMarker,
    clrxclEnqueueWaitForEvents,
    clrxclEnqueueBarrier,
    clrxclGetExtensionFunctionAddress,
    clrxclCreateFromGLBuffer,
    clrxclCreateFromGLTexture2D,
    clrxclCreateFromGLTexture3D,
    clrxclCreateFromGLRenderbuffer,
    clrxclGetGLObjectInfo,
    clrxclGetGLTextureInfo,
    clrxclEnqueueAcquireGLObjects,
    clrxclEnqueueReleaseGLObjects,
    clrxclGetGLContextInfoKHR,

    nullptr, // clGetDeviceIDsFromD3D10KHR,
    nullptr, // clCreateFromD3D10BufferKHR,
    nullptr, // clCreateFromD3D10Texture2DKHR,
    nullptr, // clCreateFromD3D10Texture3DKHR,
    nullptr, // clEnqueueAcquireD3D10ObjectsKHR,
    nullptr, // clEnqueueReleaseD3D10ObjectsKHR,

    clrxclSetEventCallback,
    clrxclCreateSubBuffer,
    clrxclSetMemObjectDestructorCallback,
    clrxclCreateUserEvent,
    clrxclSetUserEventStatus,
    clrxclEnqueueReadBufferRect,
    clrxclEnqueueWriteBufferRect,
    clrxclEnqueueCopyBufferRect,

    clrxclCreateSubDevicesEXT,
    clrxclRetainDeviceEXT,
    clrxclReleaseDeviceEXT,

    clrxclCreateEventFromGLsyncKHR

#ifdef CL_VERSION_1_2
    ,
    clrxclCreateSubDevices,
    clrxclRetainDevice,
    clrxclReleaseDevice,
    clrxclCreateImage,
    clrxclCreateProgramWithBuiltInKernels,
    clrxclCompileProgram,
    clrxclLinkProgram,
    clrxclUnloadPlatformCompiler,
    clrxclGetKernelArgInfo,
    clrxclEnqueueFillBuffer,
    clrxclEnqueueFillImage,
    clrxclEnqueueMigrateMemObjects,
    clrxclEnqueueMarkerWithWaitList,
    clrxclEnqueueBarrierWithWaitList,
    clrxclGetExtensionFunctionAddressForPlatform,
    clrxclCreateFromGLTexture,

    nullptr, // clGetDeviceIDsFromD3D11KHR,
    nullptr, // clCreateFromD3D11BufferKHR,
    nullptr, // clCreateFromD3D11Texture2DKHR,
    nullptr, // clCreateFromD3D11Texture3DKHR,
    nullptr, // clCreateFromDX9MediaSurfaceKHR,
    nullptr, // clEnqueueAcquireD3D11ObjectsKHR,
    nullptr, // clEnqueueReleaseD3D11ObjectsKHR,

    nullptr, // clGetDeviceIDsFromDX9MediaAdapterKHR,
    nullptr, // clEnqueueAcquireDX9MediaSurfacesKHR,
    nullptr // clEnqueueReleaseDX9MediaSurfacesKHR
#endif
#ifdef CL_VERSION_2_0
    ,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    clrxclCreateCommandQueueWithProperties,
    clrxclCreatePipe,
    clrxclGetPipeInfo,
    clrxclSVMAlloc,
    clrxclSVMFree,
    clrxclEnqueueSVMFree,
    clrxclEnqueueSVMMemcpy,
    clrxclEnqueueSVMMemFill,
    clrxclEnqueueSVMMap,
    clrxclEnqueueSVMUnmap,
    clrxclCreateSamplerWithProperties,
    clrxclSetKernelArgSVMPointer,
    clrxclSetKernelExecInfo
#endif
} };

void clrxReleaseOnlyCLRXDevice(CLRXDevice* device)
{
    if (device->parent != nullptr)
        if (device->refCount.fetch_sub(1) == 1)
        {   // amdOclDevice has been already released, we release only our device
            clrxReleaseOnlyCLRXDevice(device->parent);
            delete device;
        }
}

void clrxReleaseOnlyCLRXContext(CLRXContext* context)
{
    if (context->refCount.fetch_sub(1) == 1)
    {   // amdOclContext has been already released, we release only our context
        for (cl_uint i = 0; i < context->devicesNum; i++)
            clrxReleaseOnlyCLRXDevice(context->devices[i]);
        delete context;
    }
}

void clrxReleaseOnlyCLRXMemObject(CLRXMemObject* memObject)
{
    if (memObject->refCount.fetch_sub(1) == 1)
    {   // amdOclContext has been already released, we release only our context
        clrxReleaseOnlyCLRXContext(memObject->context);
        if (memObject->parent != nullptr)
            clrxReleaseOnlyCLRXMemObject(memObject->parent);
        if (memObject->buffer != nullptr)
            clrxReleaseOnlyCLRXMemObject(memObject->buffer);
        delete memObject;
    }
}

void clrxWrapperInitialize()
{
    std::unique_ptr<DynLibrary> tmpAmdOclLibrary = nullptr;
    try
    {
        useCLRXWrapper = !parseEnvVariable<bool>("CLRX_FORCE_ORIGINAL_AMDOCL", false);
        std::string amdOclPath = parseEnvVariable<std::string>("CLRX_AMDOCL_PATH",
                           DEFAULT_AMDOCLPATH);
        tmpAmdOclLibrary.reset(new DynLibrary(amdOclPath.c_str(), DYNLIB_NOW));
        
        amdOclGetPlatformIDs = (CLRXpfn_clGetPlatformIDs)
                tmpAmdOclLibrary->getSymbol("clGetPlatformIDs");
        if (amdOclGetPlatformIDs == nullptr)
            throw Exception("AMDOCL clGetPlatformIDs have invalid value!");
        
        try
        { amdOclUnloadCompiler = (CLRXpfn_clUnloadCompiler)
            tmpAmdOclLibrary->getSymbol("clUnloadCompiler"); }
        catch(const CLRX::Exception& ex)
        { /* ignore if not found */ }
        
        amdOclGetExtensionFunctionAddress = (CLRXpfn_clGetExtensionFunctionAddress)
                tmpAmdOclLibrary->getSymbol("clGetExtensionFunctionAddress");
        if (amdOclGetExtensionFunctionAddress == nullptr)
            throw Exception("AMDOCL clGetExtensionFunctionAddress have invalid value!");
        
        const pfn_clIcdGetPlatformIDs pgetPlatformIDs = (pfn_clIcdGetPlatformIDs)
                amdOclGetExtensionFunctionAddress("clIcdGetPlatformIDsKHR");
        if (amdOclGetExtensionFunctionAddress == nullptr)
            throw Exception("AMDOCL clIcdGetPlatformIDsKHR have invalid value!");
        
        /* specific amd extensions functions */
#ifdef CL_VERSION_1_2
        amdOclEnqueueWaitSignalAMD = (clEnqueueWaitSignalAMD_fn)
            amdOclGetExtensionFunctionAddress("clEnqueueWaitSignalAMD");
        amdOclEnqueueWriteSignalAMD = (clEnqueueWriteSignalAMD_fn)
            amdOclGetExtensionFunctionAddress("clEnqueueWriteSignalAMD");
        amdOclEnqueueMakeBuffersResidentAMD = (clEnqueueMakeBuffersResidentAMD_fn)
            amdOclGetExtensionFunctionAddress("clEnqueueMakeBuffersResidentAMD");
#endif
        
        /* update clrxExtensionsTable */
        for (CLRXExtensionEntry& extEntry: clrxExtensionsTable)
            // erase CLRX extension entry if not reflected in AMD extensions
            if (amdOclGetExtensionFunctionAddress(extEntry.funcname) == nullptr)
                extEntry.address = nullptr;
        /* end of clrxExtensionsTable */
        
        cl_uint platformCount;
        cl_int status = pgetPlatformIDs(0, nullptr, &platformCount);
        if (status != CL_SUCCESS)
        {
            clrxWrapperInitStatus = status;
            return;
        }
        
        if (platformCount == 0)
            return;
        
        std::vector<cl_platform_id> amdOclPlatforms(platformCount);
        std::fill(amdOclPlatforms.begin(), amdOclPlatforms.end(), nullptr);
        
        status = pgetPlatformIDs(platformCount, amdOclPlatforms.data(), nullptr);
        if (status != CL_SUCCESS)
        {
            clrxWrapperInitStatus = status;
            return;
        }
        
        if (useCLRXWrapper)
        {
            const cxuint clrxExtEntriesNum =
                    sizeof(clrxExtensionsTable)/sizeof(CLRXExtensionEntry);
            
            clrxPlatforms.reset(new CLRXPlatform[platformCount]);
            for (cl_uint i = 0; i < platformCount; i++)
            {
                if (amdOclPlatforms[i] == nullptr)
                    continue;
                
                const cl_platform_id amdOclPlatform = amdOclPlatforms[i];
                CLRXPlatform& clrxPlatform = clrxPlatforms[i];
                
                /* clrxPlatform init */
                clrxPlatform.amdOclPlatform = amdOclPlatform;
                clrxPlatform.dispatch = new CLRXIcdDispatch;
                ::memcpy(clrxPlatform.dispatch, &clrxDispatchRecord,
                         sizeof(CLRXIcdDispatch));
                
                // add to extensions "cl_radeon_extender"
                size_t extsSize;
                if (amdOclPlatform->dispatch->clGetPlatformInfo(amdOclPlatform,
                            CL_PLATFORM_EXTENSIONS, 0, nullptr, &extsSize) != CL_SUCCESS)
                    continue;
                std::unique_ptr<char[]> extsBuffer(new char[extsSize + 19]);
                if (amdOclPlatform->dispatch->clGetPlatformInfo(amdOclPlatform,
                        CL_PLATFORM_EXTENSIONS, extsSize, extsBuffer.get(),
                        nullptr) != CL_SUCCESS)
                    continue;
                if (extsSize > 2 && extsBuffer[extsSize-2] != ' ')
                    ::strcat(extsBuffer.get(), " cl_radeon_extender");
                else
                    ::strcat(extsBuffer.get(), "cl_radeon_extender");
                clrxPlatform.extensionsSize = ::strlen(extsBuffer.get())+1;
                clrxPlatform.extensions = std::move(extsBuffer);
                
                // add to version " (clrx 0.0)"
                size_t versionSize;
                if (amdOclPlatform->dispatch->clGetPlatformInfo(amdOclPlatform,
                            CL_PLATFORM_VERSION, 0, nullptr, &versionSize) != CL_SUCCESS)
                    continue;
                std::unique_ptr<char[]> versionBuffer(new char[versionSize+20]);
                if (amdOclPlatform->dispatch->clGetPlatformInfo(amdOclPlatform,
                        CL_PLATFORM_VERSION, versionSize,
                        versionBuffer.get(), nullptr) != CL_SUCCESS)
                    continue;
                ::strcat(versionBuffer.get(), " (clrx 0.0)");
                clrxPlatform.versionSize = ::strlen(versionBuffer.get())+1;
                clrxPlatform.version = std::move(versionBuffer);
                
                /* parse OpenCL version */
                cxuint openCLmajor = 0;
                cxuint openCLminor = 0;
                const char* verEnd = nullptr;
                if (::strncmp(clrxPlatform.version.get(), "OpenCL ", 7) == 0)
                {
                    try
                    {
                        openCLmajor = CLRX::cstrtoui(
                                clrxPlatform.version.get() + 7, nullptr, verEnd);
                        if (verEnd != nullptr && *verEnd == '.')
                        {
                            const char* verEnd2 = nullptr;
                            openCLminor = CLRX::cstrtoui(verEnd + 1, nullptr, verEnd2);
                            if (openCLmajor > UINT16_MAX || openCLminor > UINT16_MAX)
                                openCLmajor = openCLminor = 0; // if failed
                        }
                    }
                    catch(const ParseException& ex)
                    { /* ignore parse exception */ }
                    clrxPlatform.openCLVersionNum = getOpenCLVersionNum(
                                    openCLmajor, openCLminor);
                }
                /*std::cout << "Platform OCL version: " <<
                        openCLmajor << "." << openCLminor << std::endl;*/
                
                /* initialize extTable */
                clrxPlatform.extEntries.reset(new CLRXExtensionEntry[clrxExtEntriesNum]);
                std::copy(clrxExtensionsTable, clrxExtensionsTable + clrxExtEntriesNum,
                          clrxPlatform.extEntries.get());
                
                if (clrxPlatform.openCLVersionNum >= getOpenCLVersionNum(1, 2))
                {   /* update p->extEntries for platform */
                    for (size_t k = 0; k < clrxExtEntriesNum; k++)
                    {   // erase CLRX extension entry if not reflected in AMD extensions
                        CLRXExtensionEntry& extEntry = clrxPlatform.extEntries[k];
#ifdef CL_VERSION_1_2
                        if (amdOclPlatform->dispatch->
                            clGetExtensionFunctionAddressForPlatform
                                    (amdOclPlatform, extEntry.funcname) == nullptr)
                            extEntry.address = nullptr;
#else
                        if (amdOclGetExtensionFunctionAddress(extEntry.funcname) == nullptr)
                            extEntry.address = nullptr;
#endif
                    }
                    /* end of p->extEntries for platform */
                }
                
                /* filtering for different OpenCL standards */
                size_t icdEntriesToKept = CLRXICD_ENTRIES_NUM;
                if (clrxPlatform.openCLVersionNum < getOpenCLVersionNum(1, 1))
                    // if earlier than OpenCL 1.1
                    icdEntriesToKept =
                            offsetof(CLRXIcdDispatch, clSetEventCallback)/sizeof(void*);
#ifdef CL_VERSION_1_2
                else if (clrxPlatform.openCLVersionNum < getOpenCLVersionNum(1, 2))
                    // if earlier than OpenCL 1.2
                    icdEntriesToKept =
                            offsetof(CLRXIcdDispatch, clCreateSubDevices)/sizeof(void*);
#endif
#ifdef CL_VERSION_2_0
                else if (clrxPlatform.openCLVersionNum < getOpenCLVersionNum(2, 0))
                    // if earlier than OpenCL 2.0
                    icdEntriesToKept =
                            offsetof(CLRXIcdDispatch, emptyFunc119)/sizeof(void*);
#endif
                
                // zeroing unsupported entries for later version of OpenCL standard
                std::fill(clrxPlatform.dispatch->entries + icdEntriesToKept,
                          clrxPlatform.dispatch->entries + CLRXICD_ENTRIES_NUM, nullptr);
                
                for (size_t k = 0; k < icdEntriesToKept; k++)
                    /* disable function when not in original driver */
                    if (amdOclPlatform->dispatch->entries[k] == nullptr)
                        clrxPlatform.dispatch->entries[k] = nullptr;
            }
        }
        
        amdOclLibrary = std::move(tmpAmdOclLibrary);
        amdOclNumPlatforms = platformCount;
    }
    catch(const std::bad_alloc& ex)
    {
        clrxPlatforms = nullptr;
        amdOclLibrary = nullptr;
        amdOclNumPlatforms = 0;
        clrxWrapperInitStatus = CL_OUT_OF_HOST_MEMORY;
        return;
    }
    catch(...)
    {
        clrxPlatforms = nullptr;
        amdOclLibrary = nullptr;
        amdOclNumPlatforms = 0;
        throw; // fatal exception
    }
}

struct ManCLContext
{
    cl_context clContext;
    
    ManCLContext() : clContext(nullptr) { }
    ~ManCLContext()
    {
        if (clContext != nullptr)
            if (clContext->dispatch->clReleaseContext(clContext) != CL_SUCCESS)
            {
                std::cerr << "Can't release amdOfflineContext!" << std::endl;
                abort();
            }
    }
    cl_context operator()()
    { return clContext; }
    
    ManCLContext& operator=(cl_context c)
    {
        clContext = c;
        return *this;
    }
};

void clrxPlatformInitializeDevices(CLRXPlatform* platform)
{
    cl_int status = platform->amdOclPlatform->dispatch->clGetDeviceIDs(
            platform->amdOclPlatform, CL_DEVICE_TYPE_ALL, 0, nullptr,
            &platform->devicesNum);
    
    if (status != CL_SUCCESS && status != CL_DEVICE_NOT_FOUND)
    {
        platform->devicesNum = 0;
        platform->deviceInitStatus = status;
        return;
    }
    if (status == CL_DEVICE_NOT_FOUND)
        platform->devicesNum = 0; // reset
    
    /* check whether OpenCL 1.2 or later */
#ifdef CL_VERSION_1_2
    bool isOCL12OrLater = platform->openCLVersionNum >= getOpenCLVersionNum(1, 2);
    //std::cout << "IsOCl12OrLater: " << isOCL12OrLater << std::endl;
    
    ManCLContext offlineContext;
    cl_uint amdOfflineDevicesNum = 0;
    cl_uint offlineContextDevicesNum = 0;
    /* custom devices not listed in all devices */
    cl_uint customDevicesNum = 0;
    if (isOCL12OrLater)
    {
        status = platform->amdOclPlatform->dispatch->clGetDeviceIDs(
                platform->amdOclPlatform, CL_DEVICE_TYPE_CUSTOM, 0, nullptr,
                &customDevicesNum);
        
        if (status != CL_SUCCESS && status != CL_DEVICE_NOT_FOUND)
        {
            platform->devicesNum = 0;
            platform->deviceInitStatus = status;
            return;
        }
        
        if (status == CL_SUCCESS) // if some devices
            platform->devicesNum += customDevicesNum;
        else // status == CL_DEVICE_NOT_FOUND
            customDevicesNum = 0;
        
        if (::strstr(platform->extensions.get(), "cl_amd_offline_devices") != nullptr)
        {   // if amd offline devices is available
            cl_context_properties clctxprops[5];
            clctxprops[0] = CL_CONTEXT_PLATFORM;
            clctxprops[1] = (cl_context_properties)platform->amdOclPlatform;
            clctxprops[2] = CL_CONTEXT_OFFLINE_DEVICES_AMD;
            clctxprops[3] = (cl_context_properties)1;
            clctxprops[4] = 0;
            offlineContext = platform->amdOclPlatform->dispatch->
                clCreateContextFromType(clctxprops, CL_DEVICE_TYPE_ALL, NULL, NULL, &status);
            
            if (offlineContext() == nullptr)
            {
                platform->devicesNum = 0;
                platform->deviceInitStatus = status;
                return;
            }
            
            status = offlineContext()->dispatch->clGetContextInfo(offlineContext(),
                    CL_CONTEXT_NUM_DEVICES, sizeof(cl_uint),
                    &offlineContextDevicesNum, nullptr);
            
            if (status == CL_SUCCESS &&
                offlineContextDevicesNum > (platform->devicesNum - customDevicesNum))
            {
                amdOfflineDevicesNum = offlineContextDevicesNum -
                        (platform->devicesNum - customDevicesNum);
                platform->devicesNum += amdOfflineDevicesNum;
            }
            else // no additional offline devices
                amdOfflineDevicesNum = 0;
        }
    }
#else
    cl_uint customDevicesNum = 0;
    cl_uint amdOfflineDevicesNum = 0;
#endif
    
    try
    {
        std::vector<cl_device_id> amdDevices(platform->devicesNum);
        platform->devicesArray = new CLRXDevice[platform->devicesNum];
    
        /* get amd devices */
        status = platform->amdOclPlatform->dispatch->clGetDeviceIDs(
                platform->amdOclPlatform, CL_DEVICE_TYPE_ALL,
                platform->devicesNum-customDevicesNum-amdOfflineDevicesNum,
                amdDevices.data(), nullptr);
        if (status != CL_SUCCESS)
        {
            delete[] platform->devicesArray;
            platform->devicesNum = 0;
            platform->devicesArray = nullptr;
            platform->deviceInitStatus = status;
            return;
        }
        // custom devices
#ifdef CL_VERSION_1_2
        if (customDevicesNum != 0)
        {
            status = platform->amdOclPlatform->dispatch->clGetDeviceIDs(
                    platform->amdOclPlatform, CL_DEVICE_TYPE_CUSTOM, customDevicesNum,
                    amdDevices.data() + platform->devicesNum-
                        customDevicesNum-amdOfflineDevicesNum, nullptr);
            if (status != CL_SUCCESS)
            {
                delete[] platform->devicesArray;
                platform->devicesNum = 0;
                platform->devicesArray = nullptr;
                platform->deviceInitStatus = status;
                return;
            }
        }
        if (amdOfflineDevicesNum != 0)
        {
            std::vector<cl_device_id> offlineDevices(offlineContextDevicesNum);
            status = offlineContext()->dispatch->clGetContextInfo(offlineContext(),
                    CL_CONTEXT_DEVICES, sizeof(cl_device_id)*offlineContextDevicesNum,
                    offlineDevices.data(), nullptr);
            
            if (status != CL_SUCCESS)
            {
                delete[] platform->devicesArray;
                platform->devicesNum = 0;
                platform->devicesArray = nullptr;
                platform->deviceInitStatus = status;
                return;
            }
            /* filter: only put unavailable devices (offline) */
            cl_uint k = platform->devicesNum-amdOfflineDevicesNum;
            /* using std::vector, some strange fails on Catalyst 15.7 when
             * NBody tries to dump kernel code */
            std::vector<cl_device_id> normalDevices(
                        amdDevices.begin(), amdDevices.begin()+k);
            std::sort(normalDevices.begin(), normalDevices.end());
            for (cl_device_id deviceId: offlineDevices)
            {   /* broken CL_DEVICE_AVAILABLE in latest driver (Catalyst 15.7) */
                //if (!available)
                if (binaryFind(normalDevices.begin(), normalDevices.end(), deviceId)
                            == normalDevices.end())
                    amdDevices[k++] = deviceId;
            }
        }
#endif
        
        for (cl_uint i = 0; i < platform->devicesNum; i++)
        {
            CLRXDevice& clrxDevice = platform->devicesArray[i];
            clrxDevice.dispatch = platform->dispatch;
            clrxDevice.amdOclDevice = amdDevices[i];
            clrxDevice.platform = platform;
            
            cl_device_type devType;
            status = amdDevices[i]->dispatch->clGetDeviceInfo(amdDevices[i], CL_DEVICE_TYPE,
                        sizeof(cl_device_type), &devType, nullptr);
            if (status != CL_SUCCESS)
                break;
            
            if ((devType & CL_DEVICE_TYPE_GPU) == 0)
                continue; // do not change extensions if not gpu
            
            // add to extensions "cl_radeon_extender"
            size_t extsSize;
            status = amdDevices[i]->dispatch->clGetDeviceInfo(amdDevices[i],
                      CL_DEVICE_EXTENSIONS, 0, nullptr, &extsSize);
            
            if (status != CL_SUCCESS)
                break;
            std::unique_ptr<char[]> extsBuffer(new char[extsSize+19]);
            status = amdDevices[i]->dispatch->clGetDeviceInfo(amdDevices[i],
                  CL_DEVICE_EXTENSIONS, extsSize, extsBuffer.get(), nullptr);
            if (status != CL_SUCCESS)
                break;
            if (extsSize > 2 && extsBuffer[extsSize-2] != ' ')
                strcat(extsBuffer.get(), " cl_radeon_extender");
            else
                strcat(extsBuffer.get(), "cl_radeon_extender");
            clrxDevice.extensionsSize = ::strlen(extsBuffer.get())+1;
            clrxDevice.extensions = std::move(extsBuffer);
            
            // add to version " (clrx 0.0)"
            size_t versionSize;
            status = amdDevices[i]->dispatch->clGetDeviceInfo(amdDevices[i],
                      CL_DEVICE_VERSION, 0, nullptr, &versionSize);
            if (status != CL_SUCCESS)
                break;
            std::unique_ptr<char[]> versionBuffer(new char[versionSize+20]);
            status = amdDevices[i]->dispatch->clGetDeviceInfo(amdDevices[i],
                      CL_DEVICE_VERSION, versionSize, versionBuffer.get(), nullptr);
            if (status != CL_SUCCESS)
                break;
            ::strcat(versionBuffer.get(), " (clrx 0.0)");
            clrxDevice.versionSize = ::strlen(versionBuffer.get())+1;
            clrxDevice.version = std::move(versionBuffer);
        }
        
        if (status != CL_SUCCESS)
        {
            delete[] platform->devicesArray;
            platform->devicesNum = 0;
            platform->devicesArray = nullptr;
            platform->deviceInitStatus = status;
            return;
        }
        // init device pointers
        platform->devicePtrs.reset(new CLRXDevice*[platform->devicesNum]);
        for (cl_uint i = 0; i < platform->devicesNum; i++)
            platform->devicePtrs[i] = platform->devicesArray + i;
    }
    catch(const std::bad_alloc& ex)
    {
        delete[] platform->devicesArray;
        platform->devicesNum = 0;
        platform->devicesArray = nullptr;
        platform->deviceInitStatus = CL_OUT_OF_HOST_MEMORY;
        return;
    }
    catch(...)
    {
        delete[] platform->devicesArray;
        platform->devicesNum = 0;
        platform->devicesArray = nullptr;
        throw;
    }
}

/*
 * clrx object utils
 */

static inline bool clrxDeviceCompareByAmdDevice(const CLRXDevice* l, const CLRXDevice* r)
{
    return l->amdOclDevice < r->amdOclDevice;
}

void translateAMDDevicesIntoCLRXDevices(cl_uint allDevicesNum,
           const CLRXDevice** allDevices, cl_uint amdDevicesNum, cl_device_id* amdDevices)
{
    /* after it we replaces amdDevices into ours devices */
    if (allDevicesNum < 16)  //efficient for small
    {   // 
        for (cl_uint i = 0; i < amdDevicesNum; i++)
        {
            cl_uint j;
            for (j = 0; j < allDevicesNum; j++)
                if (amdDevices[i] == allDevices[j]->amdOclDevice)
                {
                    amdDevices[i] = const_cast<CLRXDevice**>(allDevices)[j];
                    break;
                }
                
            if (j == allDevicesNum)
            {
                std::cerr << "Fatal error at translating AMD devices" << std::endl;
                abort();
            }
        }
    }
    else if(amdDevicesNum != 0) // sorting
    {
        std::vector<const CLRXDevice*> sortedOriginal(allDevices,
                 allDevices + allDevicesNum);
        std::sort(sortedOriginal.begin(), sortedOriginal.end(),
                  clrxDeviceCompareByAmdDevice);
        auto newEnd = std::unique(sortedOriginal.begin(), sortedOriginal.end());
        
        CLRXDevice tmpDevice;
        for (cl_uint i = 0; i < amdDevicesNum; i++)
        {
            tmpDevice.amdOclDevice = amdDevices[i];
            const auto& found = binaryFind(sortedOriginal.begin(),
                     newEnd, &tmpDevice, clrxDeviceCompareByAmdDevice);
            if (found != newEnd)
                amdDevices[i] = (cl_device_id)(*found);
            else
            {
                std::cerr << "Fatal error at translating AMD devices" << std::endl;
                abort();
            }
        }
    }
}

/* called always on creating context */
cl_int clrxSetContextDevices(CLRXContext* c, const CLRXPlatform* platform)
{
    cl_uint amdDevicesNum;
    cl_int status = c->amdOclContext->dispatch->clGetContextInfo(c->amdOclContext,
        CL_CONTEXT_NUM_DEVICES, sizeof(cl_uint), &amdDevicesNum, nullptr);
    if (status != CL_SUCCESS)
        return status;
    
    std::unique_ptr<cl_device_id[]> amdDevices(new cl_device_id[amdDevicesNum]);
    status = c->amdOclContext->dispatch->clGetContextInfo(c->amdOclContext,
            CL_CONTEXT_DEVICES, sizeof(cl_device_id)*amdDevicesNum,
            amdDevices.get(), nullptr);
    if (status == CL_OUT_OF_HOST_MEMORY)
        return status;
    if (status != CL_SUCCESS)
        return status;

    try
    {
        translateAMDDevicesIntoCLRXDevices(platform->devicesNum,
               (const CLRXDevice**)(platform->devicePtrs.get()), amdDevicesNum,
               amdDevices.get());
        // now is ours devices
        c->devicesNum = amdDevicesNum;
        c->devices.reset(reinterpret_cast<CLRXDevice**>(amdDevices.release()));
    }
    catch(const std::bad_alloc& ex)
    { return CL_OUT_OF_HOST_MEMORY; }
    return CL_SUCCESS;
}

cl_int clrxSetContextDevices(CLRXContext* c, cl_uint inDevicesNum,
            const cl_device_id* inDevices)
{
    cl_uint amdDevicesNum;
    
    cl_int status = c->amdOclContext->dispatch->clGetContextInfo(c->amdOclContext,
        CL_CONTEXT_NUM_DEVICES, sizeof(cl_uint), &amdDevicesNum, nullptr);
    if (status != CL_SUCCESS)
        return status;
    
    std::unique_ptr<cl_device_id> amdDevices(new cl_device_id[amdDevicesNum]);
    status = c->amdOclContext->dispatch->clGetContextInfo(c->amdOclContext,
            CL_CONTEXT_DEVICES, sizeof(cl_device_id)*amdDevicesNum,
            amdDevices.get(), nullptr);
    if (status == CL_OUT_OF_HOST_MEMORY)
        return status;
    if (status != CL_SUCCESS)
        return status;
    
    try
    {
        translateAMDDevicesIntoCLRXDevices(inDevicesNum, (const CLRXDevice**)inDevices,
                       amdDevicesNum, amdDevices.get());
        // now is ours devices
        c->devicesNum = amdDevicesNum;
        c->devices.reset(reinterpret_cast<CLRXDevice**>(amdDevices.release()));
    }
    catch(const std::bad_alloc& ex)
    { return CL_OUT_OF_HOST_MEMORY; }
    return CL_SUCCESS;
}

cl_int clrxUpdateProgramAssocDevices(CLRXProgram* p)
{
    size_t amdAssocDevicesNum;
    try
    {
        cl_uint totalDevicesNum = (p->transDevicesMap != nullptr) ?
                    p->transDevicesMap->size() : p->context->devicesNum;
        
        std::unique_ptr<cl_device_id[]> amdAssocDevices(new cl_device_id[totalDevicesNum]);
        cl_program amdProg = (p->amdOclAsmProgram!=nullptr) ? p->amdOclAsmProgram :
                p->amdOclProgram;
        
        // single OpenCL call should be atomic:
        // reason: can be called between clBuildProgram which changes associated devices
        const cl_int status = p->amdOclProgram->dispatch->clGetProgramInfo(
            amdProg, CL_PROGRAM_DEVICES, sizeof(cl_device_id)*totalDevicesNum,
                      amdAssocDevices.get(), &amdAssocDevicesNum);
        
        if (status != CL_SUCCESS)
            return status;
        
        amdAssocDevicesNum /= sizeof(cl_device_id); // number of amd devices
        
        if (totalDevicesNum != amdAssocDevicesNum)
        {   /* reallocate amdAssocDevices */
            std::unique_ptr<cl_device_id[]> tmpAmdAssocDevices(
                        new cl_device_id[amdAssocDevicesNum]);
            std::copy(amdAssocDevices.get(), amdAssocDevices.get()+amdAssocDevicesNum,
                      tmpAmdAssocDevices.get());
            amdAssocDevices = std::move(tmpAmdAssocDevices);
        }
        
        /* compare with previous assocDevices */
        if (p->assocDevicesNum == amdAssocDevicesNum && p->assocDevices != nullptr)
        {
            bool haveDiffs = false;
            for (cl_uint i = 0; i < amdAssocDevicesNum; i++)
                if (static_cast<CLRXDevice*>(p->assocDevices[i])->amdOclDevice !=
                    amdAssocDevices[i])
                {
                    haveDiffs = true;
                    break;
                }
            if (!haveDiffs) // no differences between calls
                return CL_SUCCESS;
        }
        
        if (p->transDevicesMap != nullptr)
            for (cl_uint i = 0; i < amdAssocDevicesNum; i++)
            {   // translate AMD device to CLRX device
                CLRXProgramDevicesMap::const_iterator found =
                        p->transDevicesMap->find(amdAssocDevices[i]);
                if (found == p->transDevicesMap->end())
                {
                    std::cerr << "Fatal error at translating AMD devices" << std::endl;
                    abort();
                }
                amdAssocDevices[i] = found->second;
            }
        else
            translateAMDDevicesIntoCLRXDevices(p->context->devicesNum,
                   const_cast<const CLRXDevice**>(p->context->devices.get()),
                   amdAssocDevicesNum, static_cast<cl_device_id*>(amdAssocDevices.get()));
        
        p->assocDevicesNum = amdAssocDevicesNum;
        p->assocDevices.reset(reinterpret_cast<CLRXDevice**>(amdAssocDevices.release()));
    }
    catch(const std::bad_alloc& ex)
    { return CL_OUT_OF_HOST_MEMORY; }
    return CL_SUCCESS;
}

void clrxBuildProgramNotifyWrapper(cl_program program, void * user_data)
{
    CLRXBuildProgramUserData* wrappedDataPtr =
            static_cast<CLRXBuildProgramUserData*>(user_data);
    CLRXBuildProgramUserData wrappedData = *wrappedDataPtr;
    CLRXProgram* p = wrappedDataPtr->clrxProgram;
    try
    {
        std::lock_guard<std::mutex> l(p->mutex);
        if (!wrappedDataPtr->inClFunction)
        {   // do it if not done in clBuildProgram
            const cl_int newStatus = clrxUpdateProgramAssocDevices(p);
            if (newStatus != CL_SUCCESS)
            {
                std::cerr << "Fatal error: cant update programAssocDevices" << std::endl;
                abort();
            }
            clrxReleaseConcurrentBuild(p);
        }
        wrappedDataPtr->callDone = true;
        if (wrappedDataPtr->inClFunction) // delete if done in clBuildProgram
        {
            //std::cout << "Delete WrappedData: " << wrappedDataPtr << std::endl;
            delete wrappedDataPtr;
        }
    }
    catch(const std::exception& ex)
    {
        std::cerr << "Fatal exception happened: " << ex.what() << std::endl;
        abort();
    }
    
    // must be called only once (freeing wrapped data)
    wrappedData.realNotify(wrappedData.clrxProgram, wrappedData.realUserData);
}

void clrxLinkProgramNotifyWrapper(cl_program program, void * user_data)
{
    CLRXLinkProgramUserData* wrappedDataPtr =
            static_cast<CLRXLinkProgramUserData*>(user_data);
    
    bool initializedByCallback = false;
    cl_program clrxProgram = nullptr;
    void* realUserData = nullptr;
    try
    {
        std::lock_guard<std::mutex> l(wrappedDataPtr->mutex);
        if (!wrappedDataPtr->clrxProgramFilled)
        {
            initializedByCallback = true;
            CLRXProgram* outProgram = nullptr;
            if (program != nullptr)
            {
                outProgram = new CLRXProgram;
                outProgram->dispatch = wrappedDataPtr->clrxContext->dispatch;
                outProgram->amdOclProgram = program;
                outProgram->context = wrappedDataPtr->clrxContext;
                outProgram->transDevicesMap = wrappedDataPtr->transDevicesMap;
                clrxUpdateProgramAssocDevices(outProgram);
                outProgram->transDevicesMap = nullptr;
                clrxRetainOnlyCLRXContext(wrappedDataPtr->clrxContext);
            }
            wrappedDataPtr->clrxProgram = outProgram;
            wrappedDataPtr->clrxProgramFilled = true;
        }
        clrxProgram = wrappedDataPtr->clrxProgram;
        realUserData = wrappedDataPtr->realUserData;
    }
    catch(std::bad_alloc& ex)
    {
        std::cerr << "Out of memory on link callback" << std::endl;
        abort();
    }
    catch(const std::exception& ex)
    {
        std::cerr << "Fatal exception happened: " << ex.what() << std::endl;
        abort();
    }
    
    void (*realNotify)(cl_program program, void * user_data) = wrappedDataPtr->realNotify;
    if (!initializedByCallback) // if not initialized by this callback to delete
    {
        delete wrappedDataPtr->transDevicesMap;
        delete wrappedDataPtr;
    }
    
    realNotify(clrxProgram, realUserData);
}

CLRXProgram* clrxCreateCLRXProgram(CLRXContext* c, cl_program amdProgram,
          cl_int* errcode_ret)
{
    CLRXProgram* outProgram = nullptr;
    cl_int error = CL_SUCCESS;
    try
    {
        outProgram = new CLRXProgram;
        outProgram->dispatch = c->dispatch;
        outProgram->amdOclProgram = amdProgram;
        outProgram->context = c;
        error = clrxUpdateProgramAssocDevices(outProgram);
    }
    catch(const std::bad_alloc& ex)
    { error = CL_OUT_OF_HOST_MEMORY; }
    
    if (error != CL_SUCCESS)
    {
        delete outProgram;
        if (c->amdOclContext->dispatch->clReleaseProgram(amdProgram) != CL_SUCCESS)
        {
            std::cerr << "Fatal Error at handling error at program creation!" << std::endl;
            abort();
        }
        if (errcode_ret != nullptr)
            *errcode_ret = error;
        return nullptr;
    }
    
    clrxRetainOnlyCLRXContext(c);
    return outProgram;
}

cl_int clrxApplyCLRXEvent(CLRXCommandQueue* q, cl_event* event,
             cl_event amdEvent, cl_int status)
{
    CLRXEvent* outEvent = nullptr;
    if (event != nullptr && amdEvent != nullptr)
    {  // create event
        try
        {
            outEvent = new CLRXEvent;
            outEvent->dispatch = q->dispatch;
            outEvent->amdOclEvent = amdEvent;
            outEvent->commandQueue = q;
            outEvent->context = q->context;
            *event = outEvent;
        }
        catch (const std::bad_alloc& ex)
        {
            if (q->amdOclCommandQueue->dispatch->clReleaseEvent(amdEvent) != CL_SUCCESS)
            {
                std::cerr <<
                    "Fatal Error at handling error at apply event!" << std::endl;
                abort();
            }
            return CL_OUT_OF_HOST_MEMORY;
        }
        clrxRetainOnlyCLRXContext(q->context);
        clrxRetainOnlyCLRXCommandQueue(q);
    }
    
    return status;
}

cl_int clrxCreateOutDevices(CLRXDevice* d, cl_uint devicesNum,
       cl_device_id* out_devices, cl_int (*AMDReleaseDevice)(cl_device_id),
       const char* fatalErrorMessage)
{
    cl_uint dp = 0;
    try
    {
        for (dp = 0; dp < devicesNum; dp++)
        {
            CLRXDevice* device = new CLRXDevice;
            device->dispatch = d->dispatch;
            device->amdOclDevice = out_devices[dp];
            device->platform = d->platform;
            device->parent = d;
            if (d->extensionsSize != 0)
            {
                device->extensionsSize = d->extensionsSize;
                device->extensions.reset(new char[d->extensionsSize]);
                ::memcpy(device->extensions.get(), d->extensions.get(), d->extensionsSize);
            }
            if (d->versionSize != 0)
            {
                device->versionSize = d->versionSize;
                device->version.reset(new char[d->versionSize]);
                ::memcpy(device->version.get(), d->version.get(), d->versionSize);
            }
            out_devices[dp] = device;
        }
    }
    catch(const std::bad_alloc& ex)
    {   // revert translation
        for (cl_uint i = 0; i < dp; i++)
        {
            CLRXDevice* d = static_cast<CLRXDevice*>(out_devices[i]);
            out_devices[i] = d->amdOclDevice;
            delete d;
        }
        // free all subdevices
        for (cl_uint i = 0; i < devicesNum; i++)
        {
            if (AMDReleaseDevice(out_devices[i]) != CL_SUCCESS)
            {
                std::cerr << fatalErrorMessage << std::endl;
                abort();
            }
        }
        return CL_OUT_OF_HOST_MEMORY;
    }
    return CL_SUCCESS;
}

void clrxEventCallbackWrapper(cl_event event, cl_int exec_status, void * user_data)
{
    CLRXEventCallbackUserData* wrappedDataPtr =
            static_cast<CLRXEventCallbackUserData*>(user_data);
    CLRXEventCallbackUserData wrappedData = *wrappedDataPtr;
    // must be called only once (freeing wrapped data)
    delete wrappedDataPtr;
    wrappedData.realNotify(wrappedData.clrxEvent, exec_status,
                wrappedData.realUserData);
}

void clrxMemDtorCallbackWrapper(cl_mem memobj, void * user_data)
{
    CLRXMemDtorCallbackUserData* wrappedDataPtr =
            static_cast<CLRXMemDtorCallbackUserData*>(user_data);
    CLRXMemDtorCallbackUserData wrappedData = *wrappedDataPtr;
    // must be called only once (freeing wrapped data)
    delete wrappedDataPtr;
    wrappedData.realNotify(wrappedData.clrxMemObject, wrappedData.realUserData);
}

#ifdef CL_VERSION_2_0
void clrxSVMFreeCallbackWrapper(cl_command_queue queue,
      cl_uint num_svm_pointers, void** svm_pointers, void* user_data)
{
    CLRXSVMFreeCallbackUserData* wrappedDataPtr =
            static_cast<CLRXSVMFreeCallbackUserData*>(user_data);
    CLRXSVMFreeCallbackUserData wrappedData = *wrappedDataPtr;
    delete wrappedDataPtr;
    wrappedData.realNotify(wrappedData.clrxCommandQueue, num_svm_pointers, svm_pointers,
           wrappedData.realUserData);
}
#endif

cl_int clrxInitKernelArgFlagsMap(CLRXProgram* program)
{
    if (program->kernelArgFlagsInitialized)
        return CL_SUCCESS; // if already initialized
    // clear before set up
    program->kernelArgFlagsMap.clear();
    
    if (program->assocDevicesNum == 0)
        return CL_SUCCESS;
    
    cl_program amdProg = (program->asmState != CLRXAsmState::NONE) ?
            program->amdOclAsmProgram : program->amdOclProgram;
#ifdef CL_VERSION_1_2
    cl_program_binary_type ptype;
    cl_int status = program->amdOclProgram->dispatch->clGetProgramBuildInfo(
        amdProg, program->assocDevices[0]->amdOclDevice,
        CL_PROGRAM_BINARY_TYPE, sizeof(cl_program_binary_type), &ptype, nullptr);
    if (status != CL_SUCCESS)
    {
        std::cerr << "Can't get program binary type" << std::endl;
        abort();
    }
    
    if (ptype != CL_PROGRAM_BINARY_TYPE_EXECUTABLE)
        return CL_SUCCESS; // do nothing if not executable
#else
    cl_int status = 0;
#endif
    
    std::unique_ptr<std::unique_ptr<unsigned char[]>[]> binaries = nullptr;
    try
    {
        std::vector<size_t> binarySizes(program->assocDevicesNum);
        
        status = program->amdOclProgram->dispatch->clGetProgramInfo(amdProg,
                CL_PROGRAM_BINARY_SIZES, sizeof(size_t)*program->assocDevicesNum,
                binarySizes.data(), nullptr);
        if (status != CL_SUCCESS)
        {
            std::cerr << "Can't get program binary sizes!" << std::endl;
            abort();
        }
        
        binaries.reset(new std::unique_ptr<unsigned char[]>[program->assocDevicesNum]);
        
        for (cl_uint i = 0; i < program->assocDevicesNum; i++)
            if (binarySizes[i] != 0) // if available
                binaries[i].reset(new unsigned char[binarySizes[i]]);
        
        status = program->amdOclProgram->dispatch->clGetProgramInfo(amdProg,
                CL_PROGRAM_BINARIES, sizeof(char*)*program->assocDevicesNum,
                (unsigned char**)binaries.get(), nullptr);
        if (status != CL_SUCCESS)
        {
            std::cerr << "Can't get program binaries!" << std::endl;
            abort();
        }
        /* get kernel arg info from all binaries */
        for (cl_uint i = 0; i < program->assocDevicesNum; i++)
        {
            if (binaries[i] == nullptr)
                continue; // skip if not built for this device
            
            std::unique_ptr<AmdMainBinaryBase> amdBin(
                createAmdBinaryFromCode(binarySizes[i], binaries[i].get(),
                             AMDBIN_CREATE_KERNELINFO));
            
            size_t kernelsNum = amdBin->getKernelInfosNum();
            const KernelInfo* kernelInfos = amdBin->getKernelInfos();
            /* create kernel argsflags map (for setKernelArg) */
            const size_t oldKernelMapSize = program->kernelArgFlagsMap.size();
            program->kernelArgFlagsMap.resize(oldKernelMapSize+kernelsNum);
            for (size_t i = 0; i < kernelsNum; i++)
            {
                const KernelInfo& kernelInfo = kernelInfos[i];
                std::vector<bool> kernelFlags(kernelInfo.argInfos.size()<<1);
                for (cxuint k = 0; k < kernelInfo.argInfos.size(); k++)
                {
                    const AmdKernelArg& karg = kernelInfo.argInfos[k];
                    // if mem object (image or
                    kernelFlags[k<<1] = ((karg.argType == KernelArgType::POINTER &&
                            (karg.ptrSpace == KernelPtrSpace::GLOBAL ||
                             karg.ptrSpace == KernelPtrSpace::CONSTANT)) ||
                             (karg.argType >= KernelArgType::MIN_IMAGE &&
                              karg.argType <= KernelArgType::MAX_IMAGE) ||
                             karg.argType == KernelArgType::COUNTER32 ||
                             karg.argType == KernelArgType::COUNTER64);
                    // if sampler
                    kernelFlags[(k<<1)+1] = (karg.argType == KernelArgType::SAMPLER);
                }
                
                program->kernelArgFlagsMap[oldKernelMapSize+i] =
                        std::make_pair(kernelInfo.kernelName, kernelFlags);
            }
            CLRX::mapSort(program->kernelArgFlagsMap.begin(),
                      program->kernelArgFlagsMap.end());
            size_t j = 1;
            for (size_t k = 1; k < program->kernelArgFlagsMap.size(); k++)
                if (program->kernelArgFlagsMap[k].first ==
                    program->kernelArgFlagsMap[k-1].first)
                {
                    if (program->kernelArgFlagsMap[k].second !=
                        program->kernelArgFlagsMap[k-1].second) /* if not match!!! */
                        return CL_INVALID_KERNEL_DEFINITION;
                    continue;
                }
                else // copy to new place
                    program->kernelArgFlagsMap[j++] = program->kernelArgFlagsMap[k];
            program->kernelArgFlagsMap.resize(j);
        }
    }
    catch(const std::bad_alloc& ex)
    { return CL_OUT_OF_HOST_MEMORY; }
    catch(const std::exception& ex)
    {
        std::cerr << "Fatal error at kernelArgFlagsMap creation: " <<
                ex.what() << std::endl;
        abort();
    }
    
    program->kernelArgFlagsInitialized = true;
    return CL_SUCCESS;
}


void clrxInitProgramTransDevicesMap(CLRXProgram* program,
            cl_uint num_devices, const cl_device_id* device_list,
            const std::vector<cl_device_id>& amdDevices)
{
    /* initialize transDevicesMap if not needed */
    if (program->transDevicesMap == nullptr) // if not initialized
    {   // initialize transDevicesMap
        program->transDevicesMap = new CLRXProgramDevicesMap;
        for (cl_uint i = 0; i < program->context->devicesNum; i++)
        {
            CLRXDevice* device = program->context->devices[i];
            program->transDevicesMap->insert(std::make_pair(
                        device->amdOclDevice, device));
        }
    }
    // add device_list into translate device map
    for (cl_uint i = 0; i < num_devices; i++)
        program->transDevicesMap->insert(std::make_pair(amdDevices[i],
              device_list[i]));
}

void clrxReleaseConcurrentBuild(CLRXProgram* program)
{
    program->concurrentBuilds--; // after this building
    if (program->concurrentBuilds == 0)
    {
        delete program->transDevicesMap;
        program->transDevicesMap = nullptr;
    }
}

/* Bridge between Assembler and OpenCL wrapper */

bool detectCLRXCompilerCall(const char* compilerOptions)
{
    bool isAsm = false;
    const char* co = compilerOptions;
    while (*co!=0)
    {
        while (*co!=0 && (*co==' ' || *co=='\t')) co++;
        if (::strncmp(co, "-x", 2)==0)
        {
            co+=2;
            while (*co!=0 && (*co==' ' || *co=='\t')) co++;
            if (::strncmp(co, "asm", 3)==0 && (co[3]==0 || co[3]==' ' || co[3]=='\t'))
            {
                isAsm = true;
                co+=3;
                continue;
            }
            else
                isAsm = false;
        }
        while (*co!=0 && *co!=' ' && *co!='\t') co++;
    }
    return isAsm;
}

static bool verifySymbolName(const CString& symbolName)
{
    if (symbolName.empty())
        return false;
    auto c = symbolName.begin();
    if (isAlpha(*c) || *c=='.' || *c=='_' || *c=='$')
        while (isAlnum(*c) || *c=='.' || *c=='_' || *c=='$') c++;
    return *c==0;
}

static std::pair<CString, uint64_t> getDefSym(const CString& word)
{
    std::pair<CString, uint64_t> defSym = { "", 0 };
    size_t pos = word.find('=');
    if (pos != CString::npos)
    {
        const char* outEnd;
        defSym.second = cstrtovCStyle<uint64_t>(word.c_str()+pos+1, nullptr, outEnd);
        if (*outEnd!=0)
            throw Exception("Garbages at value");
        defSym.first = word.substr(0, pos);
    }
    else
        defSym.first = word;
    if (!verifySymbolName(defSym.first))
        throw Exception("Invalid symbol name");
    return defSym;
}

struct CLRX_INTERNAL CLProgBinEntry: public FastRefCountable
{
    Array<cxbyte> binary;
    CLProgBinEntry() { }
    CLProgBinEntry(const Array<cxbyte>& _binary) : binary(_binary) { }
    CLProgBinEntry(Array<cxbyte>&& _binary) noexcept
            : binary(std::move(_binary)) { }
};

static cl_int genDeviceOrder(cl_uint devicesNum, const cl_device_id* devices,
               cl_uint assocDevicesNum, const cl_device_id* assocDevices,
               cxuint* devAssocOrders)
{
    if (assocDevicesNum < 16)
    {   // small amount of devices
        for (cxuint i = 0; i < devicesNum; i++)
        {
            auto it = std::find(assocDevices, assocDevices+assocDevicesNum, devices[i]);
            if (it != assocDevices+assocDevicesNum)
                devAssocOrders[i] = it-assocDevices;
            else // error
                return CL_INVALID_DEVICE;
        }
    }
    else
    {   // more devices
        std::unique_ptr<std::pair<cl_device_id, cxuint>[]> sortedAssocDevs(
                new std::pair<cl_device_id, cxuint>[assocDevicesNum]);
        for (cxuint i = 0; i < assocDevicesNum; i++)
            sortedAssocDevs[i] = std::make_pair(assocDevices[i], i);
        mapSort(sortedAssocDevs.get(), sortedAssocDevs.get()+assocDevicesNum);
        for (cxuint i = 0; i < devicesNum; i++)
        {
            auto it = binaryMapFind(sortedAssocDevs.get(),
                            sortedAssocDevs.get()+assocDevicesNum, devices[i]);
            if (it != sortedAssocDevs.get()+assocDevicesNum)
                devAssocOrders[i] = it->second;
            else // error
                return CL_INVALID_DEVICE;
        }
    }
    return CL_SUCCESS;
}

cl_int clrxCompilerCall(CLRXProgram* program, const char* compilerOptions,
            cl_uint devicesNum, CLRXDevice* const* devices)
try
{
    std::lock_guard<std::mutex> lock(program->asmMutex);
    if (devices==nullptr)
    {
        devicesNum = program->assocDevicesNum;
        devices = program->assocDevices.get();
    }
    cl_uint ctxDevicesNum = program->context->devicesNum;
    CLRXDevice** ctxDevices = program->context->devices.get();
    /* get source code */
    size_t sourceCodeSize;
    std::unique_ptr<char[]> sourceCode;
    const cl_program amdp = program->amdOclProgram;
    program->asmState = CLRXAsmState::IN_PROGRESS;
    program->asmProgEntries.reset();
    
    cl_int error = amdp->dispatch->clGetProgramInfo(amdp, CL_PROGRAM_SOURCE,
                    0, nullptr, &sourceCodeSize);
    if (error!=CL_SUCCESS)
    {
        std::cerr << "Fatal error from clGetProgramInfo in clrxCompilerCall" << std::endl;
        abort();
    }
    if (sourceCodeSize==0)
    {
        program->asmState = CLRXAsmState::FAILED;
        return CL_INVALID_OPERATION;
    }
    
    sourceCode.reset(new char[sourceCodeSize]);
    error = amdp->dispatch->clGetProgramInfo(amdp, CL_PROGRAM_SOURCE, sourceCodeSize,
                             sourceCode.get(), nullptr);
    if (error!=CL_SUCCESS)
    {
        std::cerr << "Fatal error from clGetProgramInfo in clrxCompilerCall" << std::endl;
        abort();
    }
    /* set up device order in assoc devices table */
    std::unique_ptr<cxuint[]> devAssocOrders(new cxuint[devicesNum]);
    if (devices != program->assocDevices.get())
    {
        error = genDeviceOrder(devicesNum, (const cl_device_id*)devices, ctxDevicesNum,
                   (const cl_device_id*)ctxDevices, devAssocOrders.get());
        if (error!=CL_SUCCESS)
        {
            program->asmState = CLRXAsmState::FAILED;
            return error;
        }
    }
    else // if this devices == null
        for (cxuint i = 0; i < devicesNum; i++)
            devAssocOrders[i] = i;
    
    Flags asmFlags = ASM_WARNINGS;
    // parsing compile options
    const char* co = compilerOptions;
    std::vector<CString> includePaths;
    std::vector<std::pair<CString, uint64_t> > defSyms;
    bool nextIsIncludePath = false;
    bool nextIsDefSym = false;
    bool nextIsLang = false;
    
    try
    {
    while(*co!=0)
    {
        while (*co!=0 && (*co==' ' || *co=='\t')) co++;
        if (*co==0)
            break;
        const char* wordStart = co;
        while (*co!=0 && *co!=' ' && *co!='\t') co++;
        const CString word(wordStart, co);
        
        if (nextIsIncludePath) // otherwise
        {
            nextIsIncludePath = false;
            includePaths.push_back(word);
        }
        else if (nextIsDefSym) // otherwise
        {
            nextIsDefSym = false;
            defSyms.push_back(getDefSym(word));
        }
        else if (nextIsLang) // otherwise
            nextIsLang = false;
        else if (word[0] == '-')
        {   // if option
            if (word == "-w")
                asmFlags &= ~ASM_WARNINGS;
            else if (word == "-forceAddSymbols")
                asmFlags |= ASM_FORCE_ADD_SYMBOLS;
            else if (word == "-I" || word == "-includepath")
                nextIsIncludePath = true;
            else if (word.compare(0, 2, "-I")==0)
                includePaths.push_back(word.substr(2, word.size()-2));
            else if (word.compare(0, 13, "-includepath=")==0)
                includePaths.push_back(word.substr(13, word.size()-13));
            else if (word == "-D" || word == "-defsym")
                nextIsDefSym = true;
            else if (word.compare(0, 2, "-D")==0)
                defSyms.push_back(getDefSym(word.substr(2, word.size()-2)));
            else if (word.compare(0, 8, "-defsym=")==0)
                defSyms.push_back(getDefSym(word.substr(8, word.size()-8)));
            else if (word == "-x" )
                nextIsLang = true;
            else if (word != "-xasm")
            {   // if not language selection to asm
                program->asmState = CLRXAsmState::FAILED;
                return CL_INVALID_BUILD_OPTIONS;
            }
        }
        else
        {
            program->asmState = CLRXAsmState::FAILED;
            return CL_INVALID_BUILD_OPTIONS;
        }
    }
    if (nextIsDefSym || nextIsIncludePath || nextIsLang)
    {
        program->asmState = CLRXAsmState::FAILED;
            return CL_INVALID_BUILD_OPTIONS;
    }
    } // error
    catch(const Exception& ex)
    {
        program->asmState = CLRXAsmState::FAILED;
        return CL_INVALID_BUILD_OPTIONS;
    }
    
    for (const auto& v: includePaths)
        std::cout << "IncPath:" << v << std::endl;
    for (const auto& v: defSyms)
        std::cout << "DefSym:" << v.first << "=" << v.second << std::endl;
    
    const bool is64Bit = parseEnvVariable<bool>("GPU_FORCE_64BIT_PTR");
    
    /* compiling programs */
    typedef std::pair<cl_device_id, cl_device_id> OutDevEntry;
    Array<OutDevEntry> outDeviceIndexMap(devicesNum);
    
    for (cxuint i = 0; i < devicesNum; i++)
        outDeviceIndexMap[i] = { devices[i], devices[i]->amdOclDevice };
    mapSort(outDeviceIndexMap.begin(), outDeviceIndexMap.end());
    devicesNum = std::unique(outDeviceIndexMap.begin(), outDeviceIndexMap.end(),
                [](const OutDevEntry& a, const OutDevEntry& b)
                { return a.first==b.first; }) - outDeviceIndexMap.begin();
    outDeviceIndexMap.resize(devicesNum);
    
    std::unique_ptr<ProgDeviceEntry[]> progDeviceEntries(new ProgDeviceEntry[devicesNum]);
    Array<RefPtr<CLProgBinEntry> > compiledProgBins(devicesNum);
    std::unique_ptr<cl_device_id[]> sortedDevs(new cl_device_id[devicesNum]);
    for (cxuint i = 0; i < devicesNum; i++)
        sortedDevs[i] = outDeviceIndexMap[i].first;
    
    for (cxuint i = 0; i < devicesNum; i++)
        progDeviceEntries[i].status = CL_BUILD_IN_PROGRESS;
    
    bool asmFailure = false;
    bool asmNotAvailable = false;
    GPUDeviceType prevDeviceType = GPUDeviceType::CAPE_VERDE;    
    for (cxuint i = 0; i < devicesNum; i++)
    {
        const auto& entry = outDeviceIndexMap[i];
        ProgDeviceEntry& progDevEntry = progDeviceEntries[i];
        
        // get device type
        size_t devNameSize;
        std::unique_ptr<char[]> devName;
        error = amdp->dispatch->clGetDeviceInfo(entry.second, CL_DEVICE_NAME,
                        0, nullptr, &devNameSize);
        if (error!=CL_SUCCESS)
        {
            std::cerr << "Fatal error at clCompilerCall (clGetDeviceInfo)" << std::endl;
            abort();
        }
        devName.reset(new char[devNameSize]);
        error = amdp->dispatch->clGetDeviceInfo(entry.second, CL_DEVICE_NAME,
                                  devNameSize, devName.get(), nullptr);
        if (error!=CL_SUCCESS)
        {
            std::cerr << "Fatal error at clCompilerCall (clGetDeviceInfo)" << std::endl;
            abort();
        }
        GPUDeviceType devType;
        try
        { devType = getGPUDeviceTypeFromName(devName.get()); }
        catch(const Exception& ex)
        {   // is error
            progDevEntry.status = CL_BUILD_ERROR;
            asmNotAvailable = true;
            continue;
        }
        
        if (i!=0 && devType == prevDeviceType)
        {   // copy from previous
            compiledProgBins[i] = compiledProgBins[i-1];
            progDevEntry = progDeviceEntries[i-1];
            continue; // skip if this same architecture
        }
        prevDeviceType = devType;
        // compile
        ArrayIStream astream(sourceCodeSize-1, sourceCode.get());
        std::vector<char> msgVector;
        VectorOStream msgStream(msgVector);
        Assembler assembler("", astream, asmFlags, BinaryFormat::AMD, devType, msgStream);
        assembler.set64Bit(is64Bit);
        
        for (const CString& incPath: includePaths)
            assembler.addIncludeDir(incPath);
        for (const auto& defSym: defSyms)
            assembler.addInitialDefSym(defSym.first, defSym.second);
        /// assemble
        bool good = false;
        try
        { good = assembler.assemble(); }
        catch(...)
        {   // if failed
            progDevEntry.log = RefPtr<CLProgLogEntry>(
                            new CLProgLogEntry(std::move(msgVector)));
            progDevEntry.status = CL_BUILD_ERROR;
            asmFailure = true;
            throw;
        }
        progDevEntry.log = RefPtr<CLProgLogEntry>(
                            new CLProgLogEntry(std::move(msgVector)));
        
        if (good)
        {
            try
            {
                const AsmFormatHandler* formatHandler = assembler.getFormatHandler();
                if (formatHandler!=nullptr)
                {
                    progDevEntry.status = CL_BUILD_SUCCESS;
                    Array<cxbyte> output;
                    formatHandler->writeBinary(output);
                    compiledProgBins[i] = RefPtr<CLProgBinEntry>(
                                new CLProgBinEntry(std::move(output)));
                }
                else
                {
                    compiledProgBins[i].reset();
                    progDevEntry.status = CL_BUILD_ERROR;
                    asmFailure = true;
                }
            }
            catch(const Exception& ex)
            {   // if exception during writing binary
                msgVector.insert(msgVector.end(), ex.what(),
                                 ex.what()+::strlen(ex.what())+1);
                progDevEntry.log = RefPtr<CLProgLogEntry>(
                            new CLProgLogEntry(std::move(msgVector)));
                progDevEntry.status = CL_BUILD_ERROR;
                asmFailure = true;
            }
        }
        else // error
        {
            progDevEntry.status = CL_BUILD_ERROR;
            asmFailure = true;
        }
    }
    /* set program binaries in order of original devices list */
    std::unique_ptr<size_t[]> programBinSizes(new size_t[devicesNum]);
    std::unique_ptr<cxbyte*[] > programBinaries(new cxbyte*[devicesNum]);
    std::unique_ptr<cxuint[]> outDeviceOrders(new cxuint[devicesNum]);
    
    cxuint compiledNum = 0;
    std::unique_ptr<cl_device_id[]> amdDevices(new cl_device_id[devicesNum]);
    for (cxuint i = 0; i < devicesNum; i++)
        if (compiledProgBins[i])
        {   // update from new compiled program binaries
            outDeviceOrders[i] = compiledNum;
            amdDevices[compiledNum] = devices[i]->amdOclDevice;
            programBinSizes[compiledNum] = compiledProgBins[i]->binary.size();
            programBinaries[compiledNum++] = compiledProgBins[i]->binary.data();
        }
    std::unique_ptr<CLRXDevice*[]> failedDevices(new CLRXDevice*[devicesNum-compiledNum]);
    cxuint j = 0;
    for (cxuint i = 0; i < devicesNum; i++)
        if (!compiledProgBins[i])
            failedDevices[j++] = devices[i];
    
    cl_program newAmdAsmP = nullptr;
    cl_int errorLast = CL_SUCCESS;
    if (compiledNum != 0)
    {
        std::unique_ptr<cl_int[]> binaryStatuses(new cl_int[compiledNum]);
        /// just create new amdAsmProgram
        newAmdAsmP = amdp->dispatch->clCreateProgramWithBinary(
                    program->context->amdOclContext, compiledNum, amdDevices.get(),
                    programBinSizes.get(), (const cxbyte**)programBinaries.get(),
                    binaryStatuses.get(), &error);
        if (newAmdAsmP==nullptr)
        {   // return error
            program->asmState = CLRXAsmState::FAILED;
            return error;
        }
        /// and build (errorLast holds last error to be returned)
        errorLast = amdp->dispatch->clBuildProgram(newAmdAsmP, compiledNum,
                          amdDevices.get(), "", nullptr, nullptr);
    }
    if (errorLast == CL_SUCCESS)
    {
        if (asmFailure)
            errorLast = CL_BUILD_PROGRAM_FAILURE;
        else if (asmNotAvailable)
            errorLast = CL_COMPILER_NOT_AVAILABLE;
    }
    
    std::lock_guard<std::mutex> clock(program->mutex);
    if (program->amdOclAsmProgram!=nullptr)
    { // release old asm program
        cl_int error = amdp->dispatch->clReleaseProgram(program->amdOclAsmProgram);
        if (error!=CL_SUCCESS)
        {
            program->asmState = CLRXAsmState::FAILED;
            return error;
        }
        program->amdOclAsmProgram = nullptr;
    }
    
    program->amdOclAsmProgram = newAmdAsmP;
    clrxUpdateProgramAssocDevices(program);
    if (compiledNum!=devicesNum)
    {   // add extra devices (failed) to list
        std::unique_ptr<CLRXDevice*[]> newAssocDevices(new CLRXDevice*[devicesNum]);
        std::copy(program->assocDevices.get(), program->assocDevices.get()+compiledNum,
                  newAssocDevices.get());
        std::copy(failedDevices.get(), failedDevices.get()+devicesNum-compiledNum,
                newAssocDevices.get()+compiledNum);
        program->assocDevices = std::move(newAssocDevices);
        program->assocDevicesNum = devicesNum;
    }
    
    std::unique_ptr<cxuint[]> asmDevOrders(new cxuint[program->assocDevicesNum]);
    if (genDeviceOrder(program->assocDevicesNum,
           (const cl_device_id*)program->assocDevices.get(), devicesNum,
           (const cl_device_id*)sortedDevs.get(), asmDevOrders.get()) != CL_SUCCESS)
    {
        std::cerr << "Fatal error at genDeviceOrder at clrxCompilerCall" << std::endl;
        abort();
    }
    
    program->asmProgEntries.reset(new ProgDeviceEntry[program->assocDevicesNum]);
    /* move logs and build statuses to CLRX program structure */
    for (cxuint i = 0; i < devicesNum; i++)
        program->asmProgEntries[asmDevOrders[i]] = std::move(progDeviceEntries[i]);
    program->asmOptions = compilerOptions;
    program->asmState = (errorLast!=CL_SUCCESS) ?
                CLRXAsmState::FAILED : CLRXAsmState::SUCCESS;
    return errorLast;
}
catch(const std::bad_alloc& ex)
{
    program->asmState = CLRXAsmState::FAILED;
    return CL_OUT_OF_HOST_MEMORY;
}
catch(const std::exception& ex)
{
    std::cerr << "Fatal error at CLRX compiler call:" <<
                ex.what() << std::endl;
    abort();
}
