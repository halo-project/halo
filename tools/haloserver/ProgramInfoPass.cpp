#include "halo/compiler/Profiler.h"
#include "halo/compiler/ProgramInfoPass.h"
#include "llvm/Analysis/CallGraph.h"

#include "Logging.h"

namespace llvm {

PreservedAnalyses ProgramInfoPass::run(Module &M, ModuleAnalysisManager &MAM) {
  // For now, we just collect the call graph.

  CallGraphAnalysis::Result &CGR = MAM.getResult<CallGraphAnalysis>(M);
  halo::CallGraph &CallGraph = Profiler.getCallGraph();

  for (auto const& Func : M.functions()) {
    // record whether we have bitcode for the function
    if (!Func.isDeclaration())
      Profiler.setBitcodeStatus(Func.getName(), true);

    // check the function's call-sites to populate the call-graph.
    CallGraphNode *CGNode = CGR[&Func];
    for (CallGraphNode::CallRecord const& Record : *CGNode) {
      // NOTE: the first member of the record is the call/invoke instruction itself.
      // thus, each CallRecord corresponds to one call-site in the function.
      CallGraphNode* CalleeNode = Record.second;

      // A call graph may contain nodes where the function that they correspond to
      // is null.  These 'external' nodes are used to represent control flow that is
      // not represented (or analyzable) in the module.
      Function* Callee = CalleeNode->getFunction();

      if (Callee)
        CallGraph.addCall(Func.getName(), Callee->getName());
      else
        CallGraph.addCall(Func.getName(), CallGraph.getUnknown());
    }
  }

  halo::logs() << "Dumping static call graph:\n";
  CallGraph.dumpDOT(halo::clogs());

  return PreservedAnalyses::all();
}

} // end namespace llvm