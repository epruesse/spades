/*
 * next_path_searcher.hpp
 *
 *  Created on: Sep 27, 2013
 *      Author: ira
 */

#ifndef NEXT_PATH_SEARCHER_HPP_
#define NEXT_PATH_SEARCHER_HPP_

#include <set>
#include <vector>
#include <map>

#include "../graph_pack.hpp"
#include "../debruijn_graph.hpp"
#include "bidirectional_path.hpp"
#include "pe_utils.hpp"

namespace path_extend {
using debruijn_graph::Graph;
using std::set;
using std::vector;
using std::multimap;

class Edge {
public:
    Edge(const Graph& g, EdgeId id, Edge* prev_e, size_t dist, int gap = 0)
            : g_(g),
              id_(id),
              prev_edge_(prev_e),
              dist_(dist),
              gap_(gap) {
    }
    ~Edge() {
        for (size_t i = 0; i < out_edges_.size(); ++i) {
            delete out_edges_[i];
        }
        for (size_t i = 0; i < not_out_edges_.size(); ++i) {
            delete not_out_edges_[i];
        }
    }
    Edge* AddOutEdge(EdgeId edge, int gap = 0) {
        return AddIfNotExist(edge, gap, out_edges_);
    }
    Edge* AddIncorrectOutEdge(EdgeId edge, int gap = 0) {
        for (size_t i = 0; i < out_edges_.size(); ++i) {
            if (out_edges_[i]->GetId() == edge) {
                not_out_edges_.push_back(out_edges_[i]);
                out_edges_.erase(out_edges_.begin() + i);
                break;
            }
        }
        return AddIfNotExist(edge, gap, not_out_edges_);
    }
    Edge* AddPath(const BidirectionalPath& path, size_t from) {
        Edge* e = this;
        for (size_t i = from; i < path.Size(); ++i) {
            e = e->AddOutEdge(path.At(i), path.GapAt(i));
        }
        return e;
    }
    int GetOutEdgeIndex(EdgeId edge) const {
        return GetEdgeIndex(edge, out_edges_);
    }
    int GetIncorrectEdgeIndex(EdgeId edge) const {
        return GetEdgeIndex(edge, not_out_edges_);
    }
    size_t OutSize() const {
        return out_edges_.size();
    }
    Edge* GetOutEdge(size_t i) const {
        return out_edges_[i];
    }
    BidirectionalPath GetPrevPath(size_t from) const {
        BidirectionalPath result(g_);
        vector<pair<EdgeId, int> > edges_wgaps;
        const Edge* e = this;
        edges_wgaps.push_back(make_pair(e->GetId(), e->Gap()));
        while (e->prev_edge_) {
            e = e->prev_edge_;
            edges_wgaps.push_back(make_pair(e->GetId(), e->Gap()));
        }
        for (int i = (int) edges_wgaps.size() - 1 - (int) from; i >= 0; i--) {
            result.PushBack(edges_wgaps[i].first, edges_wgaps[i].second);
        }
        return result;
    }
    bool IsCorrect() {
        Edge* e = this;
        while (e->prev_edge_) {
            if (e->prev_edge_->GetOutEdgeIndex(e->GetId()) == -1) {
                TRACE("after " << g_.int_id(e->prev_edge_->GetId()) << " souldn't go " << g_.int_id(e->GetId()));
                return false;
            }
            e = e->prev_edge_;
        }
        return true;
    }

    bool IsCycled(const GraphCoverageMap& cov_map) {
        BidirectionalPath path = GetPrevPath(0);
        size_t identical_edges = 0;
        LoopDetector loop_detect(&path, cov_map);
        bool is_cycled = loop_detect.IsCycled(cfg::get().pe_params.param_set.loop_removal.mp_max_loops, identical_edges);
        if (path.Size() > 1 && path.At(path.Size() - 1) == path.At(path.Size() - 2)) {
            is_cycled = true;
        }
        return is_cycled;
    }

    Edge* RemoveCycle(const GraphCoverageMap& cov_map, size_t min_length) {
        size_t skip_identical_edges = 0;
        BidirectionalPath path = GetPrevPath(0);
        LoopDetector loop_detect(&path, cov_map);
        loop_detect.IsCycled(cfg::get().pe_params.param_set.loop_removal.mp_max_loops, skip_identical_edges);
        size_t remove = loop_detect.EdgesToRemove(skip_identical_edges, false);
        Edge* prev_edge = this;
        Edge* last_loop_edge = this;
        for (size_t i = 0; i < remove && prev_edge->Length() > min_length; ++i) {
            last_loop_edge = prev_edge;
            prev_edge = last_loop_edge->prev_edge_;
        }
        prev_edge->AddIncorrectOutEdge(last_loop_edge->GetId());
        VERIFY(prev_edge->Length() >= min_length);
        return prev_edge;
    }

    bool EqualBegins(const BidirectionalPath& path, int pos) {
        BidirectionalPath p = this->GetPrevPath(0);
        return path_extend::EqualBegins(path, (size_t) pos, p, p.Size() - 1, true);
    }
    size_t Length() const {
        return dist_;
    }
    set<Edge*> GetPrevEdges(size_t dist) {
        size_t init_len = Length();
        Edge* e = this;
        set<Edge*> result;
        while (e && init_len - e->Length() < dist) {
            result.insert(e);
            e = e->prev_edge_;
        }
        return result;
    }
    EdgeId GetId() const {
        return id_;
    }
    int Gap() const {
        return gap_;
    }
private:
    Edge* AddIfNotExist(EdgeId e, int gap, vector<Edge*>& vect) {
        int i = GetEdgeIndex(e, vect);
        if (i != -1) {
            return vect[i];
        }
        size_t dist = dist_ + gap + g_.length(e);
        vect.push_back(new Edge(g_, e, this, dist, gap));
        return vect.back();
    }
    int GetEdgeIndex(EdgeId e, const vector<Edge*>& vect) const {
        for (size_t i = 0; i < vect.size(); ++i) {
            if (vect[i]->GetId() == e)
                return (int) i;
        }
        return -1;
    }
    const Graph& g_;
    EdgeId id_;
    vector<Edge*> out_edges_;
    vector<Edge*> not_out_edges_;
    Edge* prev_edge_;
    size_t dist_;
    int gap_;

protected:
    DECL_LOGGER("NextPathSearcher")
};
struct PathWithDistance {
    PathWithDistance(BidirectionalPath p, int dist)
            : p_(p),
              dist_(dist) {

    }
    BidirectionalPath p_;
    int dist_;
};
class NextPathSearcher {
public:
    typedef set<EdgeWithDistance, EdgeWithDistance::DistanceComparator> EdgeSet;
    typedef multimap<EdgeId, PathWithDistance> ConstructedPathT;

    NextPathSearcher(const Graph& g, const GraphCoverageMap& cover_map, size_t search_dist, PathsWeightCounter weight_counter);
    set<BidirectionalPath*> FindNextPaths(const BidirectionalPath& path, EdgeId begin_edge, bool jump = true);
    set<BidirectionalPath*> ScaffoldTree(const BidirectionalPath& path);
private:
    bool IsOutTip(VertexId v) const;
    bool IsInTip(VertexId v) const;
    void GrowPath(const BidirectionalPath& init_path, Edge* e, size_t max_len, vector<Edge*>& to_add);
    Edge* AddPath(const BidirectionalPath& init_path, Edge* prev_e, size_t max_len, const BidirectionalPath& path, size_t start_pos);
    Edge* AnalyzeBubble(const BidirectionalPath& p, EdgeId buldge_edge, size_t gap, Edge* prev_edge);

    void ScaffoldTip(const BidirectionalPath& path, Edge * current_path, vector<Edge*>& result_edges, vector<Edge*>& stopped_paths, vector<Edge*>& to_add,
                     bool jump);
    void ScaffoldChristmasTree(const BidirectionalPath& path, Edge * current_path, vector<Edge*>& to_add);
    void Scaffold(const BidirectionalPath& init_path, Edge* current_path, ConstructedPathT& constructed_paths, set<EdgeId>& seeds, bool is_gap);
    void FindScaffoldingCandidates(const BidirectionalPath& init_path, Edge* current_path, EdgeSet& candidate_set);
    void FindScaffoldingCandidates(EdgeId e, size_t distance_to_tip, vector<EdgeWithDistance>& jump_edges);
    void OrderScaffoldingCandidates(EdgeSet& candidate_set, const BidirectionalPath& init_path, Edge* current_path, ConstructedPathT& constructed_paths, set<EdgeId>& seeds, bool is_gap);
    void RemoveRedundant(ConstructedPathT& constructed_paths) const;
    void ConvertPaths(const ConstructedPathT& constructed_paths, Edge* current_path, vector<Edge*>& to_add) const;
    void ProcessScaffoldingCandidate(EdgeWithDistance& e, EdgeSet& candidate_set, Edge* current_path, size_t grown_path_len,
                                     ConstructedPathT& constructed_paths, bool is_gap);
    int EstimateGapForPath(EdgeSet& candidate_set, const BidirectionalPath& p);
    void AddConstructedPath(const BidirectionalPath& cp, size_t from, int gap, ConstructedPathT& constructed_paths);
    void FilterBackPaths(set<BidirectionalPath*>& back_paths, EdgeId edge_to_reach, set<BidirectionalPath*>& reached_paths, size_t max_len = -1UL);
    void JoinPathsByGraph(ConstructedPathT& constructed_paths);
    void JoinPathsByPI(ConstructedPathT& constructed_paths);

    const Graph& g_;
    const GraphCoverageMap& cover_map_;
    size_t search_dist_;
    PathsWeightCounter weight_counter_;
    size_t long_edge_len_;
    size_t max_paths_;

protected:
    DECL_LOGGER("NextPathSearcher")
};

inline NextPathSearcher::NextPathSearcher(const Graph& g, const GraphCoverageMap& cover_map, size_t search_dist, PathsWeightCounter weight_counter)
        : g_(g),
          cover_map_(cover_map),
          search_dist_(search_dist),
          weight_counter_(weight_counter),
          long_edge_len_(500),
          max_paths_(1000) {

}
inline set<BidirectionalPath*> NextPathSearcher::ScaffoldTree(const BidirectionalPath& path) {
    Edge* start_e = new Edge(g_, path.At(0), NULL, g_.length(path.At(0)) + path.GapAt(0), path.GapAt(0));
    Edge* e = start_e->AddPath(path, 1);
    //jump forward when too much paths
    DEBUG("Scaffolding tree for edge " << g_.int_id(start_e->GetId()));
    path.Print();
    vector<Edge*> result_edges;
    ScaffoldChristmasTree(path, e, result_edges);
    std::set<BidirectionalPath*> result_paths;
    DEBUG("adding paths " << result_edges.size());
    for (size_t i = 0; i < result_edges.size(); ++i) {
        BidirectionalPath result_path = result_edges[i]->GetPrevPath(path.Size());
        if (!result_path.Empty())
            result_paths.insert(new BidirectionalPath(result_path));
    }
    delete start_e;
    DEBUG( "for path " << path.GetId() << " several extension " << result_paths.size());
    return result_paths;
}
inline set<BidirectionalPath*> NextPathSearcher::FindNextPaths(const BidirectionalPath& path, EdgeId begin_edge, bool jump) {
    TRACE("begin find next paths");
    vector<Edge*> grow_paths;
    vector<Edge*> result_edges;
    vector<Edge*> stopped_paths;
    size_t max_len = search_dist_ + path.Length();
    std::set<Edge*> used_edges;

    Edge* start_e = new Edge(g_, path.At(0), NULL, g_.length(path.At(0)) + path.GapAt(0), path.GapAt(0));
    Edge* e = start_e->AddPath(path, 1);
    if (begin_edge != path.Back()) {
        e = e->AddOutEdge(begin_edge);
        DEBUG( "Try to find next path for path with edge " << g_.int_id(begin_edge));
    } else {
        DEBUG( "Try to search for path with last edge " << g_.int_id(path.Back()) << " Scaffolding: " << jump << ", next edges " << g_.OutgoingEdgeCount(g_.EdgeEnd(path.Back())));
    }
    //size_t init_length = e->Length();
    grow_paths.push_back(e);

    size_t ipath = 0;
    while (ipath < grow_paths.size()) {
        TRACE("Processing path " << ipath << " of " << grow_paths.size());
        Edge* current_path = grow_paths[ipath++];
        TRACE(" edge " << g_.int_id(current_path->GetId()));
        if (!current_path->IsCorrect() /*|| (current_path->IsCycled() && current_path->Length() > init_length) */|| used_edges.count(current_path) > 0) {
            used_edges.insert(current_path);
            continue;
        }
        used_edges.insert(current_path);
        if (current_path->Length() >= max_len) {
            result_edges.push_back(current_path);
            continue;
        }
        vector<Edge*> to_add;
        GrowPath(path, current_path, max_len, to_add);
        if (to_add.empty()) {
            DEBUG("scaffold tip");
            //path.Print();
            ScaffoldTip(path, current_path, result_edges, stopped_paths, to_add, jump);
        }
        grow_paths.insert(grow_paths.end(), to_add.begin(), to_add.end());

        if (used_edges.size() > max_paths_) {
            DEBUG("too many paths");
            delete start_e;
            return set<BidirectionalPath*>();
        }
    }

    std::set<BidirectionalPath*> result_paths;
    TRACE("adding paths " << result_edges.size());
    for (size_t i = 0; i < result_edges.size(); ++i) {
        BidirectionalPath result_path = result_edges[i]->GetPrevPath(path.Size());
        if (!result_path.Empty()) {
            //result_path.Print();
            result_paths.insert(new BidirectionalPath(result_path));
        }
    }
    delete start_e;
    DEBUG( "for path " << path.GetId() << " several extension " << result_paths.size());
    return result_paths;
}

inline Edge* NextPathSearcher::AnalyzeBubble(const BidirectionalPath& p, EdgeId buldge_edge, size_t gap, Edge* prev_edge) {
    EdgeId max_edge = buldge_edge;
    double max_w = 0.0;
    bool analyzed = true;
    for (EdgeId e : g_.OutgoingEdges(g_.EdgeStart(buldge_edge))) {
        if (prev_edge->GetOutEdgeIndex(e) == -1 && prev_edge->GetIncorrectEdgeIndex(e) == -1) {
            analyzed = false;
        }
        double w = weight_counter_.CountPairInfo(p, 0, p.Size(), e, gap);
        if (math::gr(w, max_w) || (math::eq(w, max_w) && g_.int_id(e) < g_.int_id(max_edge))) {
            max_w = w;
            max_edge = e;
        }
    }
    if (analyzed) {
        int out_index = prev_edge->GetOutEdgeIndex(buldge_edge);
        if (out_index == -1) {
            return NULL;
        } else {
            return prev_edge->GetOutEdge(out_index);
        }
    }
    for (EdgeId e : g_.OutgoingEdges(g_.EdgeStart(buldge_edge))) {
        if (e == max_edge) {
            //DEBUG("correct " << g_.int_id(e));
            prev_edge->AddOutEdge(e);
        } else {
            //DEBUG("incorrect " << g_.int_id(e));
            prev_edge->AddIncorrectOutEdge(e);
        }
    }
    if (max_edge == buldge_edge) {
        Edge* res = prev_edge->AddOutEdge(buldge_edge);
        //DEBUG("correct edge added " << (res != NULL) << " " << (bool)(!res) << " "<< g_.int_id(res->GetId()));
        return res;
    }
    //DEBUG("incorrect edge");
    return NULL;
}

inline Edge* NextPathSearcher::AddPath(const BidirectionalPath& init_path, Edge* prev_e, size_t max_len, const BidirectionalPath& path, size_t start_pos) {
    Edge* e = prev_e;
    for (size_t ie = start_pos; ie < path.Size() && e->Length() <= max_len; ++ie) {
        int inext = e->GetOutEdgeIndex(path.At(ie));
        if (inext != -1) {
            e = e->GetOutEdge(inext);
            continue;
        }
        if (e->GetIncorrectEdgeIndex(path.At(ie)) != -1) {
            break;
        }
        if (InBuble(path.At(ie), g_)) {
            size_t gap = path.Length() - path.LengthAt(ie);
            Edge* next_edge = AnalyzeBubble(init_path, path.At(ie), gap, e);
            if (!next_edge)
                break;
            e = next_edge;
        } else if (e->GetId() != path.At(ie)) {
            Edge* next_edge = e->AddOutEdge(path.At(ie));
            if (next_edge->IsCycled(cover_map_)) {
                //e->AddIncorrectOutEdge(path.At(ie));
                //e = next_edge->RemoveCycle(init_path.Length());
            	TRACE("remove cycle");
                break;
            } else {
                e = next_edge;
            }
        }
    }
    return e;
}

inline void NextPathSearcher::GrowPath(const BidirectionalPath& init_path, Edge* e, size_t max_len, vector<Edge*>& to_add) {
	TRACE("in growing path");
    if (!e->IsCorrect()){
    	TRACE("incorrect");
        return;
    }
    for (EdgeId next_edge : g_.OutgoingEdges(g_.EdgeEnd(e->GetId()))) {
        set<BidirectionalPath*> cov_paths = cover_map_.GetCoveringPaths(next_edge);
        bool path_ended_here = cov_paths.size() == 0;
        TRACE("cov_map size " << cov_paths.size() << " for edge " << g_.int_id(next_edge));
        bool found_path = false;
        for (auto inext_path = cov_paths.begin(); inext_path != cov_paths.end(); ++inext_path) {
            vector<size_t> positions = (*inext_path)->FindAll(next_edge);
            for (size_t ipos = 0; ipos < positions.size(); ++ipos) {
                size_t pos = positions[ipos];
                if (pos == 0 || e->EqualBegins(**inext_path, (int) pos - 1)) {
                	TRACE("Found equal begin");
                    Edge* next_edge = AddPath(init_path, e, max_len, **inext_path, pos);
                    if (next_edge) {
                        if (e == next_edge) {
                            path_ended_here = true;
                        } else {
                            to_add.push_back(next_edge);
                            found_path = true;
                        }
                    } else {
                        path_ended_here = true;
                    }
                }
            }
        }
        TRACE("path ended here for edge " << g_.int_id(e->GetId())  << " is " << path_ended_here << " for next edge " << g_.int_id(next_edge));
        TRACE("out " << (e->GetOutEdgeIndex(next_edge) == -1) << " incorrect " <<  (e->GetIncorrectEdgeIndex(next_edge) == -1));
        if (path_ended_here || (e->GetOutEdgeIndex(next_edge) == -1 && e->GetIncorrectEdgeIndex(next_edge) == -1)) {
        	TRACE("in buble " << InBuble(next_edge, g_) << " e != next " << (e->GetId() != next_edge));
            if (InBuble(next_edge, g_)) {
            	TRACE("analyze bubble " << (!(bool)AnalyzeBubble(init_path, next_edge, 0, e)));
            }
            if ((!InBuble(next_edge, g_) || !AnalyzeBubble(init_path, next_edge, 0, e)) && e->GetId() != next_edge)
                to_add.push_back(e->AddOutEdge(next_edge));
        }
        if (!found_path && InBuble(next_edge, g_) && (e->GetIncorrectEdgeIndex(next_edge) != -1)) {
            to_add.push_back(e->AddOutEdge(next_edge));
        }
    }
    stringstream str;
    str << " for edge " << g_.int_id(e->GetId()) << " add ";
    for (Edge* e1 : to_add) {
        str << " " << g_.int_id(e1->GetId());
    }
    TRACE(str.str());
}

inline void NextPathSearcher::ScaffoldTip(const BidirectionalPath& path, Edge * current_path, vector<Edge*>& result_edges, vector<Edge*>& stopped_paths,
                                          vector<Edge*>& to_add, bool jump) {

    if (jump) {
        //jump forward when tip
        DEBUG("Scaffolding");
        ConstructedPathT constructed_paths;
        set<EdgeId> seeds;
        Scaffold(path, current_path, constructed_paths, seeds, true);
        if (constructed_paths.empty()) {
            stopped_paths.push_back(current_path);
        } else {
            DEBUG("Jumped! " << to_add.size());
            ConvertPaths(constructed_paths, current_path, to_add);
        }
    } else {
        DEBUG("Not scaffolding because going back");
        result_edges.push_back(current_path);
    }
}

inline void NextPathSearcher::ScaffoldChristmasTree(const BidirectionalPath& path, Edge * current_path, vector<Edge*>& to_add) {
    //jump forward when too much paths
    DEBUG("========= Scaffolding when too many paths =========");
    ConstructedPathT constructed_paths;
    set<EdgeId> seeds;
    Scaffold(path, current_path, constructed_paths, seeds, false);
    DEBUG("scaffolding candidates for tree " << constructed_paths.size());
    for (auto iter = constructed_paths.begin(); iter != constructed_paths.end(); ++iter){
        iter->second.p_.Print();
    }
    if (constructed_paths.size() > 0 &&
            constructed_paths.upper_bound(constructed_paths.begin()->first) == constructed_paths.end()) {
        DEBUG("All paths from one seed");
        int first_seed_pos = 0;
        auto p = constructed_paths.begin();
        if (constructed_paths.size() > 1) {
            //Searching for path with max number of seeds
            DEBUG("Many paths from one seed " << constructed_paths.size());
            int max_seeds = 0;
            for (auto it = constructed_paths.begin(); it != constructed_paths.end(); ++it) {
                int seed_count = 0;
                for (EdgeId e : seeds) {
                    if (it->second.p_.Contains(e)) {
                        ++seed_count;
                    }
                }
                if (seed_count > max_seeds) {
                    max_seeds = seed_count;
                    p = it;
                }
            }
            DEBUG("Max seed containing contains " << max_seeds << " seeds");
            //Looking for first seed in that path
            PathWithDistance& winner(p->second);
            first_seed_pos = (int) winner.p_.Size() + 1;
            for (EdgeId e : seeds) {
                int pos = winner.p_.FindFirst(e);
                if (pos != -1)
                    first_seed_pos = min(pos, first_seed_pos);
            }
            VERIFY(first_seed_pos != (int) winner.p_.Size() + 1);
            DEBUG("First seed position " << first_seed_pos << " seeds");
        }
        PathWithDistance& path_to_add(p->second);
        int distance = path_to_add.dist_ + (int) path_to_add.p_.Length() - (int) path_to_add.p_.LengthAt(first_seed_pos);
        to_add.push_back(current_path->AddOutEdge(path_to_add.p_[first_seed_pos], distance));
        to_add.back() = to_add.back()->AddPath(path_to_add.p_, first_seed_pos + 1);
    }
    DEBUG("========= Done scaffolding when too many paths =========");
}

inline void NextPathSearcher::Scaffold(const BidirectionalPath& init_path, Edge* current_path,
                                       ConstructedPathT& constructed_paths, set<EdgeId>& seeds, bool is_gap) {

    EdgeSet candidate_set;
    FindScaffoldingCandidates(init_path, current_path, candidate_set);

    DEBUG("Scafolding candidates");
    for (EdgeWithDistance e : candidate_set) {
        DEBUG( "Edge " << g_.int_id(e.e_) << " (" << g_.length(e.e_) << ")" << ", distance " << e.d_);
    }

    OrderScaffoldingCandidates(candidate_set, init_path, current_path, constructed_paths, seeds, is_gap);
}

inline void NextPathSearcher::FindScaffoldingCandidates(const BidirectionalPath& init_path, Edge* current_path, EdgeSet& candidate_set) {
    set<EdgeId> path_end;
    set<Edge*> prev_edges = current_path->GetPrevEdges(search_dist_);
    for (Edge* e : prev_edges) {
        path_end.insert(e->GetId());
        path_end.insert(g_.conjugate(e->GetId()));
    }
    map<EdgeId, vector<int> > candidates;
    //current_path->GetPrevPath(0).Print();
    TRACE(current_path->Length() << " " << init_path.Length());
    VERIFY(current_path->Length() >= init_path.Length());
    size_t grown_path_len = current_path->Length() - init_path.Length();
    TRACE("Path already grown to " << grown_path_len);

    for (size_t i = 0; i < init_path.Size(); ++i) {
        vector<EdgeWithDistance> jump_edges;
        size_t distance_to_tip = init_path.LengthAt(i) + grown_path_len;
        FindScaffoldingCandidates(init_path[i], distance_to_tip, jump_edges);
        for (EdgeWithDistance e : jump_edges) {
            if (candidates.find(e.e_) == candidates.end()) {
                candidates[e.e_] = vector<int>();
            }
            candidates[e.e_].push_back(
            /*max(e.d_ - (int) distance_to_tip, 100)*/100);
        }
    }

    for (std::pair<EdgeId, vector<int> > e : candidates) {
        if (path_end.count(e.first) > 0) {
            continue;
        }
        int avg_distance = 0;
        TRACE( "All distances for edge " << g_.int_id(e.first) << " (" << g_.length(e.first) << ")");
        for (int dist : e.second) {
        	TRACE(dist);
            avg_distance += dist;
        }
        avg_distance /= (int) e.second.size();
        candidate_set.insert(EdgeWithDistance(e.first, avg_distance));
    }
}

inline void NextPathSearcher::FindScaffoldingCandidates(EdgeId e, size_t distance_to_tip, vector<EdgeWithDistance>& jump_edges) {
    if (g_.length(e) < long_edge_len_ || distance_to_tip - g_.length(e) >= search_dist_)
        return;

    TRACE("Edge " << g_.int_id(e) << ", length " << g_.length(e));
    TRACE( distance_to_tip << " " << distance_to_tip - g_.length(e) << " " << search_dist_);

    set<EdgeId> candidate_edges;
    int min_distance = std::max((int) distance_to_tip - (int) weight_counter_.GetLib().GetLeftVar(), 0);
    int max_distance = (int) search_dist_ + (int) g_.length(e);
    TRACE("Looking in range " << min_distance << " " << max_distance);
    weight_counter_.FindJumpCandidates(e, min_distance, max_distance, long_edge_len_, candidate_edges);
    weight_counter_.FindJumpEdges(e, candidate_edges, min_distance, max_distance, jump_edges);
    TRACE("Found " << jump_edges.size() << " candidate(s) from  this edge");
}

inline void NextPathSearcher::OrderScaffoldingCandidates(EdgeSet& candidate_set, const BidirectionalPath& init_path,
                                                         Edge* current_path, ConstructedPathT& constructed_paths,
                                                         set<EdgeId>& seeds, bool is_gap) {
    size_t grown_path_len = current_path->Length() - init_path.Length();

    TRACE("Order Scaffolding Candidates, is gap " << is_gap);
    for (EdgeWithDistance e : candidate_set) {
        TRACE("e " << g_.int_id(e.e_));
        if (constructed_paths.count(e.e_) > 0) {
            TRACE("visited");
            continue;
        }
        ProcessScaffoldingCandidate(e, candidate_set, current_path, grown_path_len, constructed_paths, is_gap);
        for (auto p1 = constructed_paths.begin(); p1 != constructed_paths.end(); ++p1) {
        	TRACE("current constructed paths " << g_.int_id(p1->first));
            //p1->second.p_.Print();
        }

    }
    RemoveRedundant(constructed_paths);
    for (auto it = constructed_paths.begin(); it != constructed_paths.end(); ++it) {
        seeds.insert(it->first);
    }
    JoinPathsByGraph(constructed_paths);
    JoinPathsByPI(constructed_paths);
    RemoveRedundant(constructed_paths);
}

inline void NextPathSearcher::ConvertPaths(const ConstructedPathT& constructed_paths, Edge* current_path, vector<Edge*>& to_add) const {
    for (auto edge = constructed_paths.begin(); edge != constructed_paths.end(); ++edge) {
        to_add.push_back(current_path->AddOutEdge(edge->second.p_[0], edge->second.dist_));
        to_add.back() = to_add.back()->AddPath(edge->second.p_, 1);
    }
}

inline void NextPathSearcher::RemoveRedundant(ConstructedPathT& constructed_paths) const {
    for (auto edge = constructed_paths.begin(); edge != constructed_paths.end();) {
        if (edge->second.p_.Empty()) {
            edge = constructed_paths.erase(edge);
        } else {
            ++edge;
        }
    }
}

inline void NextPathSearcher::ProcessScaffoldingCandidate(EdgeWithDistance& e, EdgeSet& candidate_set, Edge* current_path, size_t grown_path_len,
                                                          ConstructedPathT& constructed_paths, bool is_gap) {
    bool looking_for_tip = is_gap;
    //Search back from e till tip or maximim length back
    TRACE(" === Searching back === ");
    TRACE( "Distances: search = " << search_dist_ << ", grown = " << grown_path_len << ", estimated gap = " << e.d_);
    VERIFY(search_dist_ >= grown_path_len);
    VERIFY((int) search_dist_ >= e.d_);

    size_t max_length_back = search_dist_ - grown_path_len;
    TRACE(search_dist_ << " " << grown_path_len);
    TRACE( "Searchin for edge of length " << g_.length(e.e_) << " to dist " << max_length_back);
    NextPathSearcher back_searcher(g_, cover_map_, max_length_back, weight_counter_);
    BidirectionalPath jumped_edge(g_, g_.conjugate(e.e_));
    set<BidirectionalPath*> back_paths = back_searcher.FindNextPaths(jumped_edge, jumped_edge.Back(), false);
    TRACE(" === DONE SEARCHING === ");
    TRACE("Found " << back_paths.size() << " is tip " << IsInTip(g_.EdgeStart(e.e_)) << " look for tip " << looking_for_tip);

    if (back_paths.empty()) {
        if (IsInTip(g_.EdgeStart(e.e_)) && looking_for_tip) {
        	TRACE( "Added tip edge " << g_.int_id(e.e_) << " (" << g_.length(e.e_) << ")" << ", distance " << e.d_);
            constructed_paths.insert(make_pair(e.e_, PathWithDistance(BidirectionalPath(g_, e.e_), e.d_)));
        } else if (!IsInTip(g_.EdgeStart(e.e_)) && !looking_for_tip) {
            constructed_paths.insert(make_pair(e.e_, PathWithDistance(BidirectionalPath(g_, e.e_), e.d_)));
        }
    } else {
    	TRACE("Found several back paths " << back_paths.size());
        set<BidirectionalPath*> reached_paths;
        FilterBackPaths(back_paths, g_.conjugate(current_path->GetId()), reached_paths, search_dist_ - grown_path_len);
        //Found a path back to the init path
        if (reached_paths.size() > 0 && !looking_for_tip) {
        	TRACE("Found " << reached_paths.size() << " direct path(s) back");
            int i = 0;
            for (BidirectionalPath* p : reached_paths) {
            	TRACE("Processing reached path " << i++);
                BidirectionalPath cp = p->Conjugate();
                //Adding jumped edge since its not included in the path
                cp.PushBack(e.e_);
                //cp.Print();
                int reached_edge_pos = cp.FindLast(current_path->GetId());
                VERIFY(reached_edge_pos != -1);
                AddConstructedPath(cp, reached_edge_pos + 1, 0, constructed_paths);
            }
        } else if (reached_paths.size() > 0 && looking_for_tip) {
            WARN("Impossible: back path reaches tip");
        } else if (looking_for_tip) {
        	TRACE( "Found " << back_paths.size() << " path(s) going back to tip");
            int i = 0;
            for (BidirectionalPath* p : back_paths) {
                DEBUG("Processing tip path " << i++);
                BidirectionalPath cp = p->Conjugate();
                //Adding jumped edge since its not included in the path
                cp.PushBack(e.e_);
                AddConstructedPath(cp, 0, EstimateGapForPath(candidate_set, cp), constructed_paths);
            }
        }
    }
    for (BidirectionalPath* p : back_paths) {
        delete p;
    }
}

inline int NextPathSearcher::EstimateGapForPath(EdgeSet& candidate_set, const BidirectionalPath& p) {
    int gap = 0;
    int count = 0;
    for (EdgeWithDistance e : candidate_set) {
        int pos = p.FindFirst(e.e_);
        if (pos != -1) {
            size_t length_to_e = 0;
            for (int i = 0; i < pos; ++i) {
                length_to_e += p.LengthAt(i);
            }
            gap += e.d_ - (int) length_to_e;
        }
        ++count;
    }
    gap /= count;
    return gap > 0 ? gap : 100;
}

inline void NextPathSearcher::AddConstructedPath(const BidirectionalPath& cp, size_t from, int gap, ConstructedPathT& constructed_paths) {
    VERIFY(!cp.Empty());

    //Adding if there is unique (candidate - tip)
    EdgeId candidate = cp.Back();
    for (auto it = constructed_paths.lower_bound(candidate); it != constructed_paths.upper_bound(candidate); ++it) {
        if (it->second.p_.Front() == cp.Front()) {
            return;
        }
    }

    TRACE("Adding path starting from " << from);
    constructed_paths.insert(make_pair(candidate, PathWithDistance(cp.SubPath(from), gap)));
    TRACE("add constructed path " << g_.int_id(candidate));
    //cp.Print();

    for (size_t i = 0; i < cp.Size() - 1; ++i) {
        EdgeId edge = cp[i];
        for (auto it = constructed_paths.lower_bound(edge); it != constructed_paths.upper_bound(edge); ++it) {
        	TRACE("found " << g_.int_id(edge));
            //it->second.p_.Print();
            TRACE("clear");
            it->second.p_.Clear();
        }
    }
}
inline bool NextPathSearcher::IsOutTip(VertexId v) const {
    if (g_.OutgoingEdgeCount(v) == 0) {
        return true;
    }
    if (g_.OutgoingEdgeCount(v) != 1) {
        return false;
    }
    EdgeId oute = *g_.OutgoingEdges(v).begin();
    for (EdgeId ine : g_.IncomingEdges(v)) {
        if (oute == ine) {
            return true;
        }
    }
    return false;
}
inline bool NextPathSearcher::IsInTip(VertexId v) const {
    if (g_.IncomingEdgeCount(v) == 0) {
        return true;
    }
    if (g_.IncomingEdgeCount(v) != 1) {
        return false;
    }
    EdgeId ine = *g_.IncomingEdges(v).begin();
    for (EdgeId oute : g_.OutgoingEdges(v)) {
        if (oute == ine) {
            return true;
        }
    }
    return false;
}
inline void NextPathSearcher::FilterBackPaths(set<BidirectionalPath*>& back_paths, EdgeId edge_to_reach, set<BidirectionalPath*>& reached_paths,
                                              size_t max_len) {
	TRACE("Searching for proper back paths");

    int i = 0;
    for (auto piter = back_paths.begin(); piter != back_paths.end();) {
        BidirectionalPath* p = *piter;
        VERIFY(!p->Empty());
        EdgeId last_e = p->Back();
        VertexId last_v = g_.EdgeEnd(last_e);
        TRACE("Processing path " << i++);
        //p->Print();
        if (p->FindFirst(edge_to_reach) != -1) {
            reached_paths.insert(p);
            ++piter;
        } else if (IsInTip(last_v) == 0 && p->Length() < max_len) {
            ++piter;
        } else {
            delete p;
            piter = back_paths.erase(piter);
        }
    }
}

inline void NextPathSearcher::JoinPathsByGraph(ConstructedPathT& constructed_paths) {
	TRACE("==  try to join paths using graph ==");
    for (auto p1 = constructed_paths.begin(); p1 != constructed_paths.end(); ++p1) {
        //p1->second.p_.Print();
    }
    TRACE("==  printed ==");

    //Removing edges whose seed is contained in any other path
    set<EdgeId> to_remove;
    for (auto p1 = constructed_paths.begin(); p1 != constructed_paths.end(); ++p1) {
        if (to_remove.count(p1->first) > 0) {
            continue;
        }
        for (auto p2 = constructed_paths.begin(); p2 != constructed_paths.end(); ++p2) {
            if (p1->first == p2->first || to_remove.count(p2->first) > 0) {
                continue;
            }
            if (p1->second.p_.Contains(p2->first)) {
                to_remove.insert(p2->first);
            }
        }
    }
    for (auto p = constructed_paths.begin(); p != constructed_paths.end(); ) {
        if (to_remove.count(p->first) > 0) {
            p = constructed_paths.erase(p);
        } else {
            ++p;
        }
    }
}

inline void NextPathSearcher::JoinPathsByPI(ConstructedPathT& constructed_paths) {
	DEBUG("==  try to join paths ===");
    for (auto p1 = constructed_paths.begin(); p1 != constructed_paths.end(); ++p1) {
        p1->second.p_.Print();
    }
    DEBUG("==  printed ===");

    //Checking paired info
    set<EdgeId> visited;
    for (auto p1 = constructed_paths.begin(); p1 != constructed_paths.end(); ++p1) {
        if (visited.count(p1->first) > 0) {
            continue;
        }
        for (auto p2 = constructed_paths.begin(); p2 != constructed_paths.end(); ++p2) {
            if (p1->first == p2->first) {
                continue;
            }
            BidirectionalPath& path1 = p1->second.p_;
            BidirectionalPath& path2 = p2->second.p_;
            bool has_pi = false;
            for (size_t i = 0; i < path1.Size(); ++i) {

                for (size_t j = 0; j < path2.Size(); ++j) {
                    size_t len_to_e2 = path2.Length() - path2.LengthAt(j);
                    size_t dist = path1.LengthAt(i) + len_to_e2;
                    size_t min_dist = (size_t) max(0, (int) dist - (int) weight_counter_.GetLib().GetLeftVar());
                    size_t max_dist = dist + search_dist_;
                    DEBUG("try to find pair info between " << g_.int_id(path1[i]) << " and  " << g_.int_id(path2[j])
                          << " distance from " << min_dist
                          <<" to " << max_dist);
                    if (path1[i] != path2[j] &&
                            weight_counter_.HasPI(path1[i], path2[j], min_dist, max_dist)) {
                        has_pi = true;
                        break;
                    }
                }
                if (has_pi) {
                    break;
                }
            }

            set<EdgeId> edges_path1;
            for (size_t i = 0; i < path1.Size(); ++i) {
                edges_path1.insert(path1.At(i));
            }
            for (size_t i = 0; i < path2.Size(); ++i) {
                if (edges_path1.count(path2.At(i)) > 0 || edges_path1.count(g_.conjugate(path2.At(i))) > 0) {
                    has_pi = false;
                }
            }
            if (has_pi) {
            	DEBUG("has pi from ");
                path1.Print();
                DEBUG("to");
                path2.Print();
                path1.PushBack(path2.Front(), 100);
                for (int i = 1; i < (int) path2.Size(); ++i) {
                    path1.PushBack(path2[i], path2.GapAt(i));
                }
                DEBUG("new path");
                path1.Print();
                path2.Clear();
                visited.insert(p2->first);
            }
        }
    }
}

}  // namespace path_extend
#endif /* NEXT_PATH_SEARCHER_HPP_ */
