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
    // record whether we have bitcode for the function, which is if it's not a decl.
    Profiler.setBitcodeStatus(Func.getName().str(), !Func.isDeclaration());

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
        CallGraph.addCall(Func.getName().str(), Callee->getName().str());
      else
        CallGraph.addCall(Func.getName().str(), CallGraph.getUnknown());
    }
  }

  const auto LC = halo::LC_ProgramInfoPass;
  halo::logs(LC) << "Dumping static call graph:\n";
  CallGraph.dumpDOT(halo::clogs(LC));

  return PreservedAnalyses::all();
}

} // end namespace llvm