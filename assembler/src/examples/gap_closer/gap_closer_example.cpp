//***************************************************************************
//* Copyright (c) 2022 Saint Petersburg State University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#include "examples/examples_utils/edge_comparation.hpp"
#include "examples/graph_io/gfa_io.hpp"

#include "common/assembly_graph/core/graph.hpp"
#include "common/pipeline/graph_pack.hpp"
#include "common/utils/logger/log_writers.hpp"
#include "projects/spades/gap_closer.hpp"

#include <clipp/clipp.h>

void GraphPackBasedExample(const std::filesystem::path& saves, const std::filesystem::path& work_dir,
                              const std::filesystem::path& output_dir) {
    /*
     * GraphPack is a data structure, which stores information about assembly de Bruijn graph. It is declared
     * in common/pipeline/graph_pack.hpp file.
     *
     * Data storage is organized in a such way, that only one instance of GraphPack may exist during the runtime.
     *
     * Access to information about the graph is carried out via get<T>() and get_mutable<T>() functions,
     * where T is a name of data structure in which this information is stored.
     *
     * GraphPack has 3 obligatory arguments: size_t k - the length of reads, std::filesystem::path work_dir -
     * some directory, which GraphPack uses during the runtime, and size_t lib_count.
     *
     * TODO: it would be useful to write about option arguments of graph pack constructor
     */
    create_directories(work_dir);
    graph_pack::GraphPack gp(55, work_dir, 1); //std::vector<std::string>(0), 55, 0, 0, true);
    INFO("Graph pack instance is created");

    debruijn_graph::GapClosing gapClosingStage("gap_closer_example");
    INFO("Gap closing stage instance is created");

   /*
    * It is necessary to create <saves>/.bin_reads directory in which converted binary reads will be written during
    * the loading of the assembly graph. Without this directory binary reads would be converted, but would not be
    * saved, because of what stage running would fall.
    */
    std::filesystem::path binary_reads_dir = saves / ".bin_reads";
    create_directory(binary_reads_dir);

    std::filesystem::path load_from = saves / "K55/saves";
    gapClosingStage.load(gp, load_from, "ec_threshold_finder");
    INFO("Graph pack is loaded from saves");
    //TODO: print some information about the graph (size, are indexes are attached, are indexes are empty)

    gapClosingStage.run(gp, "");
    INFO("Gap closing stage is finished");

    remove_all(output_dir);
    create_directories(output_dir);

    gapClosingStage.save(gp, output_dir, "gap_closer");
    INFO("Graph pack is saved to " << output_dir / "gap_closer");

    const auto& g1 = gp.get<debruijn_graph::Graph>();
    spades_example::SaveToGFA(g1, "temp.gfa");
    debruijn_graph::Graph g(55);
    spades_example::ReadFromGFA(g, "temp.gfa");
    remove("temp.gfa");
    /*
     * This action with saving and loading the graph caused by a necessity to compare 2 graphs g1 and g2.
     * Both of them stored in a GraphPack instance, and after loading the reference graph both g1 and g2
     * refer to the same Graph instance. Due to the fact that debruijn_graph::Graph is an uncopyable class,
     * the only way to compare graphs is to save g1 and then load it in a new instance.
     */

    gapClosingStage.load(gp, load_from, "early_gapcloser");
    const auto& g2 = gp.get<debruijn_graph::Graph>();
    INFO("The reference graph is loaded");

    spades_example::CompareEdges(g, g2);

    remove_all(work_dir);
}

// TODO: GraphBasedExample - load and save the graph itself (from .grseq file)

void CreateConsoleLogger(const std::filesystem::path& log_fn="") {
   /*
    * To track the process of program execution it is useful to create a console logger.
    * In SPAdes logger is declared in common/utils/logger/logger.hpp file and logger writer is declared
    * in common/utils/logger/log_writters.hpp. To use logger in the project common/utils/logger/log_writters.hpp
    * should be included (in this case common/utils/logger/logger.hpp is including transitive) and link
    * utils library in CMake file.
    *
    * After logger creation it is convenient to use TRACE, DEBUG, INFO, WARN and ERROR macro.
    *
    * TODO: add logger file
    */
    using namespace logging;
    logger *lg = create_logger(exists(log_fn) ? log_fn : "");
    lg->add_writer(std::make_shared<console_writer>());
    attach_logger(lg);
}

void ParseCommandLine(int argc, char *argv[], std::filesystem::path& saves, std::filesystem::path& tmp_dir,
                        std::filesystem::path& out_dir) {
    using namespace clipp;
    std::string saves_, tmp_dir_, out_dir_;
    auto cli = (
        option("-s", "--saves") & value("dir", saves_),
        option("--tmp_dir") & opt_value("dir", tmp_dir_),
        option("-o", "--out_dir") & opt_value("dir", out_dir_)
    );
    parse(argc, argv, cli);

    saves = saves_;
    tmp_dir = tmp_dir_.empty() ? "tmp_dir" : tmp_dir_;
    out_dir = out_dir_.empty() ? "tmp" : out_dir_;
}

int main(int argc, char *argv[]) {
   /*
    * This example shows how to use one of the SPAdes stages (in this case it is Gap Closer stage)
    * separately from the SPAdes pipeline.
    *
    * Before starting of this example user should run SPAdes to have saved versions of assembly graph after
    * every stage of assembling. For example, "./spades.py --tmp-dir <some temporary directory>  --sc
    * --checkpoints all --only-assembler -m 20 -k 55 -1 <file with forward paired-end reads> -2
    * <file with reverse paired-end reads> -s <file with unpaired reads> -o <output directory>".
    *
    * The "--checkpoints all" option guarantees that a state of the assembly graph after each of the SPAdes stages
    * will be saved in the <output directory>/K55/saves directory.
    *
    * Usage of this example: gap_closer_example -s <output directory of the preliminary command> [--tmp_dir
    * <temporary dir for graph pack> -o <output dir>].
    */

    std::filesystem::path saves, tmp_dir, out_dir;
    ParseCommandLine(argc, argv, saves, tmp_dir, out_dir);

    CreateConsoleLogger();
    INFO("Start of the example");

    /*
     * Config are obligatory to make an example work correctly. Config files, necessary for the SPAdes stage,
     * are generated during the preliminary command run and written to
     * <output directory of the preliminary command>/saves/K55/configs. Before the start of the example user should
     * create an instance of the config based on these config files.
     */
    std::vector<std::filesystem::path> cfg_fns = {
            saves / "K55/configs/config.info", saves / "K55/configs/mda_mode.info"
    };
    cfg::create_instance(cfg_fns);

    GraphPackBasedExample(saves, tmp_dir, out_dir);
    return 0;
}
