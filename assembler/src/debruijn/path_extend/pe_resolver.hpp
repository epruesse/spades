//***************************************************************************
//* Copyright (c) 2011-2013 Saint-Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************

/*
 * pe_resolver.hpp
 *
 *  Created on: Mar 12, 2012
 *      Author: andrey
 */

#ifndef PE_RESOLVER_HPP_
#define PE_RESOLVER_HPP_

#include "path_extender.hpp"
#include "pe_io.hpp"

namespace path_extend {


class SimpleOverlapRemover {

protected:
	Graph& g_;

	GraphCoverageMap& coverageMap;

private:

	pair<int, int> ComparePaths(int startPos1, int startPos2,
			BidirectionalPath& path1, BidirectionalPath& path2, size_t maxOverlap){
		int curPos = startPos1;
		int lastPos2 = startPos2;
		int lastPos1 = curPos;
		curPos++;
		size_t diff_len = 0;
		while (curPos < (int)path1.Size()) {
			EdgeId currentEdge = path1[curPos];
			vector<size_t> poses2 = path2.FindAll(currentEdge);
			bool found = false;
			for (size_t pos2 = 0; pos2 < poses2.size(); ++pos2) {
				if ((int) poses2[pos2] > lastPos2) {
					if (diff_len > maxOverlap) {
						return make_pair(-1, -1);
					}
					lastPos2 = poses2[pos2];
					lastPos1 = curPos;
					found = true;
					break;
				}
			}
			if (!found) {
				diff_len += g_.length(currentEdge);
			} else {
				diff_len = 0;
			}
			curPos++;
		}
		return make_pair(lastPos1, lastPos2);
	}


	bool cutOverlaps(BidirectionalPath* path1, int path1_first, int path1_last,
			int size1, BidirectionalPath* path2, int path2_first,
			int path2_last, int size2,  bool delete_subpaths, bool delete_begins, bool delete_all) {
		if (path1_first == 0 && path1_last == size1 - 1 && delete_subpaths
				&& !path1->hasOverlapedBegin() && !path1->hasOverlapedEnd()) {
			DEBUG("delete path 1");
			path1->Clear();
		} else if (path2_first == 0 && path2_last == size2 - 1 && delete_subpaths
				&& !path2->hasOverlapedBegin() && !path2->hasOverlapedEnd()) {
			DEBUG("delete path 2");
			path2->Clear();
		} else if (path2_first == 0 && path1_first == 0 && delete_begins) {
			if (size1 < size2 && !path1->hasOverlapedBegin()) {
				DEBUG("delete begin path 1");
				path1->getConjPath()->PopBack(path1_last + 1);
			} else if (!path2->hasOverlapedBegin()){
				DEBUG("delete begin path 2");
				path2->getConjPath()->PopBack(path2_last + 1);
			}
		} else if ((path1_last == size1 - 1 && path2_last == size2 - 1) && delete_begins) {
			if (size1 < size2 && !path1->hasOverlapedEnd()) {
				DEBUG("delete end path 1");
				path1->PopBack(path1_last + 1 - path1_first);
			} else if (!path2->hasOverlapedEnd()){
				DEBUG("delete end path 2");
				path2->PopBack(path2_last + 1 - path2_first);
			}
		} else if (path2_first == 0 && delete_all && !path2->hasOverlapedBegin()) {
			DEBUG("delete path 2 begin");
			path2->getConjPath()->PopBack(path2_last + 1);
		} else if (path2_last == size2 - 1 && delete_all && !path2->hasOverlapedEnd()) {
			DEBUG("delete path 2 end");
			path2->PopBack(path1_last + 1 - path1_first);
		} else if (path1_first == 0 && delete_all && !path1->hasOverlapedBegin()) {
			DEBUG("delete path1 begin");
			path1->getConjPath()->PopBack(path1_last + 1);
		} else if (path1_last == size1 - 1 && delete_all && !path1->hasOverlapedEnd()) {
			path1->PopBack(path1_last + 1 - path1_first);
			DEBUG("delete path1 end")
		} else {
			DEBUG("nothing delete");
			return false;
		}
		return true;
	}

	class PathIdComparator {
		public:

		    bool operator() (const BidirectionalPath* p1, const BidirectionalPath* p2) const {
		        if (p1->GetId() == p2->GetId()){
		        	DEBUG("WRONG ID !!!!!!!!!!!!!!!!!");
		        	return p1->Length() < p2->Length();
		        }
		    	return p1->GetId() < p2->GetId();
		    }
	};
	class EdgeIdComparator {
		Graph& g_;
	public:
		EdgeIdComparator(Graph& g): g_(g){

		}

		bool operator()(const EdgeId& e1, const EdgeId& e2) const {
			if (g_.length(e1) < g_.length(e2)) {
				return true;
			}
			if (g_.length(e2) < g_.length(e1)) {
				return false;
			}
			return e1.int_id() < e2.int_id();
		}
	};


	std::vector<EdgeId> getSortedEdges() {
		std::set<EdgeId> edges_set;
		for (auto iter = g_.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
			edges_set.insert(*iter);
			edges_set.insert(g_.conjugate(*iter));
		}
		std::vector<EdgeId> edges(edges_set.begin(), edges_set.end());
		std::sort(edges.begin(), edges.end(), EdgeIdComparator(g_));
		return edges;
	}

public:
	SimpleOverlapRemover(Graph& g, GraphCoverageMap& cm) :
			g_(g), coverageMap(cm) {
	}

	void removeSimilarPaths(PathContainer& paths, size_t maxOverlapLength, bool remove_only_equal, bool delete_subpaths, bool delete_begins, bool delete_all) {
		std::vector<EdgeId> edges = getSortedEdges();
		for (size_t edgeId = 0; edgeId < edges.size(); ++edgeId) {
			EdgeId edge = edges.at(edgeId);
			std::set<BidirectionalPath *> covPaths = coverageMap.GetCoveringPaths(edge);
			std::vector<BidirectionalPath*> covVector(covPaths.begin(), covPaths.end());
			std::sort(covVector.begin(), covVector.end(), PathIdComparator());
			for (size_t vect_i = 0; vect_i < covVector.size(); ++vect_i) {
				BidirectionalPath* path1 = covVector.at(vect_i);
				if (covPaths.find(path1) == covPaths.end() ) {
					continue;
				}
				for (size_t vect_i1 = vect_i + 1; vect_i1 < covVector.size(); ++vect_i1) {
					BidirectionalPath* path2 = covVector.at(vect_i1);
					if (covPaths.find(path2) == covPaths.end()) {
						continue;
					}
					if ((*path1) == (*path2)) {
						if (path2->isOverlap()) {
							path1->setOverlap(true);
						}
						path2->Clear();
						covPaths = coverageMap.GetCoveringPaths(edge);
						DEBUG("delete path2 it is equal");
						continue;
					}
					if (g_.length(edge) <= maxOverlapLength || path1->isOverlap() || path2->isOverlap() || remove_only_equal) {
						DEBUG("small edge");
						continue;
					}
					vector<size_t> poses1 = path1->FindAll(edge);
					for (size_t index_in_posis = 0; index_in_posis < poses1.size(); ++index_in_posis) {
						vector<size_t> poses2 = path2->FindAll(edge);
						for (size_t index_in_posis2 = 0; index_in_posis2 < poses2.size(); ++index_in_posis2) {
							int path2_last = poses2[index_in_posis2];
							int path1_last = poses1[index_in_posis];
							if (path1_last >= (int)path1->Size() || path2_last >= (int)path2->Size()){
								continue;
							}
							vector<int> other_path_end;
							pair<int, int > posRes = ComparePaths(path1_last, path2_last, *path1, *path2, maxOverlapLength);
							path1_last = posRes.first;
							path2_last = posRes.second;
							if (path1_last == -1 || path2_last == -1){
								continue;
							}
							BidirectionalPath* conj_current = path1->getConjPath();
							BidirectionalPath* conj_second = path2->getConjPath();
							int path1_first = conj_current->Size() - poses1[index_in_posis] - 1;
							int path2_first = conj_second->Size() - poses2[index_in_posis2] - 1;
							posRes = ComparePaths(path1_first, path2_first, *conj_current, *conj_second, maxOverlapLength);
							path1_first = posRes.first;
							path2_first = posRes.second;
							if (path1_first == -1 || path2_first == -1) {
								continue;
							}
							DEBUG("pos " << path1_last << " pos " << path2_last );
							DEBUG("try to delete smth ");
							path1->Print();
							DEBUG("second path");
							path2->Print();
							path2_first = conj_second->Size() - path2_first - 1;
							path1_first = conj_current->Size() - path1_first - 1;
							DEBUG("path1 begin " << path1_first << " path1 end "
									<< path1_last << " path2_begin "
									<< path2_first << " path2_end "
									<< path2_last);
							cutOverlaps(path1, path1_first, path1_last,
									path1->Size(), path2, path2_first,
									path2_last, path2->Size(), delete_subpaths, delete_begins, delete_all);
							covPaths = coverageMap.GetCoveringPaths(edge);
						}
					}
				}
			}

		}
		DEBUG("END ALL CUT")
	}


	void removeOverlaps(PathContainer& paths, size_t maxOverlapedSize) {
		for (size_t i = 0; i < paths.size(); i++) {
			removePathOverlap(paths, paths.Get(i),maxOverlapedSize);
			removePathOverlap(paths, paths.GetConjugate(i), maxOverlapedSize);
		}
	}

	bool hasAlreadyOverlapedEnd(BidirectionalPath * path){
		return !path->isOverlap() and path->getOverlapedEnd().size() > 0;
	}

	bool hasAlreadyOverlapedBegin(BidirectionalPath * path){
		return !path->isOverlap() and path->getOverlapedBegin().size() > 0;
	}

	bool isSamePath(BidirectionalPath * path1, BidirectionalPath * path2){
		return *path2 == *path1 or *path2 == *path1->getConjPath();
	}


	void removePathOverlap(PathContainer& pathsContainer, BidirectionalPath* currentPath, size_t maxOverlapedSize) {
        int last_index = currentPath->Size() - 1;
		if (last_index <= 0
				or coverageMap.GetCoverage(currentPath->At(last_index)) <= 1
				or hasAlreadyOverlapedEnd(currentPath)){
			return;
		}

		std::set<BidirectionalPath *> paths = coverageMap.GetCoveringPaths(currentPath->At(last_index));

		BidirectionalPath* overlapedPath = NULL;

		size_t overlapedSize = 0;

		for (auto path_iter = paths.begin(); path_iter != paths.end(); ++path_iter) {
			if (isSamePath(*path_iter, currentPath)
					|| hasAlreadyOverlapedBegin(*path_iter)) {
				continue;
			}
			size_t over_size = currentPath->OverlapEndSize(*path_iter);
			if (over_size > overlapedSize) {
				overlapedSize = over_size;
				overlapedPath = *path_iter;
			} else if (over_size == overlapedSize
					&& (overlapedPath == NULL
							|| (*path_iter)->GetId() < overlapedPath->GetId())) {
				overlapedPath = *path_iter;
			}
		}
        if (overlapedPath == NULL){
            return;
        }

		BidirectionalPath* conj_path2 = overlapedPath->getConjPath();
		if (overlapedSize > 0) {
			DEBUG("remove overlaps, change paths " << overlapedSize);
			currentPath->Print();
			DEBUG("next");
			overlapedPath->Print();
			if (currentPath->isOverlap() && overlapedSize == currentPath->Size()) {
				conj_path2->PopBack(overlapedSize);
				DEBUG("change second path");
				overlapedPath->changeOverlapedBeginTo(currentPath);
			}else if (overlapedPath->isOverlap() && overlapedPath->Size() == overlapedSize) {
				currentPath->PopBack(overlapedSize);
				DEBUG("change first path");
				currentPath->changeOverlapedEndTo(overlapedPath);
			}else if (overlapedSize < overlapedPath->Size() && overlapedSize < currentPath->Size()) {
				BidirectionalPath* overlap = new BidirectionalPath(g_, currentPath->Head());
				BidirectionalPath* conj_overlap = new BidirectionalPath(g_, g_.conjugate(currentPath->Head()));
				pathsContainer.AddPair(overlap, conj_overlap);
				currentPath->PopBack();
				conj_path2->PopBack();
				for (size_t i = 1; i < overlapedSize; ++i) {
					conj_overlap->Push(g_.conjugate(currentPath->Head()));
					currentPath->PopBack();
					conj_path2->PopBack();
				}
				coverageMap.Subscribe(overlap);
				overlap->setOverlap(true);
				coverageMap.Subscribe(conj_overlap);
				currentPath->changeOverlapedEndTo(overlap);
				overlapedPath->changeOverlapedBeginTo(overlap);
				DEBUG("add new overlap");
				overlap->Print();

			}
		}
	}

};


class PathExtendResolver {

protected:
    Graph& g_;
    size_t k_;

public:
    PathExtendResolver(Graph& g): g_(g), k_(g.k()) {
    }

    bool InCycle(EdgeId e)
    {
    	auto edges = g_.OutgoingEdges(g_.EdgeEnd(e));
    	if (edges.size() >= 1) {
			for (auto it = edges.begin(); it != edges.end();  ++ it) {
				if (g_.EdgeStart(e) == g_.EdgeEnd(*it)) {
				   return true;
				}
			}
    	}
    	return false;
    }

    bool InBuble(EdgeId e){
    	auto edges = g_.OutgoingEdges(g_.EdgeStart(e));
    	auto endVertex = g_.EdgeEnd(e);
    	for (auto it = edges.begin(); it != edges.end(); ++it){
    		if ((g_.EdgeEnd(*it) == endVertex) and (*it != e)){
    			return true;
    		}
    	}
    	return false;
    }


    PathContainer makeSimpleSeeds() {
		std::set<EdgeId> included;
		PathContainer edges;
		for (auto iter = g_.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
			if (g_.int_id(*iter) <= 0 or InCycle(*iter)) {
				continue;
			}
			if (included.count(*iter) == 0) {
				edges.AddPair(new BidirectionalPath(g_, *iter),
						new BidirectionalPath(g_, g_.conjugate(*iter)));
				included.insert(*iter);
				included.insert(g_.conjugate(*iter));
			}
		}
		return edges;
	}

    PathContainer extendSeeds(PathContainer& seeds, PathExtender& pathExtender) {
        PathContainer paths;
        pathExtender.GrowAll(seeds, &paths);
        return paths;
    }

    void removeOverlaps(PathContainer& paths, GraphCoverageMap& coverageMap, size_t maxOverlapedSize, ContigWriter& writer, string output_dir) {
        SimpleOverlapRemover overlapRemover(g_, coverageMap);
        DEBUG("Removing overlaps");
        //writer.writePaths(paths, output_dir + "/before.fasta");
        overlapRemover.removeSimilarPaths(paths, maxOverlapedSize, false, true, true, false);
        //writer.writePaths(paths, output_dir + "/remove_similar.fasta");
        overlapRemover.removeOverlaps(paths, maxOverlapedSize);
        //writer.writePaths(paths, output_dir + "/remove_overlaps.fasta");
        overlapRemover.removeSimilarPaths(paths, maxOverlapedSize, true, false, false, false);
        //writer.writePaths(paths, output_dir + "/remove_equals.fasta");
        DEBUG("remove equals paths");
		overlapRemover.removeSimilarPaths(paths, 0, false, true, true, false);
		//writer.writePaths(paths, output_dir + "/remove_begins.fasta");
		overlapRemover.removeSimilarPaths(paths, maxOverlapedSize, false, true, true, true);
		//writer.writePaths(paths, output_dir + "/remove_all.fasta");
		DEBUG("end removing");
    }

    void addUncoveredEdges(PathContainer& paths, GraphCoverageMap& coverageMap) {
        std::set<EdgeId> included;
        for (auto iter = g_.SmartEdgeBegin(); !iter.IsEnd(); ++iter) {
            if (included.count(*iter) == 0 && !coverageMap.IsCovered(*iter)) {
                paths.AddPair(new BidirectionalPath(g_, *iter), new BidirectionalPath(g_, g_.conjugate(*iter)));
                included.insert(*iter);
                included.insert(g_.conjugate(*iter));
            }
        }
    }

};

} /* PE_RESOLVER_HPP_ */

#endif
