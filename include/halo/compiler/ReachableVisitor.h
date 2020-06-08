#include "boost/graph/depth_first_search.hpp"
#include <unordered_set>

namespace halo {

// A generic boost visitor to find all reachable vertices from a given root using a DFS traversal.
template <typename Graph>
class ReachableVisitor : public boost::default_dfs_visitor {
  public:
    using VertexID = typename Graph::vertex_descriptor;

    /// This ctor takes a function to further filter the reachable vertices.
    /// If the function returns true, then it will be included as a reachable vertex.
    ReachableVisitor(std::unordered_set<VertexID>& visited, VertexID root, std::function<bool(VertexID,Graph const&)>& Filter)
      : Root(root), Visited(visited), Predicate(Filter) {}

    ReachableVisitor(std::unordered_set<VertexID>& visited, VertexID root)
      : Root(root), Visited(visited), Predicate(DefaultPredicate) {}

    void start_vertex(VertexID u, const Graph& g) {
      if (Root == u)
        InSubtree = true;
    }

    void discover_vertex(VertexID u, const Graph& g) {
      if (InSubtree && Predicate(u, g))
        Visited.insert(u);
    }

    void finish_vertex(VertexID u, const Graph& g) {
      if (Root == u)
        InSubtree = false;
    }

  private:
    std::function<bool(VertexID, Graph const&)> DefaultPredicate{ [](VertexID, Graph const&){ return true; } };
    VertexID Root;
    std::unordered_set<VertexID> &Visited;
    std::function<bool(VertexID, Graph const&)> &Predicate;
    bool InSubtree{false};

  }; // end ReachableVisitor

}