/*GRB*
    Gerbera - https://gerbera.io/

    runtime_test.cc - this file is part of Gerbera.

    Copyright (C) 2016-2021 Gerbera Contributors

    Gerbera is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    Gerbera is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Gerbera.  If not, see <http://www.gnu.org/licenses/>.

    $Id$
*/

#ifndef __PY_RUNTIME_H__
#define __PY_RUNTIME_H__

#ifdef HAVE_PY

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <mutex>
#include <filesystem>

#include "common.h"
#include "cds_objects.h"

namespace py = pybind11;
using t_addCdsObject = std::function<int(CdsObject&,const std::vector<std::string>&, std::string)>;

class PyRuntime
{
public:
    PyRuntime(std::string scriptpath);
    ~PyRuntime() = default;

protected:
    py::scoped_interpreter guard {};
    py::module importModule;

private:
    static t_addCdsObject callback;

public:    
    void processCdsObject(const std::shared_ptr<CdsObject>& obj, const std::string& scriptpath, t_addCdsObject callback);
    static int addCdsObject(CdsObject& obj, const std::vector<std::string>& chain, std::string containerclass);
};
#endif
#endif // __PY_RUNTIME_H__
