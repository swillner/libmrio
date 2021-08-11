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

#ifndef LIBMRIO_PROXYDATA_H
#define LIBMRIO_PROXYDATA_H

#ifdef LIBMRIO_VERBOSE
#include <iostream>
#endif
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace csv {
class Parser;
}

namespace settings {
class SettingsNode;
}

namespace mrio {

template<typename I>
class IndexSet;
template<typename I>
class Region;
template<typename I>
class Sector;
template<typename T, typename I>
class Table;

template<typename I>
struct FullIndex {
    const Sector<I>* i;
    const Region<I>* r;
    const Sector<I>* j;
    const Region<I>* s;
};

template<typename T, typename I, typename Func>
inline void do_for_all_sub(Func func, const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
    func(i, r, j, s);
}

template<typename T, typename I, typename Inner, typename... Arguments>
inline void do_for_all_sub(const Inner* k, const Arguments&... params) {
    do_for_all_sub<T, I>(params..., k);
}

template<typename T, typename I, typename Inner, typename... Arguments>
inline void do_for_all_sub(const std::vector<Inner*>& vec, const Arguments&... params) {
    for (const auto k : vec) {
        do_for_all_sub<T, I>(params..., k);
    }
}

template<typename T, typename I, typename Func, typename... Arguments>
inline typename std::enable_if<!(std::is_same<Func, const Region<I>*>::value || std::is_same<Func, const Sector<I>*>::value), void>::type for_all_sub(
    Func func, const Arguments&... params) {
    do_for_all_sub<T, I>(params..., func);
}

template<typename T, typename I, typename... Arguments>
inline void for_all_sub(const Region<I>* r, const Arguments&... params) {
    if (r->has_sub()) {
        for_all_sub<T, I>(params..., r->as_super()->sub());
    } else {
        for_all_sub<T, I>(params..., r);
    }
}

template<typename T, typename I, typename... Arguments>
inline void for_all_sub(const Sector<I>* i, const Arguments&... params) {
    if (i->has_sub()) {
        for_all_sub<T, I>(params..., i->as_super()->sub());
    } else {
        for_all_sub<T, I>(params..., i);
    }
}

template<typename T, typename I>
class ProxyData {
  protected:
    struct MappingIndexPart {
        const I index;
        std::unordered_set<MappingIndexPart*> mapped_to;
        std::shared_ptr<std::unordered_set<MappingIndexPart*>> native_cluster;
        std::shared_ptr<std::unordered_set<MappingIndexPart*>> foreign_cluster;
#ifdef LIBMRIO_VERBOSE
        const std::string name;
        explicit MappingIndexPart(I index_p, std::string name_p) : index(index_p), name(std::move(name_p)) {}
        explicit MappingIndexPart(const Sector<I>* sector) : index(*sector), name(sector->name) {}
        explicit MappingIndexPart(const Region<I>* region) : index(*region), name(region->name) {}
        friend std::ostream& operator<<(std::ostream& os, const MappingIndexPart& m) { return os << m.name; }
        friend std::ostream& operator<<(std::ostream& os, const std::unordered_set<MappingIndexPart*>& m) {
            for (const auto& i : m) {
                os << *i << " ";
            }
            return os << "(" << m.size() << ")";
        }
#else
        explicit MappingIndexPart(I index_p) : index(index_p) {}
        explicit MappingIndexPart(const Sector<I>* sector) : index(*sector) {}
        explicit MappingIndexPart(const Region<I>* region) : index(*region) {}
#endif
    };
    struct ProxyIndex {
        enum class Type { SECTOR, SUBSECTOR, REGION, SUBREGION };
        ProxyIndex(bool mapped_p, Type type_p) : mapped(mapped_p), type(type_p), sub(type_p == Type::SUBSECTOR || type_p == Type::SUBREGION) {}
        const bool mapped;
        const bool sub;
        const Type type;
        std::size_t size;
        std::size_t stride;
        std::vector<std::unique_ptr<MappingIndexPart>> native_indices;
        std::vector<std::unique_ptr<MappingIndexPart>> foreign_indices;
        std::unordered_map<std::string, MappingIndexPart*> foreign_indices_map;
    };
    struct Column {
        enum class Type { SELECT, IGNORE, VALUE, INDEX };
        Type type;
        std::string value;
        ProxyIndex* index;
    };
    struct Application {
        ProxyIndex* i = nullptr;
        ProxyIndex* r = nullptr;
        ProxyIndex* j = nullptr;
        ProxyIndex* s = nullptr;
        Application() = default;
        Application(Application* application1, Application* application2);
        Application(ProxyIndex* i_p, ProxyIndex* r_p, ProxyIndex* j_p, ProxyIndex* s_p) : i(i_p), r(r_p), j(j_p), s(s_p) {}
        inline T get_flow(const Table<T, I>& table, const Sector<I>* i_p, const Region<I>* r_p, const Sector<I>* j_p, const Region<I>* s_p) const;
        inline T get_flow_share_denominator(
            const Table<T, I>& table, const Sector<I>* i_p, const Region<I>* r_p, const Sector<I>* j_p, const Region<I>* s_p) const;
        inline bool applies_to(const Sector<I>* i_p, const Region<I>* r_p, const Sector<I>* j_p, const Region<I>* s_p) const {
            return (i == nullptr || i->sub == i_p->has_sub()) && (r == nullptr || r->sub == r_p->has_sub()) && (j == nullptr || j->sub == j_p->has_sub())
                   && (s == nullptr || s->sub == s_p->has_sub());
        }
#ifdef LIBMRIO_VERBOSE
        friend std::ostream& operator<<(std::ostream& os, const Application& a) {
            return os << (a.i == nullptr ? "" : "i") << (a.r == nullptr ? "" : "r") << (a.j == nullptr ? "" : "j") << (a.s == nullptr ? "" : "s");
        }
#endif
    };

    std::vector<T> data;
    std::vector<std::unique_ptr<ProxyIndex>> indices;
    std::vector<std::unique_ptr<Application>> applications;
    const IndexSet<I> table_indices;

    static void set_clusters_for_native(MappingIndexPart* native_index_part);
    static void set_clusters_for_foreign(MappingIndexPart* foreign_index_part);
    T get_mapped_value(
        const Application* application, const Table<T, I>& table, const Sector<I>* i_p, const Region<I>* r_p, const Sector<I>* j_p, const Region<I>* s_p) const;
    inline T sum_proxy_over_all_foreign_clusters_helper(I index,
                                                        I level_index,
                                                        const ProxyIndex* proxy_index,
                                                        const ProxyIndex* i,
                                                        const ProxyIndex* r,
                                                        const ProxyIndex* j,
                                                        const ProxyIndex* s,
                                                        const Sector<I>* i_p,
                                                        const Region<I>* r_p,
                                                        const Sector<I>* j_p,
                                                        const Region<I>* s_p) const;
    inline T sum_proxy_over_all_foreign_clusters(I index,
                                                 const ProxyIndex* i,
                                                 const ProxyIndex* r,
                                                 const ProxyIndex* j,
                                                 const ProxyIndex* s,
                                                 const Sector<I>* i_p,
                                                 const Region<I>* r_p,
                                                 const Sector<I>* j_p,
                                                 const Region<I>* s_p) const;
    inline Application* find_application_from(std::size_t& index, const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const;
    void read_mapping_from_file(const settings::SettingsNode& mapping_node, ProxyIndex& index);

  public:
    explicit ProxyData(IndexSet<I> table_indices_p) : table_indices(std::move(table_indices_p)) {}
    void read_from_file(const settings::SettingsNode& settings_node);
    void approximate(
        const std::vector<FullIndex<I>>& full_indices, Table<T, I>& table, Table<std::size_t, I>& quality, const Table<T, I>& last_table, std::size_t d) const;
    void adjust(const std::vector<FullIndex<I>>& full_indices,
                Table<T, I>& table,
                const Table<std::size_t, I>& quality,
                const Table<T, I>& basetable,
                std::size_t d) const;
};
}  // namespace mrio

#endif
