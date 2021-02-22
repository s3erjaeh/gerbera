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

#ifndef __PY_SCRIPTING_SCRIPT_H__
#define __PY_SCRIPTING_SCRIPT_H__

#ifdef HAVE_PY

#include <mutex>
#include <filesystem>

#include "common.h"
#include "context.h"
#include "py_runtime.h"

// forward declaration
class CdsObject;
class ContentManager;
class PyScriptingRuntime;
class StringConverter;


class PyImportScript {
public:
    PyImportScript(std::shared_ptr<ContentManager> content);
    ~PyImportScript() = default;
    void processCdsObject( const std::shared_ptr<CdsObject>& obj, const std::string& scriptpath);
    int addCdsObject(const std::shared_ptr<CdsObject>& orgObj,CdsObject& obj, const std::vector<std::string>&, std::string containerclass);

    std::shared_ptr<Config> getConfig() const { return config; }
    std::shared_ptr<ContentManager> getContent() const { return content; }

protected:
    std::unique_ptr<PyRuntime> runtime;

    std::shared_ptr<Config> config;
    std::shared_ptr<ContentManager> content;

private:
    static PyImportScript* context;
};
#endif
#endif // __PY_SCRIPTING_SCRIPT_H__
