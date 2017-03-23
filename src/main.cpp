#include <Disaggregation.h>
#include <MRIOTable.h>
#include <settingsnode.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef DEBUG
#include <cassert>
#else
#define assert(a) \
    {}
#endif

// Define types used in templates
using I = unsigned int;  // Index type
using T = double;        // Data type

static void print_usage(const char* program_name) {
    std::cerr << "Regional and sectoral disaggregation of multi-regional input-output tables\n"
                 "Version:  " LIBMRIO_VERSION
                 "\n"
                 "Author:   Sven Willner <sven.willner@pik-potsdam.de>\n\n"
                 "Algorithm described in (please cite when using)\n"
                 "   L. Wenz, S.N. Willner, A. Radebach, R. Bierkandt, J.C. Steckel, A. Levermann.\n"
                 "   Regional and sectoral disaggregation of multi-regional input-output tables:\n"
                 "   a flexible algorithm. Economic Systems Research 27 (2015).\n"
                 "   DOI: 10.1080/09535314.2014.987731\n\n"
                 "Source:   https://github.com/swillner/disaggregation\n"
                 "License:  ISC, (c) 2014-2017 Sven Willner (see LICENSE file)\n\n"
                 "Usage: "
              << program_name
              << " (<option> | <settingsfile>)\n"
                 "Options:\n"
                 "   -h, --help     Print this help text\n"
                 "   -v, --version  Print version"
              << std::endl;
}

int main(int argc, char* argv[]) {
#ifndef DEBUG
    try {
#endif
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }
        const std::string arg = argv[1];
        if (arg.length() > 1 && arg[0] == '-') {
            if (arg == "--version" || arg == "-v") {
                std::cout << LIBMRIO_VERSION << std::endl;
            } else if (arg == "--help" || arg == "-h") {
                print_usage(argv[0]);
            } else {
                print_usage(argv[0]);
                return 1;
            }
        } else {
            settings::SettingsNode settings;
            if (arg == "-") {
                std::cin >> std::noskipws;
                settings = settings::SettingsNode(std::cin);
            } else {
                std::ifstream settings_file(arg);
                settings = settings::SettingsNode(settings_file);
            }
            std::cout << "Loading basetable... " << std::flush;
            std::ifstream indices(settings["basetable"]["index"].as<std::string>());
            if (!indices) {
                std::cout << "error" << std::endl;
                throw std::runtime_error("Could not open indices file");
            }
            std::ifstream data(settings["basetable"]["index"].as<std::string>());
            if (!data) {
                std::cout << "error" << std::endl;
                throw std::runtime_error("Could not open data file");
            }
            mrio::Table<T, I> basetable;
            basetable.read_from_csv(indices, data, settings["basetable"]["threshold"].as<T>());
            indices.close();
            data.close();
            std::cout << "done" << std::endl;

            mrio::Disaggregation<T, I> disaggregation(&basetable);

            std::cout << "Loading settings... " << std::flush;
            disaggregation.read_disaggregation_file(settings["disaggregation"]["file"].as<std::string>());
            std::cout << "done" << std::endl;

            std::cout << "Loading proxies... " << std::flush;
            disaggregation.read_proxy_file(settings["proxies"]["file"].as<std::string>());
            std::cout << "done" << std::endl;

            std::cout << "Applying disaggregation algorithm... " << std::flush;
            disaggregation.refine();
            std::cout << "done" << std::endl;

            std::cout << "Writing disaggregated table... " << std::flush;
            std::ofstream outfile(settings["output"]["file"].as<std::string>());
            if (!outfile) {
                std::cout << "error" << std::endl;
                throw std::runtime_error("Could not create output file");
            }
            disaggregation.refined_table().write_to_csv(outfile);
            outfile.close();
            std::cout << "done" << std::endl;
        }
#ifndef DEBUG
    } catch (std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return -1;
    }
#endif
    return 0;
}
