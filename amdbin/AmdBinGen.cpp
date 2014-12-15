/*
 *  CLRadeonExtender - Unofficial OpenCL Radeon Extensions Library
 *  Copyright (C) 2014 Mateusz Szpakowski
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
#include <elf.h>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <CLRX/Utilities.h>
#include <CLRX/MemAccess.h>
#include <CLRX/AmdBinGen.h>

using namespace CLRX;

static const uint32_t gpuDeviceCodeTable[14] =
{
    0, // GPUDeviceType::UNDEFINED
    0x3ff, // GPUDeviceType::CAPE_VERDE
    0x3fe, // GPUDeviceType::PITCAIRN
    0x3fd, // GPUDeviceType::TAHITI
    0x402, // GPUDeviceType::OLAND
    0x403, // GPUDeviceType::BONAIRE
    0x404, // GPUDeviceType::SPECTRE
    0x405, // GPUDeviceType::SPOOKY
    0x406, // GPUDeviceType::KALINDI
    0x407, // GPUDeviceType::HAINAN
    0x408, // GPUDeviceType::HAWAII
    0x409, // GPUDeviceType::ICELAND
    0x40a, // GPUDeviceType::TONGA
    0x40b // GPUDeviceType::MULLINS
};

static const uint16_t gpuDeviceInnerCodeTable[14] =
{
    0, // GPUDeviceType::UNDEFINED
    0x1c, // GPUDeviceType::CAPE_VERDE
    0x1b, // GPUDeviceType::PITCAIRN
    0x1a, // GPUDeviceType::TAHITI
    0x20, // GPUDeviceType::OLAND
    0x21, // GPUDeviceType::BONAIRE
    0x22, // GPUDeviceType::SPECTRE
    0x23, // GPUDeviceType::SPOOKY
    0x24, // GPUDeviceType::KALINDI
    0x25, // GPUDeviceType::HAINAN
    0x27, // GPUDeviceType::HAWAII
    0x29, // GPUDeviceType::ICELAND
    0x2a, // GPUDeviceType::TONGA
    0x2b // GPUDeviceType::MULLINS
};

std::vector<AmdKernelArg> CLRX::parseAmdKernelArgsFromString(
            const std::string& argsString)
{
    return std::vector<AmdKernelArg>();
}

void AmdInput::addKernel(const AmdKernelInput& kernelInput)
{
    kernels.push_back(kernelInput);
}

void AmdInput::addKernel(const char* kernelName, size_t codeSize,
       const cxbyte* code, const AmdKernelConfig& config,
       size_t dataSize, const cxbyte* data)
{
    kernels.push_back({ kernelName, dataSize, data, 0, nullptr, 0, nullptr, {},
                true, config, codeSize, code });
}

void AmdInput::addKernel(const char* kernelName, size_t codeSize,
       const cxbyte* code, const std::vector<CALNoteInput>& calNotes, const cxbyte* header,
       size_t metadataSize, const char* metadata, size_t dataSize, const cxbyte* data)
{
    kernels.push_back({ kernelName, dataSize, data, 32, header,
        metadataSize, metadata, calNotes, false, AmdKernelConfig(), codeSize, code });
}

AmdGPUBinGenerator::AmdGPUBinGenerator() : binarySize(0), binary(nullptr)
{ }

AmdGPUBinGenerator::AmdGPUBinGenerator(const AmdInput* amdInput)
        : manageable(false), input(amdInput), binarySize(0), binary(nullptr)
{ }

AmdGPUBinGenerator::AmdGPUBinGenerator(bool _64bitMode, GPUDeviceType deviceType,
       uint32_t driverVersion, size_t globalDataSize, const cxbyte* globalData, 
       const std::vector<AmdKernelInput>& kernelInputs)
        : manageable(true), input(nullptr), binarySize(0), binary(nullptr)
{
    AmdInput* newInput = new AmdInput;
    try
    {
        newInput->is64Bit = _64bitMode;
        newInput->deviceType = deviceType;
        newInput->driverVersion = driverVersion;
        newInput->globalDataSize = globalDataSize;
        newInput->globalData = globalData;
        newInput->kernels = kernelInputs;
    }
    catch(...)
    {
        delete newInput;
        throw;
    }
    input = newInput;
}

AmdGPUBinGenerator::~AmdGPUBinGenerator()
{
    if (manageable)
        delete input;
    delete[] binary;
}

static const char* gpuDeviceNameTable[14] =
{
    "UNDEFINED",
    "capeverde",
    "pitcairn",
    "tahiti",
    "oland",
    "bonaire",
    "spectre",
    "spooky",
    "kalindi",
    "hainan",
    "hawaii",
    "iceland",
    "tonga",
    "mullins"
};

static const char* imgTypeNamesTable[] = { "2D", "1D", "1DA", "1DB", "2D", "2DA", "3D" };

enum KindOfType : cxbyte
{
    KT_UNSIGNED = 0,
    KT_SIGNED,
    KT_FLOAT,
    KT_DOUBLE,
    KT_OPAQUE,
    KT_UNKNOWN,
};

struct TypeNameVecSize
{
    const char* name;
    KindOfType kindOfType;
    cxbyte elemSize;
    cxbyte vecSize;
};

static const TypeNameVecSize argTypeNamesTable[] =
{
    { "u8", KT_UNSIGNED, 1, 1 }, { "i8", KT_SIGNED, 1, 1 },
    { "u16", KT_UNSIGNED, 2, 1 }, { "i16", KT_SIGNED, 2, 1 },
    { "u32", KT_UNSIGNED, 4, 1 }, { "i32", KT_SIGNED, 4, 1 },
    { "u64", KT_UNSIGNED, 8, 1 }, { "i64", KT_SIGNED, 8, 1 },
    { "float", KT_FLOAT, 4, 1 }, { "double", KT_DOUBLE, 8, 1 },
    { nullptr, KT_UNKNOWN, 1, 1 }, // POINTER
    { nullptr, KT_UNKNOWN, 1, 1 }, // IMAGE
    { nullptr, KT_UNKNOWN, 1, 1 }, // IMAGE1D
    { nullptr, KT_UNKNOWN, 1, 1 }, // IMAGE1D_ARRAY
    { nullptr, KT_UNKNOWN, 1, 1 }, // IMAGE1D_BUFFER
    { nullptr, KT_UNKNOWN, 1, 1 }, // IMAGE2D
    { nullptr, KT_UNKNOWN, 1, 1 }, // IMAGE1D_ARRAY
    { nullptr, KT_UNKNOWN, 1, 1 }, // IMAGE3D
    { "u8", KT_UNSIGNED, 1, 2 }, { "u8", KT_UNSIGNED, 1, 3 }, { "u8", KT_UNSIGNED, 1, 4 },
    { "u8",KT_UNSIGNED, 1, 8 }, { "u8", KT_UNSIGNED, 1, 16 },
    { "i8", KT_SIGNED, 1, 2 }, { "i8", KT_SIGNED, 1, 3 }, { "i8", KT_SIGNED, 1, 4 },
    { "i8", KT_SIGNED, 1, 8 }, { "i8", KT_SIGNED, 1, 16 },
    { "u16", KT_UNSIGNED, 2, 2 }, { "u16", KT_UNSIGNED, 2, 3 },
    { "u16", KT_UNSIGNED, 2, 4 }, { "u16", KT_UNSIGNED, 2, 8 },
    { "u16", KT_UNSIGNED, 2, 16 },
    { "i16", KT_SIGNED, 2, 2 }, { "i16", KT_SIGNED, 2, 3 },
    { "i16", KT_SIGNED, 2, 4 }, { "i16", KT_SIGNED, 2, 8 },
    { "i16", KT_SIGNED, 2, 16 },
    { "u32", KT_UNSIGNED, 4, 2 }, { "u32", KT_UNSIGNED, 4, 3 },
    { "u32", KT_UNSIGNED, 4, 4 }, { "u32", KT_UNSIGNED, 4, 8 },
    { "u32", KT_UNSIGNED, 4, 16 },
    { "i16", KT_SIGNED, 4, 2 }, { "i32", KT_SIGNED, 4, 3 },
    { "i32", KT_SIGNED, 4, 4 }, { "i32", KT_SIGNED, 4, 8 },
    { "i32", KT_SIGNED, 4, 16 },
    { "u64", KT_UNSIGNED, 8, 2 }, { "u64", KT_UNSIGNED, 8, 3 },
    { "u64", KT_UNSIGNED, 8, 4 }, { "u64", KT_UNSIGNED, 8, 8 },
    { "u64", KT_UNSIGNED, 8, 16 },
    { "i64", KT_SIGNED, 8, 2 }, { "i64", KT_SIGNED, 8, 3 },
    { "i64", KT_SIGNED, 8, 4 }, { "i64", KT_SIGNED, 8, 8 },
    { "i64", KT_SIGNED, 8, 16 },
    { "float", KT_FLOAT, 4, 2 }, { "float", KT_FLOAT, 4, 3 },
    { "float", KT_FLOAT, 4, 4 }, { "float", KT_FLOAT, 4, 8 },
    { "float", KT_FLOAT, 4, 16 },
    { "double", KT_DOUBLE, 4, 2 }, { "double", KT_DOUBLE, 4, 3 },
    { "double", KT_DOUBLE, 4, 4 }, { "double", KT_DOUBLE, 4, 8 },
    { "double", KT_DOUBLE, 4, 16 },
    { "u32", KT_UNSIGNED, 4, 1 }, /* SAMPLER */ { "opaque", KT_OPAQUE, 0, 1 },
    { nullptr, KT_UNKNOWN, 1, 1 }, /* COUNTER32 */
    { nullptr, KT_UNKNOWN, 1, 1 } // COUNTER64
};

struct TempAmdKernelConfig
{
    uint32_t hwRegion;
    uint32_t uavPrivate;
    uint32_t uavId;
    uint32_t constBufferId;
    uint32_t printfId;
    uint32_t privateId;
};

template<typename ElfSym>
static void putMainSymbols(size_t& offset, cxbyte* binary, const AmdInput* input,
    const std::vector<std::string>& kmetadatas, const std::vector<cxuint>& innerBinSizes)
{
    ElfSym* symbolTable = reinterpret_cast<ElfSym*>(binary+offset);
    ::memset(symbolTable, 0, sizeof(ElfSym));
    size_t namePos = 1;
    SULEV(symbolTable[1].st_name, namePos); 
    SULEV(symbolTable[1].st_value, 0);
    SULEV(symbolTable[1].st_size, input->compileOptions.size());
    SULEV(symbolTable[1].st_shndx, 4);
    symbolTable[1].st_info = 0;
    symbolTable[1].st_other = 0;
    namePos += 25;
    symbolTable += 2;
    size_t rodataPos = 0;
    if (input->globalData != nullptr)
    {
        SULEV(symbolTable->st_name, namePos); 
        SULEV(symbolTable->st_value, 0);
        SULEV(symbolTable->st_size, input->globalDataSize);
        SULEV(symbolTable->st_shndx, 5);
        symbolTable->st_info = 0;
        symbolTable->st_other = 0;
        symbolTable++;
        namePos += 18;
        rodataPos = 32;
    }
    size_t textPos = 0;
    for (cxuint i = 0; i < input->kernels.size(); i++)
    {
        const AmdKernelInput& kernel = input->kernels[i];
        /* kernel metatadata */
        const size_t metadataSize = (kernel.useConfig) ?
                kmetadatas[i].size() : kernel.metadataSize;
        SULEV(symbolTable->st_name, namePos);
        SULEV(symbolTable->st_value, rodataPos);
        SULEV(symbolTable->st_size, metadataSize);
        SULEV(symbolTable->st_shndx, 5);
        symbolTable->st_info = 0;
        symbolTable->st_other = 0;
        symbolTable++;
        namePos += 19 + kernel.kernelName.size();
        rodataPos += metadataSize;
        /* kernel code */
        SULEV(symbolTable->st_name, namePos);
        SULEV(symbolTable->st_value, textPos);
        SULEV(symbolTable->st_size, innerBinSizes[i]);
        SULEV(symbolTable->st_shndx, 6);
        symbolTable->st_info = 0;
        symbolTable->st_other = 0;
        symbolTable++;
        namePos += 17 + kernel.kernelName.size();
        textPos += innerBinSizes[i];
        /* kernel header */
        const size_t headerSize = (kernel.useConfig) ?
                32 : kernel.headerSize;
        SULEV(symbolTable->st_name, namePos);
        SULEV(symbolTable->st_value, rodataPos);
        SULEV(symbolTable->st_size, headerSize);
        SULEV(symbolTable->st_shndx, 5);
        symbolTable->st_info = 0;
        symbolTable->st_other = 0;
        symbolTable++;
        namePos += 17 + kernel.kernelName.size();
        rodataPos += headerSize;
    }
    offset += sizeof(ElfSym)*(input->kernels.size()*3 + 1 +
            (input->globalData != nullptr));
}

void AmdGPUBinGenerator::generate()
{
    const size_t kernelsNum = input->kernels.size();
    std::string driverInfo;
    uint32_t driverVersion = 99999909U;
    if (input->driverInfo.empty())
    {
        char drvInfoBuf[100];
        snprintf(drvInfoBuf, 100, "@(#) OpenCL 1.2 AMD-APP (%u.%u).  "
                "Driver version: %u.%u (VM)",
                 input->driverVersion/100U, input->driverVersion%100U,
                 input->driverVersion/100U, input->driverVersion%100U);
        driverInfo = drvInfoBuf;
        driverVersion = input->driverVersion;
    }
    else if (input->driverVersion == 0)
    {   // parse version
        size_t pos = input->driverInfo.find("AMD-APP"); // find AMDAPP
        try
        {
            if (pos != std::string::npos)
            {   /* let to parse version number */
                pos += 8;
                const char* end;
                driverVersion = cstrtovCStyle<cxuint>(
                        input->driverInfo.c_str()+pos, nullptr, end)*100;
                end++;
                driverVersion += cstrtovCStyle<cxuint>(end, nullptr, end);
            }
            driverInfo = input->driverInfo;
        }
        catch(const ParseException& ex)
        { driverVersion = 99999909U; /* newest possible */ }
    }
    
    const bool isOlderThan1124 = driverVersion < 112402;
    const bool isOlderThan1384 = driverVersion < 138405;
    const bool isOlderThan1598 = driverVersion < 159805;
    /* checking input */
    if (input->deviceType == GPUDeviceType::UNDEFINED ||
        cxuint(input->deviceType) > cxuint(GPUDeviceType::GPUDEVICE_MAX))
        throw Exception("Undefined GPU device type");
    
    std::vector<TempAmdKernelConfig> tempAmdKernelConfigs(input->kernels.size());
    
    for (cxuint i = 0; i < input->kernels.size(); i++)
    {
        const AmdKernelInput& kinput = input->kernels[i];
        if (!kinput.useConfig)
            continue;
        const AmdKernelConfig& config = kinput.config;
        TempAmdKernelConfig& tempConfig = tempAmdKernelConfigs[i];
        if (config.userDataElemsNum > 16)
            throw Exception("UserDataElemsNum must not be greater than 16");
        /* filling input */
        if (config.hwRegion == AMDBIN_DEFAULT)
            tempConfig.hwRegion = 0;
        else
            tempConfig.hwRegion = config.hwRegion;
        
        if (config.uavPrivate == AMDBIN_DEFAULT)
        {   /* compute uavPrivate */
            bool hasStructures = false;
            uint32_t amountOfArgs = 0;
            for (const AmdKernelArg arg: config.args)
            {
                if (arg.argType != KernelArgType::STRUCTURE)
                    hasStructures = true;
                if (!isOlderThan1598 && arg.argType != KernelArgType::STRUCTURE)
                    continue; // no older driver and no structure
                if (arg.argType == KernelArgType::POINTER)
                    amountOfArgs += 32;
                else if (arg.argType == KernelArgType::STRUCTURE)
                {
                    if (isOlderThan1598)
                        amountOfArgs += (arg.structSize+15)&~15;
                    else // bug in older drivers
                        amountOfArgs += 32;
                }
                else
                {
                    const TypeNameVecSize& tp = argTypeNamesTable[cxuint(arg.argType)];
                    const size_t typeSize = cxuint(tp.vecSize==3?4:tp.vecSize)*tp.elemSize;
                    amountOfArgs += ((typeSize+15)>>4)<<5;
                }
            }
            if (hasStructures || config.scratchBufferSize != 0)
                tempConfig.uavPrivate = config.scratchBufferSize + amountOfArgs;
            else
                tempConfig.uavPrivate = 0;
        }
        else
            tempConfig.uavPrivate = config.uavPrivate;
        
        if (config.uavId == AMDBIN_DEFAULT)
            tempConfig.uavId = (isOlderThan1384)?9:11;
        else
            tempConfig.uavId = config.uavId;
        
        if (config.constBufferId == AMDBIN_DEFAULT)
            tempConfig.constBufferId = (isOlderThan1384)?AMDBIN_NOTSUPPLIED : 10;
        else
            tempConfig.constBufferId = config.constBufferId;
        
        if (config.printfId == AMDBIN_DEFAULT)
            tempConfig.printfId = (isOlderThan1384)?AMDBIN_NOTSUPPLIED : 9;
        else
            tempConfig.printfId = config.printfId;
        
        if (config.privateId == AMDBIN_DEFAULT)
            tempConfig.privateId = 8;
        else
            tempConfig.privateId = config.privateId;
    }
    /* count number of bytes required to save */
    if (!input->is64Bit)
        binarySize = sizeof(Elf32_Ehdr) + sizeof(Elf32_Shdr)*7 +
                sizeof(Elf32_Sym)*(3*kernelsNum + 2);
    else
        binarySize = sizeof(Elf64_Ehdr) + sizeof(Elf64_Shdr)*7 +
                sizeof(Elf64_Sym)*(3*kernelsNum + 2);
    
    for (const AmdKernelInput& kinput: input->kernels)
        binarySize += (kinput.kernelName.size())*3;
    binarySize += 50 /*shstrtab */ + kernelsNum*(19+17+17) + 26/* static strtab size */ +
            driverInfo.size() + input->compileOptions.size() + input->globalDataSize;
    
    std::vector<cxuint> innerBinSizes(input->kernels.size());
    std::vector<std::string> kmetadatas(input->kernels.size());
    size_t uniqueId = 1025;
    for (size_t i = 0; i < input->kernels.size(); i++)
    {
        cxuint& innerBinSize = innerBinSizes[i];
        innerBinSize = sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr)*3 + sizeof(Elf32_Shdr)*6 +
            sizeof(CALEncodingEntry) + 2 + 16 + 40;
        
        const AmdKernelInput& kinput = input->kernels[i];
        const AmdKernelConfig& config = kinput.config;
        size_t readOnlyImages = 0;
        size_t uavsNum = 1;
        bool notUsedUav = false;
        size_t samplersNum = config.samplers.size();
        size_t constBuffersNum = 2;
        for (const AmdKernelArg& arg: config.args)
        {
            if (arg.argType >= KernelArgType::MIN_IMAGE &&
                arg.argType <= KernelArgType::MAX_IMAGE)
            {
                if ((arg.ptrAccess & KARG_PTR_ACCESS_MASK) == KARG_PTR_READ_ONLY)
                    readOnlyImages++;
                if ((arg.ptrAccess & KARG_PTR_ACCESS_MASK) == KARG_PTR_WRITE_ONLY)
                    uavsNum++;
            }
            else if (arg.argType == KernelArgType::POINTER)
            {
                if (arg.ptrSpace == KernelPtrSpace::GLOBAL)
                {   // only global pointers are defined in uav table
                    if (arg.used)
                        uavsNum++;
                    else
                        notUsedUav = true;
                }
                if (arg.ptrSpace == KernelPtrSpace::CONSTANT)
                    constBuffersNum++;
            }
           else if (arg.argType == KernelArgType::SAMPLER)
               samplersNum++;
        }
        
        if (notUsedUav)
            uavsNum++; // adds uav for not used
        
        innerBinSize += (kinput.data != nullptr) ? kinput.dataSize : 4736;
        innerBinSize += kinput.codeSize;
        // CAL notes size
        if (kinput.useConfig)
        {
            innerBinSize += 20*17 /*calNoteHeaders*/ + 16 + 128 + (18+32 +
                2*((isOlderThan1124)?16:config.userDataElemsNum))*8 /* proginfo */ +
                    readOnlyImages*4 /* inputs */ + 16*uavsNum /* uavs */ +
                    8*samplersNum /* samplers */ + 8*constBuffersNum /* cbids */;
            
            const TempAmdKernelConfig& tempConfig = tempAmdKernelConfigs[i];
            /* compute metadataSize */
            std::string& metadata = kmetadatas[i];
            metadata.reserve(100);
            metadata += ";ARGSTART:__OpenCL_";
            metadata += kinput.kernelName;
            metadata += "_kernel\n";
            if (isOlderThan1124)
                metadata += ";version:3:1:104\n";
            else
                metadata += ";version:3:1:111\n";
            metadata += ";device:";
            metadata += gpuDeviceNameTable[cxuint(input->deviceType)];
            char numBuf[21];
            metadata += "\n;uniqueid:";
            itocstrCStyle(uniqueId, numBuf, 21);
            metadata += numBuf;
            metadata += "\n;memory:uavprivate:";
            itocstrCStyle(tempConfig.uavPrivate, numBuf, 21);
            metadata += numBuf;
            metadata += "\n;memory:hwlocal:";
            itocstrCStyle(config.hwLocalSize, numBuf, 21);
            metadata += numBuf;
            metadata += "\n;memory:hwregion:";
            itocstrCStyle(tempConfig.hwRegion, numBuf, 21);
            metadata += numBuf;
            metadata += '\n';
            if (config.reqdWorkGroupSize[0] != 0 || config.reqdWorkGroupSize[1] != 0 ||
                config.reqdWorkGroupSize[1] != 0)
            {
                metadata += ";cws:";
                itocstrCStyle(config.reqdWorkGroupSize[0], numBuf, 21);
                metadata += numBuf;
                metadata += ':';
                itocstrCStyle(config.reqdWorkGroupSize[1], numBuf, 21);
                metadata += numBuf;
                metadata += ':';
                itocstrCStyle(config.reqdWorkGroupSize[2], numBuf, 21);
                metadata += numBuf;
                metadata += '\n';
            }
            
            size_t argOffset = 0;
            cxuint readOnlyImageCount = 0;
            cxuint writeOnlyImageCount = 0;
            cxuint uavId = tempConfig.uavId+1;
            cxuint constantId = 2;
            for (cxuint k = 0; k < config.args.size(); k++)
            {
                const AmdKernelArg& arg = config.args[k];
                if (arg.argType == KernelArgType::STRUCTURE)
                {
                    metadata += ";value:";
                    metadata += arg.argName;
                    metadata += ":struct:";
                    itocstrCStyle(arg.structSize, numBuf, 21);
                    metadata += numBuf;
                    metadata += ":1:";
                    itocstrCStyle(argOffset, numBuf, 21);
                    metadata += numBuf;
                    metadata += '\n';
                    argOffset += (arg.structSize+15)>>4;
                }
                else if (arg.argType == KernelArgType::POINTER)
                {
                    metadata += ";pointer:";
                    metadata += arg.argName;
                    metadata += ':';
                    const TypeNameVecSize& tp = argTypeNamesTable[cxuint(arg.pointerType)];
                    if (tp.kindOfType == KT_UNKNOWN)
                        throw Exception("Type not supported!");
                    const cxuint typeSize =
                        cxuint((tp.vecSize==3) ? 4 : tp.vecSize)*tp.elemSize;
                    metadata += tp.name;
                    metadata += ":1:1:";
                    itocstrCStyle(argOffset, numBuf, 21);
                    metadata += numBuf;
                    metadata += ':';
                    if (arg.ptrSpace == KernelPtrSpace::LOCAL)
                        metadata += "hl:1";
                    if (arg.ptrSpace == KernelPtrSpace::CONSTANT)
                    {
                        metadata += (isOlderThan1384)?"hc":"c";
                        if (isOlderThan1384)
                            itocstrCStyle(constantId++, numBuf, 21);
                        else
                        {
                            if (arg.used)
                                itocstrCStyle(uavId++, numBuf, 21);
                            else // if has not been used in kernel
                                itocstrCStyle(tempConfig.uavId, numBuf, 21);
                        }
                        metadata += numBuf;
                    }
                    if (arg.ptrSpace == KernelPtrSpace::GLOBAL)
                    {
                        metadata += "uav:";
                        itocstrCStyle(uavId++, numBuf, 21);
                        metadata += numBuf;
                    }
                    metadata += ':';
                    const size_t elemSize = (arg.pointerType==KernelArgType::STRUCTURE)?
                        ((arg.structSize!=0)?arg.structSize:4) : typeSize;
                    itocstrCStyle(elemSize, numBuf, 21);
                    metadata += ':';
                    metadata += (arg.ptrAccess & KARG_PTR_CONST)?"RO":"RW";
                    metadata += ':';
                    metadata += (arg.ptrAccess & KARG_PTR_VOLATILE)?'1':'0';
                    metadata += ':';
                    metadata += (arg.ptrAccess & KARG_PTR_RESTRICT)?'1':'0';
                    metadata += '\n';
                    argOffset += 32;
                }
                else if ((arg.argType >= KernelArgType::MIN_IMAGE) ||
                    (arg.argType <= KernelArgType::MAX_IMAGE))
                {
                    metadata += ";image:";
                    metadata += arg.argName;
                    metadata += ':';
                    metadata += imgTypeNamesTable[
                            cxuint(arg.argType)-cxuint(KernelArgType::IMAGE)];
                    metadata += ':';
                    if ((arg.ptrAccess & KARG_PTR_ACCESS_MASK) == KARG_PTR_READ_ONLY)
                        metadata += "RO";
                    else if ((arg.ptrAccess & KARG_PTR_ACCESS_MASK) == KARG_PTR_WRITE_ONLY)
                        metadata += "WO";
                    else if ((arg.ptrAccess & KARG_PTR_ACCESS_MASK) == KARG_PTR_READ_WRITE)
                        metadata += "RW";
                    else
                        throw Exception("Invalid image access qualifier!");
                    metadata += ':';
                    if ((arg.ptrAccess & KARG_PTR_ACCESS_MASK) == KARG_PTR_READ_ONLY)
                        itocstrCStyle(readOnlyImageCount++, numBuf, 21);
                    else // write only
                        itocstrCStyle(writeOnlyImageCount++, numBuf, 21);
                    metadata += numBuf;
                    metadata += ":1:";
                    itocstrCStyle(argOffset, numBuf, 21);
                    metadata += numBuf;
                    metadata += '\n';
                    argOffset += 32;
                }
                else if (arg.argType == KernelArgType::COUNTER32)
                {
                    metadata += ";counter:";
                    metadata += arg.argName;
                    metadata += ":32:0:1:";
                    itocstrCStyle(argOffset, numBuf, 21);
                    metadata += numBuf;
                    metadata += '\n';
                    argOffset += 16;
                }
                else
                {
                    metadata += ";value:";
                    metadata += arg.argName;
                    const TypeNameVecSize& tp = argTypeNamesTable[cxuint(arg.argType)];
                    if (tp.kindOfType == KT_UNKNOWN)
                        throw Exception("Type not supported!");
                    const cxuint typeSize =
                        cxuint((tp.vecSize==3) ? 4 : tp.vecSize)*tp.elemSize;
                    metadata += tp.name;
                    metadata += ':';
                    itocstrCStyle(tp.vecSize, numBuf, 21);
                    metadata += numBuf;
                    metadata += ':';
                    itocstrCStyle(argOffset, numBuf, 21);
                    metadata += numBuf;
                    metadata += '\n';
                    argOffset += (typeSize+15)>>4;
                }
                
                if (arg.ptrAccess & KARG_PTR_CONST)
                {
                    metadata += ";constant:";
                    itocstrCStyle(k, numBuf, 21);
                    metadata += numBuf;
                    metadata += ':';
                    metadata += arg.argName;
                    metadata += '\n';
                }
            }
            
            if (config.constDataRequired)
                metadata += ";memory:datareqd\n";
            metadata += ";function:1:";
            itocstrCStyle(uniqueId, numBuf, 21);
            metadata += numBuf;
            metadata += '\n';
            
            cxuint sampId = 0;
            for (sampId = 0; sampId < config.samplers.size(); sampId++)
            {   /* constant samplers */
                const cxuint samp = config.samplers[sampId];
                metadata += ";sampler:unknown_";
                itocstrCStyle(samp, numBuf, 21);
                metadata += numBuf;
                metadata += ':';
                itocstrCStyle(sampId, numBuf, 21);
                metadata += numBuf;
                metadata += ":1:";
                itocstrCStyle(samp, numBuf, 21);
                metadata += numBuf;
                metadata += '\n';
            }
            /* kernel argument samplers */
            for (const AmdKernelArg& arg: config.args)
                if (arg.argType == KernelArgType::SAMPLER)
                {
                    metadata += ";sampler:";
                    metadata += arg.argName;
                    metadata += ':';
                    itocstrCStyle(sampId, numBuf, 21);
                    metadata += numBuf;
                    metadata += ":0:0\n";
                }
            
            if (input->is64Bit)
                metadata += ";memory:64bitABI\n";
            metadata += ";uavid:";
            itocstrCStyle(tempConfig.uavId, numBuf, 21);
            metadata += numBuf;
            metadata += '\n';
            if (tempConfig.printfId != AMDBIN_NOTSUPPLIED)
            {
                metadata += ";printfid:";
                itocstrCStyle(tempConfig.printfId, numBuf, 21);
                metadata += numBuf;
                metadata += '\n';
            }
            if (tempConfig.constBufferId != AMDBIN_NOTSUPPLIED)
            {
                metadata += ";cbid:";
                itocstrCStyle(tempConfig.constBufferId, numBuf, 21);
                metadata += numBuf;
                metadata += '\n';
            }
            metadata += ";privateid:";
            itocstrCStyle(tempConfig.privateId, numBuf, 21);
            metadata += numBuf;
            metadata += '\n';
            for (size_t k = 0; config.args.size(); k++)
            {
                const AmdKernelArg& arg = config.args[k];
                metadata += ";reflection:";
                itocstrCStyle(k, numBuf, 21);
                metadata += numBuf;
                metadata += ':';
                metadata += arg.typeName;
                metadata += '\n';
            }
            
            metadata += ";ARGEND:__OpenCL_";
            metadata += kinput.kernelName;
            metadata += "_kernel\n";
            innerBinSize += metadata.size();
            innerBinSize += 32; // header size
        }
        else // if defined in calNotes (no config)
        {
            for (const CALNoteInput& calNote: kinput.calNotes)
                innerBinSize += 20 + calNote.header.descSize;
            if (kinput.metadata != nullptr)
                innerBinSize += kinput.metadataSize;
            if (kinput.header != nullptr)
                innerBinSize += kinput.headerSize;
        }
        uniqueId++;
        binarySize += innerBinSize;
    }
    /* writing data */
    delete[] binary;    // delete of pointer
    binary = nullptr;
    binary = new cxbyte[binarySize];
    size_t offset = 0;
    
    if (!input->is64Bit)
    {
        Elf32_Ehdr& mainHdr = *reinterpret_cast<Elf32_Ehdr*>(binary);
        static const cxbyte elf32Ident[16] = {
                0x7f, 'E', 'L', 'F', ELFCLASS32, ELFDATA2LSB, EV_CURRENT, 
                ELFOSABI_SYSV, 0, 0, 0, 0, 0, 0, 0, 0 };
        ::memcpy(mainHdr.e_ident, elf32Ident, 16);
        SULEV(mainHdr.e_type, ET_EXEC);
        SULEV(mainHdr.e_machine, gpuDeviceCodeTable[cxuint(input->deviceType)]);
        SULEV(mainHdr.e_version, EV_CURRENT);
        SULEV(mainHdr.e_entry, 0);
        SULEV(mainHdr.e_flags, 0);
        SULEV(mainHdr.e_phoff, 0);
        SULEV(mainHdr.e_ehsize, sizeof(Elf32_Ehdr));
        SULEV(mainHdr.e_phnum, 0);
        SULEV(mainHdr.e_phentsize, 0);
        SULEV(mainHdr.e_shnum, 7);
        SULEV(mainHdr.e_shentsize, sizeof(Elf32_Shdr));
        SULEV(mainHdr.e_shstrndx, 1);
        offset += sizeof(Elf32_Ehdr);
    }
    else
    {
        Elf64_Ehdr& mainHdr = *reinterpret_cast<Elf64_Ehdr*>(binary);
        static const cxbyte elf64Ident[16] = {
                0x7f, 'E', 'L', 'F', ELFCLASS64, ELFDATA2LSB, EV_CURRENT, 
                ELFOSABI_SYSV, 0, 0, 0, 0, 0, 0, 0, 0 };
        ::memcpy(mainHdr.e_ident, elf64Ident, 16);
        SULEV(mainHdr.e_type, ET_EXEC);
        SULEV(mainHdr.e_machine, gpuDeviceCodeTable[cxuint(input->deviceType)]);
        SULEV(mainHdr.e_version, EV_CURRENT);
        SULEV(mainHdr.e_entry, 0);
        SULEV(mainHdr.e_flags, 0);
        SULEV(mainHdr.e_phoff, 0);
        SULEV(mainHdr.e_ehsize, sizeof(Elf64_Ehdr));
        SULEV(mainHdr.e_phnum, 0);
        SULEV(mainHdr.e_phentsize, 0);
        SULEV(mainHdr.e_shnum, 7);
        SULEV(mainHdr.e_shentsize, sizeof(Elf64_Shdr));
        SULEV(mainHdr.e_shstrndx, 1);
        offset += sizeof(Elf64_Ehdr);
    }
    size_t sectionOffsets[6];
    sectionOffsets[0] = offset;
    ::memcpy(binary+offset,
         "\000.shstrtab\000.strtab\000.symtab\000.comment\000.rodata\000.text", 50);
    offset += 50;
    
    // .strtab
    sectionOffsets[1] = offset;
    ::memcpy(binary+offset, "\000__OpenCL_compile_options", 26);
    offset += 26;
    if (input->globalData != nullptr)
    {
        if (!isOlderThan1384)
            ::memcpy(binary+offset, "__OpenCL_0_global", 18);
        else
            ::memcpy(binary+offset, "__OpenCL_2_global", 18);
        offset += 18;
    }
    for (const AmdKernelInput& kernel: input->kernels)
    {
        ::memcpy(binary+offset, "__OpenCL_", 9);
        offset += 9;
        ::memcpy(binary+offset, kernel.kernelName.c_str(), kernel.kernelName.size());
        offset += kernel.kernelName.size();
        ::memcpy(binary+offset, "_metadata", 10);
        offset += 10;
        ::memcpy(binary+offset, "__OpenCL_", 9);
        offset += 9;
        ::memcpy(binary+offset, kernel.kernelName.c_str(), kernel.kernelName.size());
        offset += kernel.kernelName.size();
        ::memcpy(binary+offset, "_kernel", 8);
        offset += 8;
        ::memcpy(binary+offset, "__OpenCL_", 9);
        offset += 9;
        ::memcpy(binary+offset, kernel.kernelName.c_str(), kernel.kernelName.size());
        offset += kernel.kernelName.size();
        ::memcpy(binary+offset, "_header", 8);
        offset += 8;
    }
    
    // .symtab
    sectionOffsets[2] = offset;
    if (!input->is64Bit)
        putMainSymbols<Elf32_Sym>(offset, binary, input, kmetadatas, innerBinSizes);
    else
        putMainSymbols<Elf64_Sym>(offset, binary, input, kmetadatas, innerBinSizes);
    // .comment
    sectionOffsets[3] = offset;
    ::memcpy(binary+offset, input->compileOptions.c_str(), input->compileOptions.size());
    offset += input->compileOptions.size();
    ::memcpy(binary+offset, driverInfo.c_str(), driverInfo.size());
    offset += driverInfo.size();
    // .rodata
    sectionOffsets[4] = offset;
    if (input->globalData != nullptr)
    {
        ::memcpy(binary+offset, input->globalData, input->globalDataSize);
        offset += input->globalDataSize;
    }
    for (cxuint i = 0; i < input->kernels.size(); i++)
    {
        const AmdKernelInput& kernel = input->kernels[i];
        if (kernel.useConfig)
        {
            const TempAmdKernelConfig& tempConfig = tempAmdKernelConfigs[i];
            ::memcpy(binary+offset, kmetadatas[i].c_str(), kmetadatas[i].size());
            offset += kmetadatas[i].size();
            
            uint32_t* header = reinterpret_cast<uint32_t*>(binary+offset);
            SULEV(header[0], (driverVersion >= 164205) ? tempConfig.uavPrivate : 0);
            SULEV(header[1], 0);
            SULEV(header[2], tempConfig.uavPrivate);
            SULEV(header[3], kernel.config.hwLocalSize);
            SULEV(header[4], input->is64Bit?8:0);
            SULEV(header[5], 1);
            SULEV(header[6], 0);
            SULEV(header[7], 0);
            offset += 32;
        }
        else if (kernel.header != nullptr)
        {
            ::memcpy(binary+offset, kernel.metadata, kernel.metadataSize);
            offset += kernel.metadataSize;
            ::memcpy(binary+offset, kernel.header, kernel.headerSize);
            offset += kernel.headerSize;
        }
    }
    
    /* kernel binaries */
    sectionOffsets[4] = offset;
    for (cxuint i = 0; i < input->kernels.size(); i++)
    {
        const AmdKernelInput& kernel = input->kernels[i];
        const AmdKernelConfig& config = kernel.config;
        size_t readOnlyImages = 0;
        size_t writeOnlyImages = 0;
        size_t uavsNum = 1;
        bool notUsedUav = false;
        size_t samplersNum = config.samplers.size();
        size_t constBuffersNum = 2;
        for (const AmdKernelArg& arg: config.args)
        {
            if (arg.argType >= KernelArgType::MIN_IMAGE &&
                arg.argType <= KernelArgType::MAX_IMAGE)
            {
                if ((arg.ptrAccess & KARG_PTR_ACCESS_MASK) == KARG_PTR_READ_ONLY)
                    readOnlyImages++;
                if ((arg.ptrAccess & KARG_PTR_ACCESS_MASK) == KARG_PTR_WRITE_ONLY)
                {
                    writeOnlyImages++;
                    uavsNum++;
                }
            }
            else if (arg.argType == KernelArgType::POINTER)
            {
                if (arg.ptrSpace == KernelPtrSpace::GLOBAL)
                {   // only global pointers are defined in uav table
                    if (arg.used)
                        uavsNum++;
                    else
                        notUsedUav = true;
                }
                if (arg.ptrSpace == KernelPtrSpace::CONSTANT)
                    constBuffersNum++;
            }
           else if (arg.argType == KernelArgType::SAMPLER)
               samplersNum++;
        }
        
        Elf32_Ehdr& innerHdr = *reinterpret_cast<Elf32_Ehdr*>(binary + offset);
        static const cxbyte elf32Ident[16] = {
                0x7f, 'E', 'L', 'F', ELFCLASS32, ELFDATA2LSB, EV_CURRENT, 
                0x64, 0, 0, 0, 0, 0, 0, 0, 0 };
        ::memcpy(innerHdr.e_ident, elf32Ident, 16);
        SULEV(innerHdr.e_type, ET_EXEC);
        SULEV(innerHdr.e_machine, 0x7d);
        SULEV(innerHdr.e_version, EV_CURRENT);
        SULEV(innerHdr.e_entry, 0);
        SULEV(innerHdr.e_flags, 1);
        SULEV(innerHdr.e_ehsize, sizeof(Elf32_Ehdr));
        SULEV(innerHdr.e_phnum, 3);
        SULEV(innerHdr.e_phentsize, sizeof(Elf32_Phdr));
        SULEV(innerHdr.e_shnum, 6);
        SULEV(innerHdr.e_shentsize, sizeof(Elf32_Shdr));
        SULEV(innerHdr.e_shstrndx, 1);
        offset += sizeof(Elf32_Ehdr);
        
        /* ??? */
        Elf32_Phdr* programHdrTable = reinterpret_cast<Elf32_Phdr*>(binary + offset);
        ::memset(programHdrTable, 0, sizeof(Elf32_Phdr));
        SULEV(programHdrTable->p_type, 0x70000002U);
        SULEV(programHdrTable->p_flags, 0);
        SULEV(programHdrTable->p_offset, 0x94);
        SULEV(programHdrTable->p_vaddr, 0);
        SULEV(programHdrTable->p_paddr, 0);
        SULEV(programHdrTable->p_filesz, sizeof(CALEncodingEntry));
        SULEV(programHdrTable->p_memsz, 0);
        SULEV(programHdrTable->p_align, 0);
        programHdrTable++;
        SULEV(programHdrTable->p_type, PT_NOTE);
        SULEV(programHdrTable->p_flags, 0);
        SULEV(programHdrTable->p_offset, 0x1c0);
        SULEV(programHdrTable->p_vaddr, 0);
        SULEV(programHdrTable->p_paddr, 0);
        SULEV(programHdrTable->p_memsz, 0);
        SULEV(programHdrTable->p_align, 0);
        Elf32_Phdr* notePrgHdr = programHdrTable;
        programHdrTable++;
        SULEV(programHdrTable->p_type, PT_LOAD);
        SULEV(programHdrTable->p_flags, 0);
        SULEV(programHdrTable->p_vaddr, 0);
        SULEV(programHdrTable->p_paddr, 0);
        SULEV(programHdrTable->p_align, 0);
        Elf32_Phdr* loadPrgHdr = programHdrTable;
        programHdrTable++;
        
        offset += sizeof(Elf32_Phdr)*3;
        /* CALEncodingEntry */
        CALEncodingEntry& encEntry = *reinterpret_cast<CALEncodingEntry*>(binary + offset);
        SULEV(encEntry.type, 4);
        SULEV(encEntry.machine, gpuDeviceInnerCodeTable[cxuint(input->deviceType)]);
        SULEV(encEntry.flags, 0);
        SULEV(encEntry.offset, 0x1c0);
        offset += sizeof(CALEncodingEntry);
        // shstrtab content
        ::memcpy(binary + offset,
                 "\000.shstrtab\000.text\000.data\000.symtab\000.strtab\000", 40);
        offset += 40;
        /* section headers */
        Elf32_Shdr* sectionHdrTable = reinterpret_cast<Elf32_Shdr*>(binary + offset);
        ::memset(binary+offset, 0, sizeof(Elf32_Shdr));
        sectionHdrTable++;
        // .shstrtab
        SULEV(sectionHdrTable->sh_name, 1);
        SULEV(sectionHdrTable->sh_type, SHT_STRTAB);
        SULEV(sectionHdrTable->sh_flags, 0);
        SULEV(sectionHdrTable->sh_addr, 0);
        SULEV(sectionHdrTable->sh_addralign, 0);
        SULEV(sectionHdrTable->sh_offset, 0xa8);
        SULEV(sectionHdrTable->sh_size, 40);
        SULEV(sectionHdrTable->sh_link, 0);
        SULEV(sectionHdrTable->sh_entsize, 0);
        sectionHdrTable++;
        
        offset += sizeof(Elf32_Shdr)*6;
        if (kernel.useConfig)
        {
            // CAL CALNOTE_INPUTS
            CALNoteHeader* noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_INPUTS);
            SULEV(noteHdr->descSize, 4*readOnlyImages);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            uint32_t* data32 = reinterpret_cast<uint32_t*>(binary + offset);
            for (cxuint k = 0; k < readOnlyImages; k++)
                SULEV(data32[k], (driverVersion == 101602 || driverVersion == 112402) ?
                        readOnlyImages-k-1 : k);
            offset += noteHdr->descSize;
            // CALNOTE_OUTPUTS
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_OUTPUTS);
            SULEV(noteHdr->descSize, 0);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            // CALNOTE_UAV
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->type, CALNOTE_ATI_OUTPUTS);
            SULEV(noteHdr->descSize, 16*uavsNum);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            data32 = reinterpret_cast<uint32_t*>(binary + offset);
            if (isOlderThan1124)
            {   // for new drivers
                for (cxuint k = 0; k < writeOnlyImages; k++)
                {   /* writeOnlyImages */
                    SULEV(data32[k<<2], k);
                    SULEV(data32[(k<<2)+1], 2);
                    SULEV(data32[(k<<2)+2], 2);
                    SULEV(data32[(k<<2)+3], 3);
                }
                data32 += writeOnlyImages<<2;
                // global buffers
                for (cxuint k = 0; k < uavsNum-writeOnlyImages-1; k++)
                {
                    SULEV(data32[k<<2], k+config.uavId+1);
                    SULEV(data32[(k<<2)+1], 4);
                    SULEV(data32[(k<<2)+2], 0);
                    SULEV(data32[(k<<2)+3], 5);
                }
                data32 += (uavsNum-writeOnlyImages-1)<<2;
            }
            else
            {   /* in argument order */
                cxuint writeOnlyImagesCount = 0;
                cxuint uavIdsCount = config.uavId+1;
                for (const AmdKernelArg& arg: config.args)
                {
                    if (arg.argType >= KernelArgType::MIN_IMAGE &&
                        arg.argType <= KernelArgType::MAX_IMAGE &&
                        (arg.ptrAccess & KARG_PTR_ACCESS_MASK) == KARG_PTR_WRITE_ONLY)
                    {   // write_only images
                        SULEV(data32[0], writeOnlyImagesCount++);
                        SULEV(data32[1], 2);
                        SULEV(data32[2], 2);
                        SULEV(data32[3], 5);
                    }
                    else if (arg.argType == KernelArgType::POINTER &&
                        arg.ptrSpace == KernelPtrSpace::GLOBAL)
                    {   // uavid
                        SULEV(data32[0], uavIdsCount);
                        SULEV(data32[1], 4);
                        SULEV(data32[2], 0);
                        SULEV(data32[3], 5);
                    }
                    data32++;
                }
            }
            // privateid or uavid (???)
            SULEV(data32[0], (isOlderThan1384)?config.privateId:config.uavId);
            SULEV(data32[1], (isOlderThan1384)?3:4);
            SULEV(data32[2], 0);
            SULEV(data32[3], 5);
            
            // CALNOTE_CONDOUT
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_CONDOUT);
            SULEV(noteHdr->descSize, 4);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            data32 = reinterpret_cast<uint32_t*>(binary + offset);
            SULEV(*data32, config.condOut);
            offset += 4;
            // CALNOTE_FLOAT32CONSTS
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_FLOAT32CONSTS);
            SULEV(noteHdr->descSize, 0);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            // CALNOTE_INT32CONSTS
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_INT32CONSTS);
            SULEV(noteHdr->descSize, 0);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            // CALNOTE_BOOL32CONSTS
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_BOOL32CONSTS);
            SULEV(noteHdr->descSize, 0);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            
            // CALNOTE_EARLYEXIT
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_EARLYEXIT);
            SULEV(noteHdr->descSize, 4);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            data32 = reinterpret_cast<uint32_t*>(binary + offset);
            SULEV(*data32, config.earlyExit);
            offset += 4;
            
            // CALNOTE_GLOBAL_BUFFERS
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_GLOBAL_BUFFERS);
            SULEV(noteHdr->descSize, 0);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            
            // CALNOTE_CONSTANT_BUFFERS
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_CONSTANT_BUFFERS);
            SULEV(noteHdr->descSize, 8*constBuffersNum);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            data32 = reinterpret_cast<uint32_t*>(binary + offset);
            offset += noteHdr->descSize;
            
            // CALNOTE_SCRATCH_BUFFERS
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_SCRATCH_BUFFERS);
            SULEV(noteHdr->descSize, 4);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
            data32 = reinterpret_cast<uint32_t*>(binary + offset);
            SULEV(*data32, config.scratchBufferSize);
            offset += 4;
            
            // CALNOTE_PERSISTENT_BUFFERS
            noteHdr = reinterpret_cast<CALNoteHeader*>(binary + offset);
            SULEV(noteHdr->nameSize, 8);
            SULEV(noteHdr->type, CALNOTE_ATI_PERSISTENT_BUFFERS);
            SULEV(noteHdr->descSize, 0);
            ::memcpy(noteHdr->name, "ATI CAL", 8);
            offset += sizeof(CALNoteHeader);
        }
        else // from CALNotes array
            for (const CALNoteInput& calNote: kernel.calNotes)
            {
                ::memcpy(binary + offset, &calNote.header, sizeof(CALNoteHeader));
                offset += sizeof(CALNoteHeader);
                ::memcpy(binary + offset, calNote.data, calNote.header.descSize);
                offset += calNote.header.descSize;
            }
        
        // .text
        SULEV(sectionHdrTable->sh_name, 11);
        SULEV(sectionHdrTable->sh_type, SHT_PROGBITS);
        SULEV(sectionHdrTable->sh_flags, 0);
        SULEV(sectionHdrTable->sh_addr, 0);
        SULEV(sectionHdrTable->sh_addralign, 0);
        SULEV(sectionHdrTable->sh_link, 0);
        SULEV(sectionHdrTable->sh_entsize, 0);
        sectionHdrTable++;
        // .data
        SULEV(sectionHdrTable->sh_name, 17);
        SULEV(sectionHdrTable->sh_type, SHT_PROGBITS);
        SULEV(sectionHdrTable->sh_flags, 0);
        SULEV(sectionHdrTable->sh_addr, 0);
        SULEV(sectionHdrTable->sh_addralign, 0);
        SULEV(sectionHdrTable->sh_link, 0);
        //SULEV(sectionHdrTable->sh_entsize, 0);
        sectionHdrTable++;
        // .symtab
        SULEV(sectionHdrTable->sh_name, 23);
        SULEV(sectionHdrTable->sh_type, SHT_SYMTAB);
        SULEV(sectionHdrTable->sh_flags, 0);
        SULEV(sectionHdrTable->sh_addr, 0);
        SULEV(sectionHdrTable->sh_addralign, 0);
        SULEV(sectionHdrTable->sh_size, sizeof(Elf32_Sym));
        SULEV(sectionHdrTable->sh_link, 5);
        SULEV(sectionHdrTable->sh_entsize, sizeof(Elf32_Sym));
        sectionHdrTable++;
        // .strtab
        SULEV(sectionHdrTable->sh_name, 30);
        SULEV(sectionHdrTable->sh_type, SHT_STRTAB);
        SULEV(sectionHdrTable->sh_flags, 0);
        SULEV(sectionHdrTable->sh_addr, 0);
        SULEV(sectionHdrTable->sh_addralign, 0);
    }
}