/*

	Copyright (C) 2014 Etienne Dechamps (e-t172) <etienne@edechamps.fr>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include <windows.h>

#include <string>
#include <sstream>

namespace flexasio {

	class Log
	{
	private:
		std::stringstream ss;

	public:
		Log()
		{
			ss << "FlexASIO: [" << timeGetTime() << "] ";
		}

		~Log()
		{
			ss << std::endl;
			OutputDebugString(ss.str().c_str());
		}

		template <typename T> friend Log& operator<<(Log& lhs, T&& rhs) {
			lhs.ss << std::forward<T>(rhs);
			return lhs;
		}
	};

}
