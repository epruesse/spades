/*
 * pac_index.hpp
 *
 *  Created on: Jan 21, 2013
 *      Author: lab42
 */
#pragma once

#include "debruijn_kmer_index.hpp"
#include "graph_pack.hpp"
#include <algorithm>

//
//template<class Graph>
//struct position{
//	typename Graph::EdgeId edgeId_;
//	int offset_;
//	position(typename Graph::EdgeId edgeId, int offset) :
//	      edgeId_(edgeId), offset_(offset) { }
//};

//TODO: dangerousCode
template<class T>
struct pair_iterator_less {
    bool operator ()(pair<size_t, T>const& a, pair<size_t, T> const& b) const {
    	if (a.first < b.first)
    		return true;
    	else
    		return false;
    }
};


struct MappingInstance {
	int edge_position;
	int read_position;
	//Now - multiplicity, so best quality is 1,
	int quality;
	MappingInstance(int edge_position, int read_position, int quality): edge_position(edge_position), read_position(read_position), quality(quality) {}
	inline bool IsUnique() const{
		return (quality == 1);
	}
	string str(){
		stringstream s;
		s << "E: "<< edge_position << " R: " << read_position << " Q: " << quality;
		return s.str();
	}
	bool operator < ( MappingInstance const& b) const {
		if (edge_position< b.edge_position  || (edge_position == b.edge_position && read_position< b.read_position))
			return true;
		else
			return false;
	}
};

struct ReadPositionComparator{
    bool operator ()(MappingInstance const& a, MappingInstance const& b) const {
    	if (a.read_position< b.read_position  || (a.read_position == b.read_position && a.edge_position< b.edge_position))
    		return true;
    	else
    		return false;
    }
};

template<class Graph>
struct MappingDescription {

};
template<class Graph>
class PacBioMappingIndex{
public:

	//First - in edge, second - in Sequence(pacbio read)
	typedef map<typename Graph::EdgeId, vector<MappingInstance > > MappingDescription;
	typedef map<typename Graph::EdgeId, set<vector<MappingInstance > > > MappingClustersDescription;
	typedef pair<typename Graph::EdgeId, vector<MappingInstance > >   ClusterDescription;
	typedef set<ClusterDescription> ClustersSet;

	typedef typename Graph::VertexId VertexId;
protected :
	Graph &g_;
	size_t K_;
	const static int short_edge_cutoff = 0;
	const static size_t min_cluster_size = 8;
	double compression_cutoff;
	double domination_cutoff;
	set<Sequence> banned_kmers;
	DeBruijnEdgeMultiIndex<typename Graph::EdgeId> tmp_index;
	map<pair<VertexId, VertexId>, vector<size_t> > distance_cashed;
//	MappingDescription BanUselessMapping(MappingDescription res){
//
//	}
private:
	DECL_LOGGER("PacIndex")

public :
	typedef typename Graph::EdgeId EdgeId;

	PacBioMappingIndex(Graph &g, size_t k):g_(g), K_(k), tmp_index(K_, "tmp") {
		DeBruijnEdgeMultiIndexBuilder<runtime_k::RtSeq> builder;
		builder.BuildIndexFromGraph<typename Graph::EdgeId, Graph>(tmp_index, g_);
		fill_banned_kmers();
		compression_cutoff = 0.6;
		domination_cutoff = 1.5;

	}
	void fill_banned_kmers(){

		for (int i = 0; i < 4; i++){
			auto base = nucl(i);
			for (int j = 0; j < 4 ; j ++) {
				auto other = nucl(j);
				for (size_t other_pos = 0; other_pos < K_; other_pos++){
					string s = "";
					for (size_t k = 0; k < K_; k ++){
						if (k!= other_pos)
							s += base;
						else
							s += other;
					}
//					banned_kmers.insert(runtime_k::RtSeq(Sequence(s).start<runtime_k::RtSeq>(K_)));
					banned_kmers.insert(Sequence(s));
				}
			}
		}

	}
	MappingDescription Locate(Sequence &s);

//for pairs of indexes on  edge and in Sequence.
	inline bool similar(const MappingInstance &a, const MappingInstance &b, int shift = 0) const{
		if (b.read_position + shift < a.read_position) {
			return similar(b, a, -shift);
		} else if (b.read_position == a.read_position) {
			return (abs(int(b.edge_position) + shift - int(a.edge_position)) < 2);
		} else {
			return ((b.edge_position + shift - a.edge_position >= (b.read_position - a.read_position) * compression_cutoff) &&(b.edge_position + shift - a.edge_position <= (b.read_position - a.read_position) / compression_cutoff));
		}
	}

	void dfs_cluster(vector<int> &used, vector<MappingInstance > &to_add, const int cur_ind, const typename MappingDescription::iterator iter)const {
		size_t len = iter->second.size();
		for(size_t k = 0; k < len; k++) {
			if (!used[k] && similar(iter->second[cur_ind], iter->second[k])) {
				to_add.push_back(iter->second[k]);
				used[k] = 1;
				dfs_cluster(used, to_add, k, iter);
			}
		}
	}

	ClustersSet GetClusters(Sequence &s){
		MappingDescription descr = Locate(s);
		ClustersSet res;
		for(auto iter = descr.begin(); iter!= descr.end(); ++iter){
			set<vector<MappingInstance > > edge_cluster_set;
			size_t len =  iter->second.size();
			vector<int> used(len);
			//TODO: recursive call
			for (size_t i = 0; i < len; i++){
				if (!used[i]) {
					used[i] = 1;
					vector<MappingInstance > to_add;
					to_add.push_back(iter->second[i]);
					dfs_cluster(used, to_add, i, iter);
//					for (size_t  j = i+ 1; j < len; j++) {
//						if (!used[j] && similar(iter->second[i], iter->second[j]) ) {
//							to_add.push_back(iter->second[j]);
//							used[j] = 1;
//							for(size_t k = 0; k < len; k++) {
//								if (!used[k] && similar(iter->second[j], iter->second[k])) {
//									to_add.push_back(iter->second[k]);
//									used[k] = 1;
//								}
//							}
//						}
//					}
					sort(to_add.begin(), to_add.end(), ReadPositionComparator());
					size_t count = 1;
					size_t longest_len = 0;
					auto cur_start = to_add.begin();
					auto best_start = to_add.begin();
					size_t j = 0;
					for (auto j_iter = to_add.begin(); j_iter < to_add.end()- 1; j_iter ++, j++ ) {
//Do not spilt clusters in the middle, only beginning is interesting.
						if ((j* 5< to_add.size() || (j + 1 )*5 > to_add.size() * 4 ) && !similar(*j_iter, *(j_iter + 1))){
							if (longest_len < count) {
								longest_len = count;
								best_start = cur_start;
							}
							count = 1;
							cur_start = j_iter + 1;
						} else {
							count ++;
						}
					}
					if (longest_len < count) {
						longest_len = count;
						best_start = cur_start;
					}

					vector<MappingInstance > filtered(best_start, best_start + longest_len);
					if (count < to_add.size() && to_add.size() > min_cluster_size) {
						DEBUG("in cluster size " << to_add.size() << ", " << to_add.size() - longest_len<<"were removed as trash")
					}
					res.insert(make_pair(iter->first, filtered));
				}
			}
		}
		FilterClusters(res);
		return res;
	}

	//filter clusters that are too small or fully located on a vertex or dominated by some other cluster.
	void FilterClusters(ClustersSet &clusters){
		for(auto i_iter = clusters.begin(); i_iter!= clusters.end(); ){
			int  len = g_.length(i_iter->first);
			auto sorted_by_edge = i_iter->second;
			sort(sorted_by_edge.begin(), sorted_by_edge.end());
			size_t good = 0;
			for (auto iter = sorted_by_edge.begin(); iter < sorted_by_edge.end(); iter++) {
				if (iter->IsUnique())
					good ++;
			}
			if (good < min_cluster_size){
				auto tmp_iter = i_iter;
				tmp_iter ++;
				clusters.erase(i_iter);
                                i_iter = tmp_iter;
			} else if (len < short_edge_cutoff){
				DEBUG("Life is too long, and edge is too short!");
			} else {

				if (sorted_by_edge[0].edge_position >= len || sorted_by_edge[i_iter->second.size() -1 ].edge_position <= int(cfg::get().K) - int(K_)) {
 					DEBUG ("All anchors in vertex");

					auto tmp_iter = i_iter;
					tmp_iter ++;
					clusters.erase(i_iter);
					i_iter = tmp_iter;
				} else {
					i_iter ++;
				}
			}
		}
		for(auto i_iter = clusters.begin(); i_iter!= clusters.end(); ){
			for(auto j_iter = clusters.begin(); j_iter !=clusters.end();) {
				if (i_iter != j_iter) {
					if (dominates(*i_iter, *j_iter)) {
						DEBUG("cluster is dominated");
						auto tmp_iter = j_iter;
						tmp_iter ++;
						clusters.erase(j_iter);
						j_iter = tmp_iter;
					} else {
						j_iter++;
					}
				} else {
					j_iter++;
				}
			}
			i_iter++;
		}
	}

	//TODO:: non strictly dominates?
	inline bool dominates (const ClusterDescription &a, const ClusterDescription &b) const{
		size_t a_size = a.second.size();
		size_t b_size = b.second.size();
		if (a_size < b_size * domination_cutoff || a.second[0].read_position > b.second[0].read_position || a.second[a_size - 1].read_position < b.second[b_size -1].read_position) {
			return false;
		} else {
			return true;
		}
	}

//TODO:: NOT SAFE FOR WORK
	vector<EdgeId> FillGapsInCluster(vector <pair<size_t,  typename ClustersSet::iterator> > &cur_cluster, Sequence &s) {

		//debug stuff

		set<int> interesting_starts({7969653, 7968954, 7968260, 7965581, 7966399});
		set<int> interesting_ends({7973410, 7972240, 7971473, 7970031, 7969246});

		vector<EdgeId> cur_sorted;
		EdgeId prev_edge = EdgeId(0);
		if (cur_cluster.size() > 1) {
			for(auto iter = cur_cluster.begin(); iter != cur_cluster.end(); ++iter){
				EdgeId cur_edge = iter->second->first;
				INFO(g_.int_id(cur_edge));
			}
		}
		for(auto iter = cur_cluster.begin(); iter != cur_cluster.end(); ++iter){
			EdgeId cur_edge = iter->second->first;
			if (prev_edge != EdgeId(0)) {
//Need to find sequence of edges between clusters
				VertexId start_v = g_.EdgeEnd(prev_edge);
				VertexId end_v = g_.EdgeStart(cur_edge);

				//TODO: || (start_v == end_v && some condition on indexes)

				auto prev_iter = iter - 1;

				bool debug_info = false;
				if (interesting_starts.find(g_.int_id(prev_edge)) != interesting_starts.end() && interesting_ends.find(g_.int_id(cur_edge)) != interesting_ends.end())
					debug_info = true;
//ignore non-unique kmers for distance determination
				auto first_unique_iter = (iter->second->second.begin());
				while (first_unique_iter != (iter->second->second.end() - 1)  && !first_unique_iter->IsUnique()) {
					first_unique_iter += 1;
				}

				auto last_unique_iter = (prev_iter->second->second.end() - 1);
				while (last_unique_iter != (iter->second->second.begin())  && !last_unique_iter->IsUnique()) {
					last_unique_iter -= 1;
				}
				MappingInstance cur_first_index = *first_unique_iter;
				MappingInstance prev_last_index = *(last_unique_iter);
//TODO: reasonable constant?
				if (start_v != end_v || (start_v == end_v && (cur_first_index.read_position - prev_last_index.read_position) > (cur_first_index.edge_position + g_.length(prev_edge) - prev_last_index.edge_position) * 1.3)) {

					INFO("closing gap between "<< g_.int_id(prev_edge)<< " " << g_.int_id(cur_edge));
					//MappingInstance cur_first_index = *(iter->second->second.begin());

					INFO (" first pair" << cur_first_index.str() << " edge_len" << g_.length(cur_edge));


					//MappingInstance prev_last_index = *(prev_iter->second->second.end()-1);
					INFO (" last pair" << prev_last_index.str() << " edge_len" << g_.length(prev_edge));
//					int seq_end = cur_first_index.second - cur_first_index.edge_position  + cfg::get().K;
//					int seq_start = prev_last_index.second - prev_last_index.edge_position + g_.length(prev_edge) ;
					string s_add = "";
					string e_add = "";
					int seq_end = cur_first_index.read_position;
					int seq_start = prev_last_index.read_position ;
					string tmp = g_.EdgeNucls(prev_edge).str();
					s_add =  tmp.substr(prev_last_index.edge_position, g_.length(prev_edge) - prev_last_index.edge_position);
					tmp = g_.EdgeNucls(cur_edge).str();
					e_add = tmp.substr(0, cur_first_index.edge_position);
//					e_add = tmp.substr(cfg::get().K - 1, max(0, int(cur_first_index.edge_position) - int(cfg::get().K - 1)));
					vector<EdgeId> intermediate_path = BestScoredPath(s, start_v, end_v, seq_start, seq_end, s_add, e_add, debug_info);
					if (intermediate_path.size() == 0) {
						WARN("Gap between edgees "<< g_.int_id(prev_edge) << " " << g_.int_id(cur_edge) << " is not closed, additions from edges: "<< int(g_.length(prev_edge)) - int(prev_last_index.edge_position) <<" " << int(cur_first_index.edge_position) - int(cfg::get().K - K_ ) << " and seq "<< - seq_start + seq_end );
						vector<EdgeId> intermediate_path = BestScoredPath(s, start_v, end_v, seq_start, seq_end, s_add, e_add);
						return intermediate_path;
					}
					for(auto j_iter = intermediate_path.begin(); j_iter != intermediate_path.end(); j_iter++) {
						cur_sorted.push_back(*j_iter);
					}
				}
			}
			cur_sorted.push_back(cur_edge);
			prev_edge = cur_edge;
		}
		return cur_sorted;
	}

	vector<vector<EdgeId> > GetReadAlignment(Sequence &s){
		ClustersSet mapping_descr = GetClusters(s);
		int len = mapping_descr.size();
		vector<vector<int> > table(len);
		for(int i= 0; i < len; i ++) {
			table[i].resize(len);
			table[i][i] = 1;
		}
		int i = 0;
		vector<int> colors(len);
		vector<int> cluster_size(len);
		vector<int> used(len);
		for(int i = 0; i  < len; i++) {
			colors[i] = i;
		}
		for (auto i_iter = mapping_descr.begin(); i_iter != mapping_descr.end(); ++i_iter, ++i){
			cluster_size[i] = i_iter->second.size();
		}
		i = 0;
		if (len > 1) {
			TRACE(len << "clusters");
		}
		for (auto i_iter = mapping_descr.begin(); i_iter != mapping_descr.end(); ++i_iter, ++i){
			int j = 0;
			for (auto j_iter = mapping_descr.begin(); j_iter != mapping_descr.end(); ++j_iter, ++j){
				if (i_iter == j_iter)
					continue;
				table[i][j] = IsConsistent(s, *i_iter, *j_iter);
				if (table[i][j]){
					int j_color = colors[j];
					for (int k = 0; k < int(len); k++) {
						if (colors[k] == j_color){
							colors[k] = colors[i];
							TRACE("recoloring");
							cluster_size[i] += cluster_size[k];
							cluster_size[k] = 0;
						}
					}
				}
			}
		}
		vector<vector<EdgeId> >sortedEdges;
		for (i = 0; i < len; i ++)
			used[i] = 0;
		for (i = 0; i < len; i ++) {
			if (! used[i]) {
				vector <pair<size_t,  typename ClustersSet::iterator> > cur_cluster;
				used[i] = 1;
				int j = 0;
				int cur_color = colors[i];
				for (auto i_iter = mapping_descr.begin(); i_iter != mapping_descr.end(); ++i_iter, ++j){
					if (colors[j] == cur_color) {
						cur_cluster.push_back(make_pair(i_iter->second.begin()->read_position, i_iter));
						used[j] = 1;
					}
				}
//TODO: check than consequent clusters are consistent!
 				sort(cur_cluster.begin(), cur_cluster.end(), pair_iterator_less<typename ClustersSet::iterator>());
 				VERIFY(cur_cluster.size() > 0);
 				auto cur_cluster_start = cur_cluster.begin();
 				for(auto iter = cur_cluster.begin() ; iter != cur_cluster.end(); ++iter){
					auto next_iter = iter + 1;
 					if (next_iter == cur_cluster.end() || !IsConsistent(s, *(iter->second), *(next_iter->second))){
 						if (next_iter != cur_cluster.end()) {
 							INFO("splitting cluster sequences...")

 						}
 						vector <pair<size_t,  typename ClustersSet::iterator> > splitted_cluster(cur_cluster_start, next_iter);
 						vector<EdgeId> cur_sorted = FillGapsInCluster(splitted_cluster, s);
 						if (cur_sorted.size() > 0)
 							sortedEdges.push_back(cur_sorted);
 						cur_cluster_start = next_iter;
 					}
 				}
			}
		}
		return sortedEdges;
	}


//0 - No, 1 - Yes
	int IsConsistent(Sequence &s, const ClusterDescription &a, const ClusterDescription &b) {
		EdgeId a_edge = a.first;
		EdgeId b_edge = b.first;
//		if (a_edge == b_edge){
//		//	return 0;
//			for (auto a_iter = a.second.begin(); a_iter != a.second.end(); ++a_iter)
//				for (auto b_iter = b.second.begin(); b_iter != b.second.end(); ++b_iter)
//					if (similar(*a_iter, *b_iter) ){
//						INFO("WTF?! Two clusters on edge " << g_.int_id(a_edge) << " have similar elements !!" );
//						return 1;
//					}
//			return 0;
//		}

		VertexId start_v = g_.EdgeEnd(a_edge);
		size_t addition = g_.length(a_edge);
		VertexId end_v = g_.EdgeStart(b_edge);
		pair<VertexId, VertexId> vertex_pair = make_pair(start_v, end_v);
		vector<size_t> result;
		if (distance_cashed.find(vertex_pair) == distance_cashed.end()){
			DistancesLengthsCallback<Graph> callback(g_);
			PathProcessor<Graph> path_processor(g_, 0, s.size()/3, start_v, end_v, callback);
			path_processor.Process();
			result = callback.distances();
			distance_cashed[vertex_pair] = result;
		}
		result = distance_cashed[vertex_pair];
		//TODO: Serious optimization possible
		for (size_t i = 0; i < result.size(); i++) {
			for (auto a_iter = a.second.begin(); a_iter != a.second.end(); ++a_iter)
				for (auto b_iter = b.second.begin(); b_iter != b.second.end(); ++b_iter)
					if (similar(*a_iter, *b_iter, result[i] + addition) ){
						return 1;
					}
		}
		return 0;

	}

	string PathToString( const vector<EdgeId>& path) const {
		string res = "";
//		bool first = true;
		for(auto iter = path.begin(); iter != path.end(); iter ++){
			size_t len = g_.length(*iter);
			string tmp = g_.EdgeNucls(*iter).First(len).str();
			res =  res + tmp;
		}
		return res;
	}

	int StringDistance(string &a, string &b){
		int a_len = a.length();
		int b_len = b.length();
		int d = min(a_len/3, b_len/3);
		d = max(d, 10);
		DEBUG(a_len << " " << b_len << " " << d);
		vector<vector<int> > table(a_len);
		//int d =
		for (int i = 0; i < a_len; i++){
			table[i].resize(b_len);
			int low = max(max(0, i - d - 1), i + b_len - a_len - d - 1);
			int high = min(min(b_len, i + d  + 1), i + a_len - b_len + d + 1);
			DEBUG(low << " " <<high);
			for(int j = low; j < high; j++)
				table[i][j] = 1000000000;
		}
		table[a_len - 1][b_len - 1] = 1000000000;
		table[0][0] = 0;
//free deletions on begin
//		for(int j = 0; j < b_len; j++)
//			table[0][j] = 0;

		for (int i = 0; i < a_len; i++) {
			int low = max(max(0, i - d ), i + b_len - a_len - d );
			int high = min(min(b_len, i + d  ), i + a_len - b_len + d );

			DEBUG(low << " " <<high);
			for(int j = low; j < high; j++) {

//			for(int j = max(0, i - d); j < min(b_len, i + d); j++){
			//for(int j = 0; j < b_len; j++) {
				if (i > 0) table[i][j] = min(table[i][j], table[i-1][j] + 1);
				if (j > 0) table[i][j] = min(table[i][j], table[i][j-1] + 1);
				if (i>0 && j>0) {
					int add = 1;
					if (a[i] == b[j]) add = 0;
					table[i][j] = min(table[i][j], table[i-1][j-1] + add);
				}
			}
		}
		//return table[a_len - 1][b_len - 1];
//free deletions on end
		int res = table[a_len - 1][b_len - 1];
		DEBUG(res);
//		for(int j = 0; j < b_len; j++){
//			res = min(table[a_len - 1][j], res);
//		}
		return res;
	}

	vector<EdgeId> BestScoredPath(Sequence &s, VertexId start_v, VertexId end_v, int start_pos, int end_pos, string &s_add, string &e_add, bool debug_info = false){
		INFO("start and end vertices: " << g_.int_id(start_v) <<" " << g_.int_id(end_v));
		int seq_len = -start_pos + end_pos;
		PathStorageCallback<Graph> callback(g_);
//TODO::something more reasonable
		int path_min_len = max(int(floor((seq_len  - int(cfg::get().K)) * 0.7)), 0);
		int path_max_len = (seq_len  + int(cfg::get().K)) * 1.3;
		if (seq_len < 0) {
			WARN("suspicious negative seq_len " << start_pos << " " << end_pos << " " << path_min_len << " " << path_max_len);
			return vector<EdgeId>(0);
		}
		path_min_len = max(path_min_len - int(s_add.length() + e_add.length()), 0);
		path_max_len = max(path_max_len - int(s_add.length() + e_add.length()), 0);
		INFO("path length limits: "<< path_min_len <<"  "<< path_max_len);
		PathProcessor<Graph> path_processor(g_, path_min_len , path_max_len , start_v, end_v, callback);
		path_processor.Process();
		vector<vector<EdgeId> > paths = callback.paths();
		INFO("taking subseq" << start_pos <<" "<< end_pos <<" " << s.size());
		int s_len = int(s.size());
		string seq_string = s.Subseq(start_pos, min(end_pos + 1, s_len)).str();
		size_t best_path_ind = paths.size();
		size_t best_score = 1000000000;
		INFO("need to find best scored path between "<<paths.size()<<" , seq_len " << seq_string.length());
		if (paths.size() == 0)
			return vector<EdgeId> (0);
		for(size_t i = 0; i < paths.size(); i++){

			string cur_string = s_add + PathToString(paths[i]) + e_add;
			if (paths.size() > 1 && paths.size() < 10) {
				INFO ("candidate path number "<< i << " , len " << cur_string.length());
				if (debug_info) {
					INFO("graph candidate: " << cur_string);
					INFO("in pacbio read: " << seq_string);
				}
				for(auto j_iter = paths[i].begin(); j_iter != paths[i].end();++j_iter) {
					INFO (g_.int_id(*j_iter));
				}
			}
			size_t cur_score = StringDistance( cur_string, seq_string);
			if (paths.size() > 1 && paths.size() < 10 ) {
				INFO ("score: "<< cur_score);
			}
			if (cur_score < best_score){
				best_score = cur_score;
				best_path_ind = i;
			}
		}
		if (best_score == 1000000000)
			return vector<EdgeId>(0);
		if (paths.size() > 1 && paths.size() < 10){
			INFO("best score found! Path " <<best_path_ind <<" score "<< best_score);
		}

		return paths[best_path_ind];
	}
};

template<class Graph>
typename PacBioMappingIndex<Graph>::MappingDescription PacBioMappingIndex<Graph>::Locate(Sequence &s){

	//PacBioMappingIndex<Graph>::MappedPositionsVector res;
	MappingDescription res;
	runtime_k::RtSeq kmer = s.start<runtime_k::RtSeq>(K_);
	for (size_t j = K_; j < s.size(); ++j) {
		kmer <<= s[j];
		if (tmp_index.contains(kmer)){
			for (auto iter = tmp_index[kmer].begin(); iter != tmp_index[kmer].end(); ++iter){
				int quality = tmp_index[kmer].size();
//				DEBUG(g_.int_id(iter->edgeId_));
// TODO: operator< for RtSeqs
				if ( banned_kmers.find(Sequence(kmer)) == banned_kmers.end()) {
//Never trust a kmer on vertex
					if (int(iter->offset_) > int(cfg::get().K- this->K_ ) && int(iter->offset_) < int(g_.length(iter->edgeId_))  )
						res[iter->edgeId_].push_back(MappingInstance(iter->offset_, size_t(j - K_+ 1), quality));
				}
			}
		}
	}
	for (auto iter = res.begin(); iter != res.end(); ++ iter){
		sort(iter->second.begin(), iter->second.end());
	}
	return res;
}


