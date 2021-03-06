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
#include "../stdafx.h"
#include "snOperator/src/Operator/fullyConnected.h"
#include "snAux/auxFunc.h"
#include "snOperator/src/weightInit.h"
#include "snOperator/src/activeFunctions.h"
#include "snOperator/src/optimizer.h"
#include "snOperator/src/structurs.h"
#include "snOperator/src/mathFunctions.h"

using namespace std;
using namespace SN_Base;

/// fullyConnected layer
FullyConnected::FullyConnected(void* net, const string& name, const string& node, std::map<std::string, std::string>& prms) :
    OperatorBase(net, name, node, prms){
        
    load(prms);
}

FullyConnected::~FullyConnected(){
    
    baseInput_ = 0;

    if (calcMode_ == calcMode::CUDA)
        freeParamCUDA(gpuParams_);
    else  if (calcMode_ == calcMode::OpenCL)
        freeParamOCL(gpuParams_);
}

void FullyConnected::load(std::map<std::string, std::string>& prms){
    
    baseOut_ = new Tensor();
    baseGrad_ = new Tensor();
    baseWeight_ = new Tensor();    
    
    if ((prms.find("kernel") != prms.end()) && SN_Aux::is_number(prms["kernel"])){

        size_t kernel = stoi(prms["kernel"]);
        if (kernel > 0)
            kernel_ = kernel;
        else
            ERROR_MESS("param 'kernel' <= 0");
    }
    else
        ERROR_MESS("not found (or not numder) param 'kernel'");

    if (prms.find("batchNorm") != prms.end()){

        string bnType = prms["batchNorm"];
        if (bnType == "none") batchNormType_ = batchNormType::none;
        else if (bnType == "beforeActive") batchNormType_ = batchNormType::beforeActive;
        else if (bnType == "postActive") batchNormType_ = batchNormType::postActive;
        else
            ERROR_MESS("param 'batchNorm' = " + bnType + " indefined");
    }

    if (prms.find("gpuClearMem") != prms.end())
        gpuClearMem_ = stoi(prms["gpuClearMem"]) == 1;
       
    if (prms.find("mode") != prms.end()){

        string mode = prms["mode"];
        if (mode == "CPU") calcMode_ = calcMode::CPU;
        else if (mode == "CUDA") calcMode_ = calcMode::CUDA;
        else if (mode == "OpenCL") calcMode_ = calcMode::OpenCL;
        else
            ERROR_MESS("param 'mode' = " + mode + " indefined");
    }
       
    if (prms.find("gpuDeviceId") != prms.end())
        gpuDeviceId_ = stoi(prms["gpuDeviceId"]);

    baseOut_->resize(snSize(kernel_));
  
    // currrect params
    setInternPrm(prms);
  
    // aux array
    auxParams_["dWeight"] = vector<snFloat>();
    auxParams_["dWPrev"] = vector<snFloat>();
    auxParams_["dWGrad"] = vector<snFloat>();

    if (batchNormType_ != batchNormType::none){
        auxParams_["bn_mean"] = vector<snFloat>(kernel_, 0);     baseBatchNorm_.mean = auxParams_["bn_mean"].data();
        auxParams_["bn_varce"] = vector<snFloat>(kernel_, 1);    baseBatchNorm_.varce = auxParams_["bn_varce"].data();
        auxParams_["bn_scale"] = vector<snFloat>(kernel_, 1);    baseBatchNorm_.scale = auxParams_["bn_scale"].data();
        auxParams_["bn_schift"] = vector<snFloat>(kernel_, 0);   baseBatchNorm_.schift = auxParams_["bn_schift"].data();
        auxParams_["bn_norm"] = vector<snFloat>();               baseBatchNorm_.norm = auxParams_["bn_norm"].data();
        auxParams_["bn_dScale"] = vector<snFloat>(kernel_, 0);   baseBatchNorm_.dScale = auxParams_["bn_dScale"].data();
        auxParams_["bn_dSchift"] = vector<snFloat>(kernel_, 0);  baseBatchNorm_.dSchift = auxParams_["bn_dSchift"].data();
        auxParams_["bn_onc"] = vector<snFloat>();                baseBatchNorm_.onc = auxParams_["bn_onc"].data();
    
        baseBatchNorm_.sz = snSize(kernel_);
    }
}

bool FullyConnected::setInternPrm(std::map<std::string, std::string>& prms){

    basePrms_ = prms;

    if (prms.find("active") != prms.end()){

        string atype = prms["active"];
        if (atype == "none") activeType_ = activeType::none;
        else if (atype == "sigmoid") activeType_ = activeType::sigmoid;
        else if (atype == "relu") activeType_ = activeType::relu;
        else if (atype == "leakyRelu") activeType_ = activeType::leakyRelu;
        else if (atype == "elu") activeType_ = activeType::elu;
        else
            ERROR_MESS("param 'active' = " + atype + " indefined");
    }

    if (prms.find("optimizer") != prms.end()){

        string optType = prms["optimizer"];
        if (optType == "sgd") optimizerType_ = optimizerType::sgd;
        else if (optType == "sgdMoment") optimizerType_ = optimizerType::sgdMoment;
        else if (optType == "adagrad") optimizerType_ = optimizerType::adagrad;
        else if (optType == "adam") optimizerType_ = optimizerType::adam;
        else if (optType == "RMSprop") optimizerType_ = optimizerType::RMSprop;
        else
            ERROR_MESS("param 'optimizer' = " + optType + " indefined");
    }

    if (prms.find("weightInit") != prms.end()){

        string wInit = prms["weightInit"];
        if (wInit == "uniform") weightInitType_ = weightInitType::uniform;
        else if (wInit == "he") weightInitType_ = weightInitType::he;
        else if (wInit == "lecun") weightInitType_ = weightInitType::lecun;
        else if (wInit == "xavier") weightInitType_ = weightInitType::xavier;
        else
            ERROR_MESS("param 'weightInit' = " + wInit + " indefined");
    }
       
    if (prms.find("decayMomentDW") != prms.end())
        opt_decayMomentDW_ = stof(prms["decayMomentDW"]);

    if (prms.find("decayMomentWGr") != prms.end())
        opt_decayMomentWGr_ = stof(prms["decayMomentWGr"]);

    if (prms.find("lmbRegular") != prms.end())
        opt_lmbRegular_ = stof(prms["lmbRegular"]);

    if (prms.find("batchNormLr") != prms.end())
        baseBatchNorm_.lr = stof(prms["batchNormLr"]);

    if (prms.find("freeze") != prms.end())
        isFreeze_ = prms["freeze"] == "1";
    
    if (prms.find("dropOut") != prms.end()){
        dropOut_ = stof(prms["dropOut"]);
        if (dropOut_ > 1.F) dropOut_ = 1.F;
        else if (dropOut_ < 0.F) dropOut_ = 0.F;
    }

    return true;
}

bool FullyConnected::setBatchNorm(const batchNorm& bn){

    size_t osz = bn.sz.size();

    auxParams_["bn_mean"] = vector<snFloat>(osz, 0);     baseBatchNorm_.mean = auxParams_["bn_mean"].data();
    auxParams_["bn_varce"] = vector<snFloat>(osz, 1);    baseBatchNorm_.varce = auxParams_["bn_varce"].data();
    auxParams_["bn_scale"] = vector<snFloat>(osz, 1);    baseBatchNorm_.scale = auxParams_["bn_scale"].data();
    auxParams_["bn_schift"] = vector<snFloat>(osz, 0);   baseBatchNorm_.schift = auxParams_["bn_schift"].data();

    memcpy(baseBatchNorm_.mean, bn.mean, osz * sizeof(snFloat));
    memcpy(baseBatchNorm_.varce, bn.varce, osz * sizeof(snFloat));
    memcpy(baseBatchNorm_.scale, bn.scale, osz * sizeof(snFloat));
    memcpy(baseBatchNorm_.schift, bn.schift, osz * sizeof(snFloat));

    baseBatchNorm_.sz = bn.sz;

    return true;
}

std::vector<std::string> FullyConnected::Do(const operationParam& operPrm, const std::vector<OperatorBase*>& neighbOpr){
    
    if (operPrm.action == snAction::forward){

        if (neighbOpr.size() > 1){
            ERROR_MESS("neighbOpr.size() > 1");
            return std::vector < std::string > {"noWay"};
        }

        forward(neighbOpr[0]->getOutput(), operPrm);
    }
    else{
        if (neighbOpr.size() == 1){
            backward(neighbOpr[0]->getGradient(), operPrm);
        }
        else{
            Tensor tns = *neighbOpr[0]->getGradient();
            for (size_t i = 1; i < neighbOpr.size(); ++i){

                if (tns != *neighbOpr[i]->getGradient()){
                    ERROR_MESS("operators size is not equals");
                    return std::vector < std::string > {"noWay"};
                }
                tns += *neighbOpr[i]->getGradient();
            }
            backward(&tns, operPrm);
        }
    }

    return std::vector<std::string>();
}

void FullyConnected::forward(SN_Base::Tensor* inTns, const operationParam& operPrm){

    snSize insz = inTns->size();
    baseInput_ = inTns;

    if (insz != inSzMem_){
        inSzMem_ = insz;
        updateConfig(insz);
    }
             
    /// calculation of the output values of neurons
    snFloat* out = baseOut_->getData();
        
    switch (calcMode_){
    case calcMode::CPU:    forwardCPU(kernel_, insz, inTns->getData(), baseWeight_->getData(), out); break;
    case calcMode::CUDA:   forwardCUDA(kernel_, insz, inTns->getData(), baseWeight_->getData(), out, gpuParams_); break;
    case calcMode::OpenCL: forwardOCL(kernel_, insz, inTns->getData(), baseWeight_->getData(), out, gpuParams_); break;
    }

    /// dropOut
    snSize outSz = baseOut_->size();
    if (dropOut_ > 0.F)
        dropOut(operPrm.isLerning, dropOut_, outSz, out);
    
    /// batchNorm
    if (batchNormType_ == batchNormType::beforeActive)
        calcBatchNorm(true, operPrm.isLerning, outSz, out, out, baseBatchNorm_);
    
    /// active func
    activeFuncForward(kernel_ * insz.n, out, activeType_);
       
    /// batchNorm
    if (batchNormType_ == batchNormType::postActive)
        calcBatchNorm(true, operPrm.isLerning, outSz, out, out, baseBatchNorm_);
}

void FullyConnected::backward(SN_Base::Tensor* inTns, const operationParam& operPrm){

    snFloat* gradIn = inTns->getData();

    /// batchNorm
    snSize gsz = inTns->size();
    if (batchNormType_ == batchNormType::postActive)
        calcBatchNorm(false, true, gsz, gradIn, gradIn, baseBatchNorm_);
      
    // active func
    if (activeType_ != activeType::none){

        snFloat* out = baseOut_->getData();
        
        size_t osz = kernel_ * inSzMem_.n;
        activeFuncBackward(osz, out, activeType_);
                
        // update grad
        for (size_t i = 0; i < osz; ++i) gradIn[i] *= out[i];
    }

    /// batchNorm
    if (batchNormType_ == batchNormType::beforeActive)
        calcBatchNorm(false, true, gsz, gradIn, gradIn, baseBatchNorm_);
      
    // calculation of the output gradient and weight correction
    snFloat* gradOut = baseGrad_->getData();
    snFloat* weight = baseWeight_->getData();
       
    if (!isFreeze_){
                
        snFloat* dWeight = auxParams_["dWeight"].data();
       
        switch (calcMode_){
        case calcMode::CPU:    backwardCPU_GW(kernel_, weight, inSzMem_, baseInput_->getData(), gradIn, gradOut, dWeight); break;
        case calcMode::CUDA:   backwardCUDA_GW(kernel_, weight, inSzMem_, baseInput_->getData(), gradIn, gradOut, dWeight, gpuParams_); break;
        case calcMode::OpenCL: backwardOCL_GW(kernel_, weight, inSzMem_, baseInput_->getData(), gradIn, gradOut, dWeight, gpuParams_); break;
        }
                
        // correct weight
        snFloat* dWPrev = auxParams_["dWPrev"].data();
        snFloat* dWGrad = auxParams_["dWGrad"].data();
        size_t wsz = baseWeight_->size().size();
        
        switch (optimizerType_){
        case optimizerType::sgd:       opt_sgd(dWeight, weight, wsz, operPrm.lr, opt_lmbRegular_); break;
        case optimizerType::sgdMoment: opt_sgdMoment(dWeight, dWPrev, weight, wsz, operPrm.lr, opt_lmbRegular_, opt_decayMomentDW_); break;
        case optimizerType::RMSprop:   opt_RMSprop(dWeight, dWGrad, weight, wsz, operPrm.lr, opt_lmbRegular_, opt_decayMomentWGr_); break;
        case optimizerType::adagrad:   opt_adagrad(dWeight, dWGrad, weight, wsz, operPrm.lr, opt_lmbRegular_); break;
        case optimizerType::adam:      opt_adam(dWeight, dWPrev, dWGrad, weight, wsz, operPrm.lr, opt_lmbRegular_, opt_decayMomentDW_, opt_decayMomentWGr_); break;
        default: break;
        }      
    }
    else{ // isFreeze
        switch (calcMode_){
        case calcMode::CPU:    backwardCPU_G(kernel_, weight, inSzMem_, gradIn, gradOut); break;
        case calcMode::CUDA:   backwardCUDA_G(kernel_, weight, inSzMem_, gradIn, gradOut, gpuParams_); break;
        case calcMode::OpenCL: backwardOCL_G(kernel_, weight, inSzMem_, gradIn, gradOut, gpuParams_); break;
        }
    }   
}

void FullyConnected::calcBatchNorm(bool fwBw, bool isLern, const snSize& insz, snFloat* in, snFloat* out, const batchNorm& prm){

    if (!isLern){
        size_t sz = insz.w * insz.h * insz.d, bsz = insz.n;
              
        /// y = ^x * γ + β
        for (size_t j = 0; j < bsz; ++j){

            snFloat* cin = in + j * sz, *cout = out + j * sz;
            for (size_t i = 0; i < sz; ++i)                    
                cout[i] = (cin[i] - prm.mean[i])  * prm.scale[i] / prm.varce[i] + prm.schift[i];                
        }                
    }
    else{ // isLerning
        if (fwBw)
           batchNormForward(insz, in, out, baseBatchNorm_);         
        else
           batchNormBackward(insz, in, out, baseBatchNorm_);        
    }
}

void FullyConnected::updateConfig(const snSize& newsz){
    
    size_t stp = newsz.w * newsz.h * newsz.d, ntp = (stp + 1) * kernel_;
        
    // leave the existing weights as they are, initialize the remainder
    size_t wcsz = baseWeight_->size().size();
    if (ntp > wcsz){
                
        baseWeight_->resize(snSize(kernel_, stp + 1));
        snFloat* wd = baseWeight_->getData();
        switch (weightInitType_){
        case weightInitType::uniform: wi_uniform(wd + wcsz, ntp - wcsz); break;
        case weightInitType::he: wi_he(wd + wcsz, ntp - wcsz, stp + 1); break;
        case weightInitType::lecun:wi_lecun(wd + wcsz, ntp - wcsz, kernel_); break;
        case weightInitType::xavier:wi_xavier(wd + wcsz, ntp - wcsz, stp + 1, kernel_); break;
        }
    }
    
    baseOut_->resize(snSize(kernel_, 1, 1, newsz.n));
    baseGrad_->resize(newsz);
           
    // aux array
    auxParams_["dWeight"].resize(ntp, 0);
    auxParams_["dWPrev"].resize(ntp, 0);
    auxParams_["dWGrad"].resize(ntp, 0);

    if (batchNormType_ != batchNormType::none){
        auxParams_["bn_norm"].resize(newsz.n * kernel_, 0); baseBatchNorm_.norm = auxParams_["bn_norm"].data();
        auxParams_["bn_onc"].resize(newsz.n, 1.F);          baseBatchNorm_.onc = auxParams_["bn_onc"].data();
    }
    
    if (calcMode_ == calcMode::CUDA)
        iniParamCUDA(newsz, kernel_, &gpuParams_);
    else if (calcMode_ == calcMode::OpenCL)
        iniParamOCL(newsz, kernel_, &gpuParams_);
} 