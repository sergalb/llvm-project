#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/SourceMgr.h"
#include "gtest/gtest.h"
#include <llvm/Analysis/TestTaskAnalysis.h>

using namespace llvm;

void testProvider(char const *ModuleString, int ExpectationResult,
std::function<int(Function const &)> testedFunction) {
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
int Res = testedFunction(*F);
EXPECT_EQ(Res, ExpectationResult);
}

TEST(BasicBlockCount, SingleBB) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  ret void"
                           "}\n";
testProvider(ModuleString, 1, countBasicBlock);
}

TEST(BasicBlockCount, TwoSequentiallyBB) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  br label %bb1\n"
                           "bb1:\n"
                           "  ret void"
                           "}\n";
testProvider(ModuleString, 2, countBasicBlock);
}

TEST(BasicBlockCount, FreeBBWithIf) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  br i1 0, label %bb1, label %bb2 \n"
                           "bb1:\n"
                           "  ret void\n"
                           "bb2:\n"
                           "  ret void\n"
                           "}\n";
testProvider(ModuleString, 3, countBasicBlock);
}

TEST(InstructionsCount, SingleInstruction) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  ret void"
                           "}\n";
testProvider(ModuleString, 1, countIRInstructions);
}

TEST(InstructionsCount, FewInstructionInOneBlock) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  %x = add i32 0, 0\n"
                           "  %y = add i32 1, 2\n"
                           "  ret void"
                           "}\n";
testProvider(ModuleString, 3, countIRInstructions);
}
TEST(InstructionsCount, FewInstructionsInFewBlocks) {
const char *ModuleString = "define i32 @test() {\n"
                           "entry:\n"
                           "  %res = icmp eq i32 0, 0 \n"
                           "  br i1 %res, label %bb1, label %bb2 \n"
                           "bb1:\n"
                           "  %x = add i32 0, 0\n"
                           "  ret i32 %x\n"
                           "bb2:\n"
                           "  %y = add i32 1, 2\n"
                           "  ret i32 %y\n"
                           "}\n";
testProvider(ModuleString, 6, countIRInstructions);
}

TEST(McCabeMeasure, TwoSequentiallyBB) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  br label %bb1\n"
                           "bb1:\n"
                           "  ret void"
                           "}\n";
testProvider(ModuleString, 1, McCabeMeasure);
}

TEST(McCabeMeasure, OneIf) {
const char *ModuleString = "define i32 @test(i32 %x) {\n"
                           "  %2 = icmp eq i32 %x, 1\n"
                           "  %3 = select i1 %2, i32 2, i32 3\n"
                           "  ret i32 %3\n"
                           "}";
testProvider(ModuleString, 2, McCabeMeasure);
}

TEST(McCabeMeasure, TwoNestedIf) {
const char *ModuleString = "define i32 @test() {\n"
                           "  %x = select i1 0, i32 1, i32 2\n"
                           "  %y = select i1 0, i32 %x, i32 3\n"
                           "  ret i32 %y\n"
                           "}";
testProvider(ModuleString, 3, McCabeMeasure);
}

TEST(McCabeMeasure, SwitchFunction) {
const char *ModuleString = "define void @test(i32 %a) {\n"
                           "entry:\n"
                           "  switch i32 %a, label %default [\n"
                           "    i32 0, label %case0\n"
                           "    i32 1, label %case1\n"
                           "    i32 2, label %case2\n"
                           "  ]\n"
                           "\n"
                           "case0:\n"
                           "  ret void\n"
                           "\n"
                           "case1:\n"
                           "  ret void\n"
                           "\n"
                           "case2:\n"
                           "  ret void\n"
                           "\n"
                           "default:\n"
                           "  ret void\n"
                           "\n"
                           "end:\n"
                           "  ret void\n"
                           "}";
testProvider(ModuleString, 4, McCabeMeasure);
}

TEST(InstructionsOnLongestPathCount, SingleInstruction) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  ret void"
                           "}\n";
testProvider(ModuleString, 1, countInstructionsOnLongestPath);
}

TEST(InstructionsOnLongestPathCount, BiggerPathBiggerInstructions) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  br i1 0, label %bb1, label %bb3\n"
                           "bb1:\n"
                           "  br label %bb2\n"
                           "bb2:\n"
                           "  ret void\n"
                           "bb3:"
                           "  ret void\n"
                           "}\n";
testProvider(ModuleString, 3, countInstructionsOnLongestPath);
}

TEST(InstructionsOnLongestPathCount, SmallerPathBiggerInstructions) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  br i1 0, label %bb1, label %bb3\n"
                           "bb1:\n"
                           "  br label %bb2\n"
                           "bb2:\n"
                           "  ret void\n"
                           "bb3:"
                           "  %x = add i32 1, 0"
                           "  %y = add i32 1, 2"
                           "  ret void\n"
                           "}\n";
testProvider(ModuleString, 3, countInstructionsOnLongestPath);
}

TEST(InstructionsOnLongestPathCount, TwoPathWithOneEnd) {
const char *ModuleString = "define void @test() {\n"
                           "entry:\n"
                           "  br i1 0, label %bb1, label %bb3\n"
                           "bb1:\n"
                           "  br label %bb2\n"
                           "bb2:\n"
                           "  br label %bb3\n"
                           "bb3:"
                           "  %x = add i32 1, 0"
                           "  %y = add i32 1, 2"
                           "  ret void\n"
                           "}\n";
testProvider(ModuleString, 6, countInstructionsOnLongestPath);
}

//TEST(GeomeanTest, LoopWithForthLevels) {
//  const char *ModuleString =
//      "define void @test(i64 %n, double* %A, double* %B) {\n"
//      "entry:\n"
//      "  br label %for.cond\n"
//      "\n"
//      "for.cond:\n"
//      "  %i.0 = phi i64 [ 0, %entry ], [ %add28, %for.inc27 ]\n"
//      "  %cmp = icmp slt i64 %i.0, %n\n"
//      "  br i1 %cmp, label %for.cond2, label %for.end29\n"
//      "\n"
//      "for.cond2:\n"
//      "  %j.0 = phi i64 [ %add25, %for.inc24 ], [ 0, %for.cond ]\n"
//      "  %cmp3 = icmp slt i64 %j.0, %n\n"
//      "  br i1 %cmp3, label %for.cond6, label %for.inc27\n"
//      "\n"
//      "for.cond6:\n"
//      "  %k.0 = phi i64 [ %add22, %for.inc21 ], [ 0, %for.cond2 ]\n"
//      "  %cmp7 = icmp slt i64 %k.0, %n\n"
//      "  br i1 %cmp7, label %for.cond10, label %for.inc24\n"
//      "\n"
//      "for.cond10:\n"
//      "  %l.0 = phi i64 [ %add20, %for.body13 ], [ 0, %for.cond6 ]\n"
//      "  %cmp11 = icmp slt i64 %l.0, %n\n"
//      "  br i1 %cmp11, label %for.body13, label %for.inc21\n"
//      "\n"
//      "for.body13:\n"
//      "  %add = add nuw nsw i64 %i.0, %j.0\n"
//      "  %add14 = add nuw nsw i64 %add, %k.0\n"
//      "  %add15 = add nuw nsw i64 %add14, %l.0\n"
//      "  %arrayidx = getelementptr inbounds double, double* %A, i64 %add15\n"
//      "  store double 2.100000e+01, double* %arrayidx, align 8, "
//      "!llvm.access.group !5\n"
//      "  %add16 = add nuw nsw i64 %i.0, %j.0\n"
//      "  %add17 = add nuw nsw i64 %add16, %k.0\n"
//      "  %add18 = add nuw nsw i64 %add17, %l.0\n"
//      "  %arrayidx19 = getelementptr inbounds double, double* %B, i64 %add18\n"
//      "  store double 4.200000e+01, double* %arrayidx19, align 8, "
//      "!llvm.access.group !6\n"
//      "  %add20 = add nuw nsw i64 %l.0, 1\n"
//      "  br label %for.cond10, !llvm.loop !11\n"
//      "\n"
//      "for.inc21:\n"
//      "  %add22 = add nuw nsw i64 %k.0, 1\n"
//      "  br label %for.cond6, !llvm.loop !14\n"
//      "\n"
//      "for.inc24:\n"
//      "  %add25 = add nuw nsw i64 %j.0, 1\n"
//      "  br label %for.cond2, !llvm.loop !16\n"
//      "\n"
//      "for.inc27:\n"
//      "  %add28 = add nuw nsw i64 %i.0, 1\n"
//      "  br label %for.cond, !llvm.loop !18\n"
//      "\n"
//      "for.end29:\n"
//      "  ret void\n"
//      "}"
//      "; access groups\n"
//      "!7 = distinct !{}\n"
//      "!8 = distinct !{}\n"
//      "!10 = distinct !{}\n"
//      "\n"
//      "; access group lists\n"
//      "!5 = !{!7, !10}\n"
//      "!6 = !{!7, !8, !10}\n"
//      "\n"
//      "; LoopIDs\n"
//      "!11 = distinct !{!11, !{!\"llvm.loop.parallel_accesses\", !10}}\n"
//      "!14 = distinct !{!14, !{!\"llvm.loop.parallel_accesses\", !8, !10}}\n"
//      "!16 = distinct !{!16, !{!\"llvm.loop.parallel_accesses\", !8}}\n"
//      "!18 = distinct !{!18, !{!\"llvm.loop.parallel_accesses\", !7}}";
//  LLVMContext C;
//  SMDiagnostic Err;
//  std::unique_ptr<Module> M = parseAssemblyString(ModuleString, Err, C);
//  std::string ErrMsg;
//  raw_string_ostream OS(ErrMsg);
//  Err.print("", OS);
//  if (!M) {
//    report_fatal_error(OS.str().c_str());
//  }
//  Function *F = M->getFunction("test");
//  double Res = geomean(*F);
//  EXPECT_EQ(Res, 4);
//}