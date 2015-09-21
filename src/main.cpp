#include "MRIOTable.h"
#include "Disaggregation.h"
#include <iostream>
#include <deque>
#include <stdexcept>
#include <fstream>
#include <cassert>
#include <cstdlib>
#include <stdlib.h>
#include <argp.h>

using namespace std;

// Define types used in templates
typedef unsigned short I; // Index type
typedef float T; // Data type

const char* argp_program_bug_address = "<sven.willner@pik-potsdam.de>";
static char doc[] = "Flexible algorithm for regional and sectoral disaggregation of multi-regional input-output tables.\n\
Described in:\n\
    L. Wenz, S.N. Willner, A. Radebach, R. Bierkandt, J.C. Steckel, A. Levermann:\
 Regional and sectoral disaggregation of multi-regional input-output tables: a flexible algorithm\
 Economic Systems Research 27 (2015), DOI: 10.1080/09535314.2014.987731\n\n\
Source: https://github.com/swillner/disaggregation";
static char args_doc[] = "INDEX BASETABLE SETTINGS PROXIES OUTPUTFILE";
static struct argp_option options[] = {
    { "threshold", 't', "THRESHOLD", 0, "Only read values > THRESHOLD from basetable (default: 0)" },
    { 0 }
};

struct arguments {
    T threshold;
    char* basetable_csv;
    char* index_csv;
    char* settings_csv;
    char* proxies_csv;
    char* output_file;
};

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    struct arguments* args = static_cast<arguments*>(state->input);
    switch (key) {
        case 't':
            args->threshold = stof(arg);
            cout << args->threshold << endl;
            break;

        case ARGP_KEY_ARG:
            if (state->arg_num >= 5) {
                argp_usage(state);
            }
            switch (state->arg_num) {
                case 0:
                    args->index_csv = arg;
                    break;
                case 1:
                    args->basetable_csv = arg;
                    break;
                case 2:
                    args->settings_csv = arg;
                    break;
                case 3:
                    args->proxies_csv = arg;
                    break;
                case 4:
                    args->output_file = arg;
                    break;
            }
            break;

        case ARGP_KEY_END:
            if (state->arg_num < 5) {
                argp_usage(state);
            }
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char* argv[]) {
#ifndef DEBUG
    try {
#endif
        struct arguments arguments;
        arguments.threshold = 0;
        argp_parse(&argp, argc, argv, 0, 0, &arguments);

        cout << "Loading basetable... " << flush;
        ifstream indices(arguments.index_csv);
        if (!indices) {
            throw runtime_error("Could not open indices file");
        }
        ifstream data(arguments.basetable_csv);
        if (!data) {
            throw runtime_error("Could not open data file");
        }
        mrio::Table<T, I> basetable;
        basetable.read_from_csv(indices, data, arguments.threshold);
        indices.close();
        data.close();
        cout << "done" << endl;

        mrio::Disaggregation<T, I> disaggregation(&basetable);

        cout << "Loading settings... " << flush;
        disaggregation.read_disaggregation_file(arguments.settings_csv);
        cout << "done" << endl;

        cout << "Loading proxies... " << flush;
        disaggregation.read_proxy_file(arguments.proxies_csv);
        cout << "done" << endl;

        cout << "Applying disaggregation algorithm... " << flush;
        disaggregation.refine();
        cout << "done" << endl;

        cout << "Writing disaggregated table... " << flush;
        ofstream outfile(arguments.output_file);
        if (!outfile) {
            throw runtime_error("Could not create output file");
        }
        disaggregation.refined_table().write_to_csv(outfile);
        outfile.close();
        cout << "done" << endl;

        return 0;
#ifndef DEBUG
    } catch (const exception& ex) {
        cerr << ex.what() << endl;
        return -1;
    }
#endif
}
