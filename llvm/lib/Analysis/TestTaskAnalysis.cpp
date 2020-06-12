#include "llvm/Analysis/TestTaskAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include <map>

using namespace llvm;

int llvm::countBasicBlock(Function const &F) { return F.size(); }

int llvm::countIRInstructions(Function const &F) {
  return F.getInstructionCount();
}

namespace {
int basicBlockMcCabeMeasure(BasicBlock const *BB,
                            std::map<BasicBlock const *, int> &Was) {
  auto PrevMeasure = Was.find(BB);
  if (PrevMeasure != Was.end()) {
    assert(PrevMeasure->second != -1 && "Infinity recurse detected");
    return PrevMeasure->second;
  }
  Was.insert({BB, -1});
  int Counter = 0;
  for (Instruction const &Instr : BB->instructionsWithoutDebug()) {
    if (isa<SelectInst>(&Instr)) {
      Counter++;
    }
  }
  Instruction const *TI = BB->getTerminator();
  int CountSuccessors = TI->getNumSuccessors();
  if (CountSuccessors == 0) {
    return Counter + 1;
  }
  for (int I = 0; I < CountSuccessors; ++I) {
    BasicBlock const *Successor = TI->getSuccessor(I);
    Counter += basicBlockMcCabeMeasure(Successor, Was);
  }
  Was[BB] = Counter;
  return Counter;
}

std::pair<int, int> instructionsOnLongestPath(
    BasicBlock const *BB,
    std::map<BasicBlock const *, std::pair<int, int>> &Was) {
auto PrevMeasure = Was.find(BB);
if (PrevMeasure != Was.end()) {
assert(PrevMeasure->second.second != -1 && "Infinity recurse detected");
return PrevMeasure->second;
}
Was.insert({BB, {-1, -1}});
std::pair<int, int> LongestSuccessor = {0, 0};
Instruction const *TI = BB->getTerminator();
int CountSuccessors = TI->getNumSuccessors();
for (int I = 0; I < CountSuccessors; ++I) {
BasicBlock const *Successor = TI->getSuccessor(I);
std::pair<int, int> InstructionInSuccessor =
    instructionsOnLongestPath(Successor, Was);
if (LongestSuccessor <= InstructionInSuccessor) {
LongestSuccessor = InstructionInSuccessor;
}
}
Was[BB] = LongestSuccessor;
return {LongestSuccessor.first + 1,
LongestSuccessor.second + BB->sizeWithoutDebug()};
}

void loopGeomeanDepth(Loop const *L, double Depth, int &CountLoops,
                      double &Accum) {
  std::vector<double> Res;
  bool HasSubLoops = false;
  for (Loop const *I : *L) {
    HasSubLoops = true;
    loopGeomeanDepth(I, Depth + 1, CountLoops, Accum);
  }
  if (!HasSubLoops) {
    Accum *= Depth;
    CountLoops += 1;
  }
}

} // namespace

int llvm::McCabeMeasure(Function const &F) {
  std::map<BasicBlock const *, int> Was = std::map<BasicBlock const *, int>();
  return basicBlockMcCabeMeasure(&F.front(), Was);
}

int llvm::countInstructionsOnLongestPath(const Function &F) {
  std::map<BasicBlock const *, std::pair<int, int>> Was =
                                                  std::map<BasicBlock const *, std::pair<int, int>>();
  return instructionsOnLongestPath(&F.front(), Was).second;
}

//double llvm::geomean(Function &F) {
//  PassBuilder PB;
//  LoopAnalysisManager LAM;
//  FunctionAnalysisManager FAM;
//  CGSCCAnalysisManager CGAM;
//  ModuleAnalysisManager MAM;
//
//  PB.registerModuleAnalyses(MAM);
//  PB.registerCGSCCAnalyses(CGAM);
//  PB.registerFunctionAnalyses(FAM);
//  PB.registerLoopAnalyses(LAM);
//  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
//  LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
//
//  if (std::distance(LI.begin(), LI.end()) == 0) {
//    return 0;
//  }
//  double Accum = 1;
//  int CountLoops = 0;
//  for (Loop const *I : LI) {
//    loopGeomeanDepth(I, 1, CountLoops, Accum);
//  }
//  if (CountLoops == 0) {
//    return 0;
//  } else {
//    return pow(Accum, 1.0 / CountLoops);
//  }
//}