/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>

  This file is part of libmrio.

  libmrio is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  libmrio is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with libmrio.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "disaggregation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

#include "MRIOTable.h"
#include "ProxyData.h"
#include "csv-parser.h"
#ifdef LIBMRIO_SHOW_PROGRESS
#include "progressbar.h"
#endif
#include "settingsnode.h"

namespace mrio {

template<typename T, typename I>
Table<T, I> disaggregate(const Table<T, I>& basetable, const settings::SettingsNode& settings_node) {
    Table<T, I> table{basetable};

    for (const auto& subs_node : settings_node["subs"].as_sequence()) {
        std::vector<std::string> subs;
        for (const auto& sub : subs_node["into"].as_sequence()) {
            subs.emplace_back(sub.as<std::string>());
        }
        const auto& id = subs_node["id"].as<std::string>();
        const auto& type = subs_node["type"].as<settings::hstring>();
        switch (type) {
            case settings::hstring::hash("sector"):
                try {
                    table.insert_subsectors(id, subs);
                } catch (std::out_of_range& ex) {
                    throw std::runtime_error("Sector '" + id + "' not found");
                }
                break;
            case settings::hstring::hash("region"):
                try {
                    table.insert_subregions(id, subs);
                } catch (std::out_of_range& ex) {
                    throw std::runtime_error("Region '" + id + "' not found");
                }
                break;
                throw std::runtime_error("Unknown type");
        }
    }

    Table<T, I> last_table{table.index_set(), 0};  // table in disaggregation used for accessing d-1 values
    Table<std::size_t, I> quality{table.index_set(), 0};

    std::vector<FullIndex<I>> full_indices;
    for (const auto& ir : table.index_set().super_indices) {
        if (ir.sector->has_sub() || ir.region->has_sub()) {
            for (const auto& js : table.index_set().super_indices) {
                full_indices.emplace_back(FullIndex<I>{ir.sector, ir.region, js.sector, js.region});
            }
        } else {
            for (const auto& js : table.index_set().super_indices) {
                if (js.sector->has_sub() || js.region->has_sub()) {
                    full_indices.emplace_back(FullIndex<I>{ir.sector, ir.region, js.sector, js.region});
                }
            }
        }
    }

    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(std::begin(full_indices), std::end(full_indices), g);
    }

    std::size_t d = 1;
    for (const auto& proxy_node : settings_node["proxies"].as_sequence()) {
#ifdef LIBMRIO_SHOW_PROGRESS
        std::cout << "Proxy " << d << ":\n" << std::flush;
#endif
        ProxyData<T, I> proxy(table.index_set());
        proxy.read_from_file(proxy_node);
        last_table.replace_table_from(table);

        proxy.approximate(full_indices, table, quality, last_table, d);
        proxy.adjust(full_indices, table, quality, basetable, d);

        ++d;
    }

    return table;
}

template Table<double, std::size_t> disaggregate(const Table<double, std::size_t>& basetable, const settings::SettingsNode& settings_node);
template Table<float, std::size_t> disaggregate(const Table<float, std::size_t>& basetable, const settings::SettingsNode& settings_node);

}  // namespace mrio
