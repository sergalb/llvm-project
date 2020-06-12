#ifndef LLVM_TESTTASKANALYSIS_H
#define LLVM_TESTTASKANALYSIS_H

#include <llvm/IR/Function.h>
namespace llvm {
int countBasicBlock(Function const &F);
int countIRInstructions(Function const &F);
int McCabeMeasure(Function const &F);
int countInstructionsOnLongestPath(Function const &F);
//double geomean(Function &F);
} // namespace llvm

#endif // LLVM_TESTTASKANALYSIS_H