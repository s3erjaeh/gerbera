/// \file script.cc

#ifdef HAVE_PY
#include "py_runtime.h" // API
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include "cds_objects.h"
#include <filesystem>

#include "script_names.h"

#ifdef ONLINE_SERVICES
#include "content/onlineservice/online_service.h"
#endif

#ifdef ATRAILERS
#include "content/onlineservice/atrailers_content_handler.h"
#endif

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(gerbera, m)
{
    m.def("log", [](std::string s) {log_info(s);});
    m.def("log", [](py::str format, py::args args, py::kwargs kwargs) {
        log_info(format.format(*args, **kwargs).cast<std::string>().c_str());
    });
    m.def("addCdsObject", &PyRuntime::addCdsObject, py::arg(), py::arg() ,py::arg("containerclass") = UPNP_CLASS_CONTAINER);

    for (const auto& [k, v] : upnp_classes) {
        if (v) m.attr(v) = k;
    }
    for (const auto& [k, v] : ot_names) {
        if(v) m.attr(v) = k;
    }
    for (const auto& [k, v] : mt_names) {
        if (v) m.attr(v) = MetadataHandler::getMetaFieldName(k);
    }
    for (const auto& [k, v] : res_names) {
        if (v) m.attr(v) = MetadataHandler::getResAttrName(k);
    }

    py::class_<CdsResource> res(m, "CdsResource");
    res.def_property_readonly("attributes", &CdsResource::getAttributes)
       .def_property_readonly("parameters", &CdsResource::getParameters)
       .def_property_readonly("options",    &CdsResource::getOptions); 
    
    py::class_<CdsObject, std::unique_ptr<CdsObject, py::nodelete>> obj(m, "CdsObject");
     obj.def_property("id",         &CdsObject::getID,      &CdsObject::setID)
        .def_property("parentID",   &CdsObject::getParentID,&CdsObject::setParentID)
        .def_property("title",      &CdsObject::getTitle,   &CdsObject::setTitle)
        .def_property("upnpclass",  &CdsObject::getClass,   &CdsObject::setClass)
        .def_property("mtime",      &CdsObject::getMTime,   &CdsObject::setMTime)
        .def_property("sizeOnDisk", &CdsObject::getSizeOnDisk, &CdsObject::setSizeOnDisk)
        .def_property("restricted", &CdsObject::isRestricted, &CdsObject::setRestricted)
        .def_property("location", [](CdsObject* o) { return o->getLocation().string(); }, &CdsObject::setLocation)
        .def_property("theora",   [](CdsObject* o) { return o->getFlag(OBJECT_FLAG_OGG_THEORA); }, [](CdsObject* o, int i) {if(i) o->setFlag(OBJECT_FLAG_OGG_THEORA); })
        .def_property_readonly("objectType", &CdsObject::getObjectType) 
        .def_property_readonly("onlineservice", [](CdsObject* o) { return o->getFlag(OBJECT_FLAG_OGG_THEORA); })
        .def_property_readonly("meta", py::overload_cast<>(&CdsObject::getMetadata, py::const_))
        .def_property_readonly("aux", py::overload_cast<>(&CdsObject::getAuxData, py::const_))
        .def_property_readonly("res", [](CdsObject* o) { return o->getResourceCount() > 0 ? o->getResource(0).get() : nullptr; })
#ifdef ONLINE_SERVICES
        .def_property_readonly("onlineservice", [](CdsObject* o) { o->getFlag(OBJECT_FLAG_ONLINE_SERVICE) ? service_type_t(std::stoi(o->getAuxData(ONLINE_SERVICE_AUX_ID))) : 0; })
#else
        .def_property_readonly("onlineservice", [](CdsObject* o) { return 0; })
#endif
        ;
    py::class_<CdsItem,CdsObject> item(m, "CdsItem");
    item.def_property("mimeType", &CdsItem::getMimeType, &CdsItem::setMimeType)
        .def_property("trackNumber", &CdsItem::getTrackNumber, &CdsItem::setTrackNumber)
        .def_property("serviceID", &CdsItem::getServiceID, &CdsItem::setServiceID);
}

t_addCdsObject PyRuntime::callback = nullptr;

PyRuntime::PyRuntime(std::string scriptpath)
{
    auto path = std::filesystem::path(scriptpath);
    try {
        py::module::import("sys").attr("path").attr("append")(path.parent_path().string());
        importModule = py::module::import(path.stem().string().c_str());
    } catch (py::error_already_set& e) {
        if (e.matches(PyExc_SyntaxError)) {
            auto filename = e.value().attr("filename").cast<std::string>();
            auto lineno = e.value().attr("lineno").cast<int>();
            auto text = py::str(e.value().attr("text")).cast<std::string>();
            auto offset = e.value().attr("offset").cast<int>();
            log_error("Python {}\n{} at line {}\n{}\n{}", e.what(), filename, lineno, text.erase(text.find_last_not_of("\n")+1), std::string(offset - 1, '.') + '^');
        } else log_error("Python Error: {}", e.what());
    } catch (const std::runtime_error& ex) {
        log_error("Import error : {}", ex.what());
        while (PyErr_Occurred()) {
            auto e = new py::error_already_set();
            log_error("Python Error: {}\n{}", e->what());
        }
    }
}

int PyRuntime::addCdsObject(CdsObject& obj, const std::vector<std::string>& chain, std::string containerclass = UPNP_CLASS_CONTAINER)
{
    if (PyRuntime::callback != nullptr)
        return PyRuntime::callback(obj,chain, containerclass);
    return -1;
}

void PyRuntime::processCdsObject(const std::shared_ptr<CdsObject>& obj, const std::string& importpath, t_addCdsObject func)
{
    callback = func;
    try {
        importModule.attr("processCdsObject")(py::cast(obj.get()).inc_ref(), importpath);
    } catch (py::error_already_set& e) {
        log_error("Python Error: {}", e.what());
    } catch (const std::runtime_error& ex) {
        log_error("Import error : {}", ex.what());
        while (PyErr_Occurred()) {
            auto e = new py::error_already_set();
            log_error("Python Error: {}\n{}", e->what());
        }
    }
    callback = nullptr;
}
#endif // HAVE_PY
