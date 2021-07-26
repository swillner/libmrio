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

#ifndef LIBMRIO_MRIOTABLE_H
#define LIBMRIO_MRIOTABLE_H

#include <iosfwd>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include "MRIOIndexSet.h"
#ifdef DEBUG
#include <cassert>
#else
#undef assert
#define assert(a) \
    {}
#endif

namespace mrio {

template<typename T, typename I>
class Table {
  protected:
    std::vector<T> data;
    IndexSet<I> index_set_;

    void read_indices_from_csv(std::istream& indicesstream);
    void read_data_from_csv(std::istream& datastream, const T& threshold);
    inline void insert_sector_offset(const Sector<I>* i, const I& i_regions_count, const I& subsectors_count) noexcept;
    inline void insert_sector_offset_row(
        const Sector<I>* i, const I& i_regions_count, const I& subsectors_count, const I& y, const I& y_offset, const I& divide_by) noexcept;
    inline void insert_region_offset(const Region<I>* r, const I& r_sectors_count, const I& subregions_count) noexcept;
    inline void insert_region_offset_row(const Region<I>* r,
                                         const I& r_sectors_count,
                                         const I& subregions_count,
                                         const I& y,
                                         const I& y_offset,
                                         const I& divide_by,
                                         const I& first_index,
                                         const I& last_index) noexcept;

    template<bool use_base>
    void build_sum_source(T& res, const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept;
    template<bool use_base, typename Arg_i, typename Arg_r>
    void build_sum_target(T& res, Arg_i&& i, Arg_r&& r, const Sector<I>* j, const Region<I>* s) const noexcept;
    template<bool use_base, int c, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    void add_sum(T& res, const std::vector<Arg1*>& arg1, Arg2&& arg2, Arg3&& arg3, Arg4&& arg4) const noexcept;
    template<bool use_base, int c, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    void add_sum(T& res, const std::vector<std::unique_ptr<Arg1>>& arg1, Arg2&& arg2, Arg3&& arg3, Arg4&& arg4) const noexcept;
    template<bool use_base, int c, typename Arg3, typename Arg4>
    void add_sum(T& res, const std::vector<std::unique_ptr<Sector<I>>>& arg1, bool, Arg3&& arg3, Arg4&& arg4) const noexcept;
    template<bool use_base, int c, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    void add_sum(T& res, const Arg1* arg1, Arg2&& arg2, Arg3&& arg3, Arg4&& arg4) const noexcept;

  public:
    Table() {}
    explicit Table(const IndexSet<I>& index_set_p, const T default_value_p = std::numeric_limits<T>::signaling_NaN()) : index_set_(index_set_p) {
        data.resize(index_set_.size() * index_set_.size(), default_value_p);
    }
    inline const IndexSet<I>& index_set() const { return index_set_; }
    void insert_subsectors(const std::string& name, const std::vector<std::string>& subsectors);
    void insert_subregions(const std::string& name, const std::vector<std::string>& subregions);
    void write_to_csv(std::ostream& indicesstream, std::ostream& datastream) const;
    void write_to_mrio(std::ostream& outstream) const;
#ifdef LIBMRIO_WITH_NETCDF
    void write_to_netcdf(const std::string& filename) const;
#endif
    void read_from_csv(std::istream& indicesstream, std::istream& datastream, const T& threshold);
    void read_from_mrio(std::istream& instream, const T& threshold);
#ifdef LIBMRIO_WITH_NETCDF
    void read_from_netcdf(const std::string& filename, const T& threshold);
#endif

    T sum(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept;
    T basesum(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept;

    inline T& at(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
        assert(index_set_.at(i, r) >= 0);
        assert(index_set_.at(j, s) >= 0);
        return at(index_set_.at(i, r), index_set_.at(j, s));
    }
    inline const T& at(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const {
        assert(index_set_.at(i, r) >= 0);
        assert(index_set_.at(j, s) >= 0);
        return at(index_set_.at(i, r), index_set_.at(j, s));
    }
    inline T& operator()(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) noexcept {
        assert(index_set_.at(i, r) >= 0);
        assert(index_set_.at(j, s) >= 0);
        return (*this)(index_set_(i, r), index_set_(j, s));
    }
    inline const T& operator()(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
        assert(index_set_.at(i, r) >= 0);
        assert(index_set_.at(j, s) >= 0);
        return (*this)(index_set_(i, r), index_set_(j, s));
    }
    inline T& at(const I& from, const I& to) {
        assert(from >= 0);
        assert(to >= 0);
        return data.at(from * index_set_.size() + to);
    }
    inline const T& at(const I& from, const I& to) const {
        assert(from >= 0);
        assert(to >= 0);
        return data.at(from * index_set_.size() + to);
    }
    inline T& operator()(const I& from, const I& to) noexcept {
        assert(from >= 0);
        assert(to >= 0);
        assert(from * index_set_.size() + to < data.size());
        return data[from * index_set_.size() + to];
    }
    inline const T& operator()(const I& from, const I& to) const noexcept {
        assert(from >= 0);
        assert(to >= 0);
        assert(from * index_set_.size() + to < data.size());
        return data[from * index_set_.size() + to];
    }
    /**
     * @brief Returns reference to value with Sectors/Regions of a foreign Table
     *        that is a disaggregated version of this non-disaggregated Table
     *
     * @return Reference to value
     */
    inline T& base(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) noexcept {
        assert(index_set_.base(i, r) >= 0);
        assert(index_set_.base(j, s) >= 0);
        return (*this)(index_set_.base(i, r), index_set_.base(j, s));
    }
    inline const T& base(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
        assert(index_set_.base(i, r) >= 0);
        assert(index_set_.base(j, s) >= 0);
        return (*this)(index_set_.base(i, r), index_set_.base(j, s));
    }
    void replace_table_from(const Table& other) { data = other.data; }
    const std::vector<T>& raw_data() const { return data; }
    void debug_out() const;
};
}  // namespace mrio

#endif
