//***************************************************************************
//* Copyright (c) 2011-2013 Saint-Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************

#include "compare_standard.hpp"
#include "synthetic_tests.hpp"
#include "logger/log_writers.hpp"
#include "graphio.hpp"
#include <boost/test/unit_test.hpp>

#include "comparison_utils.hpp"
#include "diff_masking.hpp"
#include "repeat_masking.hpp"
#include "assembly_compare.hpp"
#include "test_utils.hpp"

::boost::unit_test::test_suite* init_unit_test_suite(int, char*[]) {
    //logging::create_logger("", logging::L_DEBUG);
    //logging::__logger()->add_writer(make_shared<logging::console_writer>());
    logging::logger *log = logging::create_logger("", logging::L_INFO);
    log->add_writer(std::make_shared<logging::console_writer>());
    logging::attach_logger(log);

    using namespace ::boost::unit_test;
    char module_name[] = "cap_test";

    assign_op(framework::master_test_suite().p_name.value,
              basic_cstring<char>(module_name), 0);

    return 0;
}

#include "lseq_test.hpp"

namespace cap {

inline void CheckDiffs(size_t k, const string& actual_prefix, const string& etalon_prefix,
                       bool exact_match = true) {
    if (exact_match) {
        INFO("Checking differences for graphs: exact match");
        vector<string> suffixes = { "grp", "clr", "sqn" };
        FOREACH (auto suff, suffixes) {
            BOOST_CHECK_MESSAGE(
                    CheckFileDiff(actual_prefix + "." + suff, etalon_prefix + "." + suff),
                    "Check for suffix " + suff + " failed");
        }
    } else {
        INFO("Checking differences for graphs: graph isomorphism");
        ColoredGraphIsomorphismChecker<conj_graph_pack> checker(k, "tmp");
        BOOST_CHECK_MESSAGE(
                checker.Check(actual_prefix, etalon_prefix),
                "GRAPHS DIFFER");
    }
}

inline void RegenerateEtalon(size_t k, const string& filename,
                             const string& output_dir,
                             const string& etalon_root,
                             const string& work_dir) {
    remove_dir(etalon_root);
    make_dir(etalon_root);
    SyntheticTestsRunner<runtime_k::RtSeq> test_runner(filename, k,
                                                       etalon_root,
                                                       work_dir);
    test_runner.Run();
}

template<class Seq>
void RunTests(size_t k, const string& filename, const string& output_dir,
                     const string& etalon_root, const string& work_dir,
                     bool exact_match = true,
                     const vector<size_t>& example_ids = vector<size_t>()) {
    SyntheticTestsRunner<Seq> test_runner(filename, k, output_dir, work_dir);
    vector<size_t> launched = test_runner.Run();
    FOREACH(size_t id, launched) {
        CheckDiffs(k, output_dir + ToString(id), etalon_root + ToString(id), exact_match);
    }
}

BOOST_AUTO_TEST_CASE( SyntheticExamplesTestsRtSeq ) {
    utils::TmpFolderFixture _("tmp");
    make_dir("simulated_tests");
    string input_dir = "./src/test/cap/tests/synthetic/";
    RunTests<runtime_k::RtSeq> (25, input_dir + "tests.xml",
                                "simulated_tests/",
                                input_dir + "etalon/",
                                "tmp", false);
    remove_dir("simulated_tests");
}

BOOST_AUTO_TEST_CASE( SyntheticExamplesTestsLSeq ) {
    utils::TmpFolderFixture _("tmp");
    make_dir("simulated_tests");
    string input_dir = "./src/test/cap/tests/synthetic/";
    RunTests<LSeq> (25, input_dir + "tests.xml",
                                "simulated_tests/",
                                input_dir + "etalon/",
                                "tmp", false);
    remove_dir("simulated_tests");
}

/*
 BOOST_AUTO_TEST_CASE( SyntheticExamplesWithErrorsTests ) {
 utils::TmpFolderFixture _("tmp");
 make_dir("bp_graph_test");
 LoadAndRunBPG<15, 25, runtime_k::RtSeq>("./src/test/cap/tests/synthetic_with_err/tests2.xml",
 "bp_graph_test/simulated_common_err/", "./src/test/cap/tests/synthetic_with_err/etalon/", "1_err", false);
 LoadAndRunBPG<15, 25, LSeq>("./src/test/cap/tests/synthetic_with_err_lseq/tests2.xml",
 "bp_graph_test/simulated_common_err/", "./src/test/cap/tests/synthetic_with_err_lseq/etalon/", "1_err_lseq", false);
 remove_dir("bp_graph_test");
 }
 */
}
