#include "halo/tuner/CodeVersion.h"
#include "halo/compiler/CodeRegionInfo.h"
#include "llvm/Support/SHA1.h"

#include "llvm/Support/CommandLine.h"

namespace cl = llvm::cl;

namespace halo {
  namespace Metric {
    enum Kind {
      IPC,
      CallFreq
    };
  }
}


static cl::opt<halo::Metric::Kind> CL_Metric(
  "halo-metric",
  cl::desc("The metric to use when evaluating code quality."),
  cl::init(halo::Metric::IPC),
  cl::values(clEnumValN(halo::Metric::IPC, "ipc", "instructions-per-cycle"),
             clEnumValN(halo::Metric::CallFreq, "calls", "Tuning-group call frequency")));


namespace halo {

  /// Creates a code version corresponding to the original library in the client.
  CodeVersion::CodeVersion() : LibName(CodeRegionInfo::OriginalLib) {
    ObjFileHashes.insert({}); // the {} here is a value initializer for std::array that is zero-filled.
  }

  /// Code version for original lib, along with its config.
  CodeVersion::CodeVersion(KnobSet OriginalConfig) : LibName(CodeRegionInfo::OriginalLib) {
    ObjFileHashes.insert({});
    Configs.push_back(std::move(OriginalConfig));
  }

  // Create a code version for a finished job.
  CodeVersion::CodeVersion(CompilationManager::FinishedJob &&Job) : LibName(Job.UniqueJobName) {
    if (!Job.Result) {
      warning("Compile job failed with an error, library is broken.");
      Broken = true;
    }

    ObjFile = std::move(Job.Result.getValue());
    SHAHash Hash = llvm::SHA1::hash(llvm::arrayRefFromStringRef(ObjFile->getBuffer()));
    ObjFileHashes.insert(Hash);
    Configs.push_back(Job.Config);
  }

  std::string const& CodeVersion::getLibraryName() const { return LibName; }

  std::unique_ptr<llvm::MemoryBuffer> const& CodeVersion::getObjectFile() const { return ObjFile; }

  /// returns true if the given code version was merged with this code version.
  /// The check is performed by comparing the object files for equality.
  /// If they're equal, this code version has its configs extended with the other's.
  bool CodeVersion::tryMerge(CodeVersion &CV) {

    bool Mergable = false;
    for (auto TheirHash : CV.ObjFileHashes)
      if (ObjFileHashes.count(TheirHash) == 1) {
        Mergable = true;
        break;
      }

    if (Mergable)
      return forceMerge(CV);

    return false;
  }

  bool CodeVersion::forceMerge(CodeVersion &CV) {

    for (auto TheirHash : CV.ObjFileHashes)
      ObjFileHashes.insert(TheirHash);

    for (auto KS : CV.Configs)
      Configs.push_back(std::move(KS));

    Quality.merge(CV.Quality);

    // empty out the other one for safety.
    CV.Configs.clear();
    CV.ObjFileHashes.clear();
    CV.clearQuality();

    return true;
  }

  bool CodeVersion::updateQuality(Profiler &Prof, FunctionGroup const& FG) {
    SampledQuantity SQ;

    // first, check for fresh perf info
    if (CL_Metric == Metric::IPC)
      SQ = Prof.currentIPC(FG, LibName);
    else if (CL_Metric == Metric::CallFreq)
      SQ = Prof.currentCallFreq(FG);
    else
      fatal_error("unhandled metric");

    assert(SamplesSeen <= SQ.Samples && "non-increasing sample count?");

    // not enough new samples? can't update.
    if ((SamplesSeen - SQ.Samples) < 2)
      return false;
    else
      SamplesSeen = SQ.Samples;

    Quality.observe(SQ.Quantity);
    return true;
  }

  bool CodeVersion::isBroken() const { return Broken; }

  bool CodeVersion::isOriginalLib() const { return LibName == CodeRegionInfo::OriginalLib; }

} // end namespace
