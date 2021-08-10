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

#include "ProxyData.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include "MRIOTable.h"
#include "csv-parser.h"
#include "progressbar.h"
#include "settingsnode.h"

#ifdef LIBMRIO_VERBOSE
struct DebugGroupGuard {
    static std::size_t debug_group_level;
    DebugGroupGuard() { ++debug_group_level; }
    ~DebugGroupGuard() { --debug_group_level; }
};
std::size_t DebugGroupGuard::debug_group_level = 0;
#define debug(a)                                                                                                   \
    {                                                                                                              \
        for (std::size_t debug_param_i = 0; debug_param_i < DebugGroupGuard::debug_group_level; ++debug_param_i) { \
            std::cout << "  ";                                                                                     \
        }                                                                                                          \
        std::cout << a << std::endl;                                                                               \
    }                                                                                                              \
    DebugGroupGuard _debug_group_guard;
#define debugp(i, r, j, s) ((i) ? (i)->name : "") << ":" << ((r) ? (r)->name : "") << "->" << ((j) ? (j)->name : "") << ":" << ((s) ? (s)->name : "")
#define debug_begin_group() \
    { ++debug_group_level; }
#define debug_end_group() \
    { --debug_group_level; }
#else
#define debug_begin_group() \
    {}
#define debug_end_group() \
    {}
#define debug(a) \
    {}
#define debugp(i, r, j, s) \
    {}
#endif

#ifdef DEBUG
#include <cassert>
#else
#undef assert
#define assert(a) \
    {}
#endif

namespace mrio {

template<typename T, typename I>
void ProxyData<T, I>::set_clusters_for_foreign(MappingIndexPart* foreign_index_part) {
    foreign_index_part->foreign_cluster->insert(foreign_index_part);
    for (auto& native_index_part : foreign_index_part->mapped_to) {
        if (!native_index_part->foreign_cluster) {
            native_index_part->native_cluster = foreign_index_part->native_cluster;
            native_index_part->foreign_cluster = foreign_index_part->foreign_cluster;
            set_clusters_for_native(native_index_part);
        }
        assert(native_index_part->native_cluster == foreign_index_part->native_cluster);
        assert(native_index_part->foreign_cluster == foreign_index_part->foreign_cluster);
    }
}

template<typename T, typename I>
void ProxyData<T, I>::set_clusters_for_native(MappingIndexPart* native_index_part) {
    native_index_part->native_cluster->insert(native_index_part);
    for (auto& foreign_index_part : native_index_part->mapped_to) {
        if (!foreign_index_part->native_cluster) {
            foreign_index_part->native_cluster = native_index_part->native_cluster;
            foreign_index_part->foreign_cluster = native_index_part->foreign_cluster;
            set_clusters_for_foreign(foreign_index_part);
        }
        assert(foreign_index_part->native_cluster == native_index_part->native_cluster);
        assert(foreign_index_part->foreign_cluster == native_index_part->foreign_cluster);
    }
}

template<typename T, typename I>
void ProxyData<T, I>::read_mapping_from_file(const settings::SettingsNode& mapping_node, ProxyIndex& index) {
    const auto& filename = mapping_node["file"].as<std::string>();
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open mapping file " + filename);
    }

    switch (index.type) {
        case ProxyIndex::Type::SECTOR:
            index.native_indices.reserve(table_indices.supersectors().size());
            for (const auto& i : table_indices.supersectors()) {
                index.native_indices.emplace_back(new MappingIndexPart{i.get()});
            }
            break;
        case ProxyIndex::Type::SUBSECTOR:
            index.native_indices.reserve(table_indices.subsectors().size());
            for (const auto& i : table_indices.subsectors()) {
                index.native_indices.emplace_back(new MappingIndexPart{i.get()});
            }
            break;
        case ProxyIndex::Type::REGION:
            index.native_indices.reserve(table_indices.superregions().size());
            for (const auto& r : table_indices.superregions()) {
                index.native_indices.emplace_back(new MappingIndexPart{r.get()});
            }
            break;
        case ProxyIndex::Type::SUBREGION:
            index.native_indices.reserve(table_indices.subregions().size());
            for (const auto& r : table_indices.subregions()) {
                index.native_indices.emplace_back(new MappingIndexPart{r.get()});
            }
            break;
    }

    try {
        csv::Parser in(file);
        std::size_t foreign_column = 0;
        std::size_t native_column = 0;
        {
            bool foreign_column_found = false;
            bool native_column_found = false;
            const auto& foreign_column_name = mapping_node["foreign_column"].as<std::string>();
            const auto& native_column_name = mapping_node["native_column"].as<std::string>();
            std::size_t i = 0;
            do {
                const auto& name = in.read<std::string>();
                if (name == foreign_column_name) {
                    foreign_column = i;
                    foreign_column_found = true;
                    if (native_column_found) {
                        break;
                    }
                } else if (name == native_column_name) {
                    native_column = i;
                    native_column_found = true;
                    if (foreign_column_found) {
                        break;
                    }
                }
                ++i;
            } while (in.next_col());
            if (!foreign_column_found) {
                throw std::runtime_error("Column " + foreign_column_name + " not found in " + filename);
            }
            if (!native_column_found) {
                throw std::runtime_error("Column " + native_column_name + " not found in " + filename);
            }
        }

        while (in.next_row()) {
            for (std::size_t i = 0; i < std::min(foreign_column, native_column); ++i) {
                in.next_col();
            }
            auto a = in.read_and_next<std::string>();
            if (a == "-") {
                continue;
            }
            for (std::size_t i = std::min(foreign_column, native_column) + 1; i < std::max(foreign_column, native_column); ++i) {
                in.next_col();
            }
            auto b = in.read_and_next<std::string>();
            if (b == "-") {
                continue;
            }
            std::string foreign_id;
            std::string native_id;
            if (foreign_column < native_column) {
                foreign_id = a;
                native_id = b;
            } else {
                native_id = a;
                foreign_id = b;
            }
            MappingIndexPart* foreign_index_part;
            auto foreign_it = index.foreign_indices_map.find(foreign_id);
            if (foreign_it == std::end(index.foreign_indices_map)) {
#ifdef LIBMRIO_VERBOSE
                foreign_index_part = new MappingIndexPart{index.foreign_indices.size(), foreign_id};
#else
                foreign_index_part = new MappingIndexPart{index.foreign_indices.size()};
#endif
                index.foreign_indices.emplace_back(foreign_index_part);
                index.foreign_indices_map.emplace(foreign_id, foreign_index_part);
            } else {
                foreign_index_part = foreign_it->second;
            }
            std::size_t native_index;
            switch (index.type) {
                case ProxyIndex::Type::SECTOR:
                case ProxyIndex::Type::SUBSECTOR:
                    try {
                        native_index = table_indices.sector(native_id)->level_index();
                        break;
                    } catch (std::out_of_range& ex) {
                        throw std::runtime_error("Sector " + native_id + " from " + filename + " not found");
                    }
                case ProxyIndex::Type::REGION:
                case ProxyIndex::Type::SUBREGION:
                    try {
                        native_index = table_indices.region(native_id)->level_index();
                        break;
                    } catch (std::out_of_range& ex) {
                        throw std::runtime_error("Region " + native_id + " from " + filename + " not found");
                    }
            }
            auto native_index_part = index.native_indices[native_index].get();
            foreign_index_part->mapped_to.insert(native_index_part);
            native_index_part->mapped_to.insert(foreign_index_part);
        }
    } catch (const csv::parser_exception& ex) {
        throw std::runtime_error(ex.format(filename));
    }

    for (auto& native_index_part : index.native_indices) {
        if (native_index_part->mapped_to.size() > 0 && !native_index_part->native_cluster) {
            native_index_part->native_cluster = std::make_shared<std::unordered_set<MappingIndexPart*>>();
            native_index_part->foreign_cluster = std::make_shared<std::unordered_set<MappingIndexPart*>>();
            set_clusters_for_native(native_index_part.get());
        }
    }

    for (auto& native_index_part : index.native_indices) {
        if (native_index_part->native_cluster) {
            debug(*native_index_part << ": native={ " << *native_index_part->native_cluster << "}, foreign={ " << *native_index_part->foreign_cluster << "}");
        } else {
            debug(*native_index_part << ": not in mapping");
        }
    }

    for (auto& foreign_index_part : index.foreign_indices) {
        if (foreign_index_part->native_cluster) {
            debug(*foreign_index_part << ": native={ " << *foreign_index_part->native_cluster << "}, foreign={ " << *foreign_index_part->foreign_cluster
                                      << "}");
        } else {
            debug(*foreign_index_part << ": not in mapping");
        }
    }
}

template<typename T, typename I>
void ProxyData<T, I>::read_from_file(const settings::SettingsNode& settings_node) {
    debug("");
    const auto& filename = settings_node["file"].as<std::string>();
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open proxy file " + filename);
    }

    try {
        std::vector<Column> columns;
        std::unordered_set<std::string> column_names;
        csv::Parser in(file);
        const auto& columns_node = settings_node["columns"];

        std::size_t size = 1;
        do {
            Column column;
            auto name = in.read<std::string>();
            if (columns_node.has(name)) {
                const auto& column_node = columns_node[name];
                const auto& type = column_node["type"].as<std::string>();
                if (type == "select") {
                    column.type = Column::Type::SELECT;
                    column.value = column_node["value"].as<std::string>();
                } else if (type == "value") {
                    column.type = Column::Type::VALUE;
                } else {
                    column.type = Column::Type::INDEX;
                    typename ProxyIndex::Type index_type;
                    std::size_t index_native_size;
                    switch (settings::hstring(type)) {
                        case settings::hstring::hash("sector"):
                            index_type = ProxyIndex::Type::SECTOR;
                            index_native_size = table_indices.supersectors().size();
                            break;
                        case settings::hstring::hash("subsector"):
                            index_type = ProxyIndex::Type::SUBSECTOR;
                            index_native_size = table_indices.subsectors().size();
                            break;
                        case settings::hstring::hash("region"):
                            index_type = ProxyIndex::Type::REGION;
                            index_native_size = table_indices.superregions().size();
                            break;
                        case settings::hstring::hash("subregion"):
                            index_type = ProxyIndex::Type::SUBREGION;
                            index_native_size = table_indices.subregions().size();
                            break;
                        default:
                            throw std::runtime_error("Unknown column type " + type);
                    }
                    std::unique_ptr<ProxyIndex> index{new ProxyIndex{column_node.has("mapping"), index_type}};
                    if (index->mapped) {
                        read_mapping_from_file(column_node["mapping"], *index);
                        index->size = index->foreign_indices.size();
                    } else {
                        index->size = index_native_size;
                    }
                    size *= index->size;
                    column.index = index.release();
                    indices.emplace_back(column.index);
                }
                column_names.insert(name);
            } else {  // !columns_node.has(name)
                column.type = Column::Type::IGNORE;
            }
            columns.emplace_back(column);
        } while (in.next_col());

        for (const auto& column_node : columns_node.as_map()) {
            if (column_names.count(column_node.first) == 0) {
                throw std::runtime_error("Column " + column_node.first + " not found in " + filename);
            }
        }

        if (indices.size() == 0) {
            throw std::runtime_error("Proxies must not be empty");
        }

        data.resize(size, std::numeric_limits<T>::quiet_NaN());

        while (in.next_row()) {
            std::size_t value_index = 0;
            T value = 0;
            bool skip = false;
            for (const auto& column : columns) {
                switch (column.type) {
                    case Column::Type::SELECT:
                        skip = column.value != in.read_and_next<std::string>();
                        break;
                    case Column::Type::IGNORE:
                        in.next_col();
                        break;
                    case Column::Type::VALUE:
                        value = in.read_and_next<T>();
                        break;
                    case Column::Type::INDEX: {
                        auto str = in.read_and_next<std::string>();
                        value_index *= column.index->size;
                        switch (column.index->type) {
                            case ProxyIndex::Type::SECTOR:
                                try {
                                    if (column.index->mapped) {
                                        value_index += column.index->foreign_indices_map.at(str)->index;
                                    } else {
                                        value_index += table_indices.sector(str)->level_index();
                                    }
                                } catch (std::out_of_range& ex) {
                                    throw std::runtime_error("Sector " + str + " from " + filename + " not found");
                                }
                                break;
                            case ProxyIndex::Type::SUBSECTOR:
                                try {
                                    if (column.index->mapped) {
                                        value_index += column.index->foreign_indices_map.at(str)->index;
                                    } else {
                                        value_index += table_indices.sector(str)->level_index();
                                    }
                                } catch (std::out_of_range& ex) {
                                    throw std::runtime_error("Sector " + str + " from " + filename + " not found");
                                }
                                break;
                            case ProxyIndex::Type::REGION:
                                try {
                                    if (column.index->mapped) {
                                        value_index += column.index->foreign_indices_map.at(str)->index;
                                    } else {
                                        value_index += table_indices.region(str)->level_index();
                                    }
                                } catch (std::out_of_range& ex) {
                                    throw std::runtime_error("Region " + str + " from " + filename + " not found");
                                }
                                break;
                            case ProxyIndex::Type::SUBREGION:
                                try {
                                    if (column.index->mapped) {
                                        value_index += column.index->foreign_indices_map.at(str)->index;
                                    } else {
                                        value_index += table_indices.region(str)->level_index();
                                    }
                                } catch (std::out_of_range& ex) {
                                    throw std::runtime_error("Region " + str + " from " + filename + " not found");
                                }
                                break;
                        }
                    } break;
                }
                if (skip) {
                    break;
                }
            }
            if (!skip) {
                if (value < 0) {
                    throw std::runtime_error("Invalid proxy value " + std::to_string(value) + " in " + filename);
                }
#ifdef DEBUG
                data.at(value_index) = value;
#else
                data[value_index] = value;
#endif
            }
        }
    } catch (const csv::parser_exception& ex) {
        throw std::runtime_error(ex.format(filename));
    }

    for (const auto& application_node : settings_node["applications"].as_sequence()) {
        std::unique_ptr<Application> application{new Application{}};
        std::size_t i = 0;
        for (const auto& index_node : application_node.as_sequence()) {
            if (i >= indices.size()) {
                throw std::runtime_error("Too many indices for application given");
            }
            auto index_str = index_node.as<settings::hstring>();
            switch (index_str) {
                case settings::hstring::hash("i"):
                    if (indices[i]->type != ProxyIndex::Type::SECTOR && indices[i]->type != ProxyIndex::Type::SUBSECTOR) {
                        throw std::runtime_error("Cannot apply non-sector column to sector index i");
                    }
                    application->i = indices[i].get();
                    break;
                case settings::hstring::hash("r"):
                    if (indices[i]->type != ProxyIndex::Type::REGION && indices[i]->type != ProxyIndex::Type::SUBREGION) {
                        throw std::runtime_error("Cannot apply non-region column to region index r");
                    }
                    application->r = indices[i].get();
                    break;
                case settings::hstring::hash("j"):
                    if (indices[i]->type != ProxyIndex::Type::SECTOR && indices[i]->type != ProxyIndex::Type::SUBSECTOR) {
                        throw std::runtime_error("Cannot apply non-sector column to sector index j");
                    }
                    application->j = indices[i].get();
                    break;
                case settings::hstring::hash("s"):
                    if (indices[i]->type != ProxyIndex::Type::REGION && indices[i]->type != ProxyIndex::Type::SUBREGION) {
                        throw std::runtime_error("Cannot apply non-region column to region index s");
                    }
                    application->s = indices[i].get();
                    break;
                default:
                    throw std::runtime_error("Unknown index name " + index_str);
            }
            ++i;
        }
        if (i < indices.size()) {
            throw std::runtime_error("All indices must be used for application");
        }
        applications.emplace_back(application.release());
    }
}

template<typename T, typename I>
inline T ProxyData<T, I>::sum_proxy_over_all_foreign_clusters_helper(I index,
                                                                     I level_index,
                                                                     const ProxyIndex* proxy_index,
                                                                     const ProxyIndex* i,
                                                                     const ProxyIndex* r,
                                                                     const ProxyIndex* j,
                                                                     const ProxyIndex* s,
                                                                     const Sector<I>* i_p,
                                                                     const Region<I>* r_p,
                                                                     const Sector<I>* j_p,
                                                                     const Region<I>* s_p) const {
    T res = 0;
    index *= proxy_index->size;
    if (proxy_index->mapped) {
        const auto& cluster = proxy_index->native_indices[level_index]->foreign_cluster;
        if (!cluster) {
            debug("index value not in mapping");
            // level_index is not included in mapping
            return std::numeric_limits<T>::quiet_NaN();
        }
        debug("sum over foreign cluster " << *cluster);
        for (const auto& k : *cluster) {
            res += sum_proxy_over_all_foreign_clusters(index + k->index, i, r, j, s, i_p, r_p, j_p, s_p);
        }
    } else {
        debug("index does not need to be mapped");
        res += sum_proxy_over_all_foreign_clusters(index + level_index, i, r, j, s, i_p, r_p, j_p, s_p);
    }
    return res;
}

template<typename T, typename I>
inline T ProxyData<T, I>::sum_proxy_over_all_foreign_clusters(I index,
                                                              const ProxyIndex* i,
                                                              const ProxyIndex* r,
                                                              const ProxyIndex* j,
                                                              const ProxyIndex* s,
                                                              const Sector<I>* i_p,
                                                              const Region<I>* r_p,
                                                              const Sector<I>* j_p,
                                                              const Region<I>* s_p) const {
    if (i != nullptr) {
        debug("sum proxy mapping share for i (" << debugp(i_p, r_p, j_p, s_p) << ")");
        return sum_proxy_over_all_foreign_clusters_helper(index, i_p->level_index(), i, nullptr, r, j, s, i_p, r_p, j_p, s_p);
    } else if (r != nullptr) {
        debug("sum proxy mapping share for r (" << debugp(i_p, r_p, j_p, s_p) << ")");
        return sum_proxy_over_all_foreign_clusters_helper(index, r_p->level_index(), r, i, nullptr, j, s, i_p, r_p, j_p, s_p);
    } else if (j != nullptr) {
        debug("sum proxy mapping share for j (" << debugp(i_p, r_p, j_p, s_p) << ")");
        return sum_proxy_over_all_foreign_clusters_helper(index, j_p->level_index(), j, i, r, nullptr, s, i_p, r_p, j_p, s_p);
    } else if (s != nullptr) {
        debug("sum proxy mapping share for s (" << debugp(i_p, r_p, j_p, s_p) << ")");
        return sum_proxy_over_all_foreign_clusters_helper(index, s_p->level_index(), s, i, r, j, nullptr, i_p, r_p, j_p, s_p);
    } else {
        debug("return data[" << index << "] = " << data[index]);
        return data[index];
    }
}

template<typename T, typename I>
T ProxyData<T, I>::get_mapped_value(
    const Application* application, const Table<T, I>& table, const Sector<I>* i_p, const Region<I>* r_p, const Sector<I>* j_p, const Region<I>* s_p) const {
    // sum foreign ones (all combinations) and take share of native ones (product for all indices)
    T proxy_value = sum_proxy_over_all_foreign_clusters(0, application->i, application->r, application->j, application->s, i_p, r_p, j_p, s_p);
    if (!std::isnan(proxy_value)) {
        if (application->i != nullptr && application->i->mapped) {
            debug("i has mapping -> calc flow mapping share (" << debugp(i_p, r_p, j_p, s_p) << ")");
            T sum = 0;
            const auto& cluster = application->i->native_indices[i_p->level_index()]->native_cluster;
            if (!cluster) {
                debug(i_p->name << " not in mapping");
                return std::numeric_limits<T>::quiet_NaN();
            }
            if (cluster->size() > 0) {
                debug("sum over native cluster " << *cluster);
                for (const auto& k : *cluster) {
                    I from = table_indices(k->index, *r_p);
                    I to = table_indices(j_p, s_p);
                    if (from != IndexSet<I>::NOT_GIVEN && to != IndexSet<I>::NOT_GIVEN) {
                        sum += table(from, to);
                    } else {
                        // need to sum, but regions involved do not have corresponding sectors -> assume region does not have sector -> 0
                        debug("cannot use " << debugp(k, r_p, j_p, s_p));
                    }
                }
                proxy_value *= table(i_p, r_p, j_p, s_p) / sum;
            } else {
                debug("mapped x to 1 -> no summing necessary");
            }
        }
        if (application->r != nullptr && application->r->mapped) {
            debug("r has mapping -> calc flow mapping share (" << debugp(i_p, r_p, j_p, s_p) << ")");
            T sum = 0;
            const auto& cluster = application->r->native_indices[r_p->level_index()]->native_cluster;
            if (!cluster) {
                debug(r_p->name << " not in mapping");
                return std::numeric_limits<T>::quiet_NaN();
            }
            if (cluster->size() > 0) {
                debug("sum over native cluster " << *cluster);
                for (const auto& k : *cluster) {
                    I from = table_indices(*i_p, k->index);
                    I to = table_indices(j_p, s_p);
                    if (from != IndexSet<I>::NOT_GIVEN && to != IndexSet<I>::NOT_GIVEN) {
                        sum += table(from, to);
                    } else {
                        // need to sum, but regions involved do not have corresponding sectors -> assume region does not have sector -> 0
                        debug("cannot use " << debugp(i_p, k, j_p, s_p));
                    }
                }
                proxy_value *= table(i_p, r_p, j_p, s_p) / sum;
            } else {
                debug("mapped x to 1 -> no summing necessary");
            }
        }
        if (application->j != nullptr && application->j->mapped) {
            debug("j has mapping -> calc flow mapping share (" << debugp(i_p, r_p, j_p, s_p) << ")");
            T sum = 0;
            const auto& cluster = application->j->native_indices[j_p->level_index()]->native_cluster;
            if (!cluster) {
                debug(j_p->name << " not in mapping");
                return std::numeric_limits<T>::quiet_NaN();
            }
            if (cluster->size() > 0) {
                debug("sum over native cluster " << *cluster);
                for (const auto& k : *cluster) {
                    I from = table_indices(i_p, r_p);
                    I to = table_indices(k->index, *s_p);
                    if (from != IndexSet<I>::NOT_GIVEN && to != IndexSet<I>::NOT_GIVEN) {
                        sum += table(from, to);
                    } else {
                        // need to sum, but regions involved do not have corresponding sectors -> assume region does not have sector -> 0
                        debug("cannot use " << debugp(i_p, r_p, k, s_p));
                    }
                }
                proxy_value *= table(i_p, r_p, j_p, s_p) / sum;
            } else {
                debug("mapped x to 1 -> no summing necessary");
            }
        }
        if (application->s != nullptr && application->s->mapped) {
            debug("s has mapping -> calc flow mapping share (" << debugp(i_p, r_p, j_p, s_p) << ")");
            T sum = 0;
            const auto& cluster = application->s->native_indices[s_p->level_index()]->native_cluster;
            if (!cluster) {
                debug(s_p->name << " not in mapping");
                return std::numeric_limits<T>::quiet_NaN();
            }
            if (cluster->size() > 0) {
                debug("sum over native cluster " << *cluster);
                for (const auto& k : *cluster) {
                    I from = table_indices(i_p, r_p);
                    I to = table_indices(*j_p, k->index);
                    if (from != IndexSet<I>::NOT_GIVEN && to != IndexSet<I>::NOT_GIVEN) {
                        sum += table(from, to);
                    } else {
                        // need to sum, but regions involved do not have corresponding sectors -> assume region does not have sector -> 0
                        debug("cannot use " << debugp(i_p, r_p, j_p, k));
                    }
                }
                proxy_value *= table(i_p, r_p, j_p, s_p) / sum;
            } else {
                debug("mapped x to 1 -> no summing necessary");
            }
        }
    }
    return proxy_value;
}

template<typename T, typename I>
inline T ProxyData<T, I>::Application::get_flow(
    const Table<T, I>& table, const Sector<I>* i_p, const Region<I>* r_p, const Sector<I>* j_p, const Region<I>* s_p) const {
    const auto i_e = (i == nullptr) ? i_p : (i->sub ? i_p->parent() : i_p);
    const auto r_e = (r == nullptr) ? r_p : (r->sub ? r_p->parent() : r_p);
    const auto j_e = (j == nullptr) ? j_p : (j->sub ? j_p->parent() : j_p);
    const auto s_e = (s == nullptr) ? s_p : (s->sub ? s_p->parent() : s_p);
    const auto flow = table.sum(i_e, r_e, j_e, s_e);
    debug("flow " << debugp(i_e, r_e, j_e, s_e) << " = " << flow);
    return flow;
}

template<typename T, typename I>
inline T ProxyData<T, I>::Application::get_flow_share_denominator(
    const Table<T, I>& table, const Sector<I>* i_p, const Region<I>* r_p, const Sector<I>* j_p, const Region<I>* s_p) const {
    const auto i_d = (i == nullptr) ? nullptr : i_p->super();
    const auto r_d = (r == nullptr) ? nullptr : r_p->super();
    const auto j_d = (j == nullptr) ? nullptr : j_p->super();
    const auto s_d = (s == nullptr) ? nullptr : s_p->super();
    const auto denominator = table.sum(i_d, r_d, j_d, s_d);
    debug("denominator " << debugp(i_d, r_d, j_d, s_d) << " = " << denominator);
    return denominator;
}

template<typename T, typename I>
ProxyData<T, I>::Application::Application(Application* application1, Application* application2) {
    if (application1->i == application2->i) {
        i = application1->i;
    } else {
        if (application1->i == nullptr) {
            i = application2->i;
        } else if (application2->i == nullptr) {
            i = application1->i;
        } else {
            throw std::runtime_error("Applications cannot be combined");
        }
    }
    if (application1->r == application2->r) {
        r = application1->r;
    } else {
        if (application1->r == nullptr) {
            r = application2->r;
        } else if (application2->r == nullptr) {
            r = application1->r;
        } else {
            throw std::runtime_error("Applications cannot be combined");
        }
    }
    if (application1->j == application2->j) {
        j = application1->j;
    } else {
        if (application1->j == nullptr) {
            j = application2->j;
        } else if (application2->j == nullptr) {
            j = application1->j;
        } else {
            throw std::runtime_error("Applications cannot be combined");
        }
    }
    if (application1->s == application2->s) {
        s = application1->s;
    } else {
        if (application1->s == nullptr) {
            s = application2->s;
        } else if (application2->s == nullptr) {
            s = application1->s;
        } else {
            throw std::runtime_error("Applications cannot be combined");
        }
    }
}

template<typename T, typename I>
void ProxyData<T, I>::approximate(
    const std::vector<FullIndex<I>>& full_indices, Table<T, I>& table, Table<std::size_t, I> quality, const Table<T, I>& last_table, std::size_t d) const {
#ifdef LIBMRIO_SHOW_PROGRESS
    progressbar::ProgressBar bar(full_indices.size(), "    Approximation");
#endif
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t k = 0; k < full_indices.size(); ++k) {
        const auto& full_index = full_indices[k];
        const auto i_p = full_index.i;
        const auto r_p = full_index.r;
        const auto j_p = full_index.j;
        const auto s_p = full_index.s;

        Application* application1 = nullptr;
        Application* application2 = nullptr;
        for (const auto& app : applications) {
            if (app->applies_to(i_p, r_p, j_p, s_p)) {
                if (application1 == nullptr) {
                    application1 = app.get();
                } else if (application2 == nullptr) {
                    application2 = app.get();
                } else {
                    throw std::runtime_error("More than two applications apply to " + i_p->name + ":" + r_p->name + "->" + j_p->name + ":" + s_p->name);
                }
            }
        }

        if (application1 != nullptr) {
            if (application2 == nullptr) {
                const auto denominator = application1->get_flow_share_denominator(last_table, i_p, r_p, j_p, s_p);
                if (denominator > 0 && !std::isnan(denominator)) {
                    for_all_sub<T, I>(i_p, r_p, j_p, s_p, [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
                        const auto share = get_mapped_value(application1, last_table, i, r, j, s) / denominator;
                        if (!std::isnan(share)) {
                            const auto value = application1->get_flow(last_table, i, r, j, s) * share;
                            if (!std::isnan(value)) {
                                assert(value >= 0);
                                table(i, r, j, s) = value;
                                quality(i, r, j, s) = d;
                            }
                        }
                    });
                }
            } else {
                const auto denominator1 = application1->get_flow_share_denominator(last_table, i_p, r_p, j_p, s_p);
                if (denominator1 > 0 && !std::isnan(denominator1)) {
                    const auto denominator2 = application2->get_flow_share_denominator(last_table, i_p, r_p, j_p, s_p);
                    if (denominator2 > 0 && !std::isnan(denominator2)) {
                        Application application_combo{application1, application2};

                        for_all_sub<T, I>(i_p, r_p, j_p, s_p, [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
                            T value;
                            auto share1 = get_mapped_value(application1, last_table, i, r, j, s) / denominator1;
                            auto share2 = get_mapped_value(application2, last_table, i, r, j, s) / denominator2;
                            if (std::isnan(share1)) {
                                if (std::isnan(share2)) {
                                    return;
                                }
                                value = application2->get_flow(last_table, i, r, j, s) * share2;
                            } else if (std::isnan(share2)) {
                                value = application1->get_flow(last_table, i, r, j, s) * share1;
                            } else {
                                value = application_combo.get_flow(last_table, i, r, j, s) * share1 * share2;
                            }
                            if (!std::isnan(value)) {
                                assert(value >= 0);
                                table(i, r, j, s) = value;
                                quality(i, r, j, s) = d;
                            }
                        });
                    }
                }
            }
        }
#ifdef LIBMRIO_SHOW_PROGRESS
        ++bar;
#endif
    }
}

template<typename T, typename I>
void ProxyData<T, I>::adjust(
    const std::vector<FullIndex<I>>& full_indices, Table<T, I>& table, Table<std::size_t, I> quality, const Table<T, I>& basetable, std::size_t d) const {
#ifdef LIBMRIO_SHOW_PROGRESS
    progressbar::ProgressBar bar(full_indices.size(), "    Adjustment");
#endif
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t k = 0; k < full_indices.size(); ++k) {
        const auto& full_index = full_indices[k];
        const T& base = basetable.base(full_index.i, full_index.r, full_index.j, full_index.s);
        if (base > 0) {
            T sum_of_exact = 0;
            T sum_of_non_exact = 0;
            for_all_sub<T, I>(full_index.i, full_index.r, full_index.j, full_index.s,
                              [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
                                  if (quality(i, r, j, s) == d) {
                                      sum_of_exact += table(i, r, j, s);
                                  } else {
                                      sum_of_non_exact += table(i, r, j, s);
                                  }
                              });
            assert(sum_of_exact > 0 || sum_of_non_exact > 0);
            T correction_factor = base / (sum_of_exact + sum_of_non_exact);
            if (base > sum_of_exact && sum_of_non_exact > 0) {
                for_all_sub<T, I>(full_index.i, full_index.r, full_index.j, full_index.s,
                                  [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
                                      if (quality(i, r, j, s) != d) {
                                          table(i, r, j, s) = (base - sum_of_exact) * table(i, r, j, s) / sum_of_non_exact;
                                      }
                                  });
            } else if (correction_factor < 1 || correction_factor > 1) {
                for_all_sub<T, I>(full_index.i, full_index.r, full_index.j, full_index.s,
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

template class ProxyData<double, std::size_t>;
template class ProxyData<float, std::size_t>;

}  // namespace mrio
