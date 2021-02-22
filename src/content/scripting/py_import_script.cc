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

#ifdef HAVE_PY
#include "py_import_script.h" // API
#include "py_runtime.h" // API
#include "cds_objects.h"
#include "config/config_manager.h"
#include "content/content_manager.h"
#include "metadata/metadata_handler.h"
#include "script_names.h"
#include "util/string_converter.h"
#include "util/tools.h"

PyImportScript::PyImportScript(std::shared_ptr<ContentManager> content)
    : runtime(new PyRuntime(content->getContext()->getConfig()->getOption(CFG_IMPORT_SCRIPTING_IMPORT_SCRIPT))),
      config(content->getContext()->getConfig()),
      content(std::move(content))
{
}

int PyImportScript::addCdsObject(const std::shared_ptr<CdsObject>& orgObj, CdsObject& obj, const std::vector<std::string>& chain, std::string containerclass)
{
    try {
        auto toBeAdded = CdsObject::createObject(orgObj->getObjectType());
        obj.copyTo(toBeAdded);
        toBeAdded->setVirtual(true);

        auto path = std::string("/").append(fmt::format("{}", fmt::join(chain, "/")));
        int parentId = getContent()->addContainerChain(path, containerclass, INVALID_OBJECT_ID, orgObj);
        toBeAdded->setParentID(parentId);
        toBeAdded->setRefID(orgObj->getID());
        toBeAdded->setFlag(OBJECT_FLAG_USE_RESOURCE_REF);
        toBeAdded->setID(INVALID_OBJECT_ID);
        getContent()->addObject(toBeAdded);
        log_debug("returning for: {}", obj.getLocation().string());
        return toBeAdded->getID();

    } catch (const ServerShutdownException& se) {
        log_warning("Aborting script execution due to server shutdown.");
    } catch (const std::runtime_error& e) {
        log_error("{}", e.what());
    }
    return 0;
}

void PyImportScript::processCdsObject(const std::shared_ptr<CdsObject>& obj, const std::string& importpath)
{
    std::weak_ptr<CdsObject> org_obj(obj);
    runtime->processCdsObject(obj, importpath, [this, org_obj](CdsObject& obj, const std::vector<std::string>& chain, std::string containerclass) {
        return addCdsObject(org_obj.lock(),obj, chain, containerclass);
    });
}
#endif // HAVE_PY
