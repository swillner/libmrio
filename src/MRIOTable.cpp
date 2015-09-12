#include "MRIOTable.h"
#include "MRIOIndexSet.h"
#include <sstream>
#include <cassert>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <iomanip>

using namespace std;
using namespace mrio;

template<typename T, typename I>
const T Table<T, I>::sum(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
    T res = 0;
    if (i == nullptr) {
        for (auto& i_ : index_set_.supersectors()) {
            res += sum(i_, r, j, s);
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
            res += sum(i, r, j_, s);
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

template<typename T, typename I>
const T Table<T, I>::basesum(const SuperSector<I>* i, const SuperRegion<I>* r, const SuperSector<I>* j, const SuperRegion<I>* s) const noexcept {
    T res = 0;
    if (i == nullptr) {
        for (auto& i_ : index_set_.supersectors()) {
            res += basesum(i_, r, j, s);
        }
    } else if (r == nullptr) {
        for (auto& r_ : i->regions()) {
            res += basesum(i, r_, j, s);
        }
    } else if (j == nullptr) {
        for (auto& j_ : index_set_.supersectors()) {
            res += basesum(i, r, j_, s);
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

template<typename T, typename I>
void Table<T, I>::read_indices(istream& indicesstream) {
    string cols[2];
    I l = 0;
    try {
        while (readline(indicesstream, cols, 2)) {
            if (l == numeric_limits<I>::max()) {
                throw runtime_error("Too many rows");
            }
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

template<typename T, typename I>
void Table<T, I>::read_data(istream& datastream, const T& threshold) {
    string line;
    I l = 0;
    try {
        for (const auto& row : index_set_.total_indices) {
            if (l == numeric_limits<I>::max()) {
                throw runtime_error("Too many rows");
            }
            l++;
            if (!getline(datastream, line)) {
                throw runtime_error("Not enough rows");
            }
            istringstream ss(line);
            string tmp;
            for (const auto& col : index_set_.total_indices) {
                if (!getline(ss, tmp, ',')) {
                    throw runtime_error("Not enough columns");
                }
                T flow = stof(tmp.c_str());
                if (flow > threshold) {
                    at(row.index, col.index) = flow;
                } else {
                    at(row.index, col.index) = 0;
                }
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

template<typename T, typename I>
void Table<T, I>::read_from_csv(istream& indicesstream, istream& datastream, const T& threshold) {
    read_indices(indicesstream);
    data.resize(index_set_.size() * index_set_.size(), numeric_limits<T>::signaling_NaN());
    read_data(datastream, threshold);
}

template<typename T, typename I>
void Table<T, I>::write_to_csv(ostream& outstream) const {
    debug_out();
    for (const auto& row : index_set_.total_indices) {
        for (const auto& col : index_set_.total_indices) {
            outstream << at(row.index, col.index) << ",";
        }
        outstream.seekp(-1, ios_base::end);
        outstream << endl;
    }
    outstream.seekp(-1, ios_base::end);
}

template<typename T, typename I>
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
        for (I x = index_set_.size() - 1; x >= 0; x--) {
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
                for (I offset = subsectors_count - 1; offset >= 0; offset--) {
                    insert_sector_offset_y(i, i_regions_count, subsectors_count, x, x_offset + offset, subsectors_count);
                }
            } else {
                x_offset--;
                insert_sector_offset_y(i, i_regions_count, subsectors_count, x, x_offset, 1);
            }
        }
    }
}

template<typename T, typename I>
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
        for (I y = index_set_.size() - 1; y >= 0; y--) {
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
                for (I offset = subsectors_count - 1; offset >= 0; offset--) {
                    data[x_offset * new_size + y_offset + offset] = data[x * index_set_.size() + y] / subsectors_count / divide_by;
                }
            } else {
                y_offset--;
                data[x_offset * new_size + y_offset] = data[x * index_set_.size() + y] / divide_by;
            }
        }
    }
}

template<typename T, typename I>
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
        for (I x = index_set_.size() - 1; x > last_index; x--) {
            insert_region_offset_y(r, r_sectors_count, subregions_count, x, new_size + x - index_set_.size(), 1, first_index, last_index);
        }
        for (I x = last_index; x >= first_index; x--) {
            for (I offset = subregions_count - 1; offset >= 0; offset--) {
                insert_region_offset_y(r, r_sectors_count, subregions_count, x, x + offset * r_sectors_count, subregions_count, first_index, last_index);
            }
        }
        for (I x = first_index - 1; x >= 0; x--) {
            insert_region_offset_y(r, r_sectors_count, subregions_count, x, x, 1, first_index, last_index);
        }
    }
}

template<typename T, typename I>
void Table<T, I>::insert_region_offset_y(const SuperRegion<I>* r, const I& r_sectors_count, const I& subregions_count, const I& x, const I& x_offset,
        const I& divide_by, const I& first_index, const I& last_index) {
    I new_size = index_set_.size() + r_sectors_count * (subregions_count - 1);
    for (I y = index_set_.size() - 1; y > last_index; y--) {
        data[x_offset * new_size + new_size + y - index_set_.size()] = data[x * index_set_.size() + y] / divide_by;
    }
    for (I y = last_index; y >= first_index; y--) {
        for (I offset = subregions_count - 1; offset >= 0; offset--) {
            data[x_offset * new_size + y + offset * r_sectors_count] = data[x * index_set_.size() + y] / subregions_count / divide_by;
        }
    }
    for (I y = first_index - 1; y >= 0; y--) {
        data[x_offset * new_size + y] = data[x * index_set_.size() + y] / divide_by;
    }
}

template<typename T, typename I>
void Table<T, I>::debug_out() const {
#ifdef DEBUG
    cout << "\n====\n";
    cout << std::setprecision(3) << std::fixed;
    for (const auto& y : index_set_.total_indices) {
        cout << index_set_.at(y.sector, y.region) << " "
             << y.sector->name << " " << (!y.sector->parent() ? "     " : y.sector->parent()->name) << " " << (y.sector->parent() ? *y.sector->parent() : *y.sector) << " "
             << (*y.sector) << " " << y.sector->level_index() << " "
             << y.region->name << " " << (!y.region->parent() ? "     " : y.region->parent()->name) << " " << (y.region->parent() ? *y.region->parent() : *y.region) << " "
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

template<typename T, typename I>
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

template<typename T, typename I>
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

template class Table<double, short>;
template class Table<unsigned char, short>;
