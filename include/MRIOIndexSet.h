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

#ifndef LIBMRIO_MRIOINDEXSET_H
#define LIBMRIO_MRIOINDEXSET_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#ifdef DEBUG
#include <cassert>
#else
#undef assert
#define assert(a) \
    {}
#endif
#ifdef LIBMRIO_VERBOSE
#include <iostream>
#endif

namespace mrio {

template<typename I>
class IndexSet;

template<typename I>
class IndexPart {
    friend class IndexSet<I>;

  protected:
    I total_index_;  ///< index on all sector/region levels
    I level_index_;  ///< index on current level
    IndexPart(const std::string& name_p, const I& total_index_p, const I& level_index_p) noexcept
        : name(name_p), total_index_(total_index_p), level_index_(level_index_p) {}

  public:
    const std::string name;
    inline operator I() const noexcept { return total_index_; }
    inline const I& total_index() const noexcept { return total_index_; }
    inline const I& level_index() const noexcept { return level_index_; }
};

template<typename I>
class Region;

template<typename I>
class Sector : public IndexPart<I> {
    friend class IndexSet<I>;

  protected:
    I subindex_;  ///< index in parent sector
    const Sector<I>* parent_;
    std::vector<Sector<I>*> sub_;
    std::vector<Region<I>*> regions_;
    Sector(const std::string& name_p, const I& total_index_p, const I& level_index_p) noexcept
        : IndexPart<I>(name_p, total_index_p, level_index_p), parent_(nullptr) {}
    Sector(const std::string& name_p, const I& total_index_p, const I& level_index_p, const Sector<I>* parent_p, const I& subindex_p) noexcept
        : IndexPart<I>(name_p, total_index_p, level_index_p), parent_(parent_p), subindex_(subindex_p) {}

  public:
    inline bool is_sub() const noexcept { return parent_ != nullptr; }
    inline bool has_sub() const noexcept { return sub_.size() > 0; }
    inline const Sector<I>* parent() const noexcept { return parent_; }
    inline const std::vector<Region<I>*>& regions() const noexcept { return regions_; }
    inline const std::vector<Sector<I>*>& sub() const noexcept { return sub_; }
    inline const Sector<I>* super() const noexcept { return parent_ != nullptr ? parent_ : this; }
    inline Sector<I>* as_super() noexcept { return parent_ != nullptr ? nullptr : this; }
    inline const Sector<I>* as_super() const noexcept { return parent_ != nullptr ? nullptr : this; }
};

template<typename I>
class Region : public IndexPart<I> {
    friend class IndexSet<I>;

  protected:
    I subindex_;  ///< index in parent sector
    const Region<I>* parent_;
    std::vector<Region<I>*> sub_;
    std::vector<Sector<I>*> sectors_;
    Region(const std::string& name_p, const I& total_index_p, const I& level_index_p) noexcept
        : IndexPart<I>(name_p, total_index_p, level_index_p), parent_(nullptr) {}
    Region(const std::string& name_p, const I& total_index_p, const I& level_index_p, const Region<I>* parent_p, const I& subindex_p) noexcept
        : IndexPart<I>(name_p, total_index_p, level_index_p), parent_(parent_p), subindex_(subindex_p) {}

  public:
    inline bool is_sub() const noexcept { return parent_ != nullptr; }
    inline bool has_sub() const noexcept { return sub_.size() > 0; }
    inline const Region<I>* parent() const noexcept { return parent_; }
    inline const std::vector<Sector<I>*>& sectors() const noexcept { return sectors_; }
    inline const std::vector<Region<I>*>& sub() const noexcept { return sub_; }
    inline const Region<I>* super() const noexcept { return parent_ != nullptr ? parent_ : this; }
    inline Region<I>* as_super() noexcept { return parent_ != nullptr ? nullptr : this; }
    inline const Region<I>* as_super() const noexcept { return parent_ != nullptr ? nullptr : this; }
};

template<typename I>
class IndexSet {
  private:
    I size_;
    I total_regions_count_;
    I total_sectors_count_;
    std::unordered_map<std::string, Sector<I>*> sectors_map;
    std::unordered_map<std::string, Region<I>*> regions_map;
    std::vector<std::unique_ptr<Sector<I>>> supersectors_;
    std::vector<std::unique_ptr<Region<I>>> superregions_;
    std::vector<std::unique_ptr<Sector<I>>> subsectors_;
    std::vector<std::unique_ptr<Region<I>>> subregions_;
    std::vector<I> indices_;

    void copy_pointers(const IndexSet<I>& other);

  public:
    static const I NOT_GIVEN;
    class total_iterator {
      private:
        const IndexSet& index_set;
        I index;
        typename std::vector<Sector<I>*>::const_iterator supersector_it;
        typename std::vector<std::unique_ptr<Region<I>>>::const_iterator superregion_it;
        typename std::vector<Sector<I>*>::const_iterator subsector_it;
        typename std::vector<Region<I>*>::const_iterator subregion_it;
        explicit total_iterator(const IndexSet& index_set_p) : index_set(index_set_p), index(0) {}

      public:
        struct Index {
            const Sector<I>* sector;
            const Region<I>* region;
            const I& index;
        };
        static total_iterator begin(const IndexSet& index_set) {
            total_iterator res(index_set);
            res.superregion_it = index_set.superregions().begin();
            if (res.superregion_it != index_set.superregions().end()) {
                res.subregion_it = (*res.superregion_it)->sub().begin();
                res.supersector_it = (*res.superregion_it)->sectors().begin();
                if (res.supersector_it != (*res.superregion_it)->sectors().end()) {
                    res.subsector_it = (*res.supersector_it)->sub().begin();
                }
            }
            return res;
        }
        static total_iterator end(const IndexSet& index_set) {
            total_iterator res(index_set);
            res.superregion_it = index_set.superregions().end();
            res.index = index_set.size();
            return res;
        }
        total_iterator operator++() {
            if (subsector_it == (*supersector_it)->sub().end() || ++subsector_it == (*supersector_it)->sub().end()) {
                if (supersector_it == (*superregion_it)->sectors().end() || ++supersector_it == (*superregion_it)->sectors().end()) {
                    if (subregion_it == (*superregion_it)->sub().end() || ++subregion_it == (*superregion_it)->sub().end()) {
                        if (superregion_it == index_set.superregions().end() || ++superregion_it == index_set.superregions().end()) {
                            ++index;
                            return *this;
                        }
                        subregion_it = (*superregion_it)->sub().begin();
                    }
                    supersector_it = (*superregion_it)->sectors().begin();
                }
                subsector_it = (*supersector_it)->sub().begin();
            }
            ++index;
            return *this;
        }
        const Index operator*() const {
            return {(subsector_it == (*supersector_it)->sub().end()) ? static_cast<Sector<I>*>(*supersector_it) : static_cast<Sector<I>*>(*subsector_it),
                    (subregion_it == (*superregion_it)->sub().end()) ? static_cast<Region<I>*>(superregion_it->get()) : static_cast<Region<I>*>(*subregion_it),
                    index};
        }
        bool operator==(const total_iterator& rhs) const { return index == rhs.index; }
        bool operator!=(const total_iterator& rhs) const { return !(*this == rhs); }
    };
    total_iterator tbegin() const { return total_iterator::begin(*this); }
    total_iterator tend() const { return total_iterator::end(*this); }
    class TotalIndices {
      protected:
        const IndexSet& index_set;

      public:
        explicit TotalIndices(const IndexSet& index_set_p) : index_set(index_set_p) {}
        total_iterator begin() const { return index_set.tbegin(); }
        total_iterator end() const { return index_set.tend(); }
    };
    const TotalIndices total_indices;

    class super_iterator {
      private:
        const IndexSet* parent;
        typename std::vector<Sector<I>*>::const_iterator sector_it;
        typename std::vector<std::unique_ptr<Region<I>>>::const_iterator region_it;
        explicit super_iterator(const IndexSet* parent_p) : parent(parent_p) {}

      public:
        struct Index {
            const Sector<I>* sector;
            const Region<I>* region;
        };
        static super_iterator begin(const IndexSet* parent) {
            super_iterator res(parent);
            res.region_it = parent->superregions().begin();
            if (res.region_it != parent->superregions().end()) {
                res.sector_it = (*res.region_it)->sectors().begin();
            }
            return res;
        }
        static super_iterator end(const IndexSet* parent) {
            super_iterator res(parent);
            res.region_it = parent->superregions().end();
            return res;
        }
        super_iterator operator++() {
            ++sector_it;
            if (sector_it == (*region_it)->sectors().end()) {
                ++region_it;
                if (region_it != parent->superregions().end()) {
                    sector_it = (*region_it)->sectors().begin();
                }
            }
            return *this;
        }
        const Index operator*() const { return {*sector_it, region_it->get()}; }
        bool operator==(const super_iterator& rhs) const {
            return (region_it == rhs.region_it && sector_it == rhs.sector_it)
                   || (region_it == parent->superregions().end() && rhs.region_it == rhs.parent->superregions().end());
        }
        bool operator!=(const super_iterator& rhs) const { return !(*this == rhs); }
    };
    super_iterator sbegin() const { return super_iterator::begin(this); }
    super_iterator send() const { return super_iterator::end(this); }
    class SuperIndices {
      protected:
        const IndexSet& index_set;

      public:
        explicit SuperIndices(const IndexSet& index_set_p) : index_set(index_set_p) {}
        super_iterator begin() const { return index_set.sbegin(); }
        super_iterator end() const { return index_set.send(); }
    };
    const SuperIndices super_indices;

    IndexSet() : size_(0), total_regions_count_(0), total_sectors_count_(0), total_indices(*this), super_indices(*this) {}
    IndexSet(const IndexSet& other);
    IndexSet& operator=(const IndexSet& other);
    const I& size() const { return size_; }
    const I& total_regions_count() const { return total_regions_count_; }
    const I& total_sectors_count() const { return total_sectors_count_; }
    const std::vector<std::unique_ptr<Sector<I>>>& supersectors() const { return supersectors_; }
    const std::vector<std::unique_ptr<Region<I>>>& superregions() const { return superregions_; }
    const std::vector<std::unique_ptr<Sector<I>>>& subsectors() const { return subsectors_; }
    const std::vector<std::unique_ptr<Region<I>>>& subregions() const { return subregions_; }
    const Sector<I>* sector(const std::string& name) const { return sectors_map.at(name); }
    const Region<I>* region(const std::string& name) const { return regions_map.at(name); }
    void clear();
    virtual ~IndexSet() { clear(); }
    Sector<I>* add_sector(const std::string& name);
    Region<I>* add_region(const std::string& name);
    void add_index(const std::string& sector_name, const std::string& region_name);
    void add_index(Sector<I>* sector_p, Region<I>* region_p);
    void rebuild_indices();
    inline const I& at(const Sector<I>* sector_p, const Region<I>* region_p) const {
        assert(!sector_p->has_sub());
        assert(!region_p->has_sub());
        return indices_.at(*sector_p * total_regions_count_ + *region_p);
    }
    inline const I& at(const std::string& sector_name, const std::string& region_name) const {
        const Sector<I>* sector_ = sector(sector_name);
        const Region<I>* region_ = region(region_name);
        assert(!sector_->has_sub());
        assert(!region_->has_sub());
        return indices_.at(*sector_ * total_regions_count_ + *region_);
    }
    inline const I& operator()(const Sector<I>* sector_p, const Region<I>* region_p) const noexcept {
        assert(*sector_p * total_regions_count_ + *region_p >= 0);
        assert(*sector_p * total_regions_count_ + *region_p < indices_.size());
        assert(!sector_p->has_sub());
        assert(!region_p->has_sub());
        return indices_[*sector_p * total_regions_count_ + *region_p];
    }
    inline const I& operator()(const I& sector_index, const I& region_index) const noexcept {
        assert(sector_index * total_regions_count_ + region_index >= 0);
        assert(sector_index * total_regions_count_ + region_index < indices_.size());
        return indices_[sector_index * total_regions_count_ + region_index];
    }
    void insert_subsectors(const std::string& name, const std::vector<std::string>& newsubsectors);
    void insert_subregions(const std::string& name, const std::vector<std::string>& newsubregions);

    void debug_out() const {
#ifdef LIBMRIO_VERBOSE
        std::cout << "indices=[ ";
        for (const auto& i : indices_) {
            std::cout << i << " ";
        }
        std::cout << "]" << std::endl;
#endif
    }

    /**
     * @brief Returns index for Sector-Region-Combination of a foreign IndexSet
     *        that is a disaggregated version of this non-disaggregated IndexSet
     *
     * @param sector Sector (from disaggregated IndexSet)
     * @param region Region (from disaggregated IndexSet)
     * @return Reference to index (in this non-disaggregated IndexSet)
     */
    inline const I& base(const Sector<I>* sector_p, const Region<I>* region_p) const noexcept {
        assert(sector_p->level_index() * superregions_.size() + region_p->level_index() >= 0);
        assert(sector_p->level_index() * superregions_.size() + region_p->level_index() < indices_.size());
        return indices_[sector_p->level_index() * superregions_.size() + region_p->level_index()];
    }
};
}  // namespace mrio

#endif
