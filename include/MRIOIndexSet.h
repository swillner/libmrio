/**
 * \copyright Copyright (c) 2014, Sven N. Willner <sven.willner@pik-potsdam.de>
 *
 * \license {Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.}
 */

#ifndef MRIOINDEXSET_H_
#define MRIOINDEXSET_H_

#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include <iostream>
#include <cassert>

using namespace std;

namespace mrio {

    template<typename I> class IndexSet;
    template<typename I> class IndexPart {
            friend class IndexSet<I>;
        protected:
            I total_index_; ///< index on all sector/region levels
            I level_index_; ///< index on current level
            IndexPart(const string& name, const I& total_index, const I& level_index)
                : name(name), total_index_(total_index), level_index_(level_index) {};
        public:
            const string name;
            operator I() const {
                return total_index_;
            };
            const I& total_index() const {
                return total_index_;
            };
            const I& level_index() const {
                return level_index_;
            };
    };

    template<typename I> class SuperSector;
    template<typename I> class Sector : public IndexPart<I> {
            friend class IndexSet<I>;
        protected:
            Sector(const string& name, const I& total_index, const I& level_index)
                : IndexPart<I>(name, total_index, level_index) {};
        public:
            virtual const SuperSector<I>* parent() const = 0;
            virtual const SuperSector<I>* as_super() const = 0;
            virtual const SuperSector<I>* super() const = 0;
            virtual const bool has_sub() const = 0;
    };
    template<typename I> class SubSector : public Sector<I> {
            friend class IndexSet<I>;
        protected:
            I subindex_; ///< index in parent sector
            const SuperSector<I>* parent_;
            SubSector(const string& name, const I& total_index, const I& level_index, const SuperSector<I>* parent, const I& subindex)
                : Sector<I>(name, total_index, level_index), parent_(parent), subindex_(subindex) {};
        public:
            const SuperSector<I>* super() const override {
                return parent_;
            };
            const SuperSector<I>* as_super() const override {
                return nullptr;
            };
            const SuperSector<I>* parent() const override {
                return parent_;
            };
            const bool has_sub() const override {
                return false;
            };
    };
    template<typename I> class SuperRegion;
    template<typename I> class SuperSector : public Sector<I> {
            friend class IndexSet<I>;
        protected:
            vector<SubSector<I>*> sub_;
            vector<SuperRegion<I>*> regions_;
            SuperSector(const string& name, const I& total_index, const I& level_index)
                : Sector<I>(name, total_index, level_index) {};
        public:
            const vector<SuperRegion<I>*>& regions() const {
                return regions_;
            };
            inline const vector<SubSector<I>*>& sub() const {
                return sub_;
            };
            const SuperSector<I>* super() const override {
                return this;
            };
            const SuperSector<I>* as_super() const override {
                return this;
            };
            const SuperSector<I>* parent() const override {
                return nullptr;
            };
            const bool has_sub() const override {
                return sub_.size() > 0;
            };
    };

    template<typename I> class SuperRegion;
    template<typename I> class Region : public IndexPart<I> {
            friend class IndexSet<I>;
        protected:
            Region(const string& name, const I& total_index, const I& level_index)
                : IndexPart<I>(name, total_index, level_index) {};
        public:
            virtual const SuperRegion<I>* parent() const = 0;
            virtual const SuperRegion<I>* as_super() const = 0;
            virtual const SuperRegion<I>* super() const = 0;
            virtual const bool has_sub() const = 0;
    };
    template<typename I> class SubRegion : public Region<I> {
            friend class IndexSet<I>;
        protected:
            I subindex_; ///< index in parent sector
            const SuperRegion<I>* parent_;
            SubRegion(const string& name, const I& total_index, const I& level_index, const SuperRegion<I>* parent, const I& subindex)
                : Region<I>(name, total_index, level_index), parent_(parent), subindex_(subindex) {};
        public:
            const SuperRegion<I>* super() const override {
                return parent_;
            };
            const SuperRegion<I>* as_super() const override {
                return nullptr;
            };
            const SuperRegion<I>* parent() const override {
                return parent_;
            };
            const bool has_sub() const override {
                return false;
            };
    };
    template<typename I> class SuperRegion : public Region<I> {
            friend class IndexSet<I>;
        protected:
            vector<SubRegion<I>*> sub_;
            vector<SuperSector<I>*> sectors_;
            SuperRegion(const string& name, const I& total_index, const I& level_index)
                : Region<I>(name, total_index, level_index) {};
        public:
            const vector<SuperSector<I>*>& sectors() const {
                return sectors_;
            };
            inline const vector<SubRegion<I>*>& sub() const {
                return sub_;
            };
            const SuperRegion<I>* super() const override {
                return this;
            };
            const SuperRegion<I>* as_super() const override {
                return this;
            };
            const SuperRegion<I>* parent() const override {
                return nullptr;
            };
            const bool has_sub() const override {
                return sub_.size() > 0;
            };
    };

    template<typename I> class IndexSet {
        private:
            I size_;
            I total_regions_count_;
            I total_sectors_count_;
            unordered_map<string, Sector<I>*> sectors_map;
            unordered_map<string, Region<I>*> regions_map;
            vector<SuperSector<I>*> supersectors_;
            vector<SuperRegion<I>*> superregions_;
            vector<SubSector<I>*> subsectors_;
            vector<SubRegion<I>*> subregions_;
            vector<I> indices_;

            void readjust_pointers();
            SuperSector<I>* add_sector(const string& name);
            SuperRegion<I>* add_region(const string& name);
        public:
            class total_iterator {
                private:
                    const IndexSet& index_set;
                    I index;
                    typename vector<SuperSector<I>*>::const_iterator supersector_it;
                    typename vector<SuperRegion<I>*>::const_iterator superregion_it;
                    typename vector<SubSector<I>*>::const_iterator subsector_it;
                    typename vector<SubRegion<I>*>::const_iterator subregion_it;
                    total_iterator(const IndexSet& index_set)
                        : index_set(index_set), index(0) {
                    };
                public:
                    struct Index {
                        const Sector<I>* sector;
                        const Region<I>* region;
                        const I& index;
                    };
                    typedef std::forward_iterator_tag iterator_category;
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
                    };
                    static total_iterator end(const IndexSet& index_set) {
                        total_iterator res(index_set);
                        res.superregion_it = index_set.superregions().end();
                        res.index = index_set.size();
                        return res;
                    };
                    total_iterator operator++() {
                        if (subsector_it == (*supersector_it)->sub().end() || ++subsector_it == (*supersector_it)->sub().end()) {
                            if (supersector_it == (*superregion_it)->sectors().end() || ++supersector_it == (*superregion_it)->sectors().end()) {
                                if (subregion_it == (*superregion_it)->sub().end() || ++subregion_it == (*superregion_it)->sub().end()) {
                                    if (superregion_it == index_set.superregions().end() || ++superregion_it == index_set.superregions().end()) {
                                        index++;
                                        return *this;
                                    }
                                    subregion_it = (*superregion_it)->sub().begin();
                                }
                                supersector_it = (*superregion_it)->sectors().begin();
                            }
                            subsector_it = (*supersector_it)->sub().begin();
                        }
                        index++;
                        return *this;
                    };
                    const Index operator*() const {
                        return {
                            (subsector_it == (*supersector_it)->sub().end()) ? static_cast<Sector<I>*>(*supersector_it) : static_cast<Sector<I>*>(*subsector_it),
                            (subregion_it == (*superregion_it)->sub().end()) ? static_cast<Region<I>*>(*superregion_it) : static_cast<Region<I>*>(*subregion_it),
                            index
                        };
                    };
                    const bool operator==(const total_iterator& rhs) const {
                        return index == rhs.index;
                    };
                    const bool operator!=(const total_iterator& rhs) const {
                        return !(*this == rhs);
                    };
            };
            total_iterator tbegin() const {
                return total_iterator::begin(*this);
            };
            total_iterator tend() const {
                return total_iterator::end(*this);
            };
            class TotalIndices {
                protected:
                    const IndexSet& index_set;
                public:
                    TotalIndices(const IndexSet& index_set) : index_set(index_set) {};
                    total_iterator begin() const {
                        return index_set.tbegin();
                    };
                    total_iterator end() const {
                        return index_set.tend();
                    };
            };
            const TotalIndices total_indices;

            class super_iterator {
                private:
                    const IndexSet* parent;
                    typename vector<SuperSector<I>*>::const_iterator sector_it;
                    typename vector<SuperRegion<I>*>::const_iterator region_it;
                    super_iterator(const IndexSet* parent) : parent(parent) {};
                public:
                    struct Index {
                        const SuperSector<I>* sector;
                        const SuperRegion<I>* region;
                    };
                    typedef std::forward_iterator_tag iterator_category;
                    static super_iterator begin(const IndexSet* parent) {
                        super_iterator res(parent);
                        res.region_it = parent->superregions().begin();
                        if (res.region_it != parent->superregions().end()) {
                            res.sector_it = (*res.region_it)->sectors().begin();
                        }
                        return res;
                    };
                    static super_iterator end(const IndexSet* parent) {
                        super_iterator res(parent);
                        res.region_it = parent->superregions().end();
                        return res;
                    };
                    super_iterator operator++() {
                        sector_it++;
                        if (sector_it == (*region_it)->sectors().end()) {
                            region_it++;
                            if (region_it != parent->superregions().end()) {
                                sector_it = (*region_it)->sectors().begin();
                            }
                        }
                        return *this;
                    };
                    const Index operator*() const {
                        return { *sector_it, *region_it };
                    };
                    const bool operator==(const super_iterator& rhs) const {
                        return (region_it == rhs.region_it && sector_it == rhs.sector_it)
                               || (region_it == parent->superregions().end() && rhs.region_it == rhs.parent->superregions().end());
                    };
                    const bool operator!=(const super_iterator& rhs) const {
                        return !(*this == rhs);
                    };
            };
            super_iterator sbegin() const {
                return super_iterator::begin(this);
            };
            super_iterator send() const {
                return super_iterator::end(this);
            };
            class SuperIndices {
                protected:
                    const IndexSet& index_set;
                public:
                    SuperIndices(const IndexSet& index_set) : index_set(index_set) {};
                    super_iterator begin() const {
                        return index_set.sbegin();
                    };
                    super_iterator end() const {
                        return index_set.send();
                    };
            };
            const SuperIndices super_indices;

            IndexSet()
                : size_(0), total_regions_count_(0), total_sectors_count_(0),
                  total_indices(*this), super_indices(*this) {};
            IndexSet(const IndexSet& other);
            IndexSet& operator=(const IndexSet& other);
            const I& size() const {
                return size_;
            };
            const vector<SuperSector<I>*>& supersectors() const {
                return supersectors_;
            };
            const vector<SuperRegion<I>*>& superregions() const {
                return superregions_;
            };
            const vector<SubSector<I>*>& subsectors() const {
                return subsectors_;
            };
            const vector<SubRegion<I>*>& subregions() const {
                return subregions_;
            };
            const Sector<I>* sector(const string& name) const {
                return sectors_map.at(name);
            };
            const Region<I>* region(const string& name) const {
                return regions_map.at(name);
            };
            void clear();
            virtual ~IndexSet() {
                clear();
            };
            void add_index(const string& sector_name, const string& region_name);
            void rebuild_indices();
            inline const I& at(const Sector<I>* sector, const Region<I>* region) const {
                assert(!sector->has_sub());
                assert(!region->has_sub());
                return indices_.at(*sector * total_regions_count_ + *region);
            };
            inline const I& operator()(const Sector<I>* sector, const Region<I>* region) const noexcept {
                assert(*sector * total_regions_count_ + *region >= 0);
                assert(*sector * total_regions_count_ + *region < indices_.size());
                assert(!sector->has_sub());
                assert(!region->has_sub());
                return indices_[*sector * total_regions_count_ + *region];
            };
            void insert_subsectors(const string& name, const vector<string>& newsubsectors);
            void insert_subregions(const string& name, const vector<string>& newsubregions);

            /**
             * @brief Returns index for Sector-Region-Combination of a foreign IndexSet
             *        that is a disaggregated version of this non-disaggregated IndexSet
             *
             * @param sector Sector (from disaggregated IndexSet)
             * @param region Region (from disaggregated IndexSet)
             * @return Reference to index (in this non-disaggregated IndexSet)
             */
            inline const I& base(const SuperSector<I>* sector, const SuperRegion<I>* region) const noexcept {
                assert(sector->level_index() * superregions_.size() + region->level_index() >= 0);
                assert(sector->level_index() * superregions_.size() + region->level_index() < indices_.size());
                return indices_[sector->level_index() * superregions_.size() + region->level_index()];
            };
    };
}

#endif /* MRIOINDEXSET_H_ */
