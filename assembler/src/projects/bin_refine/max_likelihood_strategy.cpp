//***************************************************************************
//* Copyright (c) 2021 Saint Petersburg State University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#include "max_likelihood_strategy.hpp"
#include "math/xmath.h"

using namespace debruijn_graph;
using namespace bin_stats;

void MaxLikelihoodBinningAssignmentStrategy::AssignEdgeBins(const SoftBinsAssignment &soft_bins_assignment,
                                                            BinStats &bin_stats) const {
    std::vector<EdgeId> binned;
    for (EdgeId e : bin_stats.unbinned_edges()) {
        std::unordered_set<BinStats::BinId> assignment;

        for (const auto &entry : soft_bins_assignment.at(e).labels_probabilities) {
            if (math::le(entry.value(), thr_))
                continue;

            assignment.insert(entry.index());
        }

        if (assignment.empty())
            continue;

        binned.push_back(e);
        bin_stats.edges_binning()[e] = std::move(assignment);
    }

    for (EdgeId e : binned)
        bin_stats.unbinned_edges().erase(e);
}

blaze::CompressedVector<double>
MaxLikelihoodBinningAssignmentStrategy::AssignScaffoldBins(const std::vector<debruijn_graph::EdgeId>& path,
                                                           const SoftBinsAssignment &soft_bins_assignment,
                                                           const BinStats& bin_stats) const {
    blaze::CompressedVector<double> res(bin_stats.bins().size());

    size_t total_length = 0;
    for (EdgeId edge : path) {
        if (bin_stats.unbinned_edges().count(edge) > 0)
            continue;

        size_t length = bin_stats.graph().length(edge);
        for (auto bin_id : bin_stats.edges_binning().at(edge)) {
            double bin_prob = soft_bins_assignment.at(edge).labels_probabilities[bin_id];
            res[bin_id] += double(length) * bin_prob;
        }
        total_length += length;
    }

    if (0 && total_length) {
        double inv_length = 1.0/double(total_length);
        res *= inv_length;
    }

    return res;
}

std::vector<BinStats::BinId>
MaxLikelihoodBinningAssignmentStrategy::ChooseMajorBins(const blaze::CompressedVector<double> &bins_weights,
                                                        const SoftBinsAssignment&,
                                                        const BinStats&) const {
    double max_logL = -std::numeric_limits<double>::infinity();
    BinStats::BinId major_bin = BinStats::UNBINNED;
    for (const auto &entry : bins_weights) {
        if (math::le(entry.value(), max_logL))
            continue;

        max_logL = entry.value();
        major_bin = entry.index();
    }

    if (major_bin == BinStats::UNBINNED)
        return { };

    if (allow_multiple_) {
        std::vector<BinStats::BinId> res;
        for (const auto &entry : bins_weights) {
            if (!math::eq(entry.value(), max_logL))
                continue;

            res.push_back(entry.index());
        }
        VERIFY(res.size() >= 1);
        return res;
    } else
        return { major_bin };
}
