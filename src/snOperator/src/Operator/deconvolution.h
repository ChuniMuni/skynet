//
// SkyNet Project
// Copyright (C) 2018 by Contributors <https://github.com/Tyill/skynet>
//
// This code is licensed under the MIT License.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#pragma once

#include "snBase/snBase.h"
#include"snOperator/src/structurs.h"
#include"snOperator/src/mathFunctions.h"

/// обратная свертка
class Deconvolution : SN_Base::OperatorBase{

public:

    Deconvolution(void* net, const std::string& name, const std::string& node, std::map<std::string, std::string>& prms);

    ~Deconvolution();

    std::vector<std::string> Do(const SN_Base::operationParam&, const std::vector<OperatorBase*>& neighbOpr) override;
        
    bool setInternPrm(std::map<std::string, std::string>& prms) override;
    
    bool setBatchNorm(const SN_Base::batchNorm& bn) override;

private:
      
    struct deconvParams{
        size_t kernel = 10;                                     ///< number of output layers of convolution
        size_t fWidth = 3;                                      ///< mask width
        size_t fHeight = 3;                                     ///< mask height  
        size_t stride = 1;                                      ///< mask step
    };
    deconvParams deconvParams_;

    activeType activeType_ = activeType::relu;                  ///< activation type
    optimizerType optimizerType_ = optimizerType::adam;         ///< optimizer
    weightInitType weightInitType_ = weightInitType::he;        ///< weight init
    batchNormType batchNormType_ = batchNormType::none;         ///< batchNorm 
    SN_Base::snSize inSzMem_;                                   ///< insz mem
           
    bool isFreeze_ = false;                                     ///< not change weight
    bool gpuClearMem_ = false;                                  ///< clear mem GPU
      
    uint32_t gpuDeviceId_ = 0;                                  ///< gpu id

    calcMode calcMode_ = calcMode::CPU;                         ///< calc mode

    SN_Base::snFloat dropOut_ = 0.F;                            ///< rand off output

    SN_Base::snFloat opt_decayMomentDW_ = 0.9F,                 ///< optimizer weight coef
                     opt_decayMomentWGr_ = 0.99F,
                     opt_lmbRegular_ = 0.001F;
  
    std::map<std::string, std::vector<SN_Base::snFloat>> auxParams_;  ///< aux data
    void* gpuParams_ = nullptr     ;                                  ///< aux GPU

    void load(std::map<std::string, std::string>& prms);

    void updateConfig(const SN_Base::snSize& newSz);

    void calcBatchNorm(bool fwBw, bool isLern, const SN_Base::snSize& insz, SN_Base::snFloat* in, SN_Base::snFloat* out, SN_Base::batchNorm prm);
       
    void forward(SN_Base::Tensor* inTns, const SN_Base::operationParam& operPrm);
    void backward(SN_Base::Tensor* inTns, const SN_Base::operationParam& operPrm);
       
    /// CPU ///////////////////////////

    void forwardCPU(const deconvParams&,
        SN_Base::snFloat* weight,      
        const SN_Base::snSize& insz,   
        SN_Base::snFloat* input,       
        const SN_Base::snSize& outsz,  
        SN_Base::snFloat* output);     
    
    void backwardCPU_GW(const deconvParams&,
        SN_Base::snFloat* weight,      
        const SN_Base::snSize& insz,   
        SN_Base::snFloat* input,       
        const SN_Base::snSize& outsz,  
        SN_Base::snFloat* gradIn,      
        SN_Base::snFloat* gradOut,     
        SN_Base::snFloat* dWeightOut); 

    void backwardCPU_G(const deconvParams&,
        SN_Base::snFloat* weight,     
        const SN_Base::snSize& insz,  
        const SN_Base::snSize& outsz, 
        SN_Base::snFloat* gradIn,     
        SN_Base::snFloat* gradOut);   



    /// CUDA ///////////////////////////

    void iniParamCUDA(const SN_Base::snSize& insz, const SN_Base::snSize& outsz, const deconvParams&, void** gpuPrm);

    void freeParamCUDA(void* gpuPrm);

    void forwardCUDA(const deconvParams&,
        SN_Base::snFloat* weight,      
        const SN_Base::snSize& insz,   
        SN_Base::snFloat* input,       
        const SN_Base::snSize& outsz,  
        SN_Base::snFloat* output,      
        void* gpuPrm);

    void backwardCUDA_GW(const deconvParams&,
        SN_Base::snFloat* weight,      
        const SN_Base::snSize& insz,   
        SN_Base::snFloat* input,       
        const SN_Base::snSize& outsz,  
        SN_Base::snFloat* gradIn,      
        SN_Base::snFloat* gradOut,     
        SN_Base::snFloat* dWeightOut,  
        void* gpuPrm);

    void backwardCUDA_G(const deconvParams&,
        SN_Base::snFloat* weight,      
        const SN_Base::snSize& insz,   
        const SN_Base::snSize& outsz,  
        SN_Base::snFloat* gradIn,      
        SN_Base::snFloat* gradOut,     
        void* gpuPrm);


    /// OpenCL ///////////////////////////

    void iniParamOCL(const SN_Base::snSize& insz, const SN_Base::snSize& outsz, const deconvParams&, void** gpuPrm);

    void freeParamOCL(void* gpuPrm);

    void forwardOCL(const deconvParams&,
        SN_Base::snFloat* weight,      
        const SN_Base::snSize& insz,   
        SN_Base::snFloat* input,       
        const SN_Base::snSize& outsz,  
        SN_Base::snFloat* output,      
        void* gpuPrm);
    
    void backwardOCL_GW(const deconvParams&,
        SN_Base::snFloat* weight,      
        const SN_Base::snSize& insz,   
        SN_Base::snFloat* input,       
        const SN_Base::snSize& outsz,  
        SN_Base::snFloat* gradIn,      
        SN_Base::snFloat* gradOut,     
        SN_Base::snFloat* dWeightOut,  
        void* gpuPrm);

    void backwardOCL_G(const deconvParams&,
        SN_Base::snFloat* weight,      
        const SN_Base::snSize& insz,   
        const SN_Base::snSize& outsz,  
        SN_Base::snFloat* gradIn,      
        SN_Base::snFloat* gradOut,     
        void* gpuPrm);
};