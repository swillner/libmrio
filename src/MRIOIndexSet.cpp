#include "MRIOIndexSet.h"
#include <limits>
#include <algorithm>

using namespace std;
using namespace mrio;

template<typename I>
void IndexSet<I>::clear() {
    sectors_map.clear();
    regions_map.clear();
    for (auto& it : supersectors_) {
        delete it;
    }
    supersectors_.clear();
    for (auto& it : superregions_) {
        delete it;
    }
    superregions_.clear();
    for (auto& it : subsectors_) {
        delete it;
    }
    subsectors_.clear();
    for (auto& it : subregions_) {
        delete it;
    }
    subregions_.clear();
    indices_.clear();
    size_ = 0;
    total_sectors_count_ = 0;
    total_regions_count_ = 0;
}

template<typename I>
SuperSector<I>* IndexSet<I>::add_sector(const string& name) {
    if (subsectors_.size() > 0) {
        throw runtime_error("Cannot add new sector when already disaggregated");
    }
    const auto res = sectors_map.find(name);
    if (res == sectors_map.end()) {
        indices_.clear();
        SuperSector<I>* s = new SuperSector<I>(name, supersectors_.size(), supersectors_.size());
        supersectors_.push_back(s);
        sectors_map.insert(make_pair(name, s));
        total_sectors_count_++;
        return s;
    } else {
        return static_cast<SuperSector<I>*>(res->second);
    }
}

template<typename I>
SuperRegion<I>* IndexSet<I>::add_region(const string& name) {
    if (subregions_.size() > 0) {
        throw runtime_error("Cannot add new region when already disaggregated");
    }
    const auto res = regions_map.find(name);
    if (res == regions_map.end()) {
        indices_.clear();
        SuperRegion<I>* r = new SuperRegion<I>(name, superregions_.size(), superregions_.size());
        superregions_.push_back(r);
        regions_map.insert(make_pair(name, r));
        total_regions_count_++;
        return r;
    } else {
        return static_cast<SuperRegion<I>*>(res->second);
    }
}

template<typename I>
void IndexSet<I>::add_index(SuperSector<I>* sector, SuperRegion<I>* region) {
    region->sectors_.push_back(sector);
    sector->regions_.push_back(region);
    size_++;
}

template<typename I>
void IndexSet<I>::add_index(const string& sector_name, const string& region_name) {
    SuperSector<I>* sector = add_sector(sector_name);
    SuperRegion<I>* region = add_region(region_name);
    if (find(region->sectors_.begin(), region->sectors_.end(), sector) != region->sectors_.end()) {
        throw runtime_error("Combination of sector and region already given");
    }
    add_index(sector, region);
}

template<typename I>
void IndexSet<I>::rebuild_indices() {
    indices_.clear();
    indices_.resize(total_sectors_count_ * total_regions_count_, -1);
    I index = 0;
    for (const auto& r : superregions_) {
        if (r->has_sub()) {
            for (const auto& sub_r : r->sub()) {
                for (const auto& s : r->sectors_) {
                    if (s->has_sub()) {
                        for (const auto& sub_s : s->sub()) {
                            indices_[*sub_s * total_regions_count_ + *sub_r] = index;
                            index++;
                        }
                    } else {
                        indices_[*s * total_regions_count_ + *sub_r] = index;
                        index++;
                    }
                }
            }
        } else {
            for (const auto& s : r->sectors_) {
                if (s->has_sub()) {
                    for (const auto& sub_s : s->sub()) {
                        indices_[*sub_s * total_regions_count_ + *r] = index;
                        index++;
                    }
                } else {
                    indices_[*s * total_regions_count_ + *r] = index;
                    index++;
                }
            }
        }
    }
}

template<typename I>
IndexSet<I>::IndexSet(const IndexSet<I>& other)
    : size_(other.size_),
      total_regions_count_(other.total_regions_count_),
      total_sectors_count_(other.total_sectors_count_),
      sectors_map(other.sectors_map),
      regions_map(other.regions_map),
      supersectors_(other.supersectors_),
      superregions_(other.superregions_),
      subsectors_(other.subsectors_),
      subregions_(other.subregions_),
      indices_(other.indices_),
      total_indices(*this),
      super_indices(*this) {
    readjust_pointers();
}

template<typename I>
IndexSet<I>& IndexSet<I>::operator=(const IndexSet<I>& other) {
    size_ = other.size_;
    total_regions_count_ = other.total_regions_count_;
    total_sectors_count_ = other.total_sectors_count_;
    sectors_map = other.sectors_map;
    regions_map = other.regions_map;
    supersectors_ = other.supersectors_;
    superregions_ = other.superregions_;
    subsectors_ = other.subsectors_;
    subregions_ = other.subregions_;
    indices_ = other.indices_;
    readjust_pointers();
    return *this;
}

template<typename I>
void IndexSet<I>::readjust_pointers() {
    for (I it = 0; it < subsectors_.size(); it++) {
        SubSector<I>* n = new SubSector<I>(*subsectors_[it]);
        subsectors_[it] = n;
        sectors_map.at(n->name) = n;
    }
    for (I it = 0; it < subregions_.size(); it++) {
        SubRegion<I>* n = new SubRegion<I>(*subregions_[it]);
        subregions_[it] = n;
        regions_map.at(n->name) = n;
    }
    for (I it = 0; it < supersectors_.size(); it++) {
        SuperSector<I>* n = new SuperSector<I>(*supersectors_[it]);
        supersectors_[it] = n;
        sectors_map.at(n->name) = n;
        for (I it2 = 0; it2 < n->sub_.size(); it2++) {
            n->sub_[it2] = static_cast<SubSector<I>*>(sectors_map.at(n->sub_[it2]->name));
            n->sub_[it2]->parent_ = n;
        }
    }
    for (I it = 0; it < superregions_.size(); it++) {
        SuperRegion<I>* n = new SuperRegion<I>(*superregions_[it]);
        superregions_[it] = n;
        regions_map.at(n->name) = n;
        for (I it2 = 0; it2 < n->sub_.size(); it2++) {
            n->sub_[it2] = static_cast<SubRegion<I>*>(regions_map.at(n->sub_[it2]->name));
            n->sub_[it2]->parent_ = n;
        }
        for (I it2 = 0; it2 < n->sectors_.size(); it2++) {
            n->sectors_[it2] = static_cast<SuperSector<I>*>(sectors_map.at(n->sectors_[it2]->name));
        }
    }
    for (I it = 0; it < supersectors_.size(); it++) {
        SuperSector<I>* n = supersectors_[it];
        for (I it2 = 0; it2 < n->regions_.size(); it2++) {
            n->regions_[it2] = static_cast<SuperRegion<I>*>(regions_map.at(n->regions_[it2]->name));
        }
    }
}

template<typename I>
void IndexSet<I>::insert_subsectors(const string& name, const vector<string>& newsubsectors) {
    Sector<I>* super_ = sectors_map.at(name);
    if (!super_->as_super()) {
        throw runtime_error("Sector '" + name + "' is not a super sector");
    }
    SuperSector<I>* super = static_cast<SuperSector<I>*>(super_);
    I total_index = super->total_index_;
    I level_index = subsectors_.size();
    I subindex = 0;
    for (const auto& sub_name : newsubsectors) {
        SubSector<I>* sub = new SubSector<I>(sub_name, total_index, level_index, super, subindex);
        sectors_map.insert(make_pair(sub_name, sub));
        subsectors_.push_back(sub);
        super->sub_.push_back(sub);
        total_index++;
        level_index++;
        subindex++;
    }
    for (auto& adjust_super : supersectors_) {
        if (adjust_super->total_index_ > super->total_index_) {
            adjust_super->total_index_ += newsubsectors.size() - 1;
            for (auto& adjust_sub : adjust_super->sub()) {
                adjust_sub->total_index_ += newsubsectors.size() - 1;
            }
        }
    }
    I total_regions_size = 0;
    for (const auto& region : super->regions()) {
        I size = region->sub().size();
        if (size > 0) {
            total_regions_size += size;
        } else {
            total_regions_size++;
        }
    }
    total_sectors_count_ += (newsubsectors.size() - 1);
    size_ += (newsubsectors.size() - 1) * total_regions_size;
    rebuild_indices();
}

template<typename I>
void IndexSet<I>::insert_subregions(const string& name, const vector<string>& newsubregions) {
    Region<I>* super_ = regions_map.at(name);
    if (!super_->as_super()) {
        throw runtime_error("Region '" + name + "' is not a super region");
    }
    SuperRegion<I>* super = static_cast<SuperRegion<I>*>(super_);
    I total_index = super->total_index_;
    I level_index = subregions_.size();
    I subindex = 0;
    for (const auto& sub_name : newsubregions) {
        SubRegion<I>* sub = new SubRegion<I>(sub_name, total_index, level_index, super, subindex);
        regions_map.insert(make_pair(sub_name, sub));
        subregions_.push_back(sub);
        super->sub_.push_back(sub);
        total_index++;
        level_index++;
        subindex++;
    }
    for (auto& adjust_super : superregions_) {
        if (adjust_super->total_index_ > super->total_index_) {
            adjust_super->total_index_ += newsubregions.size() - 1;
            for (auto& adjust_sub : adjust_super->sub()) {
                adjust_sub->total_index_ += newsubregions.size() - 1;
            }
        }
    }
    I total_sectors_size = 0;
    for (const auto& sector : super->sectors()) {
        I size = sector->sub().size();
        if (size > 0) {
            total_sectors_size += size;
        } else {
            total_sectors_size++;
        }
    }
    total_regions_count_ += (newsubregions.size() - 1);
    size_ += (newsubregions.size() - 1) * total_sectors_size;
    rebuild_indices();
}

template class IndexSet<unsigned short>;
