#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>

#include "BufSizer.h"

using namespace llvm;

unsigned long BufSizeAnalyzor::getIdxNR(User::op_iterator idx_begin, User::op_iterator idx_end){
  auto* idx = idx_begin;
  unsigned long indices_nr = 0;
  for(;idx != idx_end;idx++){
    indices_nr++;
  }
  return indices_nr;
}


unsigned long BufSizeAnalyzor::getRealSize(ArrayType* arrayt, User::op_iterator idx_begin, User::op_iterator idx_end){
  unsigned long indices_nr = getIdxNR(idx_begin, idx_end);
  
  // point to buffer, short path
  if(indices_nr == 2){
    if(arrayt->getArrayElementType()->getIntegerBitWidth() == 8){
      unsigned long array_len = arrayt->getNumElements();
      if(dyn_cast<ConstantInt>((idx_begin+1)->get())){
        unsigned long index_into_array = cast<ConstantInt>((idx_begin+1)->get())->getZExtValue();
        // errs() << "[INFO] the real buffer len is => " << array_len - index_into_array << "\n";
        return array_len - index_into_array;
      }else{
        errs() << "OOPs, the index is not contant, ignore this case\n";
        return BUFFSIZER_FAIL;
      }
    }else{
      errs() << "OOPs, not a char buffer\n";
    }
  }
  
  if(arrayt->getElementType()->isArrayTy()){
    return getRealSize(cast<ArrayType>(arrayt->getElementType()), idx_begin+1, idx_end);
  }else if(arrayt->getElementType()->isStructTy()){
    return getRealSize(cast<StructType>(arrayt->getElementType()), idx_begin+1, idx_end);
  }else{
    errs() << "OOPs, the array element is not array nor struct\n";
  }
  return BUFFSIZER_FAIL;
}

unsigned long BufSizeAnalyzor::getRealSize(StructType* structt, User::op_iterator idx_begin, User::op_iterator idx_end){
  auto* idx = idx_begin;
  idx++;
  Type* t = structt->getTypeAtIndex(idx->get());

  if(auto* nest_structt = dyn_cast<StructType>(t)){
    return getRealSize(nest_structt, idx, idx_end);
  }else if(auto* arrayt = dyn_cast<ArrayType>(t)){
    return getRealSize(arrayt, idx, idx_end);
  }else{
    errs() << "OOPs, the indexed type is neither struct nor array, its => " << *t <<"\n";
  }
  return BUFFSIZER_FAIL;
}


unsigned long BufSizeAnalyzor::getBufSize(GetElementPtrInst* gepi){
  // errs() << "can cast to GetElementPtrInst\n";

  if(gepi->getSourceElementType()->isArrayTy()){
    return getRealSize(cast<ArrayType>(gepi->getSourceElementType()), gepi->idx_begin(), gepi->idx_end());
  }else if(gepi->getSourceElementType()->isStructTy()){
    return getRealSize(cast<StructType>(gepi->getSourceElementType()), gepi->idx_begin(), gepi->idx_end());
  }else{
    errs() << "GetElementPtrInst pointer operand not pointing to buffer, it's type is: " << *gepi->getSourceElementType() << "\n";
  }
  
  return BUFFSIZER_FAIL;
}

unsigned long BufSizeAnalyzor::getBufSize(LoadInst* ldi){
  // errs() << "[DEBUG] can cast to LoadInst\n";
  return BUFFSIZER_FAIL;
}

unsigned long BufSizeAnalyzor::getBufSize(GEPOperator* gepo){
  // errs() << "can cast to GEPOperator\n";
  if(gepo->getSourceElementType()->isStructTy()){
    return getRealSize(cast<StructType>(gepo->getSourceElementType()), gepo->idx_begin(), gepo->idx_end());
  }else if(gepo->getSourceElementType()->isArrayTy()){
    return getRealSize(cast<ArrayType>(gepo->getSourceElementType()), gepo->idx_begin(), gepo->idx_end());
  }else{
    errs() << "OOPs, GEPOperator not pointing to structtype nor arraytype\n";
  }
  return BUFFSIZER_FAIL;
}


unsigned long BufSizeAnalyzor::getBufSize(Value* arg){
  // errs() << "Analyzor::getBufferSize() => " << *arg << "\n";

  if(auto* inst = dyn_cast<Instruction>(arg)){
    if(auto* gepi = dyn_cast<GetElementPtrInst>(inst)){
      return getBufSize(gepi);
    }else if(auto* ldi = dyn_cast<LoadInst>(inst)){
      return getBufSize(ldi);
    }else{
      errs() << "OOPs, instruction, but not GetElementPtrInst nor LoadInst\n";
      return BUFFSIZER_FAIL;
    }
  }else if(auto* gepo = dyn_cast<GEPOperator>(arg)){
    return getBufSize(gepo);
  }else{
    errs() << "OOPs, should not happen, the target buffer is neither GEPOperator nor instruction\n";
  }
  return BUFFSIZER_FAIL;
}