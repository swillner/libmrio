#include "MRIOTable.h"
#include "MRIOIndexSet.h"
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <ncDim.h>
#include <ncFile.h>
#include <ncVar.h>
#include <netcdf>
#ifdef DEBUG
#include <cassert>
#else
#define assert(a) \
    {}
#endif

using namespace mrio;
using std::streambuf;
using std::runtime_error;
using std::stringstream;
using std::istringstream;
using std::stof;
using std::exception;
using std::endl;
using std::ostream;

template <typename T, typename I>
const T Table<T, I>::sum(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
    T res = 0;
    if (i == nullptr) {
        for (auto& i_ : index_set_.supersectors()) {
            res += sum(i_.get(), r, j, s);
        }
    } else if (i->has_sub()) {
        for (auto& i_ : i->as_super()->sub()) {
            res += sum(i_, r, j, s);
        }
    } else if (r == nullptr) {
        for (auto& r_ : (i->as_super() ? i->as_super() : i->parent())->regions()) {
            res += sum(i, r_, j, s);
        }
    } else if (r->has_sub()) {
        for (auto& r_ : r->as_super()->sub()) {
            res += sum(i, r_, j, s);
        }
    } else if (j == nullptr) {
        for (auto& j_ : index_set_.supersectors()) {
            res += sum(i, r, j_.get(), s);
        }
    } else if (j->has_sub()) {
        for (auto& j_ : j->as_super()->sub()) {
            res += sum(i, r, j_, s);
        }
    } else if (s == nullptr) {
        for (auto& s_ : (j->as_super() ? j->as_super() : j->parent())->regions()) {
            res += sum(i, r, j, s_);
        }
    } else if (s->has_sub()) {
        for (auto& s_ : s->as_super()->sub()) {
            res += sum(i, r, j, s_);
        }
    } else {
        return (*this)(i, r, j, s);
    }
    return res;
}

template <typename T, typename I>
const T Table<T, I>::basesum(const SuperSector<I>* i, const SuperRegion<I>* r, const SuperSector<I>* j, const SuperRegion<I>* s) const noexcept {
    T res = 0;
    if (i == nullptr) {
        for (auto& i_ : index_set_.supersectors()) {
            res += basesum(i_.get(), r, j, s);
        }
    } else if (r == nullptr) {
        for (auto& r_ : i->regions()) {
            res += basesum(i, r_, j, s);
        }
    } else if (j == nullptr) {
        for (auto& j_ : index_set_.supersectors()) {
            res += basesum(i, r, j_.get(), s);
        }
    } else if (s == nullptr) {
        for (auto& s_ : j->regions()) {
            res += basesum(i, r, j, s_);
        }
    } else {
        return this->base(i, r, j, s);
    }
    return res;
}

static inline bool readline(istream& stream, string* vars, unsigned char num_vars) {
    istream::sentry s(stream, true);
    streambuf* buf = stream.rdbuf();
    unsigned char i = 0;
    bool in_quotes = false;
    vars[i].clear();
    for (;;) {
        int c = buf->sbumpc();
        switch (c) {
            case '"':
                in_quotes = !in_quotes;
                break;
            case '\n':
                if (in_quotes || i < num_vars - 1) {
                    throw runtime_error("Unexpected end of line");
                }
                return true;
            case '\r':
                if (buf->sgetc() == '\n') {
                    buf->sbumpc();
                }
                if (in_quotes || i < num_vars - 1) {
                    throw runtime_error("Unexpected end of line");
                }
                return true;
            case EOF:
                if (vars[i].empty()) {
                    stream.setstate(std::ios::eofbit);
                } else if (in_quotes || i < num_vars - 1) {
                    throw runtime_error("Unexpected end of file");
                }
                return false;
            case ',':
                if (!in_quotes) {
                    i++;
                    if (i >= num_vars) {
                        throw runtime_error("Too many columns");
                    }
                    vars[i].clear();
                } else {
                    vars[i] += (char)c;
                }
                break;
            default:
                vars[i] += (char)c;
        }
    }
}

template <typename T, typename I>
void Table<T, I>::read_indices_from_csv(istream& indicesstream) {
    string cols[2];
    I l = 0;
    try {
        while (readline(indicesstream, cols, 2)) {
            l++;
            index_set_.add_index(cols[1], cols[0]);
        }
        index_set_.rebuild_indices();
    } catch (const runtime_error& ex) {
        stringstream s;
        s << ex.what();
        s << " (line " << l << ")";
        throw runtime_error(s.str());
    }
}

template <typename T, typename I>
void Table<T, I>::read_data_from_csv(istream& datastream, const T& threshold) {
    string line;
    I l = 0;
    try {
        typename deque<T>::iterator d = data.begin();
        for (l = 0; l < index_set_.size(); l++) {
            if (l == numeric_limits<I>::max()) {
                throw runtime_error("Too many rows");
            }
            if (!getline(datastream, line)) {
                throw runtime_error("Not enough rows");
            }
            istringstream ss(line);
            string tmp;
            for (I c = 0; c < index_set_.size(); c++) {
                if (!getline(ss, tmp, ',')) {
                    throw runtime_error("Not enough columns");
                }
                T flow = stof(tmp.c_str());
                if (flow > threshold) {
                    *d = flow;
                }
                d++;
            }
        }
    } catch (const runtime_error& ex) {
        stringstream s;
        s << ex.what();
        s << " (line " << l << ")";
        throw runtime_error(s.str());
    } catch (const exception& ex) {
        stringstream s;
        s << "Could not parse number";
        s << " (line " << l << ")";
        throw runtime_error(s.str());
    }
}

template <typename T, typename I>
void Table<T, I>::read_from_csv(istream& indicesstream, istream& datastream, const T& threshold) {
    read_indices_from_csv(indicesstream);
    data.resize(index_set_.size() * index_set_.size(), 0);
    read_data_from_csv(datastream, threshold);
}

template <typename T, typename I>
void Table<T, I>::write_to_csv(ostream& outstream) const {
    debug_out();
    for (const auto& row : index_set_.total_indices) {
        for (const auto& col : index_set_.total_indices) {
            outstream << at(row.index, col.index) << ",";
        }
        outstream.seekp(-1, std::ios_base::end);
        outstream << endl;
    }
    outstream.seekp(-1, std::ios_base::end);
}

template <typename T, typename I>
void Table<T, I>::read_from_mrio(istream& instream, const T& threshold) {
    unsigned char c;
    instream.read((char*)&c, sizeof(c));
    if (c != sizeof(I)) {
        throw runtime_error("index type size mismatch");
    }
    instream.read((char*)&c, sizeof(c));
    if (c != sizeof(T)) {
        throw runtime_error("data type size mismatch");
    }
    I sectors_count;
    instream.read((char*)&sectors_count, sizeof(I));
    string s;
    for (I i = 0; i < sectors_count; i++) {
        getline(instream, s, '\0');
        index_set_.add_sector(s);
    }
    I regions_count;
    instream.read((char*)&regions_count, sizeof(I));
    for (I i = 0; i < regions_count; i++) {
        getline(instream, s, '\0');
        index_set_.add_region(s);
    }
    I index_count;
    instream.read((char*)&index_count, sizeof(I));
    for (I i = 0; i < index_count; i++) {
        I sector_index, region_index;
        instream.read((char*)&sector_index, sizeof(I));
        instream.read((char*)&region_index, sizeof(I));
        index_set_.add_index(index_set_.supersectors()[sector_index].get(), index_set_.superregions()[region_index].get());
    }
    index_set_.rebuild_indices();
    data.resize(index_set_.size() * index_set_.size(), 0);
    T val;
    for (auto& d : data) {
        instream.read((char*)&val, sizeof(T));
        if (val > threshold) {
            d = val;
        }
    }
}

template <typename T, typename I>
void Table<T, I>::write_to_mrio(ostream& outstream) const {
    debug_out();
    unsigned char c = sizeof(I);
    outstream.write((const char*)&c, sizeof(c));
    c = sizeof(T);
    outstream.write((const char*)&c, sizeof(c));
    outstream.write((const char*)&index_set_.total_sectors_count(), sizeof(I));
    c = 0;
    for (const auto& sector : index_set_.supersectors()) {
        if (sector->has_sub()) {
            for (const auto& subsector : sector->sub()) {
                outstream.write(subsector->name.c_str(), subsector->name.size() + 1);
            }
        } else {
            outstream.write(sector->name.c_str(), sector->name.size() + 1);
        }
    }
    outstream.write((const char*)&index_set_.total_regions_count(), sizeof(I));
    for (const auto& region : index_set_.superregions()) {
        if (region->has_sub()) {
            for (const auto& subregion : region->sub()) {
                outstream.write(subregion->name.c_str(), subregion->name.size() + 1);
            }
        } else {
            outstream.write(region->name.c_str(), region->name.size() + 1);
        }
    }

    outstream.write((const char*)&index_set_.size(), sizeof(I));
    for (const auto& row : index_set_.total_indices) {
        outstream.write((const char*)&row.sector->total_index(), sizeof(I));
        outstream.write((const char*)&row.region->total_index(), sizeof(I));
    }
    for (const auto& row : index_set_.total_indices) {
        for (const auto& col : index_set_.total_indices) {
            outstream.write((const char*)&(*this)(row.index, col.index), sizeof(T));
        }
    }
}

template <typename T, typename I>
void Table<T, I>::read_from_netcdf(const string& filename, const T& threshold) {
    netCDF::NcFile file(filename, netCDF::NcFile::read);

    size_t sectors_count = file.getDim("sector").getSize();
    {
        netCDF::NcVar sectors_var = file.getVar("sector");
        vector<const char*> sectors_val(sectors_count);
        sectors_var.getVar(&sectors_val[0]);
        for (const auto& sector : sectors_val) {
            index_set_.add_sector(sector);
        }
    }

    size_t regions_count = file.getDim("region").getSize();
    {
        netCDF::NcVar regions_var = file.getVar("region");
        vector<const char*> regions_val(regions_count);
        regions_var.getVar(&regions_val[0]);
        for (const auto& region : regions_val) {
            index_set_.add_region(region);
        }
    }

    // TODO also read heterogeneous MRIO tables
    vector<T> data_(regions_count * sectors_count * regions_count * sectors_count);
    file.getVar("flows").getVar(&data_[0]);
    if (file.getVar("flows").getDims()[0].getName() == "sector") {
        data.resize(regions_count * sectors_count * regions_count * sectors_count);
        typename deque<T>::iterator d = data.begin();
        for (const auto& region_from : index_set_.superregions()) {
            for (const auto& sector_from : index_set_.supersectors()) {
                index_set_.add_index(sector_from.get(), region_from.get());
                for (const auto& region_to : index_set_.superregions()) {
                    for (const auto& sector_to : index_set_.supersectors()) {
                        const T& d_ = data_[((*sector_from * regions_count + *region_from) * sectors_count + *sector_to) * regions_count + *region_to];
                        if (d_ > threshold) {
                            *d = d_;
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
        for (auto& d : data_) {
            if (d <= threshold) {
                d = 0;
            }
        }
        data = deque<T>(data_.begin(), data_.end());
    }
    index_set_.rebuild_indices();
}

template <typename T, typename I>
void Table<T, I>::write_to_netcdf(const string& filename) const {
    debug_out();
    netCDF::NcFile file(filename, netCDF::NcFile::replace, netCDF::NcFile::nc4);

    netCDF::NcDim sectors_dim = file.addDim("sector", index_set_.total_sectors_count());
    {
        netCDF::NcVar sectors_var = file.addVar("sector", netCDF::NcType::nc_STRING, { sectors_dim });
        I i = 0;
        for (const auto& sector : index_set_.supersectors()) {
            if (sector->has_sub()) {
                for (const auto& subsector : sector->sub()) {
                    sectors_var.putVar({ i }, subsector->name);
                    ++i;
                }
            } else {
                sectors_var.putVar({ i }, sector->name);
                ++i;
            }
        }
    }

    netCDF::NcDim regions_dim = file.addDim("region", index_set_.total_regions_count());
    {
        netCDF::NcVar regions_var = file.addVar("region", netCDF::NcType::nc_STRING, { regions_dim });
        I i = 0;
        for (const auto& region : index_set_.superregions()) {
            if (region->has_sub()) {
                for (const auto& subregion : region->sub()) {
                    regions_var.putVar({ i }, subregion->name);
                    ++i;
                }
            } else {
                regions_var.putVar({ i }, region->name);
                ++i;
            }
        }
    }

    netCDF::NcDim index_dim = file.addDim("index", index_set_.size());
    {
        netCDF::NcVar index_sector_var = file.addVar("index_sector", netCDF::NcType::nc_UINT, { index_dim });
        netCDF::NcVar index_region_var = file.addVar("index_region", netCDF::NcType::nc_UINT, { index_dim });
        for (const auto& index : index_set_.total_indices) {
            index_sector_var.putVar({ index.index }, index.sector->total_index() );
            index_region_var.putVar({ index.index }, index.region->total_index() );
        }
    }

    netCDF::NcVar flows_var = file.addVar("flows", netCDF::NcType::nc_FLOAT, { index_dim, index_dim });
    flows_var.setCompression(false, true, 7);
    //TODO flows_var.setFill<T>(true, std::numeric_limits<T>::quiet_NaN());

    //vector<T> data_ = vector<T>(data.begin(), data.end());
    for (const auto& row : index_set_.total_indices) {
        for (const auto& col : index_set_.total_indices) {
            flows_var.putVar({ row.index, col.index }, (const char*)&(*this)(row.index, col.index));
        }
    }
}

template <typename T, typename I>
void Table<T, I>::insert_sector_offset_x_y(const SuperSector<I>* i, const I& i_regions_count, const I& subsectors_count) {
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
                        next = -1;
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
                    insert_sector_offset_y(i, i_regions_count, subsectors_count, x, x_offset + offset, subsectors_count);
                }
            } else {
                x_offset--;
                insert_sector_offset_y(i, i_regions_count, subsectors_count, x, x_offset, 1);
            }
        }
    }
}

template <typename T, typename I>
void Table<T, I>::insert_sector_offset_y(const SuperSector<I>* i, const I& i_regions_count, const I& subsectors_count, const I& x, const I& x_offset,
                                         const I& divide_by) {
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
                        next = -1;
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
                    data[x_offset * new_size + y_offset + offset] = data[x * index_set_.size() + y] / subsectors_count / divide_by;
                }
            } else {
                y_offset--;
                data[x_offset * new_size + y_offset] = data[x * index_set_.size() + y] / divide_by;
            }
        }
    }
}

template <typename T, typename I>
void Table<T, I>::insert_region_offset_x_y(const SuperRegion<I>* r, const I& r_sectors_count, const I& subregions_count) {
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
        for (I x = index_set_.size(); x-- > last_index + 1;) {
            insert_region_offset_y(r, r_sectors_count, subregions_count, x, new_size + x - index_set_.size(), 1, first_index, last_index);
        }
        for (I x = last_index + 1; x-- > first_index;) {
            for (I offset = subregions_count; offset-- > 0;) {
                insert_region_offset_y(r, r_sectors_count, subregions_count, x, x + offset * r_sectors_count, subregions_count, first_index, last_index);
            }
        }
        for (I x = first_index; x-- > 0;) {
            insert_region_offset_y(r, r_sectors_count, subregions_count, x, x, 1, first_index, last_index);
        }
    }
}

template <typename T, typename I>
void Table<T, I>::insert_region_offset_y(const SuperRegion<I>* r, const I& r_sectors_count, const I& subregions_count, const I& x, const I& x_offset,
                                         const I& divide_by, const I& first_index, const I& last_index) {
    (void)(r);
    I new_size = index_set_.size() + r_sectors_count * (subregions_count - 1);
    for (I y = index_set_.size(); y-- > last_index + 1;) {
        data[x_offset * new_size + new_size + y - index_set_.size()] = data[x * index_set_.size() + y] / divide_by;
    }
    for (I y = last_index + 1; y-- > first_index;) {
        for (I offset = subregions_count; offset-- > 0;) {
            data[x_offset * new_size + y + offset * r_sectors_count] = data[x * index_set_.size() + y] / subregions_count / divide_by;
        }
    }
    for (I y = first_index; y-- > 0;) {
        data[x_offset * new_size + y] = data[x * index_set_.size() + y] / divide_by;
    }
}

template <typename T, typename I>
void Table<T, I>::debug_out() const {
#ifdef DEBUGOUT
    cout << "\n====\n";
    cout << std::setprecision(3) << std::fixed;
    for (const auto& y : index_set_.total_indices) {
        cout << index_set_.at(y.sector, y.region) << " " << y.sector->name << " " << (!y.sector->parent() ? "     " : y.sector->parent()->name) << " "
             << (y.sector->parent() ? *y.sector->parent() : *y.sector) << " " << (*y.sector) << " " << y.sector->level_index() << " " << y.region->name << " "
             << (!y.region->parent() ? "     " : y.region->parent()->name) << " " << (y.region->parent() ? *y.region->parent() : *y.region) << " "
             << (*y.region) << " " << y.region->level_index() << "  |  ";
        for (const auto& x : index_set_.total_indices) {
            if (data[x.index * index_set_.size() + y.index] == 0) {
                cout << " .   ";
            } else {
                cout << data[x.index * index_set_.size() + y.index];
            }
            cout << " ";
        }
        cout << "\n";
    }
    cout << "====\n";
#endif
}

template <typename T, typename I>
void Table<T, I>::insert_subsectors(const string& name, const vector<string>& subsectors) {
    const Sector<I>* sector = index_set_.sector(name);
    const SuperSector<I>* i = sector->as_super();
    if (!i) {
        throw runtime_error("'" + name + "' is a subsector");
    }
    if (i->has_sub()) {
        throw runtime_error("'" + name + "' already has subsectors");
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
    insert_sector_offset_x_y(i, i_regions_count, subsectors.size());
    // alter indices
    index_set_.insert_subsectors(name, subsectors);
    debug_out();
}

template <typename T, typename I>
void Table<T, I>::insert_subregions(const string& name, const vector<string>& subregions) {
    const Region<I>* region = index_set_.region(name);
    const SuperRegion<I>* r = region->as_super();
    if (!r) {
        throw runtime_error("'" + name + "' is a subregion");
    }
    if (r->has_sub()) {
        throw runtime_error("'" + name + "' already has subregions");
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
    insert_region_offset_x_y(r, r_sectors_count, subregions.size());
    // alter indices
    index_set_.insert_subregions(name, subregions);
    debug_out();
}

template class Table<float, unsigned int>;
template class Table<double, unsigned int>;
template class Table<unsigned char, unsigned int>;
