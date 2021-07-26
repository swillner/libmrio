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

#include "MRIOTable.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#ifdef LIBMRIO_WITH_NETCDF
#include <ncDim.h>
#include <ncFile.h>
#include <ncType.h>
#include <ncVar.h>
#endif
#include "csv-parser.h"

#ifdef LIBMRIO_VERBOSE
#include <cmath>
#include <iomanip>
#include <iostream>
#endif

namespace mrio {

#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

template<typename T, typename I>
T Table<T, I>::sum(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
    T res = 0;
    build_sum_source<false>(res, i, r, j, s);
    return res;
}

template<typename T, typename I>
T Table<T, I>::basesum(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
    T res = 0;
    build_sum_source<true>(res, i, r, j, s);
    return res;
}

template<typename T, typename I>
template<bool use_base>
void Table<T, I>::build_sum_source(T& res, const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
    if (unlikely(i == nullptr)) {
        if (unlikely(r == nullptr)) {
            build_sum_target<use_base>(res, index_set_.supersectors(), true, j, s);
        } else {
            build_sum_target<use_base>(res, index_set_.supersectors(), r, j, s);
        }
    } else if (unlikely(i->has_sub())) {
        if (unlikely(r == nullptr)) {
            build_sum_target<use_base>(res, i->sub(), i->super()->regions(), j, s);
        } else if (unlikely(r->has_sub())) {
            build_sum_target<use_base>(res, i->sub(), r->sub(), j, s);
        } else {
            build_sum_target<use_base>(res, i->sub(), r, j, s);
        }
    } else {
        if (unlikely(r == nullptr)) {
            build_sum_target<use_base>(res, i, i->super()->regions(), j, s);
        } else if (unlikely(r->has_sub())) {
            build_sum_target<use_base>(res, i, r->sub(), j, s);
        } else {
            build_sum_target<use_base>(res, i, r, j, s);
        }
    }
}

template<typename T, typename I>
template<bool use_base, typename Arg_i, typename Arg_r>
void Table<T, I>::build_sum_target(T& res, Arg_i&& i, Arg_r&& r, const Sector<I>* j, const Region<I>* s) const noexcept {
    if (unlikely(j == nullptr)) {
        if (unlikely(s == nullptr)) {
            add_sum<use_base, 0>(res, std::forward<Arg_i>(i), std::forward<Arg_r>(r), index_set_.supersectors(), true);
        } else {
            add_sum<use_base, 0>(res, std::forward<Arg_i>(i), std::forward<Arg_r>(r), index_set_.supersectors(), s);
        }
    } else if (unlikely(j->has_sub())) {
        if (unlikely(s == nullptr)) {
            add_sum<use_base, 0>(res, std::forward<Arg_i>(i), std::forward<Arg_r>(r), j->sub(), j->super()->regions());
        } else if (unlikely(s->has_sub())) {
            add_sum<use_base, 0>(res, std::forward<Arg_i>(i), std::forward<Arg_r>(r), j->sub(), s->sub());
        } else {
            add_sum<use_base, 0>(res, std::forward<Arg_i>(i), std::forward<Arg_r>(r), j->sub(), s);
        }
    } else {
        if (unlikely(s == nullptr)) {
            add_sum<use_base, 0>(res, std::forward<Arg_i>(i), std::forward<Arg_r>(r), j, j->super()->regions());
        } else if (unlikely(s->has_sub())) {
            add_sum<use_base, 0>(res, std::forward<Arg_i>(i), std::forward<Arg_r>(r), j, s->sub());
        } else {
            add_sum<use_base, 0>(res, std::forward<Arg_i>(i), std::forward<Arg_r>(r), j, s);
        }
    }
}

template<typename T, typename I>
template<bool use_base, int c, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
void Table<T, I>::add_sum(T& res, const std::vector<Arg1*>& arg1, Arg2&& arg2, Arg3&& arg3, Arg4&& arg4) const noexcept {
    static_assert(c < 4);
    for (const auto k : arg1) {
        add_sum<use_base, c + 1>(res, std::forward<Arg2>(arg2), std::forward<Arg3>(arg3), std::forward<Arg4>(arg4), k);
    }
}

template<typename T, typename I>
template<bool use_base, int c, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
void Table<T, I>::add_sum(T& res, const std::vector<std::unique_ptr<Arg1>>& arg1, Arg2&& arg2, Arg3&& arg3, Arg4&& arg4) const noexcept {
    static_assert(c < 4);
    for (const auto& k : arg1) {
        add_sum<use_base, c + 1>(res, std::forward<Arg2>(arg2), std::forward<Arg3>(arg3), std::forward<Arg4>(arg4), k.get());
    }
}

template<typename T, typename I>
template<bool use_base, int c, typename Arg3, typename Arg4>
void Table<T, I>::add_sum(T& res, const std::vector<std::unique_ptr<Sector<I>>>& arg1, bool, Arg3&& arg3, Arg4&& arg4) const noexcept {
    static_assert(c < 4);
    for (const auto& sec : arg1) {
        for (const auto& reg : sec->super()->regions()) {
            add_sum<use_base, c + 2>(res, std::forward<Arg3>(arg3), std::forward<Arg4>(arg4), sec.get(), reg);
        }
    }
}

template<typename T, typename I>
template<bool use_base, int c, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
void Table<T, I>::add_sum(T& res, const Arg1* arg1, Arg2&& arg2, Arg3&& arg3, Arg4&& arg4) const noexcept {
    if constexpr (c < 4) {
        add_sum<use_base, c + 1>(res, std::forward<Arg2>(arg2), std::forward<Arg3>(arg3), std::forward<Arg4>(arg4), arg1);
    } else {
        if constexpr (use_base) {
            res += base(arg1, arg2, arg3, arg4);
        } else {
            res += (*this)(arg1, arg2, arg3, arg4);
        }
    }
}

template<typename T, typename I>
void Table<T, I>::read_indices_from_csv(std::istream& indicesstream) {
    try {
        csv::Parser parser(indicesstream);
        do {
            const std::tuple<std::string, std::string> c = parser.read<std::string, std::string>();
            index_set_.add_index(std::get<1>(c), std::get<0>(c));
        } while (parser.next_row());
        index_set_.rebuild_indices();
    } catch (const csv::parser_exception& ex) {
        throw std::runtime_error(ex.format());
    }
}

template<typename T, typename I>
void Table<T, I>::read_data_from_csv(std::istream& datastream, const T& threshold) {
    I l = 0;
    try {
        csv::Parser parser(datastream);
        auto d = data.begin();
        for (l = 0; l < index_set_.size(); l++) {
            if (l == std::numeric_limits<I>::max()) {
                throw std::runtime_error("Too many rows");
            }
            for (I c = 0; c < index_set_.size(); c++) {
                if (c == std::numeric_limits<I>::max()) {
                    throw std::runtime_error("Too many columns");
                }
                T flow = parser.read<T>();
                if (flow > threshold) {
                    *d = flow;
                }
                d++;
                parser.next_col();
            }
            parser.next_row();
        }
    } catch (const csv::parser_exception& ex) {
        throw std::runtime_error(ex.format());
    }
}

template<typename T, typename I>
void Table<T, I>::read_from_csv(std::istream& indicesstream, std::istream& datastream, const T& threshold) {
    read_indices_from_csv(indicesstream);
    data.resize(index_set_.size() * index_set_.size(), 0);
    read_data_from_csv(datastream, threshold);
}

template<typename T, typename I>
void Table<T, I>::write_to_csv(std::ostream& indicesstream, std::ostream& datastream) const {
    debug_out();
    for (const auto& row : index_set_.total_indices) {
        for (const auto& col : index_set_.total_indices) {
            datastream << at(row.index, col.index) << ",";
        }
        datastream.seekp(-1, std::ios_base::end);
        datastream << '\n';
        indicesstream << row.sector->name << "," << row.region->name << '\n';
    }
    datastream << std::flush;
    indicesstream << std::flush;
}

#ifdef LIBMRIO_WITH_NETCDF
template<typename T, typename I>
void Table<T, I>::read_from_netcdf(const std::string& filename, const T& threshold) {
    netCDF::NcFile file(filename, netCDF::NcFile::read);

    std::size_t sectors_count = file.getDim("sector").getSize();
    {
        netCDF::NcVar sectors_var = file.getVar("sector");
        std::vector<const char*> sectors_val(sectors_count);
        sectors_var.getVar(&sectors_val[0]);
        for (const auto& sector : sectors_val) {
            index_set_.add_sector(sector);
        }
    }

    std::size_t regions_count = file.getDim("region").getSize();
    {
        netCDF::NcVar regions_var = file.getVar("region");
        std::vector<const char*> regions_val(regions_count);
        regions_var.getVar(&regions_val[0]);
        for (const auto& region : regions_val) {
            index_set_.add_region(region);
        }
    }

    netCDF::NcDim index_dim = file.getDim("index");
    if (index_dim.isNull()) {
        data.resize(regions_count * sectors_count * regions_count * sectors_count);
        if (file.getVar("flows").getDims()[0].getName() == "sector") {
            std::vector<T> data_l(regions_count * sectors_count * regions_count * sectors_count);
            file.getVar("flows").getVar(&data_l[0]);
            auto d = data.begin();
            for (const auto& region_from : index_set_.superregions()) {
                for (const auto& sector_from : index_set_.supersectors()) {
                    index_set_.add_index(sector_from.get(), region_from.get());
                    for (const auto& region_to : index_set_.superregions()) {
                        for (const auto& sector_to : index_set_.supersectors()) {
                            const T& d_l = data_l[((*sector_from * regions_count + *region_from) * sectors_count + *sector_to) * regions_count + *region_to];
                            if (d_l > threshold) {
                                *d = d_l;
                            } else {
                                *d = 0;
                            }
                            d++;
                        }
                    }
                }
            }
        } else {
            for (const auto& sector : index_set_.supersectors()) {
                for (const auto& region : index_set_.superregions()) {
                    index_set_.add_index(sector.get(), region.get());
                }
            }
            file.getVar("flows").getVar(&data[0]);
            for (auto& d : data) {
                if (d <= threshold) {
                    d = 0;
                }
            }
        }
    } else {
        std::size_t index_size = index_dim.getSize();
        netCDF::NcVar index_sector_var = file.getVar("index_sector");
        std::vector<std::uint32_t> index_sector_val(index_size);
        index_sector_var.getVar(&index_sector_val[0]);
        netCDF::NcVar index_region_var = file.getVar("index_region");
        std::vector<std::uint32_t> index_region_val(index_size);
        index_region_var.getVar(&index_region_val[0]);
        for (unsigned int i = 0; i < index_size; ++i) {
            index_set_.add_index(index_set_.supersectors()[index_sector_val[i]].get(), index_set_.superregions()[index_region_val[i]].get());
        }
        data.resize(index_size * index_size);
        file.getVar("flows").getVar(&data[0]);
        for (auto& d : data) {
            if (d <= threshold) {
                d = 0;
            }
        }
    }
    index_set_.rebuild_indices();
}
#endif

#ifdef LIBMRIO_WITH_NETCDF
template<typename T, typename I>
void Table<T, I>::write_to_netcdf(const std::string& filename) const {
    debug_out();
    netCDF::NcFile file(filename, netCDF::NcFile::replace, netCDF::NcFile::nc4);

    netCDF::NcDim sectors_dim = file.addDim("sector", index_set_.total_sectors_count());
    {
        netCDF::NcVar sectors_var = file.addVar("sector", netCDF::NcType::nc_STRING, {sectors_dim});
        I i = 0;
        for (const auto& sector : index_set_.supersectors()) {
            if (sector->has_sub()) {
                for (const auto& subsector : sector->sub()) {
                    sectors_var.putVar({i}, subsector->name);
                    ++i;
                }
            } else {
                sectors_var.putVar({i}, sector->name);
                ++i;
            }
        }
    }

    netCDF::NcDim regions_dim = file.addDim("region", index_set_.total_regions_count());
    {
        netCDF::NcVar regions_var = file.addVar("region", netCDF::NcType::nc_STRING, {regions_dim});
        I i = 0;
        for (const auto& region : index_set_.superregions()) {
            if (region->has_sub()) {
                for (const auto& subregion : region->sub()) {
                    regions_var.putVar({i}, subregion->name);
                    ++i;
                }
            } else {
                regions_var.putVar({i}, region->name);
                ++i;
            }
        }
    }

    netCDF::NcDim index_dim = file.addDim("index", index_set_.size());
    {
        netCDF::NcVar index_sector_var = file.addVar("index_sector", netCDF::NcType::nc_UINT, {index_dim});
        netCDF::NcVar index_region_var = file.addVar("index_region", netCDF::NcType::nc_UINT, {index_dim});
        for (const auto& index : index_set_.total_indices) {
            index_sector_var.putVar({index.index}, static_cast<unsigned int>(index.sector->total_index()));
            index_region_var.putVar({index.index}, static_cast<unsigned int>(index.region->total_index()));
        }
    }
    netCDF::NcVar flows_var = file.addVar("flows", netCDF::NcType::nc_FLOAT, {index_dim, index_dim});
    flows_var.setCompression(false, true, 7);
    flows_var.setFill<T>(true, std::numeric_limits<T>::quiet_NaN());
    flows_var.putVar(&data[0]);
}
#endif

template<typename T, typename I>
void Table<T, I>::insert_sector_offset(const Sector<I>* i, const I& i_regions_count, const I& subsectors_count) noexcept {
    auto region = i->regions().rbegin();
    if (region != i->regions().rend()) {
        auto subregion = (*region)->sub().rbegin();
        I next;
        if (subregion == (*region)->sub().rend()) {
            next = index_set_(i, *region);
        } else {
            next = index_set_(i, *subregion);
        }
        I new_size = index_set_.size() + i_regions_count * (subsectors_count - 1);
        I y_offset = new_size;
        for (I y = index_set_.size(); y-- > 0;) {
            if (y == next) {
                y_offset -= subsectors_count;
                if (subregion != (*region)->sub().rend()) {
                    ++subregion;
                }
                if (subregion == (*region)->sub().rend()) {
                    ++region;
                    if (region == i->regions().rend()) {
                        next = IndexSet<I>::NOT_GIVEN;
                    } else {
                        subregion = (*region)->sub().rbegin();
                        if (subregion == (*region)->sub().rend()) {
                            next = index_set_(i, *region);
                        } else {
                            next = index_set_(i, *subregion);
                        }
                    }
                } else {
                    next = index_set_(i, *subregion);
                }
                for (I offset = subsectors_count; offset-- > 0;) {
                    insert_sector_offset_row(i, i_regions_count, subsectors_count, y, y_offset + offset, subsectors_count);
                }
            } else {
                --y_offset;
                insert_sector_offset_row(i, i_regions_count, subsectors_count, y, y_offset, 1);
            }
        }
    }
}

template<typename T, typename I>
void Table<T, I>::insert_sector_offset_row(
    const Sector<I>* i, const I& i_regions_count, const I& subsectors_count, const I& y, const I& y_offset, const I& divide_by) noexcept {
    auto region = i->regions().rbegin();
    if (region != i->regions().rend()) {
        auto subregion = (*region)->sub().rbegin();
        I next;
        if (subregion == (*region)->sub().rend()) {
            next = index_set_(i, *region);
        } else {
            next = index_set_(i, *subregion);
        }
        I new_size = index_set_.size() + i_regions_count * (subsectors_count - 1);
        I x_offset = new_size;
        for (I x = index_set_.size(); x-- > 0;) {
            if (x == next) {
                x_offset -= subsectors_count;
                if (subregion != (*region)->sub().rend()) {
                    ++subregion;
                }
                if (subregion == (*region)->sub().rend()) {
                    ++region;
                    if (region == i->regions().rend()) {
                        next = IndexSet<I>::NOT_GIVEN;
                    } else {
                        subregion = (*region)->sub().rbegin();
                        if (subregion == (*region)->sub().rend()) {
                            next = index_set_(i, *region);
                        } else {
                            next = index_set_(i, *subregion);
                        }
                    }
                } else {
                    next = index_set_(i, *subregion);
                }
                for (I offset = subsectors_count; offset-- > 0;) {
                    data[y_offset * new_size + x_offset + offset] = data[y * index_set_.size() + x] / subsectors_count / divide_by;
                }
            } else {
                --x_offset;
                data[y_offset * new_size + x_offset] = data[y * index_set_.size() + x] / divide_by;
            }
        }
    }
}

template<typename T, typename I>
void Table<T, I>::insert_region_offset(const Region<I>* r, const I& r_sectors_count, const I& subregions_count) noexcept {
    auto last_sector = r->sectors().rbegin();
    if (last_sector != r->sectors().rend()) {
        auto last_subsector = (*last_sector)->sub().rbegin();
        I last_index;
        if (last_subsector == (*last_sector)->sub().rend()) {
            last_index = index_set_(*last_sector, r);
        } else {
            last_index = index_set_(*last_subsector, r);
        }
        auto first_sector = r->sectors().begin();
        auto first_subsector = (*first_sector)->sub().begin();
        I first_index;
        if (first_subsector == (*first_sector)->sub().end()) {
            first_index = index_set_(*first_sector, r);
        } else {
            first_index = index_set_(*first_subsector, r);
        }
        I new_size = index_set_.size() + r_sectors_count * (subregions_count - 1);
        for (I y = index_set_.size(); y-- > last_index + 1;) {
            insert_region_offset_row(r, r_sectors_count, subregions_count, y, new_size + y - index_set_.size(), 1, first_index, last_index);
        }
        for (I y = last_index + 1; y-- > first_index;) {
            for (I offset = subregions_count; offset-- > 0;) {
                insert_region_offset_row(r, r_sectors_count, subregions_count, y, y + offset * r_sectors_count, subregions_count, first_index, last_index);
            }
        }
        for (I y = first_index; y-- > 0;) {
            insert_region_offset_row(r, r_sectors_count, subregions_count, y, y, 1, first_index, last_index);
        }
    }
}

template<typename T, typename I>
void Table<T, I>::insert_region_offset_row(const Region<I>* r,
                                           const I& r_sectors_count,
                                           const I& subregions_count,
                                           const I& y,
                                           const I& y_offset,
                                           const I& divide_by,
                                           const I& first_index,
                                           const I& last_index) noexcept {
    (void)(r);
    I new_size = index_set_.size() + r_sectors_count * (subregions_count - 1);
    for (I x = index_set_.size(); x-- > last_index + 1;) {
        data[y_offset * new_size + new_size + x - index_set_.size()] = data[y * index_set_.size() + x] / divide_by;
    }
    for (I x = last_index + 1; x-- > first_index;) {
        for (I offset = subregions_count; offset-- > 0;) {
            data[y_offset * new_size + x + offset * r_sectors_count] = data[y * index_set_.size() + x] / subregions_count / divide_by;
        }
    }
    for (I x = first_index; x-- > 0;) {
        data[y_offset * new_size + x] = data[y * index_set_.size() + x] / divide_by;
    }
}

template<typename T, typename I>
void Table<T, I>::debug_out() const {
#ifdef LIBMRIO_VERY_VERBOSE
    std::cout << "\n====\n";
    std::cout << std::setprecision(3) << std::fixed;
    for (const auto& from : index_set_.total_indices) {
        std::cout << index_set_.at(from.sector, from.region) << " " << from.sector->name << " "
                  << (!from.sector->parent() ? "     " : from.sector->parent()->name) << " " << (from.sector->parent() ? *from.sector->parent() : *from.sector)
                  << " " << (*from.sector) << " " << from.sector->level_index() << " " << from.region->name << " "
                  << (!from.region->parent() ? "     " : from.region->parent()->name) << " " << (from.region->parent() ? *from.region->parent() : *from.region)
                  << " " << (*from.region) << " " << from.region->level_index() << "  |  ";
        for (const auto& to : index_set_.total_indices) {
            if (std::isnan(data[from.index * index_set_.size() + to.index])) {
                std::cout << " .   ";
            } else {
                std::cout << data[from.index * index_set_.size() + to.index];
            }
            std::cout << " ";
        }
        std::cout << "\n";
    }
    std::cout << "====\n";
#endif
}

template<typename T, typename I>
void Table<T, I>::insert_subsectors(const std::string& name, const std::vector<std::string>& subsectors) {
    const Sector<I>* sector = index_set_.sector(name);
    const Sector<I>* i = sector->as_super();
    if (!i) {
        throw std::runtime_error("'" + name + "' is a subsector");
    }
    if (i->has_sub()) {
        throw std::runtime_error("'" + name + "' already has subsectors");
    }
    I i_regions_count = 0;
    for (const auto& region : i->regions()) {
        if (region->has_sub()) {
            i_regions_count += region->sub().size();
        } else {
            i_regions_count++;
        }
    }
    debug_out();
    data.resize((index_set_.size() + i_regions_count * (subsectors.size() - 1)) * (index_set_.size() + i_regions_count * (subsectors.size() - 1)));
    // blowup table accordingly
    // and alter values in table (equal distribution)
    insert_sector_offset(i, i_regions_count, subsectors.size());
    // alter indices
    index_set_.insert_subsectors(name, subsectors);
    debug_out();
}

template<typename T, typename I>
void Table<T, I>::insert_subregions(const std::string& name, const std::vector<std::string>& subregions) {
    const Region<I>* region = index_set_.region(name);
    const Region<I>* r = region->as_super();
    if (!r) {
        throw std::runtime_error("'" + name + "' is a subregion");
    }
    if (r->has_sub()) {
        throw std::runtime_error("'" + name + "' already has subregions");
    }
    I r_sectors_count = 0;
    for (const auto& sector : r->sectors()) {
        if (sector->has_sub()) {
            r_sectors_count += sector->sub().size();
        } else {
            r_sectors_count++;
        }
    }
    debug_out();
    data.resize((index_set_.size() + r_sectors_count * (subregions.size() - 1)) * (index_set_.size() + r_sectors_count * (subregions.size() - 1)));
    // blowup table accordingly
    // and alter values in table (equal distribution)
    insert_region_offset(r, r_sectors_count, subregions.size());
    // alter indices
    index_set_.insert_subregions(name, subregions);
    debug_out();
}

template class Table<float, std::size_t>;
template class Table<double, std::size_t>;
template class Table<int, std::size_t>;

}  // namespace mrio
