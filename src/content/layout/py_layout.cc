/// \file js_layout.cc

#ifdef HAVE_PY
#include "py_layout.h" // API

#include "content/scripting/py_import_script.h"

PyLayout::PyLayout(const std::shared_ptr<ContentManager>& content)
    : Layout(content)
    , import_script(std::make_unique<PyImportScript>(content))
{
}

PyLayout::~PyLayout() = default;

void PyLayout::processCdsObject(std::shared_ptr<CdsObject> obj, fs::path rootpath)
{
    if (import_script == nullptr)
        return;

    import_script->processCdsObject(obj, rootpath);
}

#endif // HAVE_PY
