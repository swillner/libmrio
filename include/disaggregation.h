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

#ifndef LIBMRIO_DISAGGREGATION_H
#define LIBMRIO_DISAGGREGATION_H

namespace settings {
class SettingsNode;
}

namespace mrio {

template<typename T, typename I>
class Table;

template<typename T, typename I>
mrio::Table<T, I> disaggregate(const mrio::Table<T, I>& basetable, const settings::SettingsNode& settings);
}  // namespace mrio

#endif
