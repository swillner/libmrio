#ifndef MRIOT_H_
#define MRIOT_H_

#include "MRIOIndexSet.h"
#include <string>
#include <vector>
#include <limits>
#include <deque>
#ifdef DEBUG
#include <cassert>
#else
#define assert(a) {}
#endif

namespace mrio {

    using std::deque;
    using std::istream;
    using std::ostream;
    using std::numeric_limits;

    template<typename T, typename I> class Table {
        protected:
            deque<T> data;
            IndexSet<I> index_set_;

            void read_indices_from_csv(istream& indicesstream);
            void read_data_from_csv(istream& datastream, const T& threshold);
            void insert_sector_offset_x_y(const SuperSector<I>* i, const I& i_regions_count, const I& subsectors_count);
            void insert_sector_offset_y(const SuperSector<I>* i, const I& i_regions_count, const I& subsectors_count, const I& x, const I& x_offset, const I& divide_by);
            void insert_region_offset_x_y(const SuperRegion<I>* r, const I& r_sectors_count, const I& subregions_count);
            void insert_region_offset_y(const SuperRegion<I>* r, const I& r_sectors_count, const I& subregions_count, const I& x, const I& x_offset, const I& divide_by,
                                        const I& first_index, const I& last_index);

        public:
            Table() {};
            Table(const IndexSet<I>& index_set_p, const T default_value_p = numeric_limits<T>::signaling_NaN()) : index_set_(index_set_p) {
                data.resize(index_set_.size() * index_set_.size(), default_value_p);
            };
            inline const IndexSet<I>& index_set() const {
                return index_set_;
            };
            void insert_subsectors(const string& name, const vector<string>& subsectors);
            void insert_subregions(const string& name, const vector<string>& subsectors);
            const T sum(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept;
            const T basesum(const SuperSector<I>* i, const SuperRegion<I>* r, const SuperSector<I>* j, const SuperRegion<I>* s) const noexcept;
            void write_to_csv(ostream& os) const;
            void write_to_mrio(ostream& os) const;
            void read_from_csv(istream& indicesstream, istream& datastream, const T& threshold);
            void read_from_mrio(istream& instream, const T& threshold);

            inline T& at(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
                assert(index_set_.at(i, r) >= 0);
                assert(index_set_.at(j, s) >= 0);
                return at(index_set_.at(i, r), index_set_.at(j, s));
            };
            inline const T& at(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const {
                assert(index_set_.at(i, r) >= 0);
                assert(index_set_.at(j, s) >= 0);
                return at(index_set_.at(i, r), index_set_.at(j, s));
            };
            inline T& operator()(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) noexcept {
                assert(index_set_.at(i, r) >= 0);
                assert(index_set_.at(j, s) >= 0);
                return (*this)(index_set_(i, r), index_set_(j, s));
            };
            inline const T& operator()(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
                assert(index_set_.at(i, r) >= 0);
                assert(index_set_.at(j, s) >= 0);
                return (*this)(index_set_(i, r), index_set_(j, s));
            };
            inline T& at(const I& from, const I& to) {
                assert(from >= 0);
                assert(to >= 0);
                return data.at(from * index_set_.size() + to);
            };
            inline const T& at(const I& from, const I& to) const {
                assert(from >= 0);
                assert(to >= 0);
                return data.at(from * index_set_.size() + to);
            };
            inline T& operator()(const I& from, const I& to) noexcept {
                assert(from >= 0);
                assert(to >= 0);
                assert(from * index_set_.size() + to < data.size());
                return data[from * index_set_.size() + to];
            };
            inline const T& operator()(const I& from, const I& to) const noexcept {
                assert(from >= 0);
                assert(to >= 0);
                assert(from * index_set_.size() + to < data.size());
                return data[from * index_set_.size() + to];
            };
            /**
             * @brief Returns reference to value with Sectors/Regions of a foreign Table
             *        that is a disaggregated version of this non-disaggregated Table
             *
             * @return Reference to value
             */
            inline T& base(const SuperSector<I>* i, const SuperRegion<I>* r, const SuperSector<I>* j, const SuperRegion<I>* s) noexcept {
                assert(index_set_.base(i, r) >= 0);
                assert(index_set_.base(j, s) >= 0);
                return (*this)(index_set_.base(i, r), index_set_.base(j, s));
            };
            inline const T& base(const SuperSector<I>* i, const SuperRegion<I>* r, const SuperSector<I>* j, const SuperRegion<I>* s) const noexcept {
                assert(index_set_.base(i, r) >= 0);
                assert(index_set_.base(j, s) >= 0);
                return (*this)(index_set_.base(i, r), index_set_.base(j, s));
            };
            void replace_table_from(const Table& other) {
                data = other.data;
            };
            const deque<T>& raw_data() const {
                return data;
            }
            void debug_out() const; // TODO
    };
}

#endif /* MRIOT_H_ */
