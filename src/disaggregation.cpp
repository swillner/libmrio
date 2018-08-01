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

template<typename T, typename I, typename Func>
static inline void do_for_all_sub(Func func, const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
    func(i, r, j, s);
}

template<typename T, typename I, typename Inner, typename... Arguments>
static inline void do_for_all_sub(const Inner* k, const Arguments&... params) {
    do_for_all_sub<T, I>(params..., k);
}

template<typename T, typename I, typename Inner, typename... Arguments>
static inline void do_for_all_sub(const std::vector<Inner*>& vec, const Arguments&... params) {
    for (const auto k : vec) {
        do_for_all_sub<T, I>(params..., k);
    }
}

template<typename T, typename I, typename Func, typename... Arguments>
static inline void for_all_sub(Func func, const Arguments&... params) {
    do_for_all_sub<T, I>(params..., func);
}

template<typename T, typename I, typename... Arguments>
static inline void for_all_sub(const Region<I>* r, const Arguments&... params) {
    if (r->has_sub()) {
        for_all_sub<T, I>(params..., r->as_super()->sub());
    } else {
        for_all_sub<T, I>(params..., r);
    }
}

template<typename T, typename I, typename... Arguments>
static inline void for_all_sub(const Sector<I>* i, const Arguments&... params) {
    if (i->has_sub()) {
        for_all_sub<T, I>(params..., i->as_super()->sub());
    } else {
        for_all_sub<T, I>(params..., i);
    }
}

template<typename T, typename I>
mrio::Table<T, I> disaggregate(const mrio::Table<T, I>& basetable, const settings::SettingsNode& settings_node) {
    mrio::Table<T, I> table{basetable};

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

    mrio::Table<T, I> last_table{table.index_set(), 0};  // table in disaggregation used for accessing d-1 values
    mrio::Table<std::size_t, I> quality{table.index_set(), 0};

    struct FullIndex {
        const Sector<I>* sector_from;
        const Region<I>* region_from;
        const Sector<I>* sector_to;
        const Region<I>* region_to;
    };

    std::vector<FullIndex> full_indices;
    for (const auto& ir : table.index_set().super_indices) {
        if (ir.sector->has_sub() || ir.region->has_sub()) {
            for (const auto& js : table.index_set().super_indices) {
                full_indices.emplace_back(FullIndex{ir.sector, ir.region, js.sector, js.region});
            }
        } else {
            for (const auto& js : table.index_set().super_indices) {
                if (js.sector->has_sub() || js.region->has_sub()) {
                    full_indices.emplace_back(FullIndex{ir.sector, ir.region, js.sector, js.region});
                }
            }
        }
    }

    std::sort(std::begin(full_indices), std::end(full_indices), [](const FullIndex& a, const FullIndex& b) {
        return (a.region_from->has_sub() + a.sector_from->has_sub() + a.region_to->has_sub() + a.sector_to->has_sub())
               < (b.region_from->has_sub() + b.sector_from->has_sub() + b.region_to->has_sub() + b.sector_to->has_sub());
    });

    std::size_t d = 1;
    for (const auto& proxy_node : settings_node["proxies"].as_sequence()) {
        ProxyData<T, I> proxy(table.index_set());
        proxy.read_from_file(proxy_node);
        last_table.replace_table_from(table);

        // approximation step:
        {
#ifdef LIBMRIO_SHOW_PROGRESS
            progressbar::ProgressBar bar(full_indices.size(), "Approximation");
#endif
#pragma omp parallel for default(shared) schedule(guided)
            for (std::size_t k = 0; k < full_indices.size(); ++k) {
                const auto& full_index = full_indices[k];
                for_all_sub<T, I>(full_index.sector_from, full_index.region_from, full_index.sector_to, full_index.region_to,
                                  [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
                                      T value;
                                      if (proxy.apply(value, last_table, i, r, j, s)) {
                                          if (!std::isnan(value)) {
                                              table(i, r, j, s) = value;
                                              quality(i, r, j, s) = d;
                                          }
                                      }
                                  });
#ifdef LIBMRIO_SHOW_PROGRESS
                ++bar;
#endif
            }
        }

        // adjustment step:
        {
#ifdef LIBMRIO_SHOW_PROGRESS
            progressbar::ProgressBar bar(full_indices.size(), "Adjustment");
#endif
#pragma omp parallel for default(shared) schedule(guided)
            for (std::size_t k = 0; k < full_indices.size(); ++k) {
                const auto& full_index = full_indices[k];
                const T& base = basetable.base(full_index.sector_from, full_index.region_from, full_index.sector_to, full_index.region_to);
                if (base > 0) {
                    T sum_of_exact = 0;
                    T sum_of_non_exact = 0;
                    for_all_sub<T, I>(full_index.sector_from, full_index.region_from, full_index.sector_to, full_index.region_to,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
                                          if (quality(i, r, j, s) == d) {
                                              sum_of_exact += table(i, r, j, s);
                                          } else {
                                              sum_of_non_exact += table(i, r, j, s);
                                          }
                                      });
                    T correction_factor = base / (sum_of_exact + sum_of_non_exact);
                    if (base > sum_of_exact && sum_of_non_exact > 0) {
                        for_all_sub<T, I>(full_index.sector_from, full_index.region_from, full_index.sector_to, full_index.region_to,
                                          [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
                                              if (quality(i, r, j, s) != d) {
                                                  table(i, r, j, s) = (base - sum_of_exact) * table(i, r, j, s) / sum_of_non_exact;
                                              }
                                          });
                    } else if (correction_factor < 1 || correction_factor > 1) {
                        for_all_sub<T, I>(full_index.sector_from, full_index.region_from, full_index.sector_to, full_index.region_to,
                                          [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
                                              table(i, r, j, s) = correction_factor * table(i, r, j, s);
                                          });
                    }
                }
#ifdef LIBMRIO_SHOW_PROGRESS
                ++bar;
#endif
            }
        }

        ++d;
    }

    return table;
}

template mrio::Table<double, std::size_t> disaggregate(const mrio::Table<double, std::size_t>& basetable, const settings::SettingsNode& settings_node);
template mrio::Table<float, std::size_t> disaggregate(const mrio::Table<float, std::size_t>& basetable, const settings::SettingsNode& settings_node);
}  // namespace mrio
