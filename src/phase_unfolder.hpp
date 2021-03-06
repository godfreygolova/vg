#ifndef VG_PHASE_UNFOLDER_HPP_INCLUDED
#define VG_PHASE_UNFOLDER_HPP_INCLUDED

/** \file
 * This file contains the PhaseUnfolder, which replaces pruned regions of
 * the graph with paths supported by XG paths or GBWT threads.
 */

#include "vg.hpp"
#include "xg.hpp"

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <utility>
#include <vector>

#include <gbwt/gbwt.h>
#include <gcsa/support.h>

namespace vg {

/**
 * Transforms the pruned subregions of the input graph into collections of
 * disconnected distinct traversal haplotypes. Use in combination with
 * pruning to simplify the graph for GCSA2 indexing without losing observed
 * variation.
 * Requires the XG index of the original graph and an empty GBWT index or
 * an GBWT index of the original graph.
 * Note: A "border-to-border" path is a) a path from a border node to
 * another border node containing no other border nodes; or b) maximal path
 * starting from a border node encountering no other border nodes.
 * Note: PhaseUnfolder only considers paths of length >= 2.
 */
class PhaseUnfolder {
public:
    typedef gbwt::SearchState                 search_type;
    typedef std::vector<gbwt::node_type>      path_type;
    typedef std::pair<search_type, path_type> state_type;

    /**
     * Make a new PhaseUnfolder backed by the given XG and GBWT indexes.
     * These indexes must represent the same original graph. 'next_node' should
     * usually be max_node_id() + 1 in the original graph.
     */
    PhaseUnfolder(const xg::XG& xg_index, const gbwt::GBWT& gbwt_index, vg::id_t next_node);

    /**
     * Unfold the pruned regions in the input graph:
     *
     * - Determine the connected components of edges missing from the input
     * graph, as determined by the XG paths and GBWT threads.
     *
     * - For each component, find all border-to-border paths and threads
     * supported by the indexes. Then unfold the component by duplicating the
     * nodes, so that the paths are disjoint, except for their endpoints.
     *
     * - Extend the input graph with the unfolded components.
     */
    void unfold(VG& graph, bool show_progress = false);

    /**
     * Restore the edges on XG paths. This is effectively the same as
     * unfolding with an empty GBWT index, except that the inserted nodes will
     * have their original identifiers.
     */
    void restore_paths(VG& graph, bool show_progress = false) const;

    /**
     * Verify that the graph contains the XG paths and the GBWT threads in the
     * backing indexes. Returns the number of paths for which the verification
     * failed. Uses OMP threads.
     */
    size_t verify_paths(VG& unfolded) const;

    /**
     * Write the mapping to the specified file with a header. The file will
     * contain mappings from header.next_node - header.mapping_size
     * (inclusive) to header.next_node (exclusive).
     */
    void write_mapping(const std::string& filename) const;

    /**
     * Replace the existing node mapping with the one loaded from the file.
     * This should be used before calling unfold(). The identifiers for new
     * duplicated nodes will follow the ones in the loaded mapping.
     */
    void read_mapping(const std::string& filename);

    /**
     * Get the id of the corresponding node in the original graph.
     */
    vg::id_t get_mapping(vg::id_t node) const;

    /**
     * Create an edge between two node orientations.
     */
    static Edge make_edge(gbwt::node_type from, gbwt::node_type to) {
        return xg::make_edge(gbwt::Node::id(from), gbwt::Node::is_reverse(from),
                             gbwt::Node::id(to), gbwt::Node::is_reverse(to));
    }

private:
    /**
     * Generate a complement graph consisting of the edges that are in the
     * GBWT index but not in the input graph. Split the complement into
     * disjoint components and return the components.
     */
    std::list<VG> complement_components(VG& graph, bool show_progress);

    /**
     * Generate all border-to-border paths in the component supported by the
     * GBWT index. Unfold the path by duplicating the inner nodes so that the
     * paths become disjoint, except for their endpoints.
     */
    size_t unfold_component(VG& component, VG& graph, VG& unfolded);

    /**
     * Generate all paths or threads starting from the given node that are
     * supported by the corresponding index and end at the border. Insert the
     * generated paths into the set in canonical order.
     */
    void generate_paths(VG& component, vg::id_t from);
    void generate_threads(VG& component, vg::id_t from);

    /**
     * Create or extend the state with the given node orientation, and insert
     * it into the stack if it is supported by the GBWT index.
     */
    void create_state(vg::id_t node, bool is_reverse);
    bool extend_state(state_type state, vg::id_t node, bool is_reverse);

    /// Insert the path into the set in the canonical orientation.
    void insert_path(const path_type& path);

    /// XG and GBWT indexes for the original graph.
    const xg::XG&     xg_index;
    const gbwt::GBWT& gbwt_index;

    /// Mapping from duplicated nodes to original ids.
    gcsa::NodeMapping mapping;

    /// Internal data structures for the current component.
    std::unordered_set<vg::id_t> border;
    std::stack<state_type>       states;

    /// Tries for the unfolded prefixes and reverse suffixes.
    /// prefixes[(from, to)] is the mapping for to, and
    /// suffixes[(from, to)] is the mapping for from.
    std::unordered_map<std::pair<gbwt::node_type, gbwt::node_type>, gbwt::node_type> prefixes, suffixes;
    std::unordered_set<std::pair<gbwt::node_type, gbwt::node_type>> crossing_edges;
};

}

#endif 
