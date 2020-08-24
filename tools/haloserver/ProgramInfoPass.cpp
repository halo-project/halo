#include "halo/compiler/Profiler.h"
#include "halo/compiler/ProgramInfoPass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"

#include "Logging.h"

namespace llvm {

PreservedAnalyses ProgramInfoPass::run(Module &M, ModuleAnalysisManager &MAM) {
  // For now, we just collect the call graph.

  auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  CallGraphAnalysis::Result &CGR = MAM.getResult<CallGraphAnalysis>(M);
  halo::CallGraph &CallGraph = Profiler.getCallGraph();

  for (auto &Func : M.functions()) {
    // record whether we have bitcode for the function, which is if it's not a decl.
    bool HaveBitcode = !Func.isDeclaration();

    // FIXME: this is a hack only for the raspberry pi and is specific to
    // my benchmarks, which typically mark the worker function invoked
    // from main to be noinline. ideally we would have specific annotation.
    //
    // Why do we have hinted roots?
    // The raspberry pi is sending empty calling-context
    // data in the perf samples. I don't know why and don't have time
    // to fix that, so manually hinted-roots are a workaround!
    bool HintedRoot = Func.hasFnAttribute(Attribute::NoInline);

    std::string ThisFunc = Func.getName().str();
    CallGraph.addNode(ThisFunc, HaveBitcode, HintedRoot);

    // check the function's call-sites to populate the call-graph.
    CallGraphNode *CGNode = CGR[&Func];
    for (CallGraphNode::CallRecord const& Record : *CGNode) {
      // NOTE: the first member of the record is the call/invoke instruction itself.
      // thus, each CallRecord corresponds to one call-site in the function.
      CallGraphNode* CalleeNode = Record.second;

      // determine whether this callsite is within a loop of the function.
      bool CalledInLoopBody = false;
      if (HaveBitcode) {
        auto &LI = FAM.getResult<LoopAnalysis>(Func);
        if (CallBase *CB = dyn_cast_or_null<CallBase>(Record.first.getValueOr(nullptr))) // should always be true
          if (LI.getLoopFor(CB->getParent()) != nullptr)
            CalledInLoopBody = true;
      }

      // A call graph may contain nodes where the function that they correspond to
      // is null.  These 'external' nodes are used to represent control flow that is
      // not represented (or analyzable) in the module.
      Function* Callee = CalleeNode->getFunction();

      if (Callee)
        CallGraph.addCall(ThisFunc, Callee->getName().str(), CalledInLoopBody);
      else
        CallGraph.addCall(ThisFunc, CallGraph.getUnknown(), CalledInLoopBody);
    }
  }

  const auto LC = halo::LC_CallGraph;
  halo::logs(LC) << "Dumping static call graph:\n";
  CallGraph.dumpDOT(halo::clogs(LC));

  return PreservedAnalyses::none();
}

} // end namespace llvm