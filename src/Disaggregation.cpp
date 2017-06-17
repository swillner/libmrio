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

#include "Disaggregation.h"
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace mrio {

template<typename T, typename I>
Disaggregation<T, I>::Disaggregation(const mrio::Table<T, I>* basetable_p) : basetable(basetable_p) {
    table.reset(new Table<T, I>(*basetable));
}

static inline const std::string readStr(std::istringstream& ss) {
    std::string res;
    if (!getline(ss, res, ',')) {
        throw std::runtime_error("Not enough columns");
    }
    return res;
}

template<typename T, typename I>
inline const I Disaggregation<T, I>::readI(std::istringstream& ss) {
    return std::stoi(readStr(ss).c_str());
}

template<typename T, typename I>
inline const T Disaggregation<T, I>::readT(std::istringstream& ss) {
    return std::stof(readStr(ss).c_str());
}

template<typename T, typename I>
inline const T Disaggregation<T, I>::readT_optional(std::istringstream& ss) {
    std::string str;
    if (getline(ss, str, ',')) {
        return std::stof(str.c_str());
    } else {
        return std::numeric_limits<T>::quiet_NaN();
    }
}

template<typename T, typename I>
void Disaggregation<T, I>::read_disaggregation_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open disaggregation file");
    }
    std::string line;
    int l = 0;
    try {
        while (getline(file, line)) {
            l++;
            if (line[0] == '#') {  // ignore comment lines
                continue;
            }
            std::istringstream ss(line);
            std::string type = readStr(ss);
            std::string name = readStr(ss);
            I cnt = readI(ss);
            if (cnt < 1) {
                throw std::runtime_error("Invalid subsector/subregion count");
            }
            std::vector<std::string> subs;
            subs.reserve(cnt);
            for (I it = 0; it < cnt; it++) {
                subs.push_back(name + std::to_string(it));
            }
            if (type == "sector") {
                try {
                    table->insert_subsectors(name, subs);
                } catch (std::out_of_range& ex) {
                    throw std::runtime_error("Sector '" + name + "' not found");
                }
            } else if (type == "region") {
                try {
                    table->insert_subregions(name, subs);
                } catch (std::out_of_range& ex) {
                    throw std::runtime_error("Region '" + name + "' not found");
                }
            } else {
                throw std::runtime_error("Unknown type");
            }
        }
    } catch (const std::runtime_error& ex) {
        std::ostringstream s;
        s << ex.what() << " (in " << filename << ", line " << l << ")";
        throw std::runtime_error(s.str());
    } catch (const std::exception& ex) {
        std::ostringstream s;
        s << "Could not parse number"
          << " (in " << filename << ", line " << l << ")";
        throw std::runtime_error(s.str());
    }
}

template<typename T, typename I>
const Sector<I>* Disaggregation<T, I>::readSector(std::istringstream& ss) {
    std::string str;
    try {
        str = readStr(ss);
        return table->index_set().sector(str);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("Sector '" + str + "' not found");
    }
}

template<typename T, typename I>
const Region<I>* Disaggregation<T, I>::readRegion(std::istringstream& ss) {
    std::string str;
    try {
        str = readStr(ss);
        return table->index_set().region(str);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("Region '" + str + "' not found");
    }
}

template<typename T, typename I>
const SubSector<I>* Disaggregation<T, I>::readSubsector(std::istringstream& ss) {
    const Sector<I>* sector = readSector(ss);
    const SuperSector<I>* supersector = sector->as_super();
    if (!supersector) {
        throw std::runtime_error("Sector '" + sector->name + "' is not a super sector");
    }
    I id = readI(ss);
    try {
        return supersector->sub().at(id);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("SubSector '" + sector->name + std::to_string(id) + "' not found");
    }
}

template<typename T, typename I>
const SubRegion<I>* Disaggregation<T, I>::readSubregion(std::istringstream& ss) {
    const Region<I>* region = readRegion(ss);
    const SuperRegion<I>* superregion = region->as_super();
    if (!superregion) {
        throw std::runtime_error("Region '" + region->name + "' is not a super region");
    }
    I id = readI(ss);
    try {
        return superregion->sub().at(id);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("SubRegion '" + region->name + std::to_string(id) + "' not found");
    }
}

template<typename T, typename I>
void Disaggregation<T, I>::read_proxy_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open proxy file");
    }
    std::string line;
    int l = 0;
    const I& subsectors_count = table->index_set().subsectors().size();
    const I& subregions_count = table->index_set().subregions().size();
    const I& sectors_count = table->index_set().supersectors().size();
    const I& regions_count = table->index_set().superregions().size();
    try {
        if (!getline(file, line)) {
            return;
        }
        unsigned char d;
        do {  // while (getline(file, line))
            l++;
            if (line[0] == '#') {
                continue;
            }
            std::istringstream ss(line);
            d = readI(ss);
            switch (d) {
                case LEVEL_POPULATION_1:
                case LEVEL_GDP_SUBREGION_2: {
                    const SubRegion<I>* r_lambda = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subregions_count));
                        proxy_sums[d].reset(new ProxyData(regions_count));
                    }
                    (*proxies[d])(r_lambda) = readT(ss);
                    (*proxy_sums[d])(r_lambda->parent()) = readT_optional(ss);
                } break;
                case LEVEL_GDP_SUBSECTOR_3: {
                    const SubSector<I>* i_mu = readSubsector(ss);
                    const Region<I>* r = readRegion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, regions_count));
                        proxy_sums[d].reset(new ProxyData(sectors_count, regions_count));
                    }
                    (*proxies[d])(i_mu, r) = readT(ss);
                    (*proxy_sums[d])(i_mu->parent(), r) = readT_optional(ss);
                } break;
                case LEVEL_GDP_SUBREGIONAL_SUBSECTOR_4: {
                    const SubSector<I>* i_mu = readSubsector(ss);
                    const SubRegion<I>* r_lambda = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count));
                        proxy_sums[d].reset(new ProxyData(sectors_count, regions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda) = readT(ss);
                    (*proxy_sums[d])(i_mu->parent(), r_lambda->parent()) = readT_optional(ss);
                } break;
                case LEVEL_IMPORT_SUBSECTOR_5: {
                    const SubSector<I>* i_mu = readSubsector(ss);
                    const Region<I>* s = readRegion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, regions_count));
                    }
                    (*proxies[d])(i_mu, s) = readT(ss);
                } break;
                case LEVEL_IMPORT_SUBREGION_6: {
                    const Sector<I>* j = readSector(ss);
                    const SubRegion<I>* r_lambda = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(sectors_count, subregions_count));
                    }
                    (*proxies[d])(j, r_lambda) = readT(ss);
                } break;
                case LEVEL_INTERREGIONAL_SUBSECTOR_INPUT_7: {
                    const SubSector<I>* i_mu = readSubsector(ss);
                    const SubRegion<I>* r_lambda = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda) = readT(ss);
                } break;
                case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_8: {
                    const SubSector<I>* i_mu = readSubsector(ss);
                    const SubRegion<I>* r_lambda = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda) = readT(ss);
                } break;
                case LEVEL_IMPORT_SUBSECTOR_BY_REGIONAL_SECTOR_9: {
                    const SubSector<I>* i_mu = readSubsector(ss);
                    const Sector<I>* j = readSector(ss);
                    const Region<I>* s = readRegion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, sectors_count, regions_count));
                    }
                    (*proxies[d])(i_mu, j, s) = readT(ss);
                } break;
                case LEVEL_EXPORT_SUBREGION_10: {
                    const Sector<I>* j = readSector(ss);
                    const Region<I>* s = readRegion(ss);
                    const SubRegion<I>* r_lambda = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(sectors_count, regions_count, subregions_count));
                    }
                    (*proxies[d])(j, s, r_lambda) = readT(ss);
                } break;
                case LEVEL_SUBREGIONAL_SUBSECTOR_INPUT_11: {
                    const SubSector<I>* i_mu1 = readSubsector(ss);
                    const SubSector<I>* i_mu2 = readSubsector(ss);
                    const SubRegion<I>* r_lambda = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subsectors_count, subregions_count));
                    }
                    (*proxies[d])(i_mu1, i_mu2, r_lambda) = readT(ss);
                } break;
                case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_REGION_12: {
                    const SubSector<I>* i_mu = readSubsector(ss);
                    const SubRegion<I>* r_lambda = readSubregion(ss);
                    const Region<I>* s = readRegion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count, regions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda, s) = readT(ss);
                } break;
                case LEVEL_IMPORT_SUBREGIONAL_SUBSECTOR_13: {
                    const Sector<I>* j = readSector(ss);
                    const SubSector<I>* i_mu = readSubsector(ss);
                    const SubRegion<I>* r_lambda = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(sectors_count, subsectors_count, subregions_count));
                    }
                    (*proxies[d])(j, i_mu, r_lambda) = readT(ss);
                } break;
                case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_SUBREGION_14: {
                    const SubSector<I>* i_mu = readSubsector(ss);
                    const SubRegion<I>* r_lambda1 = readSubregion(ss);
                    const SubRegion<I>* r_lambda2 = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count, subregions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda1, r_lambda2) = readT(ss);
                } break;
                case LEVEL_PETERS1_15:
                case LEVEL_PETERS2_16:
                case LEVEL_PETERS3_17:
                    throw std::runtime_error("Levels 15, 16, 17 cannot be given explicitly");
                    break;
                case LEVEL_EXACT_18: {
                    const SubSector<I>* i_mu1 = readSubsector(ss);
                    const SubRegion<I>* r_lambda1 = readSubregion(ss);
                    const SubSector<I>* i_mu2 = readSubsector(ss);
                    const SubRegion<I>* r_lambda2 = readSubregion(ss);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count, subsectors_count, subregions_count));
                    }
                    (*proxies[d])(i_mu1, r_lambda1, i_mu2, r_lambda2) = readT(ss);
                } break;
                default:
                    throw std::runtime_error("Unknown d-level");
                    break;
            }
        } while (getline(file, line));
    } catch (const std::runtime_error& ex) {
        std::stringstream s;
        s << ex.what();
        s << " (in " << filename << ", line " << l << ")";
        throw std::runtime_error(s.str());
    } catch (const std::exception& ex) {
        std::stringstream s;
        s << "Could not parse number";
        s << " (in " << filename << ", line " << l << ")";
        throw std::runtime_error(s.str());
    }
}

template<typename T, typename I>
void Disaggregation<T, I>::refine() {
    last_table.reset(new Table<T, I>(*table));
    quality.reset(new Table<unsigned char, I>(table->index_set(), 0));

    // apply actual algorithm
    for (unsigned char d = 1; d < PROXY_COUNT; d++) {
        if (proxies[d]
            || (d == LEVEL_PETERS1_15 && proxies[LEVEL_IMPORT_SUBSECTOR_5] && proxies[LEVEL_IMPORT_SUBSECTOR_BY_REGIONAL_SECTOR_9]
                && proxies[LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_REGION_12])
            || (d == LEVEL_PETERS2_16 && proxies[LEVEL_IMPORT_SUBREGION_6] && proxies[LEVEL_EXPORT_SUBREGION_10]
                && proxies[LEVEL_IMPORT_SUBREGIONAL_SUBSECTOR_13])
            || (d == LEVEL_PETERS3_17 && proxies[LEVEL_INTERREGIONAL_SUBSECTOR_INPUT_7] && proxies[LEVEL_SUBREGIONAL_SUBSECTOR_INPUT_11]
                && proxies[LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_SUBREGION_14])) {
            last_table->replace_table_from(*table);
            approximate(d);
            adjust(d);
        }
    }

    // cleanup
    quality.reset();
    last_table.reset();
}

template<typename T, typename I>
void for_all_sub(
    const Sector<I>* i,
    const Region<I>* r,
    const Sector<I>* j,
    const Region<I>* s,
    std::function<void(
        const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub, const bool j_sub, const bool s_sub)>
        func,
    const bool i_sub = false,
    const bool r_sub = false,
    const bool j_sub = false,
    const bool s_sub = false) {
    if (i->has_sub()) {
        for (const auto& i_mu : i->as_super()->sub()) {
            for_all_sub<T, I>(i_mu, r, j, s, func, true, r_sub, j_sub, s_sub);
        }
    } else if (r->has_sub()) {
        for (const auto& r_lambda : r->as_super()->sub()) {
            for_all_sub<T, I>(i, r_lambda, j, s, func, i_sub, true, j_sub, s_sub);
        }
    } else if (j->has_sub()) {
        for (const auto& j_mu : j->as_super()->sub()) {
            for_all_sub<T, I>(i, r, j_mu, s, func, i_sub, r_sub, true, s_sub);
        }
    } else if (s->has_sub()) {
        for (const auto& s_lambda : s->as_super()->sub()) {
            for_all_sub<T, I>(i, r, j, s_lambda, func, i_sub, r_sub, j_sub, true);
        }
    } else {
        func(i, r, j, s, i_sub, r_sub, j_sub, s_sub);
    }
}

template<typename T, typename I>
void Disaggregation<T, I>::approximate(const unsigned char& d) {
    switch (d) {
        case LEVEL_POPULATION_1:

        case LEVEL_GDP_SUBREGION_2:

            for (const auto& r : table->index_set().superregions()) {
                if (std::isnan((*proxy_sums[d])(r.get()))) {
                    T sum = 0;
                    for (const auto& r_lambda : r->sub()) {
                        sum += (*proxies[d])(r_lambda);
                    }
                    (*proxy_sums[d])(r.get()) = sum;
                }
            }
            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(
                        ir.sector, ir.region, js.sector, js.region,
                        [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                            const bool j_sub, const bool s_sub) {
                            T sum1 = 1, value1 = 1, sum2 = 1, value2 = 1;
                            if (r_sub) {
                                sum1 = (*proxy_sums[d])(r->parent());
                                value1 = (*proxies[d])(r);
                            }
                            if (s_sub) {
                                sum2 = (*proxy_sums[d])(s->parent());
                                value2 = (*proxies[d])(s);
                            }
                            if (r_sub || s_sub) {
                                if (!std::isnan(value1) && !std::isnan(sum1) && sum1 > 0 && !std::isnan(value2) && !std::isnan(sum2) && sum2 > 0) {
                                    (*table)(i, r, j, s) = last_table->sum(i, r->super(), j, s->super()) * value1 * value2 / sum1 / sum2;
                                    (*quality)(i, r, j, s) = d;
                                }
                            }
                        });
                }
            }
            break;

        case LEVEL_GDP_SUBSECTOR_3:

            for (const auto& i : table->index_set().supersectors()) {
                if (i->has_sub()) {
                    for (const auto& r : i->regions()) {
                        if (std::isnan((*proxy_sums[d])(i.get(), r))) {
                            T sum = 0;
                            for (const auto& i_mu : i->sub()) {
                                sum += (*proxies[d])(i_mu, r);
                            }
                            (*proxy_sums[d])(i.get(), r) = sum;
                        }
                    }
                }
            }
            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(
                        ir.sector, ir.region, js.sector, js.region,
                        [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                            const bool j_sub, const bool s_sub) {
                            T sum1 = 1, value1 = 1, sum2 = 1, value2 = 1;
                            if (i_sub) {
                                sum1 = (*proxy_sums[d])(i->parent(), r->super());
                                value1 = (*proxies[d])(i, r->super());
                            }
                            if (j_sub) {
                                sum2 = (*proxy_sums[d])(j->parent(), s->super());
                                value2 = (*proxies[d])(j, s->super());
                            }
                            if (i_sub || j_sub) {
                                if (!std::isnan(value1) && !std::isnan(sum1) && sum1 > 0 && !std::isnan(value2) && !std::isnan(sum2) && sum2 > 0) {
                                    (*table)(i, r, j, s) = last_table->sum(i->super(), r, j->super(), s) * value1 * value2 / sum1 / sum2;
                                    (*quality)(i, r, j, s) = d;
                                }
                            }
                        });
                }
            }
            break;

        case LEVEL_GDP_SUBREGIONAL_SUBSECTOR_4:

            for (const auto& i : table->index_set().supersectors()) {
                if (i->has_sub()) {
                    for (const auto& r : i->regions()) {
                        if (r->has_sub()) {
                            if (std::isnan((*proxy_sums[d])(i.get(), r))) {
                                T sum = 0;
                                for (const auto& i_mu : i->sub()) {
                                    for (const auto& r_lambda : i->sub()) {
                                        sum += (*proxies[d])(i_mu, r_lambda);
                                    }
                                }
                                (*proxy_sums[d])(i.get(), r) = sum;
                            }
                        }
                    }
                }
            }
            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(
                        ir.sector, ir.region, js.sector, js.region,
                        [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                            const bool j_sub, const bool s_sub) {
                            T sum1 = 1, value1 = 1, sum2 = 1, value2 = 1;
                            if (i_sub && r_sub) {
                                sum1 = (*proxy_sums[d])(i->parent(), r->parent());
                                value1 = (*proxies[d])(i, r);
                            }
                            if (j_sub && s_sub) {
                                sum2 = (*proxy_sums[d])(j->parent(), s->parent());
                                value2 = (*proxies[d])(j, s);
                            }
                            if ((i_sub && r_sub) || (j_sub && s_sub)) {
                                if (!std::isnan(value1) && !std::isnan(sum1) && sum1 > 0 && !std::isnan(value2) && !std::isnan(sum2) && sum2 > 0) {
                                    (*table)(i, r, j, s) = last_table->sum(i->super(), r->super(), j->super(), s->super()) * value1 * value2 / sum1 / sum2;
                                    (*quality)(i, r, j, s) = d;
                                }
                            }
                        });
                }
            }
            break;

        case LEVEL_IMPORT_SUBSECTOR_5:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub) {
                                              const T& value = (*proxies[d])(i, s->super());
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r, j, s) * value
                                                                         / last_table->sum(i->parent(), nullptr, nullptr, s->super());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_IMPORT_SUBREGION_6:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (s_sub) {
                                              const T& value = (*proxies[d])(i->super(), s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i, r, j, s->parent()) * value
                                                                         / last_table->sum(i->super(), nullptr, nullptr, s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_INTERREGIONAL_SUBSECTOR_INPUT_7:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub && s_sub) {
                                              const T& value = (*proxies[d])(i, s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r, j, s->parent()) * value
                                                                         / last_table->sum(i->parent(), nullptr, nullptr, s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_8:

            for (const auto& ir : table->index_set().super_indices) {
                const T& sum = basetable->basesum(ir.sector, ir.region, nullptr, nullptr);
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub && r_sub && j_sub == s_sub) {
                                              const T& value = (*proxies[d])(i, r);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r->parent(), j, s) * value / sum;
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_IMPORT_SUBSECTOR_BY_REGIONAL_SECTOR_9:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub) {
                                              const T& value = (*proxies[d])(i, j->super(), s->super());
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r, j, s) * value
                                                                         / last_table->sum(i->parent(), nullptr, j->super(), s->super());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_EXPORT_SUBREGION_10:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (s_sub) {
                                              const T& value = (*proxies[d])(i->super(), r->super(), s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i, r, j, s->parent()) * value
                                                                         / last_table->sum(i->super(), r->super(), nullptr, s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_SUBREGIONAL_SUBSECTOR_INPUT_11:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub && j_sub && s_sub) {
                                              const T& value = (*proxies[d])(i, j, s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r, j->parent(), s->parent()) * value
                                                                         / last_table->sum(i->parent(), nullptr, j->parent(), s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_REGION_12:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub && r_sub) {
                                              const T& value = (*proxies[d])(i, r, s->super());
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r->parent(), j, s) * value
                                                                         / last_table->sum(i->parent(), r->parent(), nullptr, s->super());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_IMPORT_SUBREGIONAL_SUBSECTOR_13:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (j_sub && s_sub) {
                                              const T& value = (*proxies[d])(i->super(), j, s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i, r, j->parent(), s->parent()) * value
                                                                         / last_table->sum(i->super(), nullptr, j->parent(), s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_SUBREGION_14:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub && r_sub && s_sub) {
                                              const T& value = (*proxies[d])(i, r, s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r->parent(), j, s->parent()) * value
                                                                         / last_table->sum(i->parent(), r->parent(), nullptr, s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_PETERS1_15:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub && r_sub) {
                                              const T& value1 = (*proxies[LEVEL_IMPORT_SUBSECTOR_5])(i, s->super());
                                              const T& value2 = (*proxies[LEVEL_IMPORT_SUBSECTOR_BY_REGIONAL_SECTOR_9])(i, j->super(), s->super());
                                              const T& value3 = (*proxies[LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_REGION_12])(i, r, s->super());
                                              if (!std::isnan(value1) && !std::isnan(value2) && !std::isnan(value3)) {
                                                  (*table)(i, r, j, s) = value2 * value3 / value1;
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_PETERS2_16:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (j_sub && s_sub) {
                                              const T& value1 = (*proxies[LEVEL_IMPORT_SUBREGION_6])(i->super(), s);
                                              const T& value2 = (*proxies[LEVEL_EXPORT_SUBREGION_10])(i->super(), r->super(), s);
                                              const T& value3 = (*proxies[LEVEL_IMPORT_SUBREGIONAL_SUBSECTOR_13])(i->super(), j, s);
                                              if (!std::isnan(value1) && !std::isnan(value2) && !std::isnan(value3)) {
                                                  (*table)(i, r, j, s) = value3 * value2 / value1;
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_PETERS3_17:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub && r_sub && j_sub && s_sub) {
                                              const T& value1 = (*proxies[LEVEL_INTERREGIONAL_SUBSECTOR_INPUT_7])(i, s);
                                              const T& value2 = (*proxies[LEVEL_SUBREGIONAL_SUBSECTOR_INPUT_11])(i, j, s);
                                              const T& value3 = (*proxies[LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_SUBREGION_14])(i, r, s);
                                              if (!std::isnan(value1) && !std::isnan(value2) && !std::isnan(value3)) {
                                                  (*table)(i, r, j, s) = value2 * value3 / value1;
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_EXACT_18:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if (i_sub && r_sub && j_sub && s_sub) {
                                              const T& value = (*proxies[d])(i, r, j, s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = value;
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        default:

            throw std::runtime_error("Unknown d-level");
            break;
    }
}

template<typename T, typename I>
void Disaggregation<T, I>::adjust(const unsigned char& d) {
    for (const auto& ir : table->index_set().super_indices) {
        for (const auto& js : table->index_set().super_indices) {
            const T& base = basetable->base(ir.sector, ir.region, js.sector, js.region);
            if (base > 0) {
                T sum_of_exact = 0;
                T sum_of_non_exact = 0;
                for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                  [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                      const bool j_sub, const bool s_sub) {
                                      if ((*quality)(i, r, j, s) == d) {
                                          sum_of_exact += (*table)(i, r, j, s);
                                      } else {
                                          sum_of_non_exact += (*table)(i, r, j, s);
                                      }
                                  });
                T correction_factor = base / (sum_of_exact + sum_of_non_exact);
                if (base > sum_of_exact && sum_of_non_exact > 0) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) {
                                          if ((*quality)(i, r, j, s) != d) {
                                              (*table)(i, r, j, s) = (base - sum_of_exact) * (*table)(i, r, j, s) / sum_of_non_exact;
                                          }
                                      });
                } else if (correction_factor < 1 || correction_factor > 1) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, const bool i_sub, const bool r_sub,
                                          const bool j_sub, const bool s_sub) { (*table)(i, r, j, s) = correction_factor * (*table)(i, r, j, s); });
                }
            }
        }
    }
}

template class Disaggregation<double, size_t>;
template class Disaggregation<float, size_t>;
}
