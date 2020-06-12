//===- LICMTest.cpp - LICM unit tests -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Testing/Support/Error.h"
#include "llvm/Transforms/Scalar/LICM.h"
#include "gtest/gtest.h"

namespace llvm {

TEST(LICMTest, TestSCEVInvalidationOnHoisting) {
  LLVMContext Ctx;
  ModulePassManager MPM;
  PassBuilder PB;
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;

  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  StringRef PipelineStr = "require<opt-remark-emit>,loop(licm)";
  ASSERT_THAT_ERROR(PB.parsePassPipeline(MPM, PipelineStr), Succeeded());

  SMDiagnostic Error;
  StringRef Text = R"(
    define void @foo(i64* %ptr) {
    entry:
      br label %loop

    loop:
      %iv = phi i64 [ 0, %entry ], [ %iv.inc, %loop ]
      %n = load i64, i64* %ptr, !invariant.load !0
      %iv.inc = add i64 %iv, 1
      %cmp = icmp ult i64 %iv.inc, %n
      br i1 %cmp, label %loop, label %exit

    exit:
      ret void
    }

    !0 = !{}
  )";

  std::unique_ptr<Module> M = parseAssemblyString(Text, Error, Ctx);
  ASSERT_TRUE(M);
  Function *F = M->getFunction("foo");
  ScalarEvolution &SE = FAM.getResult<ScalarEvolutionAnalysis>(*F);
  BasicBlock &EntryBB = F->getEntryBlock();
  BasicBlock *LoopBB = EntryBB.getUniqueSuccessor();

  // Select `load i64, i64* %ptr`.
  Instruction *IBefore = LoopBB->getFirstNonPHI();
  // Make sure the right instruction was selected.
  ASSERT_TRUE(isa<LoadInst>(IBefore));
  // Upon this query SCEV caches disposition of <load i64, i64* %ptr> SCEV.
  ASSERT_EQ(SE.getBlockDisposition(SE.getSCEV(IBefore), LoopBB),
            ScalarEvolution::BlockDisposition::DominatesBlock);

  MPM.run(*M, MAM);

  // Select `load i64, i64* %ptr` after it was hoisted.
  Instruction *IAfter = EntryBB.getFirstNonPHI();
  // Make sure the right instruction was selected.
  ASSERT_TRUE(isa<LoadInst>(IAfter));

  ScalarEvolution::BlockDisposition DispositionBeforeInvalidation =
      SE.getBlockDisposition(SE.getSCEV(IAfter), LoopBB);
  SE.forgetValue(IAfter);
  ScalarEvolution::BlockDisposition DispositionAfterInvalidation =
      SE.getBlockDisposition(SE.getSCEV(IAfter), LoopBB);

  // If LICM have properly invalidated SCEV,
  //   1. SCEV of <load i64, i64* %ptr> should properly dominate the "loop" BB,
  //   2. extra invalidation shouldn't change result of the query.
  EXPECT_EQ(DispositionBeforeInvalidation,
            ScalarEvolution::BlockDisposition::ProperlyDominatesBlock);
  EXPECT_EQ(DispositionBeforeInvalidation, DispositionAfterInvalidation);
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

double geomean(Function &F) {
  PassBuilder PB;
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;

  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
  LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);

  if (std::distance(LI.begin(), LI.end()) == 0) {
    return 0;
  }
  double Accum = 1;
  int CountLoops = 0;
  for (Loop const *I : LI) {
    loopGeomeanDepth(I, 1, CountLoops, Accum);
  }
  if (CountLoops == 0) {
    return 0;
  } else {
    return pow(Accum, 1.0 / CountLoops);
  }
}

TEST(GeomeanTest, LoopWithForthLevels) {
  const char *ModuleString =
      "define void @test(i64 %n, double* %A, double* %B) {\n"
      "entry:\n"
      "  br label %for.cond\n"
      "\n"
      "for.cond:\n"
      "  %i.0 = phi i64 [ 0, %entry ], [ %add28, %for.inc27 ]\n"
      "  %cmp = icmp slt i64 %i.0, %n\n"
      "  br i1 %cmp, label %for.cond2, label %for.end29\n"
      "\n"
      "for.cond2:\n"
      "  %j.0 = phi i64 [ %add25, %for.inc24 ], [ 0, %for.cond ]\n"
      "  %cmp3 = icmp slt i64 %j.0, %n\n"
      "  br i1 %cmp3, label %for.cond6, label %for.inc27\n"
      "\n"
      "for.cond6:\n"
      "  %k.0 = phi i64 [ %add22, %for.inc21 ], [ 0, %for.cond2 ]\n"
      "  %cmp7 = icmp slt i64 %k.0, %n\n"
      "  br i1 %cmp7, label %for.cond10, label %for.inc24\n"
      "\n"
      "for.cond10:\n"
      "  %l.0 = phi i64 [ %add20, %for.body13 ], [ 0, %for.cond6 ]\n"
      "  %cmp11 = icmp slt i64 %l.0, %n\n"
      "  br i1 %cmp11, label %for.body13, label %for.inc21\n"
      "\n"
      "for.body13:\n"
      "  %add = add nuw nsw i64 %i.0, %j.0\n"
      "  %add14 = add nuw nsw i64 %add, %k.0\n"
      "  %add15 = add nuw nsw i64 %add14, %l.0\n"
      "  %arrayidx = getelementptr inbounds double, double* %A, i64 %add15\n"
      "  store double 2.100000e+01, double* %arrayidx, align 8, "
      "!llvm.access.group !5\n"
      "  %add16 = add nuw nsw i64 %i.0, %j.0\n"
      "  %add17 = add nuw nsw i64 %add16, %k.0\n"
      "  %add18 = add nuw nsw i64 %add17, %l.0\n"
      "  %arrayidx19 = getelementptr inbounds double, double* %B, i64 %add18\n"
      "  store double 4.200000e+01, double* %arrayidx19, align 8, "
      "!llvm.access.group !6\n"
      "  %add20 = add nuw nsw i64 %l.0, 1\n"
      "  br label %for.cond10, !llvm.loop !11\n"
      "\n"
      "for.inc21:\n"
      "  %add22 = add nuw nsw i64 %k.0, 1\n"
      "  br label %for.cond6, !llvm.loop !14\n"
      "\n"
      "for.inc24:\n"
      "  %add25 = add nuw nsw i64 %j.0, 1\n"
      "  br label %for.cond2, !llvm.loop !16\n"
      "\n"
      "for.inc27:\n"
      "  %add28 = add nuw nsw i64 %i.0, 1\n"
      "  br label %for.cond, !llvm.loop !18\n"
      "\n"
      "for.end29:\n"
      "  ret void\n"
      "}"
      "; access groups\n"
      "!7 = distinct !{}\n"
      "!8 = distinct !{}\n"
      "!10 = distinct !{}\n"
      "\n"
      "; access group lists\n"
      "!5 = !{!7, !10}\n"
      "!6 = !{!7, !8, !10}\n"
      "\n"
      "; LoopIDs\n"
      "!11 = distinct !{!11, !{!\"llvm.loop.parallel_accesses\", !10}}\n"
      "!14 = distinct !{!14, !{!\"llvm.loop.parallel_accesses\", !8, !10}}\n"
      "!16 = distinct !{!16, !{!\"llvm.loop.parallel_accesses\", !8}}\n"
      "!18 = distinct !{!18, !{!\"llvm.loop.parallel_accesses\", !7}}";
  LLVMContext C;
  SMDiagnostic Err;
  std::unique_ptr<Module> M = parseAssemblyString(ModuleString, Err, C);
  std::string ErrMsg;
  raw_string_ostream OS(ErrMsg);
  Err.print("", OS);
  if (!M) {
  report_fatal_error(OS.str().c_str());
  }
  Function *F = M->getFunction("test");

  double res = geomean(*F);
  EXPECT_EQ(res, 4);
}
}
